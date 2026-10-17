#!/bin/bash
# A chord's rhythm mask in `prog` is DECIMAL (fmt_prog prints it with %d;
# the header's scale mask is hex). Both surfaces read it as hex until
# 2026-10-17, so the staff drew a different figure than the one played --
# found on Push, where hits were checked against what Stacks played.
# Pinned: the C side writes "quarter" as 4369 (= 0x1111), and every parser
# of that field reads it with radix 10.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cat > "$OUT/m.c" <<'C'
#include <stdio.h>
#include <string.h>
#include "dsp/stacks.c"
static void lg(const char *m) { (void)m; }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg;
    plugin_api_v2_t *a = move_plugin_init_v2(&h);
    void *i = a->create_instance(NULL, "{\"state_path\":\"/dev/null/x\"}");
    a->set_param(i, "progression", "Axis");
    a->set_param(i, "sel", "1");
    a->set_param(i, "rhythm", "quarter");
    char p[4096]; a->get_param(i, "prog", p, sizeof p);
    puts(p);
    return 0;
}
C
cc -O1 -I "$ROOT/src" -o "$OUT/m" "$OUT/m.c" -lpthread -lm 2>"$OUT/err" || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
mask=$("$OUT/m" | cut -d'|' -f14- | cut -d';' -f1 | cut -d, -f8)
[ "$mask" = "4369" ] || { echo "FAIL: quarter's mask in prog is '$mask', want decimal 4369 (0x1111)"; exit 1; }
for f in src/canvas.js src/web_ui.html; do
    if grep -q 'parseInt(f\[7\], *16)' "$ROOT/$f"; then echo "FAIL: $f reads the rhythm mask as hex"; exit 1; fi
    grep -q 'parseInt(f\[7\], *10)' "$ROOT/$f" || { echo "FAIL: $f does not read the rhythm mask as decimal"; exit 1; }
done
echo "PASS: the rhythm mask is decimal in prog and both surfaces read it as decimal"
