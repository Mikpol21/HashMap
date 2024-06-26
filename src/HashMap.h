#pragma once
#include <mmintrin.h>
#include <immintrin.h>
#include <bit>
#include "MaskedPointer.h"
#include "Arena.h"

#define CACHE_LINE_SIZE 64

template <typename T>
struct alignas(CACHE_LINE_SIZE) Bucket
{
    enum SlotStatus
    {
        Empty = 0,
        Full = 1,
        Tombstone = 2,
        Sentinel = 3
    };
    static constexpr int bitsForSlotStatus = 2;
    static constexpr std::size_t size = (CACHE_LINE_SIZE / 2) / sizeof(std::size_t);

    std::size_t hashes[size];
    MaskedPointer<T, bitsForSlotStatus> ptrs[size];

    using i64x4 = __m256i;
    using i32x8 = __m256i;

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

    std::size_t emptySlotsMask() const noexcept
    {
        i64x4 bucketPtrs = _mm256_load_si256((i64x4 *)ptrs);
        i64x4 splatStatusMask = _mm256_set1_epi64x(MaskedPointer<T, bitsForSlotStatus>::alignmentOfT - 1);
        i64x4 statusMask = _mm256_and_si256(bucketPtrs, splatStatusMask);

        i64x4 zeroes = _mm256_setzero_si256();
        i64x4 emptySlotsMask = _mm256_cmpeq_epi64(zeroes, statusMask);

        i64x4 emptyIndicesRep = _mm256_setr_epi64x(1, 2, 4, 8);

        i64x4 emptyIndices = _mm256_blendv_epi8(zeroes, emptyIndicesRep, emptySlotsMask);

        std::size_t sum = 0;
        for (int i = 0; i < 4; i++)
            sum += _mm256_extract_epi64(emptyIndices, i);
        return sum;
    }

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
        i64x4 matchingAndEmpty = _mm256_blendv_epi8(matchingIndices, emptyIndicesRep, emptySlotsMask);

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

template <typename K, typename V, bool uniqueHash>
struct HashMapImpl
{
    using Entry = std::pair<K, V>;
    using SlotStatus = typename Bucket<Entry>::SlotStatus;
    using MaskedPtr = MaskedPointer<Entry, Bucket<Entry>::bitsForSlotStatus>;

    static constexpr auto hashFn = std::hash<K>();
    Bucket<Entry> *buffer = nullptr;
    std::size_t capacity;
    std::size_t entryCount;

    struct HashMapIterator
    {
        Bucket<Entry> *ptr{nullptr};
        std::size_t slotNo{0};

        using iterator_category = std::forward_iterator_tag;
        // using difference_type = std::ptrdiff_t;
        using value_type = Entry;
        using pointer = Entry *;   // or also value_type*
        using reference = Entry &; // or also value_type&

        HashMapIterator(Bucket<Entry> *ptr, std::size_t slotNo) : ptr{ptr}, slotNo{slotNo} {}

        HashMapIterator(Bucket<Entry> *ptr, int slotNo) : ptr{ptr}, slotNo{static_cast<std::size_t>(slotNo)} {}

        HashMapIterator() : ptr{nullptr}, slotNo{0} {}

        Entry &operator*() const
        {
            return *getMaskedPtr();
        }

        Entry *operator->()
        {
            return getMaskedPtr().get();
        }

        HashMapIterator &operator++()
        {
            do
            {
                slotNo++;
                if (slotNo == Bucket<Entry>::size)
                    ptr++, slotNo = 0;
            } while (ptr->ptrs[slotNo].mask() != SlotStatus::Sentinel && ptr->ptrs[slotNo].mask() != SlotStatus::Full);

            return (*this);
        }

        HashMapIterator operator++(int)
        {
            auto returnIt = (*this);
            ++(*this);
            return returnIt;
        }

        friend bool operator==(const HashMapIterator &a, const HashMapIterator &b)
        {
            return a.ptr == b.ptr && a.slotNo == b.slotNo;
        }

        friend bool operator!=(const HashMapIterator &a, const HashMapIterator &b)
        {
            return a.ptr != b.ptr || a.slotNo != b.slotNo;
        }

        MaskedPtr &getMaskedPtr() const
        {
            return ptr->ptrs[slotNo];
        }

