#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Summarise a telemetry CSV (tools/fm1log.py): by genre and piece, the load, the xruns, and where the first core's time goes.
    python tools/fm1sum.py telemetry/fm1_....csv"""
import csv
import sys
from collections import defaultdict

GENRES = ["HYPNO", "LOFI", "HOUSE", "DUBSTEP", "JUNGLE", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP"]
FIELDS = ["p_clear", "p_bassacid", "p_pluck", "w_pad", "p_merge", "p_drums", "p_echo", "w_rev", "p_master"]
STAGES = ["clear", "bassacid", "pluck", "w_pad", "merge", "drums", "echo", "w_rev", "master"]


def main():
    rows = list(csv.DictReader(open(sys.argv[1])))
    if not rows:
        sys.exit("empty")
    g = defaultdict(list)
    prev = None
    for r in rows:
        r = {k: (int(v) if v.lstrip("-").isdigit() else v) for k, v in r.items()}
        r["dx"] = r["xruns"] - prev["xruns"] if prev else 0
        prev = r
        g[(r.get("genre", 0), r.get("piece", 0))].append(r)
    print("%-9s %2s %5s %4s %4s %3s %6s %6s | us per 5.8 ms chunk: %s" % ("genre", "pc", "secs", "cpu", "max", "xr", "peak", "gap", " ".join("%7s" % s for s in STAGES) + "  isr% chor bass"))
    for (ge, pc), s in sorted(g.items()):
        ch = sum(x.get("chunks", 0) for x in s) or 1
        st = ["%7.0f" % (sum(x.get(f, 0) for x in s) / ch) for f in FIELDS]
        print("%-9s %2d %5.0f %3.0f%% %3d%% %3d %5.1fms %5.1fms | %s" % (
            GENRES[ge] if ge < len(GENRES) else ge, pc, len(s) * 0.25, sum(x["cpu"] for x in s) / len(s), max(x["cpu"] for x in s),
            sum(x["dx"] for x in s), max(x["max_us"] for x in s) / 1000, max(x["gap_us"] for x in s) / 1000, " ".join(st) + "  %4.1f%% %3d%% %3d%%" % (100.0 * sum(x.get("isr_in", 0) for x in s) / (len(s) * 250000.0), 100.0 * sum(1 for x in s if x.get("chorus", 0)) / len(s), 100.0 * sum(1 for x in s if x.get("bass", 0)) / len(s))))


def peaks(path):
    """the slowest chunks the unit saw (columns pk_*): which stage took the time, at which step, and what had been moved"""
    rows = list(csv.DictReader(open(path)))
    if not rows or "pk_total" not in rows[0]:
        return
    names = ["clear", "bass+acid", "pluck+DX+snips", "wait pad", "merge+sidechain", "drums", "echoes", "wait reverb", "master"]
    ranked = sorted((r for r in rows if int(r["pk_total"])), key=lambda r: -int(r["pk_total"]))[:8]
    print("\nthe slowest chunks (a chunk is at most 256 frames = 5.8 ms; the two of a render must fit 11.6 ms):")
    for r in ranked:
        st = [int(r["pk_" + n]) for n in ("clear", "bassacid", "pluck", "w_pad", "merge", "drums", "echo", "w_rev", "master")]
        info = int(r["pk_info"])
        top = max(range(9), key=lambda i: st[i] if i not in (3, 7) else -1)
        print("  %5.1f s  %s %2d  %5d us (%d frames) step %2d%s  biggest %s %d us  waits %d+%d  %s%s%s" % (
            float(r["t"]), GENRES[int(r["genre"])] if int(r["genre"]) < len(GENRES) else r["genre"], int(r["piece"]), int(r["pk_total"]), (info >> 4) & 255, info & 15,
            " (just after a step)" if info >> 12 & 1 else "", names[top], st[top], st[3], st[7], "drone+acid on core 2 " if info >> 13 & 1 else "",
            "chorus on core 1 " if info >> 14 & 1 else "", "shed %d" % (info >> 15 & 3)))
    hist = [sum(int(r["h_" + k]) for r in rows) for k in ("lt5", "5", "6", "7", "8", "9", "10", "11", "ge12")]
    tot = sum(hist) or 1
    print("renders by time (ms): " + "  ".join("%s %.1f%%" % (lab, 100.0 * h / tot) for lab, h in zip(["<5", "5", "6", "7", "8", "9", "10", "11", ">=12"], hist)))


peaks(sys.argv[1])
main()
