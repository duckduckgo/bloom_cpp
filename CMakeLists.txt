cmake_minimum_required(VERSION 3.12)
project(ddg_bloom_cpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_COMPILER "clang++")

enable_testing()

add_subdirectory(src)
add_subdirectory(test)