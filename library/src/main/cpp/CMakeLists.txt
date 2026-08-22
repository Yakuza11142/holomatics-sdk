cmake_minimum_required(VERSION 3.22.1)

project("tessnative")

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
