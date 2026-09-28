cmake_minimum_required(VERSION 3.22.1)
project("TessSDK" LANGUAGES C CXX)

# Force modern C11 and C++17 standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Target-Specific Optimization Flags (Safer than globally appending strings)
add_compile_options("$<$<CONFIG:Release>:-O3;-fdata-sections;-ffunction-sections;-flto>")
add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-flto>")

# 2. Explicitly define C/C++ Source Files (Avoids GLOB build-sync bugs)
set(TESS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_lighting.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_occlusion.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_ops.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_unified.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_pro.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_queue.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/velo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/TessBridge.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_engine.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_concurrency.c"
)

# 3. Define the Shared Library Target
add_library(TessSDK SHARED ${TESS_SOURCES})

# 4. Configure Include Paths
target_include_directories(TessSDK PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 5. Locate and Link Android NDK System Dependencies
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(TessSDK 
    PRIVATE 
    ${log-lib}
    ${android-lib}
    # If you compiled your Rust modules into 'libtesseract_rust.a', uncomment the next line:
    # "${CMAKE_CURRENT_SOURCE_DIR}/path_to_rust_libs/${ANDROID_ABI}/libtesseract_rust.a"
)
