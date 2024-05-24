#include <iostream>
#include <cassert>
#include "src/HashMap.h"

int main()
{
    HashMap<int, int> map;
    [[maybe_unused]] auto [it, flag] = map.insert(std::make_pair(10, 5));
    return 0;
}