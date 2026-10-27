# BERLIN: plan and status

BERLIN is a generative Berlin School firmware for the M-VAVE FM-1, forked from DUB-1 (itself from AMB-1).
The design is [docs/superpowers/specs/2026-10-27-berlin-design.md](docs/superpowers/specs/2026-10-27-berlin-design.md);
the implementation plan is [docs/superpowers/plans/2026-10-27-berlin.md](docs/superpowers/plans/2026-10-27-berlin.md).
Research: `I:\FM1\reports\Berlin School synth techniques.md` (outside the repository).

## Status: 0.1, in the emulator

Done, with the whole suite passing (`tests/run_tests.sh`, gate `tools/suite_gate.sh` with no allowed failures):

- **The Sequence** (`seq.h`/`seq.c`): steps with gates, accents, octave jumps, skips; grown from a core cell; varied
  rarely (Evolve, Motion); transposed by the chord (Parallel, Diatonic, Bed); **Repeats** A and B at the echo's
  times, as notes; Density thinning by priority; eighths or sixteenths; the Pattern page.
- **Sequence 2**, its own length (by default a step shorter), Chance, Sub voice.
- **The Drone**: two banks (chord + pedal), five voices, ensemble, phaser.
- **The Lead**: one gliding voice, phrases, five voices, dreamy sends (reverb and echo 90); the white keys play it.
- **The Atmosphere**: None, Wind, Surf, Space, Air, Tape.
- **The arc**: Emerge, Flow, Solo, Drift; the Sweep; the swell; endings Crash, Supernova, Fade; a piece changed
  starts its own arc.
- **The pages** unified across the layers; PERFORM for BERLIN; the effects grid re-themed (Repeat … Supernova);
  the screen reader's words; the right meter shows the echo.
- **The eight pieces**: Timewind, Wahnfried, Phaedra, Rubycon, Stratosfear, Sunrain, Mirage, Cyborg.
- **Storage**: one project, magic "BRL1", format 6; other firmwares' saves and older BERLIN formats are not read.
- **Docs**: README, MANUAL, this file; the manual's screenshots (`tests/listen/docshots.amb`) and panel drawing.

## CPU (host proxy)

Budget: **1.10%** of real time (DUB-1's heaviest piece × 1.10; [docs/BASELINE.md](docs/BASELINE.md)).
`tests/listen/baseline.sh`, three runs, medians: Timewind 0.69, Wahnfried 0.64, Phaedra 0.71, Rubycon 0.60,
Stratosfear 0.74, Sunrain 0.83, Mirage 0.79, Cyborg 0.58; stressed (Sunrain, Density, both repeats, Sequence 2,
Phaser and Phrases at 100) 0.85; the effects scenario, with all sixteen held at once in it, 0.98. All within the
budget, so nothing was cut.

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

2. **Tuning the pieces by ear** (the user): tempos, keys, levels, Emerge and Arc lengths. Renders of three minutes
   each are in `I:\FM1\berlin_t13_pieces\`, with Phaedra at 75 and Rubycon at 72 to compare with their readings.
3. **The dub's dead code** (DUB-1's stabs, bass, bells, drums and phrase plan, silenced behind `EN_DUB 0`):
   verified silent (the eight pieces hash the same to the bit with them stubbed out, `build/tmp/inert.c`); to delete
   when convenient, and before 1.0 if the device build is short of flash or RAM.

## Open proposals (to decide with the user)

- **SELECT as the selected layer's volume** (spec §9b), freeing it from paging.
- **MIDI in still edits the scale** (channel 1 notes in/out); keep it, or let MIDI in play the Lead like the keys.
- **A clean minor triad at the arc's peak** (spec §5) is not built; the peak is the filter's top and the
  Atmosphere settling back.
- **Phaedra's tuning drift**: the record's sequences drift out of tune; BERLIN has no pitch wobble yet (the Moog
  voice's two oscillators only).
- **Mirage's occasional lift to major** and **Wahnfried's six layers**: not built as such (the Drone's two banks,
  the pedal and the Lead stand in).
- **Ending on the panel**: each piece's Ending (Crash, Supernova, Fade) is in its recipe only.

## Known minor issues

- `seq_next` doesn't clamp the length to 16 (the engine never passes more); a few edge cases of the sequence
  functions are correct by reading but untested.
- The Lock card still says "the groove kept" (DUB-1's words).
- Some DUB-1 scenarios that covered help cards were removed with the pages they tested. (The nudges' tables were
  DUB-1's, so a nudge, or Evolve over 40, moved the Lead's sends, the Ending, Transpose and the Grid: fixed,
  `tests/scenarios/nudge.amb`.)
