#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The FM-1 pack's showcase video: HYP-1, LOF-1, ECO-1 and GRAV-1, each run in its own host simulator (its
real screens, lights and sound), drawn into the FM-1 of the site's pictures, with captions.

  RedtailWorks' card, the four boot cards in a montage, then each firmware's features, then each one's
  easter egg, then the end card.

  (build each firmware's host first: host/build_host.sh in ., lof1, eco1, grav1)
  python3 tools/site/pack_video.py           ->  build/video/pack/fm1-pack.mp4 (1920 x 1080, 30 fps)
  python3 tools/site/pack_video.py --reuse   ->  the simulators' frames as they are, drawn again

After tools/site/video.py (HYP-1's) and lof1/tools/site/video.py. Needs cairosvg, Pillow, numpy, ffmpeg."""
import importlib.util
import io
import subprocess
import sys
import wave
from pathlib import Path

import cairosvg
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mockups as M  # noqa: E402

SRC = M.SRC
OUT = SRC / "build" / "video" / "pack"
FPS, W, H, SR = 30, 1920, 1080, 44100
SCALE = 1.1                                     # the device's units (1000 x 640) to pixels
DX, DY = 56, 190                                 # the device, left; the screen large on the right
BIG, BX, BY = 660, 1196, 150
CAP_X, CAP_Y, FOOT = DX, 52, (W - 56, H - 40)
FONTS = SRC / "assets" / "fonts"

FW = {   # name: (folder, accent, the card's line)
    "HYP-1": (".", "#b06cff", "hypnotic techno + acid"),
    "LOF-1": ("lof1", "#e8873a", "a lofi groovebox: you play, the band follows"),
    "ECO-1": ("eco1", "#76c35e", "a plant plays the FM-1"),
    "GRAV-1": ("grav1", "#f2b84b", "a star and its planets play the FM-1"),
}
BTN = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"]
BTN_BOX = {}
R1, R2 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
for _i in range(6):
    BTN_BOX[R1[_i]] = (582 + _i * 63, 174, 54, 48)
    BTN_BOX[R2[_i]] = (582 + _i * 63, 236, 54, 48)
BTN_BOX["OCT-"] = (72, 268, 82, 38)
BTN_BOX["OCT+"] = (170, 268, 82, 38)
KNOB = {"MASTER": (105, 100, 24), "SELECT": (215, 100, 24), "PRESET": (105, 205, 24), "ALGO": (215, 205, 24),
        "K1": (610, 100, 22), "K2": (714, 100, 22), "K3": (818, 100, 22), "K4": (922, 100, 22)}
KEY_BOX = {}
_x0, _y0, _w, _h = 36, 340, 928, 270
_p = (_w - 24) / 16
_kw = _p * 0.78
for _i in range(16):
    KEY_BOX[M.WHITE_K[_i]] = (_x0 + 12 + _i * _p + (_p - _kw) / 2, _y0 + _h * 0.43, _kw, _h * 0.52)
for _j, _a in enumerate(M.BLACK_AFTER):
    KEY_BOX[M.BLACK_K[_j]] = (_x0 + 12 + (_a + 1) * _p - _kw / 2, _y0 + _h * 0.06, _kw, _h * 0.33)
N = {n: m - 53 for n, m in dict(F3=53, G3=55, A3=57, B3=59, C4=60, D4=62, E4=64, F4=65, G4=67, A4=69, B4=71,
                                 C5=72, D5=74, E5=76, G5=79).items()}   # key 0 is F3


# ------------------------------------------------------------------ a segment's script, frame by frame --
class Seg:
    """one simulator run: its pre-roll (not filmed), then frames, with what's pressed, turned and said"""

    def __init__(self, name, fw, pre=(), egg=False):
        self.name, self.fw, self.egg = name, fw, egg
        self.lines = ["master 3000", *pre, "wav seg.wav"]
        self.n, self.caption, self.turning = 0, ("", ""), None
        self.caps, self.turns = {}, {}

    def cap(self, a, b=""):
        self.caption = (a, b)

    def f(self, n):
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

    def combo(self, a, b, after=10):              # a held, b tapped, both let go
        self.lines.append(f"press {a}")
        self.f(6)
        self.lines.append(f"press {b}")
        self.f(6)
        self.lines.append(f"release {b}")
        self.f(3)
        self.lines.append(f"release {a}")
        self.f(after)

    def key(self, k, hold=5, after=6):
        self.lines.append(f"key {k} down")
        self.f(hold)
        self.lines.append(f"key {k} up")
        self.f(after)

    def spin(self, enc, n, per=2, ring=6):        # n detents, one every `per` frames, the knob ringed
        self.turning = enc
        s = 1 if n > 0 else -1
        for _ in range(abs(n)):
            self.lines.append(f"spin {enc} {s}")
            self.f(per)
        self.f(ring)
        self.turning = None

    def phrase(self, notes, beat):                # [(offset, beats, key)], beat in frames
        t = 0
        for off, dur, k in notes:
            start, ln = round(off * beat), max(round(dur * beat) - 2, 2)
            if start > t:
                self.f(start - t)
            self.key(k, ln, 0)
            t = start + ln
        return t

    def close(self):
        self.lines.append("wavstop")


# ------------------------------------------------------------------ the segments --
def boot(fw):
    s = Seg(f"boot-{fw}", fw, ["splash 3200", "reboot"])
    s.f(100)
    return s


def bed():                                       # the music under the cards: GRAV-1's Sol, a while in
    s = Seg("bed", "GRAV-1", ["wait 9000"])
    s.f(30 * 14)
    return s


def bed_end():                                   # and under the end card: GRAV-1's Horizon
    s = Seg("bed-end", "GRAV-1", ["wait 1000", "spin PRESET 7", "wait 12000"])
    s.f(30 * 8)
    return s


def hyp():
    s = Seg("hyp", "HYP-1", ["wait 3000"])
    s.cap("HYP-1  ·  HYPNOTIC TECHNO + ACID", "a pad, a drone, an acid line and the drums, all generated: it plays "
                                             "from the moment it's switched on")
    s.f(130)
    s.cap("THE ACID LINE", "ARP: its page. KNOB 2 the cutoff, KNOB 3 the resonance")
    s.tap("ARP")
    s.spin("K2", 22, 2)
    s.spin("K3", 10, 2)
    s.f(40)
    s.spin("K2", -22, 1)
    s.spin("K3", -10, 1)
    s.cap("FX: EVERY KEY AN EFFECT", "hold one: a beat repeat ... a tape stop")
    s.tap("FX")
    s.lines.append("key 2 down")
    s.f(55)
    s.lines.append("key 2 up")
    s.f(10)
    s.lines.append("key 12 down")
    s.f(42)
    s.lines.append("key 12 up")
    s.f(10)
    s.tap("FX")
    s.cap("SAVE: BREAKDOWN, BUILD, DROP", "the drums drop out on the bar; SAVE again, a four-bar build ...")
    s.tap("HOME")
    s.tap("SAVE")
    s.f(120)
    s.tap("SAVE")
    s.f(290)
    s.cap("... AND THE DROP", "")
    s.f(90)
    return s


LOF_PRE = ["spin PRESET 1", "wait 200"] * 4 + ["tap HOME", "wait 2500"]
LOF_BEAT = 30 * 60 / 70                          # Nightshift: 70 BPM
P1 = [(0, 1.5, N["E4"]), (1.5, 0.5, N["G4"]), (2, 1, N["A4"]), (3, 1, N["C5"]), (4, 2.5, N["A4"]),
      (6.5, 0.5, N["G4"]), (7, 1, N["E4"])]
CALL = [(0, 0.5, N["A4"]), (0.5, 0.5, N["C5"]), (1, 1, N["D5"]), (2, 1.5, N["E5"]), (3.5, 0.5, N["D5"]),
        (4, 2, N["C5"])]


def lof():
    s = Seg("lof", "LOF-1", LOF_PRE)
    s.cap("LOF-1  ·  A LOFI GROOVEBOX", "you play the lead on the keys; a tired jazz band follows the chords "
                                       "your notes imply")
    s.f(20)
    s.phrase(P1, LOF_BEAT)
    s.f(40)
    s.cap("CALL AND RESPONSE", "ARP: leave a gap after a phrase, and the band answers it")
    s.tap("ARP")
    s.phrase(CALL, LOF_BEAT)
    s.f(150)
    s.cap("DUST: THE TAPE UNDER IT ALL", "GLO: cassette, reel to reel, the radio, vinyl. KNOB 2: how worn it is")
    s.tap("GLO")
    for _ in range(3):
        s.spin("ALGO", 1, 2, 4)
        s.f(42)
    s.spin("K2", 40, 2)
    s.f(40)
    s.spin("K2", -40, 1)
    s.spin("ALGO", -3, 2)
    s.cap("SAVE: SIDE B", "a breakdown on the next bar; SAVE again and the band comes back in with a fill")
    s.tap("HOME")
    s.tap("SAVE")
    s.f(140)
    s.tap("SAVE")
    s.f(120)
    return s


def eco():
    s = Seg("eco", "ECO-1", ["wait 6000"])
    s.cap("ECO-1  ·  A PLANT PLAYS THE FM-1", "its leaves charge as they drink and fire as notes; the stem drones "
                                             "under them, through a day and a night")
    s.f(150)
    s.cap("EDIT: TOUCH THE PLANT", "a charge runs up the stem and every leaf answers")
    s.tap("EDIT")
    s.f(100)
    s.cap("THE WEATHER", "SEQ: rain, wind, clouds. A storm: the leaves drink, sway and fire")
    s.tap("SEQ")
    s.spin("K1", 40, 1, 2)
    s.spin("K2", 40, 1, 2)
    s.spin("K3", 40, 1, 2)
    s.f(150)
    s.spin("K1", -40, 1, 2)
    s.spin("K2", -40, 1, 2)
    s.spin("K3", -40, 1, 2)
    s.cap("EIGHT PLANTS", "PRESETS: a fern, an orchid, moss, a cactus, a willow, a lotus, ivy, bamboo: "
                          "each its own place and sound")
    s.tap("HOME")
    for _ in range(5):
        s.spin("PRESET", 1, 2, 4)
        s.f(62)
    return s


def grav():
    s = Seg("grav", "GRAV-1", ["wait 5000"])
    s.cap("GRAV-1  ·  A STAR AND ITS PLANETS", "the star hisses in coloured noise; each planet is a voice, playing "
                                              "its phrase as it goes round")
    s.f(150)
    s.cap("THE DRUMS SET THE ORBITS", "hold SEQ: a lane's hits are its planet's year. More hits, a faster "
                                      "orbit, closer in (Kepler)")
    s.lines.append("press SEQ")
    s.f(24)
    s.lines.append("release SEQ")
    s.f(10)
    s.key(M.BLACK_K[3], 4, 10)
    for w in (2, 6, 10, 14):
        s.key(M.WHITE_K[w], 4, 8)
    s.f(90)
    s.tap("HOME")
    s.f(40)
    s.cap("WEATHER", "LFO twice; black keys call an asteroid, a comet, a dust cloud")
    s.tap("LFO")
    s.tap("LFO")
    s.key(M.BLACK_K[1], 4, 30)
    s.key(M.BLACK_K[2], 4, 30)
    s.key(M.BLACK_K[0], 4, 6)
    s.f(160)
    s.cap("THE STAR: EIGHT NOISE COLOURS", "ENV: white, pink, brown, blue ... each drawn in its own colour")
    s.tap("ENV")
    for _ in range(3):
        s.spin("K2", 1, 2, 4)
        s.f(36)
    s.spin("K2", -3, 2)
    s.tap("HOME")
    return s


def grav_orbits():
    s = Seg("grav-orbits", "GRAV-1", ["wait 55000"])
    s.cap("A MINUTE UNTOUCHED: THE ORBIT VIEW", "the knobs and words slip away, the system fills the screen, "
                                               "and the lights play along")
    s.f(330)
    return s


def egg_hyp():
    s = Seg("egg-hyp", "HYP-1", ["wait 4000"], egg=True)
    s.cap("EASTER EGG  ·  KICK RUNNER", "HYP-1: hold SEL, press EDIT. SELECT steers; the kicks send walls and "
                                       "gates, and each gate brings a part of the track back")
    s.f(15)
    s.combo("SEL", "EDIT")
    s.lines.append("autopilot 1")                # SELECT turned for it, a notch at a time, toward the gates
    s.turning = "SELECT"
    s.f(560)
    s.turning = None
    return s


def egg_lof():
    s = Seg("egg-lof", "LOF-1", LOF_PRE + ["saver 1500"], egg=True)
    s.cap("EASTER EGG  ·  RED'S COFFEE BREAK", "LOF-1: leave it alone for half an hour, and Red leans on the "
                                              "tree with a coffee, nodding to the beat")
    s.f(300)
    s.cap("EASTER EGG  ·  RED'S COFFEE BREAK", "any key brings it back")
    s.lines.append("saver 0")
    s.key(N["A4"], 20, 40)
    return s


def egg_eco():
    s = Seg("egg-eco", "ECO-1", ["wait 2000", "spin K2 -24", "flies 1000", "wait 29000"], egg=True)
    s.cap("EASTER EGG  ·  FIREFLIES", "ECO-1: leave the garden be for half an hour, and look again when it's "
                                     "dark. Each firefly flashes with its leaf")
    s.f(330)
    return s


def egg_grav():
    s = Seg("egg-grav", "GRAV-1", ["wait 6000"], egg=True)
    s.cap("EASTER EGG  ·  ASTEROIDS", "GRAV-1: hold SEL, press EDIT. SELECT turns the ship, any key fires. "
                                     "The music sends the rocks; a hit one rings its planet")
    s.f(15)
    s.combo("SEL", "EDIT")
    for i in range(24):
        s.spin("SELECT", 2 if i % 6 < 4 else -3, 1, 0)
        s.key(M.WHITE_K[i % 8], 3, 8)
    return s


# ------------------------------------------------------------------ running them --
def run(seg, reuse):
    d = OUT / seg.name
    d.mkdir(parents=True, exist_ok=True)
    if reuse and (d / "frames.txt").exists() and (d / "seg.wav").exists():
        return d
    for f in d.glob("frame_*.png"):
        f.unlink()
    seg.close()
    (d / "seg.amb").write_text("\n".join(seg.lines) + "\n")
    folder = SRC / FW[seg.fw][0]
    r = subprocess.run([str(folder / "build" / "host" / "amb_host"), str(d / "seg.amb"), str(d)], capture_output=True,
                       text=True, cwd=str(folder))
    bad = [ln for ln in r.stdout.splitlines() if ln.startswith("FAIL")]
    if bad or r.returncode:
        sys.exit(f"{seg.name}: {bad or r.returncode}")
    print(f"  ran {seg.name}: {seg.n} frames", flush=True)
    return d


def read_wav(path, n_frames):
    with wave.open(str(path)) as w:
        ch, sw = w.getnchannels(), w.getsampwidth()
        raw = w.readframes(w.getnframes())
    a = np.frombuffer(raw, dtype=np.int16 if sw == 2 else np.int32).astype(np.float32)
    a = a.reshape(-1, ch) / (32768.0 if sw == 2 else 2147483648.0)
    if ch == 1:
        a = np.repeat(a, 2, axis=1)
    need = round(n_frames * SR / FPS)
    if len(a) < need:
        a = np.vstack([a, np.zeros((need - len(a), 2), np.float32)])
    return a[:need]


def level(a, target_db=-20.0):                   # to a common loudness (RMS), before the whole is normalized
    rms = float(np.sqrt(np.mean(a ** 2)) + 1e-9)
    return a * min(10 ** (target_db / 20) / rms, 8.0)


def fade(a, fin, fout):
    n = len(a)
    if fin:
        k = min(int(fin * SR), n)
        a[:k] *= np.linspace(0, 1, k)[:, None]
    if fout:
        k = min(int(fout * SR), n)
        a[n - k:] *= np.linspace(1, 0, k)[:, None]
    return a


# ------------------------------------------------------------------ drawing --
def svg_png(svg, w):
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(), output_width=w))).convert("RGBA")


