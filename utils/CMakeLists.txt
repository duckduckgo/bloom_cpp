cmake_minimum_required(VERSION 3.5)

if(APPLE)
    if(EXISTS /usr/local/opt/openssl/)
        set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
    elseif(EXISTS /opt/homebrew/opt/openssl@1.1/)
        set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@1.1)
    endif()
elseif(WIN32)
    if(EXISTS "C:/Program Files/OpenSSL-Win64")
        set(OPENSSL_ROOT_DIR "C:/Program FIles/OpenSSL-Win64")
    endif()
endif()

find_package (OpenSSL REQUIRED)

add_executable (GenerateFilter GenerateFilter.cpp)
target_link_libraries (GenerateFilter LINK_PUBLIC BloomFilter OpenSSL::Crypto)
