#!/usr/bin/env bash
#
# Stacks: the chord-shape table is declared TWICE and must not drift.
#
# SHAPES[] in stacks.c is what the module plays and names; the `shape` enum in
# module.json is what the knob shows. A knob option with no matching row writes
# a shape the module does not have -- and an enum written by NAME simply does
# not match, so the write is dropped and the knob appears to do nothing on one
# specific value. Nothing logs that.
#
# It also pins the FAMILY ORDERING, which is load-bearing rather than tidy:
# `family` moves `shape` to the first row of a family and `shape` then steps
# within it, so a row filed out of order splits its family into two runs and
# the jump silently lands in the wrong one.
set -uo pipefail
cd "$(dirname "$0")/.."

C=src/dsp/stacks.c
J=src/module.json
fail=0
note() { echo "  $1"; }
bad()  { echo "FAIL: $1"; fail=1; }

[ -f "$C" ] || { echo "FAIL: missing $C"; exit 1; }
[ -f "$J" ] || { echo "FAIL: missing $J"; exit 1; }

python3 - "$C" "$J" <<'PY'
import json, re, sys
c = open(sys.argv[1]).read()
j = json.load(open(sys.argv[2]))
bad = []

tbl = c[c.index('static const stk_shape_t SHAPES[] = {'):c.index('#define SHAPE_REST')]
rows = re.findall(r'\{\s*"([^"]+)",\s*"([^"]*)",\s*(FAM_\w+)\s*,\s*(\d+)\s*,\s*\{([^}]*)\}', tbl)
if not rows:
    bad.append("could not parse SHAPES[] out of stacks.c")

opts = [r[0] for r in rows]
fams = [r[2] for r in rows]

# 1. families contiguous and in the declared order
order = ['FAM_5TH', 'FAM_TRIAD', 'FAM_6TH', 'FAM_7TH', 'FAM_9TH', 'FAM_EXT']
runs = [f for i, f in enumerate(fams) if i == 0 or fams[i-1] != f]
if runs != order:
    bad.append(f"SHAPES[] families are not contiguous/in order: {runs}")

# 2. every row's interval count matches its declared n, and fits STK_MAX_TONES
maxt = int(re.search(r'#define STK_MAX_TONES\s+(\d+)', c).group(1))
for name, _suf, _f, n, ivs in rows:
    got = len([x for x in ivs.split(',') if x.strip()])
    # A REST has n=0 and therefore no intervals, but C forbids an empty brace
    # initialiser -- it carries a single placeholder that `n` tells every
    # consumer to ignore. Counting braces instead of reading n would make the
    # only silent chord in the table look malformed.
    if int(n) == 0:
        if got != 1:
            bad.append(f"shape {name}: n=0 must carry exactly one placeholder, has {got}")
    elif got != int(n):
        bad.append(f"shape {name}: declares n={n} but lists {got} intervals")
    if int(n) > maxt:
        bad.append(f"shape {name}: n={n} exceeds STK_MAX_TONES={maxt}")

# 3. no two shapes share an interval set -- a duplicate is unreachable by name
seen = {}
for name, _s, _f, _n, ivs in rows:
    if int(_n) == 0:
        continue          # the rest has no interval set to collide with
    k = tuple(sorted(int(x) for x in ivs.split(',') if x.strip()))
    if k in seen:
        bad.append(f"shape {name} duplicates {seen[k]} ({list(k)})")
    seen[k] = name

cp = {e['key']: e for e in j['capabilities']['chain_params']}

# 4. the knob's options ARE the table, in the table's order
if cp['shape']['options'] != opts:
    bad.append("module.json shape options != SHAPES[] (order or content)")
if cp['shape']['options'][cp['shape']['default']] != 'maj':
    bad.append("shape default is not maj")

# 5. the family knob's options are the C family names
fam_names = [x.strip().strip('"') for x in
             re.search(r'FAMILIES\[\] = \{([^}]*)\}', c).group(1).split(',') if x.strip()]
if cp['family']['options'] != fam_names:
    bad.append(f"module.json family options {cp['family']['options']} != C {fam_names}")

# 6. SHAPE_SINGLE really is the single-note fallback the import uses
if opts[int(re.search(r'#define SHAPE_SINGLE\s+(\d+)', c).group(1))] != 'note':
    bad.append("SHAPE_SINGLE does not point at the \"note\" row")
if opts[int(re.search(r'#define SHAPE_REST\s+(\d+)', c).group(1))] != 'rest':
    bad.append("SHAPE_REST does not point at the \"rest\" row")

