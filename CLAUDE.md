# CLAUDE.md — Stacks

A chord-progression MIDI FX for Ableton Move, running under
[Schwung](https://github.com/charlesvestal/schwung). Three surfaces, one module:

    src/dsp/stacks.c    the DSP -- harmony, the scheduler, the clip reader/stamp
    src/canvas.js       the Move's own screen (a 128x64 takeover) and its grid cell
    src/web_ui.html     the Remote UI panel, served by schwung-manager in a browser

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
Schwung, never a local patch. Two such capabilities exist today and both are
general, not Stacks-specific:

  * `canvas_overlay.onPoll` -- a takeover's only way to read a param between
    inputs (`draw`/`tick` have the accessors stripped, deliberately);
  * the Remote UI's refresh of `viz.extra_keys` on change plus a heartbeat --
    derived values are named by no write, and the transport by nothing at all.

Without them the module still works; the two surfaces just stop following each
other live.

## Song.abl is READ-ONLY everywhere except this module's opt-in stamp

The `stacks` MIDI FX (`src/`) is the first thing here
that both reads and writes Move's set file, and the two directions are not
symmetric. Reading is free; **writing fights Move**, which holds the set in
memory and autosaves over it — so `stamp_mode` defaults to **rec arm** (inject
into a record-armed track and let Move's own code write the clip) and the
direct write is opt-in. The write is a **text splice** of the clip's `notes`
array into a temp file plus a rename, never a re-serialisation: a set carries a
great deal this module does not understand, and re-emitting it is how that gets
lost.

Three rules it exists to demonstrate, all of them things an author otherwise
gets wrong:

- **A MIDI FX entry point is the SPI callback, so the parse is not there.**
  `set_param` bumps an atomic counter; a worker created with
  `PTHREAD_EXPLICIT_SCHED` + `SCHED_OTHER` parses and publishes with a
  release-store that `tick` acquires. Inheriting the callback's FIFO 70 starves
  Move's `Link Main` at 35. This is the granny bug avoided by construction.
- **A module page can be a PICTURE, and it costs ONE read.** `view` is
  `type: "canvas"` + `as_page`, and the whole progression — names, lengths,
  offsets, resolved pitches, the scale mask — arrives as a single
  `viz.extra_keys` value. The C side publishes **resolved** notes so voicing is
  not implemented twice and the staff cannot draw a chord the synth never
  played.
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
- **REC-ARM STAMPING HAD NO STATE, so it read as a dead button.** It does not
  write a clip -- it ARMS, waits for Move's downbeat and plays exactly one lap
  for Move to record -- and the module set no `status` for any of that, so the
  panel invented an armed flag of its own, started when IT pressed the button.
  A stamp armed from the MOVE therefore lit nothing anywhere ("stamping on the
  Move doesn't work", for a mechanism that was working the whole time), and a
  transport stop cancelled the arm while the browser went on saying ARMED.
  `status` publishes `armed` now, both surfaces read it, the takeover blinks a
  mark and toasts `REC + PLAY` (it cannot press Record itself -- the shim
  claims CC 118). Arming also PARKS `run` so nothing sounds before the
  downbeat, and **every exit must put it back**: a cancel, and the take itself.
  Stopping after one lap REQUIRES `run = 0` (the sequencer follows it, and the
  alternative is stopping Move), but leaving it off is an action silently
  changing a setting -- the take ends and from then on **Move's play button
  does nothing at all**, with nothing on either screen saying why. Run is owed
  back and returns when that take's transport stops. Pressing the same control
  while armed CANCELS, because a state whose only exit was Move's stop button
  is a state the surface that entered it could not leave.
- **A PATTERN IS THE BUFFER, NOT A CLIP.** Sixteen slots holding the
  progression, the clip LENGTH it was written against and the voice grouping
  it was voiced with -- nothing touches Song.abl, so storing is free and Move
  cannot overwrite it. Three rules the obvious version gets wrong: recall
  brings the BARS with it (a twelve-bar phrase recalled into a four-bar clip
  otherwise leaves two thirds of itself outside, playing and drawn and absent
  from any stamp); `pat_cur` means "where this buffer CAME FROM", so deleting
  the current slot clears it, because a lit button over an empty slot is a lie
  the eye cannot check; and store/delete capture NO undo while recall does --
  an undo stack filling with "you pressed store" pushes the edit you wanted to
  undo off the end of it. They ride in the `state` blob behind an `@` marker so
  a blob written before patterns existed still loads, and `patterns` is one
  read -- a mask and the current slot -- that draws all sixteen buttons on both
  surfaces. `tests/test_stacks_patterns.sh`.
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
  able to stop it -- so the takeover releases the hold in `onClose` and the
  panel does on `pagehide`/`visibilitychange`.
- **Shift belongs to the HOST on a claimed CC.** Shift+Copy / Shift+Delete are
  Schwung's snapshot and recall, and the shim withholds a shift-held press from
  a claimed CC entirely -- "the module gets the BARE buttons only". A
  `shiftHeld()` branch inside a Copy/Undo/Delete handler is unreachable code
  that reads as a working feature, which is exactly what shipped: Double Length
  on Shift+Copy and Redo on Shift+Undo were both documented and both dead,
  while Mute+Copy and Mute+Undo worked by accident. Use MUTE, which is
  forwarded. The JOG is unaffected -- a different claim path, so Shift+Jog and
  Shift+Click do work. `tools/check_edit_cc_modifier.py`.
- **Shared controls are RECONCILED from the screen, never bookkept** — the CC
  claim and `host_pad_block` are both GLOBAL, and a UI that strands one costs
  every later module that control until a reboot. `reconcilePadBlock()` only
  ever CLEARS, tests whether the display is SHOWING (not just the view), and
  RESTORES Move's pad LEDs rather than darkening them. `docs/SHADOW_UI.md`.
- **A TAKEOVER MUST BE ABLE TO ASK AGAIN, and a frame is not the place.**
  `draw` and `tick` are DRAW_PATH_HOOKS with the accessors stripped, so a
  takeover used to read only on its own input -- and it owns the screen for
  minutes while a worker finishes or a **Remote UI panel edits the same module
  from a browser**. `onPoll` is the event on a metronome: optional, full ctx,
  at most one call per `CANVAS_POLL_MS` (250ms, ~1% of the budget). Read a
  handful of keys there, never a page -- stacks reads `prog` (one read, the
  whole picture) and buys its dozen-read bank refresh only when that picture
  says the values changed, which is why the staleness signature must exclude
  every time-varying field or it says "stale" four times a second forever.
- **A long press cannot ACT at the threshold.** `tick` is a DRAW_PATH_HOOK, so
  the runtime strips getParam/setParam from its context: it can see the
  threshold pass and DRAW that, but the action waits for the release, where
  `onMidi` has setParam again. Which is no bad thing -- the screen can say what
  the release will do while you are still holding.
- **A rule enforced at call sites is a rule that will be missed.** "Grow the
  clip to hold the music" was four copies of the same arithmetic, so `len`,
  `off`, `insert` and `remove` never got it and each silently pushed the
  progression past the end of the clip -- playing, drawn, and absent from the
  stamp. It runs once at the parameter door now, as does undo capture, with
  the exemptions written down beside them.

`tests/test_stacks_shapes.sh` pins the shape table against the contract
(the 38 shapes are declared in both C and `module.json`), the family runs being
contiguous, and both realtime rules above. `test_stacks_clip_length.sh` walks
every door that changes how much music there is.
