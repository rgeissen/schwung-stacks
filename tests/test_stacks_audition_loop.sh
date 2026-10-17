#!/bin/bash
# Audition loops the selected chord with its rhythm while on. See the .c.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cc -O1 -I "$ROOT/src" -o "$OUT/p" "$ROOT/tests/test_stacks_audition_loop.c" -lpthread -lm 2>"$OUT/err" \
  || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/p"
