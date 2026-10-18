#!/usr/bin/env python3
"""Builds docs/panel.html: Stacks' browser panel, region by region.

Run by tools/panel/build.sh. Input: the folder shots.js wrote (the panel's
own pictures, from its demo mode, and the controls of each tab in order).
What a control does is the control matrix's own text (tools/gen_matrix.py's
DESC), found through module.json's names: the facts are never typed twice.
The panel is the same file on Move (Schwung's Tool tab) and on Push
(push-hack-stacks serves it), so this is one page for both, with what
differs said once. A control with no text stops the build.

    python3 tools/panel/build.py <shots dir> <out.html> <git ref>
"""
import ast
import html
import json
import pathlib
import re
import sys

DATA = pathlib.Path(sys.argv[1])
OUT = pathlib.Path(sys.argv[2])
REF = sys.argv[3]
ROOT = pathlib.Path(__file__).resolve().parents[2]
data = json.loads((DATA / "data.json").read_text())
mj = json.loads((ROOT / "src" / "module.json").read_text())
VERSION = mj["version"]


def matrix_desc():
    """DESC from tools/gen_matrix.py, read as data (it is a literal)."""
    tree = ast.parse((ROOT / "tools" / "gen_matrix.py").read_text())
    for n in tree.body:
        if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "") == "DESC":
            return ast.literal_eval(n.value)
    sys.exit("tools/gen_matrix.py has no DESC")


DESC = matrix_desc()
KEY_BY_NAME = {p["name"].lower(): p["key"] for p in mj["capabilities"]["chain_params"]}
# The panel's own words for a few of them.
KEY_BY_NAME.update({"arm 1 lap": "stamp", "halftime": "stretch", "double-time": "compress", "degree": "degree"})
OWN = {
    "PATTERNS": "Sixteen pattern slots, and you are always in one. Tap a slot to go there (an empty one is where the next pattern is written); hold one to copy what you play into it; DELETE, then a tap, empties one. Gold is the slot you are in, blue holds music.",
}

REGIONS = [
    ("top", "The top bar", "Play and Stop, undo and redo, Bypass, and Remove the selected chord (a minus, not a bin: Undo puts it back). On the right: the key, the clip's bars, the tempo and a dot that lights while the module plays."),
    ("roll", "The staff", "The progression drawn as notes, the bar numbers below, the playhead running. Every note is a note the module plays: voicings, inversions and rhythms included. Tap a chord to select it and hear it while you hold."),
    ("strip", "The chord strip", "The chords in order, each card as wide as its length, so the strip lines up with the staff. Tap or hold a card to select it and hear it; + adds a copy of the selected chord after it."),
]

TAB_NOTE = {
    "MAIN": "The harmony and the clip: scale, key, how many chords, how long the clip is, how the voicings group.",
    "START": "The libraries to begin from: 142 progressions by genre, 12 common and 13 uncommon, and the voicing colour.",
    "CHORD": "The selected chord: its degree or root, its octave, its shape, inversion, length, offset and velocity.",
    "VOICE": "The selected chord's voicing and feel: colour, strum, gate, mute, rhythm, transpose.",
    "FEEL": "The whole progression's feel: velocity, gate, swing and the humanising, with its dice.",
    "CLIP": "The patterns, and what you do to the clip while it plays: transpose, octave, read, stamp, run, and the length gestures.",
}

HOSTS = [
    ("On Ableton Move", [
        ("Open it", "Schwung's web manager, Remote UI, the Tool tab, while Stacks is open on Move."),
        ("Play", "Plays the progression, or joins Move's transport while Run is on."),
        ("Read Clip", "Reads the clip on the track Read Track names, from Move's own set."),
        ("Arm 1 lap", "Then press Record on Move: Stacks plays the progression once, on Move's downbeat."),
        ("Every control", "Stacks Control Matrix (schwung-stacks, docs/stacks-matrix.html)."),
    ]),
    ("On Push 3", [
        ("Open it", "http://<push>:7711 (Push Manager lists the port Push Hack Catalog gave it)."),
        ("Play", "Plays the progression, or follows Live's transport while Run is on."),
        ("Read Clip", "Not on Push yet: Live's sets are another format."),
        ("Arm 1 lap", "Then press Record in Live: Stacks plays the progression once, from Live's downbeat. PLAY CLOSED and Stacks Harmony are on Push's own surface."),
        ("Every control", "Stacks Push Matrix (push-hack-stacks, docs/control-matrix.html)."),
    ]),
]


def e(s):
    return html.escape(s or "", quote=True)


def text_of(label):
    if label in OWN:
        return OWN[label]
    base = re.split(r"\s+·\s+", label)[0].strip().lower()
    key = KEY_BY_NAME.get(base)
    if key and key in DESC:
        return DESC[key]
    return None


