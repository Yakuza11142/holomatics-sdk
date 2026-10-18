cmake_minimum_required(VERSION 3.22.1)
project("TessSDK" LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization & LTO
add_compile_options("$<$<CONFIG:Release>:-O3;-fdata-sections;-ffunction-sections;-flto>")
add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-flto>")

set(CARGO_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Map Android ABIs to Rust target triples for zero-error linking
if(ANDROID)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(RUST_TARGET "aarch64-linux-android")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(RUST_TARGET "armv7-linux-androideabi")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(RUST_TARGET "i686-linux-android")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(RUST_TARGET "x86_64-linux-android")
    else()
        set(RUST_TARGET "")
    endif()
    
    if(RUST_TARGET)
        set(RUST_STATIC_LIB "${CARGO_MANIFEST_DIR}/target/${RUST_TARGET}/release/libtesseract_rust.a")
    else()
        set(RUST_STATIC_LIB "${CARGO_MANIFEST_DIR}/target/release/libtesseract_rust.a")
    endif()
else()
    set(RUST_STATIC_LIB "${CARGO_MANIFEST_DIR}/target/release/libtesseract_rust.a")
endif()

# 1. Automated Rust Component Compilation via Cargo
add_custom_target(rust_engine ALL
    COMMENT "Compiling all Rust modules for target..."
)

# 2. Automatically glob all C and C++ files in the directory
file(GLOB TESS_C_SOURCES CONFIGURE_DEPENDS "*.c" "*.cpp")

# 3. Define the Unified Shared Library Target
add_library(TessSDK SHARED ${TESS_C_SOURCES})

add_dependencies(TessSDK rust_engine)

target_include_directories(TessSDK PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TessSDK 
    PRIVATE 
    "${RUST_STATIC_LIB}"
    m
    pthread
)

# 4. Conditional Android JNI Integration
if(ANDROID)
    message(STATUS "Android target detected: Integrating TessBridge.c and NDK system libraries.")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/TessBridge.c")
        target_sources(TessSDK PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/TessBridge.c")
    endif()
    find_library(log-lib log)
    find_library(android-lib android)
    target_link_libraries(TessSDK PRIVATE ${log-lib} ${android-lib})
endif()
