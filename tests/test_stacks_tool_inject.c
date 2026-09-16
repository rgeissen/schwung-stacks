/*
 * THE TOOL ENTRY INJECTS WHAT THE MIDI FX RETURNED, and nothing else.
 *
 * As an overtake tool, Stacks has no chain to hand notes to: on_midi and
 * render_block run the same state machine and push each output message into
 * Move through host->midi_inject_to_move. This drives one lap through the
 * v2 surface with a fake injector and pins the packet contract from
 * docs/ADDRESSING_MOVE_SYNTHS.md:
 *
 *   - cable 2, always -- cable 0 is Move's pad protocol and a pitched note
 *     there reaches no track;
 *   - CIN equals the status nibble (note-off 0x8, note-on 0x9);
 *   - the channel is `track` - 1, on every packet;
 *   - a cable-0 note tagged INTERNAL is a PAD ID, not a pitch, and selects
 *     nothing;
 *   - after a stop, every note-on has had its note-off: the host synthesises
 *     no all-notes-off, so a voice left on rings on Move after exit.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"

static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }

static uint8_t pk[4096][4];
static int npk = 0;
static int inject(const uint8_t *msg, int len) {
    if (len == 4 && npk < 4096) { memcpy(pk[npk], msg, 4); npk++; }
    return 0;
}

int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h);
    h.log = lg; h.get_bpm = bp; h.midi_inject_to_move = inject;
    plugin_api_v2_t *api = move_plugin_init_v2(&h);
    int bad = 0;

    void *inst = api->create_instance(NULL, NULL);
    api->set_param(inst, "clear", "on");
    api->set_param(inst, "hum_time", "0");
    api->set_param(inst, "track", "3");
    const char *deg[4] = { "I", "IV", "V", "I" };
    for (int i = 0; i < 4; i++) {
        char b[8]; snprintf(b, sizeof b, "%d", i + 1);
        api->set_param(inst, "sel", b); api->set_param(inst, "degree", deg[i]);
    }
    char v[16];
    api->get_param(inst, "track", v, sizeof v);
    if (strcmp(v, "3") != 0) { printf("FAIL: track reads back %s\n", v); bad = 1; }
    api->get_param(inst, "module_id", v, sizeof v);
    if (strcmp(v, "stacks") != 0) { printf("FAIL: module_id %s\n", v); bad = 1; }

    /* A pad press on the audio thread: pad 1 is note 68 on cable 0. As a
     * pitch that is G#, chord 9 by pitch class -- it must select nothing. */
    npk = 0;
    uint8_t pad[3] = { 0x90, 68, 100 };
    api->on_midi(inst, pad, 3, MOVE_MIDI_SOURCE_INTERNAL);
    for (int i = 0; i < 20; i++) api->render_block(inst, NULL, 128);
    if (npk != 0) { printf("FAIL: a pad id sounded %d packets\n", npk); bad = 1; }

    /* Move's transport: Start, then a lap and a half of clocks. */
    api->set_param(inst, "run", "on");
    int16_t audio[256];
    uint8_t start = 0xFA;
    api->on_midi(inst, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    for (int c = 0; c < BAR_CLOCKS * 6; c++) {
        uint8_t clk = 0xF8;
        api->on_midi(inst, &clk, 1, MOVE_MIDI_SOURCE_EXTERNAL);
        api->render_block(inst, audio, 128);
        if (audio[0] != 0 || audio[255] != 0) { printf("FAIL: audio not silent\n"); bad = 1; break; }
    }
    uint8_t stop = 0xFC;
    api->on_midi(inst, &stop, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    api->set_param(inst, "run", "off");
    for (int i = 0; i < 50; i++) api->render_block(inst, audio, 128);

    int ons = 0, offs = 0, sounding[128] = {0};
    for (int i = 0; i < npk; i++) {
        uint8_t cable = pk[i][0] >> 4, cin = pk[i][0] & 0x0F;
        uint8_t type = pk[i][1] & 0xF0, ch = pk[i][1] & 0x0F;
        if (cable != 2) { printf("FAIL: pkt %d on cable %d\n", i, cable); bad = 1; }
        if (cin != (type >> 4)) { printf("FAIL: pkt %d CIN %x for status %02x\n", i, cin, type); bad = 1; }
        if (ch != 2) { printf("FAIL: pkt %d on channel %d, track 3 is channel 2\n", i, ch + 1); bad = 1; }
        if (type == 0x90 && pk[i][3] > 0) { ons++; sounding[pk[i][2]]++; }
        else if (type == 0x80 || type == 0x90) { offs++; sounding[pk[i][2]]--; }
    }
    int left = 0;
    for (int n = 0; n < 128; n++) if (sounding[n] > 0) left++;
    printf("  %d packets: %d note-ons, %d note-offs, %d still sounding\n", npk, ons, offs, left);
    if (ons < 4) { printf("FAIL: a lap of four chords injected only %d note-ons\n", ons); bad = 1; }
    if (left) { printf("FAIL: %d notes left sounding after stop\n", left); bad = 1; }

    /*
     * THE ECHO LOOP. Move echoes every injected packet back on MIDI_OUT and
     * the shim hands MIDI_OUT to this entry; a message the module merely
     * PASSED THROUGH therefore comes straight back, is passed through again,
     * and loops once per block for the session. Press Stop and Move sends
     * All Sound Off (CC 120) -- on the device that silenced every later
     * chord within a block. Feed the CC and echo whatever comes out: nothing
     * may come out, and after the echo settles the audition must still sound.
     */
    npk = 0;
    uint8_t cc120[3] = { 0xB0, 0x78, 0x00 };
    api->on_midi(inst, cc120, 3, MOVE_MIDI_SOURCE_EXTERNAL);
    for (int round = 0; round < 8 && npk > 0; round++) {   /* Move's echo */
        int m = npk; npk = 0;
        for (int i = 0; i < m; i++) { uint8_t e[3] = { pk[i][1], pk[i][2], pk[i][3] }; api->on_midi(inst, e, 3, MOVE_MIDI_SOURCE_EXTERNAL); }
    }
    if (npk != 0) { printf("FAIL: a CC fed to the tool came back out (%d packets) -- the echo loop\n", npk); bad = 1; }
    uint8_t sustain[3] = { 0xB0, 0x40, 0x7F };
    npk = 0; api->on_midi(inst, sustain, 3, MOVE_MIDI_SOURCE_EXTERNAL);
    if (npk != 0) { printf("FAIL: a sustain CC was re-injected\n"); bad = 1; }
    npk = 0; api->set_param(inst, "play", "on");
    for (int i = 0; i < 40; i++) api->render_block(inst, audio, 128);
    api->set_param(inst, "play", "off");
    for (int i = 0; i < 40; i++) api->render_block(inst, audio, 128);
    int ons2 = 0; for (int i = 0; i < npk; i++) if ((pk[i][1] & 0xF0) == 0x90 && pk[i][3]) ons2++;
    if (ons2 < 3) { printf("FAIL: after Move's Stop-time CC the audition is silent (%d ons)\n", ons2); bad = 1; }

    api->destroy_instance(inst);
    puts(bad ? "FAIL" : "PASS: the tool entry injects the lap on cable 2, on the track's channel, leaves nothing ringing, and never re-injects what it was handed");
    return bad;
}
