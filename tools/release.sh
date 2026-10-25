#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# A release build of PERF-1: the editor's parameter data, the manual's page, the tests, the firmware package and the browser emulator.
#   tools/release.sh VERSION [--no-tests]        e.g. tools/release.sh 0.13.0
# Then:  python3 tools/make_pages_site.py VERSION build/site   (the installer, emulator, editor and manual as a static site)
# See BUILDING.md for the toolchain (JIELI_TOOLCHAIN, AC79_SDK), clang with the wasm target and node.
set -e
cd "$(dirname "$0")/.."
V="${1:?usage: tools/release.sh VERSION [--no-tests]}"
python3 tools/make_params.py
python3 tools/manual_html.py
if [ "$2" != "--no-tests" ]; then
    tests/run_tests.sh
fi
./build.sh --release "$V"
EN_VERSION="$V" web/emu/build_clang.sh | tail -1
ls -la "build/amb-$V.fwsc"
