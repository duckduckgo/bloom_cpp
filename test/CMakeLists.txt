cmake_minimum_required(VERSION 3.12)

add_library(Catch ../third-party/catch2/single_include/catch2/catch.hpp empty.cpp)
target_include_directories(Catch PUBLIC ../third-party/catch2/single_include/catch2)

add_executable (RunTests BloomFilterTest.cpp)
target_link_libraries (RunTests LINK_PUBLIC Catch)
target_link_libraries (RunTests LINK_PUBLIC BloomFilter)

add_test(NAME RunTests COMMAND RunTests)
