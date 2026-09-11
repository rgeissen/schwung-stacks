# Stacks

A chord-progression MIDI FX for [Ableton Move](https://www.ableton.com/move/)
running [Schwung](https://github.com/charlesvestal/schwung).

Write a progression, hear it, and stamp it into a Move clip. It is edited from
two places at once — the Move's own screen and a Remote UI panel in a browser —
and both show the same selection, the same playhead and the same transport.

* **38 chord shapes** in six families, 24 rhythm figures, 10 voicing colours,
  and a library of 60+ progressions filed by genre.
* **The staff is the picture**: pitches, the rhythm as the hits it plays, the
  chord names along the foot, and a keyboard down the side that lights the
  notes as they sound.
* **Sixteen pattern slots**, and you are always in one — tap a slot to switch,
  and everything you write belongs to it. Buffers, not clips: nothing touches
  the set, and each pattern carries its own clip length.
* **Stamp to a clip** by arming one lap: Move records what you just heard,
  on its own downbeat.

## Install

Get `stacks-module.tar.gz` from the releases page and install it from the
Schwung web manager (`http://move.local:7700`), or build and push it yourself:

    scripts/build.sh
    scripts/install.sh --host=move.local --reload

## Building

`scripts/build.sh` cross-compiles the DSP for aarch64 in Docker and writes
`dist/stacks-module.tar.gz`. Set `CROSS_PREFIX` to build without Docker.

## Tests

    for t in tests/*.sh; do bash "$t"; done

They compile the DSP natively and run it — the shape table against the
contract, the clip length through every door that changes it, both `prog`
parsers over one corpus, and the chords' timing counted in MIDI clocks from
Move's downbeat.

## Requires

Schwung on the device. Two of the cross-surface features need host capabilities
that are offered upstream as pull requests; see CLAUDE.md.
