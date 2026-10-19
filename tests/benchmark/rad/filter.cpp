#include <benchmark/benchmark.h>
#include <deque>
#include <limits>
#include <list>
#include <ranges>
#include <vector>

#include <radr/test/aux_ranges.hpp>
#include <radr/test/hardware_counters.hpp>

#include <radr/rad/filter.hpp>
#include <radr/rad/take_while.hpp>
#include <radr/rad/to_common.hpp>

/* Adaptor stacks (third component of the name):
 *  - a1: a single filter (rejects 1/7 of the elements).
 *  - a2: two filters (the second one rejects half of the remaining elements); radr folds nested filters into one.
 *  - n1, n2: like a1, a2, but the first predicate is inverted (keeps 1/7 of the elements); n2 keeps 1/14.
 *  - s1, s2: one/two filters with stateful predicates (`!= k`, k read from a volatile; practically no rejections).
 *  - c1, c2: like s1, s2, but with stateless predicates (k is a template argument); control for s1, s2.
 * Patterns:
 *  - full: read every element.
 *  - post: like full, but the adaptor is created inside the timed loop (includes the search for begin).
 *  - full_rev: read every element backwards (lst_t only); radr::filter is not common, so both arms add
 *    to_common/views::common (a no-op for std::views::filter on a common range).
 *  - copy: write every element to an output buffer in a noinline function that receives the view by reference
 *    (vec_t, s1/s2/c1/c2 only). std's iterator reaches the predicate through a pointer to the view, so the stores may
 *    alias the predicate's state; radr's iterator holds a copy of the predicate.
 * filter is never random-access, so there are no stride arms.
 * The data is random, so the predicates cause branch mispredictions in all arms. */

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

struct a1
{};

struct a2
{};

struct n1
{};

struct n2
{};

inline constexpr auto not_div_7 = [](uint32_t i) noexcept
{
    return i % 7 != 0;
};

inline constexpr auto bigger_than_halfmax = [](uint32_t i) noexcept
{
    return i > std::numeric_limits<uint32_t>::max() / 2;
};

inline constexpr auto div_7 = [](uint32_t i) noexcept
{
    return i % 7 == 0;
};

struct s1
{};

struct s2
{};

struct c1
{};

struct c2
{};

struct not_k
{
    uint32_t k;

    constexpr bool operator()(uint32_t i) const noexcept { return i != k; }
};

template <uint32_t k>
struct not_k_c
{
    constexpr bool operator()(uint32_t i) const noexcept { return i != k; }
};

uint32_t volatile k1_volatile = 7;
uint32_t volatile k2_volatile = 11;

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

// --------------------------------------------------------------------------
// arms
// --------------------------------------------------------------------------

struct std_arm
{
    template <typename Container>
    static auto base()
    {
        if constexpr (take_while_tag<Container>)
            return vec | std::views::take_while(take_while_pred<Container>);
        else
            return std::views::all(data<Container>());
    }

    template <typename Container, typename A>
    static auto adapted()
    {
        if constexpr (std::same_as<A, a1>)
            return base<Container>() | std::views::filter(not_div_7);
        else if constexpr (std::same_as<A, a2>)
            return base<Container>() | std::views::filter(not_div_7) | std::views::filter(bigger_than_halfmax);
        else if constexpr (std::same_as<A, n1>)
            return base<Container>() | std::views::filter(div_7);
        else if constexpr (std::same_as<A, n2>)
            return base<Container>() | std::views::filter(div_7) | std::views::filter(bigger_than_halfmax);
        else if constexpr (std::same_as<A, s1>)
            return base<Container>() | std::views::filter(not_k{k1_volatile});
        else if constexpr (std::same_as<A, s2>)
            return base<Container>() | std::views::filter(not_k{k1_volatile}) | std::views::filter(not_k{k2_volatile});
        else if constexpr (std::same_as<A, c1>)
            return base<Container>() | std::views::filter(not_k_c<7>{});
        else
            return base<Container>() | std::views::filter(not_k_c<7>{}) | std::views::filter(not_k_c<11>{});
    }

    template <typename Container, typename A>
    static auto adapted_common()
    {
        return adapted<Container, A>() | std::views::common;
    }
};

struct radr_arm
{
    template <typename Container>
    static auto base()
    {
        if constexpr (take_while_tag<Container>)
            return std::ref(vec) | radr::take_while(take_while_pred<Container>);
        else
            return std::ref(data<Container>());
    }

    template <typename Container, typename A>
    static auto adapted()
    {
        if constexpr (std::same_as<A, a1>)
            return base<Container>() | radr::filter(not_div_7);
        else if constexpr (std::same_as<A, a2>)
            return base<Container>() | radr::filter(not_div_7) | radr::filter(bigger_than_halfmax);
        else if constexpr (std::same_as<A, n1>)
            return base<Container>() | radr::filter(div_7);
        else if constexpr (std::same_as<A, n2>)
            return base<Container>() | radr::filter(div_7) | radr::filter(bigger_than_halfmax);
        else if constexpr (std::same_as<A, s1>)
            return base<Container>() | radr::filter(not_k{k1_volatile});
        else if constexpr (std::same_as<A, s2>)
            return base<Container>() | radr::filter(not_k{k1_volatile}) | radr::filter(not_k{k2_volatile});
        else if constexpr (std::same_as<A, c1>)
            return base<Container>() | radr::filter(not_k_c<7>{});
        else
            return base<Container>() | radr::filter(not_k_c<7>{}) | radr::filter(not_k_c<11>{});
    }

