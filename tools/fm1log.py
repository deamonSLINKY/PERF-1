#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PERF-1 telemetry logger: reads the status frames the firmware sends over USB-MIDI (SysEx) and writes a CSV.

    python tools/fm1log.py --list                 the MIDI ports
    python tools/fm1log.py                        log to telemetry/fm1_<time>.csv until Ctrl-C (and a live line)
    python tools/fm1log.py --port FM-1 --out x.csv
    python tools/fm1log.py --selftest             the decoder, without a device

The device sends nothing until this program asks (F0 7D 50 46 01 F7, repeated every 3 s as a keep-alive);
ten seconds without one and it stops. Needs: pip install mido python-rtmidi
"""
import argparse
import csv
import os
import struct
import sys
import time

PREFIX = [0x7D, 0x50, 0x46]
FIELDS = ["seq", "ms", "cpu", "cpu1", "core2", "xruns", "late", "max_us", "last_us", "nested", "shed", "playing",
          "stores", "store_us", "crashes", "renders", "ui_frames", "budget_us", "gap_us", "ui_us"]


def pack7(data):
    acc = nb = 0
    out = []
    for b in data:
        acc |= b << nb
        nb += 8
        while nb >= 7:
            out.append(acc & 0x7F)
            acc >>= 7
            nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return out


def unpack7(data):
    acc = nb = 0
    out = []
    for b in data:
        acc |= (b & 0x7F) << nb
        nb += 7
        if nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return out


def decode(msg):
    """msg: the SysEx data bytes without F0/F7. Returns a dict, or None if it is not a status frame."""
    msg = list(msg)
    if msg[:3] != PREFIX:
        return None
    raw = unpack7(msg[3:])
    n = len(FIELDS) * 4
    if len(raw) < n:
        return None
    return dict(zip(FIELDS, struct.unpack("<%dI" % len(FIELDS), bytes(raw[:n]))))


def selftest():
    w = list(range(1, len(FIELDS) + 1))
    msg = PREFIX + pack7(struct.pack("<%dI" % len(w), *w))
    d = decode(msg)
    assert d and [d[k] for k in FIELDS] == w, d
    assert decode([0x00, 0x59, 0x11]) is None
    print("selftest ok:", len(msg) + 2, "bytes a frame")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--port", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    import mido
    if a.list:
        print("in :", mido.get_input_names())
        print("out:", mido.get_output_names())
        return
    ins, outs = mido.get_input_names(), mido.get_output_names()
    key = a.port.lower()
    pick = lambda names: next((n for n in names if (key and key in n.lower()) or (not key and any(s in n.lower() for s in ("fm-1", "fm1", "perf", "m-vave", "mvave")))), None)
    pin, pout = pick(ins), pick(outs)
    if not pin or not pout:
        sys.exit("no FM-1 MIDI port found (ports: in %s, out %s). Use --port NAME." % (ins, outs))
    out = a.out or os.path.join("telemetry", time.strftime("fm1_%Y%m%d_%H%M%S.csv"))
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    print("in:", pin, "| out:", pout, "| csv:", out)
    fin, fout = mido.open_input(pin), mido.open_output(pout)
    arm = mido.Message("sysex", data=PREFIX + [0x01])
    last_arm = 0.0
    prev = None
    t0 = time.time()
    with open(out, "w", newline="") as f:
        cw = csv.writer(f)
        cw.writerow(["t"] + FIELDS + ["event"])
        try:
            while True:
                now = time.time()
                if now - last_arm > 3.0:
                    fout.send(arm)
                    last_arm = now
                for m in fin.iter_pending():
                    if m.type != "sysex":
                        continue
                    d = decode(m.data)
                    if not d:
                        continue
                    ev = []
                    if prev:
                        if d["xruns"] > prev["xruns"]:
                            ev.append("XRUN+%d" % (d["xruns"] - prev["xruns"]))
                        if d["stores"] > prev["stores"]:
                            ev.append("FLASH(%dus)" % d["store_us"])
                        if d["seq"] != prev["seq"] + 1:
                            ev.append("LOST%d" % (d["seq"] - prev["seq"] - 1))
                        if d["shed"] != prev["shed"]:
                            ev.append("SHED%d" % d["shed"])
                        if d["crashes"] != prev["crashes"]:
                            ev.append("CRASH")
                    if d["max_us"] > d["budget_us"]:
                        ev.append("OVER")
                    if d["gap_us"] > 20000:
                        ev.append("GAP%dms" % (d["gap_us"] // 1000))
                    prev = d
                    cw.writerow(["%.2f" % (now - t0)] + [d[k] for k in FIELDS] + [" ".join(ev)])
                    f.flush()
                    print("\r%6.1fs cpu %3d%% c1 %3d%% xr %-4d max %5.1fms/%.1f gap %5.1fms ui %5.1fms shed %d %s        " % (
                        now - t0, d["cpu"], d["cpu1"], d["xruns"], d["max_us"] / 1000, d["budget_us"] / 1000,
                        d["gap_us"] / 1000, d["ui_us"] / 1000, d["shed"], " ".join(ev)), end="", flush=True)
                time.sleep(0.01)
        except KeyboardInterrupt:
            pass
        finally:
            try:
                fout.send(mido.Message("sysex", data=PREFIX + [0x00]))
            except Exception:
                pass
    print("\nwritten:", out)


if __name__ == "__main__":
    main()
