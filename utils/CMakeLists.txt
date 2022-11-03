cmake_minimum_required(VERSION 3.5)

if(APPLE)
    if(EXISTS /usr/local/opt/openssl/)
        include_directories(/usr/local/opt/openssl/include)
        link_directories(/usr/local/opt/openssl/lib)
    endif()

    if(EXISTS /opt/homebrew/opt/openssl@1.1/)
        include_directories(/opt/homebrew/opt/openssl@1.1/include)
        link_directories(/opt/homebrew/opt/openssl@1.1/lib)
    endif()
endif()

link_libraries(crypto)
add_executable (GenerateFilter GenerateFilter.cpp)

if(APPLE)
    target_link_libraries (GenerateFilter LINK_PUBLIC BloomFilter crypto)
else()
    target_link_libraries (GenerateFilter LINK_PUBLIC BloomFilter)
endif()