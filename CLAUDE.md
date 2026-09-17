# CLAUDE.md — Stacks

A chord-progression **overtake tool** for Ableton Move, running under
[Schwung](https://github.com/charlesvestal/schwung) and opened from its Tools
menu. Four files, one module:

    src/dsp/stacks.c    the DSP -- harmony, the scheduler, the clip reader; injects to a Move track
    src/canvas.js       the whole 128x64 surface: the roll, the banks, the pads, the gestures
    src/ui.js           the overtake lifecycle around canvas.js (the `ctx` it was written for)
    src/web_ui.html     the Remote UI panel, served by schwung-manager under the Tool tab

It was a chain MIDI FX until 2026-09-15. The port, and why, is
`docs/plans/2026-09-15-overtake-port-design.md`; the short version is
Charles's: a module that wants the pads and the knobs is an overtake, and
slot modules must not touch Move's native UI.

## Build, install, test

    scripts/build.sh                     # cross-compiles dsp.so (Docker), packs dist/
    scripts/install.sh --host=IP --reload   # installs the module ONLY, then reloads
    for t in tests/*.sh; do bash "$t"; done

`--reload` is not optional after a `dsp.so` change: installing leaves the
already-loaded instance running, so a C fix looks like it did nothing.

## THIS REPO DOES NOT CONTAIN THE HOST

Only `src/host/*.h` is Schwung's, vendored so this builds standalone, PINNED to
an upstream commit and checked by `tests/test_host_abi_pinned.sh`. Read the
story in that file before touching them: a module that declares a callback the
host does not provide reads one field past the struct, passes its own
`if (host->fn)` guard, and boot-loops the device.

Anything this module needs that Schwung does not offer is a PULL REQUEST to
Schwung, never a local patch. As an overtake it needs nothing today. One host
finding is open: `button_passthrough` is honoured for Play (85) and not for
Record (118) while a tool is on screen -- measured 2026-09-15, neither Move
nor the module sees the press -- which is why a stamp is recorded after Back.

## THE HOST CONTRACT, in the order it bit

- **An overtake owns the surface, and nothing else.** Every pad, knob and
  button is the tool's while it is up; the host clears every LED on the way in
  and on the way out; there is no `host_pad_block` to take and no CC to claim
  -- the chain-era reconciler, the claims and the pad-LED snapshot all went
  with the chain. The two exceptions are declared: Play and Record pass
  through to Move.
- **The DSP is the same state machine.** `plugin_api_v2` is an ADAPTER beside
  the MIDI FX entry points: `on_midi` is `stk_process_midi`, `render_block` is
  `stk_tick`, and what they return is injected into Move on cable 2
  (`host->midi_inject_to_move`), channel 1 always -- Move plays an injected
  note on the SELECTED track and ignores the channel (measured), so nothing
  is chosen; `Read Track` exists only because `Read Clip` needs a Song.abl
  track index and the host does not yet expose the selection. Both init symbols
  are exported, so every DSP test drives the same code through the MIDI FX
  surface. Cable-0 notes tagged INTERNAL are pad IDENTIFIERS (68 is G#, chord
  9 by pitch class) and are dropped before the selector; Move's echo of our
  own output comes back, so the own-tail filter stays load-bearing. **And the
  tool injects only what it GENERATES, never what it received**: process_midi
  passes a CC through (chain semantics -- a pedal should reach the synth),
  but a tool's output is echoed back to its input, so press Stop, Move sends
  All Sound Off, and a passed-through CC looped once per block for the rest
  of the session -- "no chords after stop". Notes only, both ways.
- **ui.js is the `ctx`.** canvas.js derives every size from `ctx` and never
  reaches absolute space, so the port was an adapter: the overtake draw
  globals, method for method, and the lifecycle (`init` -> `onOpen`, `tick`
  -> `tick` + `draw` at ~30fps + `onPoll` at 250ms, `onResume`, `onUnload` ->
  `onClose`). Every ctx method reaches for the global BY NAME at call time:
  the host swaps them for no-ops while the tool is parked.
- **Parked, touch nothing -- and play only for a take.** Back suspends
  (`suspend_keeps_js`): the DSP keeps ticking -- an armed take goes on
  waiting for Move's Start -- and `tick()` keeps firing with
  `globalThis.overtakeParked` raised. A parked tick draws nothing and reads
  nothing: the param mailbox does not answer a parked module (a give-up per
  poll, measured), and a picture nobody sees is not worth the reads. It
  WRITES once: `parked=on`. Move plays an injected note on whichever track is
  SELECTED, not on a channel, so a parked tool following the transport or
  looping lands on every track you touch afterwards ("the other track plays
  the stamped notes too"). So parking switches Run and the loop off unless a
  take is armed or under way, and resume gives them back -- including the
  Run a take parked, which now returns on resume rather than at the
  transport stop. `tests/test_stacks_parked.sh`.
- **Shift arrives.** As CC 49, forwarded to an overtake -- and the shim's own
  Shift tracking is documented as not working in that mode -- so canvas.js
  records it from the wire and never asks. One recorder site; gestures read
  `shiftHeld()`. (On the chain it was the opposite, and a `d1 === 49` branch
  was dead code that looked like a feature.)
- **A tool has no slot in the set, so it keeps its own file.** Exactly
  `get_param("state")`, at `/data/UserData/schwung/stacks/state.json`
  (outside the module folder: a reinstall must not wipe it). The audio thread
  never touches the disk: the worker asks, `tick` serialises into a buffer
  and releases it, the worker writes temp-plus-rename. Saved once the rev has
  been quiet for 1.5 s -- a jog spin is one save, not one per detent -- and
  synchronously on destroy. The worker's "already saved" mark starts at
  ZERO, not at the rev when the thread first runs: `pthread_create` returns
  before that, and an edit in the gap counted as saved.
  `tests/test_stacks_persist.sh`.
- **The panel comes through the Tool tab.** The manager discovers the tool by
  `module_id`, seeds the browser from `state` -- a FLAT MAP of strings, the
  save blob first under `s` so a restore still finds it, then every readable
  param walked from the one declaration -- and re-reads it only when
  `rui_poll`'s rev moves. So rev bumps on everything snapshot-visible: every
  write at the door, a parse landing, a status transition. The transport is
  `rui_play` pushes, not a param. `tests/test_stacks_tool_tab.sh`.

## Song.abl is READ-ONLY

Reading a clip is free. **Writing fights Move**, which holds the set in memory
and autosaves over it (a spliced file was overwritten 142 seconds later with
nothing touched), and Schwung's rule is to work within Move's workflow rather
than override it. So there is no file stamp: Stamp ARMS, the user presses
Record on Move, and the module plays exactly one lap on Move's downbeat for
Move's own recorder to capture. The splice-and-rename writer is in git.

Rules the DSP exists to demonstrate, all of them things an author otherwise
gets wrong:

- **An entry point is the SPI callback, so the parse is not there.**
  `set_param` bumps an atomic counter; a worker created with
  `PTHREAD_EXPLICIT_SCHED` + `SCHED_OTHER` parses and publishes with a
  release-store that `tick` acquires. Inheriting the callback's FIFO 70 starves
  Move's `Link Main` at 35. This is the granny bug avoided by construction.
- **The picture costs ONE read.** The whole progression — names, lengths,
  offsets, resolved pitches, the scale mask — is a single `prog` value. The C
  side publishes **resolved** notes so voicing is not implemented twice and
  the staff cannot draw a chord the synth never played.
- **Humanise is a HASH, not a generator.** The deviation for one note is a pure
  function of (seed, chord, voice), so the feel repeats until Randomize is
  pressed and **Stamp writes the take Preview played**. A per-pass `rand()`
  gives three different performances. The spread is *shifted* so its earliest
  note lands on the beat rather than clamped at zero — clamping makes a chord
  tighter the more humanise you dial in, which is backwards.

- **ONE CONTROL MUST NOT REDEFINE ANOTHER'S UNITS.** A chord's `len` was
  counted in units of `rate`, so turning Rate changed what every existing
  length MEANT: the same four chords were 4 bars at "1 bar" and 8 at "2 bar",
  and a library entry played at whatever scale happened to be selected -- a
  "12-bar" blues was 6 bars at "1/2" and 24 at "2 bar", where it truncated
  against the 16-bar clip. That single coupling produced three separate bugs
  (a mis-scaled library, a clip that truncated on a Rate change, and a playhead
  sweeping a different length than the music) before it was named. `len` is an
  absolute duration now -- eighths of a bar, always -- and Rate only chooses
  what a NEW chord gets. The fix DELETED code, which is the usual sign the cut
  is in the right place.
- **STAMPING HAD NO STATE, so it read as a dead button.** It does not write a
  clip -- it ARMS, waits for Move's downbeat and plays exactly one lap for
  Move to record -- and the module set no `status` for any of that, so the
  panel invented an armed flag of its own, started when IT pressed the button.
  A stamp armed from the MOVE therefore lit nothing anywhere ("stamping on the
  Move doesn't work", for a mechanism that was working the whole time), and a
  transport stop cancelled the arm while the browser went on saying ARMED.
  `status` publishes `armed` now, both surfaces read it, the surface blinks a
  mark and toasts `BACK, REC ON MOVE` (it cannot press Record itself, and
  the shim swallows the button while the tool is up). Arming also PARKS `run` so nothing sounds before the
  downbeat, and **every exit must put it back**: a cancel, and the take itself.
  Stopping after one lap REQUIRES `run = 0` (the sequencer follows it, and the
  alternative is stopping Move), but leaving it off is an action silently
  changing a setting -- the take ends and from then on **Move's play button
  does nothing at all**, with nothing on either screen saying why. Run is owed
  back and returns when that take's transport stops. Pressing the same control
  while armed CANCELS, because a state whose only exit was Move's stop button
  is a state the surface that entered it could not leave.
- **YOU ARE ALWAYS IN A SLOT.** Sixteen patterns, and this is pattern SELECT,
  not save/load: slot 1 is active and empty at start, every edit belongs to the
  active slot, and activating another READS it. There is no store step, so the
  switch is what must write the buffer through -- miss that and the edit is
  simply gone. Built the other way round first (a working buffer plus sixteen
  snapshots), which produced two complaints that were the same one: tapping an
  empty slot either captured the buffer into it or did nothing at all, and
  neither is what a pattern button does anywhere else. `used` means HAS MUSIC
  IN IT and for the ACTIVE slot is computed from the live buffer, so the lamp
  lights as the first chord is written rather than waiting for a save. A slot
  carries its CLIP LENGTH (a twelve-bar phrase read into a four-bar clip
  otherwise loses two thirds of itself), deleting the slot you are in empties
  what you are playing because they are the same thing, and the long press
  COPIES what you are playing into another slot -- the one thing select alone
  cannot do. They ride in the `state` blob behind an `@` marker so a blob
  written before patterns existed still loads.
  `tests/test_stacks_patterns.sh` is the spec.
- **THE MODULE MUST NOT BE DRIVEN BY ITS OWN OUTPUT.** An incoming note is a
  chord SELECTOR by pitch class -- that is the feature, so a pad, a keyboard
  and a clip can all drive it. But its output goes to the track and the track
  comes back: as Move's echo, and again as a clip once you have stamped one. An
  Am7 contains a C, C is pitch class 0, pitch class 0 is chord 1, so the chords
  it played were selecting chords -- and a selection is armed with HOLD, whose
  voices never expire, so the symptom was "the chord never moves on" rather
  than "it jumps". Filtered on the SOUNDING VOICES, not on "is the transport
  running": a driver clip still drives it and a keyboard still plays it, only
  its own tail is ignored. Underneath it, `held` was ONE pitch class cleared
  only on an exact match, so two notes held together or one lost note-off
  stranded the selection on for the session; it is a mask now.
  `tests/test_stacks_own_tail.sh` feeds every emitted note straight back in.
- **THE FIRST CLOCK AFTER START IS THE DOWNBEAT, and counting it made every
  chord early.** `pulse` was zeroed on 0xFA and incremented by every 0xF8, so
  it read 1 at the downbeat and the module fired a clock ahead of Move forever
  -- reported from the device as "the chords are stamped too early". This is
  the THIRD consumer to make the identical mistake; the other two are in
  `src/host/transport_grid.h`, measured at two tempos. Measured natively here
  as well (`tests/test_stacks_downbeat_phase.sh` drives Start + clocks and
  records the clock each chord lands on): chord 1 sounded at the START MESSAGE,
  before the downbeat existed, and the bar line came out at 95 instead of 96.
  Fixed at the COUNTER, not at the reading sites, because this `pulse` has a
  second producer -- the internal clock an audition self-runs on, which has no
  Start to be offset from -- so a correction at the dozen reading sites would
  have to know which clock it was reading. Nothing may sound between Start and
  that first clock either.
- **A QUANTISED REPORT NAMES A RANGE, NOT A POINT.** `posUnits` is floored to a
  whole unit, so a module truly at 3.7 reports 3, and re-anchoring the drawn
  playhead on it pulls the head BACK by up to a unit -- an eighth of a step, a
  third of a second at 98bpm -- before it runs forward to be pulled back again.
  "Every read is a re-sync" was safe only while reads were rare; once both
  surfaces asked several times a second it became a judder in the middle of
  every chord. The anchor is kept while the drawn position lies inside
  `[pos, pos+1)` and taken only outside it (a wrap, a jump, real drift). Two
  traps: the slack must cover the age of the read, and "have we anchored" is a
  FLAG -- testing the timestamp for truthiness makes a clock reading 0 look
  like no anchor and snaps on every publish, which is the bug it was meant to
  fix. `tests/test_stacks_surfaces_agree.sh` runs the anchor over a
  simulated take and fails on a single backwards step.
- **THE STAFF NAMES ITS CHORDS, at the foot, where the panel puts its cards.**
  Everything it drew was a SHAPE, and a shape is not a name -- you had to
  select a chord and read the CHORD bank to learn it was Amaj7, one at a time,
  which is what a picture of the whole progression exists to save you. Two
  rules: the tiny font is UPPERCASE-ONLY and that is fine because the module
  spells quality in full (AMAJ7 / AMIN7, never an ambiguous AM7); and a name is
  truncated but **never inside the ROOT** -- one glyph of "C#M7" is "C", a
  different chord printed with nothing to say it was cut, so a block too narrow
  for the root goes unnamed and the 6px band is not taken at all when no block
  can hold one.
- **A MOMENTARY THAT MISSES ITS RELEASE PLAYS FOREVER, and `preview` off does
  not stop it.** Stacks sounds for three reasons -- its own `preview` loop, a
  `play` hold, and MOVE'S TRANSPORT while Run is on -- so a transport button
  wired to `preview` was answering a different question from the playhead
  beside it: press stop while Move drives the module and the music continues.
  Both surfaces report `prog.running` now, and Stop clears `play` first and
  then whichever transport is actually running (`run` off for Move's, which
  Play puts back). The latch is the real hazard: a pointer lost to a
  re-render, a tab closed mid-press, or a claimed CC whose release the shim
  withheld leaves `hold_run` set for the session with nothing on either screen
  able to stop it -- so the surface releases the hold in `onClose` and the
  panel does on `pagehide`/`visibilitychange`.
- **A SURFACE THAT OWNS THE SCREEN FOR MINUTES MUST ASK AGAIN, and a frame is
  not the place.** `draw` and `tick` read nothing; `onPoll` is the event on a
  metronome, 250ms, and reads a handful of keys, never a page -- `prog` (one
  read, the whole picture), `status`, `patterns` -- and buys the dozen-read
  bank refresh only when that picture says the values changed, which is why
  the staleness signature must exclude every time-varying field or it says
  "stale" four times a second forever.
- **A long press acts on the RELEASE, not at the threshold.** `tick` sees the
  threshold pass and DRAWS that; the action waits for the release in
  `onMidi`. Which is no bad thing -- the screen can say what the release will
  do while you are still holding.
- **A rule enforced at call sites is a rule that will be missed.** "Grow the
  clip to hold the music" was four copies of the same arithmetic, so `len`,
  `off`, `insert` and `remove` never got it and each silently pushed the
  progression past the end of the clip -- playing, drawn, and absent from the
  stamp. It runs once at the parameter door now, as does undo capture, with
  the exemptions written down beside them.

`tests/test_stacks_shapes.sh` pins the shape table against the contract
(the 38 shapes are declared in both C and `module.json`), the family runs being
contiguous, the manifest being a tool's, and both realtime rules above.
`test_stacks_tool_inject.sh` drives a lap through the tool entry. `test_stacks_clip_length.sh` walks
every door that changes how much music there is.
