#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""LOF-1's renders: the FM-1 running LOF-1, drawn from the emulator's geometry (web/emu/index.html) with the
simulator's real screens and lights (tools/site/shots.amb), and the screens themselves: every page, the FX
map, the four tapes, the screensaver, the boot cards. (After HYP-1's tools/site/mockups.py.)

  sh host/build_host.sh && mkdir -p build/site_shots && \
      build/host/amb_host tools/site/shots.amb build/site_shots > build/site_shots/log.txt
  python3 tools/site/mockups.py          ->  docs/img/lof-*.png

Needs cairosvg and Pillow, and Barlow Semi Condensed installed (assets/fonts)."""
import base64
import io
import re
import sys
from pathlib import Path

import cairosvg
from PIL import Image

SRC = Path(__file__).resolve().parents[2]
SHOTS = SRC / "build" / "site_shots"
OUT = SRC / "docs" / "img"
FONT = "'Barlow Semi Condensed', sans-serif"
HEAD = "'Barlow Semi Condensed', sans-serif"
WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
LABEL = "#e6e6ea"


def shot_uri(name, scale=4):
    im = Image.open(SHOTS / f"{name}.png").convert("RGB")
    im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    b = io.BytesIO()
    im.save(b, "PNG")
    return "data:image/png;base64," + base64.b64encode(b.getvalue()).decode()


def lights():
    """@name -> (buttons lit, keys lit), from the simulator's log"""
    log = (SHOTS / "log.txt").read_text()
    out, name = {}, None
    for ln in log.splitlines():
        if ln.startswith("@"):
            name = ln[1:].strip()
        elif "leds:" in ln and name:
            m = re.search(r"buttons(.*)\| white ([#.]+) black ([#.]+)", ln)
            btns = set(m.group(1).split())
            keys = {WHITE_K[i] for i, c in enumerate(m.group(2)) if c == "#"} | \
                   {BLACK_K[i] for i, c in enumerate(m.group(3)) if c == "#"}
            out[name] = (btns, keys)
            name = None
    return out


def device(screen, btns=(), keys=(), held=()):
    """the FM-1 (1000 x 640, the emulator's units), shaded; its screen showing `screen`"""
    o = ['''<defs>
  <linearGradient id="body" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4a4d54"/><stop offset="0.5" stop-color="#3b3e44"/><stop offset="1" stop-color="#2f3237"/></linearGradient>
  <linearGradient id="edge" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#6a6e76"/><stop offset="1" stop-color="#2a2c30"/></linearGradient>
  <linearGradient id="tray" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1d1f22"/><stop offset="1" stop-color="#292b2f"/></linearGradient>
  <radialGradient id="knob" cx="0.38" cy="0.32" r="0.75"><stop offset="0" stop-color="#4a4d53"/><stop offset="0.55" stop-color="#25272b"/><stop offset="1" stop-color="#16171a"/></radialGradient>
  <linearGradient id="btn" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3a3d42"/><stop offset="1" stop-color="#2a2c30"/></linearGradient>
  <linearGradient id="btnlit" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffa055"/><stop offset="1" stop-color="#ff7a14"/></linearGradient>
  <linearGradient id="btnplay" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#7dff9c"/><stop offset="1" stop-color="#2ed15a"/></linearGradient>
  <linearGradient id="keyw" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f0f0f2"/><stop offset="1" stop-color="#c4c5c9"/></linearGradient>
  <linearGradient id="keyb" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#dcdde0"/><stop offset="1" stop-color="#b3b4b8"/></linearGradient>
  <linearGradient id="keylit" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffd9bd"/><stop offset="1" stop-color="#f6a46e"/></linearGradient>
  <linearGradient id="keyheld" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffc08a"/><stop offset="1" stop-color="#ff8020"/></linearGradient>
  <linearGradient id="glass" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffffff" stop-opacity="0.07"/><stop offset="0.6" stop-color="#ffffff" stop-opacity="0"/></linearGradient>
</defs>''']

    def t(x, y, s, size, fill=LABEL, w=600, anchor="middle"):
        o.append(f'<text x="{x}" y="{y}" text-anchor="{anchor}" font-family="{FONT}" font-size="{size}" font-weight="{w}" '
                 f'fill="{fill}" letter-spacing="0.9">{s}</text>')

    for k in range(14, 0, -2):                      # the shadow under it
        o.append(f'<rect x="{4 - k * 0.5}" y="{4 + k * 1.6}" width="{992 + k}" height="{632 + k * 0.4}" rx="{44 + k}" '
                 f'fill="#000" opacity="0.07"/>')
    o.append('<rect x="4" y="4" width="992" height="632" rx="44" fill="url(#edge)"/>')
    o.append('<rect x="7" y="7" width="986" height="626" rx="41" fill="url(#body)"/>')

    def knob(x, y, r, label, ang=0):
        o.append(f'<circle cx="{x}" cy="{y + 2}" r="{r + 7}" fill="#000" opacity="0.25"/>')
        o.append(f'<circle cx="{x}" cy="{y}" r="{r + 5}" fill="url(#tray)"/>')
        o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="url(#knob)" stroke="#55585f" stroke-width="1.5"/>')
        o.append(f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 14}" stroke="#f2f2f4" stroke-width="3" stroke-linecap="round" '
                 f'transform="rotate({ang} {x} {y})"/>')
        t(x, y - r - 13, label, 16)
    knob(105, 100, 24, "MASTER", -40)
    knob(215, 100, 24, "SELECT", 30)
    knob(105, 205, 24, "PRESETS", 110)
    knob(215, 205, 24, "ALGORITHM", -100)
    for i, a in enumerate((-60, 20, 75, -20)):
        knob(610 + i * 104, 100, 22, f"KNOB{i + 1}", a)

    def button(x, y, w, h, label, two=None):
        lit = label in btns
        fill = "url(#btnplay)" if lit and label == "PLAY" else "url(#btnlit)" if lit else "url(#btn)"
        if lit:
            col = "#40ff80" if label == "PLAY" else "#ff8020"
            for k in (10, 6, 3):
                o.append(f'<rect x="{x - k}" y="{y - k}" width="{w + 2 * k}" height="{h + 2 * k}" rx="{9 + k}" fill="{col}" opacity="0.10"/>')
        o.append(f'<rect x="{x}" y="{y + 2}" width="{w}" height="{h}" rx="9" fill="#000" opacity="0.35"/>')
        o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="9" fill="{fill}" stroke="{"#ffd2a8" if lit else "#4e5157"}" stroke-width="1.2"/>')
        ink = "#2a1404" if lit else LABEL
        if two:
            t(x + w / 2, y + h / 2 - 1, label, 12, ink)
            t(x + w / 2, y + h / 2 + 12, two, 12, ink)
        else:
            t(x + w / 2, y + h / 2 + 5, label, 15, ink)

    o.append('<rect x="58" y="258" width="210" height="58" rx="14" fill="url(#tray)"/>')
    button(72, 268, 82, 38, "OCT-")
    button(170, 268, 82, 38, "OCT+")
    # the screen: a bezel, the panel's own image, a little glass
    o.append('<rect x="296" y="40" width="244" height="244" rx="28" fill="#0b0b0d" stroke="#55585f" stroke-width="1.2"/>')
    o.append('<rect x="314" y="58" width="208" height="208" rx="6" fill="#000"/>')
    if screen:
        o.append(f'<image x="318" y="62" width="200" height="200" href="{shot_uri(screen)}" preserveAspectRatio="none"/>')
    o.append('<rect x="318" y="62" width="200" height="200" fill="url(#glass)"/>')
    o.append('<rect x="568" y="160" width="400" height="140" rx="18" fill="url(#tray)"/>')
    r1, r2 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
    for i in range(6):
        button(582 + i * 63, 174, 54, 48, r1[i])
        button(582 + i * 63, 236, 54, 48, r2[i], "STOP" if r2[i] == "PLAY" else None)
    x0, y0, w, h = 36, 340, 928, 270
    o.append(f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="18" fill="url(#tray)"/>')
    pitch = (w - 24) / 16
    kw = pitch * 0.78

    def key(x, y, kw_, kh, k, base):
        fill = "url(#keyheld)" if k in held else "url(#keylit)" if k in keys else base
        if k in keys or k in held:
            col = "#ff8020" if k in held else "#f6a46e"
            for g in (9, 5):
                o.append(f'<rect x="{x - g}" y="{y - g}" width="{kw_ + 2 * g}" height="{kh + 2 * g}" rx="{kw_ / 2 + g}" fill="{col}" opacity="0.14"/>')
        o.append(f'<rect x="{x}" y="{y + 3}" width="{kw_}" height="{kh}" rx="{kw_ / 2}" fill="#000" opacity="0.35"/>')
        o.append(f'<rect x="{x}" y="{y}" width="{kw_}" height="{kh}" rx="{kw_ / 2}" fill="{fill}"/>')
    for i in range(16):
        key(x0 + 12 + i * pitch + (pitch - kw) / 2, y0 + h * 0.43, kw, h * 0.52, WHITE_K[i], "url(#keyw)")
    for j, a in enumerate(BLACK_AFTER):
        key(x0 + 12 + (a + 1) * pitch - kw / 2, y0 + h * 0.06, kw, h * 0.33, BLACK_K[j], "url(#keyb)")
    return "\n".join(o)



def scene(dev, out, title=None, sub=None, w=1600, h=None, glow="#e8873a"):
    """the device on a late-night violet ground, a lamp's glow behind it (and a title over it, if any)"""
    top = 160 if title else 70
    s = (w - 160) / 1000
    h = h or int(top + 640 * s + 80)
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',
         '<defs><radialGradient id="glow" cx="0.5" cy="0.55" r="0.6"><stop offset="0" stop-color="' + glow +
         '" stop-opacity="0.34"/><stop offset="0.55" stop-color="' + glow + '" stop-opacity="0.07"/><stop offset="1" stop-color="#16131e" stop-opacity="0"/></radialGradient>'
         '<linearGradient id="ground" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1c1828"/><stop offset="1" stop-color="#0e0c14"/></linearGradient></defs>',
         f'<rect width="{w}" height="{h}" fill="url(#ground)"/>', f'<rect width="{w}" height="{h}" fill="url(#glow)"/>']
    if title:
        o.append(f'<text x="80" y="92" font-family="{HEAD}" font-size="64" font-weight="700" fill="#f2eade" letter-spacing="2">{title}</text>')
        o.append('<rect x="80" y="112" width="120" height="6" fill="#ffb07c"/><rect x="206" y="112" width="60" height="6" fill="#7eccc0"/>')
        if sub:
            o.append(f'<text x="290" y="120" font-family="{HEAD}" font-size="24" font-weight="600" fill="#b4a9c4" letter-spacing="3">{sub}</text>')
    o.append(f'<g transform="translate(80 {top}) scale({s:.4f})">{dev}</g>')
    o.append("</svg>")
    cairosvg.svg2png(bytestring="\n".join(o).encode(), write_to=str(out), output_width=w)
    print("wrote", out.relative_to(SRC))


