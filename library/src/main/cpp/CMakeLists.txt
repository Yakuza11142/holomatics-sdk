cmake_minimum_required(VERSION 3.22.1)
project("TessSDK")

# Optimization flags for Release builds across all devices
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections -flto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -flto")

# AUTOMATICALLY MAP ALL FILES:
# This scans the current directory and collects every .c and .cpp file into a variable called 'TESS_SOURCES'
file(GLOB TESS_SOURCES 
     "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Compile the library using the automatically generated file list
add_library(
    TessSDK
    SHARED
    ${TESS_SOURCES}
)

# Tell the compiler where your header files are located
target_include_directories(
    TessSDK PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(log-lib log)
target_link_libraries(TessSDK ${log-lib})
