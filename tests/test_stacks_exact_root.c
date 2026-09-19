/*
 * A ROOT YOU NAMED IS THE ROOT THAT SOUNDS. A root you asked for BY DEGREE is
 * the scale's to move.
 *
 * The library stores progressions in semitones so that a bVII, a borrowed bVI
 * or a tritone sub can step OUTSIDE the scale -- its own comment says a degree
 * table would render those "as their nearest diatonic neighbour, a different
 * chord, quietly". Then chord_notes() snapped every root to the scale and did
 * exactly that: "bVII descent" in C Major played C G B F, "Mixolydian" played
 * C B F C, and the staff captioned the B as "A#" because the caption read the
 * WRITTEN root. Two answers to "what chord is this", and the one you could
 * read was the wrong one. Measured 2026-09-19.
 *
 * So a chord carries `exact`: set by the libraries, the Root knob and a clip
 * read, cleared by the degree pads. And the caption names the SOUNDING root.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m){ (void)m; } static float bp(void){ return 120.f; }

static int sounds_pc(stk_t *s, int k) {
    int t[8];
    int n = chord_notes_g(&s->prog.ch[k], 0, s->prog.scale, s->prog.key, 0, 0, NULL, 0, t, 6);
    return n ? t[0] % 12 : -1;
}
/* The NAME field of chord k in `prog`, e.g. "A#" of "A#,A#3,0,..." */
static void caption(stk_t *s, int k, char *out, int len) {
    char buf[8192]; stk_get_param(s, "prog", buf, sizeof buf);
    char *p = strchr(buf, '|'); for (int i = 0; i < 12 && p; i++) p = strchr(p + 1, '|');
    p = p ? p + 1 : buf;
    for (int i = 0; i < k && p; i++) { p = strchr(p, ';'); if (p) p++; }
    int i = 0;
    while (p && p[i] && p[i] != ',' && i < len - 1) { out[i] = p[i]; i++; }
    out[i] = 0;
}

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    move_midi_fx_init(&h);
    int bad = 0;
    char cap[16];

    stk_t *s = (stk_t *)stk_create(NULL, "{\"state_path\":\"/dev/null\"}");
    stk_set_param(s, "key", "C");
    stk_set_param(s, "scale", "Major");

    /* 1. The library's bVII sounds as written, and is captioned so. */
    stk_set_param(s, "progression", "bVII descent");
    caption(s, 2, cap, sizeof cap);
    int pc = sounds_pc(s, 2);
    int ok = pc == 10 && strcmp(cap, "A#") == 0;
    printf("  bVII descent in C Major, chord 3 sounds %s, captioned %-4s %s\n",
           NOTE_NAMES[pc], cap, ok ? "ok" : "FAIL: the library's bVII was snapped");
    if (!ok) bad = 1;

    /* 2. A root named on the knob is exact too. */
    stk_set_param(s, "clear", "on");
    stk_set_param(s, "sel", "1");
    stk_set_param(s, "shape", "maj");
    stk_set_param(s, "root", "D#3");
    pc = sounds_pc(s, 0);
    caption(s, 0, cap, sizeof cap);
    ok = pc == 3 && strcmp(cap, "D#") == 0;
    printf("  Root knob D# in C Major sounds %s, captioned %-4s          %s\n",
           NOTE_NAMES[pc], cap, ok ? "ok" : "FAIL: a named root was snapped");
    if (!ok) bad = 1;

    /* 3. A degree follows the scale: III in Major is E; in Minor it moves,
     *    and the caption moves with it. */
    stk_set_param(s, "degree", "III");
    int pc_major = sounds_pc(s, 0);
    stk_set_param(s, "scale", "Minor");
    int pc_minor = sounds_pc(s, 0);
    caption(s, 0, cap, sizeof cap);
    ok = pc_major == 4 && pc_minor != 4 && strcmp(cap, NOTE_NAMES[pc_minor]) == 0;
    printf("  degree III: Major sounds %s, Minor sounds %s, captioned %-4s %s\n",
           NOTE_NAMES[pc_major], NOTE_NAMES[pc_minor], cap,
           ok ? "ok" : "FAIL: a degree must re-snap and the caption must follow");
    if (!ok) bad = 1;

    /* 4. Persistence: `exact` rides in the blob, and a blob written before
     *    it existed (13 fields) loads with it clear. */
    stk_set_param(s, "scale", "Major");
    stk_set_param(s, "root", "A#3");             /* exact */
    char blob[STK_STATE_MAX];
    stk_get_param(s, "state", blob, sizeof blob);
    stk_t *r = (stk_t *)stk_create(NULL, "{\"state_path\":\"/dev/null\"}");
    stk_set_param(r, "state", blob);
    ok = r->prog.ch[0].exact == 1 && sounds_pc(r, 0) == 10;
    printf("  exact survives a save and restore                        %s\n",
           ok ? "ok" : "FAIL");
    if (!ok) bad = 1;
    const char *old = "{\"s\":\"v6|0|1|1|4|3|0|100|90|0|0|0|1|1|0|0|0|0|1|0|0|0|0|1|1|58.4.0.8.0.0.0.0.0.0.0.0.0;#1\"}";
    stk_t *o = (stk_t *)stk_create(NULL, "{\"state_path\":\"/dev/null\"}");
    stk_set_param(o, "state", old);
    ok = o->prog.count == 1 && o->prog.ch[0].root == 58 && o->prog.ch[0].exact == 0
         && sounds_pc(o, 0) == 11;
    printf("  a 13-field blob loads, root 58 not exact, snaps to B        %s\n",
           ok ? "ok" : "FAIL: the old blob must still load");
    if (!ok) bad = 1;

    stk_destroy(s); stk_destroy(r); stk_destroy(o);
    printf("\n%s\n", bad ? "FAIL" : "a named root sounds as named; a degree is the scale's");
    return bad;
}