def screens(names, out, cols=None, label=None, scale=2, gap=24):
    """screens side by side on the dark ground, captions under them"""
    ims = [Image.open(SHOTS / f"{n}.png").convert("RGB") for n in names]
    cols = cols or len(ims)
    rows = (len(ims) + cols - 1) // cols
    cw = 240 * scale
    capt = 40 if label else 0
    W, H = cols * cw + (cols + 1) * gap, rows * (cw + capt) + (rows + 1) * gap
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><rect width="{W}" height="{H}" fill="#16131e"/>']
    for i, im in enumerate(ims):
        x, y = gap + (i % cols) * (cw + gap), gap + (i // cols) * (cw + capt + gap)
        b = io.BytesIO()
        im.resize((cw, cw), Image.NEAREST).save(b, "PNG")
        o.append(f'<rect x="{x - 3}" y="{y - 3}" width="{cw + 6}" height="{cw + 6}" rx="8" fill="#000" stroke="#3a3348" stroke-width="2"/>')
        o.append(f'<image x="{x}" y="{y}" width="{cw}" height="{cw}" href="data:image/png;base64,{base64.b64encode(b.getvalue()).decode()}"/>')
        if label:
            o.append(f'<text x="{x + cw / 2}" y="{y + cw + 30}" text-anchor="middle" font-family="{HEAD}" font-size="22" '
                     f'font-weight="600" fill="#d8cfe4" letter-spacing="2">{label[i]}</text>')
    o.append("</svg>")
    cairosvg.svg2png(bytestring="\n".join(o).encode(), write_to=str(out), output_width=W)
    print("wrote", out.relative_to(SRC))


SCREENS = ["splash", "splash_lof", "groove", "playing", "groove2", "lead", "daydream", "daydream2", "comp", "compstyle",
           "bass", "answer", "drums", "kit", "seq", "dust", "dust2", "space", "global", "help", "chords", "fx",
           "fx_muffle", "fx_lock", "tape2", "tape4", "tape6", "dreaming", "saver", "global2",
           "theme0", "theme1", "theme2", "theme3", "theme4"]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    L = lights()
    b, k = L["playing"]
    scene(device("playing", b, k, {16}), OUT / "lof-hero.png", "LOF-1", "YOU PLAY, THE BAND FOLLOWS")
    scene(device("groove", *L["groove"]), OUT / "lof-device.png")
    scene(device("chords", *L["chords"]), OUT / "lof-device-chords.png", "CHORDS ON THE KEYS", "SEL + HOME")
    scene(device("fx", *L["fx"]), OUT / "lof-device-fx.png", "THE FX KEYS", "FX: EVERY KEY AN EFFECT", glow="#c48cff")
    scene(device("seq", *L["seq"]), OUT / "lof-device-seq.png", "THE DRUMS", "HOLD SEQ: THE SEQUENCER", glow="#7eccc0")
    scene(device("tape6", *L["tape6"]), OUT / "lof-device-radio.png", glow="#c87a48")
    scene(device("saver", *L["saver"]), OUT / "lof-device-saver.png", "LEAVE IT A WHILE", "SOMEONE DROPS BY", glow="#6fae6a")
    for n in SCREENS:
        im = Image.open(SHOTS / f"{n}.png").convert("RGB")
        im.resize((480, 480), Image.NEAREST).save(OUT / f"lof-screen-{n.replace('_', '-')}.png")
    print("wrote docs/img/lof-screen-*.png")
    screens(["groove", "lead", "comp", "bass", "answer", "drums", "dust", "global2"], OUT / "lof-pages.png", cols=4,
            label=["GROOVE · HOME", "LEAD · ENV", "COMP · LFO", "BASS · EDIT", "ANSWER · ARP", "DRUMS · SEQ",
                   "DUST · GLO", "GLOBAL 2 · GLO x5"])
    screens(["groove", "tape2", "tape4", "tape6"], OUT / "lof-tapes.png", cols=4,
            label=["VINYL", "CASSETTE", "REEL TO REEL", "RADIO"])
    screens(["playing", "chords", "dreaming", "fx_lock", "seq", "saver"], OUT / "lof-moods.png", cols=3,
            label=["FOLLOWING YOU", "CHORDS ON THE KEYS", "DAYDREAMING", "FX LOCKED ON", "THE SEQUENCER", "A VISITOR"])
    screens(["splash", "splash_lof"], OUT / "lof-boot.png", label=["REDTAILWORKS", "LOF-1"])
    screens(["theme0", "theme1", "theme2", "theme3", "theme4"], OUT / "lof-themes.png", cols=5,
            label=["LAMP", "MATCHA", "SAKURA", "MIDNIGHT", "SEPIA"])


if __name__ == "__main__":
    sys.exit(main())
