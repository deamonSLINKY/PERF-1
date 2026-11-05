#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Ported to PERF-1 (0.16.17) from Melodee 1.3.5's tests/boot_stub_pkg_test.py.
# Modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca)
"""The package builder's side of the boot stub (tools/fm1pkg_make.py, docs/BOOT-SAFETY.md), without a
firmware build: two apps with the same stub and different bodies give a byte-identical sector 0x4000 (the directory
with app.bin's CRC-16 and the app_area_head block CRC, and the stub), while the body and the marker's sector differ;
the marker is where and what the stub reads (firmware/stub/stub_core.h); every CRC of the directory holds, computed
the slow way on the decrypted image; another stub changes the sector; an app without a stub is packed as before.

  python3 tests/boot_stub_pkg_test.py  (SDK: $AC79_SDK, else ~/fw-AC79_AIoT_SDK)
"""
import os
import random
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1pkg_make as P  # noqa: E402

if not os.environ.get("AC79_SDK"):
    P.SDK = Path.home() / "fw-AC79_AIoT_SDK"
fails = checks = 0


def ok(cond, what):
    global fails, checks
    checks += 1
    if not cond:
        fails += 1
        print("FAIL", what)


def plain(flash):
    """the app area as XIP shows it"""
    a = bytearray(flash[0x4000:])
    P.sfc(a, 0, len(a), 0, P.KEY)
    return bytes(a)


def stub_image(rng, version=1):
    s = bytearray(rng.randbytes(P.STUB_LEN))
    s[0:4] = bytes.fromhex("04818000")
    s[8:14] = P.STUB_SIG
    struct.pack_into("<H", s, 14, version)
    return bytes(s)


def directory_ok(area):
    """the app_area_head entry, the app.bin entry and both data CRCs, bit by bit"""
    blk = struct.unpack_from("<I", area, 8)[0]
    return (P.crc16_slow(area[2:32]) == struct.unpack_from("<H", area, 0)[0] and
            P.crc16_slow(area[32:blk]) == struct.unpack_from("<H", area, 2)[0] and
            P.crc16_slow(area[34:64]) == struct.unpack_from("<H", area, 32)[0] and
            P.crc16_slow(area[0x120:0x120 + P.APP_SLOT]) == struct.unpack_from("<H", area, 34)[0])


