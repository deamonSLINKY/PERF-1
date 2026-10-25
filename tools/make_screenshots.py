#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The pictures of the screen, from the firmware itself run in the simulator:

  tests/run_tests.sh                      (the scenarios write their screenshots to build/scenarios/)
  python3 tools/make_screenshots.py       (copies the chosen ones to docs/img/perf-screen-*.png, for the manual, and
                                           makes the README's pictures, docs/img/readme-*.png)

Needs Pillow. Re-run after a change to the screen, then commit docs/img."""
import sys
from pathlib import Path
from PIL import Image

SRC = Path(__file__).resolve().parents[1]
SC = SRC / "build" / "scenarios"
IMG = SRC / "docs" / "img"

MANUAL = {                                  # docs/img/perf-screen-NAME.png: scenario/shot
    "home": "imp/imp_tempo", "pad": "imp/imp_pad", "acid-filter": "imp/imp_acid", "pluck": "imp/imp_pluck", "mixer": "imp/imp_mixer",
    "global": "imp/imp_global", "pad-env": "first/pad_env", "chords": "chords/chords_9ths", "master": "first/master",
    "reverb": "first/reverb", "space": "first/space", "kit": "sounds/kit", "links": "links/links_on", "drum-drift": "drumdrift/drumdrift",
    "seq": "seq/seq", "seq-tricks": "seqtricks/seq_tricks", "fx-menu": "fx/fx_latched", "beat-fx": "beatfx/beatfx_bank",
    "flex": "flex/flex_bank", "snips": "sampler/snips_held", "gate": "fxedit/gate_page", "sweep": "fxscopes/fxs_sweep",
    "glo-mix": "keymap/keymap_perf_glo", "shift": "keymap/keymap_shift", "perform": "perf/perf_screen", "settings": "settings/settings",
    "store": "store/global_stored", "breakdown": "sections/breakdown", "build": "sections/build", "jam": "jam/jam_pluck",
    "remote": "remote/remote_link", "saver": "stage/saver", "help": "help/help_lfo", "viz-disco": "viz/viz_disco",
    "viz-kaleido": "viz/viz_kaleido", "groove": "arrange/groove", "acid-answer": "answer/answer_page",
}
BG = (20, 14, 12)


def shot(name):
    return Image.open(SC / (name + ".png")).convert("RGB")


def sheet(names, cols, out, scale=2, gap=16):
    ims = [shot(n).resize((240 * scale, 240 * scale), Image.NEAREST) for n in names]
    rows = (len(ims) + cols - 1) // cols
    w, h = 240 * scale, 240 * scale
    img = Image.new("RGB", (cols * w + (cols + 1) * gap, rows * h + (rows + 1) * gap), BG)
    for i, im in enumerate(ims):
        img.paste(im, (gap + (i % cols) * (w + gap), gap + (i // cols) * (h + gap)))
    img.save(IMG / out, optimize=True)
    print("wrote", out, img.size)


def main():
    miss = [n for n in MANUAL.values() if not (SC / (n + ".png")).exists()]
    if miss:
        sys.exit("missing screenshots (run tests/run_tests.sh first): " + ", ".join(miss))
    for name, src in MANUAL.items():
        shot(src).save(IMG / f"perf-screen-{name}.png", optimize=True)
    print(len(MANUAL), "manual screenshots")
    sheet(["imp/imp_tempo", "sections/build", "sections/breakdown"], 3, "readme-sigil.png")
    sheet(["arrange/groove", "answer/answer_page", "seq/seq", "fx/fx_latched", "perf/perf_screen", "sampler/snips_page"], 3, "readme-pages.png")
    sheet(["imp/imp_tempo", "imp/theme_synth", "imp/theme_acid", "imp/theme_ice", "imp/theme_ember", "imp/theme_phosphor"], 3, "readme-palettes.png")


main()
