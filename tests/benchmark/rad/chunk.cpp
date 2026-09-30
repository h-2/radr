#include <algorithm>
#include <benchmark/benchmark.h>
#include <deque>
#include <list>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <radr/test/aux_ranges.hpp>

#include <radr/rad/chunk.hpp>
#include <radr/rad/lazy_chunk.hpp>
#include <radr/rad/take_while.hpp>

// every tenth chunk, i.e. the strided benchmark reads ~10% of the chunks (each one read completely)
inline constexpr ptrdiff_t stride = 10;

/* The containers put the adaptor into three very different regimes:
 *
 *  - vec_t is contiguous, so subrange creation and traversal within a chunk are as cheap as pointer arithmetic
 *    gets.
 *  - deq_t is random-access and sized but *not* contiguous, and its operator++ carries a branch (one per 512-byte
 *    block); it is also the container for which the size difference between ra_chunk_like_iterator (one iterator
 *    + three integers) and bidi_chunk_like_iterator/std::chunk_view's iterator (two-or-more "fat" deque iterators)
 *    is largest.
 *  - lst_t is bidirectional, common and sized but not random-access, which is the only combination that selects
 *    radr::detail::bidi_chunk_like_iterator; vec_t and deq_t both select ra_chunk_like_iterator, which uses no
 *    BoundaryFinder at all. It is therefore also the only regime in which operator-- (and with it the recovery
 *    of the potentially shorter last chunk) is reachable, see the reverse benchmarks below.
 *
 * So vec_t is the control that shows costs on the cheapest possible base range, deq_t is where the cost of
 * the outer iterator's own representation is actually observable, and lst_t is where the BoundaryFinder is. */

using vec_t = std::vector<uint32_t>;
using deq_t = std::deque<uint32_t>;
using lst_t = std::list<uint32_t>;

/* The chunk size is passed as a type so that it travels next to the container through BENCHMARK_TEMPLATE. */
template <ptrdiff_t V>
using n = std::integral_constant<ptrdiff_t, V>;

using n4 = n<4>;
using n8 = n<8>;

vec_t const vec = radr::test::generate_numeric_sequence<uint32_t>(10'000'000);
deq_t const deq(vec.begin(), vec.end());

/* The list is deliberately much smaller than the other two containers. At 10'000'000 nodes it would occupy
 * ~240 MiB and every traversal would be bound by pointer-chasing latency, which is exactly what hides the
 * per-chunk book-keeping this regime exists to measure; vec_t and deq_t already cover the memory-bound case.
 * At this size the nodes stay in L3, so the outer iterator's own cost is observable.
 * Consequence: absolute times are comparable between the arms of one row, but not between lst_t and the other
 * containers, because the element count differs. */
inline constexpr size_t list_size = 65'536;

lst_t const lst(vec.begin(), vec.begin() + list_size);

template <typename Container>
Container const & data()
{
    if constexpr (std::same_as<Container, vec_t>)
        return vec;
    else if constexpr (std::same_as<Container, deq_t>)
        return deq;
    else
        return lst;
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
// reverse read: the chunks are visited from the last one to the first
// --------------------------------------------------------------------------

/* Registered for lst_t only, because that is the one regime here whose outer iterator is
 * radr::detail::bidi_chunk_like_iterator (see the note on the containers above); for vec_t and deq_t the outer
 * range is random-access and the stride benchmark below already covers non-sequential access.
 *
 * Decrementing is not the mirror image of incrementing: the last chunk may be shorter than n, so operator--
 * cannot simply step back n elements from the end, and the length it has to step back instead is not derivable
 * from the two iterators alone. The forward benchmarks cannot observe any of that. Both a full and a partial
 * variant are registered for the same reason as in the forward case: to separate producing a chunk from
 * reading it.
 *
 * radr::lazy_chunk does not appear here; its outer range is forward-only. */

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
void std_full_rev(benchmark::State & state)
{
    auto v = std_chunked<Container, N>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            for (uint32_t elem : *it)
                count += elem;
        }
    }

    benchmark::DoNotOptimize(count);
}
#endif

template <typename Container, typename N>
void radr_full_rev(benchmark::State & state)
{
    auto v = radr_chunked<Container, N>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            for (uint32_t elem : *it)
                count += elem;
        }
    }

    benchmark::DoNotOptimize(count);
}

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
void std_partial_rev(benchmark::State & state)
{
    auto v = std_chunked<Container, N>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && ch  = *it;
            auto    it2 = ch.begin();
            count += *it2;
            if (++it2 != ch.end())
                count += *it2;
        }
    }

    benchmark::DoNotOptimize(count);
}
#endif