missing = []
for t in data["tabs"]:
    for label in t["labels"]:
        if text_of(label) is None:
            missing.append(f"{t['name']}/{label}")
    if t["name"] not in TAB_NOTE:
        missing.append("tab " + t["name"])
if missing:
    sys.exit("no text for: " + ", ".join(missing))
if data.get("errors"):
    sys.exit("the panel had script errors: " + "; ".join(data["errors"]))

n_controls = sum(len(t["labels"]) for t in data["tabs"])
sections = []
for rid, title, intro in REGIONS:
    sections.append(f'''
<section class="region" id="{rid}">
  <header class="region__head"><h2>{e(title)}</h2><p>{e(intro)}</p></header>
  <div class="region__shot"><img alt="{e(title)}: the panel as it is" src="data:image/jpeg;base64,{data[rid]}"></div>
</section>''')

tabs = []
for t in data["tabs"]:
    def shown(l):
        l = re.split(r"\s+·\s+", l)[0]
        return l.capitalize() if l.isupper() else l
    items = "".join(f"<li><b>{e(shown(l))}</b><span>{text_of(l)}</span></li>" for l in t["labels"])
    scope = "the selected chord" if "CHORD" in t["scope"] else "the whole progression"
    tabs.append(f'''
<section class="region tab" id="tab-{t["name"].lower()}">
  <header class="region__head"><h2>{e(t["name"])}</h2><p>{e(TAB_NOTE[t["name"]])} Acts on <strong>{scope}</strong>.</p></header>
  <div class="tabbody">
    <div class="rail-shot"><img alt="The {e(t["name"])} tab of the panel" src="data:image/jpeg;base64,{t["shot"]}"></div>
    <ul class="controls">{items}</ul>
  </div>
</section>''')

hosts = "".join(
    f'<div class="host"><h3>{e(h)}</h3><dl>' + "".join(f"<dt>{e(k)}</dt><dd>{e(v)}</dd>" for k, v in rows) + "</dl></div>"
    for h, rows in HOSTS)
toc = "".join(f'<a href="#{rid}">{e(title)}</a>' for rid, title, _ in REGIONS) + \
      "".join(f'<a href="#tab-{t["name"].lower()}">{e(t["name"])}</a>' for t in data["tabs"])

