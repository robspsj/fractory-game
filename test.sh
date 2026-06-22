#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

GRID_LIMIT=""
EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --grid-limit)
            GRID_LIMIT="--grid-limit $2"
            shift 2
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

# Build native if needed
if [ ! -f build/native/fractory ]; then
    echo "==> Building native..."
    cmake -B build/native -S . &>/dev/null
    cmake --build build/native &>/dev/null
fi

echo "==> Running tests..."
echo ""
for test_file in tests/*.csv; do
    echo "--- $(basename "$test_file") ---"
    # shellcheck disable=SC2086
    ./build/native/fractory --test "$test_file" $GRID_LIMIT
    echo ""
done
echo "==> All tests complete."