template <typename Container, typename N>
void radr_partial_rev(benchmark::State & state)
{
    auto v = radr_chunked<Container, N>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && ch  = *it;
            auto    it2 = ch.begin();
            count += *it2;
            if (++it2 != ch.end())
                count += *it2;
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
 * pair runs first, so without a warm-up it would systematically absorb that cold cost; adjacent.cpp quantifies
 * the difference. The warm-up therefore has to be on for a plain run, not only when asked for on the command
 * line.
 *
 * main() below raises the default of --benchmark_min_warmup_time rather than setting the warm-up per
 * registration: a per-registration MinWarmUpTime() is silently ignored unless that same registration also sets
 * MinTime() (which would in turn take precedence over --benchmark_min_time and silently ignore it), and every
 * setting applied per registration is appended to the reported benchmark name. A separate leading warm-up
 * registration is not an option either: it duplicates the name of an already registered benchmark, and
 * same-named entries silently collapse into one when the JSON output is aggregated by name. */
inline constexpr std::string_view warmup_flag = "--benchmark_min_warmup_time=0.5";

/* The reported name of every benchmark is "<container>/<access pattern>/<n>/<arm>", e.g. "lst_t/full/n4/radr",
 * instead of the "radr_full<lst_t, n4>" that BENCHMARK_TEMPLATE would produce on its own. It is shorter, the
 * three arms of one comparison share a common prefix, and --benchmark_filter=lst_t/full then selects exactly
 * one group of them. The container and n parts are the spellings used in this file, so they can be grepped back.
 */
#define RADR_BENCH_NAME(bench, Container, N, arm) #Container "/" #bench "/" #N "/" arm

/* Registers the std_/radr_ pair of one benchmark for one container x N combination; falls back to registering
 * only the radr_ arm under C++20, where std::views::chunk does not exist. */
#ifdef __cpp_lib_ranges_chunk
#    define RADR_BENCH_PAIR(bench, Container, N)                                                                       \
        BENCHMARK_TEMPLATE(std_##bench, Container, N)->Name(RADR_BENCH_NAME(bench, Container, N, "std"));              \
        BENCHMARK_TEMPLATE(radr_##bench, Container, N)->Name(RADR_BENCH_NAME(bench, Container, N, "radr"))
#else
#    define RADR_BENCH_PAIR(bench, Container, N)                                                                       \
        BENCHMARK_TEMPLATE(radr_##bench, Container, N)->Name(RADR_BENCH_NAME(bench, Container, N, "radr"))
#endif

#define RADR_BENCH_ALL(bench)                                                                                          \
    RADR_BENCH_PAIR(bench, vec_t, n4);                                                                                 \
    RADR_BENCH_PAIR(bench, vec_t, n8);                                                                                 \
    RADR_BENCH_PAIR(bench, deq_t, n4);                                                                                 \
    RADR_BENCH_PAIR(bench, deq_t, n8)

/* Same, plus the experimental radr::lazy_chunk arm; only for benchmarks that need no random access. */
#define RADR_BENCH_TRIPLE(bench, Container, N)                                                                         \
    RADR_BENCH_PAIR(bench, Container, N);                                                                              \
    BENCHMARK_TEMPLATE(lazy_##bench, Container, N)->Name(RADR_BENCH_NAME(bench, Container, N, "lazy"))

#define RADR_BENCH_ALL3(bench)                                                                                         \
    RADR_BENCH_TRIPLE(bench, vec_t, n4);                                                                               \
    RADR_BENCH_TRIPLE(bench, vec_t, n8);                                                                               \
    RADR_BENCH_TRIPLE(bench, deq_t, n4);                                                                               \
    RADR_BENCH_TRIPLE(bench, deq_t, n8)

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

// the bidirectional, non-random-access underlying range (see lst_t); no stride, because it is not random-access
RADR_BENCH_TRIPLE(full, lst_t, n4);
RADR_BENCH_TRIPLE(full, lst_t, n8);
RADR_BENCH_TRIPLE(partial, lst_t, n4);
RADR_BENCH_TRIPLE(partial, lst_t, n8);

// the chunks in reverse order; only meaningful for lst_t, see the reverse-read section above
RADR_BENCH_PAIR(full_rev, lst_t, n4);
RADR_BENCH_PAIR(full_rev, lst_t, n8);
RADR_BENCH_PAIR(partial_rev, lst_t, n4);
RADR_BENCH_PAIR(partial_rev, lst_t, n8);

#undef RADR_BENCH_ALL
#undef RADR_BENCH_PAIR
#undef RADR_BENCH_NAME

/* Like BENCHMARK_MAIN(), but with the warm-up flag defaulted to warmup_flag (see the rationale above). An
 * explicit --benchmark_min_warmup_time on the command line is left untouched and therefore still wins. */
int main(int argc, char ** argv)
{
    std::string         warmup_arg{warmup_flag};
    std::vector<char *> args(argv, argv + argc);

    auto const is_warmup_flag = [](char const * arg)
    {
        return std::string_view{arg}.starts_with("--benchmark_min_warmup_time");
    };

    if (std::ranges::none_of(args, is_warmup_flag))
        args.push_back(warmup_arg.data());

    int argc_ = static_cast<int>(args.size());
    benchmark::Initialize(&argc_, args.data());
    if (benchmark::ReportUnrecognizedArguments(argc_, args.data()))
        return 1;

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
