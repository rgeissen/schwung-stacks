/*
 * AUDITION LOOPS THE SELECTED CHORD WITH ITS RHYTHM, FOR AS LONG AS IT IS ON.
 *
 * A held pad (held_pad) sustains one strike; `play` runs onward. Neither let
 * a chord be heard WITH its figure while you stay on it -- asked for on
 * Push, where a chord pad is held to audition. Pinned:
 *   - only the selected chord sounds, and its figure repeats every pass
 *     (quarter = 4 hits a bar, every bar);
 *   - off silences it: nothing left sounding;
 *   - it edits nothing: the progression and the undo history are untouched;
 *   - parking ends it.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }
static int ons = 0, sounding[128], foreign = 0, chord_pc_mask = 0;
static int inject(const uint8_t *m, int len) {
    (void)len;
    int type = m[1] & 0xF0, note = m[2], vel = m[3];
    if (type == 0x90 && vel) {
        ons++; sounding[note]++;
        if (!(chord_pc_mask & (1 << (note % 12)))) foreign++;
    } else if (type == 0x80 || type == 0x90) {
        if (sounding[note] > 0) sounding[note]--;
    }
    return 0;
}
static void blocks(plugin_api_v2_t *a, void *i, int n) { int16_t b[256]; for (int k = 0; k < n; k++) a->render_block(i, b, 128); }
static int left(void) { int n = 0; for (int k = 0; k < 128; k++) n += sounding[k]; return n; }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp; h.midi_inject_to_move = inject;
    plugin_api_v2_t *a = move_plugin_init_v2(&h);
    void *i = a->create_instance(NULL, "{\"state_path\":\"/dev/null/x\"}");
    stk_t *st = (stk_t *)i;
    int bad = 0;
    a->set_param(i, "progression", "Axis");         /* A E F#m D, one bar each */
    a->set_param(i, "sel", "2");
    a->set_param(i, "rhythm", "quarter");
    a->set_param(i, "hum_time", "0");
    blocks(a, i, 20);
    char before[4096], after[4096];
    a->get_param(i, "prog", before, sizeof before);
    int depth = st->hist_depth, head = st->hist_head;
    chord_pc_mask = (1 << 4) | (1 << 8) | (1 << 11);   /* E major: E G# B */

    /* 120 BPM: a bar is 2 s = 689 blocks of 128 at 44.1 kHz. Three bars. */
    ons = 0;
    a->set_param(i, "audition", "on");
    blocks(a, i, 689 * 3);
    int hits = ons / 3;                              /* three voices a hit */
    if (hits < 11 || hits > 13) { printf("FAIL: %d hits in three bars of quarters, want 12\n", hits); bad = 1; }
    if (foreign) { printf("FAIL: %d notes outside the selected chord\n", foreign); bad = 1; }

    a->set_param(i, "audition", "off");
    blocks(a, i, 50);
    if (left()) { printf("FAIL: %d notes still sounding after off\n", left()); bad = 1; }

    a->get_param(i, "prog", after, sizeof after);
    /* `prog` carries the playhead; compare the chords, after field 13 */
    const char *cb = before, *ca = after;
    for (int f = 0; f < 13 && cb && ca; f++) { cb = strchr(cb, '|'); ca = strchr(ca, '|'); if (cb) cb++; if (ca) ca++; }
    if (!cb || !ca || strcmp(cb, ca) != 0) { puts("FAIL: the audition changed the progression"); bad = 1; }
    if (st->hist_depth != depth || st->hist_head != head) { puts("FAIL: the audition added undo steps"); bad = 1; }

    /* parking ends it */
    a->set_param(i, "audition", "on"); blocks(a, i, 200);
    a->set_param(i, "parked", "on"); blocks(a, i, 50);
    ons = 0; blocks(a, i, 689);
    if (ons) { printf("FAIL: parked, the audition still plays (%d ons)\n", ons); bad = 1; }
    if (left()) { printf("FAIL: parked, %d notes still sounding\n", left()); bad = 1; }

    a->destroy_instance(i);
    puts(bad ? "FAIL" : "PASS: the audition loops the selected chord with its rhythm, ends clean, edits nothing");
    return bad;
}
