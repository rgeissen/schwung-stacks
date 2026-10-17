#!/bin/bash
# Coverage of name_played (chord_of, listen mode): every chord shape --
# SHAPES[] from the triads on and the played-only ones -- in all 12 keys,
# in every inversion, close-voiced, and from four notes up also without
# its 5th, must come back named (KNOWN 1), with no note left over. Asked
# on Push: "we are not covering all chords".
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cat > "$OUT/m.c" <<'C'
#include <stdio.h>
#include <string.h>
#include "dsp/stacks.c"
int main(void) {
    int checked = 0, failed = 0;
    for (int t = 0; ; t++) {
        const char *suf; const int *iv; int tn;
        int ok = played_template(t, &suf, &iv, &tn);
        if (ok < 0) break;
        if (!ok) continue;
        for (int drop5 = 0; drop5 < 2; drop5++) {
            int have5 = 0;
            for (int k = 0; k < tn; k++) if (iv[k] == 7) have5 = 1;
            if (drop5 && (tn < 4 || !have5)) continue;
            for (int root = 48; root < 60; root++) {
                int tones[8], m = 0;
                for (int k = 0; k < tn; k++) if (!(drop5 && iv[k] == 7)) tones[m++] = root + iv[k];
                for (int inv = 0; inv < m; inv++) {
                    int v[8];
                    for (int k = 0; k < m; k++) v[k] = tones[(k + inv) % m] + ((k + inv) >= m ? 12 : 0);
                    for (int i = 1; i < m; i++) for (int j = i; j > 0 && v[j-1] > v[j]; j--) { int x = v[j]; v[j] = v[j-1]; v[j-1] = x; }
                    int rpc, in, known; const char *s; uint16_t omit;
                    name_played(v, m, &rpc, &s, &in, &known, &omit);
                    checked++;
                    if (known != 1 && !(m == 2 && known == 2)) { /* two notes may be an interval */
                        failed++;
                        if (failed <= 12) printf("UNNAMED %s%s root %d inv %d:", suf, drop5 ? " no5" : "", root, inv);
                        if (failed <= 12) { for (int k = 0; k < m; k++) printf(" %d", v[k]); printf("\n"); }
                    }
                }
            }
        }
    }
    printf("checked %d, unnamed %d\n", checked, failed);
    return failed != 0;
}
C
cc -O1 -I "$ROOT/src" -o "$OUT/m" "$OUT/m.c" -lpthread -lm 2>"$OUT/err" || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
if "$OUT/m" > "$OUT/o"; then
    echo "PASS: every shape, key, inversion and 5th-less voicing named ($(tail -1 "$OUT/o"))"
else
    echo "FAIL: chords left unnamed"; cat "$OUT/o"; exit 1
fi
