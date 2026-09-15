/*
 * stacks/ui.js -- PHASE 0 SPIKE. Not the module's UI.
 *
 * This exists to prove the overtake path on the device before a line of
 * canvas.js is ported (docs/plans/2026-09-15-overtake-port-design.md, phase 0):
 *
 *   - the Tools menu lists Stacks and loads dsp.so as the overtake DSP;
 *   - Move's cable-0 clock reaches the DSP (the bar counter below moves);
 *   - a chord reaches a Move track through midi_inject_to_move;
 *   - Record and Play PASS THROUGH (button_passthrough) so Move records a lap
 *     while this screen is up.
 *
 * Four controls, nothing else:
 *   Jog click    load a library progression if the buffer is empty, then
 *                toggle the module's own loop (preview) -- hear it on the track
 *   Knob 1       track 1-4 (the channel the output addresses)
 *   Knob 2       the library progression
 *   Play         Move's transport, passed through; the DSP follows its clock
 *   Record       Move's, passed through
 *   Shift+Jog    Stamp: arm one lap for Move to record (press Record on Move)
 *   Back         suspend (the DSP keeps ticking); Shift+Back exits
 */

import { decodeDelta } from
    '/data/UserData/schwung/shared/input_filter.mjs';

const CC_SHIFT = 49, CC_BACK = 51, CC_JOG_CLICK = 3, CC_KNOB1 = 71, CC_KNOB2 = 72;

let shiftHeld = 0;
let dirty = true;
let lastPoll = 0;
let view = { hasChord: false, count: 0, running: 0, bar: 0, status: "idle", track: "1", preview: "off", prog: "" };

function log(m) { if (typeof shadow_log === "function") shadow_log("stacks ui: " + m); }
function setP(k, v) {
    log("set " + k + "=" + v + " (fn=" + typeof host_module_set_param + ")");
    if (typeof host_module_set_param === "function") host_module_set_param(k, String(v));
}
function getP(k) { return typeof host_module_get_param === "function" ? host_module_get_param(k) : null; }

/* `prog` is v13|count|sel|playing|stepUnits|mask|key|clipUnits|running|stepBeats|bpm|posUnits|grouping|... */
function poll() {
    const p = String(getP("prog") || "");
    const h = p.split("|");
    view.count   = parseInt(h[1], 10) || 0;
    /* chords are `NAME,ROOT,...;` after the 13 header fields; a rest is "REST" */
    view.hasChord = h.slice(13).join("|").split(";").some(function (c) {
        var name = c.split(",")[0]; return name && name !== "REST";
    });
    view.running = h[8] === "1" ? 1 : 0;
    const stepUnits = parseInt(h[4], 10) || 8;
    const pos = parseInt(h[11], 10) || 0;
    view.bar = Math.floor(pos / stepUnits) + 1;
    view.status  = String(getP("status") || "idle");
    view.track   = String(getP("track") || "1");
    view.preview = String(getP("preview") || "off");
    view.prog    = String(getP("progression") || "");
    dirty = true;
}

function draw() {
    clear_screen();
    print(0, 0, "STACKS  tool spike", 1);
    fill_rect(0, 9, 128, 1, 1);
    print(0, 14, "track " + view.track + "   " + view.prog.slice(0, 14), 1);
    print(0, 24, "chords " + view.count + "   loop " + view.preview, 1);
    print(0, 34, (view.running ? "RUNNING  bar " + view.bar : "stopped"), 1);
    print(0, 44, "status " + view.status, 1);
    print(0, 56, "Clk=loop K1=trk S+Clk=stamp", 1);
    dirty = false;
}

globalThis.init = function () {
    shiftHeld = 0;
    poll();
    draw();
};

globalThis.onResume = function () { dirty = true; };

globalThis.tick = function () {
    const now = Date.now();
    if (now - lastPoll >= 250) { lastPoll = now; poll(); }
    if (dirty) draw();
};

globalThis.onUnload = function () {
    /* Leave nothing sounding: the host synthesises no all-notes-off. */
    setP("preview", "off");
    setP("play", "off");
};

globalThis.onMidiMessageInternal = function (data) {
    if (!data) return;
    const status = data[0] | 0, d1 = data[1] | 0, d2 = data[2] | 0;
    if ((status & 0xF0) === 0xB0) log("cc " + d1 + "=" + d2);
    if ((status & 0xF0) !== 0xB0) return;
    if (d1 === CC_SHIFT) { shiftHeld = d2 > 0 ? 1 : 0; return; }
    if (d1 === CC_BACK) return;                 /* the host's: suspend / exit */

    if (d1 === CC_JOG_CLICK && d2 > 0) {
        if (shiftHeld) { setP("stamp", "1"); poll(); return; }
        /* Stacks opens on FOUR EMPTY BARS -- four rests, count 4 -- so
         * "count === 0" never held and the loop played silence. Empty means
         * no chord that is not a rest. */
        if (!view.hasChord) { setP("progression", "Axis"); setP("preview", "loop"); poll(); return; }
        setP("preview", view.preview === "loop" ? "off" : "loop");
        poll();
        return;
    }
    if (d1 === CC_KNOB1 || d1 === CC_KNOB2) {
        const raw = decodeDelta(d2);
        const step = raw > 0 ? 1 : (raw < 0 ? -1 : 0);
        if (!step) return;
        if (d1 === CC_KNOB1) setP("track", Math.max(1, Math.min(4, (parseInt(view.track, 10) || 1) + step)));
        else setP("progression", "~" + step);   /* an enum walks by a relative step */
        poll();
    }
};

globalThis.onMidiMessageExternal = function () {};
