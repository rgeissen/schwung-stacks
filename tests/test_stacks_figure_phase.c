/*
 * A RHYTHM FIGURE IS A PROPERTY OF THE BAR, and four things followed from
 * forgetting that -- all measured 2026-09-19, all in arm_chord():
 *
 *  1. The grid was counted from the CHORD'S onset, so a chord starting on
 *     beat three played the first half of its figure again: two half-bar
 *     chords under "son 3-2" came out as `3 | 3`, and the "2" side of the
 *     clave never sounded.
 *  2. Every hit got the figure's FIRST gap as its duration, so in an uneven
 *     figure the note-off of hit N fell after hit N+1 had re-struck the same
 *     pitches -- and cut it. Tresillo's "2" lasted four clocks, a 32nd.
 *  3. Swing moved the odd sixteenths only, so under the "eighth" figure --
 *     the one swing is for -- it did nothing at all.
 *  4. Humanise was hashed per (chord, voice), so every hit of a bar carried
 *     the identical deviation: a constant lean, repeated.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m){ (void)m; } static float bp(void){ return 120.f; }

typedef struct { int clock, on, off; } ev;

/* Drive Start + one bar of clocks; record the note-on / note-off count per clock. */
static int run_bar(stk_t *s, ev *evs, int max) {
    uint8_t out[64][3]; int lens[64];
    uint8_t start = 0xFA;
    stk_process_midi(s, &start, 1, out, lens, 64);
    stk_tick(s, 128, 44100, out, lens, 64);
    int ne = 0;
    for (int c = 0; c < BAR_CLOCKS; c++) {
        uint8_t clk = 0xF8; int on = 0, off = 0;
        int n = stk_process_midi(s, &clk, 1, out, lens, 64);
        for (int i = 0; i < n; i++) { if ((out[i][0]&0xF0)==0x90 && out[i][2]>0) on++; if ((out[i][0]&0xF0)==0x80) off++; }
        n = stk_tick(s, 128, 44100, out, lens, 64);
        for (int i = 0; i < n; i++) { if ((out[i][0]&0xF0)==0x90 && out[i][2]>0) on++; if ((out[i][0]&0xF0)==0x80) off++; }
        if ((on || off) && ne < max) { evs[ne].clock = c; evs[ne].on = on; evs[ne].off = off; ne++; }
    }
    uint8_t stop = 0xFC; stk_process_midi(s, &stop, 1, out, lens, 64);
    return ne;
}

