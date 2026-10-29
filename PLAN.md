# NACHTFAHRT: plan and status

NACHTFAHRT is a generative Berlin School firmware for the M-VAVE FM-1, forked from DUB-1 (itself from AMB-1).
(Called BERLIN while it was built, too close to Berlinan's name; the code keeps BERLIN.)

## Status: 0.9.2 released, running on the FM-1

| Version | Date | Identity | What |
|---|---|---|---|
| 0.9.0 | 2026-10-28 | FM-1_8000900 | the first public build |
| 0.9.1 | 2026-10-28 | FM-1_8000901 | PERFORM's Filter, Wet and Energy move by themselves; the chord's ring; SELECT on the home page = Energy |
| 0.9.2 | 2026-10-28 | FM-1_197000902 | the Lead that remembers, answers and listens (below); its own identity; saves reset (format 8) |

NACHTFAHRT's identity is FM-1_197XXYYZZ ("197x"): no other firmware in The Firmware Hub's catalogue matches it (the
FM-1_8... before was shared with DUB-1 and others). Releases: `./build.sh --release X.Y.Z`, `tools/make_pages.py`,
the site on the gh-pages branch, a GitHub release per version with its `.fwsc`.

On the FM-1 (the Setup page's CPU, the highest peak found): 0.9.0 about 65%, Sunrain 73%; 0.9.1 most pieces under 70,
many under 60 (Timewind, Rubycon, Wahnfried), an occasional 72; Cyborg about 40%, 44% while the Lead is played on
the keys (a sounding Lead voice is about 4 points). The engine sheds (thins its sound for half a second) above 75%
in a block, so the headroom is about 3 points: every new sound has to pay for itself. 0.9.2: installed and tested,
its peak not read yet.

Done, with the whole suite passing (`tests/run_tests.sh`, gate `tools/suite_gate.sh` with no allowed failures):

- **The Sequence** (`seq.h`/`seq.c`): steps with gates, accents, octave jumps, skips; grown from a core cell; varied
  rarely (Evolve, Motion); transposed by the chord (Parallel, Diatonic, Bed); **Repeats** A and B at the echo's
  times, as notes; Density thinning by priority; eighths or sixteenths; the Pattern page.
- **Sequence 2**, its own length (by default a step shorter), Chance, Sub voice.
- **The Drone**: two banks (chord + pedal), six voices (Organ, Strings, Horn, Saws, Glass, Choir: Horn was the old "Choir"; Choir, appended, is a formant choir), ensemble, phaser.
- **The Lead**: one gliding voice, five voices, dreamy sends (reverb and echo 90); calls and answers that remember
  their motifs and the piece's theme (Memory, below); the white keys play it and write its theme: a figure of three
  notes or more (up to 16, within four bars) and a two-beat pause is answered a beat later, in its rhythm with
  related notes of the Lead's own; the Lead's next ideas grow out of it, straying further (about 90 s at Memory 50),
  and the figure itself comes back at the theme's moments. **FORGET** (PERFORM, the top key, G5) wipes the Lead's
  memory.
- **The Atmosphere**: None, Wind, Surf, Space, Air, Tape.
- **The arc**: Emerge (only while the Intros open), Flow, Solo, Bridge; the swell into Flow; no endings (a piece
  repeats for ever); a piece changed starts its own arc.
- **Filter motion**:
  no hidden levels (Emerge, Seq2 in, Ending and the level fades are gone); each layer's filter wanders, opens
  (Intro) and takes the Bridge's move as an offset from its knob; a Move page (Width, Time, Rate, Intro) for each
  layer, the Sequence's Width being Sweep; the Arc page (Lead in, Bridge, Arc; its fourth knob the Lead's Memory);
  the motion shown (meter ticks, the dot on the brightness knob's ring, "MOVING DARKER, -n OCT", the screen reader).
- **The black keys** play eleven effects while held (Take-off, Supernova, Twitter, Repeat, Shimmer, Freeze, Octave,
  Return, Rise, Close, Transpose).
- **PERFORM finished**: a muted Lead and a muted Drone are silent (MIDI too); Energy drives Repeat B; Wet moves
  every layer's echo send. Filter, Wet and Energy move by themselves with Motion and the Arc (a hand turn holds
  until the next section); a piece change returns them to 0; SELECT on the home page turns Energy.
- **Shimmer** (the fifth black key, where Swell was): while held the plate's tail climbs an octave at a time, back
  into the plate, darkened and bounded (never self-sustaining; +3 to +5 dB held); zero cost at rest. Not released
  yet.
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
- **Storage**: one project, magic "BRL1", format 8 (8: the Lead's Memory appended); other firmwares' saves and older
  NACHTFAHRT formats are not read (a format-7 save, 0.9.1's, loads as the recipes).
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
budget, so nothing was cut. After the Lead's memory (2026-10-28, a busier session): 0.68 .. 1.02 (Mirage),
stressed 0.94, all within the budget ([docs/BASELINE.md](docs/BASELINE.md)).

## Next

**A Lead that remembers: done** (six steps). The Lead plays in pairs, a call and its answer a beat or two later;
it remembers four motifs and plays one for a block of pairs, then keeps it (Memory's chance), varies it or finds a
new one; its first is the piece's theme, back when Solo begins, after the rise peaks, at each Bridge and after six
blocks without it; intensity follows the arc (more notes, shorter and higher towards the peak, a run to a close);
three white-key notes or more and a pause become the theme. **Memory** is the Arc page's fourth knob (0–100,
"How much the Lead keeps its ideas"); the pieces: Timewind 55, Wahnfried 40, Phaedra 75, Rubycon 45, Stratosfear
85, Sunrain 60, Mirage 50, Cyborg 35 (the starting values, to tune by ear). CPU after it (baseline.sh, medians):
Timewind 0.82, Wahnfried 0.78, Phaedra 0.83, Rubycon 0.70, Stratosfear 0.84, Sunrain 0.92, Mirage 1.02, Cyborg
0.68, stressed 0.94 (the same session before Memory went on the panel: 0.80 0.75 0.83 0.72 0.84 0.90 1.03 0.65,
0.95: the host's noise). Still to hear: the user's ear on each piece's Memory; whether the theme (about 40% of the calls in the
first minutes) is too much of a loop. (The user, listening: "much more melodic and purposeful".)

**The voices: under way.** The user's direction: no new layer playing alongside, more voices to choose from in the
layers there are. In this order, each measured in the heaviest pieces (Mirage, Sunrain) before it goes to the FM-1:

1. **Shimmer** in place of Swell, and the Drone's "Choir" renamed **Horn**: done (not released).
2. **Pluck** (Karplus-Strong, after fm1_open's MIT pluck) for Sequence and Sequence 2, appended to their voice
   lists so no save changes: in progress.
3. **Choir**, a formant voice whose vowels move slowly, as the Drone's sixth voice (after Felucca's and Skiff's VOICE
   engine, rewritten in float). The costly one: the Drone sounds two chords at once while it crosses over, so it
   needs a light design, or room made first.
4. **The Atmospheres**: pink and brown noise in Wind and Surf (the FM-1's speaker is bright), a drifting band-pass
   for Wind, and a new **Rain** (soft resonant drops), their levels matched to the user's tuning (Wind 7, Surf 5,
   Tape and Space 10).
5. **FM voices**: a small "three FM pairs" voice (fm1_open's demo recipes are all DX7 algorithm 5, three
   modulator-carrier pairs, plus one six-sine organ), then all 16 recipes (Neon Keys, Orbit Bell, Soft Organ,
   Round Bass, Warm Air, Wood Reed, Soft EP, Dry Clav, FM Guitar, Marimba, Vibes, Soft Brass, FM Strings, Sub
   Bass, Metal Hit, Soft Lead) offered in the layers that suit them, the favourites picked by ear.

## Later (proposed, not decided)

From the survey of Felucca, fm1_open and Skiff (2026-10-28), ranked for NACHTFAHRT:

- **The FM-1's second CPU core** (Skiff's beta work): the only big CPU gain; it would make room for everything
  else. Platform work, risky, tested on the device.
- **A master limiter and a bass lift** for the small speaker (tiny, cheap).
- **Sympathetic strings** tuned to the chord, ringing under the plucks.
- **A hall reverb**, a second, longer and darker space.
- **A rotary effect** for the Organ.
- **Sequencer ideas**: n:m step conditions, ratchets, new orders (converging, diverging, walking), slewed random.
- **REC as "record the theme"**: an explicit way to give the Lead its theme (the capture is built so a future
  trigger can call it).
- **A pulsing-root bass layer** (discussed; parked: the user didn't want Sequence 2 changed much).
- Licences: everything with source can come in under GPL-3.0 with its notices kept; fm1_open's binary SDK can't;
  check Felucca's engine licence before copying from it.

## To do

1. **On the device: done for 0.9.0** (2026-10-28). `./build.sh --release 0.9.0` (JieLi toolchain, AC79 SDK in
   WSL): image 198,788 B, RAM 72,004 of 98,304 B, pool 268,368 of 344,064 B. Installed from the web installer
   and running on the user's FM-1, behaving as in the emulator. The Setup page's CPU: about 48% now / 65% peak
   at its heaviest; **Sunrain** peaks at 73% and settles under 60% (no dropouts reported). For scale, DUB-1:
   20/50% (Basic), 30/67% (Night Drive), 50/72% stressed. Roughly 50× the host proxy's figure (one reading).
   Still to read: each piece's own numbers, and whether Sunrain's peak returns at every chord change.

2. **Tuning the pieces by ear** (the user): tempos, keys, levels, each piece's Moves (the Intros, Widths and Bridge) and Arc lengths. Three-minute renders
   of each: `tests/listen/pieces.amb`. Done by ear so far: Timewind (Sequence 2 at 60, Reverb 50, lengths 16/15),
   Wahnfried (a faint heartbeat of sequences, Strings 95, the theremin Lead 54), Rubycon (the 2600 and the Synthi,
   octaves −2 / 0), Cyborg (Sequence 20, the Lead 70), the Atmospheres quieter (Surf 5, Wind 7, Tape and Space 10).

3. **Read 0.9.2's CPU on the FM-1** (the highest peak on the Setup page), and the same with Shimmer held in Sunrain
   and Mirage once it's released (estimated about +1.3 points: an inference from the host).

4. **Listen**: Shimmer's strength (Mirage the strongest, Wahnfried the faintest); the Lead's answer to a played
   figure (related enough?); whether a played theme should come back at its own pitches over a new chord (now it
   moves with the chords).

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
- From the reviews, deferred: Memory 100 keeps one motif for ever (an ear decision); after a piece change the first
  Lead phrase can come up to ~7 bars late; two register calculations disagree outside 7-note scales (harmless);
  a few test thresholds are tighter than the code guarantees (the strong-beat guides, one mean bound); Shimmer's
  high-pass comment says 150 Hz for ~155.
- Some DUB-1 scenarios that covered help cards were removed with the pages they tested. (The nudges' tables were
  DUB-1's, so a nudge, or Evolve over 40, moved the Lead's sends, the Ending, Transpose and the Grid: fixed,
  `tests/scenarios/nudge.amb`.)
