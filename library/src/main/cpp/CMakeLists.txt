cmake_minimum_required(VERSION 3.22.1)
project("TessSDK")

# Optimization flags for Release builds
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections -flto")
# NOTE: Removed '-fvisibility=hidden' and '-Wl,-s' to guarantee JNI exports stay visible
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")

# Rename the target from 'tessnative' to 'TessSDK' so System.loadLibrary("TessSDK") succeeds
add_library(
    TessSDK
    SHARED
    TessBridge.c # Or tess_bridge.cpp depending on your primary bridge choice
)

# Tell the compiler where your header files (like tess_core.h) are located
target_include_directories(
    TessSDK PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(log-lib log)
target_link_libraries(TessSDK ${log-lib})
