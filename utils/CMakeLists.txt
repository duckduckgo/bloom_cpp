cmake_minimum_required(VERSION 3.5)

add_executable (GenerateFilter GenerateFilter.cpp)

if(APPLE)
    if(EXISTS /usr/local/opt/openssl/)
        set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
    elseif(EXISTS /opt/homebrew/opt/openssl@1.1/)
        set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@1.1)
    endif()
endif()

find_package (OpenSSL REQUIRED)
target_link_libraries (GenerateFilter LINK_PUBLIC BloomFilter OpenSSL::Crypto)
