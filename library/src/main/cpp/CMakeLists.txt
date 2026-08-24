cmake_minimum_required(VERSION 3.22.1)

project("tessnative")

# Set C/C++ Optimization and Obfuscation Flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fvisibility=hidden -fdata-sections -ffunction-sections -flto")

# Linker flags to strip all symbol tables and discard unused sections
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s -Wl,--exclude-libs,ALL")

add_library(
    tessnative
    SHARED
    tess_bridge.cpp
)

find_library(
    log-lib
    log
)

target_link_libraries(
    tessnative
    ${log-lib}
)
