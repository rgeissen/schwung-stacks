/*
 * YOU ARE ALWAYS IN A SLOT, and everything runs against the one you are in.
 *
 * This is pattern SELECT, not save and load. Slot 1 is active and empty at
 * start; every edit belongs to the active slot; activating another slot READS
 * it and you carry on there. A pattern is the BUFFER -- progression, clip
 * length, voice grouping -- and never a clip: nothing here touches Song.abl.
 *
 * It was built the other way first, and the two complaints that produced were
 * really one: tapping an empty slot either captured the buffer into it (so a
 * slot you meant to leave empty became a pattern) or did nothing at all (so
 * the slot could not be reached). Neither is what a pattern button does.
 *
 * The rules, each here because getting it wrong is invisible until somebody
 * loses work:
 *
 *   * SWITCHING AWAY KEEPS WHAT YOU WROTE. There is no store step, so if the
 *     switch does not write through, the edit is simply gone.
 *   * AN EMPTY SLOT IS AN EMPTY BUFFER -- activating one gives you somewhere
 *     to write, in the key and scale you were working in.
 *   * `used` MEANS HAS MUSIC IN IT, and for the active slot it is read from
 *     the live buffer, so the lamp lights as you write the first chord.
 *   * ACTIVATING BRINGS THE CLIP LENGTH WITH IT. A twelve-bar phrase read into
 *     a four-bar clip otherwise leaves two thirds of itself outside.
 *   * DELETING THE SLOT YOU ARE IN empties what you are playing, because they
 *     are the same thing.
 *   * THE PATTERNS SURVIVE, in the state blob -- and a blob written before
 *     patterns existed must still load.
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

    /* --- you start IN slot 1, and it is empty --- */
    stk_t *s = (stk_t *)stk_create(NULL, NULL);
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0000|1") == 0, "at start: slot 1 active, nothing stored");

    /* Writing a chord lights the slot you are in, with no store step. */
    progression(s, "I", "IV", "V", "VI");
    stk_set_param(s, "bars", "8");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0001|1") == 0, "writing into slot 1 lights slot 1");
    int root_1 = s->prog.ch[1].root;

    /* --- activating an empty slot gives you somewhere to write --- */
    stk_set_param(s, "pat_recall", "4");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0001|4") == 0, "slot 4 is active, and still empty");
    check(buffer_is_empty(&s->prog), "...and the buffer is empty, ready to write");
    check(s->bars == 4, "...with the clip back to its default length");

    /* What slot 1 held is still there, untouched. */
    check(s->pat[0].used && s->pat[0].prog.ch[1].root == root_1,
          "switching away KEPT what was written in slot 1");

    /* Write something different here. */
    progression(s, "II", "II", "II", "II");
    int root_4 = s->prog.ch[1].root;
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0009|4") == 0, "now both 1 and 4 hold music");

    /* --- and back again, reading what is there --- */
    stk_set_param(s, "pat_recall", "1");
    check(s->prog.ch[1].root == root_1, "activating 1 reads slot 1 back");
    check(s->bars == 8, "...including the clip length it was written against");
    stk_set_param(s, "pat_recall", "4");
    check(s->prog.ch[1].root == root_4, "activating 4 reads slot 4 back");

    /* --- the long press copies what you are playing somewhere else --- */
    stk_set_param(s, "pat_store", "9");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0109|9") == 0, "copy to 9: it holds music and you are in it");
    check(s->prog.ch[1].root == root_4, "...and you are still playing what you copied");

    /* --- deleting --- */
    stk_set_param(s, "pat_delete", "1");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0108|9") == 0, "delete 1: gone, and you are still in 9");
    stk_set_param(s, "pat_delete", "9");
    stk_get_param(s, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0008|9") == 0, "deleting the slot you are IN leaves you there, empty");
    check(buffer_is_empty(&s->prog), "...and empties what you were playing");

    /* --- the round trip, which is what makes them survive a reload --- */
    stk_t *t = (stk_t *)stk_create(NULL, NULL);
    progression(t, "I", "V", "VI", "IV");
    stk_set_param(t, "bars", "8");
    stk_set_param(t, "pat_recall", "5");
    progression(t, "II", "III", "II", "III");
    int a_root = t->pat[0].prog.ch[2].root, b_root = t->prog.ch[1].root;
    stk_get_param(t, "state", buf, sizeof buf);

    stk_t *u = (stk_t *)stk_create(NULL, NULL);
    stk_set_param(u, "state", buf);
    stk_get_param(u, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0011|5") == 0, "restored: 1 and 5 hold music, 5 is active");
    check(u->prog.ch[1].root == b_root, "restored: the ACTIVE slot is what you are playing");
    check(u->pat[0].prog.ch[2].root == a_root, "restored: the other slot kept its own music");
    check(u->pat[0].bars == 8, "restored: and its own clip length");

    /* A blob from before patterns existed must still load. */
    stk_t *v = (stk_t *)stk_create(NULL, NULL);
    char old[4096];
    snprintf(old, sizeof old, "{\"s\":\"v6|9|2|1|4|4|0|100|90|0|0|0|1|1|0|0|0|0|1|0|0|0|0|1|2|"
                              "57.14.0.8.0.0.0.0.0.0.0.0.0;59.14.0.8.0.0.0.0.0.0.0.0.0\"}");
    stk_set_param(v, "state", old);
    stk_get_param(v, "patterns", buf, sizeof buf);
    check(strcmp(buf, "0001|1") == 0, "an old blob loads into slot 1, which now holds it");
    check(v->prog.count == 2, "and its progression is intact");

    stk_destroy(s); stk_destroy(t); stk_destroy(u); stk_destroy(v);
    printf(bad ? "\nFAIL\n" : "\nPASS\n");
    return bad;
}
