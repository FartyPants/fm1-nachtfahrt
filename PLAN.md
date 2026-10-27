# DUB-1: plan and progress

A fork of AMB-1 (generative ambient for the M-VAVE FM-1) turned toward dub techno: chord stabs into a
tape echo, a sub bass, a four-on-the-floor kick, vinyl crackle, and an arrangement that drops and
brings back its layers in eight-bar phrases. Working name **DUB-1** (easy to change). Work happens on
the `dub` branch; `EMULATOR.md` says how to build and listen on the computer.

## The design

| AMB-1 | DUB-1 |
|---|---|
| Pad: four breathing voices | **Chord**: a four-voice stab (two detuned saws a voice, or FM keys) on a Euclidean pattern off the beat, through a resonant low-pass with its own envelope. One voicing (open m7, or a 9 without its fifth) shifted whole to each root, as a chord memory |
| Bells: four tape loops (13/17/23/29 quarters) | **Pings**: short FM blips on the same loop lengths, in eighths, mostly into the echo |
| Bass: sine and sub-octave, gliding | **Bass**: a sub (E1..Eb2) on five grooves: Drone, Offbeat, Dotted, Roller, Sync |
| Pulse: three Euclidean patterns (16/12/7) | **Drums**: four-on-the-floor kick with a click, a shaker on the sixteenths (Hits), an open hat off the beat; the kick ducks everything else (Duck) |
| Arc: a minutes-long tide | **Arrangement**: eight-bar phrases (Full, Break, Groove, Thin), a fill before the kick returns, the filter sweeping with it |
| Tape echo + Echoplex + plate | The same, with a band-pass that moves in the echo's feedback, a soft clip so the feedback can pass 100%, dub throws (a stab sent hard into the echo) |
| Freeze: the chord holds, the reverb rings | **Dub hold**: the echo stops listening and repeats its loop for ever; the chord holds |
| (none) | **Crackle**: vinyl hiss and clicks |

Tempo 90–140 BPM (default 122). Echo times 1/8, 1/8. (default), 1/4, 1/4., 1/2, 1 bar. Drift in bars
(2..64) or Hold.

### The pages (planned)

| Button | Page | KNOB 1–4 |
|---|---|---|
| HOME | Dub | Tempo, Evolve, Motion, Drift |
| ENV | Chord | Level, Stabs, Decay, Cutoff |
| ARP | Texture | Pings, Density, Tone, Crackle |
| LFO | Bass | Level, Octave, Groove, Fifth |
| SEQ | Drums | Level, Hits, Swing, Duck |
| FX | Space | Echo, Echo time, Feedback, Reverb |
| GLO | Setup | Root, Tune, MIDI out, Lights |

Set by the piece only: Mode (ALGORITHM), Chord tone (Saws / Keys), Hats colour, Plate size, ping octave
(OCT- / OCT+).

## Steps

1. Tempo range and the four-on-the-floor kick
2. The chord stab and its filter
3. The echo's feedback filter, throws, dub hold
4. The phrase arrangement
5. The pieces (eight recipes)
6. The UI (pages, labels, help cards, screen reader words, colours), storage, tests, manual

Steps 1–4 are done as one engine rewrite (`firmware/src/dsp/amb.c`), then tuned by ear (yours) and by
measurement (renders analysed for level, timing and spectrum).

## Done

- [x] Local simulator set up (`EMULATOR.md`, `.venv`, `tools/gen_font_noraqm.py`)
- [x] `dub` branch
- [x] Engine rewrite (steps 1–4): chord stabs, pings, bass grooves, drums with ducking, arrangement,
      dub echo (band-pass + soft-clipped feedback, throws, hold), crackle (`amb.c`, `amb.h`)
- [x] `host/en_render.c`: the engine alone to a WAV (parameter overrides, timed changes `NAME=V@T`,
      `freeze=T0-T1`, a bar-by-bar log); `tools/analyze_wav.py`: levels, bands, kick timing, spectrogram
- [x] First tuning by measurement: layer balance (the chord +12 dB, the pings +9 dB), longer echo tails,
      no half-diminished chords, fewer throws, keys as loud as the saws, crackle only while playing,
      feedback eased under 90% while stopped (tails always die); feedback at 100% stays bounded
- [x] CPU: on the host the new engine costs the same as AMB-1's or less (0.20–0.30% of real time vs
      0.27–0.31%)
- [x] Eight pieces, first draft: Basic, Chamber, Steppers, Fog, Concrete, Night Drive, Pressure, Haze
- [x] UI: pages, labels, help cards, messages, a dark palette, the section and phrase on the main
      screen, "hold"; screen reader texts
