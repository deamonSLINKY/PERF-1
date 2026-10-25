#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The web editor's data (build/params.json), from the firmware itself so it cannot drift from it:

  * every parameter (name, range, default, the names of its values): tests/host/gparams.c compiled against the engine
  * the unit's own pages and what their four knobs are: firmware/src/app/ui.c (VIEW_NAME, VIEW_KNOB, KNOB_LABEL)
  * the DX7 algorithms' routing: firmware/src/dsp/fm6_core.inc (FM6_ALG)

    python3 tools/make_params.py            (needs cc; run it where the release is built)
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
AMB_H = os.path.join(ROOT, "firmware", "src", "dsp", "amb.h")
UI_C = os.path.join(ROOT, "firmware", "src", "app", "ui.c")
FM6 = os.path.join(ROOT, "firmware", "src", "dsp", "fm6_core.inc")
OUT = os.path.join(ROOT, "build", "params.json")

LANES = ["Kick", "Snare", "Clap", "Hat", "Open hat", "Perc"]
LAYERS = ["Pad", "Drone", "Acid", "Pluck", "Drums"]


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def param_enum():
    t = open(AMB_H, encoding="utf-8").read()
    end = t.index("P_NPARAMS")
    start = t.rindex("enum {", 0, end)
    body = strip_comments(t[start + len("enum {"):end + len("P_NPARAMS")])
    env = {"EN_NLANE": 6}
    names, cur = {}, 0
    for tok in [x.strip() for x in body.split(",") if x.strip()]:
        if "=" in tok:
            n, e = [s.strip() for s in tok.split("=", 1)]
            cur = int(eval(e, {}, dict(env, **names)))
        else:
            n = tok
        names[n] = cur
        cur += 1
    return names


def ui_tables(P):
    t = strip_comments(open(UI_C, encoding="utf-8").read())

    def block(name):
        m = re.search(r"%s\[[^\]]*\](?:\[[^\]]*\])?\s*=\s*\{" % re.escape(name), t)
        i = m.end()
        depth, j = 1, i
        while depth:
            j += 1
            depth += {"{": 1, "}": -1}.get(t[j], 0)
        return t[i:j]

    names = re.findall(r'"([^"]*)"', block("VIEW_NAME"))
    rows = re.findall(r"\{([^{}]*)\}", block("VIEW_KNOB"))
    labels = [re.findall(r'"([^"]*)"', r) for r in re.findall(r"\{([^{}]*)\}", block("KNOB_LABEL"))]
    groups = []
    for n, row, lab in zip(names, rows, labels):
        knobs = []
        for k, tok in enumerate([x.strip() for x in row.split(",")]):
            if tok in P:
                knobs.append({"p": P[tok], "label": lab[k] if k < len(lab) else tok})
        if knobs and n not in ("Jam", "Perform", "Visual", "Sequencer"):
            groups.append({"name": n, "knobs": knobs})
    return groups


def algorithms():
    t = open(FM6, encoding="utf-8").read()
    m = re.search(r"FM6_ALG\[32\]\[6\]\s*=\s*\{(.*?)\};", t, re.S)
    rows = re.findall(r"\{([^{}]*)\}", m.group(1))
    return [[int(x, 16) for x in r.split(",")] for r in rows]


