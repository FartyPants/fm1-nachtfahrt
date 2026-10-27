#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUB-1 in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [EN_VERSION=0.1] web/emu/build.sh  ->  build/emu/{index.html, worklet.js, amb.wasm, reader/}
# reader/: the screen reader's words and voice (words.js, tts.js, tts_worker.js, espeak/), copied from
# fm1-reader (github.com/charlesvestal/fm1-reader, GPL-3.0; a sibling checkout, or FM1_READER=its web/ folder).
# Without it the emulator still works: its screen is said in the screen's own text, to the page's live region.
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device).
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DEN_HOST -DEN_WEB "-DEN_VERSION=\"$(printf %s "${EN_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/amb.wasm web/emu/amb_web.c firmware/src/dsp/amb.c
cp web/emu/index.html web/emu/worklet.js build/emu/
READER="${FM1_READER:-../fm1-reader/web}"
rm -rf build/emu/reader
if [ -f "$READER/words.js" ] && [ -f "$READER/espeak/espeak.wasm" ]; then
    mkdir -p build/emu/reader/espeak
    cp "$READER/words.js" "$READER/tts.js" "$READER/tts_worker.js" build/emu/reader/
    cp "$READER/espeak/espeak.js" "$READER/espeak/espeak.wasm" "$READER/espeak/LICENSE" "$READER/espeak/README.md" build/emu/reader/espeak/
else
    echo "emu: no fm1-reader at $READER (set FM1_READER): the screen reader says the screen's own text, with no voice of its own"
fi
echo "emu: build/emu ($(wc -c < build/emu/amb.wasm) B wasm)"
