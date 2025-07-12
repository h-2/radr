cmake_minimum_required (VERSION 3.12)

find_path (RADR_CLONE_DIR NAMES cmake/radr-config.cmake HINTS "${CMAKE_CURRENT_LIST_DIR}/..")
find_path (RADR_INCLUDE_DIR NAMES radr/version.hpp HINTS "${RADR_CLONE_DIR}/include")

add_library (radr_radr INTERFACE)
target_include_directories (radr_radr INTERFACE "${RADR_INCLUDE_DIR}")
add_library (radr::radr ALIAS radr_radr)
