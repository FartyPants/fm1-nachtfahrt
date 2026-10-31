#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# A nightly (test) build for the FM-1 that leaves the release alone: build it, make its installer page and stage the
# files for the site's nightly/ folder. Nothing is pushed: publishing build/nightly_stage/ is a separate step (the
# handoff, "Nightly"): its nightly/ goes to gh-pages under nightly/, its words/ is added to words/ (new files only).
#   tools/nightly.sh [X.Y.Z]      default 0.9.99 (identity FM-1_197000999, apart from every release's), shown on the
#                                  screen and the page as X.Y.Z-NIGHTLY
set -e
cd "$(dirname "$0")/.."
N="${1:-0.9.99}"
V="$N-nightly"
./build.sh --release "$V"
PKG="build/nachtfahrt-$V.fwsc"
EN_VERSION="$V" sh web/emu/build.sh || true         # (it exits non-zero when fm1-reader's voice is not bundled, which is normal)
[ -f build/emu/amb.wasm ] || { echo "the emulator was not built (build/emu/amb.wasm)"; exit 1; }
python3 tools/make_pages.py "$PKG" "$V" build/site_nightly --nightly
S=build/nightly_stage
rm -rf "$S"
mkdir -p "$S/nightly"
cp -r build/site_nightly/install build/site_nightly/firmware build/site_nightly/emu build/site_nightly/index.html "$S/nightly/"
cp -r build/site_nightly/words "$S/nightly/words"
cp -r build/site_nightly/words "$S/words"
echo "staged: $S/nightly (install/, emu/, firmware/, index.html) and $S/words (the screen reader's files)"
