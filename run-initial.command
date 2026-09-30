#!/bin/bash
# The quiet board: empty starting grid with every automatic behaviour OFF
# (the defaults — nothing grows, shrinks or splits until the player acts).
# For the self-running simulation use run.command instead.
cd "$(dirname "$0")" && ./run.sh --empty-grid "$@"