# 7. root's knobs are all real params, and the MAIN bank still fits.
#
# `sel` is the jog, not a cell, and the bank generator filters it out; the
# bank is hard-limited to eight cells, so that is what is pinned.
knobs = j['ui_hierarchy']['levels']['root']['knobs']
canvas_cells = [k for k in knobs if k not in ('sel',)]
if len(canvas_cells) > 8:
    bad.append(f"root fills {len(canvas_cells)} canvas cells; a bank holds 8")
for k in knobs:
    if k not in cp:
        bad.append(f"knob {k} is on no chain_params entry")

# 8. the custom widget's kind and extra_keys, which are what get `prog` read at
#    all: an unregistered kind falls through to a built-in dial and the staff
#    never sees a chord.
# 8b. Humanise is SEEDED, and its two axes are independent -- a shared seed
#     would make Randomize Time disturb a velocity pattern already settled on.
for k in ('hum_vel', 'hum_time', 'roll_vel', 'roll_time'):
    if k not in cp:
        bad.append(f"missing humanise param {k}")
for k in ('roll_vel', 'roll_time'):
    if cp.get(k, {}).get('access') != 'write':
        bad.append(f"{k} is not access:write, so a click would latch it on")
if 'seed_vel' not in c or 'seed_time' not in c:
    bad.append("humanise does not carry two independent seeds")
# Strip comments first: this file EXPLAINS why it is not rand(), and a
# substring search over prose fails on the explanation of its own rule.
code = re.sub(r'/\*.*?\*/', '', c, flags=re.S)
code = re.sub(r'//[^\n]*', '', code)
if re.search(r'\b(rand|random|srand|drand48)\s*\(', code):
    bad.append("humanise calls rand() -- it must be a hash, so Stamp writes what was previewed")

# The chain-era viz declaration (custom:stkstaff + extra_keys) is gone with
# the chain: the Tool tab reads `state`, and the surface is ui.js over
# canvas.js. What must still be true is that canvas.js exposes the surface.
canvas = open('src/canvas.js').read()
if 'draw(ctx)' not in canvas and 'draw(' not in canvas:
    bad.append("canvas.js exposes no fullscreen draw")
if 'onMidi' not in canvas:
    bad.append("canvas.js exposes no onMidi -- the takeover would have no jog")
# 9. A TOOL, not a chain component -- and nothing of the chain left in the
#    manifest: no canvas param, no CC claims (every button is the tool's), no
#    canvas_script for a knob grid that will never run it.
if j.get('component_type') != 'tool' or not j.get('tool_config', {}).get('overtake'):
    bad.append("module.json is not an overtake tool manifest")
if not j['capabilities'].get('suspend_keeps_js'):
    bad.append("suspend_keeps_js missing -- an armed take would die on Back")
bp = j['capabilities'].get('button_passthrough') or []
if 85 not in bp or 86 not in bp:
    bad.append(f"button_passthrough must carry Play (85) and Record (86), has {bp}")
# 118 is SAMPLE (docs/API.md; the host's own rec_arm.h measured Record at 86).
# Listing it as "Record" is how Record went missing for a fortnight -- and
# passed through, it opens Move's sampler behind the tool.
if 118 in bp:
    bad.append("button_passthrough carries 118, which is Sample, not Record")
for stale in ('canvas_script', 'claims_ccs', 'claims_edit_ccs'):
    if stale in j['capabilities']:
        bad.append(f"chain-era capability {stale} is back")
if any(p.get('type') == 'canvas' or p.get('as_page') is not None for p in cp.values()):
    bad.append("a canvas/as_page param is back -- the surface is ui.js, not a knob page")

for b in bad:
    print("FAIL: " + b)
print(f"  {len(rows)} chord shapes, {len(runs)} families, {len(knobs)} knobs")
sys.exit(1 if bad else 0)
PY
rc=$?
[ $rc -eq 0 ] || fail=1

# 9b. chain_params must be SERVED FROM get_param, not merely declared in
#     module.json. The shadow UI queries the component for it
#     (shadow_ui.js: "Chain params are typically in module.json, but we query
#     via get_param"), so declaring it only in module.json yields the knobs
#     from ui_hierarchy and silently drops the `as_page` canvas param and every
#     `viz` -- a knob page where the picture should be, with nothing logged.
if ! grep -q 'strcmp(key, "chain_params")' "$C"; then
    bad "stacks.c does not serve chain_params from get_param"
fi
if ! python3 tools/gen_chain_params.py --check; then
    bad "CHAIN_PARAMS_JSON is stale -- run: python3 tools/gen_chain_params.py"
