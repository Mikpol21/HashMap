#include <iostream>
#include <cassert>
#include "HashMap.h"

int main()
{
    HashMap<int, int> map;
    auto [it, flag] = map.insert(std::make_pair(10, 5));
    return 0;
}