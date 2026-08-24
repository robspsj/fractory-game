#!/bin/bash
# Double-clickable launcher for the blend2d game.
# Configures (if needed), builds, and runs the native macOS executable.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
EXE="$BUILD_DIR/fractory"

# Native build dir is tracked by .gitignore, so it may not exist on a fresh clone.
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring native build..."
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
fi

echo "Building..."
cmake --build "$BUILD_DIR" -j

if [ ! -x "$EXE" ]; then
    echo "Error: missing executable $EXE"
    exit 1
fi

echo "Launching..."
cd "$SCRIPT_DIR"
exec "$EXE"