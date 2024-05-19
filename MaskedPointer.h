#pragma once
#include <type_traits>

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

    constexpr MaskedPointer(T *ptr) noexcept : maskedPointer{reinterpret_cast<std::size_t>(ptr)} {};

    constexpr MaskedPointer(T *ptr, std::size_t mask) noexcept
    {
        mask &= alignmentOfT - 1;
        maskedPointer = reinterpret_cast<std::size_t>(ptr) + mask;
    }

    constexpr MaskedPointer() : maskedPointer{0} {};

    std::size_t mask() const noexcept
    {
        return maskedPointer & (alignmentOfT - 1);
    }

    T *get() const noexcept
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