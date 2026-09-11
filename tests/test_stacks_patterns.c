/*
 * SIXTEEN STORED BUFFERS, and what has to be true of them.
 *
 * A pattern is the BUFFER -- the progression, the clip length it was written
 * against, and the voice grouping it was voiced with -- and emphatically not a
 * clip: nothing here touches Song.abl, so storing costs nothing and Move
 * cannot overwrite it.
 *
 * The rules that are easy to get wrong, and each one is here because getting
 * it wrong is invisible until somebody loses work:
 *
 *   * RECALL BRINGS THE CLIP LENGTH WITH IT. A twelve-bar phrase recalled into
 *     a four-bar clip leaves two thirds of itself outside -- playing, drawn,
 *     and absent from anything Stamp writes.
 *   * `pat_cur` means "where this buffer CAME FROM", not "the last button you
 *     pressed", so deleting the current slot must clear it: a lit button
 *     pointing at an empty slot is a lie the eye cannot check.
 *   * THE PATTERNS SURVIVE, because they ride in the state blob -- and a blob
 *     written before patterns existed must still load.
 *   * Storing is not an edit, so it must not consume the undo stack; recalling
 *     IS one, so it must be undoable.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m){ (void)m; } static float bp(void){ return 120.f; }

static stk_t *fresh(void) {
    stk_t *s = (stk_t *)stk_create(NULL, NULL);
    stk_set_param(s, "clear", "on");
    return s;
}
static void progression(stk_t *s, const char *a, const char *b, const char *c, const char *d) {
    const char *g[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) {
        char n[8]; snprintf(n, sizeof n, "%d", i + 1);
        stk_set_param(s, "sel", n); stk_set_param(s, "degree", g[i]);
    }
}
static int bad = 0;
static void check(int ok, const char *what) {
    printf("  %-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) bad = 1;
}

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    move_midi_fx_init(&h);
    char buf[131072];

    stk_t *s = fresh();
    progression(s, "I", "IV", "V", "VI");
    int root_a = s->prog.ch[1].root;
    stk_set_param(s, "bars", "8");
    stk_set_param(s, "pat_store", "3");

    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0004|3") == 0, "store 3: mask says slot 3, current is 3");

    /* Change the buffer, then bring the pattern back. */
    progression(s, "II", "II", "II", "II");
    stk_set_param(s, "bars", "2");
    check(s->prog.ch[1].root != root_a, "the buffer really changed");
    stk_set_param(s, "pat_recall", "3");
    check(s->prog.ch[1].root == root_a, "recall 3: the progression came back");
    check(s->bars == 8, "recall 3: and so did its clip length (8 bars)");

    /* An empty slot is not a hole to fall into. */
    int before = s->prog.ch[1].root;
    stk_set_param(s, "pat_recall", "9");
    check(s->prog.ch[1].root == before, "recalling an EMPTY slot changes nothing");

    /* Sixteen of them, and the sixteenth is reachable. */
    stk_set_param(s, "pat_store", "16");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "8004|16") == 0, "slot 16 exists and is the current one");
    stk_set_param(s, "pat_store", "17");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "8004|16") == 0, "slot 17 is refused, not wrapped onto slot 1");

    /* Delete clears the lamp only when it was pointing at the deleted slot. */
    stk_set_param(s, "pat_delete", "3");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "8000|16") == 0, "delete 3: gone, and 16 is still current");
    stk_set_param(s, "pat_delete", "16");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0000|0") == 0, "deleting the CURRENT slot clears the lamp");

    /* --- the round trip, which is what makes them survive a reload --- */
    stk_t *t = fresh();
    progression(t, "I", "V", "VI", "IV");
    stk_set_param(t, "bars", "8");
    stk_set_param(t, "pat_store", "1");
    progression(t, "II", "III", "II", "III");
    stk_set_param(t, "bars", "4");
    stk_set_param(t, "pat_store", "5");
    int a_root = t->pat[0].prog.ch[2].root, b_root = t->pat[4].prog.ch[1].root;
    stk_get_param(t, "state", buf, sizeof buf);

    stk_t *u = fresh();
    stk_set_param(u, "state", buf);
    stk_get_param(u, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0011|5") == 0, "restored: slots 1 and 5 filled, 5 current");
    check(u->pat[0].prog.ch[2].root == a_root
       && u->pat[4].prog.ch[1].root == b_root, "restored: both progressions intact");
    check(u->pat[0].bars == 8 && u->pat[4].bars == 4, "restored: each kept its own clip length");

    /* A blob from before patterns existed must still load. */
    stk_t *v = fresh();
    char old[4096];
    snprintf(old, sizeof old, "{\"s\":\"v6|9|2|1|4|4|0|100|90|0|0|0|1|1|0|0|0|0|1|0|0|0|0|1|2|"
                              "57.14.0.8.0.0.0.0.0.0.0.0.0;59.14.0.8.0.0.0.0.0.0.0.0.0\"}");
    stk_set_param(v, "state", old);
    stk_get_param(v, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0000|0") == 0, "a pattern-less blob still loads, with no patterns");
    check(v->prog.count == 2, "and its progression is intact");

    /* Storing is not an edit; recalling is. */
    stk_t *w = fresh();
    progression(w, "I", "IV", "V", "VI");
    stk_set_param(w, "pat_store", "2");
    int depth_after_store = w->hist_depth;
    stk_set_param(w, "pat_store", "4");
    check(w->hist_depth == depth_after_store, "storing again consumes no undo");
    int r0 = w->prog.ch[0].root;
    progression(w, "II", "II", "II", "II");
    stk_set_param(w, "pat_recall", "2");
    stk_set_param(w, "undo", "on");
    check(w->prog.ch[0].root != r0 || w->prog.count > 0, "an accidental recall is undoable");

    stk_destroy(s); stk_destroy(t); stk_destroy(u); stk_destroy(v); stk_destroy(w);
    printf(bad ? "\nFAIL\n" : "\nPASS\n");
    return bad;
}
