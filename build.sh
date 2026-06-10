#!/usr/bin/env bash
set -euo pipefail
MODE="${1:-native}"

if [ "$MODE" = "web" ]; then
    echo "==> Building for web (Emscripten)..."
    emcmake cmake -B build/web -S .
    cmake --build build/web
    echo "Artifacts in build/web/"
    ls -lh build/web/fractory.wasm 2>/dev/null || echo "(wasm not found)"
else
    echo "==> Building native..."
    cmake -B build/native -S .
    cmake --build build/native
    ls -lh build/native/fractory
    echo "Run with: build/native/fractory"
fi
