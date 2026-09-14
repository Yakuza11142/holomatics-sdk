cmake_minimum_required(VERSION 3.22.1)
project("TesseractEngineNativeCore")

# Enforce Modern C++ Specifications
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Compiler Optimization Flags for Maximum Native Execution Speed
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra")

# Build Unified Target Shared Native Library Matrix
add_library(TessSDK SHARED
    # Core Engine Components
    tesseract_queue.c
    tesseract_occlusion.c
    
    # FIX: Added your custom compiler pipeline and FFI integration layers
    tess_compiler.cpp
    TessBridge.c
)

# Bind System Android Hardware Diagnostic Logging Frameworks
find_library(log-lib log)

# Link Shared Structural Subsystems Natively
target_link_libraries(TessSDK
    ${log-lib}
)
