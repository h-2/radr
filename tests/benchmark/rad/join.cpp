#include <benchmark/benchmark.h>

#include <radr/test/aux_ranges.hpp>

#include <radr/rad/join.hpp>

void std_tenK(benchmark::State & state)
{
    std::vector<std::vector<uint32_t>> vec_of_vec;
    for (size_t i = 0; i < 1'000; ++i)
        vec_of_vec.push_back(radr::test::generate_numeric_sequence<uint32_t>(10'000));

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (int32_t i : vec_of_vec | std::views::join)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

void radr_tenK(benchmark::State & state)
{
    std::vector<std::vector<uint32_t>> vec_of_vec;
    for (size_t i = 0; i < 1'000; ++i)
        vec_of_vec.push_back(radr::test::generate_numeric_sequence<uint32_t>(10'000));

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (int32_t i : std::ref(vec_of_vec) | radr::join)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}
void std_ten(benchmark::State & state)
{
    std::vector<std::vector<uint32_t>> vec_of_vec;
    for (size_t i = 0; i < 10; ++i)
        vec_of_vec.push_back(radr::test::generate_numeric_sequence<uint32_t>(1'000'000));

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (int32_t i : vec_of_vec | std::views::join)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

void radr_ten(benchmark::State & state)
{
    std::vector<std::vector<uint32_t>> vec_of_vec;
    for (size_t i = 0; i < 10; ++i)
        vec_of_vec.push_back(radr::test::generate_numeric_sequence<uint32_t>(1'000'000));

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (int32_t i : std::ref(vec_of_vec) | radr::join)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

// warm up
BENCHMARK(radr_tenK);

// multiple adaptors created before loop
BENCHMARK(std_tenK);
BENCHMARK(radr_tenK);

// multiple adaptors created within loop
BENCHMARK(std_ten);
BENCHMARK(radr_ten);

BENCHMARK_MAIN();
