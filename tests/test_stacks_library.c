/*
 * EVERY LIBRARY ENTRY APPLIES. A preset is a table row, and a row can be
 * wrong in ways the compiler never sees: a quality string no shape has, a
 * step count that does not match its steps, a total longer than the sixteen
 * bars a clip can hold. Applied one by one through the same param the knob
 * writes, each must land as exactly its chords -- no rest, every voice
 * resolved to a pitch, the count and the bars it declared.
 */
#include <stdio.h>
#include <string.h>
#include "../src/dsp/stacks.c"
static void lg(const char *m) { (void)m; }
static float bp(void) { return 120.f; }
int main(void) {
    host_api_v1_t h; memset(&h, 0, sizeof h); h.log = lg; h.get_bpm = bp;
    plugin_api_v2_t *api = move_plugin_init_v2(&h);
    void *inst = api->create_instance(NULL, "{\"state_path\":\"/dev/null/nope\"}");
    int bad = 0, n_ok = 0;
    static char prog[8192];
    for (int i = 1; i < NUM_PRESETS; i++) {           /* 0 is "none" */
        const stk_preset_t *p = &PRESETS[i];
        api->set_param(inst, "progression", p->name);
        char v[64]; api->get_param(inst, "progression", v, sizeof v);
        if (strcmp(v, p->name) != 0) { printf("FAIL: %s did not select (reads %s)\n", p->name, v); bad = 1; continue; }
        api->get_param(inst, "prog", prog, sizeof prog);
        int count = 0; sscanf(prog, "v13|%d|", &count);
        if (count != p->n) { printf("FAIL: %s declares %d steps, prog has %d\n", p->name, p->n, count); bad = 1; }
        if (strstr(prog, "REST")) { printf("FAIL: %s applied with a rest in it\n", p->name); bad = 1; }
        /* every chord carries notes: "NAME,ROOT,inv,len,off,vel,mute,rh,n:v.n:v" */
        int chords = 0, empty = 0, eighths = 0;
        const char *body = prog; for (int k = 0; k < 13 && body; k++) body = strchr(body, '|') + 1;
        char copy[8192]; snprintf(copy, sizeof copy, "%s", body ? body : "");
        for (char *tok = strtok(copy, ";"); tok; tok = strtok(NULL, ";")) {
            chords++;
            int f = 0, len = 0; const char *q = tok;
            for (const char *c = tok; *c; c++) if (*c == ',') { f++; if (f == 3) len = atoi(c + 1); if (f == 8) q = c + 1; }
            eighths += len;
            if (f < 8 || !*q || !strchr(q, ':')) empty++;
        }
        if (empty) { printf("FAIL: %s has %d chord(s) with no notes\n", p->name, empty); bad = 1; }
        if (eighths > 16 * UNITS_PER_BAR) { printf("FAIL: %s is %d eighths, longer than a 16-bar clip\n", p->name, eighths); bad = 1; }
        int want = 0; for (int k = 0; k < p->n; k++) want += p->s[k].len;
        if (eighths != want) { printf("FAIL: %s plays %d eighths, the table says %d\n", p->name, eighths, want); bad = 1; }
        n_ok++;
    }
    api->destroy_instance(inst);
    printf("  %d of %d presets applied\n", n_ok, NUM_PRESETS - 1);
    puts(bad ? "FAIL" : "PASS: every library entry applies as its chords, with pitches, at its length");
    return bad;
}
