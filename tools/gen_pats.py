#!/usr/bin/env python3
"""The pieces' factory drum patterns, as grids: writes firmware/src/dsp/pats.h (included by amb.c).
Each piece has six patterns of six lanes (kick, snare, clap, closed hat, open hat, perc), 16 steps:
x a hit, . a rest. Roughly: 1 an intro, 2-4 the groove building, 5 a variation, 6 a tool (thin or
broken), so turning Pattern from 1 to 6 is an arrangement."""
import os

LANES = ("K", "S", "C", "H", "O", "P")
_ = "................"

PIECES = [
  ("01", "hypnotic four on the floor (Hypnos)", [
    ("the floor", "x...x...x...x...", _, "....x.......x...", "..x...x...x...x.", _, _),
    ("driving", "x...x...x...x...", _, "....x.......x...", "xx.xxx.xxx.xxx.x", "..x...x...x...x.", ".......x.......x"),
    ("rolling", "x...x...x...x.x.", "....x.......x...", _, ".x.x.x.x.x.x.x.x", "......x.......x.", "...x.......x...."),
    ("acid house", "x...x...x...x...", _, "....x.......x...", "xxxxxxxxxxxxxxxx", "..x.......x.....", "...x...x..x...x."),
    ("broken", "x..x..x...x.x...", "....x.......x...", _, "..x...x...x...x.", "...............x", ".........x......"),
    ("hypnotic", "x...x...x...x...", _, _, "..x...x...x...x.", _, "...x..x....x..x."),
  ]),
  ("02", "deep and dubby, rims and shakers (Undertow)", [
    ("pulse", "x...x...x...x...", _, _, _, _, "...x.......x...."),
    ("shaker", "x...x...x...x...", _, _, "..x...x...x...x.", _, "...x..x....x...."),
    ("rim", "x...x...x...x...", ".......x......x.", _, "..x.x.x...x.x.x.", _, "...x..x....x..x."),
    ("deep", "x...x...x...x...", _, "....x.......x...", "x.xxx.xxx.xxx.xx", "..x.......x.....", "...x..x....x..x."),
    ("skip", "x...x...x..x..x.", ".......x........", _, "..x...x...x...x.", "..............x.", "...x.....x..x..."),
    ("no kick", _, ".......x......x.", _, "..x...x...x...x.", "..x.......x.....", "...x..x....x..x."),
  ]),
  ("03", "pounding, rumble and claps (Monolith)", [
    ("hammer", "x...x...x...x...", _, _, "..x...x...x...x.", _, _),
    ("rumble", "x..xx..xx..xx..x", _, "....x.......x...", "..x...x...x...x.", _, _),
    ("pressure", "x..xx..xx..xx..x", _, "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", _),
    ("industrial", "x..xx..xx..xx..x", "....x.......x..x", "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "x..x..x..x..x..."),
    ("stomp", "x...x...x.x.x...", "....x.......x...", "....x.......x.x.", "x.x.x.x.x.x.x.x.", "..x.......x.....", ".....x.........x"),
    ("tunnel", "x...............", _, "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "x..x..x..x..x..."),
  ]),
  ("04", "trance: offbeat opens, claps, a roll (Ascension)", [
    ("lift", "x...x...x...x...", _, _, _, "..x...x...x...x.", _),
    ("anthem", "x...x...x...x...", _, "....x.......x...", _, "..x...x...x...x.", _),
    ("gallop", "x...x...x...x...", _, "....x.......x...", "x.xxx.xxx.xxx.xx", "..x...x...x...x.", _),
    ("peak", "x...x...x...x...", "...............x", "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "...x...x...x...x"),
    ("roll", "x...x...x...x...", "x.x.x.x.xxxxxxxx", "....x.......x...", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", _),
    ("air", _, _, "....x.......x...", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", _),
  ]),
  ("05", "acid electro, cowbells and breaks (Night Drive)", [
    ("cruise", "x...x...x...x...", _, "....x.......x...", "..x...x...x...x.", _, _),
    ("cowbell", "x...x...x...x...", _, "....x.......x...", "x.x.x.x.x.x.x.x.", _, "..x..x....x..x.."),
    ("electro", "x.....x...x.....", "....x.......x...", _, "xxxxxxxxxxxxxxxx", _, "..x..x....x..x.."),
    ("303 house", "x...x...x...x...", _, "....x.......x...", "xxxxxxxxxxxxxxxx", "..x.......x.....", "..x..x.x..x..x.x"),
    ("break", "x.....x.x.......", "....x.......x..x", _, "x.xxx.xxx.xxx.xx", "...........x....", "......x.......x."),
    ("night", "x...x...x...x...", _, _, "..x...x...x...x.", _, "x..x..x..x..x..."),
  ]),
  ("06", "dub techno: sparse, swung, a rim on the and (Dub Chamber)", [
    ("chamber", "x...x...x...x...", _, _, "..x...x...x...x.", _, _),
    ("rim", "x...x...x...x...", "..........x.....", _, "..x...x...x...x.", _, "......x.......x."),
    ("smoke", "x...x...x...x...", "..........x.....", _, "x.xxx.xxx.xxx.xx", "..x.......x.....", "......x.......x."),
    ("tape", "x...x...x...x...", "....x.....x.....", "............x...", "x.xxx.xxx.xxx.xx", "..x...x...x...x.", "......x..x....x."),
    ("drift", "x......x..x.....", "....x...........", _, "..x...x...x...x.", "..............x.", "......x.......x."),
    ("echo", "x...............", "..........x.....", _, "..x...x...x...x.", _, "......x.......x."),
  ]),
  ("07", "minimal, glitchy, threes against fours (Glass Tunnel)", [
    ("glass", "x...x...x...x...", _, _, "..x...x...x...x.", _, "x..x..x..x..x..."),
    ("shards", "x...x...x...x...", _, "....x.......x...", "..x...x...x...x.", _, "x..x..x..x..x..x"),
    ("click", "x...x...x...x...", ".x......x.x.....", "....x.......x...", "xx.x.xx.xx.x.xx.", _, "x..x..x..x..x..x"),
    ("prism", "x...x...x...x...", ".x......x.x.....", "....x.......x...", "xxxxxxxxxxxxxxxx", "..x.....x.......", "x..x..x..x..x..x"),
    ("stutter", "x.x...x...x..x..", "......x........x", _, "..x.x.x...x.x.x.", _, "x....x....x....x"),
    ("hollow", "x.......x.......", _, _, "xx.x.xx.xx.x.xx.", _, "x..x..x..x..x..x"),
  ]),
  ("08", "rolling, tom-like percussion swells (Tidal)", [
    ("low tide", "x...x...x...x...", _, _, "..x...x...x...x.", _, "..........x....."),
    ("current", "x...x...x...x...", _, "....x.......x...", "..x...x...x...x.", _, "..x.....x.x....."),
    ("swell", "x...x...x...x...", _, "....x.......x...", "x.xxx.xxx.xxx.xx", "......x.......x.", "..x..x..x.x..x.."),
    ("high tide", "x...x...x...x..x", "............x...", "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "x.x..x..x.x..x.x"),
    ("undertow", "x..x....x..x....", "....x.......x...", _, "..x...x...x...x.", "...............x", "..x..x..x.x..x.."),
    ("ebb", _, _, "....x.......x...", "..x...x...x...x.", _, "x.x..x..x.x..x.x"),
  ]),
  ("09", "warehouse: hard, rides and rumble", [
    ("doors", "x...x...x...x...", _, _, _, "..x...x...x...x.", _),
    ("floor", "x...x...x...x...", _, "....x.......x...", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", _),
    ("ride", "x..xx..xx..xx..x", _, "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", _),
    ("strobe", "x..xx..xx..xx..x", "....x.......x...", "....x.......x...", "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "x.x.x.x.x.x.x.x."),
    ("lurch", "x...x..x..x.x...", "....x.......x...", _, "xxxxxxxxxxxxxxxx", "..x.......x.....", "...x.......x...."),
    ("rattle", _, "x.xxx.xxx.xxx.xx", _, "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "x..x..x..x..x..."),
  ]),
  ("10", "deep field: half time, breaks, space", [
    ("field", "x.........x.....", _, _, "..x...x...x...x.", _, _),
    ("half", "x.........x.....", "........x.......", _, "..x...x...x...x.", _, "......x.......x."),
    ("break", "x.........xx....", "....x.......x...", _, "x.x.x.x.x.x.x.x.", "..............x.", ".......x........"),
    ("lift", "x...x...x...x...", "....x.......x...", _, "x.xxx.xxx.xxx.xx", "..x...x...x...x.", "......x.......x."),
    ("skitter", "x......x..x.....", "....x.......x..x", _, "xx.xx.xxx.xx.x.x", _, "...x........x..."),
    ("space", _, _, _, "..x...x...x...x.", "..............x.", "x..x..x..x..x..."),
  ]),
]

def bits(s):
    assert len(s) == 16 and set(s) <= set("x."), s
    return sum(1 << i for i, c in enumerate(s) if c == "x")

out = ["/* SPDX-License-Identifier: GPL-3.0-only */",
       "/* the pieces' factory drum patterns: written by tools/gen_pats.py (edit the grids there).",
       " * Bit s: a hit on step s; lanes: kick, snare, clap, closed hat, open hat, perc. */",
       "const uint16_t EN_PIECE_PATS[EN_NPIECES][EN_NPAT][EN_NLANE] = {"]
assert len(PIECES) == 10
for name, what, pats in PIECES:
    assert len(pats) == 6
    out.append(f"    /* {name}: {what} */")
    out.append("    {")
    for i, (pn, *lanes) in enumerate(pats):
        assert len(lanes) == 6
        out.append(f"        /* {i + 1} {pn}" + "".join(f"\n         *   {l} {s}" for l, s in zip(LANES, lanes)) + " */")
        out.append("        {" + ", ".join(f"0x{bits(s):04X}u" for s in lanes) + "},")
    out.append("    },")
out.append("};")
path = os.path.join(os.path.dirname(__file__), "..", "firmware", "src", "dsp", "pats.h")
open(path, "w").write("\n".join(out) + "\n")
print("wrote", os.path.normpath(path))
