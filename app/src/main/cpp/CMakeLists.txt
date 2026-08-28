cmake_minimum_required(VERSION 3.22.1)
project("TesseractEngineNativeCore")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add your core lock-free queues, VIO engines, and perception sorters
add_library(TessSDK SHARED
    tesseract_queue.c
    tesseract_occlusion.c
)

# Bind system logging frameworks for hardware diagnostic traces
find_library(log-lib log)

target_link_libraries(TessSDK
    ${log-lib}
)
