#!/bin/bash
# The overtake-tool entry (plugin_api_v2): what the MIDI FX returns is what gets
# injected into Move -- cable 2, CIN = status nibble, channel = track - 1, pad
# ids ignored, nothing left sounding after a stop. See the .c for the contract.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -I "$ROOT/src" -o "$OUT/inject" "$ROOT/tests/test_stacks_tool_inject.c" -lpthread -lm 2>"$OUT/err"; then
  echo "FAIL: compile"; cat "$OUT/err"; exit 1
fi
"$OUT/inject"
