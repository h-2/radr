#include <benchmark/benchmark.h>

#include <radr/version.hpp>

#if !RADR_FEATURE_ZIP

void requires_cpp23(benchmark::State & state)
{
    for (auto _ : state)
    {
    }

    state.SkipWithMessage("radr::adjacent requires C++23.");
}

BENCHMARK(requires_cpp23);

#else

#    include <deque>

#    include <radr/test/aux_ranges.hpp>

#    include <radr/rad/adjacent.hpp>

// every tenth window, i.e. the strided benchmarks read ~10% of the windows
inline constexpr ptrdiff_t stride = 10;

/* summing a window by folding over it rather than by structured bindings keeps the benchmarks generic in N */
inline constexpr auto sum = [](auto && window)
{
    return std::apply([](auto &&... elems) { return (elems + ...); }, window);
};

/* Every benchmark runs over both containers, because they put the adaptor into two very different regimes:
 *
 *  - vec_t is contiguous, so the compiler can prove the N underlying iterators are affine offsets of one induction
 *    variable, collapse them into a single counter and vectorise. The N-iterator representation then costs nothing
 *    and does not appear in the generated code at all.
 *  - deq_t is random-access and sized but *not* contiguous, and its operator++ carries a branch (one per 512-byte
 *    block), so neither of those escapes applies. Its iterator is also 4 pointers wide, i.e. adjacent<4> carries
 *    128 bytes of iterator state rather than 32.
 *
 * So vec_t is the control that shows the representation is already free on contiguous memory, and deq_t is where
 * the cost of holding N iterators is actually observable. */

using vec_t = std::vector<uint32_t>;
using deq_t = std::deque<uint32_t>;

/* The window size is passed as a type so that it travels next to the container through BENCHMARK_TEMPLATE.
 * Sweeping it is not optional: GCC's schedule for these loops is sensitive to N, and the sign of the radr/std
 * difference has been observed to flip between neighbouring values of N, so a single N proves nothing. */
template <ptrdiff_t V>
using n = std::integral_constant<ptrdiff_t, V>;

using n2 = n<2>;
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

// --------------------------------------------------------------------------
// full read; adaptor created before the loop
// --------------------------------------------------------------------------

template <typename Container, typename N>
void std_pre(benchmark::State & state)
{
    auto v = data<Container>() | std::views::adjacent<N::value>;

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && window : v)
            count += sum(window);
    }

    benchmark::DoNotOptimize(count);
}

template <typename Container, typename N>
void radr_pre(benchmark::State & state)
{
    auto v = std::ref(data<Container>()) | radr::adjacent<N::value>;

    uint32_t count = 0;
    for (auto _ : state)
    {
        for (auto && window : v)
            count += sum(window);
    }

    benchmark::DoNotOptimize(count);
}

// --------------------------------------------------------------------------
// full read; adaptor created within the loop
// --------------------------------------------------------------------------

template <typename Container, typename N>
void std_post(benchmark::State & state)
{
    uint32_t count = 0;
    for (auto _ : state)
    {
        auto v = data<Container>() | std::views::adjacent<N::value>;

        for (auto && window : v)
            count += sum(window);
    }

    benchmark::DoNotOptimize(count);
}

template <typename Container, typename N>
void radr_post(benchmark::State & state)
{
    uint32_t count = 0;
    for (auto _ : state)
    {
        auto v = std::ref(data<Container>()) | radr::adjacent<N::value>;

        for (auto && window : v)
            count += sum(window);
    }

    benchmark::DoNotOptimize(count);
}

// --------------------------------------------------------------------------
// strided read, jumping over the skipped windows
// --------------------------------------------------------------------------

/* Indexing off begin() instead of advancing an iterator keeps us from forming a past-the-end iterator on the
 * final step; it exercises the random-access path of the adaptor's iterator, which is where holding N copies of
 * the underlying iterator (rather than one) has to do N times the work. */

template <typename Container, typename N>
void std_stride(benchmark::State & state)
{
    auto            v = data<Container>() | std::views::adjacent<N::value>;
    ptrdiff_t const n = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < n; i += stride)
        {
            count += sum(beg[i]);
        }
    }

    benchmark::DoNotOptimize(count);
}

