"""PERF-1's sampled DJ snips (0.17.44, the maker: "the horns are still trash, maybe a sampled sound would be easier and sound better"): rendered here,
offline, with heavier models than the FM-1 can run live, then baked to 4-bit IMA ADPCM into firmware/src/dsp/djsmp.h. Ours, so GPL-clean: no samples
from anywhere else. The kick and the snare snips use the sampled kits' (smpkits.h: MACHINE's kick, BOOMBAP's snare), so they cost nothing here.
  AIR HORN  three reeds (the root, its fifth, its octave: right over a major or a minor chord) a few cents apart, each a band-limited pulse scooping up
            into its note, through a reed's asymmetric clip and the bell's two resonances, a breath of air at the start; a held loop (crossfaded)
  RAVE HORN the hoover: five detuned saws, two width-modulated pulses and a square an octave down, the pitch swooping in from a fourth above, dipping
            under the note and settling, chorused, driven; a held loop
  CRASH     a cymbal by its modes: 180 inharmonic partials (log-spread 350 Hz .. 10 kHz, the high ones dying first and blooming in a little after the
            stick), the stick's click and a wash of noise
  python -P tools/djsamples.py [--wav DIR]   (--wav also writes each sound as a WAV, to listen)"""
import os
import sys
import wave

import numpy as np

SR = 44100
rng = np.random.default_rng(4410)


def t_of(sec):
    return np.arange(int(sec * SR)) / SR


def lfilt(b, a, x):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    a1, a2 = a[1], a[2]
    for i, xi in enumerate(x):
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, xi, y1, yi
        y[i] = yi
    return y


def biquad(kind, x, f, q=0.707):
    w = 2 * np.pi * f / SR
    al, c = np.sin(w) / (2 * q), np.cos(w)
    if kind == "bp":
        b = np.array([al, 0, -al])
    elif kind == "hp":
        b = np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2])
    else:
        b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2])
    a = np.array([1 + al, -2 * c, 1 - al])
    return lfilt(b / a[0], a / a[0], x)


def additive(phase, amps):
    """a band-limited wave from its harmonics' amplitudes (phase in cycles, per sample); each harmonic only while under 18 kHz"""
    x = np.zeros_like(phase)
    inst = np.gradient(phase) * SR
    for k, a in enumerate(amps, start=1):
        if a == 0.0:
            continue
        ok = (k * inst) < 18000.0
        x += a * np.sin(2 * np.pi * k * phase) * ok
    return x


def pulse_amps(duty, n):
    return [2.0 / (np.pi * k) * np.sin(np.pi * k * duty) for k in range(1, n + 1)]


def saw_amps(n):
    return [(-1) ** (k + 1) * 2.0 / (np.pi * k) for k in range(1, n + 1)]


def norm(x, peak=0.89):
    return x * (peak / (np.max(np.abs(x)) + 1e-9))


def loop_xfade(x, l0, l1, xf):
    """a seamless loop from l0 to l1 (samples): the stretch before l1 crossfaded into the stretch before l0, so that what follows l1 is what follows l0"""
    y = x[:l1].copy()
    k = np.arange(xf) / xf
    w_out, w_in = np.cos(0.5 * np.pi * k), np.sin(0.5 * np.pi * k)   # (equal power: the detuned reeds make the two stretches only partly alike)
    y[l1 - xf:l1] = x[l1 - xf:l1] * w_out + x[l0 - xf:l0] * w_in
    return y


