#include <iostream>
#include "HashMap.h"

std::size_t find(std::size_t hash)
{
    Bucket<int> b;
    for (std::size_t i = 0; i < 4; i++)
    {
        b.hashes[i] = 32 * i;
        b.ptrs[i] = MaskedPointer<int, 2>((int *)(0x20 * i), 1);
    }
    return b.searchSIMD(hash).mask;
}

int main()
{
    Bucket<int> b;
    for (std::size_t i = 0; i < 4; i++)
    {
        b.hashes[i] = 32 * i;
        b.ptrs[i] = MaskedPointer<int, 2>((int *)(0x20 * i), 1);
    }
    std::cout << b.search(32).mask << std ::endl;
    return 0;
}