- [x] Storage: project magic "DUB1", format 1 (AMB-1's saved projects give the recipes)
- [x] Engine unit tests rewritten (`tests/host/amb_test.c`)
- [x] A listening pack: `build/listen/pack/` (each piece, dub hold, feedback swell, the arrangement)
- [x] Simulator scenarios and the screen reader's words for DUB-1; the whole suite passes
- [x] README, manual, licensing note and screenshots for DUB-1
- [x] The device build works here (`EN_FONT_NORAQM=1 ./build.sh` -> `build/amb.fwsc`): RAM 52.6 of
      96 KB, pool 332 of 344 KB, no soft doubles
- [x] DUB-1 by name on the device: splash, crash and safe-mode screens, USB strings; MIDI out off
      silences every channel

## TODO

- [ ] **Your ears**: listen to the pack (`build/listen/pack/`, rendered again with the cloud); what to
      change in the sound and the pieces
- [ ] The web app's own page (`web/app/index.html`, the phone/desktop app): still AMB-1's UI (the
      emulator page is done)
- [ ] **On the device**: install (`python3 tools/fm1_install.py build/amb.fwsc`), check the Setup
      page's CPU and the sound on the FM-1's speaker and on headphones
- [x] The name: DUB-1 (decided 2026-10-16)
- [ ] Publishing (the words file, a hosted emulator and installer): later, after trying it longer
      (decided 2026-10-16). Where (the `# url:` in
      `firmware/words.tsv` still points at AMB-1's site)
- [ ] With everything at maximum the mix drives the output limiter (peak 0.98): fine as a limit, but
      the levels could leave more room

- [ ] `tools/gen_manual_figures.py` and `tools/make_pages.py` still make AMB-1's panel figure and site
- [x] Decisions on behaviour (2026-10-16):
  - Bass Octave −1: a true octave down (E0..Eb1)
  - Fifth with Drone: how often a whole bar is the fifth
  - Evolve 0 (with Motion 0): a strict loop, no stab gaps, no throws, no fills
  - The kick follows the key: the root in F1..B1, below F its fifth (44..62 Hz)
  - Concrete keeps its chords in fourths (7sus4 sounds)
- [ ] **Sound design** with your ears: the stab's character, the kick, the bass, the hats, the
      echo's colour, the cloud, the crackle (a round of its own)

## Release 1.1.0 (in progress, 2026-10-17)

- [x] PERFORM's Open and Shut you can hear (a filter on the music, the kick left out)
- [x] PERFORM reworked: white 1-5 mutes, black 1-5 solos (above the mutes, a mixer's strip), white 6
      dark, white 7-11 the moves; every key that does something lit (a solo blinks while on, a move
      goes dark while held); the key map on SEL's card; SOLO on the screen
- [x] Ping lock: at Evolve 0 the loops stay as they are; SAVE keeps them with the piece (format 9; 1.0.0's
      projects are read and kept)
- [x] The Feedback knob shows the real loop gain (45-112%)
- [x] White 10 is Drop: the kick and the bass out while held (was Kick drop, the same as the kick's mute)
- [x] The video take again with the new keys (`tests/film/00-take.amb`, `build/video-script.md`)
- [x] The chord's name bigger; the Setup page's battery in the header, the raw reading gone
- [ ] Manual, README, the emulator page, release notes; `./build.sh --release 1.1.0`; the site; push
      and the tag (when the user says)

## Release 1.2.0: live performance (released 2026-10-19)

Decided with the user (2026-10-19):

- [x] **HOME, Dub (the song as it plays):** Filter, Evolve, Motion, Drift. Tempo leaves knob 1 for
      **Filter**. Filter is bipolar: down is Shut's low-pass (the music under water), up is Open's resonant
      high-pass, the middle is off (the knob stops at the middle on the way through), and the kick is always
      left out. It is the same filter as PERFORM's Open and Shut keys, which push on top of it.
- [x] **HOME again: the Key page:** Tempo, Root, Mode, Swing (the piece's settings, rarely changed in a
      dub song). HOME + SELECT stays as the tempo shortcut from any page. HOME blinks on the Key page.
- [x] **ALGORITHM on HOME: the screen view** (ripples, scope, spectrum; was HOME again). ALGORITHM still
      sets the voice on a layer's page and the mode elsewhere.
- [x] **GLO only for what is global:** Tune, MIDI out, Lights (and CPU and battery in the header). Root
      moves to the Key page.
- [x] **PERFORM's knobs: the master actions** Filter, Dub, Energy, Space (Filter on knob 1, as on HOME, the
      same value). **White 6 held: the mix** (chord, pings, bass, drums), as the knobs were; the mutes still
      act on the press. White 6 is lit in PERFORM and goes dark while held.
  - Dub: the echo send (+40 at full) and feedback (+35). Energy: Density, Hits and Stabs (+45 each).
    Space: Reverb and Cloud (+45), the plate's Size (+30). All bipolar: 0 is the piece as written.
  - **For playing only:** never saved. They keep their value across a piece change (filter down, change
    piece, open it up: the night video's transition by hand), and are back in the middle on boot.
- [x] The scenarios updated (screen, scale, voices, pings, help, reader, perform: the master actions and white 6),
      the help cards, the emulator page's help
- [x] Listened to on the FM-1 (the user): the master actions widened, white 6 blinking, Swing fixed, each piece its own view
- [x] Manual, README, the release notes; `./build.sh --release 1.2.0`, the site, the tag
- [ ] The film takes (`tests/film/*.amb`) use 1.1.0's controls (HOME again for
      the screen): update them before rendering with 1.2.0
- [x] Fixed: Tape stop past the tape's end in dub hold (genkigenki's report; reproduced under AddressSanitizer);
      ALGORITHM and PRESETS skipping detents on a quick turn; the clap's bursts clicking (they started at full level)
- [x] Haze's "faint clicks" (the shorts agent): nothing broken. They're the kick's designed click, the hats' noise and the
      Blip pings' attacks, made more noticeable by Haze's long echo.
- [ ] SMK-37: genkigenki's PR #1, kept on an `smk37` branch until he tests it on his unit (the user, 2026-10-19:
      accept it in principle; host the SMK packages and an SMK emulator; the user will get an SMK-37 Pro later).
      See `build/smk37/REPORT.md` and `MERGE.md`

## Next: after 1.0.0 (the user's ideas, 2026-10-17)

**The main direction (the user, 2026-10-17):** after the small fixes and ideas, expand how playable
the instrument is. DUB-1 should be something you play live, not only leave running. PERFORM (mutes,
solos, moves), the effects grid and dub hold are the start. More hands-on control comes next, and
each idea below is weighed by how much it adds to playing it.


- [ ] **A video for YouTube and other platforms** (the most urgent). Real action and sound from the
      simulator, not stills: the screen inside a drawn FM-1 panel, with the buttons and keys lit as they
      are pressed (`film` in the host simulator, `tools/film.py`). The script follows the Reddit post:
      it plays as soon as it starts; playing it (PERFORM, dub hold, the effects grid); making your own
      piece (sounds, chord progression, drums). Clips to cut in DaVinci Resolve.
  - [x] The clips: `tools/make_film.sh` runs `tests/film/*.amb` and makes three MP4s of each in
        `build/film/` (the panel with the screen large, the panel alone, the screen alone) and the WAV
  - [x] One take (`tests/film/00-take.amb`, 2:48): every move on a downbeat (`bar N` in the simulator,
        printing the cue sheet), so the music runs unbroken; the edit cuts between the framings only.
        The script with its cues: `build/video-script.md`
  - [ ] Cut, titles, voice-over or captions (the user, in DaVinci)
- [ ] **Next release, to think through carefully: where Tempo, Root and the master actions go**
      (the user, 2026-10-17). Nothing decided yet.
  - **GLO only for what is really global:** settings that hold for every piece and the whole
    machine (Tune, MIDI out, Lights, the battery). The piece's own settings stay out of it.
  - **Tempo and Root are the piece's**, and they rarely change within a song. They could sit
    together on HOME (Root moving there from GLO), or elsewhere. HOME + SELECT stays as the tempo
    shortcut from any page.
  - **Master actions at hand.** Knobs that act on the whole music, for playing it live. Where they
    belong is open; the HOME page is a good candidate. The ideas:
    - **Filter**, bipolar: left is Shut's low-pass (the music under water), right is Open's
      high-pass (thin and bright), the middle is off, and the kick always stays out of it. It's the
      DJ mixer's filter, a continuous version of PERFORM's Open and Shut keys.
    - **Dub** macro: the echo send, feedback and band-pass on one knob. Turned up, more goes into
      the echo, it feeds back longer and turns darker, towards a runaway; turned down, it's dry and
      tight.
    - **Energy** macro: Density, Hits and Stabs together. Down, the pings, hats and stabs thin out
      towards a sparse groove; up, everything fills in.
    - (More can come in the same spirit: one knob, several parameters, a musical direction.)
  - **The user's idea (2026-10-17): the master actions in PERFORM.** The keys stay the mutes, solos
    and moves; the four knobs become the master actions (Filter, Dub, Energy, and a fourth: for
    example Space, the reverb and the cloud together). The mix is still there: **hold a key and turn
    a knob** for the levels, as the knobs are now. Two ways to read "a key":
    - *the layer's own mute key* held: any knob sets that layer's level (a mixer's channel, picked
      by hand). The mute would then act on the key's release, and only if no knob was turned while
      it was held (as the buttons do). That costs a press-to-release delay on the mutes.
    - *one shift key* held (SEL itself, or white 6, the dark key between the sets): the knobs are
      the Mix page as now (chord, pings, bass, drums). The mutes keep acting on the press.
    - The screen shows which: the master actions' page, or MIX while the key is held.
    If the master actions live in PERFORM, HOME can keep Tempo, Root and the piece's settings, and
    GLO stays for what is really global.
  - **The questions:** HOME has four knobs (Tempo, Evolve, Motion, Drift today). Do Evolve, Motion
    and Drift stay there, or move to make room for the master actions? Should the master actions
    live on their own page, or in PERFORM (the Mix knobs)? And what happens to the knobs' values when
    you change pieces: saved per piece, or performance-only, reset to the middle?
- [ ] **Projects outside the FM-1, and new sets of pieces for people to load** (the user's idea).
  - **What travels:** a whole project is the eight pieces with everything in them: tempo, scale and
    root, the voices and their sound and FM pages, the mix, the echo and space, the chord slots, the
    drum patterns. It's the same data the FM-1 keeps in flash (`project_t`, `PROJ_FORMAT` 8, magic
    "DUB1"), about 2 KB.
  - **Out and in:** from a web page (the installer's site), over USB-MIDI SysEx, the way the screen
    reader already talks to the FM-1. "Save to file" reads the project into a `.dub1` file; "Load"
    writes one back. The firmware checks the magic, the format and a CRC before taking it, and keeps
    a single piece or all eight.
  - **New sets of pieces:** write other banks of eight for people to explore, for example a deep and
    slow set, a Chain Reaction-style set, a minimal set, or a set built on two chords. Publish them
    as a library on the site, each with a short listen, loadable in one click. Load them in the
    emulator too: try before you load.
  - **Later:** people share their own `.dub1` files, and a set can carry its own names.
  - **To decide:** whether a loaded set replaces the eight pieces or goes into slots, and whether
    the firmware ships with more than one bank.
- [ ] **Nail the sounds:** some voices sound too alike. Make each voice on ALGORITHM clearly its own.
- [ ] **The live side, further:** more to play by hand (see the Reddit feedback below).
- [ ] **From the feedback (Reddit, 2026-10-17):**
  - "More for composing my own melodies": a way to write or lock the ping line. At Evolve 0 the
    loops still add and drop notes (`loop_target` rounds at random). Keep them fixed at Evolve 0, and
    let SAVE keep the current pings with the piece.
  - The Feedback knob reads 0-100 while the loop runs 45-112%: show the real figure.
- [ ] **Ideas from "Understanding Dub Techno: A Musicologist's Approach"** by Bahadırhan Koçer
      (https://www.youtube.com/watch?v=2XMWPQmSBOw; studied from its subtitles, 2026-10-17). It rests on
      three ideas: spontaneous repetition ("a river": the loop the same, its echo, reverb and filter
      always moving, some of it barely heard), atmosphere (the mix as a room, a "backcloth" filling the
      gaps between the stabs), and noise as the canvas. DUB-1 already has slow random movement on the
      cutoff and the echo, feedback past 100% held by a clip, crackle, the cloud and the plate, and a rim
      on two and four into the echo. To try:
  - the stab's decay moving at random (tight, then loose): `mod[1..4]` are named the stab's but only
    `mod[0]` (the cutoff) is read;
  - a faster random layer under Motion: small sixteenth-step jitter on the cutoff and the echo feedback;
  - the crackle tuned to the key: through a resonator on the root or the fifth and a moving band-pass,
    part of it sent into the echo;
  - part of the crackle on the sixteenths, as a quiet percussion lane;
  - a backcloth: a hum near the root and filtered tape noise, always under everything (a sibling of
    Crackle);
  - noise in the kick (a breath) and a little in the sub;
  - a "gritty shimmer": an octave-up return through a saturator into the plate (the cloud could be it);
  - a two-pad backcloth in Thin and Break (low and held, high and swept), so the gaps are never empty;
  - two-chord pieces: a recipe that keeps the walk on two degrees, the harmony still, the effects moving;
  - a flanger or phaser in the echo's feedback, at an odd rate (or as a grid effect);
  - a spring colour on the rim or the clap.
- [ ] **For later: other genres.** Fork AMB-1 again and bring DUB-1's new parts (the FM core, the
      effects grid, PERFORM, the pattern page, the screen views) to other styles: techno, industrial.

## Done: voices (a first approach)

- ALGORITHM on a layer's page steps through its voices; on Dub, Space, Setup and Mix it is still the
  mode. The voice shows at the left of the knob strip, is said ("VOICE Organ"), and is kept by the
  piece.
- Chord: Saws, Keys, Organ, Pad, Bell, Tonewheel (replaced Brass, too like the others), Noise. Pings: Blip, Marimba, Glass, Wood, Drop. Bass:
  Sub, Round, Growl, Pulse. Kits: Tight, Deep, Punch, Soft, Dusty.
- Each voice is a row of a table in `amb.c` (`CV`, `PV`, `BV_GAIN`, `KIT`). Levels are matched by
  measurement (within ~2 dB of the layer's first voice).
- The pieces got their own voices. The project format is now 2.
- CPU (host): Organ is the heaviest, 0.33% of real time (AMB-1's worst: 0.31%).

### Voices: feedback (2026-10-16)

- The Mix page's labels (CHORD, PINGS, BASS, DRUMS) are fine.
- [ ] The basses are too alike: something punchier, from the classic FM bass patches (later).
- [x] Brass was too like the other chord voices: now Tonewheel, a second organ.

## Next: a sound page for each layer (the user's plan)

Each layer gets a second page, its **sound page**, to shape its voice beyond the preset.

### How it works

- **Reaching it:** press the layer's button again (ENV on the Chord page: the Chord's sound page;
  ENV again: back). The page's name shows it ("CHORD · SOUND"), and its button blinks while there.
  SELECT's round of pages stays as it is (the sound pages are off it). ALGORITHM still picks the
  voice, on either page.
- **Four knobs a layer**, with the same meaning for every voice of that layer: each voice maps
  them onto its own insides (for a saw voice, "Colour" is the detune; for an FM voice, the
  modulation index; for the organ, the drawbars' balance).
- **Relative to the preset:** each knob's centre (shown as 0) is the preset as it is; turning
  moves away from it. Changing the voice with ALGORITHM keeps your turns, so a "brighter, slower"
  chord stays brighter and slower on the next voice. A press of SAVE held (the piece's reset)
  puts them back to 0, with the rest.
- **Saved with the piece**, like everything else: 16 more values a piece (4 layers x 4); the
  project grows from ~700 to ~980 bytes, under the 3840-byte limit. Project format 3.
- **The screen**, on a sound page: the main area shows the voice's shape instead of the ripples,
  its envelope drawn (attack, decay, the filter's own), redrawn as you turn. Later, for the FM
  voices, the algorithm as a diagram.
- Two values only the pieces set today get a knob here: the pings' tone and the hats' colour.

### The knobs

| Layer | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| Chord | **Attack** (a stab .. a swell) | **Colour** (detune / FM index / drawbars / noise colour) | **Resonance** (the filter's) | **Env** (how far each stab opens the filter) |
| Pings | **Tone** (round .. metallic: the index, today's Ping tone) | **Ratio** (the modulator: harmonic .. bell-like .. clangy) | **Length** | **Bend** (the pitch falling at the strike) |
| Bass | **Bite** (FM index / drive at each note) | **Shape** (sine .. square-ish) | **Length** (how long each note holds, against the groove's) | **Slide** (glide between notes) |
| Drums | **Kick tune** (semitones from the key's note) | **Kick length** | **Punch** (the kick's start pitch and click) | **Hats** (dark .. bright, today's piece value) |

### Then: a real FM core (phase 2)

- One 4-operator FM engine (sine operators, ratios, levels, feedback on one, an index
  envelope), with the classic algorithms (the eight 4-op layouts, as the FM-1's own firmware and
  the DX-style synths have them), for the chord and the pings. The FM voices (Keys, Bell, Marimba,
  Glass, Wood) become patches of it, and new ones come from the classic FM patch libraries (the
  punchier basses wanted, electric pianos, clavs, bells).
- The sound page gets a second set of four on an FM voice (the layer's button a third time):
  **Algorithm**, **Ratio** (of the modulator in focus), **Feedback**, **Index env** (its decay).
- **CPU first:** four chord voices x 4 operators is the heaviest thing DUB-1 would do. Measure on
  the device (the Setup page's CPU) before, and keep a lighter algorithm set if it's tight.

### Steps

- [x] The sound pages: the UI (the second press, the page, the help cards, the screen reader), 16
      parameters, project format 3, tests (`tests/scenarios/sound.amb`)
- [x] Each voice's mapping of the four knobs (a first pass; to tune by ear)
- [x] The envelope drawing on a sound page
- [x] The FM core (4 operators, the eight 4-op algorithms and a 2-op one, op 4's feedback, an envelope an
      operator), the FM voices moved onto it (Keys, Bell, all the pings); new basses: Punch, Rubber, Pluck
      (Round and Pulse gone), Growl
- [x] The FM page (Algo, Ratio of ops 3 and 4, Feedback, Mod dec), a third press on an FM voice; the
      algorithm drawn; project format 4; `tests/scenarios/fm.amb`
- [x] **CPU on the device** (2026-10-16, DUB-1 installed on the user's FM-1, over the stock V15 put back
      first): Night Drive shows 20..50% (smoothed). The host's 0.42% of real time is ~80x faster than the
      device. Setup now shows the peak too (now/peak over 2 s) and any dropouts (!n)
- [x] Readings (2026-10-16, now/peak, no dropouts anywhere; "sounds good"): Basic 20/50, Night Drive
      30/67, Night Drive stressed (Cloud 100, Density 100, the pings on algorithm 1) 50/72. About a
      quarter of the time is left at the worst moment: what's added must be light
- [x] Found on the way: the command queue (64) dropped commands once a piece had 62 parameters (PLAY
      among them): now 256, with a compile-time check and a test

## Done: DUB-1's own interface

- **Main screen, echo ripples:** each stab drops a ring that spreads and fades; the echo's repeats
  follow it at the echo's own time and feedback. Throws are double rings; dub hold is red and
  doesn't fade. Also the chord's name, the section and phrase, the layers' levels, a duck meter,
  and four lanes (stabs, kick, hats, bass) showing the bar as it plays. The scale ring, ping loops
  and drum rings are gone (the keys still light the scale).
- **PERFORM (SEL):** the knobs become the Mix page (chord, pings, bass, drums levels); white keys 1–5 mute the chord, pings, bass, kick and hats; 6–10, held, throw
  every stab, push the filter open or shut, drop the kick, push the echo. The keys light up for
  the layers playing and the moves held. Mutes keep the music's random choices, so a layer comes
  back where it would have been. The engine API is `en_mute`, `en_perf`.
- Scenario `tests/scenarios/perform.amb`; the manual, the README, the emulator page and the
  screenshots are updated.
- The web emulator (`web/emu`) is ported and builds with emsdk (`EMULATOR.md`); its test and the
  web app's engine test pass.

## Done: a granular reverb (CLOUD), after JIANT

After JIANT's CLOUD (github.com/juanjiant-bit/JIANT, GPL-3.0; credited in the README, the manual
and `amb.c`):
- A 0.74 s line at a quarter rate (11 kHz, 16 KB, in RAM: the pool is full), fed by the reverb send.
- Up to 6 grains of 80–250 ms (triangle windows, random place and pan, a few cents off, sometimes
  an octave down, rarely up), denser as Cloud turns up and as the filter opens.
- The grains go into the plate, and the plate's tail back into the line, so the wash grows with
  a bigger plate. More of it in a Break; dub hold freezes it.
- It has its own random numbers, so turning Cloud doesn't change the music's choices.
- **Cloud** is the Texture page's third knob; the pings' tone is now set by the piece.
- Measured:
  - in Haze the cloud is ~10 dB under the mix;
  - at everything max it stays bounded and dies once the layers stop;
  - CPU within ~3% of AMB-1's (host);
  - RAM 69.2 of 96 KB.

## Next after the FM core: an effects grid on REC (the user's idea)

Holding REC shows a grid of effects; the white keys play them while held (a tap on REC stays dub
hold). Sixteen in four groups of four, the first of each group lit:

| Group | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| Echo | Throw | Time x2 / /2 | Tape stop (the echo slowing, pitch diving) | Swell (all in, feeding back; was Reverse: the tape can't run backwards without much more memory) |
| Filter | Low-pass sweep | High-pass ("radio") | Wah (resonant sweep) | Telephone (narrow band, lo-fi) |
| Space | Reverb throw | Spring crash | Cloud freeze | Wash (echo and reverb flooding) |
| Rhythm / FX | Stutter (1/8 or 1/16 repeat) | Gate (the chord in 16ths) | Siren | Noise riser |

- [x] The grid on screen while REC is held; the keys lit by group; said whole to a screen reader
- [x] Each effect, eased in and out (Stutter borrows the Echoplex's buffer; Gate lets the chord ring on to be
      chopped; Freeze brings the frozen cloud forward). CPU (host): one filter effect +22%, all sixteen +55%
      (in Night Drive on the device that could reach ~100%: the effects are momentary, but worth watching)
- [x] PERFORM keeps the mix and mutes; the grid is the effects (`tests/scenarios/effects.amb`)
- Found on the way: at Cloud 30 with Reverb 35, the cloud was all but silent (its input came after the
  Reverb knob): it now listens as if Reverb were at least 63

## From the user's testing (2026-10-16)

- [x] PERFORM's Mix knobs in the layers' colours (chord teal, pings blue, bass brass, drums amber)
- [x] The Drums knob follows the Kick and Hats keys (0 when both are out, 100 when one comes back)
- [x] A muted layer (or one at level 0) leaves its lane at once
- [x] The stabs' lane shows the echo's repeats as rings, flashing as the playhead reaches them (made
      brighter, with a white flash: "they don't blink" on the device)
- [x] **A launch screen** (asked: "fancier", then "the idea of the echoes", the voices' colours): ~1.4 s
      while the FM-1 starts, a delay throw: the name struck, then four repeats dropping out of it, smaller and
      fainter, in the layers' colours, each with its ring; the line under it, the version (`ui_splash`; the
      simulator's `splash N` draws a frame)
- [ ] Idea: the launch heard too: a stab thrown into the echo as the screen plays, its repeats in time with
      the drawn ones
- [x] The FM pages always open on the third press; on a voice that isn't FM they say so and name the FM
      voices (ALGORITHM there picks one)
- [x] **Battery:** an icon on every page (three bars), a rough % on Setup with the raw reading. The
      thresholds are dxsloop's (591 / 561 / 531 on the divider's raw reading, PB1, ADC ch 3); on USB the
      bars fill in turn (no charger line). The % (521..601) is an estimate: to check against the
      FM-1's real readings, full and low
- [x] **The Chords page** (the user's design, in place of option 2): EDIT tapped, eight slots; KNOB 1 the
      slot, the keys its root, KNOB 2 its kind (Empty, Major, Minor, Sus2, Sus4, Dim, Aug), KNOB 3 its type
      (Triad, 6, 7, Maj7, 9, 11, Add9), KNOB 4 Play (Random / Prog); an empty slot between two holds the
      one before; EDIT held: the next chord. Project format 6 (`tests/scenarios/chords.amb`)
- [x] (replaced) **Editing the chords** (option 2): EDIT held + keys, a progression of up to eight chords (the keys'
      notes in the scale), played in order every Drift bars (with Hold, on EDIT's tap); held 1.5 s
      alone, random again; "1/4" under the chord; kept by the piece (project format 5;
      `tests/scenarios/chords.amb`)
- [ ] **Idea: the chord's type** (option 3): per piece (or per chord of a progression), the chord
      built as an open minor 7 (now), a 9 without its fifth (now), a sus4, a minor 11, a major 7

## Done: the drums' pattern page and a rim (2026-10-16)

- [x] A fourth drum voice, the **rim** (two tones, ~520 Hz and ~1.7 kHz, a click of noise, mostly into the
      echo): Auto plays it on two and four once Hits is 70 or more; its own lane on the main screen (pink)
- [x] The **Pattern page** (SEQ a third time): KNOB 1 the lane (Kick, Shaker, Open, Rim), the white keys
      its sixteen steps, a black key held + a white one an accent, KNOB 2 Accent, KNOB 3 the lane's Chance,
      KNOB 4 Play (Auto / Pattern); the first edit takes Auto's pattern over. Project format 7
      (`tests/scenarios/pattern.amb`)
- [x] Found on the way: with 103 parameters, the UI's own knob codes (from 100) collided with real ones
      (Play was "no knob"; Lights would have set the rim's chance): moved to 200, with a compile-time check
- [x] A fifth drum lane, the **clap** (three bursts and a tail, into the plate): rare: Auto ~5% on two and four
      (3% + 5% x Evolve, in Full and Groove); on the Pattern page its Chance starts at 5% (was 12%:
      "too conspicuous"). Hits that only
      sometimes come are rings on the screen, filled when they play. Format 8
- [x] The effects grid takes the whole screen while REC is held; the knobs rest
- [ ] **Later: pattern styles** (the user's idea 2): a short list of classic dub techno grooves (steady,
      Steppers, Half-time, Broken, Shuffle with rim) to load into the Pattern page as a start

## Release 1.0.0 (2026-10-16)

- [x] Repository: github.com/sbegueria/fm1-dub (`origin`; AMB-1 kept as `upstream`)
- [x] Release identity FM-1_8XXYYZZ (AMB-1 is 7..., X0X 9...): 1.0.0 is FM-1_8010000; packages `dub-X.Y.Z.fwsc`
- [x] The site (`tools/make_pages.py`, `tools/publish_pages.sh`): landing, emulator, web installer, manual, the
      words files, the firmware; at https://sbegueria.github.io/fm1-dub/
- [x] The screen reader's words link: https://sbegueria.github.io/fm1-dub/words/
- [ ] GitHub Pages enabled on the gh-pages branch (repository Settings -> Pages), a GitHub Release v1.0.0 with
      dub-1.0.0.fwsc attached (the earlier v1.0 tag was never released)
- [ ] The phone/desktop web app (`web/app`) is still AMB-1's: not on the site; to port later
- [ ] Announce (later): Elektronauts, MatrixSynth, the FM-1 groups

## Done: a bolder screen, and the middle three ways (2026-10-16, in 1.0.0)

The user found the screen dim and its lines and features too small. They also felt the chord and
the ripples took too much of the middle.
- [x] Brighter greys and tints, ripples two pixels wide (a throw three), lane dots 6 px with
      rings to match, level and duck bars 8 px
- [x] The ping loops orbit the ripples: a dot each, a step at a time, lit as they play
- [x] HOME again on the Dub page: ripples, scope, spectrum (`proj.screen`, saved; said "SCREEN ...").
      The engine exports `en_scope` (256 samples, each the mean of 8) and `en_spec` (512 at 22 kHz).
      The scope triggers on a rising zero crossing, shows a phosphor of two traces and sets its own
      gain. The spectrum is Winamp-style: a 512-point FFT in the UI, 18 log bands (43 Hz-11 kHz),
      segment columns falling slowly, with peak caps. VU needle meters were tried first and
      dropped: they looked too realistic for a flat, schematic screen.
- [x] Test: `tests/scenarios/screen.amb`; screenshots from `tests/listen/shots.amb`

## Study: a sixth lane, one sample a piece (not started)

The user's idea: each piece has one sample of 1-4 seconds. It is thrown very sparsely by the
arrangement, or played only by hand (from PERFORM, a key). For now this is a study of where it
would fit.

- **RAM can't hold it.** RAM is 74.7 of 96 KB used and the pool 332 of 344 KB, which leaves about
  20 + 12 KB. One second of 16-bit audio at 44.1 kHz is 88 KB. The sample has to play straight
  from the SPI flash, which is memory-mapped (XIP at 0x02000000). Reading 2 bytes per output
  sample is little bandwidth; the cost is cache misses, to measure on the device.
- **Flash** is 1 MB. The app region (`app.ld` XIP) runs to about 0x8E000, and the image uses about
  170 KB of it. The calibration sits at 0xDC000, the update loader's staging area at
  0xE0000-0xFBFFF and the project at 0xFC000. That leaves roughly 0x2C000-0xDBFFF, about 700 KB.
  Some of it is the app's room to grow, so take about 300 KB for samples: from 0x90000 up to the
  calibration. All of this must be checked against the loader (what an update erases, and where
  it stages) before anything is written there.
- **Sizes, for 4 s mono:**
  - 16-bit at 22.05 kHz: 176 KB. Too big for eight.
  - 8-bit mu-law at 16 kHz: 64 KB.
  - 4-bit IMA ADPCM at 22.05 kHz: 44 KB. Eight pieces fit in about 350 KB; at 3 s each, 265 KB.
  - So: one shared bank of about 300 KB, with each piece pointing at a sample, or eight slots of
    about 36 KB (about 3 s of ADPCM each).
- **The engine side is small.** A voice with a position, a pitch (the key, or the root), a
  gain and a band-pass costs a few hundred bytes of RAM. ADPCM decoding is a few operations per
  sample, so about 1% CPU. The sample would go into the echo like a throw.
- **Getting a sample in:** built into the image (a fixed bank, the simplest), or uploaded from the
  web page over USB MIDI SysEx. The loader-style A/B scheme isn't needed: a torn sample only
  sounds wrong, but it needs a CRC check before it plays.
- **First steps:**
  - read the loader's flash use;
  - time XIP reads in the audio callback;
  - try one built-in ADPCM sample on a key in PERFORM.

## Ideas for later

- **Korg NTS-3 kaoss pad integration** (a future project) (the user's own setup; a Kaoss pad is great for dub techno).
  Send one voice to the NTS-3 and, ideally, bring it back and mix it with the rest.
  - **The NTS-3** (Korg Nu:Tekt kaoss pad kit):
    - stereo AUDIO IN and OUT (3.5 mm), SYNC IN and OUT (3.5 mm TRS), USB-C (power; MIDI over USB
      not confirmed);
    - up to four effects at once, 200 programs;
    - custom effects through Korg's logue SDK (`platform/nts-3_kaoss/`, SDK v2, Cortex-M7).
    - One retailer says an internal switch makes the sync jacks MIDI in and out (to check).
  - **The FM-1:** a stereo audio out, MIDI in over USB and the TRS jack, MIDI out over USB only, USB
    audio *to* a computer. No audio input is known.
  - **Sending one voice (doable): a "send" mode in DUB-1.** A setting (Off / Chord / Pings /
    Chord + Pings / Drums) puts that layer dry (after its filter, before DUB-1's echo) alone on the
    left channel, and the rest, with DUB-1's own echo and reverb, in mono on the right.
  - **The return, two ways:**
    1. *A mixer.* A TRS Y-splitter: left into the NTS-3; its output and the FM-1's right into a mixer.
       Works with the NTS-3's own effects; the mix happens outside.
    2. *The NTS-3 as the mixer (the elegant one).* The FM-1's whole output into the NTS-3, with a
       custom logue SDK effect that treats the left channel as the send (into the effect) and passes
       the right dry, summing both to stereo. No mixer, one cable each way, and the NTS-3's pad
       plays the send voice only.
       - To check: whether a custom unit can sit before the built-in effects in the chain (so
         Korg's own effects process only the send), or whether the custom unit has to be the dub
         effect itself, e.g. a tape echo with a band-pass in the feedback, written for the NTS-3.
    3. *Back into the FM-1:* only with an input it doesn't have: research whether the board's codec
       has an ADC and an input point (a hardware mod), or USB audio both ways through a computer
       (no longer standalone).
  - **Sync:** the NTS-3 has SYNC IN (Korg's analog sync pulses) and Tap BPM. The FM-1 has no sync
    out and sends MIDI clock over USB only. Ways to try:
    - MIDI clock over USB to the NTS-3, if its USB-C carries MIDI;
    - through a USB-MIDI host into the sync jack in its MIDI mode, if that switch exists;
    - a sync pulse on the audio (a click on the send channel's first sixteenth, which a custom
      unit could read as a clock);
    - else tap.
  - **Control from DUB-1:** if MIDI reaches the NTS-3, its X/Y and depth by CC, so the arrangement
    plays the pad: a sweep through a Break, the effect changing on a phrase.
  - First steps:
    - read `platform/nts-3_kaoss/` in the logue SDK (unit types, the chain, the I/O it gets);
    - check what the NTS-3's USB-C carries;
    - try the split mode with a Y-cable and the NTS-3's own effects.
- A spring reverb next to the plate
- MIDI CC out for the filter and the echo send, to drive other gear
- Keys as live mutes, or a held key as a manual throw

## Release 1.3.0, the musical release: the chord machine, visible, and a sound pass (2026-10-20; ready to publish)

At launch the firmware generates a full eight-chord progression (the random walk writes it into the eight slots), and
the Chords page (EDIT) shows it. Proposed: both of the user's behaviours, as KNOB 4 (Play) on the Chords page, which
today switches between Random and the slots.
- **Endless** (the default; option 1): the chord playing is always slot 1. When it moves on, all of them advance one
  slot and the walk writes a new chord in slot 8. You see the future and change it before it comes.
- **Loop** (option 2): the eight in turn, nothing new generated (the current "slots" mode, pre-filled). Removing the
  last slots shortens it. Switching from Endless to Loop freezes the eight on screen: catch a run you like.
- As now:
  - KNOB 1 picks a slot; the keys set its root, KNOB 2 its kind, KNOB 3 its type;
  - an empty slot holds the chord before it (a gap: the last one repeated);
  - Drift sets the bars a chord lasts (Hold: EDIT held moves on).
- Decided (the user, 2026-10-20): the walk's chords follow a change of key or mode (the user's stay as written);
  in Endless a gap stays a gap.
- Built: a slot is written (a kind), the walk's (no kind, the root a degree + 1, the type its voicing) or a gap
  (both 0), so no new parameters; PROJ_FORMAT 10, the same layout (9's and 8's empty slots read as gaps). The
  engine holds the slots and writes the walk's (`walk_from`, `walk_fill`, `chord_change`); the UI mirrors them
  (`en_prog_slot`, `en_prog_gen`, `prog_take`). A walk slot that becomes diminished after a change of scale is
  walked again. Tests: tests/host/amb_test.c (Loop, Endless), tests/scenarios/chords.amb.
- [x] The user tried it on the FM-1 (the builds up to build/dub-1.3.0-specks1.fwsc): "it feels more mature now"
- [ ] Publish (the user's go): push main, `tools/publish_pages.sh 1.3.0`, `gh release create v1.3.0`; then the SMK branch
- [x] **The sound pass, in 1.3.0** (decided by the user, 2026-10-20: the musical release, the chord machine and the
      sounds together): "Nail the sounds" and the sound design round below (the voices too alike, the kick, the bass,
      the hats, the echo's colour, the cloud, the crackle), by ear
- The engine: `chord_change()` already has the walk (from a degree other than home, back home 65% of the time;
  otherwise a weighted choice, IV favoured, then VII and V, never the current degree or a diminished one; now and
  then the other voicing). Slots are stored as absolute roots, kinds and types (P_CR*, P_CQ*, P_CX*); the walk would
  need to write those, or slots would store a degree.
- Added (the user, 2026-10-20): the walk's character on KNOB 4 (Home, Wander, Free, then Loop; Wander the default;
  the Dorian test: A-B-A bounces in 64 changes 22 / 2 / 5, at home 24 / 16 / 11); the modes stepped the most common
  first (Aeolian, Ionian, Dorian, Mixolydian, pentatonic minor and major, Phrygian, Lydian; the values kept as they
  were).
- [x] **The nudge and the shake** (the user's idea, 2026-10-20; tried on the FM-1: "works like a charm"): a layer's
      button held 0.5 s (ENV, ARP, LFO, SEQ) moves 3-4 of its sound values, steps up to an eighth of the range, a walk
      pulled back to the recipe (a quarter of the range away: always back); held on 1.5 s, a shake: another voice or
      kit (not the chord's Noise) and every value a big step. FX too (the space: echo, feedback, reverb, size, cloud; its
      shake a new echo time). EDIT held on 1.5 s: new chords (the walk's all walked again). The bass's groove (2026-10-20):
      its shake moves it to a neighbour; Evolve above 60 moves it now and then by itself, back towards the piece's
      (the user's earlier idea of slow groove changes within a song). Evolve above 40 nudges by itself on a phrase's first
      bar (1-2 values, half steps; never a shake). The help cards removed to make room for the hold.
- [x] **The pages and controls** (the user, 2026-10-20): the round in the panel's order; the levels in PERFORM's mix only,
      the freed knobs Throws, Ghost, Notes, Hats; OCT− / OCT+ a layer's level and mute (a level ring in the middle);
      format 13. The noise muddier (a worn cartridge). The screen: a layer's page shows that layer alone (ripples,
      scope, spectrum, its lanes in colour, the rest grey); the ripples' outlines the wave, the echo's repeats measured,
      the noise's clicks as specks; a knob's help line while it turns. Fixed: the grid flickering over the Chords and
      Pattern pages; line() overshooting a short slope (an endless loop).
- The videos for it: see "Videos to make" below.

## Videos to make (the channel's full plan: build/film/channel/CHANNEL.md)

- **After 1.3.0, with its firmware** (the user, 2026-10-20). Each one long, with Serena's voice-over, made in the
  simulator like the 1.2.0 video:
  1. **The chord machine** (4–5 min):
     1. a scale's seven chords;
     2. the walk's rules: home, the pull back, the favoured degrees, the voicings; Home, Wander and Free;
     3. Drift;
     4. Endless on the Chords page, the chords flowing in;
     5. steering it: edit a coming chord, make a gap, catch it in Loop, shorten it;
     6. what makes it generative: rules, chance, limits you set.
  2. **The modes** (music theory): the eight scales DUB-1 has, in its order (Aeolian, Ionian, Dorian, Mixolydian,
     the pentatonics, Phrygian, Lydian); how each sounds over the same groove; why dub techno lives in the minor
     ones; changing the mode live.
  3. **The chords** (music theory): a scale's chords as stacked thirds; kinds (major, minor, sus, dim, aug) and
     types (6, 7, maj7, 9, 11, add9); DUB-1's voicings (the open 7th, the 9th without its fifth); writing your own
     on the Chords page.
- Done: the 1.1.0 walkthrough, the 1-hour night video, shorts 1-9, the 1.2.0 launch video.

## To talk about (the user, 2026-10-21, after playing 1.3.0's sparse build on the FM-1)

- **Structured randomness for the drums and the bass.** The user liked the sparseness (and the muddy echoes), but
  thinning the drums and the bass at random "is not very dubby". Find other sources of more structured randomness:
  e.g. patterns that thin in a phrase-long shape, call and response between the kick and the bass, a sparse bar that
  comes back on the same beat, probabilities tied to the phrase's bar and the section, Euclidean thinning, motifs that
  return. Talk it through before building.
- **The noise machine, again.** "Too loud, and it sounds mostly like... simply noise" (zeroed in most pieces). Its
  reference: https://youtu.be/bbfI2fvCerc, the talk and its music example (build/research/dubtechno/REFERENCES.md #4;
  a study under way).
- **The filter, played.** The user enjoys playing the DJ filter most: a long, pleasant listen low down, then opening it
  up for a while. Worth building on (a slow filter journey? the filter's range, its feel at the low end).
- **A future set of pieces** in the vein of the listening list's ambient electronica (Max Cooper's "Parting Ways",
  REPORT3-partingways.md): an ostinato mode, the Tempo down to 75-80, real stereo width; and re-evaluating the eight.

## Now: two lines in parallel (the user, 2026-10-23)

After 1.3.0's release (the docs done, e20d31b..e10ddf4; a release video being made in build/film/release-1.3.0/):
1. **The CPU: the hot code in RAM** (below, "CPU: the hot DSP code from RAM"): first the test build
   (build/dub-1.3.0l-perf.fwsc, EN_PERF=1: the stall counters on the Setup page), then the FAST region.
2. **The firmware, on:** the UI review (structure from sonority), song structure (an intro and an outro), the virtual
   dub mixer, the smaller engine features, the ring marks; all below.

## Next: the classics pass (the user, 2026-10-23; after REPORT6-9, build/research/dubtechno/)

The references (the seven classics, an AI session, Outskirts) agree: dub techno's core is in time, not sound (the
loop held, the kick never leaving, an echo you hear, movement by hand rather than by section). Decided:
- **The palette** (built: the recipes after their models, the user: "do the palette pass"): the eight pieces as eight different ideas, each after one reference: Basic (Basic Channel's
  BCD), Steppers (Maurizio's M-Series), Chamber (Fluxion's Vibrant Forms), Night Drive (Intrusion's The Seduction of
  Silence), Concrete (Monolake's Hongkong), Pressure (Porter Ricks' Biokinetics), Fog (Echospace's The Coldest
  Season), Haze (the sparse intro: Parting Ways, Liumin's softer side). Their recipes as the features arrive.
- **The echo, clearer** (muddy now, some pieces especially): a clean first repeat, Blur growing with each pass; the
  returns ducked under the dry; less plate on the returns, the band-pass a little higher. Measured against
  Radiance's echo (likeness >= 0.15). First.
- **Width** (built, 6e49d06: the echo's right a few ms behind, a crossed chorus on the chord's and the pings' dry,
  the mix mono under ~80 Hz; the Echo page's Width): the user liked Width 85 over 50 in Fog and Night Drive (the
  palette's wide pieces, after Echospace and Intrusion); the narrower ones (Basic Channel's, Maurizio's) less.
  **The echo, clearer:** built, 36f899b ("sounds right").
- **Motion, three regions:** 0-40 the loop holds (Full only: the kick on four, nothing drops; the change by the
  filter, the echo, the texture); 40-70 variations (Thin or Groove now and then, 32+ bars apart, the kick half-time
  at most, never gone); 70-100 Breaks too (shorter, 4-8 bars, rarer than now). The kick four outside Thin and Break;
  the kick's shape, the hats' pattern, the bass's plan redrawn every few phrases (Evolve). Live breaks stay
  (PERFORM's Energy, Drop). The recipes mostly at 30-40.
  - **On the knob's ring:** brighter marks at 40 and 70 (the regions' edges).
  - **HOME's shake of everything** keeps Motion inside its region.
- **The virtual dub mixer** (off the 8-bar grid): slow, continuous moves of the filter, the echo's send and feedback,
  the hats' level, the noise's level, each 10-60 s, starting at any bar; the structural changes (the kick, the
  chord) on phrase lines but rarer. (Today 46% of the changes fall on an 8-bar line, the classics 24%.)
- **The kick's rumble** (built, b93e56e; **out of 1.3.0**, b517faf: "not yet to standard, we need more work on it";
  EN_RUMBLE 0, its code and its parameter P_RUMBLE kept, saved; Pressure's 35 since the user's "too much" at 60): for
  1.4.0, more work on its sound. The kick into a dark reverb under ~150 Hz, ducked by the next kick; its knob in Hat len's
  place on the Drums sound page (then all the kick's there; Hat len back to the recipes). A new parameter (a format
  change, converted).
- **Smaller engine features:** hats on the four "ands" (an auto pattern); named bass figures (BCD's x.....x.x.....x.)
  as grooves; kick shapes 3+3+2 and a four leaning on 1 and 3; a dotted-half echo time; a lower chord voicing; a
  reggae offbeat chop (a stab pattern); intro and outro (for a set or a recording).
- **CPU: the hot DSP code from RAM: tried, not kept** (2026-10-23). The core's stall counters don't run on this chip
  (F--, both orders); the engine in RAM (b757564: 62 KB in a FAST region) gave the same averages (Fog 30s, Haze
  50s-60s; flash a tad lower) and evened the peaks to 80-82 (Haze, Chamber down a little, Fog up from 63-70): the
  load is the engine's own work, not the cache. Reverted (cacd975), the counters' test too. What's left for the CPU:
  the engine's work itself (the noise machine, the voices, the plate), and the safety valve (from 75%).
- **(The study, kept for the record)** (the user's peaks to the low 90s on the FM-1, none on a Mac).
  The wl82 has a 32 KB I-cache (8 ways x 4 KB, 32 B lines) over SPI flash (XIP); the engine's hot code is ~33 KB
  (render_chunk 26.5 KB with reverb, echoes, stabs, crackle, cloud inlined; render_drums 2.3, fm_sample 1.9,
  render_bells 0.9, widen 0.9, render_hat 0.8), more than the cache, and the UI between the blocks (draw_main 9 KB,
  ~60 KB of font read from XIP) evicts it: the likely cause. All internal SRAM runs code (the SDK puts its RTOS and
  decoders there: .volatile_ram_code into ram0); POOL's "(rw)" is only the linker's label. Plan:
  1. **Measure first:** the core's counters (C0_IF_UACNT, C0_TL_CKCNT at 0x1EEE200.., to confirm: instruction-fetch
     misses?) read around render_chunk in a debug build, shown on the CPU page; if misses are the peaks, go on.
  2. **A FAST region:** POOL 0x54000 -> 0x44000, FAST (rwx) 0x01C64000, 64 KB, holding .ram_text and a new .fast_text
     (> FAST AT > XIP), copied by main_fm1.c's existing loop before the guard; the PC guard's window 1 over both.
  3. **Tag** the hot functions .fast_text; -fno-jump-tables for amb.c (its jump tables in XIP); its constant tables
     into RAM too (or accept their reads).
  4. **build.py:** calls inside FAST land inside it (the flash-driver part still call-free); FAST's size limit; the
     pool's 8 KB headroom with the new size.
  Risks: the ISR's entry stays in flash; the guard no longer protects that range; wait states unconfirmed.
- **Song structure: an intro and an outro** (the user, 2026-10-23: "a big change, plan it carefully"; not now, to
  plan before building). DUB-1 is endless today: it starts full and never ends. The references: the AI session's
  intros and outros take 24% of its bars (4-58% by song); the classics' US branch 21-27% (Echospace, Intrusion);
  BCD and M-Series start nearly full. Questions to settle first:
  - **When:** always (each piece a song: in, the body, out), or a mode (Endless / Song), or played (PLAY held: an outro
    from here; at start, an intro first)? A set (one piece into the next) and a recording are the main uses.
  - **How long:** the intro and outro in phrases (2-4: 16-32 bars), or set by a length for the whole song (5, 8, 12
    minutes)? Does the body's length stay endless till you ask for the outro?
  - **The order the layers come in** (and leave, the other way): e.g. the noise and the pad first, the hats, the
    kick, the bass last (the session's and Echospace's way); or the kick first (Basic Channel's). Per piece, from the
    palette?
  - **With what's built:** Motion's regions, the held loop, the virtual dub mixer (the filter and the echo opening
    over the intro), PERFORM's Energy and Drop, Lock; the phrase plan and the kick's shapes (an intro on the kick's
    ramp from none to four?).
  - **Between pieces:** an outro into the next piece's intro (a DJ's blend, same tempo or a tempo walk), PRESET
    turned during an outro.
  - **The UI:** where it's started and stopped, what the screen shows (the song's part, bars left), how it's said
    for the reader.
  - **Saved:** in the piece (its intro's length and order), the project format.
- **Ring marks elsewhere** (the user, 2026-10-23: Motion's marks "could be used in other places"; built for Motion,
  83b1ff9). A mark wherever a knob's behaviour changes at a point, not smoothly; to build with the UI review:
  - **Hits** (Drums): 15 (the open hat comes in, on the off-beats), 40 (two more), 70 (the rim on two and four).
  - **Kick density** (Drums sound / Drums): 90 (from here four in every Full bar) and the ladder's shapes at 25, 50,
    75 (one, half, ghosted: where the draw centres).
  - **Energy** (PERFORM): -25 (from here the next phrase Thin) and +25 (Full, with a fill into it).
  - **Notes** (Bass): 30 (under it the B bars never fill).
  - **Evolve** (Dub): 40 (the playing as loose as it gets; past it only the change's rate rises).
  - Not for smooth knobs (levels, Feedback, Width): their notch (0) or nothing.
  - Possibly two marks' colours: white for "something new comes in", red for "something drops out" (Motion's 70:
    the Breaks).
- **Later: a review of the UI:** the main ways the user shapes a piece, which knobs and where; to improve the UX and
  the clarity, separating **structure** from **sonority**.

## Releases after 1.2.0 (the user's ideas, 2026-10-19)

1.2.0 is about live performance. Then, in roughly this order:

### Sound: noise, the bass, the stereo field

- **Noise as a substantial part of DUB-1**, maybe a layer of its own (to plan: a sixth lane? see "Study: a
  sixth lane"). Start from the ideas taken from the musicologist's video ("Understanding Dub Techno",
  Bahadırhan Koçer; the list above): noise as the canvas, the backcloth (hum and filtered tape noise), the
  crackle tuned to the key and sent into the echo, noise in the kick and the sub. Also read the notes from
  his talk on dub techno; the user would like to get his book.
- **The bass:** more grooves, and slow groove changes within a song (the pattern moving over phrases, as the
  chord does with Drift).
- **The stereo field:** panning that moves (layers or echo repeats shifting between left and right,
  modulated), so the soundscape breathes across the stereo image.

### Composition: your own melodic lines

- A way to write a melodic line, for the bass or another layer (the pings, a new lead), and keep it with
  the piece. Builds on 1.1.0's ping lock and the pattern page's step entry on the keys.

### Connectivity: the output split in two

- **A split mode:** some layers on the left output, the rest on the right (mono each), so each can be
  treated on its own in a mixer (the user's Behringer Xenyx 1200) or an effects unit (the Korg NTS-3).
  The stereo field is lost in that mode, but nothing like it exists in the FM-1 firmware ecosystem; other
  firmware authors would appreciate it. The details are in "Ideas for later": the Korg NTS-3 (the send
  voice, the return, sync).


### A sidekick firmware: a second FM-1 that plays along with DUB-1 (the user's idea, 2026-10-19)

Many people own several FM-1s, mostly to keep different firmwares ready. The user is getting a second one. The idea is a
**companion firmware** for the second FM-1 that plays with DUB-1: it throws melodic lines, phrases or samples
over DUB-1's groove, in time and in key. It goes a bit beyond dub, and that's fine.

- **The link:** DUB-1 already sends MIDI clock and start/stop, and its notes (the chord, the pings, the bass,
  the drums, each on its own channel). The FM-1 sends MIDI out over USB only. It takes MIDI in over USB and
  the TRS jack. Two FM-1s therefore need a USB-MIDI host between them, such as a small host box, a computer
  or a phone. The second FM-1 then follows the clock and reads the key and the chord from DUB-1's notes.
- **To study:**
  - what DUB-1 should send for a companion: the root, the mode and the pool as CC or SysEx, the section
    (Full, Break, Thin), the phrase bar, dub hold, so the sidekick can answer the arrangement;
  - which devices act as a host at the lowest cost;
  - whether the TRS jack could carry clock from a host;
  - what the sidekick plays: phrases from a scale-aware generator, a sample player (if the flash holds
    enough), call and response with the pings.
- Related: "Connectivity: the output split in two", and the MIDI notes in section 9 of the manual.

### The screen: richer visualisations (the user's idea, 2026-10-19)

- **More information in the ripples.** The rings could carry more of the music, for example:
  - each layer's hits in its own colour or shape;
  - the echo's feedback and its band-pass in the rings' width or blur;
  - the section (Full, Break, Thin) in the field's density;
  - the master actions (Filter, Dub, Space) bending or blurring the field as they're turned.
- **More views on ALGORITHM** besides ripples, scope and spectrum. First, survey other projects for ideas that fit a
  240×240 screen and the FM-1's CPU: Winamp/MilkDrop presets, Teenage Engineering's OP-1 and OP-Z screens, Elektron's
  scopes, demoscene effects (plasma, tunnels, starfields), the Lissajous stereo scope, a tape-loop view of the echo's
  buffer, a waterfall spectrogram.

### An X-Y pad for playing live (the user's idea, 2026-10-19)

Use a touch pad as an X-Y controller for live performance: one finger moving two things at once, such as the Filter
and its resonance, or the echo's feedback and time, Dub and Space. The community would appreciate it.

- **The way in that works first: MIDI CC.** DUB-1 takes MIDI in over USB and the TRS jack, but only notes (the scale)
  and clock today. Mapping CCs to the master actions and a few parameters (a fixed map, or a "learn") would let any
  X-Y source drive it:
  - the **Korg NTS-3**, if it sends its pad as CC (to check: its MIDI implementation, and whether USB-C carries
    MIDI);
  - a phone or tablet app (TouchOSC and the like) through a USB-MIDI host;
  - a laptop's trackpad through a small app that turns it into MIDI.
- **An Apple trackpad over Bluetooth, directly: a long shot.** It's a Bluetooth HID device, so the FM-1 would need a
  Bluetooth radio and antenna, and a Bluetooth HID host stack. The chip family has Bluetooth (the SMK-37 runs
  Bluetooth MIDI on the same chip), but it's unknown whether the FM-1's board has the radio and antenna fitted. To
  research:
  - the FM-1's board;
  - the SDK's Bluetooth stack: HID host support, its code size, and the CPU it leaves for the sound;
  - how the trackpad pairs.
- **The SMK-37 already has 16 pressure pads and a mod wheel.** The pads' pressure could also play X-Y-like moves.
- First steps:
  1. MIDI CC in for the master actions (small, useful on its own);
  2. check the NTS-3's CC output;
  3. then the Bluetooth question.