static stk_t *fresh(void) {
    stk_t *s = (stk_t *)stk_create(NULL, "{\"state_path\":\"/dev/null\"}");
    stk_set_param(s, "clear", "on");
    stk_set_param(s, "hum_time", "0");
    stk_set_param(s, "hum_vel", "0");
    stk_set_param(s, "swing", "0");
    stk_set_param(s, "run", "on");
    return s;
}

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    move_midi_fx_init(&h);
    int bad = 0;
    ev evs[128];
    const int sub = BAR_CLOCKS / RHYTHM_STEPS;

    /* 1. Two half-bar chords under son 3-2 play ONE clave: 0 3 6 10 12. */
    {
        stk_t *s = fresh();
        stk_set_param(s, "steps", "2");
        stk_set_param(s, "sel", "1"); stk_set_param(s, "degree", "I"); stk_set_param(s, "rhythm", "son 3-2"); stk_set_param(s, "len", "4");
        stk_set_param(s, "sel", "2"); stk_set_param(s, "degree", "V"); stk_set_param(s, "rhythm", "son 3-2"); stk_set_param(s, "len", "4");
        int ne = run_bar(s, evs, 128);
        int want[5] = { 0, 3, 6, 10, 12 }, got[16], ng = 0;
        for (int i = 0; i < ne; i++) if (evs[i].on && ng < 16) got[ng++] = evs[i].clock / sub;
        int ok = ng == 5;
        for (int i = 0; ok && i < 5; i++) if (got[i] != want[i]) ok = 0;
        printf("  son 3-2 over two half-bar chords, hits at 16ths:");
        for (int i = 0; i < ng; i++) printf(" %d", got[i]);
        printf("  %s\n", ok ? "ok" : "FAIL: the figure must be anchored to the bar");
        if (!ok) bad = 1;
        stk_destroy(s);
    }

    /* 2. Tresillo: the "2" hit (16th 8) sounds for its own gap, not a 32nd.
     *    Gap to the next hit is 3 sixteenths; at gate 90 that is 16 clocks. */
    {
        stk_t *s = fresh();
        stk_set_param(s, "steps", "1"); stk_set_param(s, "sel", "1");
        stk_set_param(s, "degree", "I"); stk_set_param(s, "rhythm", "tresillo");
        stk_set_param(s, "gate", "90");
        int ne = run_bar(s, evs, 128);
        int on_at = 8 * sub, first_off = -1, cut = 0;
        for (int i = 0; i < ne; i++) {
            if (evs[i].clock == on_at && evs[i].off) cut = 1;        /* cut on arrival */
            if (evs[i].clock > on_at && evs[i].off && first_off < 0) first_off = evs[i].clock;
        }
        int held = first_off - on_at;
        int ok = !cut && held == 3 * sub * 90 / 100;
        printf("  tresillo: the hit at 16th 8 sounds %d clock(s)  %s\n", held,
               ok ? "ok" : "FAIL: a short hit must last its own gap");
        if (!ok) bad = 1;
        stk_destroy(s);
    }

    /* 3. Swing under "eighth": the off-beat eighths move, the beats do not. */
    {
        stk_t *s = fresh();
        stk_set_param(s, "steps", "1"); stk_set_param(s, "sel", "1");
        stk_set_param(s, "degree", "I"); stk_set_param(s, "rhythm", "eighth");
        stk_set_param(s, "swing", "50");
        int ne = run_bar(s, evs, 128);
        int ons[16], no = 0;
        for (int i = 0; i < ne; i++) if (evs[i].on && no < 16) ons[no++] = evs[i].clock;
        int lean = 2 * sub * 50 / 100;
        int ok = no == 8;
        for (int i = 0; ok && i < 8; i++)
            if (ons[i] != i * 2 * sub + ((i & 1) ? lean : 0)) ok = 0;
        printf("  eighth, swing 50: onsets");
        for (int i = 0; i < no; i++) printf(" %d", ons[i]);
        printf("  %s\n", ok ? "ok" : "FAIL: swing must lean the off-beat eighths");
        if (!ok) bad = 1;
        stk_destroy(s);
    }

    /* 4. Humanise varies across the hits of one bar. Under "quarter" with
     *    Human Time up, the four hits' leads must not all be identical. */
    {
        stk_t *s = fresh();
        stk_set_param(s, "steps", "1"); stk_set_param(s, "sel", "1");
        stk_set_param(s, "degree", "I"); stk_set_param(s, "rhythm", "quarter");
        stk_set_param(s, "hum_time", "80");
        s->pend_n = 0;
        arm_chord(s, 0, 0, 0);
        /* Each hit's voices are consecutive in pend[]; compare the SPREAD
         * (latest - earliest) of each hit, which is what humanise changes. */
        int n_hits = 0, spread[16], distinct = 0;
        for (int i = 0; i + 2 < s->pend_n && n_hits < 16; i += 3) {
            int lo = s->pend[i].at, hi = s->pend[i].at;
            for (int v = 1; v < 3; v++) { if (s->pend[i+v].at < lo) lo = s->pend[i+v].at; if (s->pend[i+v].at > hi) hi = s->pend[i+v].at; }
            spread[n_hits++] = hi - lo;
        }
        for (int i = 1; i < n_hits; i++) if (spread[i] != spread[0]) distinct = 1;
        printf("  quarter, human time 80: per-hit spreads");
        for (int i = 0; i < n_hits; i++) printf(" %d", spread[i]);
        printf("  %s\n", distinct ? "ok" : "FAIL: every hit carries the same deviation");
        if (!distinct) bad = 1;
        stk_destroy(s);
    }

    printf("\n%s\n", bad ? "FAIL" : "figures sit in the bar, hits last their own gap, swing and humanise reach every hit");
    return bad;
}
