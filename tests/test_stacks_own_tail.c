/*
 * THE MODULE MUST NOT BE DRIVEN BY ITS OWN OUTPUT.
 *
 * Stacks reads an incoming note as a chord SELECTOR by pitch class -- so a
 * pad, a keyboard and a clip can all drive it, which is the feature. Its own
 * output, though, goes to the track, and the track comes back: as Move's echo
 * while it plays, and again as a clip once you have stamped one.
 *
 * An Am7 contains a C. C is pitch class 0. Pitch class 0 is chord 1. So the
 * chords it played were selecting chords, and a selection is armed with HOLD,
 * whose voices never expire -- which is why the symptom on the device was not
 * "it jumps around" but "the chord never moves on, until I press stop".
 *
 * Captured from a real device while the transport ran:
 *
 *     playing=3 pos=95     playing=1 pos=96     playing=1 pos=100
 *     playing=0 pos=103    playing=2 pos=104    playing=1 pos=111
 *
 * -- the position advancing perfectly smoothly at the right tempo while the
 * chord index was dragged around by the module's own tail.
 *
 * This test builds that loop: every note the module emits is fed straight back
 * into it, as the track does. The progression must still walk 0,1,2,3,0.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m){ (void)m; } static float bp(void){ return 120.f; }

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    move_midi_fx_init(&h);
    int bad = 0;

    stk_t *s = (stk_t *)stk_create(NULL, NULL);
    stk_set_param(s, "clear", "on");
    stk_set_param(s, "hum_time", "0");
    /* Am7 / G / F / G -- the device's own progression, and the reason this
     * bites: those chords contain C and D, i.e. pitch classes 0 and 2. */
    const char *deg[4] = { "I", "VII", "VI", "VII" };
    for (int i = 0; i < 4; i++) {
        char b[8]; snprintf(b, sizeof b, "%d", i + 1);
        stk_set_param(s, "sel", b); stk_set_param(s, "degree", deg[i]);
    }
    stk_set_param(s, "run", "on");

    uint8_t out[64][3]; int lens[64];
    uint8_t start = 0xFA; stk_process_midi(s, &start, 1, out, lens, 64);

    int seen[8], n_seen = 0, last = -1;
    for (int c = 0; c < BAR_CLOCKS * 5; c++) {
        uint8_t clk = 0xF8;
        int n = stk_process_midi(s, &clk, 1, out, lens, 64);
        uint8_t echo[64][3]; int n_echo = 0;
        for (int i = 0; i < n; i++) { memcpy(echo[n_echo], out[i], 3); n_echo++; }
        n = stk_tick(s, 128, 44100, out, lens, 64);
        for (int i = 0; i < n && n_echo < 64; i++) { memcpy(echo[n_echo], out[i], 3); n_echo++; }
        /* THE LOOP: everything it just played comes straight back at it. */
        for (int i = 0; i < n_echo; i++) {
            uint8_t o2[8][3]; int l2[8];
            stk_process_midi(s, echo[i], 3, o2, l2, 8);
        }
        if (s->cur_step != last && s->cur_step >= 0) {
            last = s->cur_step;
            if (n_seen < 8) seen[n_seen++] = last;
        }
    }
    printf("  chord order under full echo:");
    for (int i = 0; i < n_seen; i++) printf(" %d", seen[i]);
    printf("\n");

    int want[5] = { 0, 1, 2, 3, 0 };
    int ok = n_seen >= 5;
    for (int i = 0; ok && i < 5; i++) ok = (seen[i] == want[i]);
    printf("  expected 0 1 2 3 0                       %s\n",
           ok ? "ok" : "FAIL: the module is being driven by its own output");
    if (!ok) bad = 1;

    printf("  held selection after the echo: %d        %s\n", s->held_pad,
           s->held_pad < 0 ? "ok" : "FAIL: its own notes latched a selection");
    if (s->held_pad >= 0) bad = 1;

    /* A note it is NOT sounding still selects -- the feature must survive. */
    stk_t *k = (stk_t *)stk_create(NULL, NULL);
    stk_set_param(k, "clear", "on");
    for (int i = 0; i < 4; i++) {
        char b[8]; snprintf(b, sizeof b, "%d", i + 1);
        stk_set_param(k, "sel", b); stk_set_param(k, "degree", "I");
    }
    uint8_t on[3] = { 0x90, 62, 100 };          /* D, pitch class 2 -> chord 3 */
    stk_process_midi(k, on, 3, out, lens, 64);
    printf("  an outside note still selects: held=%d   %s\n", k->held_pad,
           k->held_pad == 2 ? "ok" : "FAIL: the feature was thrown out with the loop");
    if (k->held_pad != 2) bad = 1;

    /* Two held together, released in the WRONG order, must not strand it. */
    uint8_t on2[3] = { 0x90, 60, 100 };          /* C, pitch class 0 */
    stk_process_midi(k, on2, 3, out, lens, 64);
    uint8_t off2[3] = { 0x80, 62, 0 };           /* release the FIRST one */
    stk_process_midi(k, off2, 3, out, lens, 64);
    uint8_t off1[3] = { 0x80, 60, 0 };
    stk_process_midi(k, off1, 3, out, lens, 64);
    printf("  released out of order: held=%d          %s\n", k->held_pad,
           k->held_pad < 0 ? "ok" : "FAIL: a selection outlived every note holding it");
    if (k->held_pad >= 0) bad = 1;

    stk_destroy(k); stk_destroy(s);
    printf(bad ? "\nFAIL\n" : "\nPASS\n");
    return bad;
}
