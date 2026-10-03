/*
 * EVERY WAY TO TRANSPOSE, AND WHAT EACH ONE MOVES.
 *
 * Four controls move pitch, and they were found missing on 2026-10-03
 * because none of them was called a clip transpose and one was not on the
 * panel at all. Pinned here by what they do, measured on the published
 * picture AND on the notes a lap actually injects:
 *
 *   Key (MAIN)              rewrites the progression: the WRITTEN roots move
 *   Clip Transpose (CLIP)   moves what SOUNDS by semitones, writes nothing:
 *                           Key, the written roots and a round trip to 0 are
 *                           untouched; the caption names the chord that
 *                           sounds; it lands AFTER the scale snap, or the
 *                           snap pulls a shifted degree chord back into key
 *   Clip Octave (CLIP)      moves what sounds by octaves, writes nothing
 *   Transpose (VOICE)       one chord, semitones
 *   Chord Oct (CHORD)       one chord, octaves
 *
 * And the edges: Clip Transpose survives a save, a blob from before it
 * existed restores it at 0, and everything stacked at its top still lands
 * inside MIDI.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }

static uint8_t pk[8192][4];
static int npk = 0;
static int inject(const uint8_t *msg, int len) {
    if (len == 4 && npk < 8192) { memcpy(pk[npk], msg, 4); npk++; }
    return 0;
}

static plugin_api_v2_t *A;
static void *I;
static int bad = 0;

/* Chord n (1-based) out of `prog`: the name that SOUNDS, the root that is
 * WRITTEN, and the resolved pitches. */
typedef struct { char name[16], written[8]; int p[8], n; } row_t;
static row_t row(int n) {
    static char b[65536];
    row_t r; memset(&r, 0, sizeof r);
    A->get_param(I, "prog", b, sizeof b);
    char *s = b;
    for (int k = 0; k < 13 && s; k++) { s = strchr(s, '|'); if (s) s++; }
    for (int k = 1; k < n && s; k++) { s = strchr(s, ';'); if (s) s++; }
    if (!s) return r;
    char line[512]; int i = 0;
    while (s[i] && s[i] != ';' && i < 511) { line[i] = s[i]; i++; }
    line[i] = 0;
    char *f1 = strchr(line, ','); if (!f1) return r; *f1++ = 0;
    char *f2 = strchr(f1, ','); if (f2) *f2 = 0;
    snprintf(r.name, sizeof r.name, "%s", line);
    snprintf(r.written, sizeof r.written, "%s", f1);
    char *pit = strrchr(f2 ? f2 + 1 : f1, ',');
    for (char *q = pit ? pit + 1 : NULL; q && *q && r.n < 8; ) {
        r.p[r.n++] = atoi(q);
        q = strchr(q, '.'); if (q) q++;
    }
    return r;
}
static void check(const char *what, int ok, const char *detail) {
    printf("  %-60s %s%s%s\n", what, ok ? "ok" : "FAIL", detail ? "  " : "", detail ? detail : "");
    if (!ok) bad = 1;
}
static int shifted(row_t a, row_t b, int by) {
    if (a.n == 0 || a.n != b.n) return 0;
    for (int i = 0; i < a.n; i++) if (b.p[i] != a.p[i] + by) return 0;
    return 1;
}
static char *dump(row_t r) {
    static char s[128]; int at = snprintf(s, sizeof s, "%s/%s:", r.name, r.written);
    for (int i = 0; i < r.n; i++) at += snprintf(s + at, sizeof s - at, " %d", r.p[i]);
    return s;
}
static void set(const char *k, const char *v) { A->set_param(I, k, v); }
static int geti(const char *k) { char v[32]; A->get_param(I, k, v, sizeof v); return atoi(v); }

