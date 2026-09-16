#!/usr/bin/env python3
"""
Emit stacks.c's chain_params mirror from module.json.

The shadow UI asks the COMPONENT for `chain_params` over the param channel
(shadow_ui.js: "Chain params are typically in module.json, but we query via
get_param"), so a module that only declares them in module.json gets its knobs
from ui_hierarchy and loses everything that lives ONLY in chain_params -- the
`as_page` canvas param and every `viz`. The page still draws; the picture does
not. That is a silent failure, which is why this is generated rather than
hand-kept, and why test_stacks_shapes.sh fails on drift.

    python3 tools/gen_chain_params.py        # rewrite the block
    python3 tools/gen_chain_params.py --check # exit 1 if stale
"""
import json, re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
CHECK = "--check" in sys.argv
drift = []
def emit(path, new):
    """Write, or in --check mode record drift without touching the file --
    a check that rewrites what it checks hides the very drift it exists to
    report."""
    old = path.read_text() if path.exists() else None
    if new == old:
        return
    if CHECK:
        drift.append(path.name)
    else:
        path.write_text(new)
MJ = ROOT / "src/module.json"
C = ROOT / "src/dsp/stacks.c"
CJS = ROOT / "src/canvas.js"
BEGIN = "/* ---- BEGIN GENERATED chain_params (tools/gen_chain_params.py) ---- */"
END = "/* ---- END GENERATED chain_params ---- */"

# --- sync the enum options that are DEFINED IN C -------------------------
# shape and family options are the SHAPES[] table. Generating them here rather
# than hand-keeping them in module.json is the same rule as chain_params: a
# knob option with no matching row writes a value the module does not have,
# and an enum written by NAME simply fails to match, so the knob appears dead
# on one specific value with nothing logged.
csrc = C.read_text()
tbl = csrc[csrc.index('static const stk_shape_t SHAPES[] = {'):csrc.index('#define SHAPE_REST')]
shape_opts = re.findall(r'\{\s*"([^"]+)",\s*"[^"]*",\s*FAM_', tbl)
fam_opts = [x.strip().strip('"') for x in
            re.search(r'FAMILIES\[\] = \{([^}]*)\}', csrc).group(1).split(',') if x.strip()]

# --- rhythms: ONE table, four copies, all written from here ---------------
# RHYTHMS[] is the truth: the manifest's option list, the panel's mask map
# (it DRAWS the figure, and a hand-kept copy drew tresillo as 0x0491 against
# the DSP's 0x4949 -- a picture of a rhythm the synth never played), and the
# pad row's name list. A figure added to C appears everywhere or nowhere.
rt = csrc[csrc.index('static const stk_rhythm_t RHYTHMS[] = {'):]
rt = rt[:rt.index('#undef B')]
rhythms = []
for m in re.finditer(r'\{\s*"([^"]+)",\s*([^}]*)\}', rt):
    name, expr = m.group(1), m.group(2)
    mask = 0xFFFF if '0xFFFF' in expr else 0
    for b in re.findall(r'B\((\d+)\)', expr):
        mask |= 1 << int(b)
    rhythms.append((name, mask))
rhythm_names = [n for n, _ in rhythms]
if len(rhythm_names) != len(set(rhythm_names)):
    sys.exit("RHYTHMS[] has a duplicate name")

mj = json.load(MJ.open())
for e in mj["capabilities"]["chain_params"]:
    if e.get("key") == "shape":
        e["options"] = shape_opts
        e["default"] = shape_opts.index("maj")
    elif e.get("key") == "family":
        e["options"] = fam_opts
    elif e.get("key") == "rhythm":
        e["options"] = rhythm_names
        e["default"] = rhythm_names.index("hold")
# ui_hierarchy is AUTHORED, never generated. It used to be rewritten from
# chain_params here, which was fine while the contract was one flat level and
# actively wrong once it had branches: it put every per-chord param back on
# root, so the Chord/Voice/Feel/Clip levels existed AND root still carried
# four continuation pages of the same keys. The planner builds a level's
# overflow pages from that level's own `params` (page_plan.mjs: paramKeys),
# so what root lists is exactly what root pages.
emit(MJ, json.dumps(mj, indent=2) + "\n")

# --- ranges for the takeover's knob LEDs ---------------------------------
# The LED intensity is the parameter's value normalised 0..1, and the overlay
# cannot ask the host for a range: it has no chain_params of its own. Generated
# into canvas.js so the two cannot disagree -- a stale range makes a knob sit
# at the wrong brightness, which is a lie told quietly.
ranges = {}
for e in mj["capabilities"]["chain_params"]:
    if e["type"] == "int":
        ranges[e["key"]] = [e["min"], e["max"]]
    elif e["type"] == "enum":
        ranges[e["key"]] = e["options"]
rng_js = json.dumps(ranges, separators=(",", ":"))
RB = "/* ---- BEGIN GENERATED ranges (tools/gen_chain_params.py) ---- */"
RE_ = "/* ---- END GENERATED ranges ---- */"
cjs = CJS.read_text()
names_js = json.dumps(rhythm_names, separators=(",", ":"))
block = f"{RB}\nconst PARAM_RANGES = {rng_js};\nconst RHYTHM_NAMES = {names_js};\n{RE_}"
if RB in cjs:
    cjs = re.sub(re.escape(RB) + r".*?" + re.escape(RE_), lambda _m: block, cjs, flags=re.S)
