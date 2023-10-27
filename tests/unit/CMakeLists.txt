cmake_minimum_required (VERSION 3.10)
project (radr_test_unit CXX)

include (../radr-test.cmake)

option (RADR_VERBOSE_TESTS "Run each test case individually" OFF)

function (radr_unit_test test_name)
    file (RELATIVE_PATH file_name "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_LIST_DIR}/${test_name}.cpp")
    set(target_name ${test_name})

    add_executable (${target_name} ${test_name}.cpp)
    target_link_libraries (${target_name} radr::test::unit)
    if (RADR_VERBOSE_TESTS)
        gtest_discover_tests(${file_name} TEST_PREFIX "${target_name}::" PROPERTIES TIMEOUT "30")
    else ()
        add_test (NAME "${file_name}" COMMAND ${target_name})
    endif ()

    unset (unit_test)
    unset (target_name)
    unset (file_name)
endfunction ()

enable_testing()

add_subdirectories ()
