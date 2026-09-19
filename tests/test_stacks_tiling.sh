#!/usr/bin/env bash
#
# A phrase shorter than the clip must sound on every tile of the clip, with
# its rhythm intact. See the C file for the two ways it did not.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -I src -o "$OUT/t" tests/test_stacks_tiling.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_tiling.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/t" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (the phrase tiles the clip)"
else
    echo "FAIL: the phrase does not tile the clip"; cat "$OUT/log"; exit 1
fi
