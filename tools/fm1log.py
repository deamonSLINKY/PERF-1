#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PERF-1 telemetry logger: reads the status frames the firmware sends over USB-MIDI (SysEx) and writes a CSV.

    python tools/fm1log.py --list                 the MIDI ports
    python tools/fm1log.py                        log to telemetry/fm1_<time>.csv until Ctrl-C (and a live line)
    python tools/fm1log.py --port FM-1 --out x.csv
    python tools/fm1log.py --bench                time float / int / table / flash-versus-RAM loops on the chip (once; a glitch in the music)
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
          "stores", "store_us", "crashes", "renders", "ui_frames", "budget_us", "gap_us", "ui_us", "genre", "piece",
          "p_clear", "p_bassacid", "p_pluck", "w_pad", "p_merge", "p_drums", "p_echo", "w_rev", "p_master", "chunks", "isr_in", "isr_out", "batt"]


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
    d = dict(zip(FIELDS, struct.unpack("<%dI" % len(FIELDS), bytes(raw[:n]))))
    d["chorus"] = d["shed"] >> 8 & 1              # the pad's chorus on the first core (the firmware balances it)
    d["bass"] = d["shed"] >> 9 & 1                # the drone and the acid on the other core
    d["shed"] &= 0xFF
    return d


BENCH_NAMES = ["float mul-add x10 (flash code)", "float mul-add x10 (RAM code)", "float divide x4", "float<->int x4",
               "table lerp (flash table)", "table lerp (RAM table)", "rational soft clip (1 divide)", "integer chain x4", "RAM copy+sum (per word)"]


EXTRA = ["pk_clear", "pk_bassacid", "pk_pluck", "pk_w_pad", "pk_merge", "pk_drums", "pk_echo", "pk_w_rev", "pk_master", "pk_total", "pk_info",
         "h_lt5", "h_5", "h_6", "h_7", "h_8", "h_9", "h_10", "h_11", "h_ge12"]


def decode_extra(msg):
    """the second frame: the slowest chunk of the window (its stages in us, its total, its info: step 0-3, size 4-11, tick at start 12, the drone and
    acid on core 2 13, the chorus on core 1 14, shed 15-16, section 17-18) and how many renders took <5, 5-6 ... 11-12, >=12 ms"""
    msg = list(msg)
    if msg[:3] != [0x7D, 0x50, 0x5A]:
        return None
    raw = unpack7(msg[3:])
    if len(raw) < 80:
        return None
    return dict(zip(EXTRA, struct.unpack("<20I", bytes(raw[:80]))))


def decode_bench(msg):
    msg = list(msg)
    if msg[:3] != [0x7D, 0x50, 0x47]:
        return None
    raw = unpack7(msg[3:])
    if len(raw) < 64:
        return None
    return struct.unpack("<16I", bytes(raw[:64]))


def show_bench(w):
    bn, tpu = w[0], w[1]
    print("bench: %d turns each, %d ticks per us" % (bn, tpu))
    base = w[9] / bn
    for k, name in enumerate(BENCH_NAMES):
        n = bn if k < 8 else (bn // 64) * 256
        per = w[2 + k] / n
        print("  %-34s %8.2f ticks/turn = %7.1f ns  (%.2fx the int chain)" % (name, per, per / tpu * 1000, per / base))
    if w[3]:
        print("  RAM code runs the float loop in %.0f%% of flash code's time" % (100.0 * w[3] / w[2]))
    if w[7]:
        print("  RAM table is read in %.0f%% of the flash table's time" % (100.0 * w[7] / w[6]))
    print("  cache 0x1EEE008 %08X  0x1EEE00C %08X  0x1EEE010 %08X   PLL 0x16A00 %08X  0x10008 %08X" % tuple(w[11:16]))


def selftest():
    w = list(range(1, len(FIELDS) + 1))
    msg = PREFIX + pack7(struct.pack("<%dI" % len(w), *w))
    d = decode(msg)
    assert d and d.pop("chorus") == 0 and d.pop("bass") == 0 and [d[k] for k in FIELDS] == w, d
    assert decode([0x00, 0x59, 0x11]) is None
    wb = [1500, 240] + [3000 + 100 * k for k in range(9)]
    wb = wb + [7] * 5
    assert decode_bench(PREFIX[:2] + [0x47] + pack7(struct.pack("<16I", *wb))) == tuple(wb)
    we = list(range(1, 21))
    ex = decode_extra(PREFIX[:2] + [0x5A] + pack7(struct.pack("<20I", *we)))
    assert ex and [ex[k] for k in EXTRA] == we, ex
    print("selftest ok:", len(msg) + 2, "bytes a frame")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--port", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--bench", action="store_true")
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
    print("in:", pin, "| out:", pout, "| csv:", out)
    if a.bench:
        fin, fout = mido.open_input(pin), mido.open_output(pout)
        fout.send(mido.Message("sysex", data=PREFIX + [0x02]))
        t_end = time.time() + 5
        while time.time() < t_end:
            for m in fin.iter_pending():
                if m.type == "sysex":
                    w = decode_bench(m.data)
                    if w:
                        return show_bench(w)
            time.sleep(0.02)
        sys.exit("no answer (is it the build with the bench?)")
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    fin, fout = mido.open_input(pin), mido.open_output(pout)
    arm = mido.Message("sysex", data=PREFIX + [0x01])
    last_arm = 0.0
    prev = None
    extra = {}
    t0 = time.time()
    with open(out, "w", newline="") as f:
        cw = csv.writer(f)
        cw.writerow(["t"] + FIELDS + ["chorus", "bass"] + EXTRA + ["event"])
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
                        ex = decode_extra(m.data)
                        if ex:
                            extra = ex
                        continue
                    ev = []
                    if prev:
                        if d["xruns"] > prev["xruns"]:
                            ev.append("XRUN+%d" % (d["xruns"] - prev["xruns"]))      # a half that was never rendered: its buffer played again
                        if d["late"] > prev["late"]:
                            ev.append("LATE+%d" % (d["late"] - prev["late"]))        # a render that ran past its half (the next one started at once)
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
                    cw.writerow(["%.2f" % (now - t0)] + [d[k] for k in FIELDS] + [d["chorus"], d["bass"]] + [extra.get(k, 0) for k in EXTRA] + [" ".join(ev)])
                    extra = {}
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
