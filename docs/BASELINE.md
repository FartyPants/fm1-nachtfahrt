# DUB-1 baseline (measured 2026-10-27, before any BERLIN change)

Machine: Windows 10, WSL Ubuntu 24.04, clang 18.1.3, Emscripten 6.0.13.

## Test suite on the untouched engine

`tests/run_tests.sh`: everything passes except two failures that come with the fork, not with BERLIN:

| Test | Why it fails | What happens to it |
|---|---|---|
| `recipe-prints` | `tools/gen_recipe_prints.py` walks DUB-1's git history from tag `v1.0.0`; the fork's history starts at the fork | BERLIN's own recipe prints are made in Task 13 |
| `app` | `tests/host/app_test.mjs` expects Chamber's tempo 118; DUB-1's recipe (and manual) say 128: a stale DUB-1 test | rewritten for BERLIN in Task 12 |

Two Linux-only build fixes were needed (macOS does both implicitly): `-D_GNU_SOURCE` on the host compiles
(`clock_gettime` under `-std=c99`) and `-lm` on the engine test's link line.

## CPU (the host proxy)

`tests/listen/baseline.sh`: each piece for a minute, three runs, "render % of real time" (host CPU; the
simulator now prints two decimals, since one decimal is about 10% of these values).

| Script | Piece | Runs | Median | Peak |
|---|---|---|---|---|
| baseline_0 | Basic | 0.83 0.83 0.82 | 0.83 | −7.4 dBFS |
| baseline_1 | Chamber | 0.99 0.98 0.99 | 0.99 | −4.9 dBFS |
| baseline_2 | Steppers | 0.84 0.86 0.84 | 0.84 | −6.7 dBFS |
| baseline_3 | Fog | 0.74 0.73 0.73 | 0.73 | −6.0 dBFS |
| baseline_4 | Concrete | 0.86 0.84 0.89 | 0.86 | −5.2 dBFS |
| baseline_5 | Night Drive | 0.88 0.90 0.87 | 0.88 | −3.7 dBFS |
| baseline_6 | Pressure | 0.94 0.91 0.92 | 0.92 | −6.6 dBFS |
| baseline_7 | Haze | 1.00 1.01 0.99 | **1.00** | −9.6 dBFS |
| baseline_stress | Night Drive, Density 100, Cloud 100 (checked: `par4`, `par32` = 100) | 0.98 0.97 0.92 | 0.97 | −7.0 dBFS |

Run-to-run spread: about ±3%.

**BERLIN's CPU budget: 1.00 × 1.10 = 1.10% of real time** (the heaviest median, Haze), measured the same way.

DUB-1's device readings for scale (its PLAN.md, 2026-10-16, not re-measured): Basic 20/50% now/peak, Night Drive
30/67%, Night Drive stressed 50/72%. The host is about 80× faster than the device.

## Sizes (host compile of `firmware/src/app/app.h`)

- `P_NPARAMS` = **127**
- `sizeof(slot_t)` = 340 bytes; `sizeof(project_t)` = **3004** of the 3840-byte payload (836 spare)

(Host struct layout; the device compiler is also clang and the `_Static_assert` in `project.c` checks it there.)

## Memory on the device (DUB-1's PLAN.md, not re-measured until the device build)

RAM 74.7 of 96 KB; the buffer pool 332 of 344 KB; `tools/build.py` fails a build with under 8 KiB of pool spare.

# BERLIN 0.1 (measured 2026-10-27, Task 15)

Same machine and method (`tests/listen/baseline.sh`, each piece a minute from its start, three runs). The first
minute of Timewind, Rubycon and Mirage is mostly Emerge (lighter); the stress run has no Emerge.

