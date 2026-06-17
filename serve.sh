#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Building for web (Emscripten)..."
if command -v emcmake &> /dev/null; then
    emcmake cmake -B build/web -S . &>/dev/null
    cmake --build build/web &>/dev/null
else
    echo "Error: emcmake (Emscripten) not found in PATH."
    exit 1
fi

PORT="${1:-8080}"
DIR="$(cd "$(dirname "$0")/build/web" && pwd)"
echo "==> Serving $DIR at http://127.0.0.1:$PORT/ with live reload"
npx -y browser-sync start --server "$DIR" --port "$PORT" --files "$DIR/**" --no-notify --no-open
