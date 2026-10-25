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

# Track generated external library archives
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
# 2. C / C++ EXPLICIT UNIFIED COMPILATION (Preventing Duplicate Symbols)
# ============================================================================
echo "🔍 Compiling explicit unified C source modules..."

C_FILES=(
  "tesseract_unified.c"
  "tesseract_pro.c"
)

for file in "${C_FILES[@]}"; do
  if [ -f "$file" ]; then
    echo "🔨 Compiling C: $file"
    $CLANG_C -c -fPIC -O3 -pthread $INCLUDE_FLAGS -DTESS_API="" -Wno-macro-redefined "$file" -o "${file%.c}.o"
  else
    echo "⚠️ Warning: Required file '$file' not found."
  fi
done

# Compile C++ Bridge if present
if [ -f "TessBridge.c" ]; then
  echo "🔨 Compiling C++ Bridge: TessBridge.c"
  $CLANG_CC -c -fPIC -O3 -pthread $INCLUDE_FLAGS -DTESS_API="" "TessBridge.c" -o "TessBridge.o"
elif [ -f "src/TessBridge.cpp" ]; then
  echo "🔨 Compiling C++ Bridge: src/TessBridge.cpp"
  $CLANG_CC -c -fPIC -O3 -pthread $INCLUDE_FLAGS -DTESS_API="" "src/TessBridge.cpp" -o "src/TessBridge.o"
fi

# ============================================================================
# 3. OTHER LANGUAGE HOOKS (Go, Python, etc.)
# ============================================================================
if [ -f "go.mod" ] || [ -n "$(find . -maxdepth 3 -name "*.go" 2>/dev/null)" ]; then
  echo "🐹 [Detected] Go source files found. Building C-archive..."
  go build -buildmode=c-archive -o tessergo.a 2>/dev/null || echo "⚠️ Go build skipped."
  if [ -f "tessergo.a" ]; then
    EXTERNAL_LIBS="$EXTERNAL_LIBS tessergo.a"
  fi
fi

if [ -n "$(find . -maxdepth 3 -name "*.py" 2>/dev/null)" ]; then
  echo "🐍 [Detected] Python scripts found."
  python3 -m py_compile $(find . -maxdepth 3 -name "*.py" 2>/dev/null) 2>/dev/null || true
fi

# ============================================================================
# FINAL UNIFIED LINKING STEP
# ============================================================================
# Explicitly gather valid objects while filtering out legacy conflicting modules
ALL_OBJECTS=$(find . -maxdepth 2 -name "*.o" ! -name "tesseract_core.o" ! -name "tesseract_engine.o")

if [ -n "$ALL_OBJECTS" ] || [ -n "$EXTERNAL_LIBS" ]; then
  echo "🔨 Packaging final unified shared library: libTessSDK.so"
  $CLANG_CC -shared -fPIC -O3 -pthread \
            $ALL_OBJECTS \
            $EXTERNAL_LIBS \
            -landroid -llog -lm \
            -o libTessSDK.so
  echo "✨ Smart Polyglot Build Complete Successfully!"
else
  echo "⚠️ No compiled object modules generated."
fi
