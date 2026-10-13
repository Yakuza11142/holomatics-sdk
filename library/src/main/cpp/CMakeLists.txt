cmake_minimum_required(VERSION 3.22.1)
project("TessSDK" LANGUAGES C CXX)

# Force modern C11 and C++17 standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Target-Specific Optimization Flags
add_compile_options("$<$<CONFIG:Release>:-O3;-fdata-sections;-ffunction-sections;-flto>")
add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-flto>")

# 2. Core Universal Source Files (Compiled on ALL platforms: Drones, Linux, Windows, iOS, Android)
set(TESS_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_lighting.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_occlusion.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_ops.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_queue.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/velo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_engine.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_concurrency.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tesseract_security.c"
)

# 3. Define the Universal Shared Library Target
add_library(TessSDK SHARED ${TESS_CORE_SOURCES})

# 4. Configure Include Paths
target_include_directories(TessSDK PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 5. Conditionally Add Android JNI Bindings (Only when targeting Android)
if(ANDROID)
    message(STATUS "Android target detected: Integrating optional JNI bindings.")
    target_sources(TessSDK PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/TessBridge.c")
    
    find_library(log-lib log)
    find_library(android-lib android)
    
    target_link_libraries(TessSDK 
        PRIVATE 
        ${log-lib}
        ${android-lib}
    )
endif()

# Optional: Link pre-compiled Rust spatial/math static libraries if present
# target_link_libraries(TessSDK PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libtesseract_rust.a")
