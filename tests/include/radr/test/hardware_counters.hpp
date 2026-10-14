#pragma once

#include <algorithm>
#include <array>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <optional>

#if defined(RADR_BENCHMARK_HWPMC)
#    include <pmc.h>
#    include <sys/module.h>
#    include <sys/param.h>
#    include <sys/pmc.h>
#    include <sys/syscall.h>
#    include <unistd.h>
#elif defined(RADR_BENCHMARK_LIBPFM)
#    include <linux/perf_event.h>
#    include <perfmon/pfmlib_perf_event.h>
#    include <sys/syscall.h>
#    include <unistd.h>
#endif

namespace radr::test
{

namespace detail
{

//!\brief Names of the reported counters.
inline constexpr std::array<char const *, 4> hw_counter_names{"instructions",
                                                              "cycles",
                                                              "taken_branches",
                                                              "mispredicted_branches"};

//!\brief Number of counters.
inline constexpr size_t hw_counter_count = hw_counter_names.size();

//!\brief Raw counter values, in the order of hw_counter_names.
using hw_counter_values = std::array<uint64_t, hw_counter_count>;

//!\brief Alternative event specifications of one counter; the first one that can be opened is used.
using hw_event_alternatives = std::initializer_list<char const *>;

#if defined(RADR_BENCHMARK_HWPMC)

//!\brief libpmc event specifications, in the order of hw_counter_names (branches: AMD Zen, Intel).
inline constexpr std::array<hw_event_alternatives, hw_counter_count> hw_event_specs{
  {{"instructions"},
   {"unhalted-cycles"},
   {"ex_ret_brn_tkn", "br_inst_retired.near_taken"},
   {"ex_ret_brn_misp", "br_misp_retired.all_branches"}}
};

using hw_handle = pmc_id_t;

/*!\brief Allocates, attaches and starts a process-scope counting PMC that only counts user space.
 * \details libpmc ignores the "usr" qualifier for events from the pmu-events tables (all recent CPUs), so the
 * allocation request is created by libpmc and then modified and issued directly.
 */
inline std::optional<hw_handle> hw_open(char const * const spec)
{
    static int const syscall_num = []
    {
        module_stat ms{};
        ms.version   = sizeof(ms);
        int const id = modfind(PMC_MODULE_NAME);
        return id >= 0 && modstat(id, &ms) == 0 ? ms.data.intval : -1;
    }();

    pmc_op_pmcallocate op{};
    op.pm_cpu  = PMC_CPU_ANY;
    op.pm_mode = PMC_MODE_TC;

    if (syscall_num < 0 || pmc_pmu_pmcallocate(spec, &op) != 0)
        return std::nullopt;

    // x86 event select: bit 16 is USR, bit 17 is OS
    op.pm_caps = (op.pm_caps & ~PMC_CAP_SYSTEM) | PMC_CAP_USER;
    if (op.pm_class == PMC_CLASS_K8)
        op.pm_md.pm_amd.pm_amd_config &= ~uint64_t{1u << 17};
    else if (op.pm_class == PMC_CLASS_IAP)
        op.pm_md.pm_iap.pm_iap_config &= ~uint64_t{1u << 17};

    if (syscall(syscall_num, PMC_OP_PMCALLOCATE, &op) != 0 || pmc_attach(op.pm_pmcid, 0) != 0 ||
        pmc_start(op.pm_pmcid) != 0)
        return std::nullopt;

    return op.pm_pmcid;
}

//!\brief Reads the current value of a PMC.
inline uint64_t hw_read(hw_handle const id)
{
    pmc_value_t v = 0;
    pmc_read(id, &v);
    return v;
}

//!\brief Initialises libpmc.
inline bool hw_init()
{
    return pmc_init() == 0;
}

#elif defined(RADR_BENCHMARK_LIBPFM)

//!\brief libpfm event specifications, in the order of hw_counter_names (taken branches: AMD Zen, Intel).
inline constexpr std::array<hw_event_alternatives, hw_counter_count> hw_event_specs{
  {{"perf::INSTRUCTIONS"},
   {"perf::CYCLES"},
   {"RETIRED_TAKEN_BRANCH_INSTRUCTIONS", "BR_INST_RETIRED:NEAR_TAKEN"},
   {"perf::BRANCH-MISSES"}}
};

using hw_handle = int;

//!\brief Opens a perf event of the calling thread that only counts user space and is enabled immediately.
inline std::optional<hw_handle> hw_open(char const * const spec)
{
    perf_event_attr       attr{};
    pfm_perf_encode_arg_t arg{};
    arg.attr = &attr;
    arg.size = sizeof(arg);

    if (pfm_get_os_event_encoding(spec, PFM_PLM3, PFM_OS_PERF_EVENT, &arg) != PFM_SUCCESS)
        return std::nullopt;

    attr.size           = sizeof(attr);
    attr.disabled       = 0;
    attr.exclude_kernel = 1;
    attr.exclude_hv     = 1;

    int const fd = static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0));
    return fd >= 0 ? std::optional{fd} : std::nullopt;
}

