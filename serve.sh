#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-8080}"
DIR="$(cd "$(dirname "$0")/build/web" && pwd)"
echo "Serving $DIR at http://127.0.0.1:$PORT/ with live reload"
npx -y browser-sync start --server "$DIR" --port "$PORT" --files "$DIR/**" --no-notify --no-open
