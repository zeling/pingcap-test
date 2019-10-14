cmake_minimum_required (VERSION 3.13)
project (top100)

find_package(Catch2 REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(libtop100 STATIC master.h master.cpp iterator.h memusage_allocator.h memusage_guard.h)
target_link_libraries(libtop100 Threads::Threads)

add_executable(top100 main.cpp)
target_link_libraries(top100 libtop100)

add_executable(test_main test_main.cpp test_heap.cpp test_iterator.cpp test_memusage_guard.cpp test_memusage_allocator.cpp test_master.cpp)
target_link_libraries(test_main Catch2::Catch2 libtop100)

# tests
include(CTest)
include(Catch)
catch_discover_tests(test_main)