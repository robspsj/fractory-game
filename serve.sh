#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-8080}"
DIR="$(cd "$(dirname "$0")/build/web" && pwd)"
echo "Serving $DIR at http://127.0.0.1:$PORT/fractory.html"
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIR"
