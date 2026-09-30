#!/bin/bash
# The full simulation: random world + every automatic behaviour enabled
# (--item-dynamics: items grow, shrink/vanish and split on their own).
# Worst case for performance, so this doubles as the perf test.
# For the quiet, player-driven board use run-initial.command instead.
cd "$(dirname "$0")" && ./run.sh --item-dynamics "$@"