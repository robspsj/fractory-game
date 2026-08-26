#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# Run the CSV model test suite. `fractory --test <file.csv>` is headless (no
# window) — it drives the GameModel directly and exits 0/1. Some CSVs are data
# inputs (reactions/spawns/station_recipes); scenario tests are test_*.csv.
# Note: a handful of scenario tests currently fail identically on the old
# renderer too (pre-existing model expectations), so a FAIL here is not a
# regression — it surfaces the stale expectations to fix separately.
BUILD_DIR="build-native"
if [ ! -f "$BUILD_DIR/fractory" ]; then
  echo "==> Building native..."
  cmake -B "$BUILD_DIR" -S . >/dev/null
  cmake --build "$BUILD_DIR" >/dev/null
fi

echo "==> Running tests..."
echo ""
fail=0
for f in tests/*.csv; do
  if ./"$BUILD_DIR/fractory" --test "$f" >/dev/null 2>&1; then
    echo "PASS  $(basename "$f")"
  else
    echo "FAIL  $(basename "$f")"
    fail=1
  fi
done
echo ""
echo "==> All tests complete. ($([ $fail -eq 0 ] && echo ALL PASS || echo SOME FAILED))"
exit $fail