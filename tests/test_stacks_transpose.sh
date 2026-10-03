#!/usr/bin/env bash
#
# Key, Clip Transpose, Clip Octave, Transpose and Chord Oct each move what
# they say and nothing else -- on the picture and on the injected notes. See
# the .c.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -I src -o "$OUT/t" tests/test_stacks_transpose.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_transpose.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/t" "$OUT" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (every transpose moves what it says)"
else
    echo "FAIL: a transpose control moves the wrong thing"; cat "$OUT/log"; exit 1
fi
