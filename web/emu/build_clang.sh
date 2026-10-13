#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The emulator without Emscripten: plain clang (wasm32 target) and wasm-ld, with the few libc pieces
# the code uses supplied by web/emu/shim. Same output as web/emu/build.sh.
#   web/emu/build_clang.sh  ->  build/emu/{index.html, worklet.js, amb.wasm}
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
clang --target=wasm32 -O2 -ffp-contract=off -std=gnu99 -nostdlib -ffreestanding -w \
    -DEN_HOST -DEN_WEB "-DEN_VERSION=\"$(printf %s "${EN_VERSION:-DEV}" | tr a-z A-Z)\"" \
    -Iweb/emu/shim -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    -Wl,--no-entry -Wl,--export-dynamic -Wl,--initial-memory=33554432 -Wl,-z,stack-size=1048576 \
    -o build/emu/amb.wasm web/emu/amb_web.c firmware/src/dsp/amb.c web/emu/shim/libc.c
cp web/emu/index.html web/emu/worklet.js build/emu/
echo "emu: build/emu ($(wc -c < build/emu/amb.wasm) B wasm)"
