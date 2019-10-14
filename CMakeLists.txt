cmake_minimum_required (VERSION 3.13)
project (top100)

find_package(Catch2 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_library(libsstable STATIC sstable.cpp MurmurHash3.cpp)
add_library(libtop100 STATIC worker.h worker.cpp)
add_executable(top100 main.cpp)
# target_link_libraries(top100 libsstable)

add_executable(test_main test_main.cpp test_heap.cpp test_iterator.cpp test_memusage_guard.cpp)
target_link_libraries(test_main Catch2::Catch2)

# tests
include(CTest)
include(Catch)
catch_discover_tests(test_main)