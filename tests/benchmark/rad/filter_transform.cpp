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
#include <radr/rad/transform.hpp>

/* Adaptor stack (third component of the name):
 *  - a4: transform | filter | transform | filter (each filter rejects 1/7 of the elements); there are no
 *    adjacent adaptors of the same kind, so radr folds nothing; std nests four views.
 * Patterns:
 *  - full: read every element.
 *  - post: like full, but the adaptor is created inside the timed loop (includes the search for begin).
 *  - full_rev: read every element backwards (lst_t only); radr::filter is not common, so both arms add
 *    to_common/views::common (a no-op for std::views::filter on a common range).
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

struct a4
{};

inline constexpr auto not_div_7 = [](uint32_t i) noexcept
{
    return i % 7 != 0;
};

inline constexpr auto plus1 = [](uint32_t i)
{
    return i + 1;
};

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

// --------------------------------------------------------------------------
// arms
// --------------------------------------------------------------------------

struct std_arm
{
    template <typename Container>
    [[gnu::always_inline]] static auto base()
    {
        if constexpr (take_while_tag<Container>)
            return vec | std::views::take_while(take_while_pred<Container>);
        else
            return std::views::all(data<Container>());
    }

    template <typename Container, typename A>
    [[gnu::always_inline]] static auto adapted()
    {
        static_assert(std::same_as<A, a4>);
        return base<Container>() | std::views::transform(plus1) | std::views::filter(not_div_7) |
               std::views::transform(plus1) | std::views::filter(not_div_7);
    }

    template <typename Container, typename A>
    [[gnu::always_inline]] static auto adapted_common()
    {
        return adapted<Container, A>() | std::views::common;
    }
};

struct radr_arm
{
    template <typename Container>
    [[gnu::always_inline]] static auto base()
    {
        if constexpr (take_while_tag<Container>)
            return std::ref(vec) | radr::take_while(take_while_pred<Container>);
        else
            return std::ref(data<Container>());
    }

    template <typename Container, typename A>
    [[gnu::always_inline]] static auto adapted()
    {
        static_assert(std::same_as<A, a4>);
        return base<Container>() | radr::transform(plus1) | radr::filter(not_div_7) | radr::transform(plus1) |
               radr::filter(not_div_7);
    }

    template <typename Container, typename A>
    [[gnu::always_inline]] static auto adapted_common()
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

RADR_BENCH_PAIR(full, vec_t, a4);
RADR_BENCH_PAIR(full, deq_t, a4);
RADR_BENCH_PAIR(full, vec_tw_t, a4);
RADR_BENCH_PAIR(full, vec_tp_t, a4);
RADR_BENCH_PAIR(full, lst_t, a4);

RADR_BENCH_PAIR(post, vec_t, a4);
RADR_BENCH_PAIR(post, lst_t, a4);

RADR_BENCH_PAIR(full_rev, lst_t, a4);

BENCHMARK_MAIN();
