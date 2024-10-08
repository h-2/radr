#include <benchmark/benchmark.h>

#include <radr/test/aux_ranges.hpp>

#include <radr/rad/filter.hpp>
#include <radr/rad/transform.hpp>

inline constexpr auto not_div_7 = [](uint32_t i)
{
    return i % 7 != 0;
};
inline constexpr auto plus1 = [](uint32_t i)
{
    return i + 1;
};

void std_chain(benchmark::State & state)
{
    std::vector<uint32_t> vec = radr::test::generate_numeric_sequence<uint32_t>(10'000'000);

    auto v = vec | std::views::transform(plus1) | std::views::filter(not_div_7) | std::views::transform(plus1) |
             std::views::filter(not_div_7);

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (int32_t i : v)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

void radr_chain(benchmark::State & state)
{
    std::vector<uint32_t> vec = radr::test::generate_numeric_sequence<uint32_t>(10'000'000);

    auto v = std::ref(vec) | radr::transform(plus1) | radr::filter(not_div_7) | radr::transform(plus1) |
             radr::filter(not_div_7);

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (int32_t i : v)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

void std_post_chain(benchmark::State & state)
{
    std::vector<uint32_t> vec = radr::test::generate_numeric_sequence<uint32_t>(10'000'000);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto v = vec | std::views::transform(plus1) | std::views::filter(not_div_7) | std::views::transform(plus1) |
                 std::views::filter(not_div_7);

        for (int32_t i : v)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

void radr_post_chain(benchmark::State & state)
{
    std::vector<uint32_t> vec = radr::test::generate_numeric_sequence<uint32_t>(10'000'000);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto v = std::ref(vec) | radr::transform(plus1) | radr::filter(not_div_7) | radr::transform(plus1) |
                 radr::filter(not_div_7);

        for (int32_t i : v)
            count += i;
    }

    benchmark::DoNotOptimize(count);
}

// warm up
BENCHMARK(radr_chain);

// multiple adaptors created before loop
BENCHMARK(std_chain);
BENCHMARK(radr_chain);

// multiple adaptors created within loop
BENCHMARK(std_post_chain);
BENCHMARK(radr_post_chain);

BENCHMARK_MAIN();
