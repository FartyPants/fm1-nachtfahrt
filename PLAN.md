# NACHTFAHRT: plan and status

NACHTFAHRT is a generative Berlin School firmware for the M-VAVE FM-1, forked from DUB-1 (itself from AMB-1).
(Called BERLIN while it was built, too close to Berlinan's name; the code keeps BERLIN.)

## Status: 0.1, in the emulator

Done, with the whole suite passing (`tests/run_tests.sh`, gate `tools/suite_gate.sh` with no allowed failures):

- **The Sequence** (`seq.h`/`seq.c`): steps with gates, accents, octave jumps, skips; grown from a core cell; varied
  rarely (Evolve, Motion); transposed by the chord (Parallel, Diatonic, Bed); **Repeats** A and B at the echo's
  times, as notes; Density thinning by priority; eighths or sixteenths; the Pattern page.
- **Sequence 2**, its own length (by default a step shorter), Chance, Sub voice.
- **The Drone**: two banks (chord + pedal), five voices, ensemble, phaser.
- **The Lead**: one gliding voice, phrases, five voices, dreamy sends (reverb and echo 90); the white keys play it.
- **The Atmosphere**: None, Wind, Surf, Space, Air, Tape.
- **The arc**: Emerge (only while the Intros open), Flow, Solo, Bridge; the swell into Flow; no endings (a piece
  repeats for ever); a piece changed starts its own arc.
- **Filter motion**:
  no hidden levels (Emerge, Seq2 in, Ending and the level fades are gone); each layer's filter wanders, opens
  (Intro) and takes the Bridge's move as an offset from its knob; a Move page (Width, Time, Rate, Intro) for each
  layer, the Sequence's Width being Sweep; the Arc page (Lead in, Bridge, Arc, the fourth knob empty); the motion
  shown (meter ticks, the dot on the brightness knob's ring, "MOVING DARKER, -n OCT", the screen reader).
- **The black keys** play eleven effects while held (Take-off, Supernova, Twitter, Repeat, Swell, Freeze, Octave,
  Return, Rise, Close, Transpose).
- **PERFORM finished**: a muted Lead and a muted Drone are silent (MIDI too); Energy drives Repeat B; Wet moves
  every layer's echo send.
- **The screen**: Cosmic on black (starfield, orbits round the chord, five meters, two sequence lanes, "CRANKY
  MAN"), the launch screen "NACHTFAHRT / by CRANKY MAN"; DUB-1's leftovers (drum and rhythm pages, the scrolling
  display, the spectrum and wave displays) and its dead engine code are gone.
- **The echo**: its default time is 1/4. (a dotted quarter), Repeat A stays 1/8.; the Sequences' echo sends are
  louder.
- **The pages** unified across the layers; PERFORM for NACHTFAHRT; the effects grid re-themed (Repeat … Supernova);
  the screen reader's words; the right meter shows the echo.
- **The eight pieces**: Timewind, Wahnfried, Phaedra, Rubycon, Stratosfear, Sunrain, Mirage, Cyborg, each with its
  moves (Timewind's Sequence opens over 32 bars, Rubycon's Sequence 2 after its Sequence, Stratosfear and Sunrain
  straight in, Wahnfried and Cyborg moving on the Drone).
- **Storage**: one project, magic "BRL1", format 7; other firmwares' saves and older NACHTFAHRT formats are not read.
- **Docs**: README, MANUAL, this file; the manual's screenshots (`tests/listen/docshots.amb`) and panel drawing,
  brought up to date with the filter motion.
- **The user's batch of 2026-10-28**: the name NACHTFAHRT (by CRANKY MAN); SELECT a layer's level on its pages
  (nothing elsewhere: no paging, no tempo), OCT− / OCT+ the Sequence's, the Lead's and Sequence 2's octave (lit away
  from the piece's own); the lengths Timewind 16/15, Sunrain 8/7, Mirage 12/11, Rubycon's octaves −2 / 0, Cyborg's
  Sequence at 20; Motion no longer changes Sequence 2's length; Sequence 2's octave moves with Motion, a section at a
  time. CPU after it (baseline.sh, medians): Timewind 0.74, Wahnfried 0.60, Phaedra 0.77, Rubycon 0.68, Stratosfear
  0.78, Sunrain 0.85, Mirage 0.93, Cyborg 0.58, stressed 0.86.

## CPU (host proxy)

Budget: **1.10%** of real time (DUB-1's heaviest piece × 1.10; [docs/BASELINE.md](docs/BASELINE.md)).
`tests/listen/baseline.sh`, three runs, medians (2026-10-28, after the user's batch): Timewind 0.74, Wahnfried 0.60,
Phaedra 0.77, Rubycon 0.68, Stratosfear 0.78, Sunrain 0.85, Mirage 0.93, Cyborg 0.58; stressed (Sunrain, Density,
both repeats, Sequence 2, Phaser and Phrases at 100) 0.86 (before the filter motion: 0.58 .. 0.83, stressed 0.85); the effects scenario, with all sixteen held at once in it, 0.98. All within the
budget, so nothing was cut.

## Next

**A Lead that remembers** (six steps, three done): a small motif memory for the Lead,
call-and-answer phrasing, callbacks, intensity from the arc, white-key theme capture, and a **Memory** knob on
the Arc page's fourth knob (not built yet: the knob is empty until then).

## To do

1. **The device build** (needs the user's go-ahead to download the JieLi toolchain and the AC79 SDK into WSL):
   `tools/get_toolchain.sh`, `./build.sh` → `build/amb.fwsc`; `tools/build.py`'s notes must show RAM ≤ 96 KB and
   pool headroom ≥ 8192 B (if not: the ensemble's buffer first). Then install and read the Setup page's CPU
   now/peak and dropouts per piece:

   | Piece | CPU now / peak | Dropouts |
   |---|---|---|
   | Timewind | | |
   | Wahnfried | | |
   | Phaedra | | |
   | Rubycon | | |
   | Stratosfear | | |
   | Sunrain | | |
   | Mirage | | |
   | Cyborg | | |

2. **Tuning the pieces by ear** (the user): tempos, keys, levels, each piece's Moves (the Intros, Widths and Bridge) and Arc lengths. Three-minute renders
   of each: `tests/listen/pieces.amb`.

## Open proposals (to decide with the user)

- **SELECT as the selected layer's volume** — done: SELECT is the shown layer's level, OCT- / OCT+ its octave.
- **MIDI in still edits the scale** (channel 1 notes in/out); keep it, or let MIDI in play the Lead like the keys.
- **A clean minor triad at the arc's peak** (spec §5) is not built; the peak is the Sequence's filter
  rise (the Arc knob), nothing more.
- **Phaedra's tuning drift**: the record's sequences drift out of tune; NACHTFAHRT has no pitch wobble yet (the Moog
  voice's two oscillators only).
- **Mirage's occasional lift to major** and **Wahnfried's six layers**: not built as such (the Drone's two banks,
  the pedal and the Lead stand in).

## Known minor issues

- `seq_next` doesn't clamp the length to 16 (the engine never passes more); a few edge cases of the sequence
  functions are correct by reading but untested.
- The Lock card still says "the groove kept" (DUB-1's words).
- Some DUB-1 scenarios that covered help cards were removed with the pages they tested. (The nudges' tables were
  DUB-1's, so a nudge, or Evolve over 40, moved the Lead's sends, the Ending, Transpose and the Grid: fixed,
  `tests/scenarios/nudge.amb`.)
