#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The deamonSLINKY mascot (the sprite of the boot card in firmware/src/app/ui.c, its colours) as PNGs for the web pages:
web/img/slinky.png (a logo, 4 px a pixel) and web/img/favicon.png (32 x 32).  python tools/make_logo.py
The pages carry them inline (tools/make_logo.py --inline rewrites the data: URIs in web/amb_installer.html)."""
import base64
import io
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
UI = os.path.join(HERE, "..", "firmware", "src", "app", "ui.c")
PAL = {"K": (27, 16, 34), "T": (27, 16, 34), "H": (242, 230, 196), "h": (196, 173, 126), "Y": (255, 212, 71),
       "R": (224, 52, 79), "r": (156, 29, 58), "L": (255, 143, 143)}


def sprite():
    t = open(UI, encoding="utf-8").read()
    m = re.search(r"static const char \*const MASCOT\[23\] = \{(.*?)\};", t, re.S)
    rows = re.findall(r'"([^"]*)"', m.group(1))
    assert len(rows) == 23 and all(len(r) == 23 for r in rows), "the sprite changed shape"
    return rows


def render(rows, scale, size=None):
    w = 23 * scale
    im = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c in PAL:
                for dy in range(scale):
                    for dx in range(scale):
                        im.putpixel((x * scale + dx, y * scale + dy), PAL[c] + (255,))
    if size:
        out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        out.paste(im, ((size - w) // 2, (size - w) // 2))
        return out
    return im


def png(im):
    b = io.BytesIO()
    im.save(b, "PNG", optimize=True)
    return b.getvalue()


def main():
    rows = sprite()
    logo, fav = png(render(rows, 4)), png(render(rows, 1, 32))
    os.makedirs(os.path.join(HERE, "..", "web", "img"), exist_ok=True)
    open(os.path.join(HERE, "..", "web", "img", "slinky.png"), "wb").write(logo)
    open(os.path.join(HERE, "..", "web", "img", "favicon.png"), "wb").write(fav)
    print("slinky.png %d B, favicon.png %d B" % (len(logo), len(fav)))
    if "--inline" in sys.argv:
        d1 = "data:image/png;base64," + base64.b64encode(logo).decode()
        d2 = "data:image/png;base64," + base64.b64encode(fav).decode()
        for name in ("amb_installer.html", os.path.join("editor", "index.html")):
            p = os.path.join(HERE, "..", "web", name)
            t = open(p, encoding="utf-8").read()
            t = re.sub(r'(<img class="logo" src=")[^"]*(")', lambda m: m.group(1) + d1 + m.group(2), t)
            t = re.sub(r'(<link rel="icon" type="image/png" href=")[^"]*(")', lambda m: m.group(1) + d2 + m.group(2), t)
            open(p, "w", encoding="utf-8", newline="\n").write(t)
            print("inlined into", p)


if __name__ == "__main__":
    main()
