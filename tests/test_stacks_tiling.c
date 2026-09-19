/*
 * A PHRASE SHORTER THAN THE CLIP PLAYS EVERY TILE, NOT JUST THE FIRST.
 *
 * "The loop is the clip, and the progression tiles inside it" was true of the
 * POSITION -- `pos_u %= total_u` -- and false of the two facts derived from it.
 * `want_id` named (pass, chord) and not the tile, so on the second tile of a
 * lap the chord already armed was "still that one" and never re-armed: a
 * one-chord progression in a four-bar clip sounded ONCE per lap. And
 * `onset_pulse` was the first tile's, so when a chord did re-arm on a later
 * tile its `elapsed` began a whole phrase late and every hit of its rhythm
 * figure fired in one block, as a single stacked chord.
 *
 * Reachable the moment Clip Bars is turned past the music -- the one control
 * whose job is to make the phrase repeat. Measured 2026-09-19.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m){ (void)m; } static float bp(void){ return 120.f; }

/* Drive Start + `clocks` clocks; record each block that carried a note-on
 * and how many note-ons it carried. */
static int onsets(stk_t *s, int clocks, int *at, int *cnt, int max) {
    uint8_t out[64][3]; int lens[64];
    uint8_t start = 0xFA;
    stk_process_midi(s, &start, 1, out, lens, 64);
    stk_tick(s, 128, 44100, out, lens, 64);
    int n_at = 0;
    for (int c = 0; c < clocks; c++) {
        uint8_t clk = 0xF8; int got = 0;
        int n = stk_process_midi(s, &clk, 1, out, lens, 64);
        for (int i = 0; i < n; i++) if ((out[i][0] & 0xF0) == 0x90 && out[i][2] > 0) got++;
        n = stk_tick(s, 128, 44100, out, lens, 64);
        for (int i = 0; i < n; i++) if ((out[i][0] & 0xF0) == 0x90 && out[i][2] > 0) got++;
        if (got && n_at < max) { at[n_at] = c; cnt[n_at] = got; n_at++; }
    }
    return n_at;
}

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    move_midi_fx_init(&h);
    int bad = 0;
    int at[128], cnt[128];

    /* 1. One one-bar chord, Clip Bars 4: an onset on EVERY bar line. */
    {
        stk_t *s = (stk_t *)stk_create(NULL, "{\"state_path\":\"/dev/null\"}");
        stk_set_param(s, "clear", "on");
        stk_set_param(s, "steps", "1");
        stk_set_param(s, "sel", "1");
        stk_set_param(s, "degree", "I");
        stk_set_param(s, "rhythm", "hold");
        stk_set_param(s, "hum_time", "0");
        stk_set_param(s, "bars", "4");
        stk_set_param(s, "run", "on");
        int n = onsets(s, BAR_CLOCKS * 4, at, cnt, 128);
        int ok = n == 4;
        for (int i = 0; ok && i < 4; i++) if (at[i] != i * BAR_CLOCKS) ok = 0;
        printf("  one chord in a 4-bar clip: %d onset(s) at", n);
        for (int i = 0; i < n; i++) printf(" %d", at[i]);
        printf("  %s\n", ok ? "ok" : "FAIL: expected one on every bar line");
        if (!ok) bad = 1;
        stk_destroy(s);
    }

    /* 2. Two one-bar chords under "quarter", Clip Bars 4: sixteen hits, one
     *    per beat, each of three voices -- never twelve notes in one block. */
    {
        stk_t *s = (stk_t *)stk_create(NULL, "{\"state_path\":\"/dev/null\"}");
        stk_set_param(s, "clear", "on");
        stk_set_param(s, "steps", "2");
        stk_set_param(s, "sel", "1"); stk_set_param(s, "degree", "I"); stk_set_param(s, "rhythm", "quarter");
        stk_set_param(s, "sel", "2"); stk_set_param(s, "degree", "V"); stk_set_param(s, "rhythm", "quarter");
        stk_set_param(s, "hum_time", "0");
        stk_set_param(s, "swing", "0");
        stk_set_param(s, "bars", "4");
        stk_set_param(s, "run", "on");
        int n = onsets(s, BAR_CLOCKS * 4, at, cnt, 128);
        int ok = n == 16;
        for (int i = 0; ok && i < 16; i++)
            if (at[i] != i * CLOCKS_PER_QUARTER || cnt[i] != 3) ok = 0;
        printf("  2 chords, quarter, 4-bar clip: %d hit block(s)", n);
        int stacked = 0;
        for (int i = 0; i < n; i++) if (cnt[i] > 3) stacked = cnt[i];
        if (stacked) printf(", one carried %d note-ons", stacked);
        printf("  %s\n", ok ? "ok" : "FAIL: the second tile must play its figure, not stack it");
        if (!ok) bad = 1;
        stk_destroy(s);
    }

    printf("\n%s\n", bad ? "FAIL" : "a phrase shorter than the clip plays on every tile");
    return bad;
}