/* One bar on Move's clock; the pitches of the FIRST strike, sorted. */
static int lap_first_chord(int *out) {
    npk = 0;
    int16_t audio[256];
    set("run", "on");
    uint8_t start = 0xFA, stop = 0xFC;
    A->on_midi(I, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    for (int c = 0; c < BAR_CLOCKS / 2; c++) {
        uint8_t clk = 0xF8;
        A->on_midi(I, &clk, 1, MOVE_MIDI_SOURCE_EXTERNAL);
        A->render_block(I, audio, 128);
    }
    A->on_midi(I, &stop, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    for (int i = 0; i < 50; i++) A->render_block(I, audio, 128);
    int n = 0;
    for (int i = 0; i < npk && n < 8; i++)
        if ((pk[i][1] & 0xF0) == 0x90 && pk[i][3] > 0) out[n++] = pk[i][2];
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && out[j - 1] > out[j]; j--) { int t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}

int main(int argc, char **argv) {
    if (argc < 2) { puts("usage: transpose <scratch dir>"); return 2; }
    char cfg[400]; snprintf(cfg, sizeof cfg, "{\"state_path\":\"%s/state.json\"}", argv[1]);
    host_api_v1_t h; memset(&h, 0, sizeof h);
    h.log = lg; h.get_bpm = bp; h.midi_inject_to_move = inject;
    A = move_plugin_init_v2(&h);
    I = A->create_instance(NULL, cfg);
    set("hum_time", "0"); set("hum_vel", "0");
    set("scale", "Minor"); set("key", "A");
    set("common", "I-V-vi-IV");
    set("sel", "1");
    row_t base = row(1), base2 = row(2);
    printf("  base: chord 1 %s", dump(base)); printf("   chord 2 %s\n", dump(base2));

    /* --- Key: the WRITTEN progression moves --- */
    set("key", "~1");
    row_t k1 = row(1);
    check("Key +1 detent: every sounding pitch +1", shifted(base, k1, 1), dump(k1));
    check("Key +1 detent: the WRITTEN root moved too", strcmp(k1.written, base.written) != 0, k1.written);
    set("key", "~-1");
    check("Key -1 detent: back where it was", shifted(base, row(1), 0), NULL);

    /* --- Clip Transpose: what SOUNDS moves, nothing written does --- */
    set("transpose", "1");
    row_t t1 = row(1), t2 = row(2);
    check("Clip Transpose +1: chord 1 sounds +1", shifted(base, t1, 1), dump(t1));
    check("Clip Transpose +1: chord 2 sounds +1 (whole clip)", shifted(base2, t2, 1), dump(t2));
    check("Clip Transpose +1: written root unchanged", strcmp(t1.written, base.written) == 0, t1.written);
    check("Clip Transpose +1: the caption names what sounds (not the written)",
          strcmp(t1.name, base.name) != 0, t1.name);
    { char v[8]; A->get_param(I, "key", v, sizeof v);
      check("Clip Transpose +1: Key is untouched", strcmp(v, "A") == 0, v); }
    {
        int got[8], n = lap_first_chord(got), ok = n == t1.n;
        for (int i = 0; ok && i < n; i++) ok = got[i] == t1.p[i];
        char d[64]; int at = 0; for (int i = 0; i < n; i++) at += snprintf(d + at, sizeof d - at, "%d ", got[i]);
        check("Clip Transpose +1: a lap on Move's clock INJECTS those pitches", ok, d);
    }
    set("transpose", "0");
    check("Clip Transpose back to 0: exactly where it was", shifted(base, row(1), 0), NULL);
    set("transpose", "99");
    check("Clip Transpose clamps at +12", geti("transpose") == 12, NULL);
    set("transpose", "-99");
    check("Clip Transpose clamps at -12", geti("transpose") == -12, NULL);
    set("transpose", "0");

    /* --- after the snap: a DEGREE chord shifted out of the scale stays out --- */
    set("degree", "I");                       /* chord 1 is now a degree chord: snapped */
    row_t d0 = row(1);
    set("transpose", "1");                    /* A minor + 1 = A#, not in A minor */
    row_t d1 = row(1);
    check("a degree chord + Clip Transpose 1 is NOT snapped back", shifted(d0, d1, 1), dump(d1));
    set("transpose", "0");

    /* --- Clip Octave --- */
    set("octave", "1");
    row_t o1 = row(1);
    check("Clip Octave +1: chord 1 sounds +12", shifted(d0, o1, 12), dump(o1));
    check("Clip Octave +1: chord 2 sounds +12 (whole clip)", shifted(base2, row(2), 12), NULL);
    check("Clip Octave +1: written root unchanged", strcmp(o1.written, d0.written) == 0, o1.written);
    set("octave", "0");

    /* --- per chord: VOICE Transpose and CHORD Chord Oct --- */
    set("ctrans", "2");
    check("Transpose (VOICE) +2: chord 1 sounds +2", shifted(d0, row(1), 2), NULL);
    check("Transpose (VOICE) +2: chord 2 untouched", shifted(base2, row(2), 0), NULL);
    set("ctrans", "0");
    { char v[16]; snprintf(v, sizeof v, "%d", geti("coct") + 1); set("coct", v); }
    check("Chord Oct +1: chord 1 sounds +12", shifted(d0, row(1), 12), NULL);
    check("Chord Oct +1: chord 2 untouched", shifted(base2, row(2), 0), NULL);
    { char v[16]; snprintf(v, sizeof v, "%d", geti("coct") - 1); set("coct", v); }

    /* --- it survives a save; a blob from before it restores 0 --- */
    set("transpose", "-5");
    static char blob[65536];
    A->get_param(I, "state", blob, sizeof blob);
    set("transpose", "0");
    set("state", blob);
    check("Clip Transpose survives a state round trip", geti("transpose") == -5, NULL);
    {
        static char old[65536];
        snprintf(old, sizeof old, "%s", blob);
        char *t = strstr(old, ",\"transpose\":\"-5\"");
        if (t) memmove(t, t + strlen(",\"transpose\":\"-5\""), strlen(t + strlen(",\"transpose\":\"-5\"")) + 1);
        check("  (the fixture really lacks the key)", t != NULL && !strstr(old, "\"transpose\""), NULL);
        set("state", old);
        check("a blob from before Clip Transpose restores it at 0", geti("transpose") == 0, NULL);
    }

    /* --- everything at its top still lands inside MIDI. CLAMPED, not folded:
     *     past 127 the tones pile up on 127, which is how a B5 13th at Clip
     *     Octave +2 already behaved before Clip Transpose existed. Pinned so a
     *     wrap to a low note -- the failure that would actually hurt -- cannot
     *     arrive unnoticed. --- */
    set("common", "I-V-vi-IV"); set("sel", "1");
    set("root", "B5"); set("shape", "13"); set("ctrans", "12");
    set("octave", "2"); set("transpose", "12");
    {
        row_t top = row(1); int ok = top.n > 0;
        for (int i = 0; i < top.n; i++) if (top.p[i] < 0 || top.p[i] > 127) ok = 0;
        check("everything at its top: clamped into 0..127, never wrapped", ok, dump(top));
    }

    A->destroy_instance(I);
    printf("\n%s\n", bad ? "FAIL" : "Every transpose moves what it says, and only that");
    return bad;
}
