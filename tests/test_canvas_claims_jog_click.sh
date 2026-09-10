#!/usr/bin/env bash
#
# A canvas param may CLAIM the jog click, and then Back is the only exit.
#
# MoveMainButton is CC 3 -- the jog click -- and shadow_ui steals it to close
# the fullscreen canvas BEFORE dispatchCanvasMidi runs, so an overlay can never
# see it. That is right for a viewer you look at and leave, and wrong for an
# editor meant to be locked into: the click is its primary gesture.
#
# The opt-in is why this is safe. #154 took Undo/Copy/Delete unconditionally
# whenever the shadow display was up and had to be reverted (#175) because it
# stole them from everyone; claims_ccs was the capability answer to that, and
# this is the same shape. A param declaring nothing must behave exactly as it
# did before, so the pins below are BOTH directions.
set -uo pipefail
cd "$(dirname "$0")/.."
# The HOST half of this contract -- that shadow_ui honours claims_jog_click
# rather than stealing CC 3 to close the view -- is pinned in the Schwung
# repo, where that code lives. What is pinned here is our side: the param
# DECLARES the claim, and the overlay actually handles the click.
fail=0
bad(){ echo "FAIL: $1"; fail=1; }

M=src/module.json
if [ -f "$M" ]; then
    python3 - "$M" <<'PY' || fail=1
import json, sys
cp = {e['key']: e for e in json.load(open(sys.argv[1]))['capabilities']['chain_params']}
g = cp.get('grid', {})
if g.get('claims_jog_click') is not True:
    print("FAIL: stacks' grid param does not claim the jog click"); sys.exit(1)
if g.get('show_footer') is not False:
    print("FAIL: grid does not suppress the host footer it draws over"); sys.exit(1)
PY
    grep -q 'JOG_CLICK_CC' src/canvas.js \
        || bad "canvas.js claims the click but never handles CC 3"
else
    bad "src/module.json is missing"
fi

if [ $fail -eq 0 ]; then echo "PASS: $(basename "$0")"; else echo "FAIL: $(basename "$0")"; fi
exit $fail
