# -*- CMake -*-
#===----------------------------------------------------------------------===//
#
# Copyright (c) 2023 Hannes Hauswedell
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See the LICENSE file for details.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===//

cmake_minimum_required (VERSION 3.10)

# ----------------------------------------------------------------------------
# RADR SETUP
# ----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_path (RADR_INCLUDE_DIR NAMES radr/as_const.hpp HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../../include/" REQUIRED)
find_path (RADR_TEST_INCLUDE_DIR NAMES radr/test/gtest_helpers.hpp HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../include/" REQUIRED)

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

add_library (radr_test_unit INTERFACE)
target_link_libraries (radr_test_unit INTERFACE  gtest_main gtest pthread)
target_include_directories (radr_test_unit INTERFACE "${RADR_INCLUDE_DIR}" "${RADR_TEST_INCLUDE_DIR}")
target_compile_options (radr_test_unit INTERFACE -pedantic  -Wall -Wextra -Werror)
add_library (radr::test::unit ALIAS radr_test_unit)

# ----------------------------------------------------------------------------
# add_subdirectories
# ----------------------------------------------------------------------------

macro (add_subdirectories_of directory)
    file (GLOB ENTRIES
          RELATIVE ${directory}
          ${directory}/[!.]*)

    foreach (ENTRY ${ENTRIES})
        if (IS_DIRECTORY ${directory}/${ENTRY})
            if (EXISTS ${directory}/${ENTRY}/CMakeLists.txt)
                add_subdirectory (${directory}/${ENTRY} ${CMAKE_CURRENT_BINARY_DIR}/${ENTRY})
            endif ()
        endif ()
    endforeach ()
    unset (ENTRIES)
endmacro ()

macro (add_subdirectories)
    add_subdirectories_of(${CMAKE_CURRENT_SOURCE_DIR})
endmacro ()
