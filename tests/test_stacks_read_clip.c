/*
 * READ CLIP IMPORTS THE MUSIC AT THE LENGTH IT WAS PLAYED.
 *
 * The reader was written when `len` meant "eighths of a STEP" and the step
 * was whatever Rate said. It inferred a step from the gap between the first
 * two chords, counted every length in eighths of it, and laid the chords on
 * that uniform grid. When `len` became an absolute eighth of a BAR the player
 * moved and the reader did not: a clip of two-bar chords (step = two bars, so
 * "one step" = 8) came back as ONE-bar chords and an eight-bar clip imported
 * as four; a phrase whose chords were not all the same length landed on the
 * wrong bars; and `len` was still clamped at 16 against a range of 64.
 *
 * Nothing caught it because nothing read a clip: every other door that
 * changes how much music there is has a test, and this was the one that did
 * not. Measured 2026-09-19.
 *
 * SETS_DIR is compiled in from the test script, pointing at a fixture set.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m){ (void)m; } static float bp(void){ return 120.f; }

/* One clip's worth of chords, as Song.abl carries them: (note, beat, beats). */
typedef struct { int note; double at, dur; } nt;

/* ONE set, overwritten per case: the reader picks the newest Song.abl by
 * mtime, and two files a millisecond apart are not reliably ordered. */
static void write_set(const nt *ns, int n) {
    const char *name = "set";
    char path[1024];
    mkdir(SETS_DIR, 0755);
    snprintf(path, sizeof path, "%s/u", SETS_DIR); mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/u/%s", SETS_DIR, name); mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/u/%s/Song.abl", SETS_DIR, name);
    FILE *f = fopen(path, "w");
    fprintf(f, "{\"rootNote\":0,\"scale\":\"Major\",\"tracks\":[{\"clipSlots\":[{\"clip\":{\"isPlaying\":true,\"notes\":[");
    for (int i = 0; i < n; i++)
        fprintf(f, "%s{\"noteNumber\":%d,\"startTime\":%g,\"duration\":%g,\"velocity\":100}",
                i ? "," : "", ns[i].note, ns[i].at, ns[i].dur);
    fprintf(f, "]}}]}]}");
    fclose(f);
}

static int check(const char *label, const nt *ns, int n,
                 const int *want_len, const int *want_off, int want_n) {
    write_set(ns, n);
    stk_prog_t p; memset(&p, 0, sizeof p);
    int got = read_clip_chords(0, &p);
    int ok = got == want_n;
    for (int i = 0; ok && i < want_n; i++)
        if (p.ch[i].len != want_len[i] || p.ch[i].off != want_off[i]) ok = 0;
    printf("  %-34s %d chord(s):", label, got);
    for (int i = 0; i < got; i++) printf(" len %d off %d,", p.ch[i].len, p.ch[i].off);
    printf("  %s\n", ok ? "ok" : "FAIL");
    if (!ok) {
        printf("      wanted:");
        for (int i = 0; i < want_n; i++) printf(" len %d off %d,", want_len[i], want_off[i]);
        printf("\n");
    }
    return ok;
}

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    move_midi_fx_init(&h);
    int bad = 0;

    /* 1. Four two-bar triads across eight bars: 16 units each, 8 bars total. */
    {
        nt ns[12]; int n = 0;
        int roots[4] = { 57, 53, 48, 55 };
        for (int c = 0; c < 4; c++) for (int v = 0; v < 3; v++)
            ns[n++] = (nt){ roots[c] + (v == 0 ? 0 : v == 1 ? 4 : 7), c * 8.0, 8.0 };
        int L[4] = { 16, 16, 16, 16 }, O[4] = { 0, 0, 0, 0 };
        if (!check("four 2-bar chords, 8 bars", ns, n, L, O, 4)) bad = 1;
    }
    /* 2. Mixed lengths: two bars, one bar, one bar -- each starts where the
     *    one before ends, whatever the first gap was. */
    {
        nt ns[9]; int n = 0;
        double at[3] = { 0, 8, 12 }, dur[3] = { 8, 4, 4 };
        int roots[3] = { 60, 65, 67 };
        for (int c = 0; c < 3; c++) for (int v = 0; v < 3; v++)
            ns[n++] = (nt){ roots[c] + (v == 0 ? 0 : v == 1 ? 4 : 7), at[c], dur[c] };
        int L[3] = { 16, 8, 8 }, O[3] = { 0, 0, 0 };
        if (!check("2 bars + 1 bar + 1 bar", ns, n, L, O, 3)) bad = 1;
    }
    /* 3. Half-bar chords: len 4, not 8 -- the old reader's "one step" lie. */
    {
        nt ns[12]; int n = 0;
        int roots[4] = { 60, 65, 67, 60 };
        for (int c = 0; c < 4; c++) for (int v = 0; v < 3; v++)
            ns[n++] = (nt){ roots[c] + (v == 0 ? 0 : v == 1 ? 4 : 7), c * 2.0, 2.0 };
        int L[4] = { 4, 4, 4, 4 }, O[4] = { 0, 0, 0, 0 };
        if (!check("four half-bar chords", ns, n, L, O, 4)) bad = 1;
    }
    /* 4. The first chord comes in on beat 3: the offset is the first slot's
     *    and the slot is still a whole bar, so chord 2 lands on bar 2. */
    {
        nt ns[6]; int n = 0;
        int roots[2] = { 60, 67 };
        double at[2] = { 2, 4 }, dur[2] = { 2, 4 };
        for (int c = 0; c < 2; c++) for (int v = 0; v < 3; v++)
            ns[n++] = (nt){ roots[c] + (v == 0 ? 0 : v == 1 ? 4 : 7), at[c], dur[c] };
        int L[2] = { 8, 8 }, O[2] = { 4, 0 };
        if (!check("first chord on beat 3", ns, n, L, O, 2)) bad = 1;
    }

    printf("\n%s\n", bad ? "FAIL" : "Read Clip imports chords at the length they were played");
    return bad;
}