template <typename Container, typename N>
void radr_stride(benchmark::State & state)
{
    auto            v = std::ref(data<Container>()) | radr::adjacent<N::value>;
    ptrdiff_t const n = std::ranges::ssize(v);

    uint32_t count = 0;
    for (auto _ : state)
    {
        auto beg = v.begin();

        for (ptrdiff_t i = 0; i < n; i += stride)
        {
            count += sum(beg[i]);
        }
    }

    benchmark::DoNotOptimize(count);
}

// --------------------------------------------------------------------------
// strided read, but visiting every window
// --------------------------------------------------------------------------

/* Unlike the benchmarks above, this one pays for an increment of *every* window and dereferences only every
 * tenth, which isolates the cost of the increment itself: it is O(N) as long as the iterator holds N copies of
 * the underlying iterator, and would become O(1) with a single-iterator representation for RA + sized. The
 * data-dependent branch also keeps the loop from being vectorised, so even for vec_t the increments stay in the
 * generated code. */

template <typename Container, typename N>
void std_incr_stride(benchmark::State & state)
{
    auto v = data<Container>() | std::views::adjacent<N::value>;

    uint32_t count = 0;
    for (auto _ : state)
    {
        ptrdiff_t countdown = 0;

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            if (countdown-- == 0)
            {
                countdown = stride - 1;
                count += sum(*it);
            }
        }
    }

    benchmark::DoNotOptimize(count);
}

template <typename Container, typename N>
void radr_incr_stride(benchmark::State & state)
{
    auto v = std::ref(data<Container>()) | radr::adjacent<N::value>;

    uint32_t count = 0;
    for (auto _ : state)
    {
        ptrdiff_t countdown = 0;

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            if (countdown-- == 0)
            {
                countdown = stride - 1;
                count += sum(*it);
            }
        }
    }

    benchmark::DoNotOptimize(count);
}

/* The deque is 40 MB spread over ~78k separate 512-byte blocks, so a cold traversal pays page faults and TLB
 * misses that dwarf what is being measured: the same benchmark reports ~22 ms cold and ~12 ms warm. Google
 * Benchmark does not warm up by default, and the std_ arm of every pair runs first, so without this the std_ arm
 * would systematically absorb the cold cost. Carried per registration rather than left to a --benchmark_min_warmup_time
 * on the command line, so that the numbers are right by default. */
inline constexpr double warmup = 1.0;

/* Registers the std_/radr_ pair of one benchmark for every container × N combination. The two arms are kept
 * adjacent in the output so each pair can be read off directly; N varies fastest, then the container. */
#    define RADR_BENCH_PAIR(bench)                                                                                     \
        BENCHMARK_TEMPLATE(std_##bench, vec_t, n2)->MinWarmUpTime(warmup);                                             \
        BENCHMARK_TEMPLATE(radr_##bench, vec_t, n2)->MinWarmUpTime(warmup);                                            \
        BENCHMARK_TEMPLATE(std_##bench, vec_t, n4)->MinWarmUpTime(warmup);                                             \
        BENCHMARK_TEMPLATE(radr_##bench, vec_t, n4)->MinWarmUpTime(warmup);                                            \
        BENCHMARK_TEMPLATE(std_##bench, vec_t, n8)->MinWarmUpTime(warmup);                                             \
        BENCHMARK_TEMPLATE(radr_##bench, vec_t, n8)->MinWarmUpTime(warmup);                                            \
        BENCHMARK_TEMPLATE(std_##bench, deq_t, n2)->MinWarmUpTime(warmup);                                             \
        BENCHMARK_TEMPLATE(radr_##bench, deq_t, n2)->MinWarmUpTime(warmup);                                            \
        BENCHMARK_TEMPLATE(std_##bench, deq_t, n4)->MinWarmUpTime(warmup);                                             \
        BENCHMARK_TEMPLATE(radr_##bench, deq_t, n4)->MinWarmUpTime(warmup);                                            \
        BENCHMARK_TEMPLATE(std_##bench, deq_t, n8)->MinWarmUpTime(warmup);                                             \
        BENCHMARK_TEMPLATE(radr_##bench, deq_t, n8)->MinWarmUpTime(warmup)

// warm up
BENCHMARK_TEMPLATE(radr_pre, vec_t, n4)->MinWarmUpTime(warmup);

// full read, adaptor created before loop
RADR_BENCH_PAIR(pre);

// full read, adaptor created within loop
RADR_BENCH_PAIR(post);

// every tenth window, skipped windows are jumped over
RADR_BENCH_PAIR(stride);

// every tenth window, but every window is incremented over
RADR_BENCH_PAIR(incr_stride);

#endif

BENCHMARK_MAIN();
