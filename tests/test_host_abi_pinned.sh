#!/usr/bin/env bash
#
# THE VENDORED HOST HEADERS MUST BE THE HOST'S, UNCHANGED.
#
# This module compiles against `host_api_v1_t` and `midi_fx_api_v1_t`, and
# reads `move_info_t` (move_info.h, a public header meant to be copied), all of
# which belong to Schwung. A module repo has to carry a copy to build standalone --
# and a carried copy is exactly how a device gets boot-looped.
#
# The story, from Schwung's own CLAUDE.md: breakbeat appended one callback to
# its private copy of plugin_api_v1.h that no host has ever provided. Every
# call site guards with `if (host->fn)`, which is sound only while the read is
# INSIDE the struct -- this one was one field past the end, landed on the next
# member of a heap object, tested non-NULL, and jumped into a data page.
# SIGSEGV on the SPI callback at load, taking MoveOriginal with it, on a slot
# restored at every boot. The host now carries `void *reserved[8]` so an
# over-read finds NULL, but that is a backstop, not permission.
#
# So the copies here are PINNED, not merely present: byte-identical to a named
# upstream commit. Editing them locally fails this test. Upstream moving on is
# a deliberate bump of src/host/UPSTREAM_REF, with a rebuild and a device test,
# never a silent drift.
#
# Offline it SKIPS rather than passes quietly -- a check that cannot run must
# not report success.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF="$(cat "$ROOT/src/host/UPSTREAM_REF" 2>/dev/null || echo)"
REPO="$(cat "$ROOT/src/host/UPSTREAM_REPO" 2>/dev/null || echo)"
[ -n "$REF" ] && [ -n "$REPO" ] || { echo "FAIL: src/host/UPSTREAM_REF or UPSTREAM_REPO missing"; exit 1; }

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
fail=0
for h in midi_fx_api_v1.h plugin_api_v1.h move_info.h; do
    url="https://raw.githubusercontent.com/$REPO/$REF/src/host/$h"
    if ! curl -fsS -m 20 "$url" -o "$TMP/$h" 2>/dev/null; then
        echo "SKIP: cannot reach $REPO at $REF (offline?) -- the ABI pin was NOT checked"
        exit 0
    fi
    if ! diff -q "$TMP/$h" "$ROOT/src/host/$h" >/dev/null; then
        echo "FAIL: src/host/$h differs from $REPO@${REF:0:9}"
        diff -u "$TMP/$h" "$ROOT/src/host/$h" | head -20
        fail=1
    fi
done
[ "$fail" = 0 ] || exit 1
echo "PASS: $(basename "$0")  (host ABI headers match $REPO@${REF:0:9})"