fi
note "chain_params served from get_param and in sync with module.json"

# 9c. A MODULE MUST NEVER PUT A COLON IN A setParam KEY.
#     The canvas runtime builds the full param key with
#       if (key.includes(":")) return key;
#     so a key that already contains one is assumed fully qualified and is sent
#     WITHOUT the component prefix -- it addresses nobody, silently, because a
#     write to an unknown key is not an error anywhere in the chain. That is
#     how every enum in the takeover came to be dead while looking correct.
#     Relative steps go in the VALUE ("~N") for exactly this reason.
CANVAS=src/canvas.js
if grep -q 'setParam(key + ":' "$CANVAS"; then
    bad "canvas.js builds a setParam key containing ':' -- it loses the component prefix"
fi
grep -q 'setParam(key, "~"' "$CANVAS" || bad "canvas.js does not send relative enum steps as ~N"
grep -q "val\[0\] == '~'" "$C" || bad "stacks.c does not accept a ~N relative write"
note "relative enum writes travel in the value, not the key"

# 9d. SHIFT COMES FROM THE WIRE, ONCE. As an overtake tool CC 49 IS forwarded
#     (measured on the device), and the shim's own tracking is documented as
#     not working in that mode -- so canvas.js never asks the shim, and the
#     one CC-49 site is the RECORDER; a second would be a gesture on a
#     modifier, and gestures read shiftHeld().
grep -q 'd1 === 49) { S.shiftCC = d2 > 0' "$CANVAS" \
    || bad "canvas.js does not record CC 49 into S.shiftCC"
grep -q 'function shiftHeld() { return !!S.shiftCC; }' "$CANVAS" \
    || bad "shiftHeld() is not the wire"
if grep -q 'shadow_get_shift_held' "$CANVAS"; then
    bad "canvas.js asks the shim for Shift, which does not work under an overtake"
fi
if python3 -c "
import re,sys
src=open('$CANVAS').read()
code=re.sub(r'/\*.*?\*/','',src,flags=re.S)
code=re.sub(r'//[^\n]*','',code)
sys.exit(0 if len(re.findall(r'd1 === (SHIFT_CC|49)\b', code)) > 1 else 1)
"; then
    bad "canvas.js has a second CC 49 site -- a gesture on the modifier itself"
fi
note "Shift: CC 49 from the wire, one recorder"

# 9f. AN UNBOUND KNOB MUST BE DARK. Colour 0 is reserved for "nothing is bound
#     here" (param_pages/knob_leds.mjs), and the tool drives the rings itself:
#     the host hands the surface over dark, so an unbound slot must be painted
#     dark on purpose and a bound one lit.
grep -q 'key ? ledColor' "$CANVAS" \
    || bad "canvas.js does not force colour 0 for an unbound knob slot"
grep -q 'PARAM_RANGES' "$CANVAS" \
    || bad "canvas.js has no generated ranges, so LED intensity cannot be right"
note "knob rings: unbound is dark, ranges generated"

# 9e. THE BANKS ARE THE LEVELS. Two surfaces onto one module only pay for
#     themselves while what you learn on either transfers; two layouts for the
#     same parameters is two things to learn. Compared here rather than asked
#     for in a comment, because a comment is what let them drift the first time.
python3 - <<'PYEOF' || fail=1
import json, re, sys
mj = json.load(open('src/module.json'))
lv = mj['ui_hierarchy']['levels']
src = open('src/canvas.js').read()
blk = src[src.index('const BANKS = ['):src.index('];', src.index('const BANKS = ['))]
banks = {}
for m in re.finditer(r'\{\s*name:\s*"(\w+)",\s*keys:\s*\[(.*?)\]\s*\}', blk, re.S):
    keys = re.findall(r'\["([a-z_]+)","[^"]*","[^"]*"\]|\[null,"",""\]', m.group(2))
    banks[m.group(1)] = [k for k in keys if k]
pairs = [("MAIN","root"),("START","start"),("CHORD","chord"),("VOICE","voice"),("FEEL","feel"),("CLIP","clip")]
bad = []
if list(banks) != [b for b,_ in pairs]:
    bad.append(f"bank order {list(banks)} != {[b for b,_ in pairs]}")
for bank, level in pairs:
    want = [k for k in lv[level]['knobs'] if k not in ("sel",)]
    got = banks.get(bank, [])
    if got != want:
        bad.append(f"{bank} bank {got} != {level} level {want}")
