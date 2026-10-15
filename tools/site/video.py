#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A video of HYP-1 going through its menus: the firmware itself, run in the host simulator (its real
screens, lights and sound), drawn into the FM-1 of the site's pictures, with captions.

  host/build_host.sh && python3 tools/site/video.py      ->  build/video/hyp1-tour.mp4
  python3 tools/site/video.py --vertical --seq        ->  build/video/seq/hyp1-sequencer-vertical.mp4 (a minute
                                                           in the drum sequencer; --seq works landscape too)
  python3 tools/site/video.py --vertical --voices     ->  build/video/voices/hyp1-voices-vertical.mp4 (the voices
                                                           shaped: sound, envelope, LFO, pump)
  python3 tools/site/video.py --vertical --links      ->  build/video/links/hyp1-links-vertical.mp4 (the drums
                                                           shaping the voices)
  python3 tools/site/video.py --update                ->  build/video/update/hyp1-update.mp4 (what's new in 1.5.0:
                                                           sounds, kits, by hand, Chords, FX locks, storing; 16:9)
  python3 tools/site/video.py --song                  ->  build/video/song/hyp1-song.mp4 (five minutes played
                                                           as a live set; 16:9)
  python3 tools/site/video.py --vertical [--reuse]      ->  build/video/hyp1-tour-vertical.mp4 (1080 x 1920,
                                                           for Shorts; --reuse: the simulator's frames as they are)

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
CAP_X, CAP_Y, FOOT = DX, 60, (W - 56, H - 40)
VERTICAL = dict(W=1080, H=1920, SCALE=1.0, DX=40, DY=1080, BIG=720, BX=180, BY=330, CAP_X=60, CAP_Y=120,
                FOOT=(540, 1790))                # Shorts: the screen large on top, the FM-1 under it
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
    t.lines += ["master 3000", "splash 3200", "reboot", "wav tour.wav"]
    t.cap("HYP-1", "hypnotic techno + acid for the M-VAVE FM-1")
    t.f(100)                                     # the boot cards: RedtailWorks, then HYP-1
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
    t.cap("SOUNDS", "on the drone's page, hold SEL and turn ALGORITHM: Reese")
    t.tap("LFO")
    t.lines.append("press SEL")
    t.f(8)
    t.spin("ALGO", 4, 4)
    t.lines.append("release SEL")
    t.f(80)
    t.cap("BY HAND", "on the drone's page, SEL + ENV: off the algorithm; the keys play it")
    t.lines.append("press SEL")
    t.f(6)
    t.tap("ENV", 5, 6)
    t.lines.append("release SEL")
    t.f(20)
    for k in (0, 4, 7, 4):
        t.key(M.WHITE_K[k], 22, 8)
    t.lines.append("press SEL")
    t.f(6)
    t.tap("ENV", 5, 6)
    t.lines.append("release SEL")
    t.f(30)
    t.cap("THE KIT", "SEQ three times: a sound for each drum")
    t.tap("SEQ")
    t.tap("SEQ")
    t.tap("SEQ")
    t.lines.append("press SEL")
    t.f(8)
    t.spin("ALGO", 2, 4)
    t.lines.append("release SEL")
    t.f(90)
    t.lines.append("press SEL")
    t.spin("ALGO", -2, 1)
    t.lines.append("release SEL")
    t.tap("LFO")
    t.lines.append("press SEL")
    t.spin("ALGO", -4, 1)
    t.lines.append("release SEL")
    t.f(10)
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


def update_tour():
    """what's new since the first tour (1.5.0): the sounds and kits, playing by hand, the Chords page, the
    locked effects that outlast the menu, storing a piece at once, the remote"""
    t = Tour()
    t.lines += ["master 3000", "splash 3200", "reboot", "wav tour.wav"]

    def shift(*acts):                            # SEL held for these
        t.lines.append("press SEL")
        t.f(8)
        for a in acts:
            a()
        t.lines.append("release SEL")
        t.f(6)

    t.cap("HYP-1 1.5.0", "what's new since the first tour")
    t.f(110)
    t.cap("SIXTEEN SOUNDS A VOICE", "the pad's Sound knob (KNOB 3): Trance, a supersaw")
    t.tap("ENV")
    t.spin("K3", 8, 2)
    t.f(80)
    t.cap("SIXTEEN SOUNDS A VOICE", "Rave: supersaw chords stabbed on the off-beats")
    t.spin("K3", 5, 2)
    t.f(100)
    t.spin("K3", -13, 1)
    t.cap("SEL + ALGORITHM", "the sound of the voice on screen: the drone's Rumble")
    t.tap("LFO")
    shift(lambda: t.spin("ALGO", 8, 2))
    t.f(80)
    t.cap("SEL + ALGORITHM", "Hoover, then Fifths: the root and its fifth")
    shift(lambda: t.spin("ALGO", 3, 3))
    t.f(70)
    shift(lambda: t.spin("ALGO", 1, 3))
    t.f(70)
    shift(lambda: t.spin("ALGO", -12, 1))
    t.cap("ARP LEADS", "the acid's last eight: Trance, Bleep, Laser")
    t.tap("ARP")
    t.tap("ARP")
    t.spin("K4", 8, 2)
    t.f(70)
    t.spin("K4", 1, 2)
    t.f(70)
    t.spin("K4", 4, 2)
    t.f(80)
    t.spin("K4", -13, 1)
    t.cap("EIGHT KITS", "SEQ three times; SEL + ALGORITHM swaps all four drums: Minimal")
    t.tap("SEQ")
    t.tap("SEQ")
    t.tap("SEQ")
    shift(lambda: t.spin("ALGO", 5, 2))
    t.f(90)
    t.cap("EIGHT KITS", "Hard: an industrial kick ... and 808")
    shift(lambda: t.spin("ALGO", 1, 2))
    t.f(90)
    shift(lambda: t.spin("ALGO", 1, 2))
    t.f(90)
    shift(lambda: t.spin("ALGO", -7, 1))
    t.cap("BY HAND", "on a voice's page, SEL + ENV takes it off the algorithm: the keys play it")
    t.tap("LFO")
    shift(lambda: t.tap("ENV", 5, 6))
    t.f(20)
    for k in (0, 4, 7, 4, 2, 0):
        t.key(M.WHITE_K[k], 20, 8)
    t.cap("BY HAND", "SEL + ENV again: back with the music. The keys set the scale everywhere else")
    shift(lambda: t.tap("ENV", 5, 6))
    t.f(50)
    t.cap("THE CHORDS PAGE", "ENV four times: Chord, Voicing, Drift, Root")
    for _ in range(4):
        t.tap("ENV", 5, 10)
    t.f(40)
    t.cap("CHORD: 9THS", "1, 3, 7 and 9: Am9, FM9 ... (EDIT: the next chord, on the bar)")
    t.spin("K1", 3, 3)
    t.f(80)
    t.tap("EDIT")
    t.f(110)
    t.cap("CHORD: DUB", "the same minor 9 shape on every chord: dub techno's stabs")
    t.spin("K1", 2, 3)
    t.f(70)
    t.tap("EDIT")
    t.f(110)
    t.cap("CHORD: FOURTHS", "stacked fourths, an m11; VOICING: open spreads them out")
    t.spin("K1", 1, 3)
    t.f(60)
    t.spin("K2", 1, 3)
    t.f(90)
    t.spin("K2", -1, 1)
    t.spin("K1", -6, 1)
    t.cap("PIECE 06", "dub techno: Dub chords on the Dub stabs, deep in the echo")
    t.spin("PRESET", 5, 3)
    t.f(260)
    t.cap("FX LOCK", "FX, hold SAVE, tap the gate 1/8 and the crush: locked on")
    t.tap("FX")
    t.f(10)
    t.lines.append("press SAVE")
    t.f(14)
    t.key(3, 6, 10)
    t.key(8, 6, 10)
    t.lines.append("release SAVE")
    t.f(70)
    t.cap("LOCKS OUTLAST THE MENU", "close FX: they play on (FX blinks); the keys are free again")
    t.tap("FX")
    t.f(140)
    t.cap("LOCKS OUTLAST THE MENU", "open FX and tap a locked key to stop it")
    t.tap("FX")
    t.f(20)
    t.key(3, 6, 30)
    t.key(8, 6, 30)
    t.tap("FX")
    t.f(30)
    t.cap("STORING A PIECE", "change it (here the Chords page: open voicing) ...")
    for _ in range(4):
        t.tap("ENV", 5, 8)
    t.spin("K2", 1, 3)
    t.f(50)
    t.cap("GLOBAL, THEN SAVE", "stored at once: the music pauses a moment while it's written, then carries on")
    t.tap("GLO")
    t.f(30)
    t.tap("SAVE", 6, 20)
    t.f(110)
    t.cap("STORED", "it comes back just so after power-off; hold SAVE on Global: factory")
    t.f(80)
    t.cap("SEL + GLO: REMOTE", "plugged in by USB, the FM-1's panel plays the emulator in your browser")
    shift(lambda: t.tap("GLO", 5, 6))
    t.f(110)
    shift(lambda: t.tap("GLO", 5, 6))
    t.tap("PLAY")
    t.f(40)
    t.cap("HYP-1 1.5.0", "update in your browser: rustypawz.github.io/fm1-HYP1")
    t.f(130)
    t.lines.append("wavstop")
    return t


def seq_tour():
    """about a minute in the drum sequencer: a beat built from nothing, shaped, copied and flipped"""
    W, BK = M.WHITE_K, M.BLACK_K
    t = Tour()
    t.lines += ["master 3000", "wav tour.wav"]

    def shift(*acts):                            # SEL held for these
        t.lines.append("press SEL")
        t.f(6)
        for a in acts:
            a()
        t.lines.append("release SEL")
        t.f(10)

    def steps(ss, gap=14):
        for s in ss:
            t.key(W[s], 5, gap)

    t.cap("THE DRUM SEQUENCER", "HYP-1 on the M-VAVE FM-1")
    t.f(60)
    t.cap("HOLD SEQ", "the 16 white keys become the kick's 16 steps")
    t.lines.append("press SEQ")
    t.f(16)
    t.lines.append("release SEQ")
    t.f(48)
    t.cap("SEL + OCT−", "clear the lane: start from nothing")
    shift(lambda: t.tap("OCT-", 5, 6))
    t.f(40)
    t.cap("FOUR ON THE FLOOR", "steps 1, 5, 9 and 13")
    steps((0, 4, 8, 12), 22)
    t.f(36)
    t.cap("OCT+ : THE SNARE", "cleared, then 5 and 13")
    t.tap("OCT+")
    shift(lambda: t.tap("OCT-", 5, 6))
    steps((4, 12), 24)
    t.f(40)
    t.cap("OCT+ : THE CLAP", "on 13, and a ghost on 16")
    t.tap("OCT+")
    shift(lambda: t.tap("OCT-", 5, 6))
    steps((12, 15), 24)
    t.f(36)
    t.cap("OCT+ : CLOSED HATS", "the off-beats, then a run into the bar")
    t.tap("OCT+")
    shift(lambda: t.tap("OCT-", 5, 6))
    steps((2, 6, 10, 14), 16)
    steps((13, 15), 10)
    t.f(50)
    t.cap("OCT+ : OPEN HAT", "left as the piece wrote it")
    t.tap("OCT+")
    t.f(36)
    t.cap("OCT+ : PERC", "syncopation on 4, 7, 11 and 16")
    t.tap("OCT+")
    shift(lambda: t.tap("OCT-", 5, 6))
    steps((3, 6, 10, 15), 18)
    t.f(36)
    t.cap("THE KNOBS: THE DRUM'S SOUND", "KNOB 2 tunes the perc up, KNOB 3 makes it short")
    t.spin("K2", 10, 3)
    t.f(20)
    t.spin("K3", -8, 3)
    t.f(36)
    t.cap("BACK ROUND TO THE KICK", "tuned down, with a longer boom")
    t.tap("OCT+")
    t.f(10)
    t.spin("K2", -6, 3)
    t.f(16)
    t.spin("K3", 8, 3)
    t.f(36)
    t.cap("SEL + BLACK KEY 2", "copies this pattern into pattern 2")
    shift(lambda: t.key(BK[1], 6, 30))
    t.f(20)
    t.cap("KNOB 4: PATTERN 2", "it comes in on the next bar, ready to change")
    t.spin("K4", 1, 3)
    t.f(40)
    t.cap("PATTERN 2", "kicks on 11 and 16 for a rolling variation")
    steps((10, 15), 24)
    t.f(30)
    t.cap("PATTERN 2: HATS", "a 16th-note run filling the last beat")
    for _ in range(3):
        t.tap("OCT+", 4, 6)
    steps((11, 12), 10)
    t.f(44)
    t.cap("SEL + WHITE KEYS 1 AND 2", "flip between the patterns, always on the bar")
    shift(lambda: t.key(W[0], 6, 75), lambda: t.key(W[1], 6, 70))
    t.cap("TAP SEQ", "back to the Drums page; it all keeps playing")
    t.tap("SEQ")
    t.f(50)
    t.cap("HYP-1", "rustypawz.github.io/fm1-HYP1  ·  youtube.com/@RedtailWorks")
    t.f(70)
    t.lines.append("wavstop")
    return t


def voices_tour():
    """about a minute shaping the voices: each one's sound, envelope and LFO, by hand"""
    t = Tour()
    t.lines += ["master 3000", "wav tour.wav"]

    def knob(k, n, per=1, rest=10):
        t.spin(k, n, per)
        t.f(rest)

    t.cap("SHAPING THE VOICES", "HYP-1 on the M-VAVE FM-1")
    t.f(50)
    t.cap("ENV: THE PAD", "KNOB 2 brightens it, KNOB 4 detunes it wide")
    t.tap("ENV")
    knob("K2", 40, 1, 6)
    knob("K4", 45, 1, 20)
    t.cap("ENV AGAIN: PAD ENVELOPE", "attack and release short: the breath becomes stabs")
    t.tap("ENV")
    knob("K1", -40)
    knob("K3", -45, 1, 36)
    knob("K1", 40, 1, 0)
    knob("K3", 45, 1, 6)
    t.cap("ENV AGAIN: PAD LFO", "faster and deeper, then onto the level: a tremolo")
    t.tap("ENV")
    knob("K1", -3, 3)
    knob("K2", 55)
    knob("K4", 1, 3, 50)
    knob("K2", -55, 1, 0)
    t.cap("NOISE", "eight colours of grain, and its own sidechain pump")
    t.spin("SELECT", 1, 1)
    knob("K1", 45)
    for _ in range(3):
        knob("K2", 1, 2, 14)
    knob("K4", 40, 1, 30)
    knob("K1", -45, 1, 4)
    t.cap("LFO: THE DRONE", "the 24 dB filter opened, resonance up, an octave down")
    t.tap("LFO")
    knob("K3", 35)
    knob("K4", 30)
    knob("K2", -1, 3, 40)
    t.cap("LFO AGAIN: DRONE ENVELOPE", "on every bar, sweeping the filter open")
    t.tap("LFO")
    knob("K1", 1, 3)
    knob("K4", 55, 1, 46)
    t.cap("LFO AGAIN: DRONE LFO", "the slow sweep made quick, a new shape")
    t.tap("LFO")
    knob("K1", -4, 3)
    knob("K3", 2, 4, 50)
    t.cap("ARP: THE ACID", "cutoff and resonance: the squelch")
    t.tap("ARP")
    knob("K3", 20)
    knob("K2", 45, 1, 6)
    knob("K2", -30, 1, 6)
    knob("K4", 35, 1, 24)
    t.cap("ARP AGAIN: ACID ENVELOPE", "longer decay, harder accents, more slide, square wave")
    t.tap("ARP")
    knob("K1", 30, 1, 4)
    knob("K2", 35, 1, 4)
    knob("K3", 40, 1, 4)
    knob("K4", 1, 3, 40)
    t.cap("ARP AGAIN: THE LINE", "Mutate rewrites it as it plays; 7 steps against the bar")
    t.tap("ARP")
    knob("K4", 50)
    knob("K2", -9, 2, 56)
    t.cap("SPACE", "tape echo and plate reverb, opened up")
    for _ in range(3):
        t.spin("SELECT", 1, 1)
        t.f(4)
    knob("K1", 35)
    knob("K4", 35, 1, 44)
    t.cap("HYP-1", "rustypawz.github.io/fm1-HYP1  ·  youtube.com/@RedtailWorks")
    t.f(70)
    t.lines.append("wavstop")
    return t


def links_tour():
    """about a minute of the links: stripped back to drums and drone, then each voice brought back in as
    its link is shown (each heard on and off), and the perc lane rewriting the acid line's rhythm"""
    W = M.WHITE_K
    t = Tour()
    t.lines += ["master 3000", "wav tour.wav"]

    def knob(k, n, per=1, rest=10):
        t.spin(k, n, per)
        t.f(rest)

    def shift(*acts):
        t.lines.append("press SEL")
        t.f(6)
        for a in acts:
            a()
        t.lines.append("release SEL")
        t.f(10)

    def links_page():
        t.tap("SEQ", 4, 6)
        t.tap("SEQ", 4, 10)

    t.cap("DRUMS THAT SHAPE THE VOICES", "HYP-1's links, on the M-VAVE FM-1")
    t.f(50)
    t.cap("STRIP IT BACK", "pad and acid out: just the drums and the drone")
    t.tap("ENV", 4, 6)
    knob("K1", -45, 1, 4)
    t.tap("ARP", 4, 6)
    knob("K1", -55, 1, 4)
    t.cap("THE DRONE, CLOSED DOWN", "cutoff low, resonance and level up")
    t.tap("LFO", 4, 6)
    knob("K3", -35, 1, 2)
    knob("K4", 35, 1, 2)
    knob("K1", 35, 1, 30)
    t.cap("SEQ, SEQ: THE LINKS PAGE", "SEL + SEQ turns them on")
    links_page()
    shift(lambda: t.tap("SEQ", 5, 6))
    t.cap("K>DRONE", "every kick throws the drone's filter open")
    knob("K1", 50, 1, 100)
    t.cap("LINKS OFF ...", "the drone sits still")
    shift(lambda: t.tap("SEQ", 5, 6))
    t.f(66)
    t.cap("... AND ON", "it pumps with the kick")
    shift(lambda: t.tap("SEQ", 5, 6))
    t.f(66)
    t.cap("S>PAD", "the pad back in: each snare and clap flashes it open")
    t.tap("ENV", 4, 6)
    knob("K1", 45, 1, 4)
    links_page()
    knob("K2", 60, 1, 100)
    t.cap("H>ACID", "the acid back in: the hats push accents into it")
    t.tap("ARP", 4, 6)
    knob("K1", 55, 1, 4)
    links_page()
    knob("K3", 50, 1, 90)
    t.cap("P>CLOCK", "the acid line now steps only on the perc lane's hits")
    knob("K4", 1, 3, 90)
    t.cap("HOLD SEQ, OCT−: THE PERC LANE", "change the perc, and the melody's rhythm follows")
    t.lines.append("press SEQ")
    t.f(16)
    t.lines.append("release SEQ")
    t.f(8)
    t.tap("OCT-")
    t.f(20)
    t.cap("SEL + OCT−: PERC CLEARED", "no perc hits: the acid holds still")
    shift(lambda: t.tap("OCT-", 5, 6))
    t.f(56)
    t.cap("PERC ON 1, 4, 7, 10, 13, 16", "the acid falls into a rolling 3-3-3")
    for st in (0, 3, 6, 9, 12, 15):
        t.key(W[st], 5, 10)
    t.f(84)
    t.cap("A BURST ON 2 AND 3", "the line skips ahead in a flurry")
    for st in (1, 2):
        t.key(W[st], 5, 12)
    t.f(84)
    t.cap("HYP-1", "rustypawz.github.io/fm1-HYP1  ·  youtube.com/@RedtailWorks")
    t.tap("SEQ")
    t.f(66)
    t.lines.append("wavstop")
    return t


def song_tour():
    """five minutes played, not demonstrated: piece 01 as a live set, from an ambient open to a fade.
    Captions only at the start and the end."""
    W = M.WHITE_K
    t = Tour()
    # set the stage before the recording starts: drums and acid silent, a little more pad, grain, space
    t.lines += ["master 3000", "tap SEQ", "spin K1 -75", "tap ARP", "spin K1 -55", "tap ENV", "spin K1 15",
                "tap LFO", "spin K3 -15", "tap HOME", "spin SELECT 4", "spin K1 15", "spin SELECT 9", "spin K3 20",
                "tap HOME", "wait 4000", "wav tour.wav"]

    def sec(x):
        t.f(int(round(x * 30)))

    def ride(k, n, per, rest=0):                 # a slow hand on a knob
        t.spin(k, n, per)
        t.f(rest)

    def shift(*acts):
        t.lines.append("press SEL")
        t.f(6)
        for a in acts:
            a()
        t.lines.append("release SEL")
        t.f(8)

    def hold(k, frames):                         # an effect key held (the FX menu open)
        t.lines.append(f"key {k} down")
        t.f(frames)
        t.lines.append(f"key {k} up")
        t.f(4)

    BAR = 56                                     # frames in a bar at 128

    # 0:00 the open: pad, drone, grain
    t.cap("HYP-1", "a five-minute live take  ·  piece 01, hypnotic, 128 BPM  ·  M-VAVE FM-1")
    sec(9)
    t.cap("", "")
    t.tap("LFO")
    ride("K3", 25, 16)                           # the drone opens, slowly
    sec(2)
    # 0:28 the drums fade in
    t.tap("SEQ")
    ride("K1", 75, 8)
    sec(2)
    # 0:50 the acid arrives
    t.tap("ARP")
    ride("K1", 55, 8)
    ride("K2", 12, 20)
    sec(12)
    # 1:25 pattern 2
    shift(lambda: t.key(W[1], 6, 10))
    sec(16)
    # riding the acid
    ride("K2", 28, 5)
    ride("K3", 15, 6)
    sec(6)
    ride("K2", -20, 7)
    t.tap("ARP")
    t.tap("ARP")                                 # the line: mutate it
    ride("K4", 35, 10)
    sec(6)
    # 2:15 repeats and an echo throw
    t.tap("FX")
    sec(1)
    hold(6, BAR)
    sec(3)
    hold(5, BAR // 2)
    t.tap("FX")
    # the breakdown
    t.tap("SAVE")
    sec(3)
    t.tap("ENV")
    ride("K2", 30, 14)                           # the pad brightens in the space
    t.tap("REC")                                 # and freezes
    sec(7)
    t.tap("REC")
    sec(3)
    # the build, under a high-pass, and the drop
    t.tap("SAVE")
    t.tap("FX")
    t.lines.append("key 26 down")
    t.f(4 * BAR - 30)
    t.lines.append("key 26 up")
    t.f(4)
    t.tap("FX")
    shift(lambda: t.key(W[2], 6, 10))            # pattern 3 at the drop
    sec(8)
    # 3:15 the peak: the links on, the acid opened up
    shift(lambda: t.tap("SEQ", 5, 6))
    t.tap("ARP")
    ride("K2", 18, 8)
    sec(22)
    # 3:45 a gate locked on, then a tape stop into the next phrase
    t.tap("FX")
    t.lines.append("press SAVE")
    t.f(10)
    t.key(3, 6, 8)
    t.lines.append("release SAVE")
    t.f(8 * BAR)
    t.key(3, 6, 2 * BAR - 60)
    hold(12, 46)
    t.tap("FX")
    # 4:15 winding down: pattern 1, the links off, the acid out
    shift(lambda: t.key(W[0], 6, 10))
    shift(lambda: t.tap("SEQ", 5, 6))
    sec(4)
    t.tap("HOME")
    t.tap("ARP")
    ride("K1", -60, 6)
    sec(9)
    # 4:40 the drums out; pad and drone to the end, fading
    t.tap("SAVE")
    sec(4)
    t.tap("LFO")
    t.spin("K3", -30, 10)
    for m in range(3000, 0, -150):
        t.lines.append(f"master {m}")
        t.f(12)
    t.lines.append("master 0")
    t.cap("HYP-1  ·  RedtailWorks", "rustypawz.github.io/fm1-HYP1  ·  youtube.com/@RedtailWorks")
    sec(6)
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


def wrap(d, s, font, width):                       # a caption's words, in lines that fit
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
    global W, H, SCALE, DX, DY, BIG, BX, BY, CAP_X, CAP_Y, FOOT, OUT
    vertical, reuse, seq = "--vertical" in sys.argv, "--reuse" in sys.argv, "--seq" in sys.argv
    voices, links, song = "--voices" in sys.argv, "--links" in sys.argv, "--song" in sys.argv
    update = "--update" in sys.argv
    if vertical:
        globals().update(VERTICAL)
    pre, outname = ("s_", "hyp1-tour-vertical.mp4") if vertical else ("v_", "hyp1-tour.mp4")
    one = "seq" if seq else "voices" if voices else "links" if links else "song" if song else "update" if update else None
    if one:                                      # a minute of its own, in its own folder
        OUT = OUT / one
        outname = outname.replace("tour", {"seq": "sequencer"}.get(one, one))
    OUT.mkdir(parents=True, exist_ok=True)
    t = {"seq": seq_tour, "voices": voices_tour, "links": links_tour, "song": song_tour, "update": update_tour}.get(one, tour)()
    if not (reuse and (OUT / "frames.txt").exists() and (OUT / "tour.wav").exists()):
        for f in OUT.glob("frame_*.png"):
            f.unlink()
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
    first = int(next((x.split("=")[1] for x in sys.argv if x.startswith("--from=")), 0))   # (re-draw from frame N)
    for fn, lb, lk, hb, hk in log:
        if fn < first and (OUT / f"{pre}{fn:05d}.png").exists():
            continue
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
        if ca:                                              # the caption: at the top
            x0, y0, tw = CAP_X, CAP_Y, (W - CAP_X - 168) if vertical else (BX - 50 - CAP_X - 128)   # (clear of the big screen)
            d.rectangle((x0, y0 + 20, x0 + 70, y0 + 27), fill=(255, 128, 32, 255))
            d.rectangle((x0 + 76, y0 + 20, x0 + 110, y0 + 27), fill=(164, 76, 255, 255))
            d.text((x0 + 128, y0), ca, font=font_a, fill=(239, 233, 255, 255))
            for i, ln in enumerate(wrap(d, cb, font_b, tw) if cb else []):
                d.text((x0 + 128, y0 + 56 + i * 40), ln, font=font_b, fill=(178, 170, 198, 255))
        d.text(FOOT, "HYP-1  ·  RedtailWorks", font=font_c, fill=(120, 112, 140, 255),
               anchor="ms" if vertical else "rs")
        frame.convert("RGB").save(OUT / f"{pre}{fn:05d}.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", str(OUT / f"{pre}%05d.png"),
                    "-i", str(OUT / "tour.wav"), *(["-af", "loudnorm=I=-14:TP=-1.5:LRA=11"] if one in ("song", "update") else []),
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20",
                    "-preset", "medium", "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart",
                    str(OUT / outname)], check=True)
    print("wrote", (OUT / outname).relative_to(SRC), f"({len(log)} frames)")


if __name__ == "__main__":
    main()
