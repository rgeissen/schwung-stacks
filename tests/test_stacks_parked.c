/*
 * PARKED, STACKS PLAYS ONLY FOR A TAKE IT ARMED.
 *
 * Move plays an injected note on whichever track is SELECTED, so a parked
 * tool that goes on following the transport lands on every track you touch
 * -- "the other track plays the stamped notes too". Pinned:
 *   - parked with Run on and the transport running: silence;
 *   - parked with the preview loop on: silence;
 *   - parked with a take armed: the lap still plays on Move's Start, exactly
 *     one lap, and Run stays OFF at the transport stop while parked;
 *   - resume: Run and the loop come back, including a Run the take parked.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }
static int ons = 0;
static int inject(const uint8_t *m, int len) { (void)len; if ((m[1] & 0xF0) == 0x90 && m[3]) ons++; return 0; }
static void blocks(plugin_api_v2_t *a, void *i, int n) { int16_t b[256]; for (int k = 0; k < n; k++) a->render_block(i, b, 128); }
static void clocks(plugin_api_v2_t *a, void *i, int n) { for (int k = 0; k < n; k++) { uint8_t c = 0xF8; a->on_midi(i, &c, 1, MOVE_MIDI_SOURCE_EXTERNAL); blocks(a, i, 7); } }
static int get_on(plugin_api_v2_t *a, void *i, const char *k) { char v[16]; a->get_param(i, k, v, sizeof v); return strcmp(v, "on") == 0; }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp; h.midi_inject_to_move = inject;
    plugin_api_v2_t *a = move_plugin_init_v2(&h);
    void *i = a->create_instance(NULL, "{\"state_path\":\"/dev/null/x\"}");
    a->set_param(i, "progression", "Axis");
    int bad = 0;
    uint8_t start = 0xFA, stop = 0xFC;

    /* 1. following the transport, then parked: silence */
    a->on_midi(i, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    ons = 0; clocks(a, i, 96); if (ons < 1) { puts("FAIL: not playing before park"); bad = 1; }
    a->set_param(i, "parked", "on");
    blocks(a, i, 10);
    ons = 0; clocks(a, i, 96 * 2);
    if (ons != 0) { printf("FAIL: parked, still following the transport (%d ons)\n", ons); bad = 1; }
    if (get_on(a, i, "run")) { puts("FAIL: run still on while parked"); bad = 1; }
    a->on_midi(i, &stop, 1, MOVE_MIDI_SOURCE_EXTERNAL); blocks(a, i, 10);

    /* 2. resume: Run is back and it follows again */
    a->set_param(i, "parked", "off");
    if (!get_on(a, i, "run")) { puts("FAIL: resume did not give Run back"); bad = 1; }
    a->on_midi(i, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    ons = 0; clocks(a, i, 96); if (ons < 1) { puts("FAIL: not playing after resume"); bad = 1; }
    a->on_midi(i, &stop, 1, MOVE_MIDI_SOURCE_EXTERNAL); blocks(a, i, 10);

    /* 3. the loop, parked: silence; resumed: back */
    a->set_param(i, "preview", "loop");
    ons = 0; blocks(a, i, 1500); if (ons < 1) { puts("FAIL: loop silent before park"); bad = 1; }
    a->set_param(i, "parked", "on"); blocks(a, i, 100);
    ons = 0; blocks(a, i, 1500);
    if (ons != 0) { printf("FAIL: parked, the loop still plays (%d ons)\n", ons); bad = 1; }
    a->set_param(i, "parked", "off");
    ons = 0; blocks(a, i, 1500); if (ons < 1) { puts("FAIL: loop not back on resume"); bad = 1; }
    a->set_param(i, "preview", "off"); blocks(a, i, 50);

    /* 4. the take: arm, park, Move's Start -- the lap plays; stop leaves Run off; resume restores */
    a->set_param(i, "stamp", "on");
    a->set_param(i, "parked", "on"); blocks(a, i, 10);
    a->on_midi(i, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    ons = 0; clocks(a, i, 96 * 4);                 /* one four-bar lap */
    int lap = ons;
    if (lap < 4) { printf("FAIL: parked take did not play (%d ons)\n", lap); bad = 1; }
    ons = 0; clocks(a, i, 96 * 4);                 /* a second lap must NOT play */
    if (ons != 0) { printf("FAIL: the take did not stop after one lap (%d ons)\n", ons); bad = 1; }
    a->on_midi(i, &stop, 1, MOVE_MIDI_SOURCE_EXTERNAL); blocks(a, i, 10);
    if (get_on(a, i, "run")) { puts("FAIL: Run came back at the stop while still parked"); bad = 1; }
    a->on_midi(i, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    ons = 0; clocks(a, i, 96 * 2);
    if (ons != 0) { printf("FAIL: parked after the take, Move's next Play doubled the clip (%d ons)\n", ons); bad = 1; }
    a->on_midi(i, &stop, 1, MOVE_MIDI_SOURCE_EXTERNAL); blocks(a, i, 10);
    a->set_param(i, "parked", "off");
    if (!get_on(a, i, "run")) { puts("FAIL: resume after the take did not give Run back"); bad = 1; }

    a->destroy_instance(i);
    puts(bad ? "FAIL" : "PASS: parked, Stacks plays only for a take it armed; resume gives Run and the loop back");
    return bad;
}
