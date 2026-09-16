#!/bin/bash
# Every PRESETS[] row applies through the `progression` param as exactly its
# chords: count, no rests, resolved pitches, declared length. See the .c.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cc -O1 -I "$ROOT/src" -o "$OUT/lib" "$ROOT/tests/test_stacks_library.c" -lpthread -lm 2>"$OUT/err" \
  || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/lib"
