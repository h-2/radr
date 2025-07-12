cmake_minimum_required (VERSION 3.12)
project (radr_test_unit CXX)

message(STATUS "Beginning to configure RADR unit tests...")

include (../radr-test.cmake)

enable_testing()

set (RADR_UNIT_TESTS_SRC "${RADR_CLONE_DIR}/tests/unit/")

file (GLOB_RECURSE ENTRIES
      RELATIVE ${RADR_UNIT_TESTS_SRC}
      "${RADR_UNIT_TESTS_SRC}/[!.]*cpp")

foreach (ENTRY ${ENTRIES})
    radr_add_test(${ENTRY})
    get_filename_component (target "${ENTRY}" NAME_WLE)
    message(STATUS "Adding unit-test target '${target}' for file ${ENTRY}")
endforeach ()

message(STATUS "Configuring RADR unit tests done.")
