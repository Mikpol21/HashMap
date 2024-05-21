
#include <benchmark/benchmark.h>
#include "HashMap.h"
#include <vector>
#include <unordered_map>
#include <iostream>

auto bucket = randomBucket();

int numOfIters = (1 << 0);

static void BM_searchSimd(benchmark::State &state)
{
    // Perform setup here
    std::vector<std::size_t> hashes;
    hashes.reserve(8);
    for (int i = 0; i < bucket.size; i++)
        hashes[i] = bucket.hashes[i];
    for (int i = 4; i < 8; i++)
        hashes[i] = rand();
    std::random_shuffle(hashes.begin(), hashes.end());
    for (auto _ : state)
    {
        // This code gets timed
        for (int i = 0; i < numOfIters; i++)
            benchmark::DoNotOptimize(bucket.searchSIMD(hashes[i & 7]).matchesMask());
    }
}

static void BM_search(benchmark::State &state)
{
    // Perform setup here
    std::vector<std::size_t> hashes;
    hashes.reserve(8);
    for (int i = 0; i < bucket.size; i++)
        hashes[i] = bucket.hashes[i];
    for (int i = 4; i < 8; i++)
        hashes[i] = rand();
    std::random_shuffle(hashes.begin(), hashes.end());
    for (auto _ : state)
    {
        // This code gets timed
        for (int i = 0; i < numOfIters; i++)
            benchmark::DoNotOptimize<std::size_t>(bucket.search(hashes[i & 7]).matchesMask());
    }
}

template <typename Map, int maxKey = 1000, int maxValue = 100000>
static void BM_insert_map(benchmark::State &state)
{
    Map map;
    for (auto _ : state)
    {
        int key = rand() % maxKey;
        int value = rand() % maxValue;
        map.insert(std::make_pair(key, value));
    }
}

template <typename Map, int mapSize, int keyOffset, int maxValue = (1 << 30)>
static void BM_find(benchmark::State &state)
{
    Map map;
    for (int i = 0; i < mapSize; i++)
        map.insert(std::make_pair(i, rand() % maxValue));

    for (auto _ : state)
    {
        int key = keyOffset + rand() % mapSize;
        benchmark::DoNotOptimize(map.find(key));
    }
}

// Register the function as a benchmark
BENCHMARK(BM_search);
BENCHMARK(BM_searchSimd);
BENCHMARK(BM_insert_map<HashMap<int, int>>);
BENCHMARK(BM_insert_map<std::unordered_map<int, int>>);

#define test_find(size)                                           \
    BENCHMARK(BM_find<HashMap<int, int>, size, 0>);               \
    BENCHMARK(BM_find<std::unordered_map<int, int>, size, 0>);    \
    BENCHMARK(BM_find<HashMap<int, int>, size, size>);            \
    BENCHMARK(BM_find<std::unordered_map<int, int>, size, size>); \
    BENCHMARK(BM_find<HashMap<int, int>, size, size / 2>);        \
    BENCHMARK(BM_find<std::unordered_map<int, int>, size, size / 2>);

constexpr int mediumSize = 1000;
constexpr int largeSize = 100000;
test_find(mediumSize);
test_find(largeSize);

// Run the benchmark
BENCHMARK_MAIN();