#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The boot screen's pixel red panda (firmware/src/app/ui.c, PANDA) as the site's mark:
docs/img/panda.png (1 px a pixel, transparent; scale it up with image-rendering: pixelated) and
docs/img/favicon.png (64 x 64).   python3 tools/site/panda.py"""
import re
from pathlib import Path
from PIL import Image

SRC = Path(__file__).resolve().parents[2]
COL = {"R": (200, 84, 32), "D": (140, 52, 20), "T": (232, 156, 92), "W": (246, 236, 222), "B": (64, 30, 22),
       "K": (24, 16, 18)}


def sprite():
    ui = (SRC / "firmware" / "src" / "app" / "ui.c").read_text()
    body = re.search(r"PANDA\[PANDA_H\] = \{(.*?)\};", ui, re.S).group(1)
    rows = re.findall(r'"([^"]*)"', body)
    im = Image.new("RGBA", (len(rows[0]), len(rows)), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c in COL:
                im.putpixel((x, y), COL[c] + (255,))
    return im


def main():
    im = sprite()
    out = SRC / "docs" / "img"
    im.save(out / "panda.png")
    fav = Image.new("RGBA", (34, 34), (0, 0, 0, 0))
    fav.paste(im, ((34 - im.width) // 2, (34 - im.height) // 2))
    fav.resize((68, 68), Image.NEAREST).crop((2, 2, 66, 66)).save(out / "favicon.png")
    print("wrote docs/img/panda.png", im.size, "and docs/img/favicon.png")


if __name__ == "__main__":
    main()
