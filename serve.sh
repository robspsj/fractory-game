#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# Build for web (Emscripten, Skia/SDL2) and serve build-wasm2/.
# Emscripten's emcc shells to `python3`; Homebrew's wrapper otherwise picks
# Xcode's 3.9 (too old), so we force python3.14 onto PATH for the build.
export PATH="/opt/homebrew/opt/python@3.14/bin:$PATH"

echo "==> Building for web (Emscripten / Skia / SDL2)..."
if command -v emcmake &>/dev/null; then
  emcmake cmake -B build-wasm2 -S . >/dev/null
  cmake --build build-wasm2 >/dev/null
else
  echo "Error: emcmake (Emscripten) not found in PATH."
  exit 1
fi

PORT="${1:-8080}"
DIR="$(cd build-wasm2 && pwd)"

if lsof -i :"$PORT" &>/dev/null; then
  echo "==> Server already running at http://127.0.0.1:$PORT/ (rebuild complete)"
  exit 0
fi

echo "==> Serving $DIR at http://127.0.0.1:$PORT/ with live reload"
if command -v npx &>/dev/null; then
  npx -y browser-sync start --server "$DIR" --port "$PORT" --files "$DIR/**" --no-notify --no-open
else
  python3 -m http.server "$PORT" --directory "$DIR"
fi