#!/usr/bin/env bash
#
# Sixteen stored buffers: store, recall with its clip length, delete, the lamp,
# and the round trip through the state blob that makes them survive a reload.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -Isrc -o "$OUT/pat" tests/test_stacks_patterns.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_patterns.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/pat" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (16 pattern buffers, stored and restored)"
else
    echo "FAIL: the pattern store"; cat "$OUT/log"; exit 1
fi
