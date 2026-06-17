#!/bin/bash
cd "$(dirname "$0")" || exit 1

# Configuration
BUILD_DIR="build/native"
EXECUTABLE="$BUILD_DIR/fractory"

# Build first
echo "==> Building native..."
cmake -B build/native -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=1 &>/dev/null
cmake --build build/native &>/dev/null

# Run the program
echo "==> Running fractory..."
echo ""
"$EXECUTABLE"
