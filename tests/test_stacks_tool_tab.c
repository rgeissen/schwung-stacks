/*
 * THE TOOL TAB READS `state` AS A FLAT OBJECT OF STRINGS, and `rui_poll` as
 * rev:on:tick:bpm[:devms]. This prints them for the .sh to check as JSON, and
 * pins in C what a parser cannot: the rev moves on a write and on a status
 * transition, and the stopped digest is byte-stable across ticks.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 98.f; }
int main(int argc, char **argv) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    plugin_api_v2_t *api = move_plugin_init_v2(&h);
    void *inst = api->create_instance(NULL, NULL);
    api->set_param(inst, "progression", "Axis");
    api->set_param(inst, "sel", "2");
    static char big[65536];
    if (argc > 1 && strcmp(argv[1], "--state") == 0) {
        int n = api->get_param(inst, "state", big, sizeof big);
        if (n < 0) return 2;
        fputs(big, stdout); return 0;
    }
    int bad = 0;
    char a[64], b[64];
    api->get_param(inst, "rui_poll", a, sizeof a);
    int16_t audio[256];
    for (int i = 0; i < 100; i++) api->render_block(inst, audio, 128);
    api->get_param(inst, "rui_poll", b, sizeof b);
    if (strcmp(a, b) != 0) { printf("FAIL: stopped digest moved without an edit: %s -> %s\n", a, b); bad = 1; }
    if (strchr(a, ':') == NULL || a[strlen(a) - 1] == ':') { printf("FAIL: digest shape %s\n", a); bad = 1; }
    unsigned rev0 = 0, rev1 = 0, rev2 = 0;
    sscanf(a, "%u:", &rev0);
    api->set_param(inst, "sel", "3");
    api->get_param(inst, "rui_poll", b, sizeof b); sscanf(b, "%u:", &rev1);
    if (rev1 <= rev0) { printf("FAIL: a write did not bump rev (%u -> %u)\n", rev0, rev1); bad = 1; }
    /* A status transition that no write caused: arm, then Move's Start. */
    api->set_param(inst, "stamp", "on");
    api->get_param(inst, "rui_poll", b, sizeof b); sscanf(b, "%u:", &rev1);
    uint8_t start = 0xFA; api->on_midi(inst, &start, 1, MOVE_MIDI_SOURCE_EXTERNAL);
    api->render_block(inst, audio, 128);
    api->get_param(inst, "rui_poll", b, sizeof b); sscanf(b, "%u:", &rev2);
    if (rev2 <= rev1) { printf("FAIL: the armed->working transition did not bump rev\n"); bad = 1; }
    if (b[strlen(b) - 1] == ':' || strncmp(strchr(b, ':'), ":1:", 3) != 0) {
        printf("FAIL: playing digest should read rev:1:tick:bpm:devms, got %s\n", b); bad = 1; }
    api->destroy_instance(inst);
    puts(bad ? "FAIL" : "PASS: rui_poll is stable when stopped and moves on writes and transitions");
    return bad;
}
