#pragma once
#include <mmintrin.h>
#include <immintrin.h>
#include <bit>
#include "MaskedPointer.h"

#define CACHE_LINE_SIZE 64

template <typename T>
struct alignas(CACHE_LINE_SIZE) Bucket
{
    enum SlotStatus
    {
        Empty = 0,
        Full = 1,
        Tombstone = 2
    };
    static constexpr int bitsForSlotStatus = 2;
    static constexpr std::size_t size = (CACHE_LINE_SIZE / 2) / sizeof(std::size_t);
    ;

    std::size_t hashes[size];
    MaskedPointer<T, bitsForSlotStatus> ptrs[size];

    using i64x4 = __m256i;

    Bucket(){};

    struct SearchMask
    {
        const std::size_t mask;

        std::size_t matchesMask() const noexcept
        {
            return mask & ((1 << size) - 1);
        }

        std::size_t emptySlotsMask() const noexcept
        {
            return mask >> size;
        }

        bool operator==(const SearchMask &other) const noexcept
        {
            return mask == other.mask;
        }

        SearchMask(std::size_t mask) noexcept : mask{mask} {};
    };

    SearchMask searchSIMD(std::size_t hash) const noexcept
    {
        i64x4 splatHash = _mm256_set1_epi64x(hash);
        i64x4 bucketHashes = _mm256_load_si256((i64x4 *)hashes);
        i64x4 hashCmpMask = _mm256_cmpeq_epi64(splatHash, bucketHashes);

        i64x4 bucketPtrs = _mm256_load_si256((i64x4 *)ptrs);
        i64x4 splatStatusMask = _mm256_set1_epi64x(MaskedPointer<T, bitsForSlotStatus>::alignmentOfT - 1);
        i64x4 statusMask = _mm256_and_si256(bucketPtrs, splatStatusMask);

        i64x4 splatOne = _mm256_set1_epi64x(1);
        i64x4 fullSlotsMask = _mm256_cmpeq_epi64(splatOne, statusMask);

        i64x4 zeroes = _mm256_setzero_si256();
        i64x4 emptySlotsMask = _mm256_cmpeq_epi64(zeroes, statusMask);

        i64x4 matchesMask = _mm256_and_si256(hashCmpMask, fullSlotsMask);

        i64x4 fullIndicesRep = _mm256_setr_epi64x(1, 2, 4, 8);
        i64x4 emptyIndicesRep = _mm256_setr_epi64x(16, 32, 64, 128);

        i64x4 matchingIndices = _mm256_blendv_epi8(zeroes, fullIndicesRep, matchesMask);
        i64x4 matchingAndEmpty = _mm256_blendv_epi8(zeroes, emptyIndicesRep, matchingIndices);

        std::size_t matchingAndEmptyArray[4];
        _mm256_storeu_si256((i64x4 *)matchingAndEmptyArray, matchingAndEmpty);

        return matchingAndEmptyArray[0] + matchingAndEmptyArray[1] + matchingAndEmptyArray[2] + matchingAndEmptyArray[3];
    }

    SearchMask search(std::size_t hash) const noexcept
    {
        std::size_t maskResult = 0;
        for (std::size_t i = 0; i < size; i++)
        {
            if (hashes[i] == hash && ptrs[i].mask() == Full)
                maskResult += (1 << i);
            else if (ptrs[i].mask() == Empty)
                maskResult += (1 << (i + size));
        }
        return maskResult;
    }
};

template <typename K, typename V>
class HashMapImpl
{
    using Entry = std::pair<K, V>;
    using SlotStatus = typename Bucket<Entry>::SlotStatus;
    using MaskedPtr = MaskedPointer<Entry, Bucket<Entry>::bitsForSlotStatus>;

    static constexpr auto hashFn = std::hash<K>();
    Bucket<Entry> *const buffer = nullptr;
    const std::size_t capacity;
    std::size_t entryCount;

    class HashMapIterator
    {
        Bucket<Entry> *ptr{nullptr};
        std::size_t slotNo{0};

    public:
        HashMapIterator &operator++()
        {
            while (ptr->ptrs[slotNo].mask() != Full)
            {
                slotNo++;
                if (slotNo == Bucket<Entry>::size)
                    ptr++;
            }
            return (*this);
        }

        HashMapIterator operator++(int)
        {
            const auto returnIt = (*this);
            (*this)++;
            return returnIt;
        }

        MaskedPtr &getMaskedPtr()
        {
            return ptr->ptrs[slotNo];
        }

        const MaskedPtr &getMaskedPtr() const
        {
            return ptr->ptrs[slotNo];
        }
    };

    HashMapIterator begin()
    {
        HashMapIterator it{buffer, 0};
        if (buffer[0].ptrs[0].mask() == Full)
            return it;
        return it++;
    }

    HashMapIterator end()
    {
        return {buffer + capacity, 0};
    }

    HashMapImpl(std::size_t capacity) : capacity{capacity}, buffer{new Bucket<Entry>[capacity]}, entryCount{0} {}

    HashMapIterator find(const K &key)
    {
        const std::size_t hash = hashFn(key);
        auto it = getMatchOrElseFirstEmpty(hash, key);
        if (it.getMaskedPtr().mask() == Empty)
            return end();
        return it;
    }

    template <typename EntryRefRef>
    std::pair<HashMapIterator, bool> insert(EntryRefRef &&value)
    {
        const auto &key = value.key;
        const std::size_t hash = hashFn(key);
        auto it = getMatchOrElseFirstEmpty(hash, key);
        if (it.getMaskedPtr().mask() == Empty)
        {
            Bucket<Entry> &bucket = *(it.getMaskedPtr());
            bucket.hashes[it.slotNo] = hash;
            bucket.ptrs[it.slotNo] = MaskedPointer(new Entry(std::forward(value)), Full);
            entryCount++;
            return {it, true};
        }
        it.getMaskedPtr->second = value.second;
        return {it, false};
    }

    HashMapIterator erase(HashMapIterator it)
    {
        auto &maskedPtr = it.getMaskedPtr();
        entryCount -= maskedPtr.mask() == Full;
        delete maskedPtr.get();
        maskedPtr = MaskedPtr(nullptr, Tombstone);
        return (++it);
    }

private:
    HashMapIterator getMatchOrElseFirstEmpty(const std::size_t hash, const K &key) const
    {
        const std::size_t startIdx = hash & (capacity - 1);
        for (std::size_t offset = 0; offset < capacity; offset++)
        {
            Bucket<Entry> *bucketPtr = buffer + ((startIdx + offset) & capacity);
            const auto searchResult = bucketPtr->searchSIMD(hash);
            const auto matchingSlots = searchResult.matchesMask();
            const auto emptySlots = searchResult.emptySlotsMask();

            for (std::size_t slotNo = 0; slotNo < 4; slotNo)
            {
                if ((1 << slotNo) & matchingSlots && bucketPtr->ptrs[slotNo]->first == key)
                    return {bucketPtr, slotNo};
            }

            if (emptySlots != 0)
                return {bucketPtr, std::countr_zero(emptySlots)};
        }
        return end();
    }
};

template <typename K, typename V>
class HashMap
{
};

Bucket<int> randomBucket()
{
    Bucket<int> b;
    for (int i = 0; i < Bucket<int>::size; i++)
    {
        b.hashes[i] = rand();
        b.ptrs[i] = MaskedPointer<int, 2>(nullptr, 1);
    }
    return b;
}
