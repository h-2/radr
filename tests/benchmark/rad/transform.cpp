#include <benchmark/benchmark.h>
#include <deque>
#include <limits>
#include <list>
#include <ranges>
#include <vector>

#include <radr/test/aux_ranges.hpp>
#include <radr/test/hardware_counters.hpp>

#include <radr/rad/take_while.hpp>
#include <radr/rad/to_common.hpp>
#include <radr/rad/transform.hpp>

/* Adaptor stacks (third component of the name):
 *  - a1: a single transform.
 *  - a4: four transforms; radr folds nested transforms into one, std nests four transform_views.
 *  - s1, s4: like a1, a4, but with stateful functors (constants read from a volatile).
 * Patterns:
 *  - full: read every element.
 *  - post: like full, but the adaptor is created inside the timed loop.
 *  - stride: random access to every tenth element (vec_t, deq_t only).
 *  - full_rev: read every element backwards (lst_t only).
 *  - copy: write every element to an output buffer in a noinline function that receives the view by reference
 *    (vec_t, s1/s4 only). std's iterator reaches the functor through a pointer to the view, so the stores may alias
 *    the functor's state; radr's iterator holds a copy of the functor.
 * vec_tw_t and vec_tp_t are not sized and not common, so they have neither stride nor reverse arms. */

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

struct a4
{};

inline constexpr auto plus1 = [](uint32_t i)
{
    return i + 1;
};

inline constexpr auto div2 = [](uint32_t i)
{
    return i / 2;
};

struct s1
{};

struct s4
{};

struct plus_k
{
    uint32_t k;

    constexpr uint32_t operator()(uint32_t i) const { return i + k; }
};

struct times_k
{
    uint32_t k;

    constexpr uint32_t operator()(uint32_t i) const { return i * k; }
};

uint32_t volatile k_volatile = 3;

inline constexpr ptrdiff_t stride_width = 10;

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
            return base<Container>() | std::views::transform(plus1);
        else if constexpr (std::same_as<A, a4>)
            return base<Container>() | std::views::transform(plus1) | std::views::transform(div2) |
                   std::views::transform(plus1) | std::views::transform(div2);
        else if constexpr (std::same_as<A, s1>)
            return base<Container>() | std::views::transform(plus_k{k_volatile});
        else
            return base<Container>() | std::views::transform(plus_k{k_volatile}) |
                   std::views::transform(times_k{k_volatile}) | std::views::transform(plus_k{k_volatile}) |
                   std::views::transform(times_k{k_volatile});
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
            return base<Container>() | radr::transform(plus1);
        else if constexpr (std::same_as<A, a4>)
            return base<Container>() | radr::transform(plus1) | radr::transform(div2) | radr::transform(plus1) |
                   radr::transform(div2);
        else if constexpr (std::same_as<A, s1>)
            return base<Container>() | radr::transform(plus_k{k_volatile});
        else
            return base<Container>() | radr::transform(plus_k{k_volatile}) | radr::transform(times_k{k_volatile}) |
                   radr::transform(plus_k{k_volatile}) | radr::transform(times_k{k_volatile});
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

template <typename Arm, typename Container, typename A>
void stride(benchmark::State & state)
{
    auto            v  = Arm::template adapted<Container, A>();
    ptrdiff_t const sz = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < sz; i += stride_width)
            count += beg[i];
    }

    benchmark::DoNotOptimize(count);
}

template <typename Range>
[[gnu::noinline]] void copy_out(Range const & range, uint32_t * out)
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
RADR_BENCH_PAIR(full, vec_t, a4);
RADR_BENCH_PAIR(full, deq_t, a1);
RADR_BENCH_PAIR(full, deq_t, a4);
RADR_BENCH_PAIR(full, vec_tw_t, a1);
RADR_BENCH_PAIR(full, vec_tw_t, a4);
RADR_BENCH_PAIR(full, vec_tp_t, a1);
RADR_BENCH_PAIR(full, vec_tp_t, a4);
RADR_BENCH_PAIR(full, lst_t, a1);
RADR_BENCH_PAIR(full, lst_t, a4);

RADR_BENCH_PAIR(post, vec_t, a1);
RADR_BENCH_PAIR(post, vec_t, a4);
RADR_BENCH_PAIR(post, lst_t, a1);
RADR_BENCH_PAIR(post, lst_t, a4);

RADR_BENCH_PAIR(stride, vec_t, a1);
RADR_BENCH_PAIR(stride, vec_t, a4);
RADR_BENCH_PAIR(stride, deq_t, a1);
RADR_BENCH_PAIR(stride, deq_t, a4);

RADR_BENCH_PAIR(full_rev, lst_t, a1);
RADR_BENCH_PAIR(full_rev, lst_t, a4);

RADR_BENCH_PAIR(copy, vec_t, s1);
RADR_BENCH_PAIR(copy, vec_t, s4);

BENCHMARK_MAIN();