        std::size_t &getHash()
        {
            return ptr->hashes[slotNo];
        }
    };

    HashMapIterator begin() const
    {
        HashMapIterator it{buffer, 0};
        if (it.getMaskedPtr().mask() == SlotStatus::Full)
            return it;
        return ++it;
    }

    HashMapIterator end() const
    {
        return {buffer + capacity - 1, 3};
    }

    HashMapImpl(std::size_t capacity) : buffer{new Bucket<Entry>[capacity]}, capacity{capacity}, entryCount{0}
    {
        buffer[capacity - 1].ptrs[Bucket<Entry>::size - 1] = MaskedPtr(nullptr, SlotStatus::Sentinel);
    }

    HashMapImpl() : capacity{0}, buffer{nullptr}, entryCount{0} {};

    HashMapIterator find(const K &key) const
    {
        const std::size_t hash = hashFn(key);
        auto it = getMatchOrElseFirstEmpty(hash, key);
        if (it.getMaskedPtr().mask() == SlotStatus::Empty)
            return end();
        return it;
    }

    template <typename EntryRef, typename Allocator>
    std::pair<HashMapIterator, bool> insert(EntryRef value, Allocator &allocator)
    {
        const auto &key = value.first;
        const std::size_t hash = hashFn(key);
        auto it = getMatchOrElseFirstEmpty(hash, key);
        if (it.getMaskedPtr().mask() == SlotStatus::Empty)
        {
            Bucket<Entry> &bucket = *(it.ptr);
            bucket.hashes[it.slotNo] = hash;
            auto entryPtr = new (allocator.allocate(1)) Entry(value);
            bucket.ptrs[it.slotNo] = MaskedPtr(entryPtr, SlotStatus::Full);
            entryCount++;
            return {it, true};
        }
        // it.getMaskedPtr()->second = value.second;
        return {it, false};
    }

    V &operator[](const K &key)
    {
        const std::size_t hash = hashFn(key);
        auto it = getMatchOrElseFirstEmpty(hash, key);
        if (it.getMaskedPtr().mask() == SlotStatus::Empty)
        {
            Bucket<Entry> &bucket = *(it.ptr);
            bucket.hashes[it.slotNo] = hash;
            bucket.ptrs[it.slotNo] = MaskedPtr(new Entry(std::make_pair(key, {}), SlotStatus::Full));
            entryCount++;
            return bucket.ptrs[it.slotNo]->second;
        }
        // it.getMaskedPtr()->second = value.second;
        return it->second;
    }

    template <typename Allocator>
    HashMapIterator erase(HashMapIterator it, Allocator &allocator)
    {
        auto &maskedPtr = it.getMaskedPtr();
        auto bucket = *(it.ptr);
        allocator.deallocate(maskedPtr.get(), 1);
        bool bucketHasEmptySlots = bucket.emptySlotsMask() > 0;
        auto newSlotStatus = bucketHasEmptySlots ? SlotStatus::Empty : SlotStatus::Tombstone;
        maskedPtr = MaskedPtr(nullptr, newSlotStatus);
        return (++it);
    }

    void relocateSlot(const std::size_t hash, MaskedPtr ptr)
    {
        auto it = getFirstEmptySlot(hash);
        it.getMaskedPtr() = ptr;
        it.getHash() = hash;
    }

private:
    enum class FindMode
    {
        FirstEmpty,
        FirstMatchOrElseEmpty
    };

    HashMapIterator getMatchOrElseFirstEmpty(const std::size_t hash, const K &key) const
    {
        const std::size_t startIdx = hash & (capacity - 1);
        __builtin_prefetch(buffer + startIdx);

        for (std::size_t offset = 0; offset < capacity; offset++)
        {
            Bucket<Entry> *bucketPtr = buffer + ((startIdx + offset) & (capacity - 1));
            __builtin_prefetch(bucketPtr + 1);

            const auto searchResult = bucketPtr->searchSIMD(hash);
            const auto matchingSlots = searchResult.matchesMask();
            const auto emptySlots = searchResult.emptySlotsMask();

            if (matchingSlots != 0)
            {
                if constexpr (!uniqueHash)
                {
                    // Hash not unique, hence we have to verify each match by comparing keys
                    for (std::size_t slotNo = 0; slotNo < Bucket<Entry>::size; slotNo++)
                        if (((1 << slotNo) & matchingSlots))
                            if (bucketPtr->ptrs[slotNo]->first == key) [[likely]]
                                return HashMapIterator(bucketPtr, slotNo);
                }
                else
                {
                    // Hash is unique, thus we simply return the slotNo
                    return HashMapIterator(bucketPtr, std::countr_zero(matchingSlots));
                }
            }

            if (emptySlots != 0) [[unlikely]]
                return HashMapIterator(bucketPtr, std::countr_zero(emptySlots));
        }
        return end();
    }

