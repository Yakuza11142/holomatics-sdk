#!/usr/bin/env bash
# Enforce strict error handling parameters to drop the pipeline if a step crashes
set -euo pipefail

# Default fallback values if no arguments are passed
SOURCE_FILE="${1:-tess_compiler.cpp}"
TARGET_OUTPUT="${2:-libtess_compiler.so}"

CXX="${CXX:-clang++}"
CXXFLAGS="${CXXFLAGS:--O3 -std=c++20}"

echo "⚙️ Initializing Dynamic Hardened Build Pipeline..."
echo "  📄 Source: ${SOURCE_FILE}"
echo "  🎯 Target: ${TARGET_OUTPUT}"

# Pre-execution guard: Ensure input source file exists
if [ ! -f "${SOURCE_FILE}" ]; then
    echo "❌ Error: Source file '${SOURCE_FILE}' not found."
    exit 1
fi

# Detect build target mode automatically based on file extension
EXTRA_FLAGS=()
if [[ "${TARGET_OUTPUT}" == *.so ]] || [[ "${TARGET_OUTPUT}" == *.dylib ]]; then
    echo "📦 Target identified as shared library. Applying -fPIC -shared flags..."
    EXTRA_FLAGS=(-fPIC -shared)
elif [[ "${TARGET_OUTPUT}" == *.dll ]]; then
    echo "📦 Target identified as Windows DLL. Applying dynamic library flags..."
    EXTRA_FLAGS=(-shared)
fi

echo "🔨 Executing native compilation step..."

# Dynamic compilation with hardened memory protection fences
${CXX} ${CXXFLAGS} "${EXTRA_FLAGS[@]}" \
    -fstack-protector-strong \
    -D_FORTIFY_SOURCE=2 \
    -Wformat -Werror=format-security \
    -Wall -Wextra \
    "${SOURCE_FILE}" -o "${TARGET_OUTPUT}"

# Output verification check
if [ -f "./${TARGET_OUTPUT}" ]; then
    echo "✅ Native asset compiled successfully: ./${TARGET_OUTPUT}"
else
    echo "❌ Critical compilation failure: Target asset output missing."
    exit 1
fi
