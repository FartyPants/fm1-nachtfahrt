#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# AMB-1 host tests: the maths, the platform pieces kept from Felucca / X0X, the instrument, and the
# whole firmware app in the simulator (tests/scenarios/*.amb: UI, chords, rhythm, persistence), with
# screenshots and audio in build/scenarios/.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc} -D_GNU_SOURCE"          # (Linux hides clock_gettime and friends under -std=c99)
FAIL=0
OUT=build/host
mkdir -p "$OUT" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
run amb sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Ifirmware/src/dsp \
    -o $OUT/amb_test tests/host/amb_test.c firmware/src/dsp/amb.c -lm && $OUT/amb_test"
run golden sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Ifirmware/src/dsp \
    -o $OUT/golden_test tests/host/golden_test.c firmware/src/dsp/amb.c -lm && $OUT/golden_test"
run seq sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -Ifirmware/src/dsp -o $OUT/seq_test tests/host/seq_test.c firmware/src/dsp/seq.c && $OUT/seq_test"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
# the flash driver's wait for an erase / page program (never back to XIP while the part is busy)
run flash-wait sh -c "$CC -O1 -w -o $OUT/flash_wait_test tests/flash_wait_test.c && $OUT/flash_wait_test"
# the update handshake's identity: cut for M-UPGRADE 3.11 (FM-1_197010001 -> FM-1_197), whole to the installers; a
# short identity's; the update loader's
run ota-identity sh -c "$CC -std=c11 -O1 -w -o $OUT/ota_hs_long tests/ota_hs_test.c && $OUT/ota_hs_long && \
    $CC -std=c11 -O1 -w '-DTEST_ID=\"FM-1_197\"' -o $OUT/ota_hs_short tests/ota_hs_test.c && $OUT/ota_hs_short && \
    $CC -std=c11 -O1 -w -DFELUCCA_LOADER=1 -o $OUT/ota_hs_loader tests/ota_hs_test.c && $OUT/ota_hs_loader"
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/amb.fwsc ]; then
    # FM1_STOCK_DIR: a folder with M-VAVE's official packages (FM-1.fwsc V15 / V16), to check the pins
    STOCK=""
    V16=""
    if [ -n "${FM1_STOCK_DIR:-}" ]; then
        STOCK=$(ls "$FM1_STOCK_DIR"/*.fwsc 2>/dev/null | tr '\n' ' ')
        for f in $STOCK; do   # the loader test takes V16 (its flash.bin is 0xA9000)
            if [ "$(sha256sum "$f" | cut -c1-16)" = d017fb65e70aca97 ]; then V16="$f"; fi
        done
    fi
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/amb.fwsc $STOCK"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/amb.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/amb.fwsc $V16"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/amb.fwsc: run ./build.sh)"
fi
# tools/fm1_rescue.py's write policy against a simulated UBOOT (both flash parts; V15 / V16 with FM1_STOCK_DIR)
run rescue python3 tests/rescue_test.py
run host-build sh host/build_host.sh
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh web/emu/build.sh >/dev/null && node tests/host/emu_test.mjs build/emu/amb.wasm"
    run app sh -c "sh web/app/build.sh >/dev/null && node tests/host/app_test.mjs build/app/amb.wasm"
fi
for s in tests/scenarios/*.amb; do
    n=$(basename "$s" .amb)
    mkdir -p "build/scenarios/$n"
    run "scenario-$n" build/host/amb_host "$s" "build/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
