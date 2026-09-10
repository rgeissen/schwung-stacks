#!/usr/bin/env bash
#
# The module must not be driven by its own output — see the C file for the
# device capture that produced it.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -Isrc -o "$OUT/tail" tests/test_stacks_own_tail.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_own_tail.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/tail" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (its own echo does not drive it)"
else
    echo "FAIL: the module is driven by its own output"; cat "$OUT/log"; exit 1
fi
