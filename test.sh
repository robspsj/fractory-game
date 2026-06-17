#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

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
    ./build/native/fractory --test "$test_file"
    echo ""
done
echo "==> All tests complete."