//!\brief Reads the current value of a perf event.
inline uint64_t hw_read(hw_handle const fd)
{
    uint64_t v = 0;
    return read(fd, &v, sizeof(v)) == sizeof(v) ? v : 0;
}

//!\brief Initialises libpfm.
inline bool hw_init()
{
    return pfm_initialize() == PFM_SUCCESS;
}

#else

using hw_handle = int;

inline std::optional<hw_handle> hw_open(char const *)
{
    return std::nullopt;
}

inline uint64_t hw_read(hw_handle)
{
    return 0;
}

inline bool hw_init()
{
    return false;
}

inline constexpr std::array<hw_event_alternatives, hw_counter_count> hw_event_specs{};

#endif

//!\brief Handles of the counters, in the order of hw_counter_names; std::nullopt for counters that are not available.
using hw_handles_t = std::array<std::optional<hw_handle>, hw_counter_count>;

//!\brief The process-wide counter handles.
inline hw_handles_t const & hw_handles()
{
    static hw_handles_t const handles = []
    {
        hw_handles_t ret{};
#if defined(RADR_BENCHMARK_HWPMC) || defined(RADR_BENCHMARK_LIBPFM)
        bool const init = hw_init();
        for (size_t i = 0; init && i < hw_counter_count; ++i)
            for (char const * const spec : hw_event_specs[i])
                if ((ret[i] = hw_open(spec)))
                    break;

        for (size_t i = 0; i < hw_counter_count; ++i)
            if (!ret[i])
                std::fprintf(stderr, "WARNING: hardware counter \"%s\" is not available.\n", hw_counter_names[i]);
#endif
        return ret;
    }();

    return handles;
}

//!\brief Reads all available counters (0 for the others).
inline hw_counter_values hw_read_all(hw_handles_t const & handles)
{
    hw_counter_values ret{};
    std::ranges::transform(handles, ret.begin(), [](auto const & h) { return h ? hw_read(*h) : 0; });
    return ret;
}

} // namespace detail

/*!\brief Reports user-space instructions, cycles, taken and mispredicted branches of a benchmark as per-iteration counters.
 * \details
 *
 * Counting starts on construction; on destruction, the counts are added to `state.counters` as "instructions",
 * "cycles", "taken_branches" and "mispredicted_branches" (averaged over the iterations).
 *
 * Instruction counts do not depend on code placement, while times do. Equal instruction counts with different times
 * indicate layout or front-end effects; different instruction counts indicate differences in the generated code.
 * Taken branches also depend only on the generated code (not on the microarchitecture), but include the effect of
 * the block layout. Mispredicted branches depend on the data and on the microarchitecture's branch predictor.
 *
 * Only user space is counted, so interrupts and system calls do not contribute.
 *
 * Prefer radr::test::with_hardware_counters, which leaves the code of the benchmark function untouched. An object
 * of this type inside the benchmark function changes the compiler's decisions for the timed loop (observed: 3× the
 * instructions in one row, an accumulator spilled to the stack in another), although constructor and destructor
 * are never inlined. Use it directly only if the setup before the timed loop is too expensive to be counted.
 *
 * Backends:
 *   * FreeBSD: hwpmc via libpmc (the kernel module must be loaded).
 *   * Linux: libpfm and perf_event_open (requires an appropriate `kernel.perf_event_paranoid`).
 *
 * The backend is selected by the CMake option RADR_BENCHMARK_HW_COUNTERS. Counters that are not available (no
 * backend, failure at run-time or unknown event on this CPU) are not reported; a warning is printed once per counter.
 */
class hardware_counters
{
    benchmark::State &           state;
    detail::hw_handles_t const & handles = detail::hw_handles();
    detail::hw_counter_values    begin{};

public:
    [[gnu::noinline]] explicit hardware_counters(benchmark::State & s) : state{s}
    {
        begin = detail::hw_read_all(handles);
    }

    hardware_counters(hardware_counters const &)             = delete;
    hardware_counters & operator=(hardware_counters const &) = delete;

    [[gnu::noinline]] ~hardware_counters()
    {
        detail::hw_counter_values const end = detail::hw_read_all(handles);

        for (size_t i = 0; i < end.size(); ++i)
            if (handles[i])
                state.counters[detail::hw_counter_names[i]] =
                  benchmark::Counter(static_cast<double>(end[i] - begin[i]), benchmark::Counter::kAvgIterations);
    }
};

/*!\brief A benchmark function that calls `fn` and reports its hardware counters (see radr::test::hardware_counters).
 * \details
 *
 * Register this instead of `fn`, e.g. `BENCHMARK(radr::test::with_hardware_counters<foo<int>>)`.
 *
 * `fn` is called through an opaque pointer, so it is never inlined and its code is identical to a build without
 * counters. The counts therefore include everything `fn` does outside of the timed loop (typically the creation of
 * the range and entering/leaving the loop); this is a small, constant amount per run, divided by the number of
 * iterations.
 */
template <void (*fn)(benchmark::State &)>
void with_hardware_counters(benchmark::State & state)
{
    void (* const volatile f)(benchmark::State &) = fn;

    hardware_counters counters{state};
    f(state);
}

} // namespace radr::test
