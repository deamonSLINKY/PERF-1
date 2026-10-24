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
    used = {k["p"] for g in groups for k in g["knobs"]}
    other = []

    def add(i, label):
        if i not in used:
            other.append({"p": i, "label": label})
            used.add(i)

    for l in range(6):
        add(P["P_LANE"] + 3 * l, LANES[l] + " level")
        add(P["P_LANE"] + 3 * l + 1, LANES[l] + " tune")
        add(P["P_LANE"] + 3 * l + 2, LANES[l] + " decay")
        add(P["P_PROB"] + l, LANES[l] + " probability")
        add(P["P_LLEN"] + l, LANES[l] + " length (steps)")
    for l in range(5):
        add(P["P_PAN"] + l, LAYERS[l] + " pan")
        add(P["P_SEND"] + l, LAYERS[l] + " send")
    nice = {"P_NOISE": "Noise level", "P_NZ_COLOR": "Noise colour", "P_NZ_TONE": "Noise tone", "P_NZ_PUMP": "Noise pump", "P_BASSOCT": "Drone octave",
            "P_AC_OCT": "Acid octave", "P_OCT": "Song octave", "P_PAD_OCT": "Pad octave", "P_CM_OCT": "Pluck octave", "P_MODE": "Mode (scale)",
            "P_LINKS": "Links on", "P_PAD_MAN": "Pad by hand", "P_DR_MAN": "Drone by hand", "P_AC_MAN": "Acid by hand", "P_CM_MAN": "Pluck by hand"}
    for n, v in P.items():
        if n in ("P_NPARAMS", "P_STYLE", "P_LANE", "P_ECHO") or n.startswith("P_LANE"):
            if n != "P_ECHO":
                continue
        if n != "P_NPARAMS" and v not in used and v < len(params):
            add(v, nice.get(n, params[v]["n"]))
    out = {"params": params, "groups": groups, "other": other, "alg": algorithms(), "lanes": LANES, "layers": LAYERS,
           "enum": {k: v for k, v in P.items() if k in ("P_CM_SND", "P_CM_LVL", "P_NPARAMS")}}
    json.dump(out, open(OUT, "w", encoding="utf-8"), separators=(",", ":"))
    sys.stdout.write("params.json: %d parameters, %d pages, %d other, %d algorithms (%d B)\n" % (len(params), len(groups), len(other), len(out["alg"]), os.path.getsize(OUT)))


if __name__ == "__main__":
    main()