for b in bad: print("FAIL: " + b)
sys.exit(1 if bad else 0)
PYEOF
note "the five banks mirror the five levels, key for key"

# 9e'. AND THE PANEL CARRIES EVERY ONE OF THEM, IN THE SAME BANK. The panel's
#      bank list is hand-kept -- it adds controls the Move has elsewhere
#      (Degree, Patterns, Read Track, Duplicate...) -- so it may hold MORE, but
#      a device control missing from it is a control the browser cannot reach.
#      Chord Oct was, until 2026-10-03, and "I am missing the octave" was the
#      report. Exempt: `sel` (the chord strip) and `preview` (the panel's
#      header transport button).
python3 - <<'PYEOF' || fail=1
import json, re, sys
lv = json.load(open('src/module.json'))['ui_hierarchy']['levels']
src = open('src/web_ui.html').read()
blk = src[src.index('var BANKS = ['):src.index('];', src.index('var BANKS = ['))]
panel = {m.group(1): re.findall(r'"([a-z_]+)"', m.group(2))
         for m in re.finditer(r'id:"(\w+)".*?keys:\[(.*?)\]', blk, re.S)}
bad = []
for level, pid in [("root","main"),("start","start"),("chord","chord"),("voice","voice"),("feel","feel"),("clip","clip")]:
    for k in lv[level]['knobs']:
        if k in ("sel", "preview"): continue
        if k not in panel.get(pid, []):
            bad.append(f"{k} is on the Move's {pid.upper()} bank and not on the panel's")
for b in bad: print("FAIL: " + b)
sys.exit(1 if bad else 0)
PYEOF
note "the panel carries every device control, in the same bank"

# 10. No file I/O on the realtime path. Reading Song.abl from set_param is the
#     granny bug: it stalls the param channel from the SPI callback.
for fn in stk_set_param stk_get_param stk_process_midi stk_tick; do
    body=$(awk "/^static .*${fn}\\(/,/^}/" "$C")
    if echo "$body" | grep -qE '\b(fopen|fread|fwrite|opendir|malloc|calloc|free|pthread_create)\b'; then
        bad "$fn does file I/O, allocates or spawns a thread -- it IS the SPI callback"
    fi
done
note "no file I/O or allocation in the four realtime entry points"

# 11. The worker must not inherit the callback's FIFO 70.
if ! grep -q 'PTHREAD_EXPLICIT_SCHED' "$C" || ! grep -q 'SCHED_OTHER' "$C"; then
    bad "worker thread does not explicitly request SCHED_OTHER"
fi
note "worker thread is explicitly SCHED_OTHER"

if [ $fail -eq 0 ]; then # ---------------------------------------------------------------------------
# EVERY note path resolves through chord_notes_g WITH the grouping.
#
# Voice Grouping is applied inside chord_notes_g, which is what makes the
# picture honest: the sequencer and the staff both resolve notes through that
# one function, so the staff cannot draw a chord the synth never played. (The
# file stamp was a third caller until the file write went, 2026-09-15; Move
# records the wire now, so the wire is the stamp.) The failure this guards is
# a call site added later that forgets the argument -- it would compile and
# silently voice ungrouped chords on one surface. Assert the call shape.
SRC=src/dsp/stacks.c
CALLS=$(grep -c 'chord_notes_g(' "$SRC")
# 1 prototype + 1 definition + 2 call sites
if [ "$CALLS" -ne 4 ]; then
    echo "FAIL: expected 4 mentions of chord_notes_g (proto, def, 2 call sites), found $CALLS"
    grep -n 'chord_notes_g(' "$SRC"
    exit 1
fi
if grep -n 'chord_notes_g(' "$SRC" | grep -v 'static int chord_notes_g' \
       | grep -qv 'st->grouping'; then
    # the call spans lines, so check the argument appears within each call
    for ln in $(grep -n 'chord_notes_g(' "$SRC" | grep -v 'static int' | cut -d: -f1); do
        if ! sed -n "${ln},$((ln+4))p" "$SRC" | grep -q 'st->grouping'; then
            echo "FAIL: chord_notes_g at line $ln does not pass st->grouping"
            echo "      a note path that skips grouping stamps what you never heard"
            sed -n "${ln},$((ln+4))p" "$SRC"
            exit 1
        fi
    done
fi
echo "  every note path (sequencer, staff) carries the voice grouping"
python3 tools/check_triggers.py || exit 1
python3 tools/check_gestures.py || exit 1
echo "PASS: $(basename "$0")"; else echo "FAIL: $(basename "$0")"; fi
exit $fail
