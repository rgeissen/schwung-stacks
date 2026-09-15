#!/bin/bash
# ui.js is the ctx and the lifecycle for canvas.js as an overtake tool. See the .mjs.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
command -v node >/dev/null || { echo "SKIP: node not installed"; exit 0; }
node "$ROOT/tests/test_stacks_ui_lifecycle.mjs"
