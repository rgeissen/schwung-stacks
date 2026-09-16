/*
 * A TOOL HAS NO SLOT IN THE SET, so Stacks keeps its own file.
 *
 * Three things pinned, each the failure it would otherwise be:
 *   - the quiet-time save: an edit lands on disk without any exit, so a
 *     crash or a battery death after a session does not lose the session;
 *   - the destroy-time save: what the quiet timer had not written yet is
 *     written on unload;
 *   - the restore: a fresh instance opens on what the last one had -- the
 *     progression, the selection AND the pattern slots -- and never on the
 *     four empty bars.
 * The audio thread never touches the disk: tick serialises into a buffer
 * on request and the worker writes it; this drives render_block to stand in
 * for the audio thread, as the device does.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }

static void pump(plugin_api_v2_t *api, void *inst, int ms) {
    int16_t audio[256];
    for (int i = 0; i < ms; i += 3) { api->render_block(inst, audio, 128); usleep(3000); }
}
static int get(plugin_api_v2_t *api, void *inst, const char *k, char *b, int n) {
    int r = api->get_param(inst, k, b, n); if (r < 0) b[0] = 0; return r;
}

int main(int argc, char **argv) {
    if (argc < 2) { puts("usage: persist <state path>"); return 2; }
    char cfg[400]; snprintf(cfg, sizeof cfg, "{\"state_path\":\"%s\"}", argv[1]);
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    plugin_api_v2_t *api = move_plugin_init_v2(&h);
    int bad = 0; char v[256];

    /* 1. edit, then wait out the quiet time: the file appears with no exit */
    void *a = api->create_instance(NULL, cfg);
    api->set_param(a, "progression", "Doo-wop");
    api->set_param(a, "sel", "3");
    api->set_param(a, "pat_store", "5");          /* the progression into slot 5 */
    api->set_param(a, "key", "F#");
    pump(api, a, STK_SAVE_QUIET_MS + 900);
    if (access(argv[1], R_OK) != 0) { puts("FAIL: no state file after the quiet time"); bad = 1; }
    long before = 0; char *f1 = slurp(argv[1], &before);
    if (!f1 || before < 100) { puts("FAIL: state file is empty"); bad = 1; }
    if (f1 && !strstr(f1, "\"s\":\"v6|")) { puts("FAIL: state file does not carry the blob"); bad = 1; }
    free(f1);

    /* 2. an edit inside the quiet window is not yet on disk; destroy writes it */
    api->set_param(a, "sel", "2");
    pump(api, a, 200);
    api->destroy_instance(a);
    long after = 0; char *f2 = slurp(argv[1], &after);
    if (!f2 || !strstr(f2, "\"sel\":\"2\"")) { puts("FAIL: destroy did not write the last edit"); bad = 1; }
    free(f2);

    /* 3. a fresh instance restores it all */
    void *b = api->create_instance(NULL, cfg);
    get(api, b, "progression", v, sizeof v);
    if (strcmp(v, "Doo-wop") != 0) { printf("FAIL: progression restored as %s\n", v); bad = 1; }
    get(api, b, "sel", v, sizeof v);
    if (strcmp(v, "2") != 0) { printf("FAIL: sel restored as %s\n", v); bad = 1; }
    get(api, b, "key", v, sizeof v);
    if (strcmp(v, "F#") != 0) { printf("FAIL: key restored as %s\n", v); bad = 1; }
    get(api, b, "patterns", v, sizeof v);          /* "<mask hex>|<current>" */
    unsigned mask = 0; sscanf(v, "%x|", &mask);
    if (!(mask & (1u << 4))) { printf("FAIL: pattern slot 5 not restored (patterns=%s)\n", v); bad = 1; }
    get(api, b, "prog", v, sizeof v);
    if (strstr(v, "REST") && !strstr(v, "maj") && !strstr(v, "min")) { puts("FAIL: restored to empty bars"); bad = 1; }
    api->destroy_instance(b);

    /* 4. a file this build cannot read is ignored, not half-applied */
    FILE *g = fopen(argv[1], "wb"); fputs("{\"s\":\"v99|garbage\"}", g); fclose(g);
    void *c = api->create_instance(NULL, cfg);
    get(api, c, "sel", v, sizeof v);
    if (strcmp(v, "1") != 0) { printf("FAIL: an unknown blob changed sel to %s\n", v); bad = 1; }
    api->destroy_instance(c);

    puts(bad ? "FAIL" : "PASS: state is saved on quiet and on unload, and a fresh instance restores it");
    return bad;
}
