#!/bin/bash
# Parked (Back), Stacks stops following Move's transport and stops its loop,
# unless a take is armed; resume gives them back. See the .c for why.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cc -O1 -I "$ROOT/src" -o "$OUT/p" "$ROOT/tests/test_stacks_parked.c" -lpthread -lm 2>"$OUT/err" \
  || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/p"