# The matrices' family: the same tokens as tools/gen_matrix.py's page.
css = r"""
:root { --ground:#EDF1F4; --panel:#FFFFFF; --ink:#0E1419; --muted:#5C6B78; --line:#D3DCE3; --accent:#B45309; --prog:#1F6B85;
  --screen:#090C0F; --shadow:0 1px 2px rgba(14,20,25,.06), 0 8px 24px rgba(14,20,25,.05); }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --ground:#080B0E; --panel:#11161B; --ink:#E4EBF1; --muted:#8496A4;
  --line:#1D252D; --accent:#FFB454; --prog:#79BDD4; --screen:#05070A; --shadow:0 1px 0 rgba(255,255,255,.03), 0 12px 32px rgba(0,0,0,.5); color-scheme:dark; } }
:root[data-theme="dark"] { --ground:#080B0E; --panel:#11161B; --ink:#E4EBF1; --muted:#8496A4; --line:#1D252D; --accent:#FFB454; --prog:#79BDD4;
  --screen:#05070A; --shadow:0 1px 0 rgba(255,255,255,.03), 0 12px 32px rgba(0,0,0,.5); color-scheme:dark; }
* { box-sizing:border-box; }
body { margin:0; background:var(--ground); color:var(--ink); font:400 15px/1.6 "IBM Plex Sans", ui-sans-serif, system-ui, sans-serif; -webkit-font-smoothing:antialiased; }
.wrap { max-width:1080px; margin:0 auto; padding-block:44px 72px; padding-left:max(16px, env(safe-area-inset-left, 0px)); padding-right:max(16px, env(safe-area-inset-right, 0px)); }
.masthead { display:flex; flex-wrap:wrap; align-items:flex-end; gap:20px 32px; padding-bottom:22px; border-bottom:2px solid var(--ink); }
.masthead h1 { font:700 clamp(38px,7vw,64px)/.92 "Chakra Petch", ui-sans-serif, sans-serif; letter-spacing:-.02em; margin:0; text-wrap:balance; }
.masthead p { margin:0; max-width:52ch; color:var(--muted); }
.stats { margin-left:auto; display:flex; gap:26px; font:500 12px/1 "IBM Plex Mono", ui-monospace, monospace; color:var(--muted); }
.stats b { display:block; font:700 22px/1.2 "Chakra Petch", sans-serif; color:var(--ink); font-variant-numeric:tabular-nums; }
.eyebrow { font:500 11px/1 "IBM Plex Mono", monospace; letter-spacing:.16em; text-transform:uppercase; color:var(--muted); margin:52px 0 16px; }
.lede { max-width:68ch; color:var(--muted); margin:0 0 18px; }
.hosts { display:grid; grid-template-columns:repeat(auto-fit, minmax(300px, 1fr)); gap:16px; margin-top:22px; }
.host { background:var(--panel); border:1px solid var(--line); border-top:3px solid var(--accent); border-radius:3px; box-shadow:var(--shadow); padding:16px 18px; }
.host + .host { border-top-color:var(--prog); }
.host h3 { margin:0 0 10px; font:700 18px/1.2 "Chakra Petch", sans-serif; letter-spacing:.03em; }
.host dl { margin:0; display:grid; grid-template-columns:max-content 1fr; gap:6px 14px; font-size:13.5px; }
.host dt { font:600 11.5px/1.6 "IBM Plex Mono", monospace; }
.host dd { margin:0; color:var(--muted); }
.toc { display:flex; flex-wrap:wrap; gap:8px; }
.toc a { font:500 12px/1 "IBM Plex Mono", monospace; color:var(--ink); text-decoration:none; padding:7px 10px; border:1px solid var(--line); border-radius:3px; background:var(--panel); }
.toc a:hover { border-color:var(--accent); }
.hero, .region { background:var(--panel); border:1px solid var(--line); border-radius:3px; box-shadow:var(--shadow); overflow:hidden; }
.hero { background:var(--screen); }
.hero img, .region__shot img, .rail-shot img { display:block; max-width:100%; height:auto; }
.region { margin-bottom:22px; scroll-margin-top:16px; }
.region__head { padding:16px 20px 14px; border-bottom:1px solid var(--line); border-left:3px solid var(--accent); }
.region__head h2 { margin:0 0 4px; font:700 22px/1.1 "Chakra Petch", sans-serif; letter-spacing:.03em; color:var(--accent); }
.region__head p { margin:0; color:var(--muted); max-width:80ch; }
.region__head strong { color:var(--ink); font-weight:600; }
.region__shot { background:var(--screen); overflow-x:auto; }
.tabbody { display:grid; grid-template-columns:minmax(0, 340px) minmax(0, 1fr); }
.rail-shot { background:var(--screen); border-right:1px solid var(--line); }
.controls { list-style:none; margin:0; padding:14px 20px 18px; display:grid; gap:12px; align-content:start; font-size:13.5px; color:var(--muted); }
.controls li { display:grid; gap:2px; }
.controls b { color:var(--ink); font:600 12.5px/1.4 "IBM Plex Mono", monospace; letter-spacing:.02em; }
.controls em { font-style:normal; color:var(--ink); }
footer { margin-top:52px; padding-top:18px; border-top:1px solid var(--line); font:400 12px/1.7 "IBM Plex Mono", monospace; color:var(--muted); }
@media (max-width:720px) { .tabbody { grid-template-columns:1fr; } .rail-shot { border-right:0; border-bottom:1px solid var(--line); } .stats { margin-left:0; } }
"""

page = f'''<title>Stacks Panel</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;700&family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;500;600&display=swap">
<style>{css}</style>

<div class="wrap">
  <header class="masthead">
    <div>
      <h1>Stacks Panel</h1>
      <p>Stacks' browser panel, region by region: the progression on a staff, the chords in a strip, and the six banks as tabs. It is the same panel on <strong>Ableton Move</strong> and on <strong>Push 3</strong>; what differs is said once, below.</p>
    </div>
    <div class="stats">
      <span><b>{len(REGIONS)}</b>regions</span>
      <span><b>{len(data["tabs"])}</b>tabs</span>
      <span><b>{n_controls}</b>controls</span>
    </div>
  </header>

  <div class="hosts">{hosts}</div>

  <p class="eyebrow">The panel</p>
  <p class="lede">Every picture is the panel itself, in its demo state (Am F C G7 Dm, playing; G7 selected). What each control does is the control matrix's own text, so the two cannot disagree.</p>
  <div class="hero"><img alt="Stacks' panel, the whole page" src="data:image/jpeg;base64,{data["full"]}"></div>
  <p class="eyebrow">Regions and tabs</p>
  <nav class="toc" aria-label="Regions and tabs">{toc}</nav>
  {''.join(sections)}
  <p class="eyebrow">The tabs &mdash; the six banks</p>
  {''.join(tabs)}

  <footer>Stacks {e(VERSION)}. Generated from the panel at <code>{e(REF)}</code> by <code>tools/panel/build.sh</code> (schwung-stacks). The controls of Move and of Push are in each device's control matrix.</footer>
</div>
'''
OUT.write_text(page)
print(f"wrote {OUT} ({len(page) // 1024} KB, {n_controls} controls)")
