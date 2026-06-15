#!/usr/bin/env bash
set -euo pipefail

echo "==> Building native..."
cmake -B build/native -S .
cmake --build build/native
ls -lh build/native/fractory
echo "Run with: build/native/fractory"

echo "==> Building for web (Emscripten)..."
if command -v emcmake &> /dev/null; then
    emcmake cmake -B build/web -S .
    cmake --build build/web
    echo "Artifacts in build/web/"
    ls -lh build/web/fractory.wasm 2>/dev/null || echo "(wasm not found)"
else
    echo "Warning: emcmake (Emscripten) not found in PATH. Skipping web build."
fi
