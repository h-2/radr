#include <benchmark/benchmark.h>
#include <deque>
#include <limits>
#include <list>
#include <ranges>
#include <tuple>
#include <vector>

#include <radr/test/aux_ranges.hpp>
#include <radr/test/hardware_counters.hpp>

#include <radr/rad/slide.hpp>
#include <radr/rad/slide_counted.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/version.hpp>

#if RADR_FEATURE_ZIP
#    include <radr/rad/adjacent.hpp>
#endif

/* Window adaptors with a window size known at compile time (see slide.cpp for run-time values).
 * Outer iterators:
 *  - radr::slide: vec_t, deq_t (safely indexable): transform over the underlying range (limited to the number of
 *    windows with take if sized); windows are subborrow(it, it + k). Otherwise one iterator (two underlying iterators + k;
 *    stores the end if the underlying range is not common); windows are created from the stored last element and
 *    are common.
 *  - std::views::slide: vec_t, deq_t: "caches-nothing"; lst_t: "caches-last" with counted windows;
 *    vec_tw_t, vec_tp_t: "caches-first".
 *  - radr::slide_counted: k is a template argument; preserves the category; windows are
 *    subborrow(it, it + k) for random-access ranges and counted otherwise.
 *  - radr::adjacent, std::views::adjacent: k underlying iterators; windows are tuples of references, summed by
 *    folding over the tuple.
 * vec_tw_t and vec_tp_t are not sized, so they have no stride arms.
 * The accumulator is copied before benchmark::DoNotOptimize(), so that it is not address-taken; otherwise clang
 * keeps it in memory in some loops (one store per element). */

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
using k = std::integral_constant<ptrdiff_t, V>;

using k2 = k<2>;
using k4 = k<4>;
using k8 = k<8>;

inline constexpr ptrdiff_t stride = 10;

inline constexpr size_t data_size = 65'536;
inline constexpr size_t list_size = data_size / 8;

vec_t const vec = radr::test::generate_numeric_sequence<uint32_t>(data_size);
deq_t const deq(vec.begin(), vec.end());
lst_t const lst(vec.begin(), vec.begin() + list_size);

template <typename Container>
inline Container const & data()
{
    if constexpr (std::same_as<Container, vec_t>)
        return vec;
    else if constexpr (std::same_as<Container, deq_t>)
        return deq;
    else
        return lst;
}

/* sum of all elements of a tuple-window */
inline constexpr auto tuple_sum = [](auto && win)
{
    return std::apply([](auto &&... elems) { return (elems + ...); }, win);
};

#ifdef __cpp_lib_ranges_slide
template <typename Container, typename K>
inline auto std_slid()
{
    if constexpr (take_while_tag<Container>)
        return vec | std::views::take_while(take_while_pred<Container>) | std::views::slide(K::value);
    else
        return data<Container>() | std::views::slide(K::value);
}
#endif

template <typename Container, typename K>
inline auto radr_slid()
{
    if constexpr (take_while_tag<Container>)
        return std::ref(vec) | radr::take_while(take_while_pred<Container>) | radr::slide(K::value);
    else
        return std::ref(data<Container>()) | radr::slide(K::value);
}

template <typename Container, typename K>
inline auto radr_counted_slid()
{
    if constexpr (take_while_tag<Container>)
        return std::ref(vec) | radr::take_while(take_while_pred<Container>) | radr::slide_counted<K::value>;
    else
        return std::ref(data<Container>()) | radr::slide_counted<K::value>;
}

#ifdef __cpp_lib_ranges_zip
template <typename Container, typename K>
inline auto std_adjacent_slid()
{
    if constexpr (take_while_tag<Container>)
        return vec | std::views::take_while(take_while_pred<Container>) | std::views::adjacent<K::value>;
    else
        return data<Container>() | std::views::adjacent<K::value>;
}
#endif

#if RADR_FEATURE_ZIP
template <typename Container, typename K>
inline auto radr_adjacent_slid()
{
    if constexpr (take_while_tag<Container>)
        return std::ref(vec) | radr::take_while(take_while_pred<Container>) | radr::adjacent<K::value>;
    else
        return std::ref(data<Container>()) | radr::adjacent<K::value>;
}
#endif

// --------------------------------------------------------------------------
// full
// --------------------------------------------------------------------------

