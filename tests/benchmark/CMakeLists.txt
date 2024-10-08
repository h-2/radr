cmake_minimum_required (VERSION 3.7)
project (radr_benchmark CXX)

# ----------------------------------------------------------------------------
# Preamble
# ----------------------------------------------------------------------------

message(STATUS "Beginning to configure RADR micro benchmarks...")

if (CMAKE_BUILD_TYPE AND NOT ("${CMAKE_BUILD_TYPE}" STREQUAL "Release"))
    message (WARNING "Benchmarks should typically be built in Release mode! [build type = ${CMAKE_BUILD_TYPE}]")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------

option(RADR_BENCHMARK_ALIGN_LOOPS "Pass -falign-loops=32 to the benchmark builds." ON)

# ----------------------------------------------------------------------------
# Paths to folders.
# ----------------------------------------------------------------------------

find_path (RADR_BASE_DIR
           NAMES include/radr/rad/as_const.hpp
           HINTS "${CMAKE_CURRENT_LIST_DIR}/../../")
set (RADR_INCLUDE_DIR "${RADR_BASE_DIR}/include/")
set (RADR_TEST_INCLUDE_DIR "${RADR_BASE_DIR}/tests/include/")
set (RADR_UNIT_TESTS_SRC "${RADR_BASE_DIR}/tests/benchmark/")

# ----------------------------------------------------------------------------
# Components and dependencies
# ----------------------------------------------------------------------------

find_package(benchmark REQUIRED)
enable_testing ()

# ----------------------------------------------------------------------------
# Define test library
# ----------------------------------------------------------------------------

add_library (radr_benchmark INTERFACE)
target_compile_options (radr_benchmark INTERFACE "-pedantic"  "-Wall" "-Wextra" "-Werror")
target_link_libraries (radr_benchmark INTERFACE "pthread" benchmark::benchmark_main)
target_include_directories (radr_benchmark INTERFACE "${RADR_INCLUDE_DIR}" "${RADR_TEST_INCLUDE_DIR}")
add_library (radr::test::benchmark ALIAS radr_benchmark)

if (RADR_BENCHMARK_ALIGN_LOOPS)
    target_compile_options (radr_benchmark INTERFACE "-falign-loops=32")
endif ()

# ----------------------------------------------------------------------------
# Test target macro
# ----------------------------------------------------------------------------

macro (radr_test unit_test_cpp)
    get_filename_component (target_dir "${unit_test_cpp}" DIRECTORY)
    get_filename_component (target "${unit_test_cpp}" NAME_WLE)

    add_executable (${target} "${RADR_UNIT_TESTS_SRC}/${unit_test_cpp}")
    target_link_libraries (${target} radr::test::benchmark)

    file (MAKE_DIRECTORY ${target_dir})
    set_target_properties(${target} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${target_dir}")

    add_test (NAME "${target_dir}/${target}" COMMAND "${target_dir}/${target}")

    unset (target)
    unset (target_dir)
endmacro ()

# ----------------------------------------------------------------------------
# Start generating tests
# ----------------------------------------------------------------------------

file (GLOB_RECURSE ENTRIES
      RELATIVE ${RADR_BASE_DIR}/tests/benchmark
      "${RADR_UNIT_TESTS_SRC}/[!.]*cpp")

foreach (ENTRY ${ENTRIES})
    message(STATUS ${ENTRY})
    radr_test(${ENTRY})
endforeach ()

message(STATUS "Configuring RADR micro benchmarks DONE.")
