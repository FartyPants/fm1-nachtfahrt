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
