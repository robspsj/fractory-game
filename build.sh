#!/usr/bin/env bash
set -euo pipefail
MODE="${1:-native}"

if [ "$MODE" = "web" ]; then
    echo "==> Building for web (Emscripten)..."
    emcmake cmake -B build/web -S .
    cmake --build build/web
    echo "Artifacts in build/web/fractory.html"
else
    echo "==> Building native..."
    cmake -B build/native -S .
    cmake --build build/native
    echo "Run with: build/native/fractory"
fi
