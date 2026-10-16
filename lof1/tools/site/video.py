#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""LOF-1's feature video: the firmware itself, run in the host simulator (its real screens, lights and
sound), drawn into the FM-1 of the site's pictures, with captions. Everything happens on the beat: the
script is written in bars and beats at 70 BPM and turned into the simulator's milliseconds.

  host/build_host.sh && python3 tools/site/video.py        ->  build/video/lof1-features.mp4 (16:9)
  python3 tools/site/video.py --reuse                      ->  the simulator's frames as they are, drawn again

Preset 05 (Nightshift: A minor, 70 BPM, a warm pad under a tenor sax), the answer on the ocarina.
After HYP-1's tools/site/video.py. Needs cairosvg, Pillow, ffmpeg and the fonts in assets/fonts."""
import io
import math
import subprocess
import sys
from pathlib import Path

import cairosvg
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mockups as M  # noqa: E402

SRC = M.SRC
OUT = SRC / "build" / "video"
FPS = 30
W, H = 1920, 1080
SCALE = 1.1                                     # the device's units (1000 x 640) to pixels
DX, DY = 56, 190                                 # the device, left; the screen large on the right
BIG, BX, BY = 660, 1196, 150
CAP_X, CAP_Y, FOOT = DX, 52, (W - 56, H - 40)
BPM = 70.0
BEAT = 60000.0 / BPM
PHASE = 82.0                                     # ms after the first frame: a bar line (the pre-roll fixes it)
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

# the keys by note (key 0 = F3, MIDI 53)
N = {n: m - 53 for n, m in dict(F3=53, G3=55, A3=57, B3=59, C4=60, D4=62, E4=64, F4=65, Fs4=66, G4=67, A4=69,
                                 B4=71, C5=72, D5=74, E5=76, G5=79).items()}