def main():
    cc = os.environ.get("CC", "cc")
    exe = os.path.join(ROOT, "build", "host", "gparams")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    subprocess.check_call([cc, "-O1", "-std=c99", "-DEN_HOST", "-I" + os.path.join(ROOT, "firmware", "src", "dsp"), "-o", exe,
                           os.path.join(ROOT, "tests", "host", "gparams.c"), os.path.join(ROOT, "firmware", "src", "dsp", "amb.c"), "-lm"])
    params = json.loads(subprocess.check_output([exe]).decode("utf-8"))
    P = param_enum()
    assert len(params) == P["P_NPARAMS"], "the parameter table and the enum disagree"
    groups = ui_tables(P)
    byname = {g["name"]: g["knobs"] for g in groups}
    pl = lambda *items: [{"p": P[n], "label": lab} for n, lab in items]
    ls = lambda base, k, lab: {"p": P[base] + k, "label": lab}
    tabs = [
        {"name": "Tempo & global", "sections": [
            {"name": "Tempo", "knobs": byname["Tempo"]},
            {"name": "Global", "knobs": byname["Global"]},
            {"name": "Scale and octave", "knobs": pl(("P_MODE", "Mode (scale)"), ("P_OCT", "Song octave"))}]},
        {"name": "Pad", "sections": [
            {"name": "Pad", "knobs": byname["Pad"]},
            {"name": "Envelope (the breath)", "knobs": byname["Pad env"]},
            {"name": "LFO", "knobs": byname["Pad LFO"]},
            {"name": "Chords", "knobs": [k for k in byname["Chords"] if k["label"] != "Root"]},
            {"name": "Place", "knobs": pl(("P_PAD_OCT", "Octave"), ("P_PAD_MAN", "By hand")) + [ls("P_PAN", 0, "Pan"), ls("P_SEND", 0, "Send")]}]},
        {"name": "Drone", "sections": [
            {"name": "Drone", "knobs": byname["Drone"]},
            {"name": "Envelope", "knobs": byname["Drone env"]},
            {"name": "LFO", "knobs": byname["Drone LFO"]},
            {"name": "Bass line (the drone as notes)", "knobs": byname["Bass line"]},
            {"name": "Place", "knobs": pl(("P_BASSOCT", "Octave"), ("P_DR_MAN", "By hand")) + [ls("P_PAN", 1, "Pan"), ls("P_SEND", 1, "Send")]}]},
        {"name": "Acid", "sections": [
            {"name": "Acid", "knobs": byname["Acid"]},
            {"name": "Envelope and sound", "knobs": byname["Acid env"]},
            {"name": "The line", "knobs": byname["Acid line"] + pl(("P_AC_RESP", "Answer"), ("P_AC_EVERY", "Answer every (bars; or before a drum change)"), ("P_STYLE", "Answer sync (0 bar, 1 loop)"))},
            {"name": "Place", "knobs": pl(("P_AC_OCT", "Octave"), ("P_AC_MAN", "By hand")) + [ls("P_PAN", 2, "Pan"), ls("P_SEND", 2, "Send")]}]},
        {"name": "Pluck", "sections": [
            {"name": "Pluck", "knobs": byname["Pluck"]},
            {"name": "Behaviour", "knobs": byname["Pluck 2"]},
            {"name": "Deja vu (and the acid's Mutates)", "knobs": byname["Deja vu"]},
            {"name": "Place", "knobs": pl(("P_CM_MAN", "By hand")) + [ls("P_PAN", 3, "Pan"), ls("P_SEND", 3, "Send")]}]},
        {"name": "Drums", "sections": [
            {"name": "Drums", "knobs": byname["Drums"]},
            {"name": "Kit", "knobs": byname["Kit"]},
            {"name": "Links (drums into voices)", "knobs": pl(("P_LINKS", "Links on")) + byname["Links"]},
            {"name": "Drum drift", "knobs": byname["Drum drift"]},
            {"name": "Feel (ghost notes, accents, the pad's comping)", "knobs": byname["Feel"]},
            {"name": "Place", "knobs": [ls("P_PAN", 4, "Pan"), ls("P_SEND", 4, "Send")]}] + [
            {"name": LANES[l] + " lane", "knobs": [ls("P_LANE", 3 * l, "Level"), ls("P_LANE", 3 * l + 1, "Tune"), ls("P_LANE", 3 * l + 2, "Decay"),
                                                 ls("P_PROB", l, "Probability"), ls("P_LLEN", l, "Length (steps)"), ls("P_DENS", l, "Density (50: as written)"),
                                                 ls("P_LSRC", l, "Source pattern")]} for l in range(6)]},
        {"name": "Space & master", "sections": [
            {"name": "Space", "knobs": byname["Space"]},
            {"name": "Reverb", "knobs": byname["Reverb"]},
            {"name": "Master bus", "knobs": byname["Comp/Limit"]}]},
        {"name": "Mixer", "sections": [
            {"name": "Levels", "knobs": pl(("P_PAD", "Pad"), ("P_BASS", "Drone"), ("P_ACID", "Acid"), ("P_CM_LVL", "Pluck"), ("P_PULSE", "Drums"), ("P_NOISE", "Vinyl"))},
            {"name": "Pan", "knobs": [ls("P_PAN", k, LAYERS[k]) for k in range(5)]},
            {"name": "Send (echo and reverb)", "knobs": [ls("P_SEND", k, LAYERS[k]) for k in range(5)]},
            {"name": "Vinyl", "knobs": pl(("P_NZ_COLOR", "Wear"), ("P_NZ_PUMP", "Pump (the kick ducks it)"))}]},
    ]
    placed = {k["p"] for tb in tabs for s in tb["sections"] for k in s["knobs"]}
    unused = set()                     # kept for the saved banks' layout, nothing reads them
    left = [i for i in range(len(params)) if i not in placed and i not in unused]
    if left:                                                    # never lose a parameter: whatever no tab has yet
        tabs[-1]["sections"].append({"name": "More", "knobs": [{"p": i, "label": params[i]["n"]} for i in left]})
    other = left
    out = {"params": params, "tabs": tabs, "alg": algorithms(), "lanes": LANES, "layers": LAYERS,
           "enum": {k: v for k, v in P.items() if k in ("P_CM_SND", "P_CM_LVL", "P_NPARAMS")}}
    json.dump(out, open(OUT, "w", encoding="utf-8"), separators=(",", ":"))
    sys.stdout.write("params.json: %d parameters, %d tabs, %d sections, %d not on any tab, %d algorithms (%d B)\n" % (len(params), len(tabs), sum(len(x["sections"]) for x in tabs), len(left), len(out["alg"]), os.path.getsize(OUT)))


if __name__ == "__main__":
    main()
