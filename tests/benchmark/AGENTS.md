# Benchmarks

`rad/chunk.cpp` follows the setup described here; the other benchmarks do not yet.

## Purpose

* Show how an adaptor performs on different underlying ranges, and how it compares to `std::`.
* Adaptors combine, so the possible underlying ranges are endless (range concepts × memory access patterns). The
  standard containers are used as representatives:
  * `std::vector`: contiguous; the cheapest possible base.
  * `std::deque`: random-access but not contiguous; a branch at every block boundary.
  * `std::list`: bidirectional + common, nodes not adjacent in memory. Rarely used directly, but a baseline for
    these properties.
  * `std::vector | take_while(...)`: forward, not common, not sized. Use a predicate that reads the element
    (`vec_tp_t`); a constant predicate (`vec_tw_t`) is optimised away.
* Passing numeric arguments: some adaptors take additional arguments, often numbers. These can be passed within a type,
  but it should be clear whether the benchmark's goal is to measure constants (make sure they are propagated as such!)
  or user-provided arguments (in which case going through an intermediate volatile can prevent undesired optimisation).
* Microbenchmarks are diagnostic. Larger use-case benchmarks (e.g. k-mer hashing, moving averages) should be added
  in the future.

## Structure

* Data: 65'536 `uint32_t` (256 KiB, L2-resident); the list has `data_size / 8` nodes (same footprint). Larger data
  measures the memory system and physical placement, not the adaptor; the results then depend on code layout.
* Helpers that create the ranges are `[[gnu::always_inline]]`, so the sizes stay compile-time constants in the timed
  loop.
* Access patterns: `full` (read everything), `stride` (random access at specific interval), `*_rev` (only for `lst_t`);
  `partial` (for adaptors that produce range-of-ranges, access only first two elements of each chunk).
* Names: `<container>/<pattern>/<n>/<arm>`, e.g. `lst_t/full/n4/radr`; arms are `std` and `radr`, and in some cases
  additional ones if there are different flavours. Register them via the `RADR_BENCH_*` macros; `std`
  arms are skipped if the std adaptor does not exist (C++20).

## Hardware counters

* Every arm is registered as `radr::test::with_hardware_counters<fn>` (`tests/include/radr/test/hardware_counters.hpp`).
  It reports `instructions` and `cycles` per iteration, user space only.
* Never put counting code inside a benchmark function: it changes the compiler's decisions for the timed loop
  (observed: 3× the instructions, a spilled accumulator). The wrapper calls `fn` through an opaque pointer, so `fn`
  is identical to a build without counters.
* Backends: hwpmc on FreeBSD, libpfm + `perf_event_open` on Linux (untested). CMake option
  `RADR_BENCHMARK_HW_COUNTERS`; without a backend, no counters are reported.
* Instruction counts are reproducible to ~1e-7 and independent of code placement; cycles track time (≈4.13 GHz on
  the reference machine).

Interpreting mismatches:

| observation | likely cause |
|---|---|
| same instructions, different cycles | code/data placement, front-end effects |
| different instructions | different generated code (inlining, unrolling, vectorisation); inspect the assembly |
| more instructions, same cycles | latency-bound loop, e.g. `lst_t` (bound by the dependent `next` loads, ~4 cycles per node); extra work runs in parallel |
| fewer instructions, more cycles | branch mispredicts (e.g. `deq_t` block boundaries) or dependency chains |
| time higher than cycles / frequency | time spent outside user space (interrupts, page faults); discard the run |

## Build and run

* Build in Release. `RADR_REPRODUCIBLE_BENCHMARKS` (default `ON`) aligns loops and jump targets to 64 bytes;
  labels are deliberately not aligned (executed NOPs).
* Run pinned to one core, with
  `--benchmark_repetitions=3 --benchmark_enable_random_interleaving=true --benchmark_min_warmup_time=0.5`
* Keep the SMT sibling of the benchmark core idle; a busy sibling slows all arms by up to 2×. Runs whose repetitions
  jump by ~2× within one process are disturbed and must be discarded.
* ASLR is not a source of variance (tested); no need to disable it.
* Before acting on a difference, check that the hot loop's code is what is expected; alignment and counters cannot
  fix changing compiler decisions.
