#!/bin/bash
cd "$(dirname "$0")"

if ! command -v emcmake &> /dev/null; then
    echo "Error: emcmake (Emscripten) not found in PATH."
    exit 1
fi

emcmake cmake -B build/web -S . &>/dev/null
cmake --build build/web &>/dev/null

PORT=8080
DIR="$(pwd)/build/web"

# Start server in background if not already running
if ! lsof -i :"$PORT" &>/dev/null; then
    npx -y browser-sync start --server "$DIR" --port "$PORT" --files "$DIR/**" --no-notify --no-open &>/dev/null &
fi

open "http://127.0.0.1:$PORT/fractory.html"
