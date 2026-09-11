#!/usr/bin/env bash
#
# The sixteen pattern slots, driven from the pads — see the .mjs for why each
# gesture is what it is.
set -uo pipefail
cd "$(dirname "$0")/.."
if node tests/test_stacks_pattern_pads.mjs; then
    echo "PASS: $(basename "$0")  (the pattern pads reach the store)"
else
    echo "FAIL: the pattern pads"; exit 1
fi
