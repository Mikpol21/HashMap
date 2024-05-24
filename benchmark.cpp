
#include <benchmark/benchmark.h>
#include "src/HashMap.h"
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

enum class HitRate
{
    AlwaysHit,
    AlwaysMiss,
    Fifty_Fifty
};

int toKeyOffset(HitRate spec, int mapSize)
{
    switch (spec)
    {
    case HitRate::AlwaysHit:
        return 0;
    case HitRate::AlwaysMiss:
        return mapSize;
    case HitRate::Fifty_Fifty:
        return mapSize / 2;
    }
    return 0;
}

template <typename Map, int mapSize, HitRate spec, int maxValue = (1 << 30)>
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

template <typename Map, int mapSize, HitRate spec, int maxValue = (1 << 30)>
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

using MyMap = HashMap<int, int>;
using STLMap = std::unordered_map<int, int>;
using MyMapSTLAlloc = HashMap<int, int, std::allocator<std::pair<int, int>>>;
using MyMapStr = HashMap<std::string, int>;
using STLMapStr = std::unordered_map<std::string, int>;

#define test_find(map, size)                            \
    BENCHMARK(BM_find<map, size, HitRate::AlwaysHit>);  \
    BENCHMARK(BM_find<map, size, HitRate::AlwaysMiss>); \
    BENCHMARK(BM_find<map, size, HitRate::Fifty_Fifty>);

#define test_find_across_size(map) \
    test_find(map, 1024);          \
    test_find(map, 32768);         \
    test_find(map, 1048576);       \
    test_find(map, 33554432)

#define test_find_str(map, size)                               \
    BENCHMARK(BM_find_string<map, size, HitRate::AlwaysHit>);  \
    BENCHMARK(BM_find_string<map, size, HitRate::AlwaysMiss>); \
    BENCHMARK(BM_find_string<map, size, HitRate::Fifty_Fifty>);

test_find_across_size(MyMap);
test_find_across_size(MyMapSTLAlloc);
test_find_across_size(STLMap);

// Run the benchmark
BENCHMARK_MAIN();