#!/usr/bin/env bash
set -e

echo "🚀 Starting Smart Polyglot Tesseract Build Pipeline..."

# Locate Android NDK if building for Android targets
NDK_PATH="${ANDROID_NDK_LATEST_HOME}"
if [ -z "$NDK_PATH" ]; then
  NDK_PATH="${ANDROID_HOME}/ndk-bundle"
fi

TOOLCHAIN="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64"
CLANG_C="$TOOLCHAIN/bin/aarch64-linux-android24-clang"
CLANG_CC="$TOOLCHAIN/bin/aarch64-linux-android24-clang++"
INCLUDE_FLAGS="-I. -I./src"

# Track generated object/library archives
OBJECT_FILES=""
EXTERNAL_LIBS=""

# ============================================================================
# 1. RUST (.rs / Cargo.toml)
# ============================================================================
if [ -f "Cargo.toml" ]; then
  echo "🦀 [Detected] Rust workspace found. Compiling core..."
  rustup target add aarch64-linux-android 2>/dev/null || true
  cargo build --release --target aarch64-linux-android
  
  RUST_LIB=$(find target/aarch64-linux-android/release/ -name "*.a" | head -n 1)
  if [ -n "$RUST_LIB" ]; then
    EXTERNAL_LIBS="$EXTERNAL_LIBS $RUST_LIB"
    echo "📦 Linked Rust archive: $RUST_LIB"
  fi
fi

# ============================================================================
# 2. C (.c)
# ============================================================================
if [ -n "$(find . -maxdepth 3 -name "*.c" ! -name "TessBridge.c")" ]; then
  echo "🔍 [Detected] C source files found. Compiling..."
  for file in $(find . -maxdepth 3 -name "*.c" ! -name "TessBridge.c"); do
    echo "🔨 Compiling C: $file"
    $CLANG_C -c -fPIC -O3 -pthread $INCLUDE_FLAGS -include tesseract_engine.h -DTESS_API="" -Wno-macro-redefined "$file" -o "${file%.c}.o"
  done
fi

# ============================================================================
# 3. C++ (.cpp, .cc, .cxx)
# ============================================================================
if [ -n "$(find . -maxdepth 3 \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \))" ]; then
  echo "🔍 [Detected] C++ source files found. Compiling..."
  for file in $(find . -maxdepth 3 \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \)); do
    echo "🔨 Compiling C++: $file"
    $CLANG_CC -c -fPIC -O3 -pthread $INCLUDE_FLAGS -DTESS_API="" "$file" -o "${file%.*}.o"
  done
fi

# ============================================================================
# 4. SWIFT (.swift)
# ============================================================================
if [ -n "$(find . -maxdepth 3 -name "*.swift")" ]; then
  echo "🍏 [Detected] Swift source files found."
  # Swift cross-compilation hooks can be integrated here if targeting iOS/macOS frameworks
  echo "ℹ️ Swift modules detected (handled via Xcode/Swift Package Manager)."
fi

# ============================================================================
# 5. GO (.go)
# ============================================================================
if [ -n "$(find . -maxdepth 3 -name "*.go")" ]; then
  echo "🐹 [Detected] Go source files found. Building C-archive..."
  go build -buildmode=c-archive -o tessergo.a 2>/dev/null || echo "⚠️ Go build skipped or requires module init."
  if [ -f "tessergo.a" ]; then
    EXTERNAL_LIBS="$EXTERNAL_LIBS tessergo.a"
  fi
fi

# ============================================================================
# 6. C# (.cs)
# ============================================================================
if [ -n "$(find . -maxdepth 3 -name "*.cs")" ]; then
  echo "#. [Detected] C# source files found. Building via dotnet..."
  dotnet build -c Release 2>/dev/null || echo "⚠️ Dotnet build skipped."
fi

# ============================================================================
# 7. DART (.dart)
# ============================================================================
if [ -n "$(find . -maxdepth 3 -name "*.dart")" ]; then
  echo "🐦 [Detected] Dart / Flutter project files found."
  if [ -f "pubspec.yaml" ]; then
    echo "ℹ️ Pub package manager configuration verified."
  fi
fi

# ============================================================================
# 8. PYTHON (.py)
# ============================================================================
if [ -n "$(find . -maxdepth 3 -name "*.py")" ]; then
  echo "🐍 [Detected] Python scripts found."
  python3 -m py_compile $(find . -maxdepth 3 -name "*.py") 2>/dev/null || true
fi

# ============================================================================
# FINAL UNIFIED LINKING STEP
# ============================================================================
ALL_OBJECTS=$(find . -maxdepth 3 -name "*.o")

if [ -n "$ALL_OBJECTS" ] || [ -n "$EXTERNAL_LIBS" ]; then
  echo "🔨 Packaging final unified shared library: libTessSDK.so"
  $CLANG_CC -shared -fPIC -O3 -pthread \
            $ALL_OBJECTS \
            $EXTERNAL_LIBS \
            -landroid -llog \
            -o libTessSDK.so
  echo "✨ Smart Polyglot Build Complete Successfully!"
else
  echo "⚠️ No compiled object modules generated."
fi
