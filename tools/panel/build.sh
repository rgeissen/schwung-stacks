#!/usr/bin/env bash
#
# Make docs/panel.html, Stacks' panel region by region, from the panel itself
# (src/web_ui.html in its demo mode: no Move or Push needed). Needs Chrome
# and, once, `npm --prefix tools/panel install` (playwright-core). Run it with
# any change to the panel, and commit the page with it.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
rm -rf "$ROOT/build/panel"
node "$ROOT/tools/panel/shots.js" "$ROOT/src/web_ui.html" "$ROOT/build/panel"
python3 -I "$ROOT/tools/panel/build.py" "$ROOT/build/panel" "$ROOT/docs/panel.html" "$(git -C "$ROOT" rev-parse --short HEAD)"
