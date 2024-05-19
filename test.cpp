#include "gtest/gtest.h"
#include "HashMap.h"

constexpr int maxTestCases = 10000;

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