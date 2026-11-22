#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The Firmware Hub card (docs/card.png): three of the unit's own screens, from the host scenarios (docs/img, tools/make_screenshots.py),
and the release's line. Square, 720 x 720, under the hub's 300 KB.

  python3 tools/make_hub_card.py 1.0            (needs Pillow; the fonts are the project's own, assets/fonts, OFL)"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SRC = Path(__file__).resolve().parents[1]
IMG = SRC / "docs" / "img"
FONT = SRC / "assets" / "fonts"
AMBER, BG, PANEL, WHITE, GREY = (255, 176, 0), (12, 12, 16), (28, 28, 36), (245, 245, 245), (150, 150, 160)


def screen(name, scale):
    im = Image.open(IMG / f"perf-screen-{name}.png").convert("RGB")
    return im.resize((im.width * scale, im.height * scale), Image.NEAREST)   # (whole pixels: the screen's own)


def main():
    ver = sys.argv[1] if len(sys.argv) > 1 else "1.0"
    W = 720
    card = Image.new("RGB", (W, W), BG)
    d = ImageDraw.Draw(card)
    big = ImageFont.truetype(str(FONT / "ChakraPetch-Bold.ttf"), 92)
    mid = ImageFont.truetype(str(FONT / "ChakraPetch-Bold.ttf"), 46)
    small = ImageFont.truetype(str(FONT / "ChakraPetch-SemiBold.ttf"), 25)
    # the banner: PERF-1 and the release
    d.text((24, 6), "PERF-1", font=big, fill=WHITE)
    tw = d.textlength("PERF-1", font=big)
    d.text((24 + tw + 22, 44), f"{ver} RELEASE!", font=mid, fill=AMBER)
    d.rectangle((24, 120, W - 24, 123), fill=AMBER)
    # the screens: Perform big (2x, whole pixels), the acid filter and a visualizer beside it (200 px), each in a thin frame
    y0, s = 140, 200
    small1 = Image.open(IMG / "perf-screen-acid-filter.png").convert("RGB").resize((s, s), Image.LANCZOS)
    small2 = Image.open(IMG / "perf-screen-viz-kaleido.png").convert("RGB").resize((s, s), Image.LANCZOS)
    for im, (x, y) in ((screen("perform", 2), (12, y0)), (small1, (W - 12 - s, y0 + 10)), (small2, (W - 12 - s, y0 + 480 - 10 - s))):
        d.rectangle((x - 3, y - 3, x + im.width + 2, y + im.height + 2), fill=PANEL)
        card.paste(im, (x, y))
    # between the small ones: it runs on the newer FM-1s too (their UC25HQ80 flash chip, since 0.17.0)
    note = ImageFont.truetype(str(FONT / "ChakraPetch-Bold.ttf"), 22)
    cx, cy = W - 12 - s // 2, y0 + 240
    d.text((cx, cy - 13), "WORKS ON", font=note, fill=WHITE, anchor="mm")
    d.text((cx, cy + 13), "NEWER FM-1s TOO", font=note, fill=AMBER, anchor="mm")
    # the line under them
    line = "22 genres · 220 pieces · plays itself while you steer it"
    d.text((W // 2, W - 30), line, font=small, fill=GREY, anchor="mm")
    out = SRC / "docs" / "card.png"
    card.save(out, optimize=True)
    print(f"{out} {card.size} {out.stat().st_size} B")


if __name__ == "__main__":
    main()
