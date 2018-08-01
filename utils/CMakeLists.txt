cmake_minimum_required(VERSION 3.12)

if(APPLE)
    include_directories(/usr/local/opt/openssl/include)
    link_directories(/usr/local/opt/openssl/lib)
    link_libraries(crypto)
endif()

add_executable (GenerateFilter GenerateFilter.cpp)

if(APPLE)
    target_link_libraries (GenerateFilter LINK_PUBLIC BloomFilter crypto)
else()
    target_link_libraries (GenerateFilter LINK_PUBLIC BloomFilter)
endif()