    HashMapIterator getFirstEmptySlot(const std::size_t hash) const
    {
        const std::size_t startIdx = hash & (capacity - 1);
        for (std::size_t offset = 0; offset < capacity; offset++)
        {
            Bucket<Entry> *bucketPtr = buffer + ((startIdx + offset) & (capacity - 1));
            __builtin_prefetch(bucketPtr + 1);
            const auto emptySlots = bucketPtr->emptySlotsMask();

            if (emptySlots != 0) [[unlikely]]
                return HashMapIterator(bucketPtr, std::countr_zero(emptySlots));
        }
        return end();
    }
};

template <typename K, typename V, typename Allocator = ArenaAllocator<std::pair<K, V>>>
class HashMap
{
    static constexpr bool uniqueHash = std::is_integral<K>::value;
    using MapImpl = HashMapImpl<K, V, uniqueHash>;
    using HashMapIterator = MapImpl::HashMapIterator;
    using Entry = MapImpl::Entry;
    MapImpl impl;
    Allocator allocator = Allocator();

    void resize()
    {
        std::size_t newCapacity = impl.capacity << 1;
        auto newImpl = MapImpl(newCapacity);
        auto it = impl.begin();
        while (it != impl.end())
        {
            newImpl.relocateSlot(it.getHash(), it.getMaskedPtr());
            it++;
        }
        delete[] impl.buffer;
        impl = newImpl;
    }

public:
    HashMap() : impl{MapImpl(16)} {}

    HashMapIterator begin() const
    {
        return impl.begin();
    }

    HashMapIterator end() const
    {
        return impl.end();
    }

    HashMapIterator find(const K &key) const
    {
        return impl.find(key);
    }

    V &operator[](const K &key)
    {
        return impl[key];
    }

    const V &operator[](const K &key) const
    {
        return impl[key];
    }

    std::pair<HashMapIterator, bool> insert(const Entry &value)
    {
        if (impl.entryCount * 2 > impl.capacity) [[unlikely]]
            resize();
        return impl.insert(value, allocator);
    }

    std::pair<HashMapIterator, bool> insert(Entry &&value)
    {
        if (impl.entryCount * 3 > impl.capacity * Bucket<Entry>::size) [[unlikely]]
            resize();
        return impl.insert(std::move(value), allocator);
    }

    HashMapIterator erase(HashMapIterator it)
    {
        return impl.erase(it, allocator);
    }

    HashMapIterator erase(const K &key)
    {
        auto it = find(key);
        if (it != end())
            return impl.erase(it, allocator);
        return it;
    }

    ~HashMap()
    {
        freeUpMemory();
    }

    HashMap(const HashMap &other) = delete;
    HashMap &operator=(const HashMap &other) = delete;

    HashMap(HashMap &&other) : impl{other.impl}
    {
        other.impl = MapImpl();
    }

    HashMap &operator=(HashMap &&other)
    {
        freeUpMemory();
        impl = other.impl;
        other.impl = MapImpl();
    }

private:
    void freeUpMemory()
    {
        auto it = impl.begin();
        auto end = impl.end();
        while (it != end)
        {
            allocator.deallocate(it.getMaskedPtr().get(), 1);
            it++;
        }
        delete[] impl.buffer;
    }
};

Bucket<int> randomBucket()
{
    Bucket<int> b;
    for (std::size_t i = 0; i < Bucket<int>::size; i++)
    {
        b.hashes[i] = rand();
        b.ptrs[i] = MaskedPointer<int, 2>(nullptr, 1);
    }
    return b;
}
