#!/usr/bin/env bash
# Enforce strict error handling parameters to drop the pipeline if a step crashes
set -euo pipefail

# 1. CONFIGURE YOUR NDK PATH
ANDROID_NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/26.1.10909125}"

if [ ! -d "$ANDROID_NDK" ]; then
    echo "❌ Error: Android NDK not found at: $ANDROID_NDK"
    echo " Please set the CORRECT path using: export ANDROID_NDK=/your/path"
    exit 1
fi

# Define ABIs and their corresponding Rust target triples
declare -A ABI_TARGETS=(
    ["arm64-v8a"]="aarch64-linux-android"
    ["armeabi-v7a"]="armv7-linux-androideabi"
    ["x86"]="i686-linux-android"
    ["x86_64"]="x86_64-linux-android"
)

MIN_SDK="29"

echo "⚙️ Initializing Universal Android Cross-Compilation Pipeline..."

rm -rf build_output
mkdir -p build_output

for ABI in "${!ABI_TARGETS[@]}"; do
    RUST_TARGET="${ABI_TARGETS[$ABI]}"
    echo "=========================================================="
    echo "🔨 Building Rust & TessSDK for ABI: $ABI (Target: $RUST_TARGET)"
    echo "=========================================================="

    # Ensure the Rust target toolchain is installed
    rustup target add "$RUST_TARGET" 2>/dev/null || true

    # Cross-compile Rust static library first so it exists for CMake linking
    cargo build --release --target "$RUST_TARGET" --manifest-path "Cargo.toml"

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

    # Compile native components
    cmake --build "$BUILD_DIR" --config Release --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

    if [ -f "$BUILD_DIR/libTessSDK.so" ]; then
        mv "$BUILD_DIR/libTessSDK.so" "$TARGET_DIR/libTessSDK.so"
        echo "✅ Generated Universal Asset: $TARGET_DIR/libTessSDK.so"
    else
        echo "❌ Critical compilation failure: Target asset output missing for $ABI."
        exit 1
    fi
done

echo "=========================================================="
echo "🎉 Pipeline Completed! Zero errors achieved. Binaries are in './build_output/jniLibs/'."
echo "=========================================================="
