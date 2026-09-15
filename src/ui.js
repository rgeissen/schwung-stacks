/*
 * stacks/ui.js -- the overtake tool's UI. canvas.js, under the host's
 * overtake lifecycle.
 *
 * canvas.js is the whole surface -- the roll, the bank row, the pads, the
 * rings, the gestures -- written against a `ctx` the chain's canvas runtime
 * used to hand it. This file IS that ctx, built from the overtake draw
 * globals, plus the lifecycle the runtime used to run:
 *
 *   host                       canvas_overlay
 *   init()                     onOpen(ctx)     once per load, after the
 *                                              host's progressive LED clear
 *   tick()                     tick(ctx) every frame, draw(ctx) at DRAW_MS,
 *                              onPoll(ctx) at POLL_MS -- the reads the canvas
 *                              runtime stripped from draw are ours to make
 *   onMidiMessageInternal(d)   onMidi(ctx, {data: d})
 *   onResume()                 onResume(ctx)   the host cleared the hardware
 *   onUnload()                 onClose(ctx), then leave nothing sounding
 *
 * PARKED. Back suspends this tool (suspend_keeps_js): the DSP keeps ticking
 * -- an armed take goes on waiting for Move's Start -- and tick() keeps being
 * called with the draw and LED globals swapped for no-ops and
 * globalThis.overtakeParked raised. Nothing is drawn or read while parked:
 * the param mailbox does not answer a parked module (phase 0 measured the
 * give-ups), and a picture nobody can see is not worth the reads.
 *
 * The Back button itself never reaches us -- the host takes it for suspend
 * (Shift+Back for exit, Shift+Vol+Click as the escape). Everything else on
 * the surface is ours, Shift included: it arrives as CC 49 and canvas.js
 * reads it from the wire.
 */

import "/data/UserData/schwung/modules/tools/stacks/canvas.js";

const DRAW_MS = 33;      /* ~30 fps; a full redraw is ~1.7 ms of a 2.9 ms tick */
const POLL_MS = 250;     /* the canvas runtime's CANVAS_POLL_MS, kept */

const ov = globalThis.canvas_overlay;

/*
 * The ctx. Every method reaches for the GLOBAL by name at call time rather
 * than capturing it, because the host swaps those globals for no-ops while we
 * are parked and swaps them back on resume -- a captured reference would
 * draw on a screen that is not ours.
 */
const ctx = {
    width: 128,
    height: 64,
    clear() { clear_screen(); },
    setPixel(x, y, v) { set_pixel(x, y, v === undefined ? 1 : v); },
    fillRect(x, y, w, h, v) { fill_rect(x, y, w, h, v === undefined ? 1 : v); },
    drawRect(x, y, w, h, v) { draw_rect(x, y, w, h, v === undefined ? 1 : v); },
    drawLine(x0, y0, x1, y1, v) { draw_line(x0, y0, x1, y1, v === undefined ? 1 : v); },
    print(x, y, s, size) { print(x, y, String(s), size || 1); },
    now() { return Date.now(); },
    random() { return Math.random(); },
    getParam(k) {
        return typeof host_module_get_param === "function" ? host_module_get_param(k) : null;
    },
    setParam(k, v) {
        if (typeof host_module_set_param === "function") host_module_set_param(k, String(v));
    },
};

let lastDraw = 0;
let lastPoll = 0;
let open = false;

function parked() { return globalThis.overtakeParked === true; }

globalThis.init = function () {
    ov.onOpen(ctx);
    open = true;
    lastPoll = Date.now();
    ctx.clear();
    ov.draw(ctx);
    lastDraw = Date.now();
};

globalThis.onResume = function () {
    ov.onResume(ctx);
    lastDraw = 0;          /* repaint on the next tick */
    lastPoll = Date.now();
};

globalThis.tick = function () {
    if (!open || parked()) return;
    const now = Date.now();
    ov.tick(ctx);
    if (now - lastPoll >= POLL_MS) {
        lastPoll = now;
        ov.onPoll(ctx);
    }
    if (now - lastDraw >= DRAW_MS) {
        lastDraw = now;
        ctx.clear();
        ov.draw(ctx);
    }
};

globalThis.onMidiMessageInternal = function (data) {
    if (!open || !data || data.length < 3) return;
    ov.onMidi(ctx, { data: [data[0] | 0, data[1] | 0, data[2] | 0] });
};

globalThis.onMidiMessageExternal = function () {
    /* A keyboard on cable 2 drives the DSP directly (a chord selector by
     * pitch class); the picture follows through `prog` on the next poll. */
};

globalThis.onUnload = function () {
    open = false;
    /* onClose releases the hold; these stop the two things that can still be
     * sounding on their own clock. The DSP's next blocks emit the note-offs
     * -- packets already injected have reached Move and the host synthesises
     * no all-notes-off for us. */
    ov.onClose(ctx);
    ctx.setParam("preview", "off");
    ctx.setParam("run", "off");
};
