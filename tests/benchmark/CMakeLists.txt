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

option(RADR_BENCHMARK_ALIGN_LOOPS "Pass -falign-loops=32 to the benchmark builds." ON)

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

if (RADR_BENCHMARK_ALIGN_LOOPS)
    target_compile_options (radr_benchmark INTERFACE "-falign-loops=32")
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
