#!/usr/bin/env bash
# Enforce strict error handling parameters to drop the pipeline if a step crashes
set -euo pipefail

# 1. CONFIGURE YOUR NDK PATH (Update this path to match your machine's setup)
ANDROID_NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/26.1.10909125}"

if [ ! -d "$ANDROID_NDK" ]; then
    echo "❌ Error: Android NDK not found at: $ANDROID_NDK"
    echo " Please set the CORRECT path using: export ANDROID_NDK=/your/path"
    exit 1
fi

# Define the 4 standard Android Target Architectures
ARCHS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
MIN_SDK="29" # Hardcoded to 29 due to 'memfd_create' requirements

echo "⚙️ Initializing Universal Android Cross-Compilation Pipeline..."

# Clean out any old build folders
rm -rf build_output
mkdir -p build_output

# Loop and compile for every platform dynamically
for ABI in "${ARCHS[@]}"; do
    echo "=========================================================="
    echo "🔨 Compiling TessSDK for target platform: $ABI"
    echo "=========================================================="
    
    # Create isolated output directory structures
    BUILD_DIR="build_output/cmake_${ABI}"
    TARGET_DIR="build_output/jniLibs/${ABI}"
    mkdir -p "$BUILD_DIR"
    mkdir -p "$TARGET_DIR"
    
    # Run CMake pointing directly to the Android NDK cross-compilation toolchain
    cmake -B "$BUILD_DIR" -H. \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$MIN_SDK" \
        -DCMAKE_BUILD_TYPE=Release \
        -DANDROID_STL=c++_shared

    # Compile the files
    cmake --build "$BUILD_DIR" --config Release --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
    
    # Copy the finished library out to the deployment directory
    if [ -f "$BUILD_DIR/libTessSDK.so" ]; then
        mv "$BUILD_DIR/libTessSDK.so" "$TARGET_DIR/libTessSDK.so"
        echo "✅ Generated Universal Asset: $TARGET_DIR/libTessSDK.so"
    else
        echo "❌ Critical compilation failure: Target asset output missing for $ABI."
        exit 1
    fi
done

echo "=========================================================="
echo "🎉 Pipeline Completed! Universal binaries are inside the './build_output/jniLibs/' directory."
echo "   Copy the 'jniLibs' folder directly into your Android Studio app project directory."
echo "=========================================================="
