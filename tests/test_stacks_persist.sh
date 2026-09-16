#!/bin/bash
# Stacks keeps its own state file (a tool has no slot in the set): saved when
# the rev has been quiet, saved on unload, restored on create. See the .c.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cc -O1 -I "$ROOT/src" -o "$OUT/p" "$ROOT/tests/test_stacks_persist.c" -lpthread -lm 2>"$OUT/err" \
  || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/p" "$OUT/sub/state.json"
