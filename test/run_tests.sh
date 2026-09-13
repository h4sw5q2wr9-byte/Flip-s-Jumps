#!/bin/sh
# Builds the game's simulation for the host and checks that it is playable.
# No Flipper Zero and no SDK required: the stubs/ headers stand in for the
# firmware APIs that game.c and draw.c call.
set -e

cd "$(dirname "$0")"
CC=${CC:-cc}
CFLAGS="-std=gnu11 -Wall -Wextra -O2 -I stubs -I . -I .."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

echo "building..."
$CC $CFLAGS harness.c ../game.c -lm -o "$OUT/harness"
$CC $CFLAGS screens.c canvas_sim.c ../game.c ../draw.c -lm -o "$OUT/screens"

"$OUT/harness"

if [ "$1" = "--screens" ]; then
    "$OUT/screens"
fi
