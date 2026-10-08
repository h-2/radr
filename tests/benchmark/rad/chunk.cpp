#include <benchmark/benchmark.h>
#include <deque>
#include <limits>
#include <list>
#include <ranges>
#include <vector>

#include <radr/test/aux_ranges.hpp>
#include <radr/test/hardware_counters.hpp>

#include <radr/rad/chunk.hpp>
#include <radr/rad/lazy_chunk.hpp>
#include <radr/rad/take_while.hpp>

/* Iterator selected by radr::chunk:
 *  - vec_t, deq_t: ra_chunk_like_iterator (deq_t: largest size difference to std's iterator)
 *  - lst_t: bidi_chunk_like_iterator (the only one with operator-- and BoundaryFinder)
 *  - vec_tw_t, vec_tp_t: unidi_chunk_like_iterator
 * radr::lazy_chunk is always forward-only, so it has no stride and no reverse arms. */

using vec_t = std::vector<uint32_t>;
using deq_t = std::deque<uint32_t>;
using lst_t = std::list<uint32_t>;

struct vec_tw_t
{};

struct vec_tp_t
{};

template <typename Container>
concept take_while_tag = std::same_as<Container, vec_tw_t> || std::same_as<Container, vec_tp_t>;

template <typename Container>
inline constexpr auto take_while_pred = [](uint32_t)
{
    return true;
};

template <>
inline constexpr auto take_while_pred<vec_tp_t> = [](uint32_t x)
{
    return x != std::numeric_limits<uint32_t>::max();
};

template <ptrdiff_t V>
using n = std::integral_constant<ptrdiff_t, V>;

using n4 = n<4>;
using n8 = n<8>;

// run-time chunk size; radr::lazy_chunk relies on a constant one (unrolling)
template <ptrdiff_t V>
struct rt
{
    static inline ptrdiff_t const value = []
    {
        ptrdiff_t volatile v = V;
        return v;
    }();
};

using rt4 = rt<4>;

inline constexpr ptrdiff_t stride = 10;

inline constexpr size_t data_size = 65'536;
inline constexpr size_t list_size = data_size / 8;

vec_t const vec = radr::test::generate_numeric_sequence<uint32_t>(data_size);
deq_t const deq(vec.begin(), vec.end());
lst_t const lst(vec.begin(), vec.begin() + list_size);

template <typename Container>
[[gnu::always_inline]] inline Container const & data()
{
    if constexpr (std::same_as<Container, vec_t>)
        return vec;
    else if constexpr (std::same_as<Container, deq_t>)
        return deq;
    else
        return lst;
}

#ifdef __cpp_lib_ranges_chunk
template <typename Container, typename N>
[[gnu::always_inline]] inline auto std_chunked()
{
    if constexpr (take_while_tag<Container>)
        return vec | std::views::take_while(take_while_pred<Container>) | std::views::chunk(N::value);
    else
        return data<Container>() | std::views::chunk(N::value);
}
#endif

template <typename Container, typename N>
[[gnu::always_inline]] inline auto radr_chunked()
{
    if constexpr (take_while_tag<Container>)
        return std::ref(vec) | radr::take_while(take_while_pred<Container>) | radr::chunk(N::value);
    else
        return std::ref(data<Container>()) | radr::chunk(N::value);
}

template <typename Container, typename N>
[[gnu::always_inline]] inline auto radr_lazy_chunked()
{
    if constexpr (take_while_tag<Container>)
        return std::ref(vec) | radr::take_while(take_while_pred<Container>) | radr::lazy_chunk(N::value);
    else
        return std::ref(data<Container>()) | radr::lazy_chunk(N::value);
}

// --------------------------------------------------------------------------
// full
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
void radr_lazy_full(benchmark::State & state)
{
    auto v = radr_lazy_chunked<Container, N>();

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
// partial
// --------------------------------------------------------------------------

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
void radr_lazy_partial(benchmark::State & state)
{
    auto v = radr_lazy_chunked<Container, N>();

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
// reverse (lst_t only: operator-- has to recover the length of the shorter last chunk)
// --------------------------------------------------------------------------

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
// stride
// --------------------------------------------------------------------------

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

// --------------------------------------------------------------------------
// registration
// --------------------------------------------------------------------------

#define RADR_BENCH_NAME(bench, Container, N, arm) #Container "/" #bench "/" #N "/" arm

#define RADR_BENCH_ARM(arm, bench, Container, N)                                                                       \
    BENCHMARK(radr::test::with_hardware_counters<arm##_##bench<Container, N>>)                                         \
      ->Name(RADR_BENCH_NAME(bench, Container, N, #arm))

#ifdef __cpp_lib_ranges_chunk
#    define RADR_BENCH_PAIR(bench, Container, N)                                                                       \
        RADR_BENCH_ARM(std, bench, Container, N);                                                                      \
        RADR_BENCH_ARM(radr, bench, Container, N)
#else
#    define RADR_BENCH_PAIR(bench, Container, N) RADR_BENCH_ARM(radr, bench, Container, N)
#endif

#define RADR_BENCH_TRIPLE(bench, Container, N)                                                                         \
    RADR_BENCH_PAIR(bench, Container, N);                                                                              \
    RADR_BENCH_ARM(radr_lazy, bench, Container, N)

RADR_BENCH_TRIPLE(full, vec_t, n4);
RADR_BENCH_TRIPLE(full, vec_t, n8);
RADR_BENCH_TRIPLE(full, deq_t, n4);
RADR_BENCH_TRIPLE(full, deq_t, n8);
RADR_BENCH_TRIPLE(full, vec_tw_t, n4);
RADR_BENCH_TRIPLE(full, vec_tw_t, n8);
RADR_BENCH_TRIPLE(full, vec_tp_t, n4);
RADR_BENCH_TRIPLE(full, vec_tp_t, n8);
RADR_BENCH_TRIPLE(full, lst_t, n4);
RADR_BENCH_TRIPLE(full, lst_t, n8);

RADR_BENCH_TRIPLE(partial, vec_t, n4);
RADR_BENCH_TRIPLE(partial, vec_t, n8);
RADR_BENCH_TRIPLE(partial, deq_t, n4);
RADR_BENCH_TRIPLE(partial, deq_t, n8);
RADR_BENCH_TRIPLE(partial, vec_tw_t, n4);
RADR_BENCH_TRIPLE(partial, vec_tw_t, n8);
RADR_BENCH_TRIPLE(partial, vec_tp_t, n4);
RADR_BENCH_TRIPLE(partial, vec_tp_t, n8);
RADR_BENCH_TRIPLE(partial, lst_t, n4);
RADR_BENCH_TRIPLE(partial, lst_t, n8);

RADR_BENCH_PAIR(stride, vec_t, n4);
RADR_BENCH_PAIR(stride, vec_t, n8);
RADR_BENCH_PAIR(stride, deq_t, n4);
RADR_BENCH_PAIR(stride, deq_t, n8);

RADR_BENCH_PAIR(full_rev, lst_t, n4);
RADR_BENCH_PAIR(full_rev, lst_t, n8);
RADR_BENCH_PAIR(partial_rev, lst_t, n4);
RADR_BENCH_PAIR(partial_rev, lst_t, n8);

RADR_BENCH_TRIPLE(full, vec_t, rt4);
RADR_BENCH_TRIPLE(full, vec_tw_t, rt4);
RADR_BENCH_TRIPLE(full, lst_t, rt4);

BENCHMARK_MAIN();
