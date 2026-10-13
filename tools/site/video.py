#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A video of HYP-1 going through its menus: the firmware itself, run in the host simulator (its real
screens, lights and sound), drawn into the FM-1 of the site's pictures, with captions.

  host/build_host.sh && python3 tools/site/video.py      ->  build/video/hyp1-tour.mp4

Needs cairosvg, Pillow, ffmpeg, and the fonts in assets/fonts installed."""
import io
import subprocess
import sys
from pathlib import Path

import cairosvg
from PIL import Image, ImageDraw, ImageFont, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mockups as M  # noqa: E402

SRC = M.SRC
OUT = SRC / "build" / "video"
FPS = 30
W, H = 1920, 1080
SCALE = 1.1                                     # the device's units (1000 x 640) to pixels
DX, DY = 56, 190                                 # the device, left; the screen large on the right
BIG, BX, BY = 660, 1196, 150
BTN = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"]
BTN_BOX = {}                                     # name -> (x, y, w, h) in the device's units
R1, R2 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
for i in range(6):
    BTN_BOX[R1[i]] = (582 + i * 63, 174, 54, 48)
    BTN_BOX[R2[i]] = (582 + i * 63, 236, 54, 48)
BTN_BOX["OCT-"] = (72, 268, 82, 38)
BTN_BOX["OCT+"] = (170, 268, 82, 38)
KNOB = {"MASTER": (105, 100, 24), "SELECT": (215, 100, 24), "PRESET": (105, 205, 24), "ALGO": (215, 205, 24),
        "K1": (610, 100, 22), "K2": (714, 100, 22), "K3": (818, 100, 22), "K4": (922, 100, 22)}
KEY_BOX = {}
_x0, _y0, _w, _h = 36, 340, 928, 270
_p = (_w - 24) / 16
_kw = _p * 0.78
for i in range(16):
    KEY_BOX[M.WHITE_K[i]] = (_x0 + 12 + i * _p + (_p - _kw) / 2, _y0 + _h * 0.43, _kw, _h * 0.52)
for j, a in enumerate(M.BLACK_AFTER):
    KEY_BOX[M.BLACK_K[j]] = (_x0 + 12 + (a + 1) * _p - _kw / 2, _y0 + _h * 0.06, _kw, _h * 0.33)


# ------------------------------------------------------------------ the tour --
class Tour:
    """the simulator's script, built step by step; frame by frame what's pressed, turned and said"""

    def __init__(self):
        self.lines, self.n, self.caption, self.turning = [], 0, ("", ""), None
        self.caps, self.turns = {}, {}

    def cap(self, a, b=""):
        self.caption = (a, b)

    def f(self, n):                              # n frames, as things stand
        for _ in range(n):
            self.caps[self.n] = self.caption
            self.turns[self.n] = self.turning
            self.n += 1
        self.lines.append(f"frames {n}")

    def tap(self, b, hold=5, after=12):
        self.lines.append(f"press {b}")
        self.f(hold)
        self.lines.append(f"release {b}")
        self.f(after)

    def key(self, k, hold=5, after=6):
        self.lines.append(f"key {k} down")
        self.f(hold)
        self.lines.append(f"key {k} up")
        self.f(after)

    def spin(self, enc, n, per=2):               # n detents, one every `per` frames, the knob ringed
        self.turning = enc
        s = 1 if n > 0 else -1
        for _ in range(abs(n)):
            self.lines.append(f"spin {enc} {s}")
            self.f(per)
        self.f(6)                                # the ring a moment longer
        self.turning = None


def tour():
    t = Tour()
    t.lines += ["master 3000", "splash 1800", "reboot", "wav tour.wav"]
    t.cap("HYP-1", "hypnotic techno + acid for the M-VAVE FM-1")
    t.f(70)
    t.cap("PIECE 01 · TEMPO PAGE", "it plays from the moment it's switched on")
    t.f(90)
    t.cap("SELECT", "steps through the pages: each voice, its envelope, its LFO")
    for name in ["Pad", "Pad env", "Pad LFO", "Noise", "Drone", "Drone env", "Drone LFO", "Acid"]:
        t.spin("SELECT", 1, 1)
        t.f(26)
    t.cap("ACID", "KNOB 2: the cutoff, KNOB 3: the resonance")
    t.spin("K2", 22, 2)
    t.f(20)
    t.spin("K3", 10, 2)
    t.f(30)
    t.spin("K2", -22, 2)
    t.spin("K3", -10, 1)
    t.cap("SELECT", "Acid env, Acid line, Drums, Links, Space, Global")
    for _ in range(6):
        t.spin("SELECT", 1, 1)
        t.f(24)
    t.cap("SEQ: DRUMS", "KNOB 4 flips between six patterns, each waits for the bar")
    t.tap("SEQ")
    t.spin("K4", 1, 1)
    t.f(80)
    t.cap("HOLD SEQ: THE SEQUENCER", "the 16 white keys are the steps of the drum you're on")
    t.lines.append("press SEQ")
    t.f(16)
    t.lines.append("release SEQ")
    t.f(30)
    for k in (4, 9, 14, 19):
        t.key(k, 5, 8)
    t.f(20)
    t.cap("OCT+ : THE NEXT DRUM", "a snare, tapped in")
    t.tap("OCT+")
    for k in (6, 21, 24):
        t.key(k, 5, 8)
    t.f(50)
    t.cap("LINKS", "SEL + SEQ: the drums patched into the voices")
    t.tap("SEQ")
    t.tap("SEQ")
    t.lines.append("press SEL")
    t.f(10)
    t.tap("SEQ", 6, 10)
    t.lines.append("release SEL")
    t.f(80)
    t.cap("STYLES", "hold SEL, turn ALGORITHM: the same piece as house")
    t.lines.append("press SEL")
    t.f(8)
    t.spin("ALGO", 1, 4)
    t.f(100)
    t.cap("STYLES", "... as techno")
    t.spin("ALGO", 1, 4)
    t.f(100)
    t.cap("STYLES", "... as breakcore")
    t.spin("ALGO", 2, 4)
    t.f(110)
    t.cap("STYLES", "... and back to hypnotic")
    t.spin("ALGO", 1, 4)
    t.lines.append("release SEL")
    t.f(50)
    t.cap("OCT+ ON A VOICE'S PAGE", "the pad an octave up (on Tempo: the whole song)")
    t.tap("ENV")
    t.tap("OCT+")
    t.f(70)
    t.tap("OCT-")
    t.cap("FX", "every key a momentary effect: a beat repeat, held")
    t.tap("FX")
    t.f(15)
    t.lines.append("key 2 down")
    t.f(55)
    t.lines.append("key 2 up")
    t.f(10)
    t.cap("FX", "a tape stop, held")
    t.lines.append("key 12 down")
    t.f(40)
    t.lines.append("key 12 up")
    t.f(20)
    t.cap("FX LOCK", "hold SAVE, tap gate 1/8 and crush: both lock on")
    t.lines.append("press SAVE")
    t.f(14)
    t.key(3, 6, 10)
    t.key(8, 6, 10)
    t.lines.append("release SAVE")
    t.f(80)
    t.cap("FX LOCK", "tap a locked effect to turn just that one off")
    t.key(8, 6, 60)
    t.tap("FX")
    t.f(10)
    t.cap("SAVE: BREAKDOWN", "the drums drop out on the next bar")
    t.tap("HOME")
    t.tap("SAVE")
    t.f(150)
    t.cap("SAVE AGAIN: THE BUILD", "a four-bar build ...")
    t.tap("SAVE")
    t.f(200)
    t.cap("... AND THE DROP", "")
    t.f(80)
    t.cap("GLOBAL", "five screen themes")
    t.tap("GLO")
    for _ in range(4):
        t.spin("K2", 1, 1)
        t.f(32)
    t.spin("K2", -4, 1)
    t.cap("GLOBAL: SAVE", "stores the piece: patterns, sounds, style")
    t.tap("SAVE")
    t.f(60)
    t.cap("HYP-1", "rustypawz.github.io/fm1-HYP1  ·  www.redtail.works")
    t.f(90)
    t.lines.append("wavstop")
    return t


# ------------------------------------------------------------------ drawing --
def device_png(**kw):
    dev = M.device(None, **kw)
    w, h = int(1000 * SCALE), int(640 * SCALE)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 1000 640">'
           f'{dev}</svg>')
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(), output_width=w))).convert("RGBA")


def ground():
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><defs>'
           '<radialGradient id="g" cx="0.5" cy="0.42" r="0.62"><stop offset="0" stop-color="#7b2cff" stop-opacity="0.34"/>'
           '<stop offset="0.6" stop-color="#7b2cff" stop-opacity="0.06"/><stop offset="1" stop-color="#141318" stop-opacity="0"/></radialGradient>'
           '<linearGradient id="b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#17151d"/><stop offset="1" stop-color="#0d0c11"/></linearGradient>'
           f'</defs><rect width="{W}" height="{H}" fill="url(#b)"/><rect width="{W}" height="{H}" fill="url(#g)"/></svg>')
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(), output_width=W))).convert("RGBA")


def px(x, y):
    return int(DX + x * SCALE), int(DY + y * SCALE)


def region(box, pad=10):
    x, y, w, h = box
    a, b = px(x - pad, y - pad)
    c, d = px(x + w + pad, y + h + pad)
    return (a - DX, b - DY, c - DX, d - DY)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for f in OUT.glob("frame_*.png"):
        f.unlink()
    t = tour()
    (OUT / "tour.amb").write_text("\n".join(t.lines) + "\n")
    subprocess.run([str(SRC / "build" / "host" / "amb_host"), str(OUT / "tour.amb"), str(OUT)], check=True,
                   stdout=subprocess.DEVNULL)
    allb = set(BTN) - {"PLAY"}
    allk = set(KEY_BOX)
    base = device_png()
    lit_b = device_png(btns=allb)
    lit_p = device_png(btns={"PLAY"})
    lit_k = device_png(keys=allk)
    held_k = device_png(held=allk)
    bg = ground()
    font_a = ImageFont.truetype(str(SRC / "assets" / "fonts" / "ChakraPetch-Bold.ttf"), 46)
    font_b = ImageFont.truetype(str(SRC / "assets" / "fonts" / "BarlowSemiCondensed-Medium.ttf"), 34)
    font_c = ImageFont.truetype(str(SRC / "assets" / "fonts" / "ChakraPetch-SemiBold.ttf"), 24)
    sx, sy = px(318, 62)
    big_frame = Image.new("RGBA", (BIG + 44, BIG + 44), (0, 0, 0, 0))   # the large screen's bezel
    ImageDraw.Draw(big_frame).rounded_rectangle((0, 0, BIG + 43, BIG + 43), radius=30, fill=(11, 11, 13, 255),
                                                outline=(85, 88, 95, 255), width=2)
    ss = int(200 * SCALE)
    log = [list(map(int, ln.split())) for ln in (OUT / "frames.txt").read_text().splitlines()]
    for fn, lb, lk, hb, hk in log:
        dev = base.copy()
        for i, name in enumerate(BTN):
            if lb >> i & 1:
                r = region(BTN_BOX[name])
                dev.paste((lit_p if name == "PLAY" else lit_b).crop(r), r[:2])
        for k, box in KEY_BOX.items():
            if hk >> k & 1:
                r = region(box, 8)
                dev.paste(held_k.crop(r), r[:2])
            elif lk >> k & 1:
                r = region(box, 8)
                dev.paste(lit_k.crop(r), r[:2])
        frame = bg.copy()
        frame.alpha_composite(dev, (DX, DY))
        scr = Image.open(OUT / f"frame_{fn:05d}.png").convert("RGB").resize((ss, ss), Image.NEAREST)
        frame.paste(scr, (sx, sy))
        frame.alpha_composite(big_frame, (BX - 22, BY - 22))
        frame.paste(Image.open(OUT / f"frame_{fn:05d}.png").convert("RGB").resize((BIG, BIG), Image.NEAREST), (BX, BY))
        d = ImageDraw.Draw(frame)
        for i, name in enumerate(BTN):                      # pressed: an orange ring
            if hb >> i & 1:
                x, y, w, h = BTN_BOX[name]
                a, b = px(x - 4, y - 4)
                c, e = px(x + w + 4, y + h + 4)
                d.rounded_rectangle((a, b, c, e), radius=int(13 * SCALE), outline=(255, 128, 32, 255), width=5)
        tk = t.turns.get(fn)
        if tk:                                              # turned: a ring round the knob
            x, y, r = KNOB[tk]
            a, b = px(x - r - 9, y - r - 9)
            c, e = px(x + r + 9, y + r + 9)
            d.ellipse((a, b, c, e), outline=(255, 128, 32, 255), width=5)
        ca, cb = t.caps.get(fn, ("", ""))
        if ca:                                              # the caption: above the device
            y0 = 60
            d.rectangle((DX, y0 + 20, DX + 70, y0 + 27), fill=(255, 128, 32, 255))
            d.rectangle((DX + 76, y0 + 20, DX + 110, y0 + 27), fill=(164, 76, 255, 255))
            d.text((DX + 128, y0), ca, font=font_a, fill=(239, 233, 255, 255))
            if cb:
                d.text((DX + 128, y0 + 56), cb, font=font_b, fill=(178, 170, 198, 255))
        d.text((W - 56, H - 40), "HYP-1  ·  RedtailWorks", font=font_c, fill=(120, 112, 140, 255), anchor="rs")
        frame.convert("RGB").save(OUT / f"v_{fn:05d}.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", str(OUT / "v_%05d.png"),
                    "-i", str(OUT / "tour.wav"), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20",
                    "-preset", "medium", "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart",
                    str(OUT / "hyp1-tour.mp4")], check=True)
    print("wrote", (OUT / "hyp1-tour.mp4").relative_to(SRC), f"({len(log)} frames)")


if __name__ == "__main__":
    main()
