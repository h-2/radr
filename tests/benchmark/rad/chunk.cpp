#include <benchmark/benchmark.h>
#include <deque>
#include <ranges>

#include <radr/test/aux_ranges.hpp>

#include <radr/rad/chunk.hpp>
#include <radr/rad/lazy_chunk.hpp>
#include <radr/rad/take_while.hpp>

// every tenth chunk, i.e. the strided benchmark reads ~10% of the chunks (each one read completely)
inline constexpr ptrdiff_t stride = 10;

/* Every benchmark runs over both containers, because they put the adaptor into two very different regimes:
 *
 *  - vec_t is contiguous, so subrange creation and traversal within a chunk are as cheap as pointer arithmetic
 *    gets.
 *  - deq_t is random-access and sized but *not* contiguous, and its operator++ carries a branch (one per 512-byte
 *    block); it is also the container for which the size difference between ra_chunk_like_iterator (one iterator
 *    + three integers) and bidi_chunk_like_iterator/std::chunk_view's iterator (two-or-more "fat" deque iterators)
 *    is largest.
 *
 * So vec_t is the control that shows costs on the cheapest possible base range, and deq_t is where the cost of
 * the outer iterator's own representation is actually observable. */

using vec_t = std::vector<uint32_t>;
using deq_t = std::deque<uint32_t>;

/* The chunk size is passed as a type so that it travels next to the container through BENCHMARK_TEMPLATE. */
template <ptrdiff_t V>
using n = std::integral_constant<ptrdiff_t, V>;

using n4 = n<4>;
using n8 = n<8>;

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

/* Tag for "vec_t fed through take_while", i.e. a forward, un-common, un-sized underlying range.
 *
 * This is the only configuration here in which chunk cannot use its random-access iterator; it falls back to
 * unidi_chunk_like_iterator, which computes every chunk's size on dereference. Neither stride nor any
 * random access is possible on the resulting outer range, so only full and partial are registered for it. */
struct vec_tw_t
{};

inline constexpr auto always_true = [](uint32_t)
{
    return true;
};

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
auto std_chunked()
{
    if constexpr (std::same_as<Container, vec_tw_t>)
        return vec | std::views::take_while(always_true) | std::views::chunk(N::value);
    else
        return data<Container>() | std::views::chunk(N::value);
}
#endif

template <typename Container, typename N>
auto radr_chunked()
{
    if constexpr (std::same_as<Container, vec_tw_t>)
        return std::ref(vec) | radr::take_while(always_true) | radr::chunk(N::value);
    else
        return std::ref(data<Container>()) | radr::chunk(N::value);
}

/* radr::lazy_chunk is the experimental variant whose inner range is created lazily on dereference; its outer
 * range is always forward-only, so it appears in the full and partial benchmarks but not in stride. */
template <typename Container, typename N>
auto lazy_chunked()
{
    if constexpr (std::same_as<Container, vec_tw_t>)
        return std::ref(vec) | radr::take_while(always_true) | radr::lazy_chunk(N::value);
    else
        return std::ref(data<Container>()) | radr::lazy_chunk(N::value);
}

// --------------------------------------------------------------------------
// full read: every element of every chunk is read
// --------------------------------------------------------------------------

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
void std_full(benchmark::State & state)
{
    auto v = std_chunked<Container, N>();

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
    auto v = radr_chunked<Container, N>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && ch : v)
            for (uint32_t elem : ch)
                count += elem;
    }

    benchmark::DoNotOptimize(count);
}

template <typename Container, typename N>
void lazy_full(benchmark::State & state)
{
    auto v = lazy_chunked<Container, N>();

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

/* Isolates the cost of producing a chunk (the outer range's operator* / operator++) from the cost of reading it:
 * with n = 4 or 8 but only ever 2 elements touched, most of the work is repeatedly constructing and advancing
 * the outer iterator, not summation. */

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
void std_partial(benchmark::State & state)
{
    auto v = std_chunked<Container, N>();

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
    auto v = radr_chunked<Container, N>();

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

template <typename Container, typename N>
void lazy_partial(benchmark::State & state)
{
    auto v = lazy_chunked<Container, N>();

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

// --------------------------------------------------------------------------
// random-access stride: every tenth chunk is jumped to directly and then read completely
// --------------------------------------------------------------------------

/* radr::chunk becomes random-access automatically whenever the underlying range is random-access,
 * common and sized (which both vec_t and deq_t are here) -- see chunk.hpp's documentation. This isolates
 * the cost of a random jump (dominated by ra_chunk_like_iterator's single multiplication vs. whatever
 * std::chunk_view's iterator does) from the cost of a full sequential traversal, which the "full"
 * benchmark above already covers. */

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
void std_stride(benchmark::State & state)
{
    auto            v  = data<Container>() | std::views::chunk(N::value);
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            for (uint32_t elem : beg[i])
                count += elem;
    }

    benchmark::DoNotOptimize(count);
}
#endif

template <typename Container, typename N>
void radr_stride(benchmark::State & state)
{
    auto            v  = std::ref(data<Container>()) | radr::chunk(N::value);
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            for (uint32_t elem : beg[i])
                count += elem;
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
 * only the radr_ arm under C++20, where std::views::chunk does not exist. */
#ifdef __cpp_lib_ranges_chunk
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

/* Same, plus the experimental radr::lazy_chunk arm; only for benchmarks that need no random access. */
#define RADR_BENCH_TRIPLE(bench, Container, N)                                                                         \
    RADR_BENCH_PAIR(bench, Container, N);                                                                              \
    BENCHMARK_TEMPLATE(lazy_##bench, Container, N)->MinWarmUpTime(warmup)

#define RADR_BENCH_ALL3(bench)                                                                                         \
    RADR_BENCH_TRIPLE(bench, vec_t, n4);                                                                               \
    RADR_BENCH_TRIPLE(bench, vec_t, n8);                                                                               \
    RADR_BENCH_TRIPLE(bench, deq_t, n4);                                                                               \
    RADR_BENCH_TRIPLE(bench, deq_t, n8)

// warm up
BENCHMARK_TEMPLATE(radr_full, vec_t, n4)->MinWarmUpTime(warmup);

// every element of every chunk is read
RADR_BENCH_ALL3(full);

// only the first two elements of every chunk are read
RADR_BENCH_ALL3(partial);

// every tenth chunk is jumped to directly (random access) and read completely
RADR_BENCH_ALL(stride);

// the forward, un-common underlying range (see vec_tw_t); no stride, because it is not random-access
RADR_BENCH_TRIPLE(full, vec_tw_t, n4);
RADR_BENCH_TRIPLE(full, vec_tw_t, n8);
RADR_BENCH_TRIPLE(partial, vec_tw_t, n4);
RADR_BENCH_TRIPLE(partial, vec_tw_t, n8);

#undef RADR_BENCH_ALL
#undef RADR_BENCH_PAIR

BENCHMARK_MAIN();
