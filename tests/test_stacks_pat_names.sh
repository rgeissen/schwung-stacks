#!/bin/bash
# `pat_names` says what each pattern slot holds -- count, bars, the first
# four chords by name -- so a surface can label a slot with its music
# instead of one word for every used slot. Pinned: an empty slot is an empty
# field, the active slot is the buffer, a stored slot keeps its own names
# after the buffer moves on, and a cleared slot goes empty again.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cat > "$OUT/m.c" <<'C'
#include <stdio.h>
#include <string.h>
#include "dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static plugin_api_v2_t *a; static void *in;
static void show(const char *tag) {
    char b[2048]; a->get_param(in, "pat_names", b, sizeof b); printf("%s %s\n", tag, b);
}
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg;
    a = move_plugin_init_v2(&h);
    in = a->create_instance(NULL, "{\"state_path\":\"/dev/null/x\"}");
    a->set_param(in, "clear", "on");
    show("empty");
    const char *deg[] = { "I", "IV", "V", "rest", "I" };
    for (int i = 0; i < 5; i++) {
        char s[4]; snprintf(s, sizeof s, "%d", i + 1);
        if (i >= 4) a->set_param(in, "insert", "on");
        a->set_param(in, "sel", s);
        a->set_param(in, "degree", deg[i]);
    }
    show("written");
    a->set_param(in, "pat_store", "3");
    a->set_param(in, "pat_recall", "2");
    show("moved");
    a->set_param(in, "pat_delete", "3");
    show("cleared");
    return 0;
}
C
cc -O1 -I "$ROOT/src" -o "$OUT/m" "$OUT/m.c" -lpthread -lm 2>"$OUT/err" || { echo "FAIL: compile"; cat "$OUT/err"; exit 1; }
"$OUT/m" > "$OUT/o"
field() { grep "^$1 " "$OUT/o" | cut -d' ' -f2- | cut -d'|' -f"$2"; }
fail() { echo "FAIL: $1"; cat "$OUT/o"; exit 1; }
[ "$(grep '^empty ' "$OUT/o" | cut -d' ' -f2- | tr -d '|')" = "" ] || fail "a cleared module names nothing"
w=$(field written 1)
case "$w" in 5:*:"A D E -") ;; *) fail "active slot 1 = '$w', want 5 chords named A D E - (I IV V rest in A, four max)";; esac
[ "$(echo "$w" | tr ' ' '\n' | wc -l | tr -d ' ')" = "4" ] || fail "more than four names in '$w'"
[ "$(field moved 3)" = "$w" ] || fail "stored slot 3 = '$(field moved 3)', want '$w'"
[ "$(field moved 2)" = "" ] || fail "recalled empty slot 2 should name nothing"
[ "$(field cleared 3)" = "" ] || fail "slot 3 still named after pat_delete"
echo "PASS: pat_names names each slot's chords, keeps stored ones, empties cleared ones"
