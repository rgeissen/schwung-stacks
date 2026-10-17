#!/bin/bash
# `chord_of:<notes>` names a chord someone else plays, exactly as `prog`
# names Stacks' own (listen mode on Push). Pinned: any order, inversions
# with their bass, a triad grown to a seventh, a single note, a cluster no
# shape matches (KNOWN 0), nothing for no notes -- and chords as hands play
# them: a 5th, 9th or 11th left out, a bass outside the chord, intervals.
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
"$OUT/m" 60.64.67 67.60.64 64.67.72 60.64.67.71 60.64.67.70 57.60.64 62 60.61.62 "" \
    60.64.71 60.64.70.74 60.64.70.81 48.62.66.69 43.65.69.72 54.60.64.67 60.64.65.67 \
    57.60.64.68 60.64 60.67 60.64.67.69 57.60.64.67 > "$OUT/o"
want() { grep -qx "$1=$2" "$OUT/o" || { echo "FAIL: $1 -> $(grep "^$1=" "$OUT/o"), want $2"; cat "$OUT/o"; exit 1; }; }
# NAME|ROOT|INV|BASS|KNOWN|OMIT
want 60.64.67      "C|0|0|0|1|-"          # C major, root position
want 67.60.64      "C|0|0|0|1|-"          # any order: the lowest note is the bass
want 64.67.72      "C|0|1|4|1|-"          # first inversion: bass E
want 60.64.67.71   "Cmaj7|0|0|0|1|-"      # a note added: the name grows
want 60.64.67.70   "C7|0|0|0|1|-"
want 57.60.64      "Am|9|0|9|1|-"
want 62            "D|2|0|2|1|-"          # one note is a note
want 60.61.62      "C|0|0|0|0|-"          # a cluster: nothing fits, KNOWN 0
want ""            ""
# Played, not written: what a hand leaves out, and a bass outside the chord.
want 60.64.71      "Cmaj7|0|0|0|1|7"      # no 5th
want 60.64.70.74   "C9|0|0|0|1|7"         # no 5th
want 60.64.70.81   "C13|0|0|0|1|2.7"      # no 9th, no 5th: 1-3-b7-13
want 48.62.66.69   "D7|2|3|0|1|-"         # C under D F# A: D7 in 3rd inversion, not a slash
want 43.65.69.72   "Fadd9|5|3|7|1|-"      # "F/G": complete as Fadd9/G, before G9sus4 missing its 5th
want 54.60.64.67   "C|0|-1|6|1|-"         # F# under C major: C/F#, the bass outside
want 60.64.65.67   "Cadd11|0|0|0|1|-"     # a shape Stacks does not write
want 57.60.64.68   "AmMaj7|9|0|9|1|-"
want 60.64         "|0|0|0|2|-"           # two notes: an interval, said by the caller
want 60.67         "C5|0|0|0|1|-"
want 60.64.67.69   "C6|0|0|0|1|-"         # C6, not Am7/C: the bass on the root wins a tie
want 57.60.64.67   "Am7|9|0|9|1|-"
echo "PASS: chord_of names played chords as prog names Stacks' own"