#ifdef __cpp_lib_ranges_slide
template <typename Container, typename K>
void std_full(benchmark::State & state)
{
    auto v = std_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
            for (uint32_t elem : win)
                count += elem;
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

template <typename Container, typename K>
void radr_full(benchmark::State & state)
{
    auto v = radr_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
            for (uint32_t elem : win)
                count += elem;
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

template <typename Container, typename K>
void radr_counted_full(benchmark::State & state)
{
    auto v = radr_counted_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
            for (uint32_t elem : win)
                count += elem;
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

#ifdef __cpp_lib_ranges_zip
template <typename Container, typename K>
void std_adjacent_full(benchmark::State & state)
{
    auto v = std_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
            count += tuple_sum(win);
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

#if RADR_FEATURE_ZIP
template <typename Container, typename K>
void radr_adjacent_full(benchmark::State & state)
{
    auto v = radr_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
            count += tuple_sum(win);
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

// --------------------------------------------------------------------------
// partial
// --------------------------------------------------------------------------

#ifdef __cpp_lib_ranges_slide
template <typename Container, typename K>
void std_partial(benchmark::State & state)
{
    auto v = std_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
        {
            auto it = win.begin();
            count += *it;
            ++it;
            count += *it;
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

template <typename Container, typename K>
void radr_partial(benchmark::State & state)
{
    auto v = radr_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
        {
            auto it = win.begin();
            count += *it;
            ++it;
            count += *it;
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

template <typename Container, typename K>
void radr_counted_partial(benchmark::State & state)
{
    auto v = radr_counted_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
        {
            auto it = win.begin();
            count += *it;
            ++it;
            count += *it;
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

#ifdef __cpp_lib_ranges_zip
template <typename Container, typename K>
void std_adjacent_partial(benchmark::State & state)
{
    auto v = std_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
        {
            count += std::get<0>(win);
            count += std::get<1>(win);
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

#if RADR_FEATURE_ZIP
template <typename Container, typename K>
void radr_adjacent_partial(benchmark::State & state)
{
    auto v = radr_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && win : v)
        {
            count += std::get<0>(win);
            count += std::get<1>(win);
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

// --------------------------------------------------------------------------
// reverse (lst_t only)
// --------------------------------------------------------------------------

#ifdef __cpp_lib_ranges_slide
template <typename Container, typename K>
void std_full_rev(benchmark::State & state)
{
    auto v = std_slid<Container, K>();

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

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

template <typename Container, typename K>
void radr_full_rev(benchmark::State & state)
{
    auto v = radr_slid<Container, K>();

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

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

template <typename Container, typename K>
void radr_counted_full_rev(benchmark::State & state)
{
    auto v = radr_counted_slid<Container, K>();

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

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

#ifdef __cpp_lib_ranges_zip
template <typename Container, typename K>
void std_adjacent_full_rev(benchmark::State & state)
{
    auto v = std_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            count += tuple_sum(*it);
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

#if RADR_FEATURE_ZIP
template <typename Container, typename K>
void radr_adjacent_full_rev(benchmark::State & state)
{
    auto v = radr_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            count += tuple_sum(*it);
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

#ifdef __cpp_lib_ranges_slide
template <typename Container, typename K>
void std_partial_rev(benchmark::State & state)
{
    auto v = std_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && win = *it;
            auto    it2 = win.begin();
            count += *it2;
            ++it2;
            count += *it2;
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

template <typename Container, typename K>
void radr_partial_rev(benchmark::State & state)
{
    auto v = radr_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && win = *it;
            auto    it2 = win.begin();
            count += *it2;
            ++it2;
            count += *it2;
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

template <typename Container, typename K>
void radr_counted_partial_rev(benchmark::State & state)
{
    auto v = radr_counted_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && win = *it;
            auto    it2 = win.begin();
            count += *it2;
            ++it2;
            count += *it2;
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

#ifdef __cpp_lib_ranges_zip
template <typename Container, typename K>
void std_adjacent_partial_rev(benchmark::State & state)
{
    auto v = std_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && win = *it;
            count += std::get<0>(win);
            count += std::get<1>(win);
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

#if RADR_FEATURE_ZIP
template <typename Container, typename K>
void radr_adjacent_partial_rev(benchmark::State & state)
{
    auto v = radr_adjacent_slid<Container, K>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            auto && win = *it;
            count += std::get<0>(win);
            count += std::get<1>(win);
        }
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

// --------------------------------------------------------------------------
// stride
// --------------------------------------------------------------------------

#ifdef __cpp_lib_ranges_slide
template <typename Container, typename K>
void std_stride(benchmark::State & state)
{
    auto            v  = std_slid<Container, K>();
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            for (uint32_t elem : beg[i])
                count += elem;
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

template <typename Container, typename K>
void radr_stride(benchmark::State & state)
{
    auto            v  = radr_slid<Container, K>();
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            for (uint32_t elem : beg[i])
                count += elem;
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

template <typename Container, typename K>
void radr_counted_stride(benchmark::State & state)
{
    auto            v  = radr_counted_slid<Container, K>();
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            for (uint32_t elem : beg[i])
                count += elem;
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}

#ifdef __cpp_lib_ranges_zip
template <typename Container, typename K>
void std_adjacent_stride(benchmark::State & state)
{
    auto            v  = std_adjacent_slid<Container, K>();
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            count += tuple_sum(beg[i]);
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

#if RADR_FEATURE_ZIP
template <typename Container, typename K>
void radr_adjacent_stride(benchmark::State & state)
{
    auto            v  = radr_adjacent_slid<Container, K>();
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride)
            count += tuple_sum(beg[i]);
    }

    uint32_t result = count;
    benchmark::DoNotOptimize(result);
}
#endif

// --------------------------------------------------------------------------
// registration
// --------------------------------------------------------------------------

#define RADR_BENCH_NAME(bench, Container, K, arm) #Container "/" #bench "/" #K "/" arm

#define RADR_BENCH_ARM(arm, bench, Container, K)                                                                       \
    BENCHMARK(radr::test::with_hardware_counters<arm##_##bench<Container, K>>)                                         \
      ->Name(RADR_BENCH_NAME(bench, Container, K, #arm))

/* arms whose adaptor is not available expand to a no-op declaration */
#ifdef __cpp_lib_ranges_slide
#    define RADR_BENCH_ARM_STD(bench, Container, K) RADR_BENCH_ARM(std, bench, Container, K)
#else
#    define RADR_BENCH_ARM_STD(bench, Container, K) static_assert(true)
#endif

#ifdef __cpp_lib_ranges_zip
#    define RADR_BENCH_ARM_STD_ADJACENT(bench, Container, K) RADR_BENCH_ARM(std_adjacent, bench, Container, K)
#else
#    define RADR_BENCH_ARM_STD_ADJACENT(bench, Container, K) static_assert(true)
#endif

#if RADR_FEATURE_ZIP
#    define RADR_BENCH_ARM_RADR_ADJACENT(bench, Container, K) RADR_BENCH_ARM(radr_adjacent, bench, Container, K)
#else
#    define RADR_BENCH_ARM_RADR_ADJACENT(bench, Container, K) static_assert(true)
#endif

// std, radr, radr_counted, std_adjacent, radr_adjacent
#define RADR_BENCH_ALL(bench, Container, K)                                                                            \
    RADR_BENCH_ARM_STD(bench, Container, K);                                                                           \
    RADR_BENCH_ARM(radr, bench, Container, K);                                                                         \
    RADR_BENCH_ARM(radr_counted, bench, Container, K);                                                                 \
    RADR_BENCH_ARM_STD_ADJACENT(bench, Container, K);                                                                  \
    RADR_BENCH_ARM_RADR_ADJACENT(bench, Container, K)

RADR_BENCH_ALL(full, vec_t, k2);
RADR_BENCH_ALL(full, vec_t, k4);
RADR_BENCH_ALL(full, vec_t, k8);
RADR_BENCH_ALL(full, deq_t, k2);
RADR_BENCH_ALL(full, deq_t, k4);
RADR_BENCH_ALL(full, deq_t, k8);
RADR_BENCH_ALL(full, vec_tw_t, k2);
RADR_BENCH_ALL(full, vec_tw_t, k4);
RADR_BENCH_ALL(full, vec_tw_t, k8);
RADR_BENCH_ALL(full, vec_tp_t, k2);
RADR_BENCH_ALL(full, vec_tp_t, k4);
RADR_BENCH_ALL(full, vec_tp_t, k8);
RADR_BENCH_ALL(full, lst_t, k2);
RADR_BENCH_ALL(full, lst_t, k4);
RADR_BENCH_ALL(full, lst_t, k8);

RADR_BENCH_ALL(partial, vec_t, k2);
RADR_BENCH_ALL(partial, vec_t, k4);
RADR_BENCH_ALL(partial, vec_t, k8);
RADR_BENCH_ALL(partial, deq_t, k2);
RADR_BENCH_ALL(partial, deq_t, k4);
RADR_BENCH_ALL(partial, deq_t, k8);
RADR_BENCH_ALL(partial, vec_tw_t, k2);
RADR_BENCH_ALL(partial, vec_tw_t, k4);
RADR_BENCH_ALL(partial, vec_tw_t, k8);
RADR_BENCH_ALL(partial, vec_tp_t, k2);
RADR_BENCH_ALL(partial, vec_tp_t, k4);
RADR_BENCH_ALL(partial, vec_tp_t, k8);
RADR_BENCH_ALL(partial, lst_t, k2);
RADR_BENCH_ALL(partial, lst_t, k4);
RADR_BENCH_ALL(partial, lst_t, k8);

RADR_BENCH_ALL(stride, vec_t, k2);
RADR_BENCH_ALL(stride, vec_t, k4);
RADR_BENCH_ALL(stride, vec_t, k8);
RADR_BENCH_ALL(stride, deq_t, k2);
RADR_BENCH_ALL(stride, deq_t, k4);
RADR_BENCH_ALL(stride, deq_t, k8);

RADR_BENCH_ALL(full_rev, lst_t, k2);
RADR_BENCH_ALL(full_rev, lst_t, k4);
RADR_BENCH_ALL(full_rev, lst_t, k8);
RADR_BENCH_ALL(partial_rev, lst_t, k2);
RADR_BENCH_ALL(partial_rev, lst_t, k4);
RADR_BENCH_ALL(partial_rev, lst_t, k8);

BENCHMARK_MAIN();