else:
    anchor = "const BANKS = ["
    cjs = cjs.replace(anchor, block + "\n\n" + anchor, 1)
emit(CJS, cjs)
cjs_after_ranges = cjs

# --- the panel's mask map, drawn from the same table ----------------------
WUI = ROOT / "src/web_ui.html"
WB = "/* ---- BEGIN GENERATED rhythms (tools/gen_chain_params.py) ---- */"
WE = "/* ---- END GENERATED rhythms ---- */"
wui = WUI.read_text()
rhy_js = ",".join(f'"{n}":0x{m:04X}' for n, m in rhythms)
wblock = f"{WB}\nvar RHY = {{{rhy_js}}};\n{WE}"
if WB in wui:
    wui_new = re.sub(re.escape(WB) + r".*?" + re.escape(WE), lambda _m: wblock, wui, flags=re.S)
else:
    wui_new = re.sub(r"var RHY = \{.*?\};", lambda _m: wblock, wui, count=1, flags=re.S)
if wui_new == wui and WB not in wui:
    sys.exit("web_ui.html: found no `var RHY = {...};` to replace")
emit(WUI, wui_new)

# --- BANKS, from the levels ----------------------------------------------
# Generated here rather than hand-kept: the banks ARE the levels, and every
# time I edited them by hand they drifted. Each cell carries the key, a
# 3-glyph label for the eight-cell row, and the FULL name for the status line
# that replaces that row when a knob is touched.
SHORT = {
 "root":"RT","coct":"OCT","family":"FAM","shape":"SHP","inv":"INV","len":"LEN",
 "off":"OFF","cvel":"VEL","ccolour":"COL","cstrum":"STR","cgate":"GAT",
 "cmute":"MUT","rhythm":"RHY","ctrans":"TRN","scale":"SCL","key":"KEY",
 "defoct":"DOC","grouping":"GRP","steps":"CHD","rate":"RTE","bars":"BAR","run":"RUN",
 "octave":"OCT","velocity":"VEL","gate":"GAT","swing":"SWG","hum_vel":"HVL",
 "roll_vel":"RVL","hum_time":"HTM","roll_time":"RTM","read":"RD",
 "read_mode":"RDM","stamp":"ST","stamp_mode":"STM","clear":"CLR",
 "lanes":"LAN","preview":"PRV","genre":"GEN","progression":"PRG","colour":"COL",
 "common":"CMN","uncommon":"UNC","undo":"UND",
}
BANK_ORDER = [("MAIN","root"),("START","start"),("CHORD","chord"),
              ("VOICE","voice"),("FEEL","feel"),("CLIP","clip")]
lv = mj["ui_hierarchy"]["levels"]
by_key = {e["key"]: e for e in mj["capabilities"]["chain_params"]}
rows = []
for bname, lk in BANK_ORDER:
    ks = [k for k in lv[lk]["knobs"] if k not in ("grid", "sel")]
    ks = (ks + [None] * 8)[:8]
    cells = []
    for k in ks:
        if not k:
            cells.append('[null,"",""]')
        else:
            full = by_key[k]["name"]
            cells.append(f'["{k}","{SHORT[k]}","{full}"]')
    rows.append(f'    {{ name: "{bname}", keys: [' + ", ".join(cells) + "] },")
bank_js = "\n".join(rows)
cjs = cjs_after_ranges          # the text as it should be, whether or not it was written
bi = cjs.index("const BANKS = [")
bj = cjs.index("];", bi) + 2
head = cjs[bi:bj]
head = head[:head.index("    { name:")]
emit(CJS, cjs[:bi] + head + bank_js + "\n];" + cjs[bj:])

params = mj["capabilities"]["chain_params"]
compact = json.dumps(params, separators=(",", ":"))

# Chunk into C string literals so no line runs off the page.
chunks, line, out = [], "", []
for ch in compact:
    esc = '\\"' if ch == '"' else ("\\\\" if ch == "\\" else ch)
    if len(line) + len(esc) > 88:
        chunks.append(line); line = ""
    line += esc
chunks.append(line)
body = "\n".join(f'    "{c}"' for c in chunks)
block = f"{BEGIN}\nstatic const char CHAIN_PARAMS_JSON[] =\n{body};\n{END}"

src = C.read_text()
if BEGIN in src:
    new = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END), block.replace("\\", "\\\\"), src, flags=re.S)
else:
    anchor = "/* ======================================================================\n * The progression"
    new = src.replace(anchor, block + "\n\n" + anchor, 1)

if CHECK:
    if new != src:
        drift.append("stacks.c")
    if drift:
        print("stale: " + ", ".join(sorted(set(drift))))
        sys.exit(1)
    sys.exit(0)
C.write_text(new)
print(f"wrote {len(params)} params, {len(compact)} bytes of JSON into stacks.c")