def main():
    rng = random.Random(7)
    stub = stub_image(rng)
    bodies = [bytes.fromhex("04818000") + rng.randbytes(n) for n in (439508, 200001, 4, P.BODY_MAX - 4)]
    flashes = [P.flash_image(stub + b, P.KEY) for b in bodies]
    areas = [plain(f) for f in flashes]
    for i, (b, f, a) in enumerate(zip(bodies, flashes, areas)):
        ok(f[0x4000:0x5000] == flashes[0][0x4000:0x5000], f"body {i}: sector 0x4000 identical to body 0's")
        ok(f[:0x4000] == flashes[0][:0x4000], f"body {i}: the head identical")
        ok(i == 0 or f[0x5000:0x6000] != flashes[0][0x5000:0x6000], f"body {i}: the body differs")
        ok(i == 0 or f[0x91000:0x92000] != flashes[0][0x91000:0x92000], f"body {i}: the marker's sector differs")
        ok(f[0x92000:] == flashes[0][0x92000:], f"body {i}: cfg_tool and the cfg directory identical")
        ok(directory_ok(a), f"body {i}: every directory CRC-16 holds (slow)")
        ok(struct.unpack_from("<H", a, 34)[0] == P.APP_CRC_FIXED, f"body {i}: app.bin's CRC-16 is the constant")
        ok(struct.unpack_from("<H", a, 2)[0] == struct.unpack_from("<H", areas[0], 2)[0], f"body {i}: the block CRC too")
        ok(a[0x120:0x120 + P.STUB_LEN] == stub, f"body {i}: the stub at flash 0x4120")
        padded = b + b"\xFF" * (-len(b) % 4)
        ok(a[0x1000:0x1000 + len(padded)] == padded, f"body {i}: the body from flash 0x5000")
        m = a[0x91FE0 - 0x4000:0x92000 - 0x4000]
        magic, fmt, ln, crc, mcrc = struct.unpack_from("<IIIII", m, 0)
        ok((magic, fmt, ln) == (0x4D425243, 1, len(padded)), f"body {i}: marker magic, format, length")
        ok(crc == zlib.crc32(a[0x1000:0x1000 + ln]) and mcrc == zlib.crc32(m[:16]), f"body {i}: marker CRCs")
        ok(m[24:] == b"\xFF" * 8, f"body {i}: nothing behind the compensation bytes")
        ok(a[0x1000 + len(padded):0x91FE0 - 0x4000] == b"\xFF" * (0x91FE0 - 0x5000 - len(padded)),
           f"body {i}: padding between the body and the marker")
    # the compensation's maths: any message, any place, any target
    for _ in range(200):
        n = rng.randrange(8, 3000)
        off = rng.randrange(0, n - 3)
        msg = bytearray(rng.randbytes(n))
        msg[off:off + 4] = bytes(4)
        want = rng.randrange(0x10000)
        msg[off:off + 4] = P.compensate(bytes(msg), off, want)
        ok(P.crc16_slow(msg) == want and P.crc16(msg) == want, "compensate: the CRC-16 comes out as asked")
    # the block CRC follows from app.bin's: one constraint, not two (BOOT-SAFETY.md, the maths)
    blk = struct.unpack_from("<I", areas[0], 8)[0]
    ok(len({P.crc16(a[32:blk]) for a in areas}) == 1, "the app_area_head block CRC is one value for all bodies")
    # another stub: the sector changes (and only then)
    other = P.flash_image(stub_image(rng) + bodies[0], P.KEY)
    ok(other[0x4000:0x5000] != flashes[0][0x4000:0x5000], "another stub: sector 0x4000 differs")
    ok(struct.unpack_from("<H", plain(other), 34)[0] == P.APP_CRC_FIXED and
       plain(other)[:0x120] == areas[0][:0x120], ".. its directory (0x4000..0x411F) still the same bytes")
    v2 = P.flash_image(stub_image(random.Random(7), 2) + bodies[0], P.KEY)
    ok(P.stub_version(stub) == 1 and v2[0x4000:0x5000] != flashes[0][0x4000:0x5000], "a version bump alone differs")
    # a wrong marker: only the marker's sector differs
    bad = P.flash_image(stub + bodies[0], P.KEY, lambda body: P.marker(body)[:12] + bytes(8))
    ok([s for s in range(0, len(bad), 0x1000) if bad[s:s + 0x1000] != flashes[0][s:s + 0x1000]] == [0x91000],
       "a package with a wrong marker differs in the marker's sector only")
    ok(directory_ok(plain(bad)), ".. and its directory CRCs hold")
    # no stub: packed as before (no marker, the app from flash 0x4120, its own CRC)
    legacy = bodies[1]
    lf = P.flash_image(legacy, P.KEY)
    la = plain(lf)
    ok(not P.has_stub(legacy) and la[0x120:0x120 + len(legacy)] == legacy and
       la[0x120 + len(legacy):0x120 + P.APP_SLOT] == b"\xFF" * (P.APP_SLOT - len(legacy)), "no stub: no marker, 0xFF to the slot's end")
    ok(struct.unpack_from("<H", la, 34)[0] == P.crc16(legacy + b"\xFF" * (P.APP_SLOT - len(legacy))) and directory_ok(la),
       "no stub: the directory as before")
    try:
        P.flash_image(stub + bytes(P.BODY_MAX + 4), P.KEY)
        ok(False, "a body past the marker's sector is refused")
    except SystemExit:
        ok(True, "")
    print(f"stub_pkg_test: {checks} checks, {fails} failed")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
