cmake_minimum_required (VERSION 3.10)

project(radr_clang_format NONE)

find_program(CLANG_FORMAT NAMES clang-format-17 clang-format17 clang-format)

if(CLANG_FORMAT)
    message(STATUS "Found clang-format: ${CLANG_FORMAT}")

    add_custom_target (check_format ALL find
                       "${CMAKE_CURRENT_SOURCE_DIR}/../../include/radr/"
                       "${CMAKE_CURRENT_SOURCE_DIR}/../benchmark"
                       "${CMAKE_CURRENT_SOURCE_DIR}/../unit/"
                       "${CMAKE_CURRENT_SOURCE_DIR}/../include/"
                       -name '*.[ch]pp' -exec ${CLANG_FORMAT} -style=file -n -Werror {} +
                       COMMENT "Checking the files with clang-format.")

    add_custom_target (format find
                       "${CMAKE_CURRENT_SOURCE_DIR}/../../include/radr/"
                       "${CMAKE_CURRENT_SOURCE_DIR}/../benchmark"
                       "${CMAKE_CURRENT_SOURCE_DIR}/../unit/"
                       "${CMAKE_CURRENT_SOURCE_DIR}/../include/"
                       -name '*.[ch]pp' -exec ${CLANG_FORMAT} -style=file -i {} +
                       COMMENT "Format the unit tests with clang-format.")
else ()
    message(FATAL_ERROR "clang-format not found. No targets made.")
endif()
