cmake_minimum_required (VERSION 3.12)

project (radr LANGUAGES CXX
         DESCRIPTION "Range Adaptors Reimagined (radr)."
         HOMEPAGE_URL "https://github.com/h-2/radr")

message(STATUS "Loading the RADR library and implicitly calling find_package.")
find_package (radr REQUIRED HINTS ${CMAKE_CURRENT_LIST_DIR}/cmake)
message(STATUS "Add radr::radr to your program's target_link_libraries.")

message(STATUS "ATTENTION: Including the project root only includes the library.")
message(STATUS "ATTENTION: To build unit tests, benchmarks or other utilitites, directly cmake one of the directories in tests/ .")
