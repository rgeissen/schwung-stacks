/*
 * ui.js IS THE CTX, AND THE LIFECYCLE THE CANVAS RUNTIME USED TO RUN.
 *
 * canvas.js was written against a `ctx` handed in by the chain's canvas
 * runtime; as an overtake tool, ui.js builds that ctx from the host's draw
 * globals and drives canvas_overlay from init/tick/onMidiMessageInternal.
 * Three things this pins, and the last two are the ones that hurt on a
 * device:
 *
 *  - init opens and DRAWS; a tick draws at most every DRAW_MS and polls at
 *    most every POLL_MS -- the tick runs every 2.9 ms and a full redraw is
 *    most of that.
 *  - PARKED (globalThis.overtakeParked), a tick draws NOTHING and reads
 *    NOTHING: the host swapped the globals for no-ops and the param mailbox
 *    does not answer a parked module (phase 0 measured the give-ups).
 *  - Shift arrives as CC 49 and is read from the wire; no chain-era host
 *    switch is ever touched; onUnload leaves nothing sounding.
 */
import { readFileSync } from "node:fs";

const canvasSrc = readFileSync(new URL("../src/canvas.js", import.meta.url), "utf8");
const uiSrc = readFileSync(new URL("../src/ui.js", import.meta.url), "utf8")
    .replace(/^import\s+"[^"]*canvas\.js";\s*$/m, "");   /* the harness loads it itself */
if (uiSrc.includes("import ")) { console.log("FAIL: ui.js has an import the harness does not know"); process.exit(1); }

const g = {};
let padBlock = null, LEDS = [];
new Function("globalThis", "shadow_get_shift_held", "shadow_get_overlay_state",
             "host_pad_block", "move_midi_internal_send", "shadow_restore_knob_leds",
             "shadow_get_display_mode",
             canvasSrc + "\nglobalThis.__S = S;")(g, () => 0, () => null,
                 (v) => { padBlock = v; }, (m) => LEDS.push(m), () => {}, () => 1);

/* The overtake draw globals and the param shim, counted. */
const calls = { draw: 0, clear: 0, get: 0, set: [] };
const PROG = "v12|4|1|1|8|ab5|9|32|1|4|120|0|0|" +
  ["Am,A3,0,8,0,100,0,1,57.60.64", "C,C3,0,8,0,100,0,1,48.52.55",
   "F,F3,0,8,0,100,0,1,53.57.60", "G,G3,0,8,0,100,0,1,55.59.62"].join(";");
const H = {
    clear_screen: () => { calls.clear++; },
    set_pixel: () => { calls.draw++; }, fill_rect: () => { calls.draw++; },
    draw_rect: () => { calls.draw++; }, draw_line: () => { calls.draw++; },
    print: () => { calls.draw++; },
    host_module_get_param: (k) => { calls.get++; return k === "prog" ? PROG : "0"; },
    host_module_set_param: (k, v) => { calls.set.push(k + "=" + v); },
};
new Function("globalThis", ...Object.keys(H), uiSrc)(g, ...Object.values(H));

let bad = 0;
const check = (what, got, want) => {
    const ok = String(got) === String(want);
    console.log(`  ${what.padEnd(46)} ${String(got).padEnd(14)} ${ok ? "ok" : "FAIL want " + want}`);
    if (!ok) bad = 1;
};

/* --- init: opens under the overtake flag, never takes the chain's pad block, draws --- */
g.init();
check("no chain pad switch touched", padBlock, null);
check("init drew the screen", calls.draw > 50, true);
check("init cleared before drawing", calls.clear >= 1, true);
check("init read prog", calls.get > 0, true);

/* --- tick: bounded draw and poll cadence --- */
calls.draw = 0; calls.get = 0; calls.clear = 0;
for (let i = 0; i < 5; i++) g.tick();          /* same millisecond: no second draw */
check("5 ticks in one ms: no redraw", calls.clear, 0);
const realNow = Date.now;
let t = realNow();
Date.now = () => t;
t += 40; g.tick();
check("a tick 40ms later redraws once", calls.clear, 1);
const getsBefore = calls.get;
t += 300; g.tick();
check("a tick 300ms later polls", calls.get > getsBefore, true);

/* --- parked: nothing drawn, nothing read --- */
calls.draw = 0; calls.get = 0; calls.clear = 0;
g.overtakeParked = true;
t += 1000; g.tick(); t += 1000; g.tick();
check("parked tick draws nothing", calls.draw + calls.clear, 0);
check("parked tick reads nothing", calls.get, 0);
g.overtakeParked = false;

/* --- resume: caches dropped, then a repaint on the next tick --- */
LEDS = []; calls.clear = 0;
g.onResume();
g.tick();
check("resume repaints on the next tick", calls.clear, 1);
check("resume re-emits the pad row", LEDS.some((m) => m[2] >= 68 && m[2] < 76), true);

/* --- Shift on the wire --- */
g.onMidiMessageInternal([0xB0, 49, 127]);
check("CC 49 down -> shift held", g.__S.shiftCC, true);
g.onMidiMessageInternal([0xB0, 49, 0]);
check("CC 49 up -> shift released", g.__S.shiftCC, false);

/* --- pads reach canvas.js's onMidi through the adapter --- */
calls.set = [];
g.onMidiMessageInternal([0x90, 69, 100]);      /* pad 2: select + play */
check("a pad press writes through", calls.set.some((s) => s.startsWith("sel=")), true);

/* --- unload leaves nothing sounding --- */
calls.set = [];
g.onUnload();
check("unload: play off", calls.set.includes("play=off"), true);
check("unload: preview off", calls.set.includes("preview=off"), true);
check("unload: run off", calls.set.includes("run=off"), true);
calls.draw = 0; g.tick();
check("a tick after unload draws nothing", calls.draw, 0);
Date.now = realNow;

console.log(bad ? "FAIL" : "PASS: ui.js runs canvas.js under the overtake lifecycle, and a parked tick touches nothing");
process.exit(bad);
