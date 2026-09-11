/*
 * THE SIXTEEN PATTERN SLOTS, DRIVEN FROM THE PADS.
 *
 * The store itself is tested natively against the DSP; this is the other half
 * -- that the buttons under your hand reach it. Rows 2 and 3 of the PATTERNS
 * page are slots 1-16, and the gestures have to be exactly the panel's, or the
 * same slot means two different things depending on which surface you touched:
 *
 *   tap        go to that slot, FULL OR EMPTY -- you are always in one, and an
 *              empty slot is where the next pattern gets written
 *   hold       copy what you are playing into it and carry on there
 *   Mute+tap   clear it
 *
 * And the chord row underneath must still select chords, because it does that
 * on every page -- the one row a hand can trust without checking where it is.
 */
import { readFileSync } from "node:fs";
import vm from "node:vm";
const src = readFileSync(new URL("../src/canvas.js", import.meta.url), "utf8");
let now = 1000, writes = [], says = [];
const prog = "v13|4|1|-1|8|5ad|9|32|0|4|120|0|0|"
  + "Am7,A3,0,8,0,100,0,1,57:100.60:100.64:100.67:100;"
  + "G,G4,0,8,0,100,0,1,55:100.59:100.62:100;"
  + "F,F4,0,8,0,100,0,1,53:100.57:100.60:100;"
  + "G,G4,0,8,0,100,0,1,55:100.59:100.62:100";
let patterns = "0005|1";                      /* slots 1 and 3 hold music, in 1 */
const ctx = {
  width:128, height:64, state:{},
  clear(){}, setPixel(){}, drawRect(){}, fillRect(){}, drawLine(){}, print(){},
  now(){ return now; }, random(){ return .5; },
  getValue(){ return ""; }, setValue(){ return true; },
  getParam(k){ return k === "prog" ? prog : k === "patterns" ? patterns : "0"; },
  setParam(k,v){ writes.push(k+"="+v); return true; },
  sourcePath(){ return ""; },
};
const sandbox = { console, Date, Math, JSON, parseInt, parseFloat, Number, isFinite,
                  isNaN, String, Array, Object, Set, Map, performance };
sandbox.globalThis = sandbox;
sandbox.move_midi_internal_send = (m) => { (sandbox.__leds ||= []).push(m); };
sandbox.host_pad_block = () => {};
vm.createContext(sandbox);
vm.runInContext(src, sandbox, { filename: "canvas.js" });
const ov = sandbox.canvas_overlay;
ov.onOpen(ctx);

/* Page up three times: WRITE -> VOICE -> FEEL -> PATTERNS */
for (let i = 0; i < 3; i++) ov.onMidi(ctx, { data:[0xB0, 55, 127] });
ov.onPoll(ctx);

const PAD = (row, col) => [68, 76, 84, 92][row] + col;   /* row 1 = slots 1-8 */
const run = (label, fn, expect) => {
  writes = [];
  fn();
  const got = writes.join(",") || "(nothing)";
  const ok = got === expect;
  console.log("  %s %s %s", label.padEnd(34), got.padEnd(22), ok ? "ok" : "FAIL want " + expect);
  if (!ok) process.exitCode = 1;
};

run("tap slot 3 (holds music)", () => {
  ov.onMidi(ctx, { data:[0x90, PAD(1,2), 127] });
  now += 80;
  ov.onMidi(ctx, { data:[0x90, PAD(1,2), 0] });
}, "pat_recall=3");

run("tap slot 5 (empty) still switches", () => {
  ov.onMidi(ctx, { data:[0x90, PAD(1,4), 127] });
  now += 80;
  ov.onMidi(ctx, { data:[0x90, PAD(1,4), 0] });
}, "pat_recall=5");

run("HOLD slot 9 copies there", () => {
  ov.onMidi(ctx, { data:[0x90, PAD(2,0), 127] });
  now += 700;                                   /* past LONG_PRESS_MS */
  ov.onMidi(ctx, { data:[0x90, PAD(2,0), 0] });
}, "pat_store=9");

run("MUTE + tap slot 3 clears it", () => {
  ov.onMidi(ctx, { data:[0xB0, 88, 127] });     /* Mute down */
  ov.onMidi(ctx, { data:[0x90, PAD(1,2), 127] });
  ov.onMidi(ctx, { data:[0xB0, 88, 0] });       /* Mute up */
}, "pat_delete=3");

run("slot 16 is reachable", () => {
  ov.onMidi(ctx, { data:[0x90, PAD(2,7), 127] });
  now += 80;
  ov.onMidi(ctx, { data:[0x90, PAD(2,7), 0] });
}, "pat_recall=16");

run("the CHORD row still selects chords", () => {
  ov.onMidi(ctx, { data:[0x90, PAD(0,1), 127] });
  ov.onMidi(ctx, { data:[0x90, PAD(0,1), 0] });
}, "sel=2,play=on,play=off");

/* the lamps */
sandbox.__leds = [];
ov.tick(ctx);
const lit = (sandbox.__leds || []).filter(m => m[2] >= 76 && m[2] <= 91);
const colour = {}; lit.forEach(m => colour[m[2]] = m[3]);
console.log("  %s slot1=%s slot3=%s slot2=%s",
  "LEDs (1 and 3 hold, in 1)".padEnd(34), colour[76], colour[78], colour[77]);
