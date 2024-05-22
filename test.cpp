#include <unordered_map>
#include "gtest/gtest.h"
#include "HashMap.h"

constexpr int maxTestCases = 100000;

TEST(HashMap, Insert)
{
    HashMap<int, int> map;
    auto [it, flag] = map.insert(std::make_pair(10, 5));
    ASSERT_NE(it, map.end());
    ASSERT_EQ(flag, true);
    auto it2 = map.find(10);
    ASSERT_EQ(it, it2);
    ASSERT_EQ(*it, std::make_pair(10, 5));
    auto [it3, flag2] = map.insert(std::make_pair(10, 6));
    ASSERT_EQ(flag2, false);
    auto it4 = map.find(10);
    ASSERT_EQ(it4, it3);
    ASSERT_EQ(*it4, std::make_pair(10, 5));
}

TEST(HashMap, Delete)
{
    HashMap<int, int> map;
    auto [it, flag] = map.insert(std::make_pair(10, 5));
    ASSERT_NE(it, map.end());
    ASSERT_EQ(flag, true);
    auto it2 = map.erase(10);
    ASSERT_EQ(map.find(10), map.end());
    auto [it3, flag2] = map.insert(std::make_pair(10, 6));
    ASSERT_EQ(flag2, true);
}

enum HashMapOp
{
    Insert = 0,
    Delete = 1,
    Find = 2
};

TEST(HashMap, vsUnordredMap)
{
    HashMap<int, int> myMap;
    std::unordered_map<int, int> stlMap;
    const int maxKey = 1000;
    const int maxValue = 100000;
    for (int testCase = 0; testCase < maxTestCases; testCase++)
    {
        HashMapOp op = static_cast<HashMapOp>(rand() % 3);
        int key = rand() % maxKey;
        int value = rand() % maxValue;
        switch (op)
        {
        case HashMapOp::Insert:
        {
            auto [it1_, flag1] = myMap.insert(std::make_pair(key, value));
            auto [it2_, flag2] = stlMap.insert(std::make_pair(key, value));
            ASSERT_EQ(flag1, flag2);
            break;
        }
        case HashMapOp::Delete:
        {
            myMap.erase(key);
            auto it = myMap.find(key);
            assert(it == myMap.end());
            stlMap.erase(key);
            break;
        }
        case HashMapOp::Find:
        {
            auto it1 = myMap.find(key);
            auto it2 = stlMap.find(key);
            if (it1 == myMap.end())
            {
                ASSERT_EQ(it2, stlMap.end());
            }
            else
            {
                ASSERT_EQ(it1->second, it2->second);
            }
            break;
        }
        }
    }

    for (auto &[key, value] : myMap)
    {
        auto it = stlMap.find(key);
        ASSERT_NE(it, stlMap.end());
        ASSERT_EQ(it->second, value);
    }
}

TEST(BucketSearch, SearchExistinng)
{
    for (int testCases = 0; testCases < maxTestCases; testCases++)
    {
        Bucket<int> b;
        for (int i = 0; i < Bucket<int>::size; i++)
        {
            b.hashes[i] = rand();
            b.ptrs[i] = MaskedPointer<int, 2>(nullptr, 1);
        }
        for (int i = 0; i < Bucket<int>::size; i++)
            ASSERT_EQ(b.searchSIMD(b.hashes[i]), b.search(b.hashes[i]));
    }
}

TEST(BucketSearch, SearchNonFullEntries)
{
    for (int testCases = 0; testCases < maxTestCases; testCases++)
    {
        Bucket<int> b;
        for (int i = 0; i < Bucket<int>::size; i++)
        {
            b.hashes[i] = rand();
            b.ptrs[i] = MaskedPointer<int, 2>(nullptr, rand() % 3);
        }
        for (int i = 0; i < Bucket<int>::size; i++)
            ASSERT_EQ(b.searchSIMD(b.hashes[i]), b.search(b.hashes[i]));
    }
}

TEST(BucketSearch, SearchForNonExisting)
{
    for (int testCases = 0; testCases < maxTestCases; testCases++)
    {
        Bucket<int> b;
        for (int i = 0; i < Bucket<int>::size; i++)
        {
            b.hashes[i] = rand();
            b.ptrs[i] = MaskedPointer<int, 2>(nullptr, rand() % 3);
        }
        auto hash = rand();
        ASSERT_EQ(b.searchSIMD(hash), b.search(hash));
    }
}