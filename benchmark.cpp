#include <benchmark/benchmark.h>
#include "HashMap.h"
#include <vector>
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
// Register the function as a benchmark
BENCHMARK(BM_search);
BENCHMARK(BM_searchSimd);
// Run the benchmark
BENCHMARK_MAIN();