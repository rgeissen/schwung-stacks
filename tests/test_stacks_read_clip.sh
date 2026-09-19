#!/usr/bin/env bash
#
# Read Clip must import a clip at the length it was played: two-bar chords as
# two bars, mixed lengths where they fall. The reader was the one door into
# the buffer with no test, and it was still counting in the old units.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -I src -DSETS_DIR="\"$OUT/sets\"" -o "$OUT/t" tests/test_stacks_read_clip.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_read_clip.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/t" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (a clip imports at its own length)"
else
    echo "FAIL: Read Clip mis-measures the clip"; cat "$OUT/log"; exit 1
fi
