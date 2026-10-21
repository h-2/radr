#include <benchmark/benchmark.h>
#include <deque>
#include <limits>
#include <list>
#include <ranges>
#include <vector>

#include <radr/test/aux_ranges.hpp>
#include <radr/test/hardware_counters.hpp>

#include <radr/rad/slide.hpp>
#include <radr/rad/take_while.hpp>

/* Outer iterators:
 *  - radr::slide: vec_t, deq_t (safely indexable): transform over the underlying range (limited to the number of
 *    windows with take if sized); windows are subborrow(it, it + k). Otherwise one iterator (two underlying iterators + k;
 *    stores the end if the underlying range is not common); windows are created from the stored last element and
 *    are common.
 *  - std::views::slide: vec_t, deq_t: "caches-nothing"; lst_t: "caches-last" with counted windows;
 *    vec_tw_t, vec_tp_t: "caches-first".
 * vec_tw_t and vec_tp_t are not sized, so they have no stride arms.
 * k is always a run-time value (see slide_counted.cpp for compile-time constants). */

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

// run-time window size; prevents unrolling of loops over a window
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
using rt8 = rt<8>;

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

    benchmark::DoNotOptimize(count);
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

    benchmark::DoNotOptimize(count);
}

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

    benchmark::DoNotOptimize(count);
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

    benchmark::DoNotOptimize(count);
}

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

    benchmark::DoNotOptimize(count);
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

    benchmark::DoNotOptimize(count);
}

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

    benchmark::DoNotOptimize(count);
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

    benchmark::DoNotOptimize(count);
}

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

    benchmark::DoNotOptimize(count);
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

    benchmark::DoNotOptimize(count);
}

// --------------------------------------------------------------------------
// registration
// --------------------------------------------------------------------------

#define RADR_BENCH_NAME(bench, Container, K, arm) #Container "/" #bench "/" #K "/" arm

#define RADR_BENCH_ARM(arm, bench, Container, K)                                                                       \
    BENCHMARK(radr::test::with_hardware_counters<arm##_##bench<Container, K>>)                                         \
      ->Name(RADR_BENCH_NAME(bench, Container, K, #arm))

#ifdef __cpp_lib_ranges_slide
#    define RADR_BENCH_PAIR(bench, Container, K)                                                                       \
        RADR_BENCH_ARM(std, bench, Container, K);                                                                      \
        RADR_BENCH_ARM(radr, bench, Container, K)
#else
#    define RADR_BENCH_PAIR(bench, Container, K) RADR_BENCH_ARM(radr, bench, Container, K)
#endif

RADR_BENCH_PAIR(full, vec_t, rt4);
RADR_BENCH_PAIR(full, vec_t, rt8);
RADR_BENCH_PAIR(full, deq_t, rt4);
RADR_BENCH_PAIR(full, deq_t, rt8);
RADR_BENCH_PAIR(full, vec_tw_t, rt4);
RADR_BENCH_PAIR(full, vec_tw_t, rt8);
RADR_BENCH_PAIR(full, vec_tp_t, rt4);
RADR_BENCH_PAIR(full, vec_tp_t, rt8);
RADR_BENCH_PAIR(full, lst_t, rt4);
RADR_BENCH_PAIR(full, lst_t, rt8);

RADR_BENCH_PAIR(partial, vec_t, rt4);
RADR_BENCH_PAIR(partial, vec_t, rt8);
RADR_BENCH_PAIR(partial, deq_t, rt4);
RADR_BENCH_PAIR(partial, deq_t, rt8);
RADR_BENCH_PAIR(partial, vec_tw_t, rt4);
RADR_BENCH_PAIR(partial, vec_tw_t, rt8);
RADR_BENCH_PAIR(partial, vec_tp_t, rt4);
RADR_BENCH_PAIR(partial, vec_tp_t, rt8);
RADR_BENCH_PAIR(partial, lst_t, rt4);
RADR_BENCH_PAIR(partial, lst_t, rt8);

RADR_BENCH_PAIR(stride, vec_t, rt4);
RADR_BENCH_PAIR(stride, vec_t, rt8);
RADR_BENCH_PAIR(stride, deq_t, rt4);
RADR_BENCH_PAIR(stride, deq_t, rt8);

RADR_BENCH_PAIR(full_rev, lst_t, rt4);
RADR_BENCH_PAIR(full_rev, lst_t, rt8);
RADR_BENCH_PAIR(partial_rev, lst_t, rt4);
RADR_BENCH_PAIR(partial_rev, lst_t, rt8);

BENCHMARK_MAIN();
