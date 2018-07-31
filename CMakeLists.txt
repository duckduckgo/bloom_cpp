cmake_minimum_required(VERSION 3.12)
project(ddg_bloom_cpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_COMPILER "clang++")

add_subdirectory(src)

add_subdirectory(test)
enable_testing()
add_test (NAME RunTests COMMAND Test)