| Script | Piece | Runs | Median | Peak |
|---|---|---|---|---|
| baseline_0 | Timewind | 0.83 0.69 0.65 | 0.69 | −15.7 dBFS |
| baseline_1 | Wahnfried | 0.64 0.63 0.66 | 0.64 | −10.1 dBFS |
| baseline_2 | Phaedra | 0.71 0.71 0.70 | 0.71 | −7.6 dBFS |
| baseline_3 | Rubycon | 0.59 0.60 0.61 | 0.60 | −17.2 dBFS |
| baseline_4 | Stratosfear | 0.73 0.75 0.74 | 0.74 | −4.3 dBFS |
| baseline_5 | Sunrain | 0.83 0.85 0.82 | 0.83 | −3.8 dBFS |
| baseline_6 | Mirage | 0.81 0.78 0.79 | 0.79 | −17.0 dBFS |
| baseline_7 | Cyborg | 0.58 0.59 0.58 | 0.58 | −16.3 dBFS |
| baseline_stress | Sunrain: Density, Repeat A and B, Sequence 2, Phaser, Phrases at 100 | 0.84 0.85 0.85 | 0.85 | −3.7 dBFS |

The effects scenario (`tests/scenarios/effects.amb`: each of the sixteen held in turn, and all sixteen at once)
ran at 0.98%. **Everything is within the 1.10% budget; nothing was cut.** The device's own readings wait for the
device build (PLAN.md).

# BERLIN with the filter motion (measured 2026-10-27, filter-motion Task 7)

Same machine and method. No Emerge any more: every piece plays all its layers from the first bar (the Intros only
close their filters), so the first minute is heavier than in 0.1's table above.

| Script | Piece | Runs | Median | Peak |
|---|---|---|---|---|
| baseline_0 | Timewind | 0.71 0.73 0.72 | 0.72 | −6.4 dBFS |
| baseline_1 | Wahnfried | 0.64 0.61 0.62 | 0.62 | −8.8 dBFS |
| baseline_2 | Phaedra | 0.80 0.83 0.81 | 0.81 | −5.0 dBFS |
| baseline_3 | Rubycon | 0.70 0.70 0.72 | 0.70 | −5.2 dBFS |
| baseline_4 | Stratosfear | 0.80 0.83 0.81 | 0.81 | −3.9 dBFS |
| baseline_5 | Sunrain | 0.89 0.94 0.87 | 0.89 | −3.9 dBFS |
| baseline_6 | Mirage | 1.02 1.01 1.01 | **1.01** | −5.0 dBFS |
| baseline_7 | Cyborg | 0.57 0.59 0.57 | 0.57 | −14.8 dBFS |
| baseline_stress | Sunrain: Density, Repeat A and B, Sequence 2, Phaser, Phrases at 100 | 0.94 0.93 0.99 | 0.94 | −1.3 dBFS |

**Every median within the 1.10% budget** (the heaviest, Mirage, 1.01: 0.09 left for the Lead-memory plan, whose own
budget is the same 1.10%).

# BERLIN with the Lead's memory (measured 2026-10-28, Lead-memory Task 6)

Same method; a busier session than the table above, so the base (202f76e, before Memory went on the Arc page) was
measured in it too. The peaks are the same to the 0.1 dB.

| Script | Piece | Runs | Median | Base median |
|---|---|---|---|---|
| baseline_0 | Timewind | 0.79 0.87 0.82 | 0.82 | 0.80 |
| baseline_1 | Wahnfried | 0.78 0.74 0.78 | 0.78 | 0.75 |
| baseline_2 | Phaedra | 0.83 0.83 0.83 | 0.83 | 0.83 |
| baseline_3 | Rubycon | 0.70 0.74 0.70 | 0.70 | 0.72 |
| baseline_4 | Stratosfear | 0.83 0.87 0.84 | 0.84 | 0.84 |
| baseline_5 | Sunrain | 0.97 0.91 0.92 | 0.92 | 0.90 |
| baseline_6 | Mirage | 1.02 1.05 1.02 | **1.02** | 1.03 |
| baseline_7 | Cyborg | 0.68 0.66 0.74 | 0.68 | 0.65 |
| baseline_stress | Sunrain, stressed | 1.13 0.94 0.94 | 0.94 | 0.95 |

**Every median within the 1.10% budget**; the differences from the base are the host's noise (±0.03).
