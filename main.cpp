#include <type_traits>
#include <mmintrin.h>
#include <immintrin.h>
#include <iostream>

constexpr int log2(int x)
{
    if (x == 1)
        return 0;
    return 1 + log2(x / 2);
}

template <typename T, int maskSize>
struct MaskedPointer
{

    static constexpr int alignmentOfT = alignof(T);
    static constexpr int freeBits = log2(alignmentOfT);
    static_assert(maskSize <= freeBits);
    std::size_t maskedPointer;

public:
    constexpr MaskedPointer(T *ptr) noexcept : maskedPointer{reinterpret_cast<std::size_t>(ptr)} {};

    constexpr MaskedPointer(T *ptr, std::size_t mask) noexcept
    {
        mask &= alignmentOfT - 1;
        maskedPointer = reinterpret_cast<std::size_t>(ptr) + mask;
    }

    constexpr MaskedPointer() : maskedPointer{0} {};

    std::size_t mask() noexcept
    {
        return maskedPointer & (alignmentOfT - 1);
    }

    T *get() noexcept
    {
        const std::size_t zeroFreeBits = (~(alignmentOfT - 1));
        return reinterpret_cast<T *>(maskedPointer & zeroFreeBits);
    }

    constexpr int sizeOfMask()
    {
        return maskSize;
    }

    T &operator*() noexcept
    {
        return *get();
    }

    const T &operator*() const noexcept
    {
        return *get();
    }

    T *operator->() noexcept
    {
        return get();
    }
};

#define CACHE_LINE_SIZE 64

template <typename T>
struct alignas(CACHE_LINE_SIZE) Bucket
{
    enum class EntryType
    {
        Empty = 0,
        Full = 1,
        Tombstone = 2
    };
    static constexpr int bitsForEntryType = 2;

    std::size_t hashes[CACHE_LINE_SIZE / 2];
    MaskedPointer<T, bitsForEntryType> ptrs[CACHE_LINE_SIZE / 2];

    using i64x4 = __m256i;

    Bucket(){};

    std::size_t searchForHash(std::size_t hash)
    {
        i64x4 splattedHash = _mm256_set1_epi64x(hash);
        i64x4 bucketHashes = _mm256_load_si256((i64x4 *)hashes);
        i64x4 hashCmpMask = _mm256_cmpeq_epi64(splattedHash, bucketHashes);

        i64x4 bucketPtrs = _mm256_load_si256((i64x4 *)ptrs);
        i64x4 splattedOne = _mm256_set1_epi64x(1);
        i64x4 fullMask = _mm256_cmpeq_epi64(splattedOne, bucketPtrs);

        i64x4 matchesMask = _mm256_and_si256(hashCmpMask, fullMask);

        i64x4 indices = _mm256_set_epi64x(1, 2, 3, 4);
        i64x4 zeros = _mm256_setzero_si256();
        i64x4 argmax = _mm256_blendv_epi8(indices, zeros, matchesMask);

        std::size_t tmp_arr[4];
        _mm256_storeu_si256((i64x4 *)tmp_arr, argmax);
        return tmp_arr[0] + tmp_arr[1] + tmp_arr[2] + tmp_arr[3] - 1;
    }
};

std::size_t test(std::size_t hash)
{
    Bucket<int> b;
    for (int i = 0; i < 4; i++)
    {
        b.hashes[i] = i;
        b.ptrs[i] = MaskedPointer<int, 2>(nullptr, 1);
    }
    return b.searchForHash(hash);
}

int main()
{
    for (int i = 0; i < 4; i++)
        std::cout << test(i) << std::endl;
}