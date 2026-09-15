#!/bin/bash
# The Tool tab's contract: `state` is a flat JSON object of strings carrying the
# save blob under "s", every declared param, and the derived keys the panel
# draws with; `rui_poll` is rev:on:tick:bpm[:devms]. See the .c.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cc -O1 -I "$ROOT/src" -o "$OUT/t" "$ROOT/tests/test_stacks_tool_tab.c" -lpthread -lm 2>"$OUT/err" \
  || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/t" --state > "$OUT/state.json"
python3 - "$OUT/state.json" "$ROOT/src/module.json" <<'PY'
import json, sys
st = json.load(open(sys.argv[1]))
mod = json.load(open(sys.argv[2]))
bad = 0
def need(cond, msg):
    global bad
    if not cond: print("FAIL:", msg); bad = 1
need(isinstance(st, dict), "state is not a JSON object")
need(all(isinstance(v, str) for v in st.values()), "state carries a non-string value (the manager drops those)")
need(st.get("s", "").startswith("v6|"), "the save blob is not under \"s\"")
for k in ("prog", "status", "patterns", "track", "module_id", "sel", "scale", "key", "progression", "bars"):
    need(k in st, f"state lacks {k}")
need(st.get("module_id") == "stacks", "module_id")
need(st.get("sel") == "2", f"sel should read back 2, got {st.get('sel')}")
need(st.get("prog", "").startswith("v13|"), "prog is not the v13 picture")
# Everything declared READABLE must be in the map: not the canvas, not the
# write-access triggers, and not the three pattern actions (write-only ints
# with no access flag -- the one place the declaration is silent about it).
declared = [p["key"] for p in mod["capabilities"]["chain_params"]
            if p.get("type") != "canvas" and p.get("access") != "write"]
write_only = {"pat_store", "pat_recall", "pat_delete"}
missing = [k for k in declared if k not in st and k not in write_only]
need(not missing, f"declared params missing from state: {missing}")
need(len(json.dumps(st)) < 60000, "state is too close to the 64KB param cap")
print(f"  state: {len(st)} keys, {len(json.dumps(st))} bytes")
sys.exit(bad)
PY
"$OUT/t"
