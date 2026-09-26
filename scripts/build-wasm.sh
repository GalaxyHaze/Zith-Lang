#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-wasm}"
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"

if [ ! -d "$EMSDK_DIR" ]; then
    echo "Error: emsdk directory not found at $EMSDK_DIR"
    exit 1
fi

export EMSDK_QUIET=1
source "$EMSDK_DIR/emsdk_env.sh"
export EM_CACHE="${EM_CACHE:-/tmp/emscripten_cache}"

echo "Configuring WASM build in '$BUILD_DIR'..."
emcmake cmake -S . -B "$BUILD_DIR" -DZITH_IS_WASM=ON -DCMAKE_C_COMPILER=emcc -DCMAKE_CXX_COMPILER=em++

echo "Building WASM target..."
cmake --build "$BUILD_DIR" -j$(nproc 2>/dev/null || echo 4)

ls -lh "$BUILD_DIR/zith-playground.wasm"
