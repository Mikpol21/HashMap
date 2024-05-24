#pragma once
#include <array>
#include <sys/mman.h>
#include <cassert>
#define PAGE_SIZE 4096

template <typename T>
struct Page
{
    static constexpr int payloadSize = (PAGE_SIZE - sizeof(std::size_t) - sizeof(Page *)) / sizeof(T);
    // idk if std::array calls T's constructor on element of the array; I hope not
    std::array<T, payloadSize> data;
    Page *prevPage;
    std::size_t dataEnd;

    Page() : prevPage{nullptr}, dataEnd{0} {}
    Page(const Page &other) = delete;
    Page &operator=(const Page &other) = delete;
    Page(Page &&page) = delete;
    Page &operator=(Page &&other) = delete;

    T *tryAllocate(std::size_t n)
    {
        if (dataEnd + n > payloadSize) [[unlikely]]
            return nullptr;
        dataEnd += n;
        return &data[dataEnd - n];
    }
};

template <typename T>
class ArenaAllocator
{
    Page<T> *topOfPageStack;

    Page<T> *allocateNewPage()
    {
        auto rawPagePtr = mmap(nullptr, PAGE_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(rawPagePtr != MAP_FAILED);
        return static_cast<Page<T> *>(rawPagePtr);
    }

public:
    ArenaAllocator()
    {
        topOfPageStack = allocateNewPage();
    }

    T *allocate(std::size_t n)
    {
        if (n > Page<T>::payloadSize) [[unlikely]]
            throw std::exception();

        auto ptr = topOfPageStack->tryAllocate(n);
        if (ptr != nullptr) [[likely]]

            return ptr;
        auto newPage = allocateNewPage();
        newPage->prevPage = topOfPageStack;
        topOfPageStack = newPage;
        return newPage->tryAllocate(n);
    }

    void deallocate([[maybe_unused]] T *ptr, [[maybe_unused]] std::size_t n)
    {
        // no-op
    }

    ~ArenaAllocator()
    {
        while (topOfPageStack != nullptr)
        {
            auto nextPage = topOfPageStack->prevPage;
            assert(munmap(topOfPageStack, PAGE_SIZE) == 0);
            topOfPageStack = nextPage;
        }
    }
};
