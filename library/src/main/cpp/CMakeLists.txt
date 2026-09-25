cmake_minimum_required(VERSION 3.22.1)
project("tessnative")

# Keep optimization flags but ensure visibility is unblocked for JNI entry points
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections -flto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")

# Ensure CMake targets the file where it resides
add_library(
    tessnative
    SHARED
    tess_bridge.cpp 
)

# Tell the compiler where to look for tess_core.h
target_include_directories(
    tessnative PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(log-lib log)
target_link_libraries(tessnative ${log-lib})
