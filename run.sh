#!/bin/bash
cd "$(dirname "$0")" || exit 1

# Skia/SDL2 native build + run (Ninja generator). Extra args pass to the game.
BUILD_DIR="build-native"
EXECUTABLE="$BUILD_DIR/fractory"

echo "==> Building native (Skia/SDL2)..."
cmake -B "$BUILD_DIR" -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=1 >/dev/null
cmake --build "$BUILD_DIR" >/dev/null

echo "==> Running fractory..."
echo ""
"$EXECUTABLE" "$@"