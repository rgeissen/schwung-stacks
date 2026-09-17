# Stacks as an overtake tool — design

## The decision

On 2026-09-13, in the Schwung Discord, Charles answered the ownership brief
that five PRs had been circling:

> I think that this module makes the most sense as an overtake module that can
> address slots (see TB-3PO) rather than a midi effect. I really want to make
> sure that slot modules don't effect Move's native UI, so you'd be in charge
> of the knobs/pads.

and, on stamping:

> If you want for move to capture what's played, move needs to be recording,
> yeah (or you do song.abl manipulation but unadvisable). With schwung we
> *don't* override how Move works, we work within its workflow.

So: Stacks becomes a **tool** (`component_type: "tool"`, `tool_config.overtake`)
reached from the Tools menu, owning the whole surface while it is up, emitting
MIDI to a Move track through `host->midi_inject_to_move()`, and getting a clip
the only sanctioned way — Move records it. The direct Song.abl write goes.

What this buys, in the host's own words: an overtake module owns its own tick
and all the input, so `onPoll` (#490) and the jog-click claim (#493) stop being
needed; pad ownership (#492, #494) stops being a question; and the Remote UI
comes through the Tool tab, which already carries live derived values.

## What stays, what goes

The port is bounded because the takeover already behaves like an overtake UI
wearing a canvas jacket: `canvas.js` draws the whole 128x64 screen, handles
the eight encoders and their LED rings, the jog, Copy/Delete/Undo/Mute, and all
32 pads with their lamps. The C DSP already counts Move's clock from cable 0,
runs the harmony, the scheduler, the patterns, the humanise hash and the
rec-arm stamp. None of that logic changes.

| Stays (untouched) | Changes (host surface) | Deleted |
|---|---|---|
| Harmony, shapes, rhythms, colours, library (`stacks.c`) | Manifest: MIDI FX → tool | `stamp_mode` and `stamp_to_file()` (Song.abl write) |
| Scheduler, `pulse` counter, first-clock-is-downbeat | Output: chain out → `midi_inject_to_move` cable 2 | `canvas_overlay` adapter, `onPoll`, `claims_jog_click`, `claims_edit_ccs` |
| Patterns (`@` blob, `used`, copy) | Input: `on_midi` sources, pads on the audio thread | `host_pad_block`, `reconcilePadBlock`, `shadow_get_display_mode` |
| Humanise hash; Stamp plays the take Preview played | Screen: `ctx.*` → the overtake draw globals via a 20-line shim | `drawCell` (no chain cell to draw in) |
| The drawn picture — roll, names, playhead anchor | LEDs: `move_midi_internal_send` → `setLED` / `setButtonLED` | The panel's own heartbeat and extra_keys workarounds |
| Rec-arm stamping: arm → Move's downbeat → one lap → `run` owed back | Remote UI: `midi_fx1:` chain panel → Tool tab (`overtake_dsp:`) | `viz.extra_keys` declarations, `as_page`, the `Grid` canvas param |
| `web_ui.html` layout and drawing | Persistence: set-file `state` → own file | Own-output filter stays (see *Own tail*) |
| Every DSP test that drives `set_param`/`on_midi`/`render_block` | `tests/test_stacks_pads.mjs` harness → overtake globals | |

## Manifest

```json
{
  "id": "stacks",
  "name": "Stacks",
  "version": "0.2.0",
  "description": "Chord progressions drawn on a staff. Write one, hear it, let Move record it.",
  "component_type": "tool",
  "dsp": "dsp.so",
  "ui": "ui.js",
  "web_ui": "web_ui.html",
  "tool_config": {
    "interactive": true,
    "skip_file_browser": true,
    "overtake": true
  },
  "capabilities": {
    "midi_out": true,
    "suspend_keeps_js": true,
    "button_passthrough": [85, 118]
  }
}
```

`chain_params` goes with the chain. The DSP keeps answering `get_param` for
every key it does today — the UI and the Tool tab read them by name — but the
declaration the knob grid consumed is not needed by an overtake, which draws its
own bank row already.

**`button_passthrough: [85, 118]`** was meant to be the whole answer to the
stamping question: the shim's comment on the passthrough bitmap
(`schwung_shim.c` ~7760) says it is *"used to let Play, Record etc. reach Move
firmware so Move handles them natively"*. **On the device it holds for Play and
not for Record** — see *Phase 0 result* below. Until that is resolved upstream,
the stamping flow is arm → Back (suspend) → Record on Move; the DSP keeps
ticking while parked and plays its lap on Move's Start. Keep 118 declared: it
costs nothing and is the right thing the moment the host honours it.

## Lifecycle

Host contract (`docs/MODULES.md` "Overtake Modules", `ADDRESSING_MOVE_SYNTHS.md`
"Writing a generator tool"): the host progressively clears every LED, shows
"Loading...", waits ~500 ms, then calls `init()`. `tick()` fires every frame.
Back **suspends** (`suspend_keeps_js`): the UI parks, display and LED bindings
become no-ops, input stops reaching the module, `tick()` keeps firing, the DSP
keeps rendering. Shift+Back **exits** (`onUnload`). Re-opening from Tools
resumes with `init()` again, then `onResume()`. Shift+Vol+Jog-Click is the
host's escape and cannot be trapped.

| `canvas_overlay` today | overtake `ui.js` | Notes |
|---|---|---|
| `onOpen(ctx)` | `globalThis.init()` | Also runs on every resume — read the DSP's state, do not assume a fresh one |
| `draw(ctx)` + `tick(ctx)` | `globalThis.tick()` | Draw when dirty, not every frame; the LED budget is real (~64 packets) |
| `onMidi(ctx, msg)` | `onMidiMessageInternal(data)` | Cable 0: pads, encoders, jog, buttons, knob touch |
| — | `onMidiMessageExternal(data)` | Cable 2: a keyboard, and the module's own echo (ignore here; the DSP filters) |
| `onPoll(ctx)` | (gone) | `tick()` has `host_module_get_param`; read `status`/`patterns` on a 250 ms cadence there |
| `onClose(ctx)` | `globalThis.onUnload()` | All-notes-off, release `hold_run`, persist |
| — | `globalThis.onResume()` | Invalidate the LED delta-cache and repaint: the host cleared the hardware while we were parked |

**The pad block disappears.** In overtake mode the module owns the pads by
definition — no global to take in `init`, none to strand. `lastPad`/`lastLed`
caches stay (they are the delta-cache `setLED` wants) and are cleared in
`init` and `onResume`.

**Suspend and `run`.** Parked, Stacks keeps sounding if `run` is on — that is
the point of `suspend_keeps_js`, and it is how the rec-arm take survives the
user looking at Move's clip view while it records. The "run is owed back" rule
is unchanged: a take that parked `run` returns it when that take's transport
stops, whether or not the UI is on screen.

**Exit is a real exit.** `onUnload` must send note-offs for every sounding
voice before returning — packets already injected have reached Move, and the
host synthesises no all-notes-off. Whatever is still queued is discarded by the
host, which is fine; what already went out is ours.

## Input

Sources reach the DSP's `on_midi` with a `source` tag; the UI's two hooks are
the JS mirror of the same split.

- **Cable-0 realtime** (`0xF8 / 0xFA / 0xFB / 0xFC`, len 1, `SOURCE_EXTERNAL`):
  delivered to the overtake DSP straight from the shim's MIDI_OUT scan
  (`schwung_shim.c` ~1366), "regardless of the user's MIDI Clock Out
  preference". The `pulse` counter, the first-clock-is-downbeat rule and
  `tests/test_stacks_downbeat_phase.sh` carry over unchanged.
- **Pads** (cable 0 notes 68–99, `SOURCE_INTERNAL`): the shim also hands these
  to the DSP on the audio thread (~9047), which is a lower-latency path than
  the JS round-trip. Design choice: **the UI keeps owning pad semantics**
  (select, edit rows, pattern slots, paging) and writes params, exactly as
  today; the DSP ignores pad ids in `on_midi` because 68–99 are *pad
  identifiers, not pitches* and must never enter the pitch-class selector.
- **A keyboard** (cable 2, `SOURCE_EXTERNAL`, real pitches): still a chord
  selector by pitch class. Unchanged.
- **Encoders / jog / buttons**: the UI's `onMidiMessageInternal`, the code
  that is in `canvas.js` now. Shift is *ours* in overtake mode, so Shift+Copy
  = Double Length and Shift+Undo = Redo come back to life — the Mute-modifier
  workaround from the chain era can go once the gesture doc is updated.
- **Passthrough** — Play (85) and Record (118) reach Move *and* the module
  sees them, so the transport picture (`prog.running`) still follows Move's
  buttons.

### Own tail

The shim routes MIDI_OUT — including Move's echo of what we injected on
cable 2 — to the overtake DSP's `on_midi` (~1460). So the module is still fed
its own output, and the rule from CLAUDE.md stands: **filter on the sounding
voices, not on the transport.** `tests/test_stacks_own_tail.sh` keeps its job.

## Output

```c
/* cable 2, note-on, on the channel that addresses the Move track */
uint8_t pkt[4] = { (2 << 4) | 0x09, 0x90 | (ch & 0x0F), note, vel };
host->midi_inject_to_move(pkt, 4);
```

A new `track` param (1–16, default 1) picks the channel; the chain era's
`recv_ch` becomes this. Overtake DSPs get a dedicated bounded inject queue so
output cannot re-enter the DSP's own `on_midi` through the inject path (it
still returns as Move's echo — see *Own tail*). Rate limit: the shim drains
8 packets per SPI tick (~2.9 ms); a five-voice chord with humanise spread is
well inside that, and note-offs must not be batched into the same tick as the
next chord's note-ons on a 16th-note rhythm figure — the scheduler already
spaces them by `off`, so this is a check, not new code.

`host->midi_inject_to_move` is NULL on non-shadow builds; every emit site
guards on it. It is in the pinned `src/host/plugin_api_v1.h` already, as is
`get_bpm` for the internal audition clock.

## Stamping

Mechanism unchanged; two edits.

1. **Delete the file write.** `stamp_mode`, `stamp_to_file()`, `find_song_abl`
   and the JSON walker go. The `read` (import a clip from Song.abl) is a read
   and stays — reading is free.
2. **The rec-arm sequence, with Record passing through:**
   - Stamp → `status = armed`, `run` parked, mark blinks, toast `REC + PLAY`.
   - User presses Record on Move (passes through; Move arms the track) and
     Play (passes through; Move starts, cable 0 carries `0xFA` then clocks).
   - First clock after Start = downbeat → one lap through `midi_inject_to_move`
     → `status = working` → wrap → `run = 0`, `status = ok`, toast
     `TAKE RECORDED`, `run` owed back and returned when Move's transport stops.
   - Pressing Stamp while armed cancels, as today.

## Screen and LEDs

The overtake draw API is a set of globals — `clear_screen()`, `print(x, y, s,
size)`, `fill_rect`, `draw_rect`, `set_pixel`, `draw_line` — where the canvas
gave a `ctx`. `canvas.js` derives every size from `ctx.width/height` and never
touches absolute space, so a **20-line `ctx` shim** (`{width:128, height:64,
fillRect: fill_rect, …, now: Date.now, getParam: host_module_get_param,
setParam: host_module_set_param}`) leaves every draw function as it is. The
text that `ctx.print` drew ("reading progression...") is the one call that
needs the host font instead of our glyphs, and it maps directly.

LEDs: `setLED(note, colour)` / `setButtonLED(cc, value)` from
`shared/input_filter.mjs` replace `move_midi_internal_send` and carry their own
skip-duplicate cache. The pad row's four states (selected / playing / chord /
rest) and the parameter rows are the same palette indices. Progressive init:
paint the 32 pads and 8 rings over the first few ticks, never in one burst.

## Remote UI: the Tool tab

`docs/MODULES.md` "Remote UI for overtake tools". The manager discovers the
active tool by probing `get_param("module_id")`, serves our `web_ui.html`, and
seeds/refreshes it from `get_param("state")`.

- `module_id` → `"stacks"`.
- `state` → a **flat JSON object of strings**: every param the panel binds
  (the same keys as today), plus `prog`, `status`, `patterns`, and the save
  blob under `s` (a string, so it fits the rule). Today `state` *is* the
  blob; it becomes the map that contains it. Include `rev`.
- `rui_poll` → `rev:on:tick:bpm[:devms]` — `rev` is the existing undo/edit
  counter; `on/tick/bpm` from the scheduler; `devms` only while playing (the
  stopped digest must stay byte-stable). The shim probes this in-process and
  pushes on change — **this is the heartbeat #491 built in the manager, as a
  host primitive.** The panel's `posUnits` anchor stays; `devms` lets it
  time-base the head instead of inferring from delivery.
- `remote_snapshot_rt_safe` → `"1"`. Serialising 16 patterns and a progression
  on the audio thread is exactly what this exists to avoid, and `stk_state_t`
  is a fixed struct — nothing `render_block` or `set_param` frees.
- Panel edits: `overtake_dsp:` prefix instead of `midi_fx1:`. Keys `sel`
  and `transport` get the selection/transport treatment (`sel_ruisel`, or
  the `transport` key) so a click on a chord card is not debounced behind
  the editing quiet-window — which is the `sel` exemption dropped from #517,
  solved where it belongs.
- Delete from `web_ui.html`: the client-side heartbeat, the extra_keys
  fetch, the armed-state guesswork that predates `status` (already gone),
  `pagehide`/`visibilitychange` hold-release stays (it is the browser's
  problem either way).

## Persistence

A chain module's `state` rode in the set. A tool has no slot in the set, so
Stacks owns its file: `/data/UserData/schwung/modules/stacks/state.json`,
holding the `s` blob (progression, patterns, settings) — the same format, so
`tests/test_stacks_state.sh` keeps its corpus. Written by the existing worker
thread (never the audio thread) on a debounced edit counter and in `onUnload`;
read in `create_instance`. TB-3PO does the same.

## Tests

- **Unchanged:** every `.c` test that drives the DSP through
  `set_param`/`on_midi`/`render_block` — shapes, clip length, downbeat phase,
  own tail, harmony, inversion, patterns, undo, rhythm tiling, state.
- **Harness swap:** `test_stacks_pads.mjs`, `test_stacks_pattern_pads.mjs`,
  `test_stacks_knob_type.mjs`, `test_stacks_rest_selection.mjs` load
  `canvas.js` with stubbed `canvas_overlay` globals; they load `ui.js` with
  stubbed overtake globals (`setLED` capture instead of
  `move_midi_internal_send` capture). Assertions do not change.
- **New:**
  - `test_stacks_manifest.sh` — pins `tool_config.overtake`,
    `suspend_keeps_js`, and that `button_passthrough` contains 85 and 118 (a
    stamp with no Record button is #492 all over again).
  - `test_stacks_inject.sh` — drives a lap and asserts every packet is
    cable 2, CIN matches status, channel = `track - 1`, and that `onUnload`
    leaves no note without its off.
  - `test_stacks_tool_tab.sh` — `state` parses as a flat object of strings
    containing every bound key; `rui_poll` is byte-stable while stopped.
  - `test_host_abi_pinned.sh` — unchanged; the port uses no callback the pin
    does not carry.

## Phases (all landed 2026-09-15; the commits are the record)

0. **Prove the path on the device** — manifest + DSP as a tool with a
   ten-line `ui.js` (title + Play). Verify: Tools menu entry, cable-0 clock
   arrives, a chord reaches a Move track through inject, Record and Play pass
   through and Move records a lap. No port of `canvas.js` yet. This is the
   phase that can still surprise us; do it first.
1. **`ui.js`** — the `ctx` shim, the lifecycle table above, `setLED`, pads
   without the block, Shift gestures restored. Harness swap.
2. **Tool tab** — `module_id`, `state` as a map, `rui_poll`,
   `remote_snapshot_rt_safe`; `web_ui.html` on the `overtake_dsp:` prefix.
3. **Delete** — file stamp, `canvas_overlay`, pad block, extra_keys, the
   reconciler and its doc, `drawCell`, `stamp_mode`. Run `tools/check_*.py`.
4. **Docs** — CLAUDE.md and the control matrix rewritten for a tool; the
   README's "Requires" section drops the "two capabilities offered upstream"
   line: it needs none.

## Phase 0 result (2026-09-15, on the device)

Proven, each with a log line to show for it:

- **Tools menu → load.** `Overtake DSP: loaded generator from …/tools/stacks/dsp.so`;
  `ui.js` loaded; `init()` returned; passthrough set to `85,118`.
- **Inject.** `stacks tool: inject 3 pkts, first 90 39 64 ch1` — the loop's
  triads reached track 1 and were audible.
- **Suspend / resume.** Back parked it (`suspend_keeps_js`), the Tools menu
  listed it as "Stacks, suspended", re-opening resumed with `onResume`.
- **Move's transport reaches the DSP.** On Record, `Sampler: transport_playing=1
  (MIDI Start)` and the armed lap fired within 2 ms: four chords, on/off pairs,
  one lap, silence. **Track 1's clip held the chords.** Twice.

Two things the spike changed:

- **Record does not pass through while the overtake is up.** `button_passthrough:
  [85, 118]` is honoured for Play — the module logged `cc 85` and Move's
  `component-usage-report` counted `CC85:6` — but a Record press reached
  neither Move nor the module: no `cc 118` in the module's trace, no `CC118` in
  Move's report, Record lamp dark. Not the sampler (never armed) and not a stuck
  Shift (tapping Shift first changed nothing). The same button works the moment
  the tool is suspended. A host finding for Charles; until it lands, **the
  stamping flow is arm → Back → Record on Move**, which is exactly the TB-3PO
  shape: the DSP keeps ticking while parked, hears Move's Start on cable 0,
  and plays its lap. The toast becomes `ARMED · Back, then REC on Move`.
- **The four empty bars are four rests, not an empty buffer.** `count` is 4
  on a fresh instance; "empty" means no chord that is not a rest.

And two things learned for Phase 1:

- **Do not read params from a background tick.** The spike polled five keys
  every 250 ms in `tick()`, which keeps running while suspended, and the reads
  failed (`param_giveup … overtake_dsp:progression … FAIL`). Poll only while on
  screen (the host swaps display bindings for no-ops when parked; track it from
  `onResume`/suspend, or stop polling when a draw call is a no-op).
- **`/tmp` on the device is full and the shim logs audio dropouts continuously
  from boot**, before Stacks is loaded. Device housekeeping, not the module —
  but it will confuse any timing measurement until cleared.

## Left to confirm with Charles

Two things, and the port does not block on either.

- ~~Which channel addresses which Move track~~ — **answered on the device,
  2026-09-17: none.** Move plays an injected note on whichever track is
  SELECTED and ignores the channel. The `track` param went; `Read Track`
  stays only because `Read Clip` needs a Song.abl track index and the host
  does not yet expose `clip_selected_track()` (#504) to modules. Exposing it
  is a small, natural Schwung PR and would remove the knob.
- **`button_passthrough: [118]` is swallowed** (see Phase 0 result) — is that
  a bug in the overtake filter, or does the sampler own Record by design? If
  the latter, the Back → Record flow is the contract and the docs should say so.
