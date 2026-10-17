#!/bin/bash
# `chord_of:<notes>` names a chord someone else plays, exactly as `prog`
# names Stacks' own (listen mode on Push). Pinned: any order, inversions
# with their bass, a triad grown to a seventh, a single note, a cluster no
# shape matches (KNOWN 0), and nothing for no notes.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cat > "$OUT/m.c" <<'C'
#include <stdio.h>
#include <string.h>
#include "dsp/stacks.c"
static void lg(const char *m) { (void)m; }
int main(int argc, char **argv) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg;
    plugin_api_v2_t *a = move_plugin_init_v2(&h);
    void *in = a->create_instance(NULL, "{\"state_path\":\"/dev/null/x\"}");
    for (int i = 1; i < argc; i++) {
        char k[128], b[64]; snprintf(k, sizeof k, "chord_of:%s", argv[i]);
        a->get_param(in, k, b, sizeof b); printf("%s=%s\n", argv[i], b);
    }
    return 0;
}
C
cc -O1 -I "$ROOT/src" -o "$OUT/m" "$OUT/m.c" -lpthread -lm 2>"$OUT/err" || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/m" 60.64.67 67.60.64 64.67.72 60.64.67.71 60.64.67.70 57.60.64 62 60.61.62 "" > "$OUT/o"
want() { grep -qx "$1=$2" "$OUT/o" || { echo "FAIL: $1 -> $(grep "^$1=" "$OUT/o"), want $2"; cat "$OUT/o"; exit 1; }; }
want 60.64.67      "C|0|0|0|1"        # C major, root position
want 67.60.64      "C|0|0|0|1"        # any order: the lowest note is the bass
want 64.67.72      "C|0|1|4|1"        # first inversion: bass E
want 60.64.67.71   "Cmaj7|0|0|0|1"    # a note added: the name grows
want 60.64.67.70   "C7|0|0|0|1"
want 57.60.64      "Am|9|0|9|1"
want 62            "D|2|0|2|1"        # one note is a note
want 60.61.62      "C|0|0|0|0"        # a cluster: no shape, KNOWN 0
want ""            ""
echo "PASS: chord_of names played chords as prog names Stacks' own"
