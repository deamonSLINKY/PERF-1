#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Ported to PERF-1 (0.16.17) from Melodee 1.3.5's tests/boot_fixtures.py.
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Synthetic HOST-ONLY packages for the interrupted-update model. Never install.

Run after a firmware build; every fixture is under build/host/boot-fixtures.
The new image is the actual built package. Changed bodies deliberately contain
invalid instructions, so these fixtures test flash state rather than hardware.
"""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import fm1pkg_make as P

P.SDK = os.environ.get('AC79_SDK', str(Path.home() / 'fw-AC79_AIoT_SDK'))
app = (ROOT / 'build/amb.bin').read_bytes()
loader = (ROOT / 'build/loader/ota.bin').read_bytes()
assert P.has_stub(app), 'build firmware with the boot stub first'
stub, body = app[:P.STUB_LEN], app[P.STUB_LEN:]
other_body = bytes(b ^ 0x55 for b in body)
old_stub = bytearray(stub)
old_stub[14:16] = (2).to_bytes(2, 'little')
fixtures = {
    'legacy': body,
    'other-legacy': other_body,
    'same-stub': stub + other_body,
    'old-stub': bytes(old_stub) + other_body,
}
out = ROOT / 'build/host/boot-fixtures'
out.mkdir(parents=True, exist_ok=True)
(out / 'NEVER-INSTALL.txt').write_text('Synthetic HOST-ONLY test images. Never install on hardware.\n')
for name, image in fixtures.items():
    (out / (name + '.fwsc')).write_bytes(P.ufw(P.flash_image(image, P.KEY), loader, 'FM-1_900'))
print('Created four host-only boot fixtures')
