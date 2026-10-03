#!/usr/bin/env bash
#
# Read Track follows the track selected on Move (move_info.h, Schwung 1.6.0+),
# on a change only, and Read Clip reads the track the knob names -- including
# after a restore. See the .c.
set -uo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
# -rdynamic: the test defines the host's schwung_move_info export itself, and
# move_info_read() finds it with dlsym(RTLD_DEFAULT) as it does on the device.
if ! cc -O1 -rdynamic -I src -DSETS_DIR="\"$OUT/sets\"" -o "$OUT/t" tests/test_stacks_follow_track.c -lpthread -lm 2>"$OUT/err"; then
    echo "FAIL: test_stacks_follow_track.c did not build"; sed -n '1,12p' "$OUT/err"; exit 1
fi
if "$OUT/t" "$OUT" > "$OUT/log" 2>&1; then
    echo "PASS: $(basename "$0")  (Read Track follows Move's selection)"
else
    echo "FAIL: Read Track / Read Clip disagree with the selection"; cat "$OUT/log"; exit 1
fi
