#!/usr/bin/env bash
#
# A root you NAMED -- from the library, the Root knob or a clip -- sounds as
# named; only a root asked for by degree is the scale's to move. And the
# staff captions the chord that sounds. See the C file for how the library
# was defeated by its own player.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -I src -o "$OUT/t" tests/test_stacks_exact_root.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_exact_root.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/t" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (a named root sounds as named)"
else
    echo "FAIL: root snapping"; cat "$OUT/log"; exit 1
fi
