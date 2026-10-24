#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# AMB-1 host tests: the maths, the platform pieces kept from Felucca / X0X, the instrument, and the
# whole firmware app in the simulator (tests/scenarios/*.amb: UI, chords, rhythm, persistence), with
# screenshots and audio in build/scenarios/.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
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
run core2 sh -c "$CC -O2 -ffp-contract=off -std=c11 -Wall -Wextra -DEN_HOST -Ifirmware/src/dsp -o $OUT/core2_test tests/host/core2_test.c firmware/src/dsp/amb.c -lm -pthread && \
    s=\$($OUT/core2_test 0) && d=\$($OUT/core2_test 1) && t=\$($OUT/core2_test 2) && m=\$($OUT/core2_test 3) && $OUT/core2_test 4 && $OUT/core2_test 5 && \
    [ \"\$($OUT/core2_test 0 - 1 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 2 - 1 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 - 2 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 2 - 2 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 2 - 0 0 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 3 - 0 0 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 2 - 0 2 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 3 - 0 2 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 1 - 0 2 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 0 - 0 -1 1 | cut -d' ' -f2)\" ] && \
    [ \"\$($OUT/core2_test 0 - 1 | cut -d' ' -f2)\" = \"\$($OUT/core2_test 0 - 1 -1 1 | cut -d' ' -f2)\" ] && \
    [ \"\${s#* }\" = \"\${d#* }\" ] && [ \"\${s#* }\" = \"\${t#* }\" ] && [ \"\${s#* }\" = \"\${m#* }\" ] && echo \$s \$d \$t \$m"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
if command -v node >/dev/null 2>&1; then run websyx node tests/web_syx_test.js; fi
run params sh -c "python3 tools/make_params.py"
if command -v node >/dev/null 2>&1; then
    run webvoice sh -c "$CC -O1 -std=c99 -DEN_HOST -Ifirmware/src/dsp -o $OUT/gvoice tests/host/gvoice.c firmware/src/dsp/amb.c -lm && $OUT/gvoice > $OUT/voices.json && node tests/web_voice_test.js $OUT/voices.json"
fi
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/amb.fwsc ]; then
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/amb.fwsc"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/amb.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/amb.fwsc"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/amb.fwsc: run ./build.sh)"
fi
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