# ------------------------------------------------------------------ the script, in beats --
class Show:
    """timed actions (in ms from the first frame), captions and knob rings, compiled into the simulator's
    script: frames up to each action, a wait to its exact millisecond, then the action"""

    def __init__(self):
        self.ev = []                             # (ms, order, line)
        self.caps = []                           # (ms0, ms1, title, sub)
        self.rings = []                          # (ms0, ms1, knob)
        self.end = 0.0

    @staticmethod
    def ms(beat):
        return PHASE + beat * BEAT

    def line(self, beat, s, order=1):
        self.ev.append((self.ms(beat), order, s))
        self.end = max(self.end, self.ms(beat))

    def cap(self, b0, b1, title, sub=""):
        self.caps.append((self.ms(b0), self.ms(b1), title, sub))
        self.end = max(self.end, self.ms(b1))

    def tap(self, beat, b, hold=0.15):           # a button, pressed for hold beats
        self.line(beat, f"press {b}")
        self.line(beat + hold, f"release {b}", 0)

    def go(self, beat, *buttons):               # page buttons, one after another, the last a little before beat
        n = len(buttons)
        for i, btn in enumerate(buttons):
            self.tap(beat - 0.4 * (n - i), btn)

    def shift(self, beat, btn):                 # SEL held, btn tapped, SEL let go (well apart)
        self.hold(beat, beat + 0.7, "SEL")
        self.tap(beat + 0.25, btn)

    def hold(self, b0, b1, b):
        self.line(b0, f"press {b}")
        self.line(b1, f"release {b}", 0)

    def spin(self, beat, enc, n, per=0.09):      # n detents, per beats apart (slow enough: no acceleration)
        s = 1 if n > 0 else -1
        for i in range(abs(n)):
            self.line(beat + i * per, f"spin {enc} {s}")
        self.rings.append((self.ms(beat), self.ms(beat + abs(n) * per + 0.6), enc))

    def note(self, beat, dur, k):
        self.line(beat, f"key {k} down")
        self.line(beat + max(dur - 0.07, 0.05), f"key {k} up", 0)

    def phrase(self, beat, notes):               # [(offset, beats, key)]
        for off, dur, k in notes:
            self.note(beat + off, dur, k)

    def chord(self, beat, dur, keys):
        for k in keys:
            self.note(beat, dur, k)

    def compile(self, pre):
        lines, frame, now = list(pre), 0, 0.0    # frames written; the simulator's time (ms from frame 0)
        step = 1000.0 / FPS
        lines.append("frames 1")
        frame = 1
        for ms, _, s in sorted(self.ev, key=lambda e: (e[0], e[1])):
            k = int(ms // step) + 1                # frames 0 .. k-1 are at or before ms
            if k > frame:
                lines.append(f"frames {k - frame}")
                frame = k
                now = (frame - 1) * step
            if ms - now >= 1.0:
                lines.append(f"wait {int(round(ms - now))}")
                now += round(ms - now)
            lines.append(s)
        k = int((self.end + 2000) // step) + 1
        if k > frame:
            lines.append(f"frames {k - frame}")
        lines.append("wavstop")
        return lines

    def at_frame(self, fn):
        ms = fn * 1000.0 / FPS
        cap = next(((a, b) for m0, m1, a, b in self.caps if m0 <= ms < m1), ("", ""))
        ring = next((k for m0, m1, k in self.rings if m0 <= ms < m1), None)
        return cap, ring


# the phrases (offset in beats, length in beats, key), A minor
P1 = [(0, 1.5, N["E4"]), (1.5, 0.5, N["G4"]), (2, 1, N["A4"]), (3, 1, N["C5"]), (4, 2.5, N["A4"]),
      (6.5, 0.5, N["G4"]), (7, 1, N["E4"])]
P2 = [(0, 1, N["D5"]), (1, 0.5, N["C5"]), (1.5, 0.5, N["A4"]), (2, 1.5, N["G4"]), (3.5, 0.5, N["E4"]),
      (4, 1, N["D4"]), (5, 3, N["E4"])]
CALL = [(0, 0.5, N["A4"]), (0.5, 0.5, N["C5"]), (1, 1, N["D5"]), (2, 1.5, N["E5"]), (3.5, 0.5, N["D5"]),
        (4, 2, N["C5"])]
CALL2 = [(0, 0.75, N["E4"]), (0.75, 0.25, N["G4"]), (1, 1, N["A4"]), (2, 0.5, N["G4"]), (2.5, 0.5, N["E4"]),
         (3, 3, N["D4"])]
P4 = [(0, 1, N["G4"]), (1, 0.5, N["A4"]), (1.5, 0.5, N["C5"]), (2, 1, N["A4"]), (3, 0.5, N["G4"]),
      (3.5, 0.5, N["E4"]), (4, 1, N["D4"]), (5, 1, N["C4"]), (6, 2, N["A3"])]
LICK = [(0, 0.5, N["A4"]), (0.5, 0.5, N["C5"]), (1, 0.5, N["D5"]), (1.5, 0.5, N["E5"]), (2, 2, N["D5"])]


VIEW_AT = [("THE LEAD", 2), ("THE COMP", 5), ("THE BASS", 7), ("THE ANSWER", 8), ("YOU PLAY", 0), ("CALL", 8),
           ("CHORDS", 0), ("GROOVE:", 0), ("COMP STYLE", 6), ("BASS:", 7), ("DRUMS", 9), ("THE KIT", 10),
           ("GROOVE 2", 1), ("DUST:", 11), ("DUST 2", 12), ("SPACE", 13), ("DAYDREAM", 3), ("A SCALE", 14),
           ("GLOBAL: SAVE", 14), ("GLOBAL 2", 15)]


def features():
    s = Show()
    b = 0.0                                      # the cursor, in beats from the first bar line

    def bars(n):
        nonlocal b
        b += 4 * n

    # 0 · the boot cards, then the music
    s.cap(-0.1, 6, "LOF-1", "a lofi groovebox for the M-VAVE FM-1  ·  you play the lead, a tired jazz band follows")
    bars(1.5)
    s.cap(b, b + 8, "PRESET 05 · NIGHTSHIFT", "A minor, 70 BPM. Nobody's playing yet: the band runs its own "
                                             "progression, a bar a chord")
    bars(2)

    # 1 · the band's instruments
    s.cap(b, b + 4, "THE LEAD: TENOR SAX", "ENV: the keys' page. ALGORITHM picks from twenty instruments")
    s.tap(b, "ENV")
    s.phrase(b + 1, [(0, 1, N["A4"]), (1, 1, N["C5"]), (2, 1, N["E5"])])
    bars(1)
    s.cap(b, b + 4, "THE COMP: WARM PAD", "LFO: the chord, re-voiced on every change")
    s.tap(b, "LFO")
    bars(1)
    s.cap(b, b + 4, "THE BASS: UPRIGHT", "EDIT, and ALGORITHM from the sub to the upright bass")
    s.tap(b, "EDIT")
    s.spin(b + 1, "ALGO", -1, 0.3)
    bars(1)
    s.cap(b, b + 8, "THE ANSWER: OCARINA", "ARP: call and response. ALGORITHM to the ocarina, Chatty up")
    s.tap(b, "ARP")
    s.spin(b + 0.5, "ALGO", 11, 0.18)
    s.spin(b + 3.5, "K3", 30, 0.09)
    bars(2)
    s.go(b - 0.1, "HOME")

    # 2 · playing the lead: the band follows
    s.cap(b, b + 32, "YOU PLAY THE LEAD", "the band hears which chord your notes imply, and moves with you on the beat")
    s.phrase(b, P1)
    s.phrase(b + 8, P2)
    s.phrase(b + 16, P4)
    s.phrase(b + 24, P1)
    bars(8)

    # 3 · call and response
    s.go(b - 0.1, "ARP")
    s.cap(b, b + 32, "CALL AND RESPONSE", "rest after a phrase and the ocarina answers: echoing yours, or one of its own")
    s.phrase(b, CALL)
    s.phrase(b + 8, CALL2)
    s.phrase(b + 16, CALL)
    s.phrase(b + 24, CALL2)
    bars(8)
    s.go(b - 0.2, "HOME")

    # 4 · chords on the keys
    s.cap(b, b + 20, "CHORDS ON THE KEYS", "SEL + HOME: each key plays the scale's chord on its note")
    s.shift(b, "HOME")
    for i, k in enumerate([N["A3"], N["F3"], N["G3"], N["E4"]]):
        s.note(b + 1 + 4 * i, 3.7, k)
    s.shift(b + 17.2, "HOME")
    bars(5)

    # 5 · chord hold
    s.cap(b, b + 16, "REC: CHORD HOLD", "the band stays on this chord while you solo over it")
    s.tap(b, "REC")
    s.phrase(b + 1, LICK)
    s.phrase(b + 5, P2)
    s.tap(b + 14, "REC")
    bars(4)

    # 6 · the groove
    s.cap(b, b + 8, "GROOVE: SWING", "how late the off-beat sixteenths fall")
    s.spin(b, "K2", 25, 0.09)
    s.spin(b + 5, "K2", -25, 0.06)
    bars(2)
    s.cap(b, b + 8, "GROOVE: JAZZ", "triads, 7ths, 9ths, 11ths: watch the chord's name")
    s.spin(b, "K3", 1, 0.3)
    s.spin(b + 4, "K3", 1, 0.3)
    bars(2)
    s.cap(b, b + 8, "GROOVE: BUSY", "how much the comp, bass and drums play around you")
    s.spin(b, "K4", 35, 0.09)
    s.spin(b + 5, "K4", -35, 0.06)
    s.spin(b + 7, "K3", -2, 0.2)
    bars(2)

    # 7 · comp style
    s.go(b - 0.1, "LFO", "LFO")
    s.cap(b, b + 16, "COMP STYLE", "the pad's pattern: held, stabs on two and four, off-beats, an arpeggio")
    for i in range(3):
        s.spin(b + 4 * i, "K1", 1, 0.3)
    s.spin(b + 13.5, "K1", -3, 0.15)
    bars(4)

    # 8 · bass
    s.go(b - 0.1, "EDIT")
    s.cap(b, b + 8, "BASS: WALKING", "roots, root and fifth, walking, or dub")
    s.spin(b, "K3", 2, 0.3)
    s.phrase(b + 2, P4)
    bars(2)
    s.spin(b - 0.6, "K3", -2, 0.2)

    # 9 · drums and the sequencer
    s.go(b - 0.1, "SEQ")
    s.cap(b, b + 8, "DRUMS: EIGHT PATTERNS", "SEL + the white keys picks one; it changes on the bar")
    s.hold(b, b + 0.6, "SEL")
    s.note(b + 0.2, 0.3, M.WHITE_K[5])
    s.hold(b + 4, b + 4.6, "SEL")
    s.note(b + 4.2, 0.3, M.WHITE_K[2])
    bars(2)
    s.go(b - 0.1, "SEQ")
    s.cap(b, b + 8, "THE KIT: DUSTY OR TAPE", "sampled drums, or synthesized ones")
    s.spin(b, "ALGO", 1, 0.3)
    s.spin(b + 4, "ALGO", -1, 0.3)
    bars(2)
    s.cap(b, b + 12, "HOLD SEQ: THE SEQUENCER", "the sixteen white keys are the steps, OCT− and OCT+ the lane")
    s.hold(b, b + 0.9, "SEQ")
    for i, w in enumerate([3, 11, 14]):
        s.note(b + 1.5 + i * 0.5, 0.2, M.WHITE_K[w])
    s.tap(b + 4, "OCT+")
    for i, w in enumerate([6, 15]):
        s.note(b + 5 + i * 0.5, 0.2, M.WHITE_K[w])
    s.tap(b + 10.5, "SEQ")
    bars(3)
    s.go(b - 0.1, "HOME", "HOME")

    # 10 · drift
    s.cap(b, b + 12, "GROOVE 2: DRIFT", "every few bars the music changes by itself: a new drum pattern, the "
                                       "other progression. SEL + SEQ: a change on the next bar")
    s.shift(b + 3, "SEQ")
    bars(3)

    # 11 · dust
    s.go(b - 0.1, "GLO")
    s.cap(b, b + 16, "DUST: THE TAPE UNDER IT ALL", "ALGORITHM: reel to reel, the radio, vinyl, and back to the cassette")
    for i in range(4):
        s.spin(b + 4 * i, "ALGO", 1, 0.3)
    bars(4)
    s.cap(b, b + 8, "DUST: AGE", "more wear: crackle, pops, and scratches on the record")
    s.spin(b, "K2", 45, 0.07)
    s.spin(b + 6, "K2", -45, 0.04)
    bars(2)
    s.go(b - 0.1, "GLO")
    s.cap(b, b + 8, "DUST 2: GRAIN", "a haze made of the music itself")
    s.spin(b, "K1", 45, 0.07)
    s.phrase(b + 1, LICK)
    s.spin(b + 6, "K1", -45, 0.04)
    bars(2)

    # 12 · space
    s.go(b - 0.1, "GLO")
    s.cap(b, b + 12, "SPACE", "tape wow and saturation, a low-pass, a plate reverb, an echo")
    s.spin(b, "K4", 30, 0.07)
    s.phrase(b + 2, CALL)
    s.spin(b + 9, "K4", -30, 0.05)
    bars(3)

    # 13 · FX
    s.go(b - 0.1, "HOME", "FX")
    s.cap(b, b + 4, "FX: EVERY KEY AN EFFECT", "a map on the screen; hold a key, let go and it's gone")
    bars(1)
    for name, w, dur in [("STUTTER 1/4", 0, 2), ("RECORD SKIP", 2, 2), ("TAPE STOP", 4, 2), ("BACKSPIN", 5, 2),
                         ("MUFFLE", 6, 3), ("RADIO", 7, 3)]:
        s.cap(b, b + 4, "FX: " + name, "")
        s.note(b + 0.5, dur, M.WHITE_K[w])
        bars(1)
    s.cap(b, b + 8, "FX LOCK", "hold SAVE and tap: muffle and wobble lock on")
    s.hold(b, b + 2.2, "SAVE")
    s.note(b + 0.6, 0.3, M.WHITE_K[6])
    s.note(b + 1.3, 0.3, M.BLACK_K[0])
    s.tap(b + 3, "FX")
    s.phrase(b + 4, LICK)
    bars(2)
    s.cap(b, b + 8, "FX LOCK", "the locks play on while the keys play notes. SEL + FX: every lock off")
    s.phrase(b, P2)
    s.shift(b + 6.2, "FX")
    bars(2)

    # 14 · side B
    s.cap(b, b + 8, "SAVE: SIDE B", "a breakdown on the next bar")
    s.tap(b + 0.5, "SAVE")
    s.phrase(b + 4, CALL2)
    bars(2)
    s.cap(b, b + 8, "SAVE AGAIN: BACK IN", "with a fill")
    s.tap(b + 1, "SAVE")
    bars(2)

    # 15 · daydream
    s.go(b - 0.1, "ENV", "ENV")
    s.cap(b, b + 24, "DAYDREAM", "leave the keys alone and a ghost takes the lead (Wait: Always, for now). "
                                 "Fill: how much of the time it plays")
    s.spin(b, "K1", 2, 0.3)
    s.spin(b + 12, "K2", 30, 0.09)
    bars(6)
    s.cap(b, b + 8, "ANY KEY", "takes the lead back")
    s.spin(b, "K1", -2, 0.2)
    s.spin(b + 0.6, "K2", -30, 0.04)
    s.phrase(b + 2, P1)
    bars(2)

    # 16 · a scale of your own, and storing
    s.go(b - 0.1, "GLO", "GLO", "GLO", "GLO")
    s.cap(b, b + 16, "A SCALE OF YOUR OWN", "on the Global page the keys set the scale (AMB-1's way): F out, "
                                            "F♯ in, and A minor becomes A Dorian")
    s.note(b + 1, 0.4, N["F4"])
    s.note(b + 3, 1.0, N["Fs4"])
    bars(2)
    s.go(b - 0.1, "HOME")
    s.phrase(b, [(0, 1, N["A4"]), (1, 1, N["B4"]), (2, 1, N["C5"]), (3, 1, N["D5"]), (4, 2, N["E5"]),
                 (6, 1, N["D5"]), (7, 1, N["Fs4"])])
    bars(2)
    s.go(b - 0.1, "GLO", "GLO", "GLO", "GLO")
    s.cap(b, b + 8, "GLOBAL: SAVE", "stores the preset: the music fades for the flash write and picks up "
                                    "where it was")
    s.note(b, 0.4, N["Fs4"])
    s.note(b + 0.8, 0.4, N["F4"])
    s.tap(b + 3, "SAVE")
    bars(2)

    # 17 · the look
    s.go(b - 0.1, "GLO")
    s.cap(b, b + 20, "GLOBAL 2: FIVE THEMES", "Lamp, Matcha, Sakura, Midnight, Sepia. And the old-TV look: "
                                              "scanlines, the curve, a slow rolling band")
    for i in range(4):
        s.spin(b + 4 * i, "K1", 1, 0.3)
    s.spin(b + 16, "K1", -4, 0.15)
    bars(5)
    s.go(b - 0.1, "HOME")

    # 18 · the easter egg
    s.line(b, "saver 1500")
    s.cap(b, b + 16, "AND AN EASTER EGG", "leave it alone for half an hour (here, a moment) …")
    bars(4)
    s.phrase(b, P1)
    s.line(b + 0.5, "saver 0")
    s.cap(b, b + 8, "… ANY KEY BRINGS IT BACK", "")
    bars(2)

    # 19 · the end card
    s.cap(b, b + 12, "LOF-1  ·  RedtailWorks", "rustypawz.github.io/fm1-HYP1/lof  ·  try it in your browser  ·  "
                                               "youtube.com/@RedtailWorks")
    s.phrase(b, P2)
    for i in range(16):
        s.line(b + 4 + i * 0.5, f"master {max(0, 3000 - (i + 1) * 190)}")
    bars(3)
    s.end = s.ms(b)
    return s


# the pre-roll (not filmed): preset 05 stored as the one to start on, the pad held longer, then a reboot
PRE = ["master 3000", "spin PRESET 1", "wait 200", "spin PRESET 1", "wait 200", "spin PRESET 1", "wait 200",
       "spin PRESET 1", "wait 300", "tap GLO", "tap GLO", "tap GLO", "tap GLO", "tap SAVE", "wait 1000",
       "tap LFO", "tap LFO", "wait 200", "spin K2 30", "wait 200", "tap HOME", "wait 300",
       "splash 3200", "reboot", "wav tour.wav"]


# ------------------------------------------------------------------ drawing --
def device_png(**kw):
    dev = M.device(None, **kw)
    w, h = int(1000 * SCALE), int(640 * SCALE)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 1000 640">{dev}</svg>')
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(), output_width=w))).convert("RGBA")


