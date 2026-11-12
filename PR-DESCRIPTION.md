# NACHTFAHRT on the M-VAVE SMK-37 Elite

This adds a second board to NACHTFAHRT: the **M-VAVE SMK-37 Elite**. The FM-1 build is unchanged
byte for byte — every change sits behind `BOARD_SMK37`, which defaults to 0, and the FM-1 image still
hashes to `8a2c70ed…` (427124 B) exactly as on `v1.0.0`.

The port is based on tag **v1.0.0** and has been running on real SMK-37 Elite hardware: it boots,
plays, and the panel, wheels, pads and transport all work.

## Why a second board at all

The SMK-37 Elite is the same AC79 family as the FM-1, with a keyboard rather than a 27-key pad grid,
motor faders, 16 velocity pads, two wheels and 19 panel LEDs. The sound engine, the UI and the
sequencer all run unchanged; what differs is the layer underneath them.

## What the port does

**Board switch.** `BOARD_SMK37` in `firmware/hal/fm1_cc.h`; the build selects the HAL, the panel
table, the storage objects and the packaging from it.

**Input** (`firmware/hal/smk_input.h`, new). The FM-1's input API over the SMK's own scan: 18 buttons,
37 keys, 8 encoders, 16 velocity pads over the mux, two wheels, and the 19 LED lines. The scan runs in
the 10 kHz timer ISR as on the FM-1, so nothing about the main loop timing changed.

**Flash map** (`firmware/hal/fm1_flash_map.h`, new). The SMK has 1 MiB and its own layout. The port
only ever writes `0x9D000..0xBB000` (data), `0xBB000..0xC0000` (OTA) and `0xC2000..0xEA000` (samples),
and refuses everything else with `_Static_assert` guards. It never touches `0x0..0x3FFF`,
`0xC0000..0xC1FFF` (BTIF + the USRTRIM calibration at `0xC1000`, which is irreversible),
`0xF4000..0xFDFFF` (stock USR) or `0xFF000..` (key_mac). This was checked in the disassembly: the RAM
flash driver only ever tests the port's own ranges, and `0xC1000` does not appear in the image at all.

**Display.** The LCD is on `JL_SPI1` at `0x11D00`; `0x11E00` is the key-scan shift register. Driving
the display on the wrong SPI is what kept the screen black in the first builds.

**Panel.** 14 app buttons on the SMK's own fixed matrix table, 5 knobs, 19 LEDs. The SMK has no
per-key LEDs, so `plat_leds` ignores its keys argument.

**Pads and keys.** The owner's design: the 16 pads carry the FM-1's **black-key effects** (Take-off,
Supernova, Twitter, Repeat, Shimmer, Freeze, Octave, Return, Rise, Close, Transpose), red when off,
green when on, yellow for the pads without a function; the SMK's own black keys **play notes**. The
pad RGB ring is 16 WS-class LEDs on SPI1 in a 420-byte frame, driven from the main loop only — never
from the scan ISR.

**The keybed is a piano.** Every key of the SMK (white and black) goes through `en_lead_midi()`, and
the ten outer keys are mapped to the surrounding octaves. The FM-1 branch keeps `en_lead_key` plus the
black-key effects, untouched.

**Wheels.** Pitch is `bend / 4096` semitones (±2 over `±8192`); mod adds up to the full `P_VIB` depth.

**Packaging** (`tools/fm1pkg_make.py`, `--board smk37`). The SMK's update image, its record layout
(`<HHHHIII`, as `sdk/Package.cs`), the BANKCB-encoded bootloader with a plain CRC, the SFC cipher with
one key per 32-byte block mixed with the block's own offset, and a product identity the owner can
swap. `BOARD_SMK37=1 python tools/build.py` writes `build/smk37.fwsc`.

## Two bugs worth naming

Both were found on hardware and would have been easy to miss:

- **A queued command with no handler.** `smk_wheel()` posted `C_WHEEL` to the engine's 512-entry
  command queue, but `drain()` had no `case` for it. The queue filled and every later command —
  including `en_play(1)` — was silently dropped.
- **PLAY that played nothing.** The TRS MIDI-in (FM-1 only) set up UART1 RX on PH8. On the SMK, PH8
  is one of the eight key-matrix return lines, so the UART fought the scan, and what it read arrived
  as MIDI real-time messages. `midi_in()` turns `en_ext` on for those, and `en_render` counts no bars
  while `en_ext` is set — so PLAY lit up and waited for an outside clock. `EN_TRS` is 0 for the SMK.

## How it was checked

- **The FM-1 build is byte-identical**: `amb.bin 8a2c70ed…` (427124 B), `loader/ota.bin 021cbefe…`
  (6510 B), `amb.fwsc 40ec51ca…` — all as on `v1.0.0`.
- **Host tests**: the full suite passes, 39/39 scenarios, plus three new SMK tests
  (`tests/smk_input_test.c`, `tests/smk_outer_test.c`, `tests/smk_store_test.c`) and a transport test
  in `tests/host/amb_test.c` (start, stop, start again).
- **`tests/ota_test.c`** passes against `build/smk37.fwsc`.
- **On hardware**: boots, plays, PLAY restarts with its LED green again, pads and black keys behave as
  described, wheels move the Lead.

## Notes for review

- The SMK product identity in the package is a placeholder (`SMK-37 Elite_8000001`) so that anyone can
  build and flash their own; it is one constant to change.
- The panel mapping is data, not guesswork: it lives in `smk37-port.yaml` at the repository root.
- Everything is `BOARD_SMK37`-only. `firmware/src/dsp/amb.c` gained ~66 lines, all under the switch.
