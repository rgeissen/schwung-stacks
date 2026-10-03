/*
 * READ TRACK FOLLOWS THE TRACK YOU ARE ON, and Read Clip reads the track the
 * knob names.
 *
 * Read Track existed because the host did not say which track Move had
 * selected. Schwung 1.6.0 does (move_info.h): the worker asks, tick applies.
 * Pinned here, each the failure it would otherwise be:
 *   - on a host without the export, nothing changes: the knob is the answer;
 *   - a selection on Move moves the knob, and the rev, so both surfaces show it;
 *   - it follows a CHANGE, not a level: a turn of the knob holds until you
 *     pick another track on Move, or the knob is a control that does nothing;
 *   - "unknown" (valid 0, a set loading) leaves the knob where it is;
 *   - Read Clip reads the track the knob NAMES. It read a cached copy that
 *     only the knob's own door wrote, so after a restore the knob said 2 and
 *     the clip came from track 1.
 *
 * The host's export is faked by defining it here: move_info_read() finds it
 * with dlsym(RTLD_DEFAULT) exactly as it finds the shim's on the device.
 * SETS_DIR is compiled in from the script, pointing at a fixture set.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }

/* --- the fake host export --- */
static int g_have = 0, g_valid = 1, g_sel = -1;
int schwung_move_info(move_info_t *out, size_t cap);
int schwung_move_info(move_info_t *out, size_t cap) {
    if (!g_have || cap < sizeof *out) return 0;
    memset(out, 0, cap);
    out->size = sizeof *out;
    out->version = MOVE_INFO_VERSION;
    out->valid = (uint8_t)g_valid;
    out->selected_track = (int8_t)g_sel;
    return 1;
}

/* One set, four tracks, each playing one chord: C, D, E, F major. */
static void write_set(void) {
    char path[1024];
    mkdir(SETS_DIR, 0755);
    snprintf(path, sizeof path, "%s/u", SETS_DIR); mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/u/set", SETS_DIR); mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/u/set/Song.abl", SETS_DIR);
    FILE *f = fopen(path, "w");
    fprintf(f, "{\"rootNote\":0,\"scale\":\"Major\",\"tracks\":[");
    const int roots[4] = { 60, 62, 64, 65 };
    for (int t = 0; t < 4; t++) {
        fprintf(f, "%s{\"clipSlots\":[{\"clip\":{\"isPlaying\":true,\"notes\":[", t ? "," : "");
        for (int v = 0; v < 3; v++)
            fprintf(f, "%s{\"noteNumber\":%d,\"startTime\":0,\"duration\":4,\"velocity\":100}",
                    v ? "," : "", roots[t] + (v == 0 ? 0 : v == 1 ? 4 : 7));
        fprintf(f, "]}}]}");
    }
    fprintf(f, "]}");
    fclose(f);
}

static void pump(plugin_api_v2_t *api, void *inst, int ms) {
    int16_t audio[256];
    for (int i = 0; i < ms; i += 3) { api->render_block(inst, audio, 128); usleep(3000); }
}
static int knob(plugin_api_v2_t *api, void *inst) {
    char v[16]; if (api->get_param(inst, "read_track", v, sizeof v) < 0) return -1;
    return atoi(v);
}
static unsigned rev(void *inst) {
    return atomic_load(&((stk_t *)inst)->rev);
}
/* Read Clip, then the first chord's root pitch class -- which track it came from. */
static int read_root(plugin_api_v2_t *api, void *inst) {
    stk_t *st = (stk_t *)inst;
    int before = st->staging_applied;          /* a NEW parse, not the last one */
    api->set_param(inst, "read", "on");
    for (int i = 0; i < 100 && st->staging_applied == before; i++) pump(api, inst, 20);
    if (st->staging_applied == before || atomic_load(&st->status) != 2) return -1;
    return st->prog.ch[0].root % 12;
}

static int bad = 0;
static void check(const char *what, int got, int want) {
    int ok = got == want;
    printf("  %-58s %-4d %s\n", what, got, ok ? "ok" : "FAIL");
    if (!ok) { printf("      wanted %d\n", want); bad = 1; }
}

int main(int argc, char **argv) {
    if (argc < 2) { puts("usage: follow <scratch dir>"); return 2; }
    write_set();
    char cfg[400]; snprintf(cfg, sizeof cfg, "{\"state_path\":\"%s/state.json\"}", argv[1]);
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    plugin_api_v2_t *api = move_plugin_init_v2(&h);

    /* 1. A host without the export: the knob is the answer, as before. */
    void *a = api->create_instance(NULL, cfg);
    api->set_param(a, "read_track", "2");
    pump(api, a, 300);
    check("no host info: the knob holds", knob(api, a), 2);
    check("no host info: Read Clip reads the knob's track (D)", read_root(api, a), 2);

    /* 2. Move selects track 3: the knob follows, and the rev says so. */
    g_have = 1; g_valid = 1; g_sel = 2;
    unsigned r0 = rev(a);
    pump(api, a, 300);
    check("Move selects track 3: Read Track follows", knob(api, a), 3);
    check("  ...and the rev moved, so the panel re-reads", rev(a) != r0, 1);
    check("  ...and Read Clip reads track 3 (E)", read_root(api, a), 4);

    /* 3. A turn of the knob holds while the selection does not move. */
    api->set_param(a, "read_track", "1");
    pump(api, a, 300);
    check("knob turned to 1, selection unchanged: it holds", knob(api, a), 1);
    check("  ...and Read Clip reads track 1 (C)", read_root(api, a), 0);

    /* 4. The next selection takes it again. */
    g_sel = 3;
    pump(api, a, 300);
    check("Move selects track 4: Read Track follows", knob(api, a), 4);

    /* 5. Unknown -- a set loading -- leaves it where it is. */
    g_valid = 0;
    pump(api, a, 300);
    check("selection unknown (valid 0): the knob stays", knob(api, a), 4);
    g_valid = 1; g_sel = -1;
    pump(api, a, 300);
    check("selection unknown (-1): the knob stays", knob(api, a), 4);

    /* 6. A RESTORE names the track Read Clip reads. Take the state with the
     *    knob on 2, hand it to a fresh instance, and read. */
    g_have = 0;
    api->set_param(a, "read_track", "2");
    static char blob[65536];
    if (api->get_param(a, "state", blob, sizeof blob) < 0) blob[0] = 0;
    api->destroy_instance(a);
    char cfg2[400]; snprintf(cfg2, sizeof cfg2, "{\"state_path\":\"%s/state2.json\"}", argv[1]);
    void *b = api->create_instance(NULL, cfg2);
    api->set_param(b, "state", blob);
    pump(api, b, 50);
    check("restored: the knob says 2", knob(api, b), 2);
    check("restored: Read Clip reads track 2 (D), not track 1", read_root(api, b), 2);
    api->destroy_instance(b);

    printf("\n%s\n", bad ? "FAIL" : "Read Track follows Move's selection, and Read Clip reads it");
    return bad;
}
