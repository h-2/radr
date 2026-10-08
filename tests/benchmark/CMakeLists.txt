cmake_minimum_required (VERSION 3.12)
project (radr_test_benchmark CXX)

include(FetchContent)

# ----------------------------------------------------------------------------
# Preamble
# ----------------------------------------------------------------------------

message(STATUS "Beginning to configure RADR micro benchmarks...")
include (../radr-test.cmake)

if (CMAKE_BUILD_TYPE AND NOT ("${CMAKE_BUILD_TYPE}" STREQUAL "Release"))
    message (WARNING "Benchmarks should typically be built in Release mode! [build type = ${CMAKE_BUILD_TYPE}]")
endif ()

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------

option(RADR_REPRODUCIBLE_BENCHMARKS "Align loops and jump targets to 64 bytes, so that results do not depend on unrelated code changes." ON)
option(RADR_BENCHMARK_HW_COUNTERS "Report hardware instruction and cycle counts (hwpmc on FreeBSD, libpfm on Linux)." ON)

# ----------------------------------------------------------------------------
# Paths to folders.
# ----------------------------------------------------------------------------

set (RADR_BENCHMARKS_DIR "${RADR_CLONE_DIR}/tests/benchmark/")

# ----------------------------------------------------------------------------
# Components and dependencies
# ----------------------------------------------------------------------------

enable_testing ()

set(BENCHMARK_ENABLE_TESTING NO)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG "v1.8.3"
)

FetchContent_MakeAvailable(googlebenchmark)

# ----------------------------------------------------------------------------
# Define test library
# ----------------------------------------------------------------------------

add_library (radr_benchmark INTERFACE)
target_link_libraries (radr_benchmark INTERFACE "pthread" benchmark::benchmark_main)
add_library (radr::test::benchmark ALIAS radr_benchmark)

# Aligning only loop heads is not sufficient, because hot loops are often entered via jump targets.
# Labels are deliberately not aligned, because that inserts executed NOPs into hot loops.
if (RADR_REPRODUCIBLE_BENCHMARKS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options (radr_benchmark INTERFACE "-falign-loops=64" "-falign-jumps=64")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options (radr_benchmark INTERFACE "-falign-loops=64" "SHELL:-mllvm -align-all-nofallthru-blocks=6")
    endif ()
endif ()

# Hardware counters, see radr/test/hardware_counters.hpp.
if (RADR_BENCHMARK_HW_COUNTERS)
    if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        find_library (RADR_PMC_LIBRARY pmc)
        find_path (RADR_PMC_INCLUDE_DIR pmc.h)
        if (RADR_PMC_LIBRARY AND RADR_PMC_INCLUDE_DIR)
            target_link_libraries (radr_benchmark INTERFACE "${RADR_PMC_LIBRARY}")
            target_compile_definitions (radr_benchmark INTERFACE RADR_BENCHMARK_HWPMC)
            set (RADR_HW_COUNTERS_FOUND ON)
        endif ()
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_library (RADR_PFM_LIBRARY pfm)
        find_path (RADR_PFM_INCLUDE_DIR perfmon/pfmlib_perf_event.h)
        if (RADR_PFM_LIBRARY AND RADR_PFM_INCLUDE_DIR)
            target_link_libraries (radr_benchmark INTERFACE "${RADR_PFM_LIBRARY}")
            target_include_directories (radr_benchmark INTERFACE "${RADR_PFM_INCLUDE_DIR}")
            target_compile_definitions (radr_benchmark INTERFACE RADR_BENCHMARK_LIBPFM)
            set (RADR_HW_COUNTERS_FOUND ON)
        endif ()
    endif ()

    if (NOT RADR_HW_COUNTERS_FOUND)
        message (WARNING "Hardware counters requested, but neither libpmc (FreeBSD) nor libpfm (Linux) was found.")
    endif ()
endif ()

# ----------------------------------------------------------------------------
# Start generating tests
# ----------------------------------------------------------------------------

file (GLOB_RECURSE ENTRIES
      RELATIVE ${RADR_BENCHMARKS_DIR}
      "${RADR_BENCHMARKS_DIR}/[!.]*cpp")

foreach (ENTRY ${ENTRIES})
    radr_add_test(${ENTRY})
    get_filename_component (target "${ENTRY}" NAME_WLE)
    message(STATUS "Adding benchmark target '${target}' for file ${ENTRY}")
    target_link_libraries (${target} radr::test::benchmark)
endforeach ()

message(STATUS "Configuring RADR micro benchmarks DONE.")
