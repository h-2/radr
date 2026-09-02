#include <benchmark/benchmark.h>
#include <deque>
#include <ranges>

#include <radr/test/aux_ranges.hpp>

#include <radr/rad/chunk_by.hpp>

/* Unlike radr::chunk, neither radr::chunk_by nor std::views::chunk_by ever gets a random-access
 * implementation: chunk_by's boundaries are content-dependent (found by scanning adjacent elements), not
 * arithmetic, so there is nothing for a random jump to compute without a scan -- see chunk.hpp's
 * ra_chunk_like_iterator documentation for the same reasoning. There is therefore no stride/random-access
 * benchmark here, unlike chunk.cpp. */

/* Every benchmark runs over both containers, because they put the adaptor into two very different regimes:
 *
 *  - vec_t is contiguous, so subrange creation and traversal within a chunk are as cheap as pointer arithmetic
 *    gets.
 *  - deq_t is random-access but *not* contiguous, and its operator++ carries a branch (one per 512-byte block);
 *    it is also the container for which the outer iterator's own representation (one stored underlying
 *    iterator, "fat" for a deque) is most expensive to copy around.
 *
 * So vec_t is the control that shows costs on the cheapest possible base range, and deq_t is where the cost of
 * the outer iterator's own representation is actually observable. */

using vec_t = std::vector<uint32_t>;
using deq_t = std::deque<uint32_t>;

/* Predicates chosen so that consecutive elements stay in the same chunk with probability (Mod - 1) / Mod,
 * i.e. a chunk ends with probability 1 / Mod at each step -- a geometric distribution with mean chunk length
 * Mod. The data is uniformly-distributed random uint32_t (see below), so (a + b) is effectively uniformly
 * distributed too and this holds regardless of Mod's value. */
template <uint32_t Mod>
inline constexpr auto same_chunk = [](uint32_t a, uint32_t b) noexcept
{
    return (a + b) % Mod != 0;
};

// average chunk length 4
inline constexpr auto pred4 = same_chunk<4>;
// average chunk length 8
inline constexpr auto pred8 = same_chunk<8>;

template <ptrdiff_t V>
using n = std::integral_constant<ptrdiff_t, V>;

using n4 = n<4>;
using n8 = n<8>;

template <typename N>
constexpr auto & pred_for()
{
    if constexpr (N::value == 4)
        return pred4;
    else
        return pred8;
}

vec_t const vec = radr::test::generate_numeric_sequence<uint32_t>(10'000'000);
deq_t const deq(vec.begin(), vec.end());

template <typename Container>
Container const & data()
{
    if constexpr (std::same_as<Container, vec_t>)
        return vec;
    else
        return deq;
}

// --------------------------------------------------------------------------
// full read: every element of every chunk is read
// --------------------------------------------------------------------------

#ifdef __cpp_lib_ranges_chunk_by
template <typename Container, typename N>
void std_full(benchmark::State & state)
{
    auto v = data<Container>() | std::views::chunk_by(pred_for<N>());

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && ch : v)
            for (uint32_t elem : ch)
                count += elem;
    }

    benchmark::DoNotOptimize(count);
}
#endif

template <typename Container, typename N>
void radr_full(benchmark::State & state)
{
    auto v = std::ref(data<Container>()) | radr::chunk_by(pred_for<N>());

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && ch : v)
            for (uint32_t elem : ch)
                count += elem;
    }

    benchmark::DoNotOptimize(count);
}

// --------------------------------------------------------------------------
// partial read: only the first two elements of every chunk are read
// --------------------------------------------------------------------------

/* Isolates the cost of producing a chunk (the outer range's operator* / operator++, which for chunk_by
 * includes the boundary scan) from the cost of reading it: with an average chunk length of 4 or 8 but only
 * ever 2 elements touched, most of the work is repeatedly finding the next boundary and constructing the
 * outer iterator, not summation. */

#ifdef __cpp_lib_ranges_chunk_by
template <typename Container, typename N>
void std_partial(benchmark::State & state)
{
    auto v = data<Container>() | std::views::chunk_by(pred_for<N>());

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && ch : v)
        {
            auto it = ch.begin();
            count += *it;
            if (++it != ch.end())
                count += *it;
        }
    }

    benchmark::DoNotOptimize(count);
}
#endif

template <typename Container, typename N>
void radr_partial(benchmark::State & state)
{
    auto v = std::ref(data<Container>()) | radr::chunk_by(pred_for<N>());

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && ch : v)
        {
            auto it = ch.begin();
            count += *it;
            if (++it != ch.end())
                count += *it;
        }
    }

    benchmark::DoNotOptimize(count);
}

/* The deque is spread over many separate small blocks, so a cold traversal pays page faults and TLB misses
 * that dwarf what is being measured, and Google Benchmark does not warm up by default. The std_ arm of every
 * pair runs first, so without an explicit warm-up it would systematically absorb that cold cost. Carried per
 * registration rather than left to a --benchmark_min_warmup_time on the command line, so that the numbers are
 * right by default (see adjacent.cpp for the same reasoning, measured there). */
inline constexpr double warmup = 1.0;

/* Registers the std_/radr_ pair of one benchmark for one container x N combination; falls back to registering
 * only the radr_ arm under C++20, where std::views::chunk_by does not exist. */
#ifdef __cpp_lib_ranges_chunk_by
#    define RADR_BENCH_PAIR(bench, Container, N)                                                                       \
        BENCHMARK_TEMPLATE(std_##bench, Container, N)->MinWarmUpTime(warmup);                                          \
        BENCHMARK_TEMPLATE(radr_##bench, Container, N)->MinWarmUpTime(warmup)
#else
#    define RADR_BENCH_PAIR(bench, Container, N) BENCHMARK_TEMPLATE(radr_##bench, Container, N)->MinWarmUpTime(warmup)
#endif

#define RADR_BENCH_ALL(bench)                                                                                          \
    RADR_BENCH_PAIR(bench, vec_t, n4);                                                                                 \
    RADR_BENCH_PAIR(bench, vec_t, n8);                                                                                 \
    RADR_BENCH_PAIR(bench, deq_t, n4);                                                                                 \
    RADR_BENCH_PAIR(bench, deq_t, n8)

// warm up
BENCHMARK_TEMPLATE(radr_full, vec_t, n4)->MinWarmUpTime(warmup);

// every element of every chunk is read
RADR_BENCH_ALL(full);

// only the first two elements of every chunk are read
RADR_BENCH_ALL(partial);

#undef RADR_BENCH_ALL
#undef RADR_BENCH_PAIR

BENCHMARK_MAIN();
