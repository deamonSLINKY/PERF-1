#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The four firmwares in one square picture for the site's front page: the FM-1 drawn running HYP-1,
LOF-1, ECO-1 and GRAV-1 (each firmware's own device render), laid on one dark desk, each lit by the
colour of its keys, with its name under it.

  python3 tools/site/scene.py          ->  docs/img/fw-scene.png (2000 x 2000)

Needs Pillow and the fonts in assets/fonts."""
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

SRC = Path(__file__).resolve().parents[2]
OUT = SRC / "docs" / "img" / "fw-scene.png"
FONTS = SRC / "assets" / "fonts"
SIZE = 2000
BODY = (86, 76, 1514, 986)          # the device's body in each 1600 x 1071 render
RADIUS = 74
SCALE = 0.55

FIRMWARES = [  # render, name, line, glow (sampled from the keys), centre, tilt in degrees
    ("docs/img/hyp-device.png", "HYP-1", "hypnotic techno and acid", (180, 110, 255), (500, 610), -4.0),
    ("lof1/docs/img/lof-device.png", "LOF-1", "a lofi groovebox", (255, 150, 110), (1500, 590), 3.5),
    ("eco1/docs/img/eco-device.png", "ECO-1", "a plant plays it", (150, 230, 120), (500, 1290), 3.0),
    ("grav1/docs/img/grav-device.png", "GRAV-1", "a star and its planets", (250, 210, 120), (1500, 1270), -3.5),
]


def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def cutout(path):
    im = Image.open(SRC / path).convert("RGBA").crop(BODY)
    mask = Image.new("L", im.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, im.width - 1, im.height - 1), RADIUS, fill=255)
    im.putalpha(mask)
    return im.resize((round(im.width * SCALE), round(im.height * SCALE)), Image.LANCZOS)


def main():
    scene = Image.new("RGB", (SIZE, SIZE))
    top, bottom = (13, 12, 18), (24, 22, 30)                      # the desk: a dark charcoal fall
    d = ImageDraw.Draw(scene)
    for y in range(SIZE):
        t = y / (SIZE - 1)
        d.line([(0, y), (SIZE, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))

    glow = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))               # each firmware's light pooled under it
    g = ImageDraw.Draw(glow)
    for _, _, _, col, (cx, cy), _ in FIRMWARES:
        g.ellipse((cx - 470, cy - 300, cx + 470, cy + 340), fill=tuple(round(c * 0.42) for c in col))
    scene = ImageChops.screen(scene, glow.filter(ImageFilter.GaussianBlur(150)))

    for path, name, line, col, (cx, cy), tilt in FIRMWARES:
        dev = cutout(path).rotate(tilt, Image.BICUBIC, expand=True)
        shadow = Image.new("RGBA", scene.size, (0, 0, 0, 0))
        sh = Image.new("RGBA", dev.size, (0, 0, 0, 0))
        sh.putalpha(dev.getchannel("A").point(lambda a: int(a * 0.75)))
        shadow.paste(sh, (cx - dev.width // 2 + 10, cy - dev.height // 2 + 34), sh)
        scene = Image.alpha_composite(scene.convert("RGBA"), shadow.filter(ImageFilter.GaussianBlur(28)))
        scene.alpha_composite(dev, (cx - dev.width // 2, cy - dev.height // 2))

    d = ImageDraw.Draw(scene)
    head, sub, tag = font("ChakraPetch-Bold.ttf", 70), font("BarlowSemiCondensed-Medium.ttf", 40), font("ChakraPetch-SemiBold.ttf", 60)
    d.text((SIZE // 2, 104), "FOUR FIRMWARES FOR THE M-VAVE FM-1", font=head, fill=(246, 236, 222), anchor="mm")
    d.text((SIZE // 2, 176), "Generative, free and open source  ·  by RedtailWorks", font=sub, fill=(170, 161, 184), anchor="mm")
    for _, name, line, col, (cx, cy), _ in FIRMWARES:
        y = cy + 330
        d.text((cx, y), name, font=tag, fill=col, anchor="mm")
        d.text((cx, y + 58), line, font=sub, fill=(200, 194, 210), anchor="mm")

    panda = Image.open(SRC / "docs" / "img" / "panda.png").convert("RGBA")
    panda = panda.resize((panda.width * 3, panda.height * 3), Image.NEAREST)
    word = font("ChakraPetch-Bold.ttf", 44)
    w = d.textlength("REDTAILWORKS", font=word)
    x0 = round(SIZE / 2 - (panda.width + 18 + w) / 2)
    scene.alpha_composite(panda, (x0, 1890 - panda.height // 2))
    d.text((x0 + panda.width + 18, 1890), "REDTAIL", font=word, fill=(193, 140, 255), anchor="lm")
    d.text((x0 + panda.width + 18 + d.textlength("REDTAIL", font=word), 1890), "WORKS", font=word, fill=(255, 128, 32), anchor="lm")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    scene.convert("RGB").save(OUT, optimize=True)
    print(f"scene: {OUT.relative_to(SRC)}")


if __name__ == "__main__":
    main()
