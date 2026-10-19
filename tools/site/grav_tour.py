#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""GRAV-1's video tour: the firmware itself in the host simulator (its real screens, lights and sound), drawn
into the FM-1 with captions, as the pack's video (tools/site/pack_video.py, whose parts it uses).

  (cd grav1 && sh host/build_host.sh) && python3 tools/site/grav_tour.py   ->  build/video/grav-tour/grav1-tour.mp4
  python3 tools/site/grav_tour.py --reuse                                   ->  the simulator's frames as they are

One long run of Sol and the systems after it, then the orbit view (a minute untouched) and the easter egg,
each its own run, and an end card."""
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pack_video as P  # noqa: E402

FW = "GRAV-1"
B, W_ = P.M.BLACK_K, P.M.WHITE_K


def sec(t):
    return int(round(t * P.FPS))


def tour():
    s = P.Seg("tour", FW, ["splash 3200", "reboot"])
    s.f(sec(3.6))                                # the boot cards: RedtailWorks, then GRAV-1

    s.cap("GRAV-1  ·  A STAR AND ITS PLANETS", "generative ambient for the M-VAVE FM-1: a solar system that "
                                              "plays, slow, dark and spacey")
    s.f(sec(9))
    s.cap("THE SYSTEM", "Sol: six planets round a star. Each planet is a voice, playing its phrase as it goes "
                        "round; near the star warm and dry, far out cold and wet")
    s.f(sec(10))
    s.cap("THE SYSTEM PAGE (HOME)", "Planets, Tempo, Wander (how much each year changes a phrase), Phrase")
    s.spin("K2", 6, 4)
    s.f(sec(3))
    s.spin("K2", -6, 3)
    s.f(sec(3))

    # the star
    s.cap("THE STAR (ENV)", "a bed of coloured noise, drawn in its own colour. Level is its ceiling; it roars and "
                           "flares now and then")
    s.tap("ENV")
    s.f(sec(4))
    s.cap("EIGHT NOISE COLOURS", "white, pink, brown, blue, violet, grey, black crackle, a radio's yellow hiss")
    for _ in range(4):                           # Sol's is the last (yellow): down through grey, violet, blue
        s.spin("K2", -1, 2, 4)
        s.f(sec(3.2))
    s.spin("K2", 4, 2)
    s.f(sec(2))

    # a planet
    s.cap("A PLANET (ARP)", "its voice: Level (its size, so its gravity), Instrument, Ellipse, Octave. ALGORITHM "
                           "or SEL: the next planet")
    s.tap("ARP")
    s.f(sec(4))
    s.cap("SIXTEEN INSTRUMENTS", "eight bright, eight dark: here Abyss, Dark choir, Cello, Gong ...")
    s.spin("K2", 8, 3, 4)
    s.f(sec(4))
    for _ in range(3):
        s.spin("K2", 1, 2, 4)
        s.f(sec(3.5))
    s.spin("K2", -11, 2)
    s.cap("THE ELLIPSE", "a stretched orbit swings the planet near the star and away: warm, then cold")
    s.spin("K3", 20, 2)
    s.f(sec(6))
    s.spin("K3", -20, 1)
    s.f(sec(1))

    # gravity
    s.cap("GRAVITY (LFO)", "a bigger planet swells a smaller one as it passes, peaking at the closest approach: "
                          "the lines between them are the tides")
    s.tap("LFO")
    s.f(sec(5))
    s.cap("TIDE AND REACH", "how much they swell, and from how far")
    s.spin("K1", 20, 2)
    s.spin("K2", 15, 2)
    s.f(sec(8))
    s.spin("K1", -20, 1)
    s.spin("K2", -15, 1)

    # weather
    s.cap("WEATHER (LFO AGAIN)", "dust clouds, stray asteroids and comets drift through by themselves. The black "
                                "keys call them")
    s.tap("LFO")
    s.f(sec(3))
    s.cap("AN ASTEROID", "a heavy body crossing: it pulls tides from the planets it passes, with a rumble")
    s.key(B[1], 5, 6)
    s.f(sec(8))
    s.cap("A COMET", "falling into the star, ringing bells, burning up")
    s.key(B[2], 5, 6)
    s.f(sec(8))
    s.cap("A DUST CLOUD", "the whole system warmer, softer, grainy, the sound under a closing filter")
    s.key(B[0], 5, 6)
    s.f(sec(10))

    # the pulse and the sequencer
    s.tap("HOME")
    s.cap("THE PULSE SETS THE ORBITS", "a drum lane for each planet: its hits are the planet's year. More hits, a "
                                      "shorter year, a closer orbit (Kepler)")
    s.lines.append("press SEQ")
    s.f(24)
    s.lines.append("release SEQ")
    s.f(sec(3))
    s.cap("HOLD SEQ: THE SEQUENCER", "black keys pick the lane, white keys its steps. Four hits for planet 3: "
                                    "watch it move in")
    s.key(B[2], 4, 12)
    for w in (1, 5, 9, 13):
        s.key(W_[w], 4, 10)
    s.f(sec(5))
    s.tap("HOME")
    s.f(sec(5))
    s.lines.append("press SEQ")
    s.f(24)
    s.lines.append("release SEQ")
    s.f(10)
    s.cap("EIGHT BANKS", "SELECT in the sequencer: another bank of six lanes, from the next bar")
    s.spin("SELECT", 1, 2, 4)
    s.f(sec(5))
    s.spin("SELECT", -1, 2, 4)
    s.tap("SEQ")
    s.f(sec(1))

    # drift
    s.cap("DRIFT (SEQ AGAIN)", "the drums change bank by themselves, as on LOF-1: Next or Random, on the Great "
                              "Year or every 4-64 bars, a fill the bar before")
    s.tap("SEQ")
    s.f(sec(4))
    s.cap("EDIT: THE GREAT YEAR NOW", "the chord moves, and Drift's change follows: a fill, then the new bank, and "
                                     "every planet's year with it")
    s.tap("EDIT")
    s.f(sec(12))

    # the kits
    s.cap("THREE DRUM KITS (SEQ AGAIN)", "Probe: kick, pulsar, sonar, geiger, rim, shaker")
    s.tap("SEQ")
    s.f(sec(6))
    s.cap("DEEP FIELD", "heart, static, chime, relay, drip, breath: soft and dark, far away")
    s.spin("K1", 1, 2, 4)
    s.f(sec(10))
    s.cap("RITUAL", "frame drum, woodblock, log, shaker, clave, rain")
    s.spin("K1", 1, 2, 4)
    s.f(sec(8))
    s.cap("TUNE AND DECAY", "every drum down a few semitones, longer")
    s.spin("K2", -5, 3)
    s.spin("K3", 12, 2)
    s.f(sec(6))
    s.spin("K2", 5, 2)
    s.spin("K3", -12, 1)
    s.spin("K1", -2, 2, 4)

    # moons
    s.tap("HOME")
    s.cap("MOONS (FX HELD)", "effects that orbit a planet. Hold FX: black keys a planet, white keys an effect's "
                            "moon round it. Three at most")
    s.lines.append("press FX")
    s.f(20)
    s.key(B[2], 5, 14)
    s.key(W_[4], 5, 14)                          # Sway round planet 3
    s.key(B[1], 5, 14)
    s.key(W_[7], 5, 14)                          # Ring round planet 2
    s.f(sec(3))
    s.lines.append("release FX")
    s.f(sec(1))
    s.cap("EBB AND FLOW", "each effect is gone on the near side and deepest on the far: Sway carries planet 3 "
                         "across the stereo field, Ring shimmers planet 2")
    s.f(sec(16))
    s.cap("TIDE, VEIL, HALO, ECHO, SWAY, CHORUS, PHASER, RING", "a big planet's moons are slow, a small one's quick. "
                                                              "And a planet passing close may capture another's moon")
    s.f(sec(12))

    # space
    s.cap("SPACE (FX)", "a dotted-eighth echo, a plate reverb, a radio's hiss and whistlers, the stereo width")
    s.tap("FX")
    s.spin("K3", 30, 2)
    s.f(sec(8))
    s.spin("K3", -30, 1)
    s.f(sec(1))

    # eclipse
    s.tap("HOME")
    s.cap("REC: ECLIPSE", "the orbits and the weather hold still; the space rings on")
    s.tap("REC")
    s.f(sec(6))
    s.tap("REC")
    s.cap("REC AGAIN", "and round they go")
    s.f(sec(4))

    # the systems
    s.cap("TEN SYSTEMS (PRESETS)", "each its star, its planets and their voices, its drums, kit and moons")
    s.spin("PRESET", 7, 3, 4)
    s.cap("HORIZON: THE BLACK HOLE", "the darkest, slowest system. Planet 6 drifts far out, mostly quiet, "
                                    "sighing when a bigger world passes")
    s.f(sec(16))
    s.cap("KUIPER", "the cold edge: dormant planets on the outer orbit, woken by a neighbour's tide")
    s.spin("PRESET", 1, 2, 4)
    s.f(sec(12))
    s.cap("NOVA", "the quickest system: three moons, Sway, Chorus and Halo")
    s.spin("PRESET", 1, 2, 4)
    s.f(sec(14))

    # display
    s.cap("SEVEN THEMES (GLO TWICE)", "Deep space, Nebula, Aurora, Red planet, Phosphor, Redtail, Mono. The planets "
                                     "keep their warm-to-cold colours in every one")
    s.tap("GLO")
    s.tap("GLO")
    for _ in range(6):
        s.spin("K1", 1, 2, 4)
        s.f(sec(2.4))
    s.spin("K1", -6, 2)
    s.tap("HOME")
    s.f(sec(3))
    return s


def orbits():
    s = P.Seg("orbits", FW, ["spin PRESET 9", "wait 1000", "tap HOME", "wait 54000"])
    s.cap("A MINUTE UNTOUCHED: THE ORBIT VIEW", "the words slip away, the system fills the screen and the panel's "
                                               "lights play along with it")
    s.f(sec(16))
    return s


def egg():
    s = P.Seg("egg", FW, ["wait 6000"], egg=True)
    s.cap("AND A SECRET", "hold SEL, press EDIT: ASTEROIDS. SELECT turns the ship, any key fires; the music sends "
                         "the rocks, and a rock you hit rings its planet")
    s.f(15)
    s.combo("SEL", "EDIT")
    for i in range(30):
        s.spin("SELECT", 2 if i % 6 < 4 else -3, 1, 0)
        s.key(W_[i % 8], 3, 8)
    s.lines.append("tap HOME")
    s.f(sec(1))
    return s


def main():
    reuse = "--reuse" in sys.argv
    P.OUT = P.SRC / "build" / "video" / "grav-tour"
    P.OUT.mkdir(parents=True, exist_ok=True)
    segs = [tour(), orbits(), egg()]
    dirs = {s.name: P.run(s, reuse) for s in segs}
    pt = P.Painter()

    def shot(name, i):
        return Image.open(dirs[name] / f"frame_{i:05d}.png").convert("RGB")

    def log(name):
        return [list(map(int, ln.split())) for ln in (dirs[name] / "frames.txt").read_text().splitlines()]

    # the end card: the screen large, the links
    endn = sec(9)
    fa = ImageFont.truetype(str(P.FONTS / "ChakraPetch-Bold.ttf"), 92)
    fb = ImageFont.truetype(str(P.FONTS / "BarlowSemiCondensed-Medium.ttf"), 38)
    fc = ImageFont.truetype(str(P.FONTS / "BarlowSemiCondensed-SemiBold.ttf"), 34)
    end = pt.bg[FW].copy()
    d = ImageDraw.Draw(end)
    big = 620
    d.rounded_rectangle((120 - 22, 230 - 22, 120 + big + 21, 230 + big + 21), radius=30, fill=(11, 10, 14, 255),
                        outline=P.hexrgb(P.FW[FW][1]), width=3)
    end.paste(shot("orbits", segs[1].n - 1).resize((big, big), Image.NEAREST), (120, 230))
    x0 = 860
    d.text((x0, 250), "GRAV-1", font=fa, fill=P.hexrgb(P.FW[FW][1]))
    d.text((x0, 360), "a star and its planets play the M-VAVE FM-1", font=fb, fill=(220, 210, 230))
    lines = ["Try it in your browser, install it, read the manual:", "rustypawz.github.io/fm1-HYP1/grav",
             "", "youtube.com/@RedtailWorks", "ko-fi.com/redtailworks", "", "free · GPL-3.0 · experimental"]
    for i, ln in enumerate(lines):
        d.text((x0, 450 + i * 52), ln, font=fc, fill=(242, 234, 222) if "rustypawz" in ln else (176, 166, 196))
    d.rectangle((x0, 236, x0 + 70, 243), fill=P.hexrgb(P.FW[FW][1]) + (255,))
    d.rectangle((x0 + 76, 236, x0 + 110, 243), fill=(126, 204, 192, 255))

    audio = []
    for s in segs:
        a = P.read_wav(dirs[s.name] / "seg.wav", s.n)
        audio.append(P.fade(P.level(a, -20.0), 0.4 if s.name != "tour" else 1.2, 0.4))
    tail = P.read_wav(dirs["orbits"] / "seg.wav", segs[1].n)[-sec(9) * P.SR // P.FPS:]
    tail = np.vstack([tail, np.zeros((max(0, endn * P.SR // P.FPS - len(tail)), 2), np.float32)])[:endn * P.SR // P.FPS]
    audio.append(P.fade(P.level(tail, -22.0), 0.8, 4.0))
    wav_all = np.clip(np.vstack(audio), -1, 1)
    with wave.open(str(P.OUT / "tour.wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(P.SR)
        w.writeframes((wav_all * 32767).astype(np.int16).tobytes())
    total = sum(s.n for s in segs) + endn
    dur = total / P.FPS
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", f"{P.W}x{P.H}", "-r", str(P.FPS), "-i", "-", "-i", str(P.OUT / "tour.wav"),
                           "-af", f"loudnorm=I=-14:TP=-1.5:LRA=11,afade=t=out:st={dur - 3:.2f}:d=3",
                           "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-preset", "medium",
                           "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart",
                           str(P.OUT / "grav1-tour.mp4")], stdin=subprocess.PIPE)
    done = 0
    for s in segs:
        for k, (fn, lb, lk, hb, hk) in enumerate(log(s.name)[:s.n]):
            fr = pt.device_frame(s, fn, lb, lk, hb, hk, shot(s.name, fn))
            ff.stdin.write(P.faded(fr, k, s.n, 10).convert("RGB").tobytes())
            done += 1
            if done % 1800 == 0:
                print(f"  {done / P.FPS:6.0f} s", flush=True)
    for k in range(endn):
        ff.stdin.write(P.faded(end, k, endn, 12).convert("RGB").tobytes())
    ff.stdin.close()
    ff.wait()
    print("wrote", P.OUT / "grav1-tour.mp4", f"({total} frames, {dur:.0f} s)")


if __name__ == "__main__":
    main()
