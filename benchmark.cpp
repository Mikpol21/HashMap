
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

enum class FindSpec
{
    Hit,
    Miss,
    Fifty_Fifty
};

int toKeyOffset(FindSpec spec, int mapSize)
{
    switch (spec)
    {
    case FindSpec::Hit:
        return 0;
    case FindSpec::Miss:
        return mapSize;
    case FindSpec::Fifty_Fifty:
        return mapSize / 2;
    }
    return 0;
}

template <typename Map, int mapSize, FindSpec spec, int maxValue = (1 << 30)>
static void BM_find(benchmark::State &state)
{
    Map map;
    for (int i = 0; i < mapSize; i++)
        map.insert(std::make_pair(i, rand() % maxValue));

    const int keyOffset = toKeyOffset(spec, mapSize);
    for (auto _ : state)
    {
        int key = keyOffset + rand() % mapSize;
        benchmark::DoNotOptimize(map.find(key));
    }
}

template <typename Map, int mapSize, FindSpec spec, int maxValue = (1 << 30)>
static void BM_find_string(benchmark::State &state)
{
    Map map;
    for (int i = 0; i < mapSize; i++)
        map.insert(std::make_pair(std::to_string(i), rand() % maxValue));

    const int keyOffset = toKeyOffset(spec, mapSize);
    for (auto _ : state)
    {
        int keyInt = keyOffset + rand() % mapSize;
        auto key = std::to_string(keyInt);
        benchmark::DoNotOptimize(map.find(key));
    }
}

// Register the function as a benchmark
// BENCHMARK(BM_search);
// BENCHMARK(BM_searchSimd);
// BENCHMARK(BM_insert_map<HashMap<int, int>>);
// BENCHMARK(BM_insert_map<std::unordered_map<int, int>>);

#define test_find(map, size)                       \
    BENCHMARK(BM_find<map, size, FindSpec::Hit>);  \
    BENCHMARK(BM_find<map, size, FindSpec::Miss>); \
    BENCHMARK(BM_find<map, size, FindSpec::Fifty_Fifty>);

constexpr int mediumSize = (1 << 10);
constexpr int largeSize = (1 << 17);
constexpr int hugeSize = (1 << 20);
using MyMap = HashMap<int, int>;
using STLMap = std::unordered_map<int, int>;

test_find(MyMap, mediumSize);
test_find(STLMap, mediumSize);
test_find(MyMap, largeSize);
test_find(STLMap, largeSize);
test_find(MyMap, hugeSize);
test_find(STLMap, hugeSize);

#define test_find_str(map, size)                          \
    BENCHMARK(BM_find_string<map, size, FindSpec::Hit>);  \
    BENCHMARK(BM_find_string<map, size, FindSpec::Miss>); \
    BENCHMARK(BM_find_string<map, size, FindSpec::Fifty_Fifty>);

using MyMapStr = HashMap<std::string, int>;
using STLMapStr = std::unordered_map<std::string, int>;

test_find_str(MyMapStr, mediumSize);
test_find_str(STLMapStr, mediumSize);
test_find_str(MyMapStr, largeSize);
test_find_str(STLMapStr, largeSize);
test_find_str(MyMapStr, hugeSize);
test_find_str(STLMapStr, hugeSize);

// Run the benchmark
BENCHMARK_MAIN();