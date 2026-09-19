#!/usr/bin/env bash
#
# A rhythm figure belongs to the BAR: it is anchored there, each hit lasts its
# own gap, swing leans the level the figure uses, and humanise reaches every
# hit. Four ways arm_chord() forgot that, in the C file.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
if ! cc -O1 -I src -o "$OUT/t" tests/test_stacks_figure_phase.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_figure_phase.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/t" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (figures sit in the bar)"
else
    echo "FAIL: the rhythm engine"; cat "$OUT/log"; exit 1
fi