def ground():
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><defs>'
           '<radialGradient id="g" cx="0.5" cy="0.42" r="0.62"><stop offset="0" stop-color="#e8873a" stop-opacity="0.26"/>'
           '<stop offset="0.6" stop-color="#e8873a" stop-opacity="0.05"/><stop offset="1" stop-color="#16131e" stop-opacity="0"/></radialGradient>'
           '<linearGradient id="b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1c1828"/><stop offset="1" stop-color="#0e0c14"/></linearGradient>'
           f'</defs><rect width="{W}" height="{H}" fill="url(#b)"/><rect width="{W}" height="{H}" fill="url(#g)"/></svg>')
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(), output_width=W))).convert("RGBA")


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


def main():
    reuse = "--reuse" in sys.argv
    OUT.mkdir(parents=True, exist_ok=True)
    show = features()
    for t0, _, title, _ in show.caps:            # the script checks itself: the page, A minor, the voices
        v = next((v for k, v in VIEW_AT if title.startswith(k)), None)
        if v is not None:
            show.ev.append((t0 + BEAT, 3, f"expect view {v}"))
        if "SCALE OF YOUR OWN" not in title and title != "LOF-1":
            show.ev.append((t0 + BEAT, 3, "expect par48 9"))
            show.ev.append((t0 + BEAT, 3, "expect par52 9"))
        if title.startswith(("YOU PLAY", "CALL", "GROOVE", "DAYDREAM", "THE KIT")):
            show.ev.append((t0 + BEAT, 3, "expect par55 11"))
    if not (reuse and (OUT / "frames.txt").exists() and (OUT / "tour.wav").exists()):
        for f in OUT.glob("frame_*.png"):
            f.unlink()
        (OUT / "tour.amb").write_text("\n".join(show.compile(PRE)) + "\n")
        r = subprocess.run([str(SRC / "build" / "host" / "amb_host"), str(OUT / "tour.amb"), str(OUT)],
                           capture_output=True, text=True)
        bad = [l for l in r.stdout.splitlines() if l.startswith("FAIL")]
        for l in bad:
            print(l)
        if bad or r.returncode:
            sys.exit(f"the script's checks failed ({len(bad)})")
    if "--check" in sys.argv:
        return
    allb = set(BTN) - {"PLAY"}
    allk = set(KEY_BOX)
    base, lit_b, lit_p = device_png(), device_png(btns=allb), device_png(btns={"PLAY"})
    lit_k, held_k = device_png(keys=allk), device_png(held=allk)
    bg = ground()
    font_a = ImageFont.truetype(str(SRC / "assets" / "fonts" / "BarlowSemiCondensed-Bold.ttf"), 50)
    font_b = ImageFont.truetype(str(SRC / "assets" / "fonts" / "BarlowSemiCondensed-Medium.ttf"), 34)
    font_c = ImageFont.truetype(str(SRC / "assets" / "fonts" / "BarlowSemiCondensed-SemiBold.ttf"), 26)
    sx, sy = px(318, 62)
    big_frame = Image.new("RGBA", (BIG + 44, BIG + 44), (0, 0, 0, 0))
    ImageDraw.Draw(big_frame).rounded_rectangle((0, 0, BIG + 43, BIG + 43), radius=30, fill=(11, 10, 14, 255),
                                                outline=(90, 84, 104, 255), width=2)
    ss = int(200 * SCALE)
    log = [list(map(int, ln.split())) for ln in (OUT / "frames.txt").read_text().splitlines()]
    dur = len(log) / FPS
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-", "-i", str(OUT / "tour.wav"),
                           "-af", f"loudnorm=I=-14:TP=-1.5:LRA=11,afade=t=in:d=0.5,afade=t=out:st={dur - 2:.2f}:d=2",
                           "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-preset", "medium",
                           "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart",
                           str(OUT / "lof1-features.mp4")], stdin=subprocess.PIPE)
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
        shot = Image.open(OUT / f"frame_{fn:05d}.png").convert("RGB")
        frame.paste(shot.resize((ss, ss), Image.NEAREST), (sx, sy))
        frame.alpha_composite(big_frame, (BX - 22, BY - 22))
        frame.paste(shot.resize((BIG, BIG), Image.NEAREST), (BX, BY))
        d = ImageDraw.Draw(frame)
        for i, name in enumerate(BTN):                      # pressed: an orange ring
            if hb >> i & 1:
                x, y, w, h = BTN_BOX[name]
                a, b = px(x - 4, y - 4)
                c, e = px(x + w + 4, y + h + 4)
                d.rounded_rectangle((a, b, c, e), radius=int(13 * SCALE), outline=(255, 160, 100, 255), width=5)
        (ca, cb), tk = show.at_frame(fn)
        if tk:                                              # turned: a ring round the knob
            x, y, r = KNOB[tk]
            a, b = px(x - r - 9, y - r - 9)
            c, e = px(x + r + 9, y + r + 9)
            d.ellipse((a, b, c, e), outline=(255, 160, 100, 255), width=5)
        if ca:
            x0, y0, tw = CAP_X, CAP_Y, BX - 50 - CAP_X - 128
            d.rectangle((x0, y0 + 22, x0 + 70, y0 + 29), fill=(255, 176, 124, 255))
            d.rectangle((x0 + 76, y0 + 22, x0 + 110, y0 + 29), fill=(126, 204, 192, 255))
            d.text((x0 + 128, y0), ca, font=font_a, fill=(242, 234, 222, 255))
            for i, ln in enumerate(wrap(d, cb, font_b, tw)[:2] if cb else []):
                d.text((x0 + 128, y0 + 58 + i * 38), ln, font=font_b, fill=(190, 178, 206, 255))
        d.text(FOOT, "LOF-1  ·  RedtailWorks", font=font_c, fill=(130, 120, 150, 255), anchor="rs")
        ff.stdin.write(frame.convert("RGB").tobytes())
        if fn % 900 == 0:
            print(f"  {fn / FPS:6.1f} s", flush=True)
    ff.stdin.close()
    ff.wait()
    print("wrote", (OUT / "lof1-features.mp4").relative_to(SRC), f"({len(log)} frames, {dur:.0f} s)")


if __name__ == "__main__":
    main()