_MOCK = {}


def mock(fw):                                    # each firmware's own drawing of the FM-1: its own key colours
    if fw not in _MOCK:
        path = SRC / FW[fw][0] / "tools" / "site" / "mockups.py"
        spec = importlib.util.spec_from_file_location("mockups_" + fw.replace("-", ""), path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        _MOCK[fw] = mod
    return _MOCK[fw]


def device_png(fw, **kw):
    w, h = int(1000 * SCALE), int(640 * SCALE)
    return svg_png(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 1000 640">'
                   f'{mock(fw).device(None, **kw)}</svg>', w)


def ground(accent):
    return svg_png(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><defs>'
                   f'<radialGradient id="g" cx="0.5" cy="0.42" r="0.62"><stop offset="0" stop-color="{accent}" stop-opacity="0.24"/>'
                   f'<stop offset="0.6" stop-color="{accent}" stop-opacity="0.05"/><stop offset="1" stop-color="#121018" stop-opacity="0"/></radialGradient>'
                   '<linearGradient id="b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1a1824"/><stop offset="1" stop-color="#0c0b11"/></linearGradient>'
                   f'</defs><rect width="{W}" height="{H}" fill="url(#b)"/><rect width="{W}" height="{H}" fill="url(#g)"/></svg>', W)


def px(x, y):
    return int(DX + x * SCALE), int(DY + y * SCALE)


def region(box, pad=10):
    x, y, w, h = box
    a, b = px(x - pad, y - pad)
    c, d = px(x + w + pad, y + h + pad)
    return (a - DX, b - DY, c - DX, d - DY)


def wrap(d, s, font, width):
    lines, cur = [], ""
    for w in s.split(" "):
        t = (cur + " " + w).strip()
        if cur and d.textlength(t, font=font) > width:
            lines.append(cur)
            cur = w
        else:
            cur = t
    return lines + [cur] if cur else lines


def hexrgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


class Painter:
    def __init__(self):
        allb, allk = set(BTN) - {"PLAY"}, set(KEY_BOX)
        self.dev = {fw: (device_png(fw), device_png(fw, btns=allb), device_png(fw, btns={"PLAY"}),
                         device_png(fw, keys=allk), device_png(fw, held=allk)) for fw in FW}
        self.bg = {fw: ground(FW[fw][1]) for fw in FW}
        self.bg_pack = ground("#e8873a")
        self.fa = ImageFont.truetype(str(FONTS / "BarlowSemiCondensed-Bold.ttf"), 50)
        self.fb = ImageFont.truetype(str(FONTS / "BarlowSemiCondensed-Medium.ttf"), 34)
        self.fc = ImageFont.truetype(str(FONTS / "BarlowSemiCondensed-SemiBold.ttf"), 26)
        self.fh = ImageFont.truetype(str(FONTS / "ChakraPetch-Bold.ttf"), 64)
        self.fs = ImageFont.truetype(str(FONTS / "ChakraPetch-SemiBold.ttf"), 30)
        self.big_frame = Image.new("RGBA", (BIG + 44, BIG + 44), (0, 0, 0, 0))
        ImageDraw.Draw(self.big_frame).rounded_rectangle((0, 0, BIG + 43, BIG + 43), radius=30, fill=(11, 10, 14, 255),
                                                         outline=(90, 84, 104, 255), width=2)

    def device_frame(self, seg, fn, lb, lk, hb, hk, shot):
        base, lit_b, lit_p, lit_k, held_k = self.dev[seg.fw]
        dev = base.copy()
        for i, bn in enumerate(BTN):
            if lb >> i & 1:
                r = region(BTN_BOX[bn])
                dev.paste((lit_p if bn == "PLAY" else lit_b).crop(r), r[:2])
        for k, box in KEY_BOX.items():
            if hk >> k & 1:
                r = region(box, 8)
                dev.paste(held_k.crop(r), r[:2])
            elif lk >> k & 1:
                r = region(box, 8)
                dev.paste(lit_k.crop(r), r[:2])
        frame = self.bg[seg.fw].copy()
        frame.alpha_composite(dev, (DX, DY))
        sx, sy = px(318, 62)
        ss = int(200 * SCALE)
        frame.paste(shot.resize((ss, ss), Image.NEAREST), (sx, sy))
        frame.alpha_composite(self.big_frame, (BX - 22, BY - 22))
        frame.paste(shot.resize((BIG, BIG), Image.NEAREST), (BX, BY))
        d = ImageDraw.Draw(frame)
        for i, bn in enumerate(BTN):                        # pressed: an orange ring
            if hb >> i & 1:
                x, y, w, h = BTN_BOX[bn]
                a, b = px(x - 4, y - 4)
                c, e = px(x + w + 4, y + h + 4)
                d.rounded_rectangle((a, b, c, e), radius=int(13 * SCALE), outline=(255, 160, 100, 255), width=5)
        tk = seg.turns.get(fn)
        if tk:                                              # turned: a ring round the knob
            x, y, r = KNOB[tk]
            a, b = px(x - r - 9, y - r - 9)
            c, e = px(x + r + 9, y + r + 9)
            d.ellipse((a, b, c, e), outline=(255, 160, 100, 255), width=5)
        ca, cb = seg.caps.get(fn, ("", ""))
        acc = hexrgb(FW[seg.fw][1])
        if ca:
            x0, y0, tw = CAP_X, CAP_Y, BX - 50 - CAP_X - 128
            d.rectangle((x0, y0 + 22, x0 + 70, y0 + 29), fill=acc + (255,))
            d.rectangle((x0 + 76, y0 + 22, x0 + 110, y0 + 29), fill=(126, 204, 192, 255) if not seg.egg else (255, 220, 120, 255))
            d.text((x0 + 128, y0), ca, font=self.fa, fill=(242, 234, 222, 255))
            for i, ln in enumerate(wrap(d, cb, self.fb, tw)[:2] if cb else []):
                d.text((x0 + 128, y0 + 58 + i * 38), ln, font=self.fb, fill=(190, 178, 206, 255))
        d.text(FOOT, f"{seg.fw}  ·  RedtailWorks", font=self.fc, fill=(130, 120, 150, 255), anchor="rs")
        return frame

    def centre(self, shot, fw=None, size=760, title=None, sub=None):
        frame = (self.bg[fw] if fw else self.bg_pack).copy()
        x, y = (W - size) // 2, (H - size) // 2 - (30 if title else 0)
        bf = Image.new("RGBA", (size + 44, size + 44), (0, 0, 0, 0))
        ImageDraw.Draw(bf).rounded_rectangle((0, 0, size + 43, size + 43), radius=34, fill=(11, 10, 14, 255),
                                             outline=(90, 84, 104, 255), width=2)
        frame.alpha_composite(bf, (x - 22, y - 22))
        frame.paste(shot.resize((size, size), Image.NEAREST), (x, y))
        if title:
            d = ImageDraw.Draw(frame)
            d.text((W // 2, y + size + 62), title, font=self.fs, fill=(214, 206, 226, 255), anchor="ms")
        return frame

    def grid(self, shots, title, sub, size=430, gap=40, top=150):
        frame = self.bg_pack.copy()
        d = ImageDraw.Draw(frame)
        d.text((W // 2, 96), title, font=self.fh, fill=(242, 234, 222, 255), anchor="ms")
        if sub:
            d.text((W // 2, H - 52), sub, font=self.fs, fill=(190, 178, 206, 255), anchor="ms")
        tot = 4 * size + 3 * gap
        x0 = (W - tot) // 2
        for i, (fw, sh) in enumerate(shots):
            x = x0 + i * (size + gap)
            ImageDraw.Draw(frame).rounded_rectangle((x - 16, top + 120 - 16, x + size + 15, top + 120 + size + 15), radius=26,
                                                    fill=(11, 10, 14, 255), outline=hexrgb(FW[fw][1]), width=3)
            frame.paste(sh.resize((size, size), Image.NEAREST), (x, top + 120))
            d.text((x + size // 2, top + 120 + size + 70), fw, font=self.fh, fill=hexrgb(FW[fw][1]), anchor="ms")
            d.text((x + size // 2, top + 120 + size + 112), FW[fw][2], font=self.fc, fill=(190, 178, 206, 255), anchor="ms")
        return frame


def faded(frame, k, n, edge=8):                  # in and out through black at a segment's ends
    a = min(1.0, (k + 1) / edge, (n - k) / edge)
    if a >= 1.0:
        return frame
    return Image.blend(Image.new("RGBA", frame.size, (0, 0, 0, 255)), frame, a)


def main():
    reuse = "--reuse" in sys.argv
    OUT.mkdir(parents=True, exist_ok=True)
    order = ["HYP-1", "LOF-1", "ECO-1", "GRAV-1"]
    boots = {fw: boot(fw) for fw in order}
    segs = [bed(), bed_end(), hyp(), lof(), eco(), grav(), grav_orbits(), egg_hyp(), egg_lof(), egg_eco(), egg_grav()]
    dirs = {}
    for s in list(boots.values()) + segs:
        dirs[s.name] = run(s, reuse)
    by = {s.name: s for s in segs}

    def shot(name, i):
        return Image.open(dirs[name] / f"frame_{i:05d}.png").convert("RGB")

    def log(name):
        return [list(map(int, ln.split())) for ln in (dirs[name] / "frames.txt").read_text().splitlines()]

    P = Painter()
    plan = []                                    # (frame source, audio (array) or None)
    # 1 · RedtailWorks' card (HYP-1's boot: its first card, held a little)
    plan.append(("gen", 70, lambda k: P.centre(shot("boot-HYP-1", min(k, 51)))))
    # 2 · the four boot cards, one after another, then together
    for fw in order:
        plan.append(("gen", 34, lambda k, fw=fw: P.centre(shot(f"boot-{fw}", 52 + k), fw, title=fw + "  ·  " + FW[fw][2])))
    g = P.grid([(fw, shot(f"boot-{fw}", 95)) for fw in order], "FOUR FIRMWARES  ·  ONE FM-1",
               "free, generative firmware for the M-VAVE FM-1  ·  by RedtailWorks")
    plan.append(("gen", 110, lambda k: g))
    intro_n = sum(x[1] for x in plan)
    bed_a = fade(level(read_wav(dirs["bed"] / "seg.wav", intro_n)), 3.0, 0.4)
    # 3 · the features, then 4 · the easter eggs
    body = ["hyp", "lof", "eco", "grav", "grav-orbits", "egg-hyp", "egg-lof", "egg-eco", "egg-grav"]
    audio = [bed_a]
    for name in body:
        s = by[name]
        plan.append(("seg", s))
        audio.append(fade(level(read_wav(dirs[name] / "seg.wav", s.n), -21.0 if name != "egg-lof" else -22.0), 0.25, 0.3))
    # 5 · the end card: the four eggs' last frames
    endn = 180
    pick = {"egg-lof": 220}                      # Red mid-break, not the screen he wakes to
    end = P.grid([(fw, shot(n, pick.get(n, by[n].n - 1))) for fw, n in zip(order, ["egg-hyp", "egg-lof", "egg-eco", "egg-grav"])],
                 "HYP-1  ·  LOF-1  ·  ECO-1  ·  GRAV-1",
                 "try them in your browser: rustypawz.github.io/fm1-HYP1   ·   youtube.com/@RedtailWorks")
    plan.append(("gen", endn, lambda k: end))
    audio.append(fade(level(read_wav(dirs["bed-end"] / "seg.wav", endn)), 0.6, 2.5))
    total = intro_n + sum(by[n].n for n in body) + endn
    wav_all = np.vstack(audio)
    wav_all = np.clip(wav_all, -1, 1)
    with wave.open(str(OUT / "pack.wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((wav_all * 32767).astype(np.int16).tobytes())
    dur = total / FPS
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-", "-i", str(OUT / "pack.wav"),
                           "-af", f"loudnorm=I=-14:TP=-1.5:LRA=11,afade=t=out:st={dur - 2:.2f}:d=2",
                           "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-preset", "medium",
                           "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart",
                           str(OUT / "fm1-pack.mp4")], stdin=subprocess.PIPE)
    done = 0
    for item in plan:
        if item[0] == "gen":
            _, n, gen = item
            for k in range(n):
                ff.stdin.write(faded(gen(k), k, n, 6).convert("RGB").tobytes())
            done += n
        else:
            s = item[1]
            for k, (fn, lb, lk, hb, hk) in enumerate(log(s.name)[:s.n]):
                fr = P.device_frame(s, fn, lb, lk, hb, hk, shot(s.name, fn))
                ff.stdin.write(faded(fr, k, s.n).convert("RGB").tobytes())
            done += s.n
        print(f"  {done / FPS:6.1f} s", flush=True)
    ff.stdin.close()
    ff.wait()
    print("wrote", OUT / "fm1-pack.mp4", f"({total} frames, {dur:.0f} s)")


if __name__ == "__main__":
    main()
