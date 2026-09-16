# Stacks

A chord-progression tool for [Ableton Move](https://www.ableton.com/move/)
running [Schwung](https://github.com/charlesvestal/schwung), opened from its
Tools menu (Shift + Volume + Step 13).

Write a progression, hear it on a Move track, and let Move record it into a
clip. It is edited from two places at once — the Move's own screen and a
Remote UI panel in the browser's Tool tab — and both show the same selection,
the same playhead and the same transport.

* **38 chord shapes** in six families, 24 rhythm figures, 10 voicing colours,
  and a library of 140+ progressions filed under 17 genres — house, deep and
  melodic house, lo-fi, cinematic and the melancholic end of pop among them.
* **The staff is the picture**: pitches, the rhythm as the hits it plays, the
  chord names along the foot, and a keyboard down the side that lights the
  notes as they sound.
* **Sixteen pattern slots**, and you are always in one — tap a slot to switch,
  and everything you write belongs to it. Buffers, not clips: nothing touches
  the set, and each pattern carries its own clip length.
* **Stamp to a clip** by arming one lap: press Back, then Record on Move, and
  Move records what you just heard, on its own downbeat. Stacks keeps playing
  while parked; reopen it from the Tools menu to carry on where you were.
* **Track 1–4** picks the Move track the chords address (MIDI channel 1–4).
* **It remembers.** The progression, all sixteen patterns and every setting are
  saved to `/data/UserData/schwung/stacks/state.json` a moment after each edit
  and again on exit, and are there when you open it next — across reboots and
  reinstalls.

## Install

Get `stacks-module.tar.gz` from the releases page and install it from the
Schwung web manager (`http://move.local:7700`) — it lands under
`modules/tools/stacks` — or build and push it yourself:

    scripts/build.sh
    scripts/install.sh --host=move.local --reload

## Building

`scripts/build.sh` cross-compiles the DSP for aarch64 in Docker and writes
`dist/stacks-module.tar.gz`. Set `CROSS_PREFIX` to build without Docker.

## Tests

    for t in tests/*.sh; do bash "$t"; done

They compile the DSP natively and run it — the shape table against the
contract, the clip length through every door that changes it, both `prog`
parsers over one corpus, the chords' timing counted in MIDI clocks from Move's
downbeat, a lap through the tool entry's inject path, the `ui.js` lifecycle
over stubbed overtake globals, and the Tool tab's `state`/`rui_poll` contract.

## Requires

Schwung on the device with overtake tools and the Tool tab (1.4 or later).
Nothing is patched in the host. One known gap: the Record button does not pass
through to Move while a tool is on screen, which is why a stamp is armed here
and recorded after Back — see `docs/plans/2026-09-15-overtake-port-design.md`.
