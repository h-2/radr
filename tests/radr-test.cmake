# -*- CMake -*-
#===----------------------------------------------------------------------===//
#
# Copyright (c) 2023-2025 Hannes Hauswedell
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See the LICENSE file for details.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//

cmake_minimum_required (VERSION 3.12)

# ----------------------------------------------------------------------------
# RADR SETUP
# ----------------------------------------------------------------------------

find_package(radr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/")

find_path (RADR_TEST_INCLUDE_DIR NAMES radr/test/gtest_helpers.hpp HINTS "${RADR_CLONE_DIR}/tests/include/" REQUIRED)

# ----------------------------------------------------------------------------
# GoogleTest
# ----------------------------------------------------------------------------

Include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        "v1.14.0" # release-1.10.0
)
FetchContent_MakeAvailable(googletest)

# ----------------------------------------------------------------------------
# Pseudo library to ease target definition
# ----------------------------------------------------------------------------

add_library (radr_test_base INTERFACE)
target_link_libraries (radr_test_base INTERFACE  radr::radr gtest_main gtest pthread)
target_include_directories (radr_test_base INTERFACE "${RADR_TEST_INCLUDE_DIR}")
target_compile_options (radr_test_base INTERFACE -pedantic  -Wall -Wextra -Werror)
add_library (radr::test::base ALIAS radr_test_base)

# ----------------------------------------------------------------------------
# radr_add_test macro
# ----------------------------------------------------------------------------

macro (radr_add_test unit_test_cpp)
    get_filename_component (target_dir "${unit_test_cpp}" DIRECTORY)
    get_filename_component (target "${unit_test_cpp}" NAME_WLE)

    add_executable (${target} "${unit_test_cpp}")
    target_link_libraries (${target} radr::test::base)

    file (MAKE_DIRECTORY ${target_dir})
    set_target_properties(${target} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${target_dir}")

    add_test (NAME "${target_dir}/${target}" COMMAND "${target_dir}/${target}")

    unset (target)
    unset (target_dir)
endmacro ()
