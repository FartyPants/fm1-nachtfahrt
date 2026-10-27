# BERLIN for the M-VAVE FM-1

BERLIN turns the M-VAVE FM-1 into a generative Berlin School machine, after the 1970s music of
Tangerine Dream, Klaus Schulze and Ashra: long, hypnotic pieces, repeating synthesizer sequences,
slowly evolving textures, a strong sense of space. It plays itself from the moment it's switched on.

- a **Sequence** of up to 16 steps on a plucked analog-style voice (2600, Moog, Synthi, Pulse, Glass),
  its filter wandering slowly, its notes **repeated** at the echo's times (a dotted eighth by default, the
  classic Berlin School echo), so sparse notes fill in the spaces;
- **Sequence 2**, often a step shorter, so the two drift apart and meet again;
- a **Drone**: a chord on two banks of voices (Organ, Strings, Choir, Saws, Glass) with a pedal under it, a
  phaser and a slow walk from chord to chord;
- a **Lead**: one gliding voice (Odyssey, Mini, Flute, Theremin, Synthi) playing long, dreamy phrases deep in
  the reverb and the echo, which you can also play on the white keys, in the scale;
- an **Atmosphere**: Wind, Surf, Space, Air or Tape (or none);
- an **arc**: a piece emerges from the drone, flows, lets the Lead take over, drifts, and ends (a crash into
  the echo, a supernova of feedback, or a fade) before it begins again.

You don't play it, you steer it: pick a piece, change its key and tempo, thin or fill the sequences, push the
echo, mute and solo the layers, play the Lead.

BERLIN is a fork of [DUB-1](https://github.com/sbegueria/fm1-dub), itself a fork of
[AMB-1](https://github.com/charlesvestal/fm1-amb), Charles Vestal's generative ambient firmware for the FM-1. It
keeps their platform, panel, echo, plate and much of the engine.

**Status: 0.1, in development.** It runs in the browser emulator and in the host simulator; it hasn't been
built for or run on an FM-1 yet (see [PLAN.md](PLAN.md)).

## The pieces

| # | Piece | After | Key, tempo | |
|---|---|---|---|---|
| 1 | Timewind | Schulze, *Bayreuth Return* | D minor, 96 | One sequence, an organ through the phaser, a long emergence, ends in a crash |
| 2 | Wahnfried | Schulze, *Wahnfried 1883* | B♭ Dorian, beatless | Drifting layers, the roots walking with no home, a screaming lead |
| 3 | Phaedra | Tangerine Dream, *Phaedra* | D minor, 149 | A Moog bass line of 16 over a second line of 15, flutes, much filter movement |
| 4 | Rubycon | Tangerine Dream, *Rubycon* | F minor, 144 | Dark organ, two sequences of 8 and 7 rising out of the drone |
| 5 | Stratosfear | Tangerine Dream, *Stratosfear* | E minor, 138 | Brighter and driving, two sequences interlocking, the Lead forward |
| 6 | Sunrain | Ashra, *New Age of Earth* | D major, 130 | The most hopeful: both echo repeats at full, bright strings, surf |
| 7 | Mirage | Schulze, *Mirage* | F♯ minor, 95 | Icy glass sequences, a deep phaser, a long reverb, sparse |
| 8 | Cyborg | Schulze, *Cyborg* | C Phrygian, beatless | Early and raw: deep organ drones beating, no sequence |

The names are working titles and the keys and tempos are starting points (from analysis sites, to tune by ear).
BERLIN is not connected with any of these artists.

## In short

| | |
|---|---|
| White keys | Play the Lead, in the piece's scale (the key it sounds is lit); black keys do nothing |
| PLAY · REC · EDIT | Play / rest · a tap: hold (the echo repeats what it holds), held: the effects grid (16 effects on the white keys) · a tap: the Chords page, held: the next chord |
| SEL | PERFORM: the knobs Filter, Wet, Energy, Sweep; white 1–5 mute the Drone, Sequence, Lead, Sequence 2, Atmosphere; black 1–5 solo them; white 6 held: the levels; white 7–11 held: Sweep, Open, Shut, Drop (both sequences out), Echo. Held: Lock |
| HOME | The Berlin page (Filter, Evolve, Motion, Drift); again, the Key page (Tempo, Root, Mode, Transpose) |
| ENV | The Drone (Pedal, Reso, Phaser, Attack); again, its sound (Colour, Release, Reverb, Echo) |
| ARP | The Sequence (Density, Octave, Length, Grid); again, its sound (Cutoff, Decay, Reverb, Echo); again, its Repeats (Repeat A, A time, Repeat B, B time) |
| LFO | The Lead (Phrases, Octave, Glide, Vibrato); again, its sound (Tone, Attack, Reverb, Echo) |
| SEQ | Sequence 2 (Chance, Octave, Length, Level); again, its sound; again, the Pattern page (write the steps on the keys) |
| SAVE | The Atmosphere (Density, Width, Drift, Air); again, its sound (Colour, Ring, Reverb, Echo) |
| FX | Space (Echo, Echo time, Feedback, Reverb); again, the Echo page (Blur, Tone, Width, Size) |
| GLO | Setup (Tune, MIDI out, Lights, Save); again, the Arc (Emerge, Seq2 in, Lead in, Arc). Held: save; held on: the piece back to its recipe |
| ALGORITHM | On a layer's page, its voice (the Atmosphere: its character) |
| PRESETS | The piece |
| OCT− / OCT+ | On a layer's page, its level; both together, quickly: mute it |

The [manual](docs/MANUAL.md) has the rest. [PLAN.md](PLAN.md) has what's done and what's still to do.

## Building and testing

The toolchain is the same as DUB-1's (see [BUILDING.md](BUILDING.md) and [EMULATOR.md](EMULATOR.md)):

```
tests/run_tests.sh      # the engine, the platform pieces, the app in a simulator, the web builds
web/emu/build.sh        # the browser emulator: build/emu (serve it, open index.html)
./build.sh              # build/amb.fwsc, for the FM-1
python3 tools/fm1_install.py build/amb.fwsc     # install it over USB
```

`build/host/amb_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.amb`), with audio and screenshots coming out; `build/host/en_render OUT.wav SECS piece=N`
renders the engine alone.

## Credits

BERLIN is a fork of [DUB-1](https://github.com/sbegueria/fm1-dub), a fork of
[AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal (GPL-3.0-only): the echo chain, the plate,
the pieces system, the chord walk, the UI and the platform come from there. The reverb is Jon Dattorro's plate
("Effect Design, Part 1", JAES 1997), as in [FoMni-1](https://github.com/charlesvestal/fm1-fomni); the granular
cloud (Freeze) follows the design of CLOUD in [JIANT](https://github.com/juanjiant-bit/JIANT) (GPL-3.0); the Tape
atmosphere is DUB-1's crackle, after Bahadirhan Kocer's "Using Resonators in Live". The way the sequence is
played (a root accented, sparse notes, a dotted-eighth repeat as loud as the note, two delays) owes much to
Synth Seeker's videos on Berlin School sequencing. The platform (hardware layer, USB, update loader, storage,
installer) is [Felucca](https://github.com/hugelton/Felucca)'s by Leo Kuroshita (Hügelton Instruments), by way
of [X0X](https://github.com/charlesvestal/fm1-x0x), FoMni-1, AMB-1 and DUB-1. The idea of running FM-1 firmware
in the browser comes from [Groove OS](https://www.groove-os.com/). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. BERLIN is not affiliated with M-VAVE or Hügelton Instruments.