    template <typename Container, typename A>
    static auto adapted_common()
    {
        return adapted<Container, A>() | radr::to_common;
    }
};

// --------------------------------------------------------------------------
// benchmarks
// --------------------------------------------------------------------------

template <typename Arm, typename Container, typename A>
void full(benchmark::State & state)
{
    auto v = Arm::template adapted<Container, A>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (uint32_t elem : v)
            count += elem;
    }

    benchmark::DoNotOptimize(count);
}

template <typename Arm, typename Container, typename A>
void post(benchmark::State & state)
{
    uint32_t count = 0;
    for (auto _ : state)
    {
        auto v = Arm::template adapted<Container, A>();
        for (uint32_t elem : v)
            count += elem;
    }

    benchmark::DoNotOptimize(count);
}

template <typename Arm, typename Container, typename A>
void full_rev(benchmark::State & state)
{
    auto v = Arm::template adapted_common<Container, A>();

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto it = v.end(); it != v.begin();)
        {
            --it;
            count += *it;
        }
    }

    benchmark::DoNotOptimize(count);
}

/* Non-const reference, because std::views::filter is not const-iterable. */
template <typename Range>
[[gnu::noinline]] void copy_out(Range & range, uint32_t * out)
{
    for (uint32_t elem : range)
        *out++ = elem;
}

template <typename Arm, typename Container, typename A>
void copy(benchmark::State & state)
{
    auto  v = Arm::template adapted<Container, A>();
    vec_t out(data_size);

    for (auto _ : state)
    {
        copy_out(v, out.data());
        benchmark::ClobberMemory();
    }

    benchmark::DoNotOptimize(out.data());
}

// --------------------------------------------------------------------------
// registration
// --------------------------------------------------------------------------

#define RADR_BENCH_NAME(bench, Container, A, arm) #Container "/" #bench "/" #A "/" arm

#define RADR_BENCH_ARM(arm, bench, Container, A)                                                                       \
    BENCHMARK(radr::test::with_hardware_counters<bench<arm##_arm, Container, A>>)                                      \
      ->Name(RADR_BENCH_NAME(bench, Container, A, #arm))

#define RADR_BENCH_PAIR(bench, Container, A)                                                                           \
    RADR_BENCH_ARM(std, bench, Container, A);                                                                          \
    RADR_BENCH_ARM(radr, bench, Container, A)

RADR_BENCH_PAIR(full, vec_t, a1);
RADR_BENCH_PAIR(full, vec_t, a2);
RADR_BENCH_PAIR(full, deq_t, a1);
RADR_BENCH_PAIR(full, deq_t, a2);
RADR_BENCH_PAIR(full, vec_tw_t, a1);
RADR_BENCH_PAIR(full, vec_tw_t, a2);
RADR_BENCH_PAIR(full, vec_tp_t, a1);
RADR_BENCH_PAIR(full, vec_tp_t, a2);
RADR_BENCH_PAIR(full, lst_t, a1);
RADR_BENCH_PAIR(full, lst_t, a2);

RADR_BENCH_PAIR(full, vec_t, n1);
RADR_BENCH_PAIR(full, vec_t, n2);
RADR_BENCH_PAIR(full, deq_t, n1);
RADR_BENCH_PAIR(full, deq_t, n2);
RADR_BENCH_PAIR(full, vec_tw_t, n1);
RADR_BENCH_PAIR(full, vec_tw_t, n2);
RADR_BENCH_PAIR(full, vec_tp_t, n1);
RADR_BENCH_PAIR(full, vec_tp_t, n2);
RADR_BENCH_PAIR(full, lst_t, n1);
RADR_BENCH_PAIR(full, lst_t, n2);

RADR_BENCH_PAIR(post, vec_t, a1);
RADR_BENCH_PAIR(post, vec_t, a2);
RADR_BENCH_PAIR(post, lst_t, a1);
RADR_BENCH_PAIR(post, lst_t, a2);
RADR_BENCH_PAIR(post, vec_t, n1);
RADR_BENCH_PAIR(post, lst_t, n1);

RADR_BENCH_PAIR(full_rev, lst_t, a1);
RADR_BENCH_PAIR(full_rev, lst_t, a2);

RADR_BENCH_PAIR(copy, vec_t, s1);
RADR_BENCH_PAIR(copy, vec_t, s2);
RADR_BENCH_PAIR(copy, vec_t, c1);
RADR_BENCH_PAIR(copy, vec_t, c2);

BENCHMARK_MAIN();