def horn():
    f0 = 466.16                                   # B flat: the snip's own (the engine puts it on the chord's root)
    t = t_of(0.62)
    out = np.zeros_like(t)
    for r, amp, cents, ph0 in ((1.0, 1.0, -5.0, 0.0), (1.5, 0.75, 4.0, 0.3), (2.0, 0.42, 7.0, 0.6), (1.0, 0.55, 9.0, 0.1)):
        scoop = -75.0 * np.exp(-t / 0.022) + 9.0 * np.exp(-((t - 0.045) / 0.02) ** 2)   # cents: up into the note, a small overshoot
        wob = 4.0 * np.sin(2 * np.pi * 6.3 * t + ph0 * 6.0)                             # the reed's flutter
        f = f0 * r * 2.0 ** ((cents + scoop + wob) / 1200.0)
        ph = np.cumsum(f) / SR + ph0
        out += amp * additive(ph, pulse_amps(0.42, 38))
    out *= 0.45
    att = np.minimum(t / 0.012, 1.0)
    x = out * att
    x = np.tanh(2.6 * x + 0.22) - np.tanh(0.22)                                           # the reed: an asymmetric clip (its even harmonics)
    x = 0.45 * x + 0.9 * biquad("bp", x, 1150.0, 1.3) + 0.45 * biquad("bp", x, 2650.0, 2.2)   # the bell's resonances
    breath = biquad("bp", rng.standard_normal(len(t)), 1700.0, 0.8) * (0.35 * np.exp(-t / 0.035) + 0.025)
    x += breath * att
    x = biquad("hp", x, 140.0)
    x = np.tanh(1.4 * x / (np.max(np.abs(x)) + 1e-9)) / np.tanh(1.4)
    return norm(loop_xfade(x, int(0.34 * SR), int(0.58 * SR), int(0.06 * SR))), 0.34, 0.58, f0


def rave():
    f0 = 233.08                                   # B flat below
    t = t_of(0.66)
    q1 = np.clip(t / 0.12, 0, 1)
    q2 = np.clip((t - 0.12) / 0.20, 0, 1)
    sem = np.where(t < 0.12, 5.0 - 6.0 * q1 * (2.0 - q1), -1.0 + q2 * q2 * (3.0 - 2.0 * q2))
    base = f0 * 2.0 ** (sem / 12.0)
    x = np.zeros_like(t)
    for c in (-21.0, -9.0, 0.0, 10.0, 22.0):
        ph = np.cumsum(base * 2.0 ** (c / 1200.0)) / SR + rng.random()
        x += 0.45 * additive(ph, saw_amps(70))
    for c, lfo in ((-12.0, 0.0), (13.0, 0.25)):   # two pulses, their widths swept apart
        ph = np.cumsum(base * 2.0 ** (c / 1200.0)) / SR + rng.random()
        duty = 0.5 + 0.3 * np.sin(2 * np.pi * (4.6 * t + lfo))
        saw1 = additive(ph, saw_amps(70))
        saw2 = additive(ph + duty, saw_amps(70))
        x += 0.5 * (saw1 - saw2)
    ph = np.cumsum(base * 0.5) / SR
    x += 0.6 * additive(ph, pulse_amps(0.5, 70))
    d = (0.008 + 0.002 * np.sin(2 * np.pi * 0.8 * t)) * SR                                # a chorus: one swept copy
    idx = np.clip(np.arange(len(t)) - d, 0, len(t) - 1)
    x = 0.7 * x + 0.5 * np.interp(idx, np.arange(len(t)), x)
    x = biquad("lp", x, 3600.0, 0.9)
    x = np.tanh(1.8 * x / (np.max(np.abs(x)) + 1e-9))
    x = biquad("hp", x, 55.0)
    x *= np.minimum(t / 0.004, 1.0)
    return norm(loop_xfade(x, int(0.38 * SR), int(0.62 * SR), int(0.07 * SR))), 0.38, 0.62, f0


def crash():
    t = t_of(1.25)
    x = np.zeros_like(t)
    fr = np.exp(rng.uniform(np.log(350.0), np.log(10000.0), 180))
    for f in fr:
        tau = float(np.clip(1.5 * (600.0 / f) ** 0.5, 0.22, 1.6))
        bloom = 1.0 - np.exp(-t / (0.002 + 0.03 * (f / 10000.0)))
        amp = (f / 1000.0) ** -0.25 * rng.uniform(0.4, 1.0)
        x += amp * np.sin(2 * np.pi * f * t + rng.random() * 6.283) * np.exp(-t / tau) * bloom
    x /= np.max(np.abs(x)) + 1e-9
    stick = biquad("hp", rng.standard_normal(len(t)), 2500.0) * np.exp(-t / 0.004) * 0.9
    wash = biquad("hp", rng.standard_normal(len(t)), 4500.0) * np.exp(-t / 0.45) * 0.10
    x = x + stick + wash
    x = biquad("hp", x, 300.0)
    x = np.tanh(1.3 * x) / np.tanh(1.3)
    fade = np.ones_like(t)
    k = int(0.15 * SR)
    fade[-k:] = np.cos(0.5 * np.pi * np.arange(k) / k)
    return norm(x * fade), 0.0, 0.0, 0.0


STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
        209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
        2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
        20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def adpcm(x16):
    """the nibbles (low first) and the starting index; the predictor starts at 0 (as smpkits.py)"""
    i0 = 0
    while i0 < 88 and STEP[i0] * 4 < abs(int(x16[0])):
        i0 += 1
    pred, ix = 0, i0
    nib = []
    for s in x16:
        d = int(s) - pred
        code = 0
        if d < 0:
            code = 8
            d = -d
        st = STEP[ix]
        diff = st >> 3
        if d >= st:
            code |= 4
            d -= st
            diff += st
        st >>= 1
        if d >= st:
            code |= 2
            d -= st
            diff += st
        st >>= 1
        if d >= st:
            code |= 1
            diff += st
        pred = pred - diff if code & 8 else pred + diff
        pred = max(-32768, min(32767, pred))
        ix = max(0, min(88, ix + IDX[code]))
        nib.append(code)
    if len(nib) & 1:
        nib.append(0)
    return bytes(nib[k] | nib[k + 1] << 4 for k in range(0, len(nib), 2)), i0


def down(x, out_sr):
    y = biquad("lp", x, 0.43 * out_sr, 0.707)
    y = biquad("lp", y, 0.43 * out_sr, 0.707)
    return np.interp(np.arange(0, len(y), SR / out_sr), np.arange(len(y)), y)


SOUNDS = [("HORN", horn, 16000), ("RAVE", rave, 16000), ("CRASH", crash, 22050)]


def main():
    wav_dir = sys.argv[sys.argv.index("--wav") + 1] if "--wav" in sys.argv else None
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only */",
             "/* PERF-1's sampled DJ snips (0.17.44), written by tools/djsamples.py: rendered offline by our own models (no samples from elsewhere),",
             " * 4-bit IMA ADPCM, the low nibble first; each sound its samples, its loop (start, end: 0 none), its first step index (the predictor starts",
             " * at 0), its rate and its note (Hz: the engine plays it at the chord's root). */",
             "typedef struct { const uint8_t *d; uint32_t n, ls, le; uint8_t i0; float sr, hz; } djsmp_t;"]
    total = 0
    tab = []
    for name, fn, out_sr in SOUNDS:
        x, l0, l1, hz = fn()
        y = down(x, out_sr)
        x16 = np.clip(np.round(y * 32767), -32768, 32767).astype(np.int32)
        by, i0 = adpcm(x16)
        total += len(by)
        nm = f"DJS_{name}"
        lines.append(f"static const uint8_t {nm}[{len(by)}] = {{")
        for i in range(0, len(by), 24):
            lines.append("    " + ", ".join(str(b) for b in by[i:i + 24]) + ",")
        lines.append("};")
        ls, le = (int(round(l0 * out_sr)), len(x16)) if l1 else (0, 0)
        tab.append(f"    {{{nm}, {len(x16)}u, {ls}u, {le}u, {i0}, {out_sr}.0f, {hz:.2f}f}},   /* {name} */")
        if wav_dir:
            os.makedirs(wav_dir, exist_ok=True)
            with wave.open(os.path.join(wav_dir, f"dj_{name.lower()}.wav"), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(out_sr)
                rep = np.concatenate([x16] + ([x16[ls:]] * 3 if le else []))   # (a held one: its loop three times more, to hear the seam)
                w.writeframes(rep.astype("<i2").tobytes())
        print(f"{name}: {len(x16)} samples at {out_sr} Hz, {len(by)} bytes" + (f", loop {ls}..{le}" if le else ""))
    lines.append("enum { DJS_HORN_I, DJS_RAVE_I, DJS_CRASH_I };")
    lines.append("static const djsmp_t DJ_SMP[3] = {")
    lines += tab
    lines.append("};")
    lines.append(f"/* {total} bytes */")
    open("firmware/src/dsp/djsmp.h", "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    print(f"djsmp.h: {total} bytes")


main()
