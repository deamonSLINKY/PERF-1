"""PERF-1's built-in sampled kits (0.17.36): rendered here, offline, with heavier models than the FM-1 can run live, then baked to
4-bit IMA ADPCM at 22.05 kHz into firmware/src/dsp/smpkits.h. Ours, so GPL-clean: no samples from anywhere else.
  MACHINE: an 808 / 909 style drum machine (the 808's six squares through its two band-passes for the hats, its cowbell, its clap's bursts)
  BOOMBAP: an acoustic kit by its modes (a membrane's, the snare wires, a cymbal's inharmonic partials), then through an SP-1200 (26.04 kHz,
           12 bits, no anti-alias filter) and a small room
  python -P tools/smpkits.py [--wav DIR]   (--wav also writes each sound as a WAV, to listen)"""
import os
import sys
import wave

import numpy as np

SR = 44100           # rendered at this rate
OUT_SR = 22050       # stored at this
rng = np.random.default_rng(1729)


def t_of(sec):
    return np.arange(int(sec * SR)) / SR


def env(t, dec, att=0.0005):
    return np.minimum(t / att, 1.0) * np.exp(-t / dec)


def bp(x, f, q):     # a biquad band-pass (RBJ, constant 0 dB peak)
    w = 2 * np.pi * f / SR
    al = np.sin(w) / (2 * q)
    b = np.array([al, 0, -al]) / (1 + al)
    a = np.array([1, -2 * np.cos(w) / (1 + al), (1 - al) / (1 + al)])
    return lfilt(b, a, x)


def hp(x, f, q=0.707):
    w = 2 * np.pi * f / SR
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    b = np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2]) / (1 + al)
    a = np.array([1, -2 * c / (1 + al), (1 - al) / (1 + al)])
    return lfilt(b, a, x)


def lp(x, f, q=0.707):
    w = 2 * np.pi * f / SR
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2]) / (1 + al)
    a = np.array([1, -2 * c / (1 + al), (1 - al) / (1 + al)])
    return lfilt(b, a, x)


def lfilt(b, a, x):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        v = b[0] * x[i] + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        x2, x1, y2, y1 = x1, x[i], y1, v
        y[i] = v
    return y


def sweep_sine(t, f0, f1, tau):     # a sine falling from f0 to f1 with time constant tau
    f = f1 + (f0 - f1) * np.exp(-t / tau)
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def noise(n):
    return rng.standard_normal(n)


def sq(t, f):
    return np.sign(np.sin(2 * np.pi * f * t + 0.3))


def fade(x, ms=6):
    k = int(ms * SR / 1000)
    x = x.copy()
    x[-k:] *= np.linspace(1, 0, k)
    return x


def norm(x, peak=0.89):
    return x * (peak / (np.max(np.abs(x)) + 1e-12))


# ---------------------------------------------------------------- MACHINE (808 / 909)
def m_kick():
    t = t_of(0.40)
    body = sweep_sine(t, 160.0, 49.0, 0.018) * env(t, 0.30)
    click = lp(noise(len(t)) * env(t, 0.0015), 3500)
    x = np.tanh(1.8 * (body + 0.25 * click)) / np.tanh(1.8)
    return fade(x)


def m_snare():
    t = t_of(0.28)
    tone = 0.6 * sweep_sine(t, 205, 185, 0.03) * env(t, 0.09) + 0.35 * sweep_sine(t, 350, 330, 0.02) * env(t, 0.06)
    nz = bp(noise(len(t)), 4200, 0.6) * env(t, 0.14)
    return fade(np.tanh(1.4 * (tone + 0.9 * nz)))


def m_clap():
    t = t_of(0.30)
    n = noise(len(t))
    e = np.zeros_like(t)
    for d in (0.0, 0.011, 0.022, 0.033):
        tt = np.clip(t - d, 0, None)
        e += (t >= d) * np.exp(-tt / 0.006) * (1.0 if d < 0.03 else 0.0)
    tail = (t >= 0.033) * np.exp(-np.clip(t - 0.033, 0, None) / 0.11)
    x = bp(n, 1100, 1.2) * (e + 0.85 * tail)
    return fade(x)


METAL = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)


def m_hat(dec, length):
    t = t_of(length)
    m = sum(sq(t, f * 1.0) for f in METAL) / 6.0
    x = bp(m, 7100, 1.4) + 0.6 * bp(m, 3400, 1.2) + 0.15 * hp(noise(len(t)), 7000)
    x = hp(x, 6000) * env(t, dec)
    return fade(x)


def m_perc():
    t = t_of(0.22)
    x = 0.5 * sq(t, 540) + 0.5 * sq(t, 800)
    x = bp(x, 850, 2.5) * (0.6 * env(t, 0.012) + 0.4 * env(t, 0.09))
    return fade(x)


# ---------------------------------------------------------------- BOOMBAP (modes, then an SP-1200 and a small room)
def modes(t, f0, rat, amp, dec, drop=0.0, tau=0.03):
    x = np.zeros_like(t)
    for r, a, d in zip(rat, amp, dec):
        f = f0 * r * (1 + drop * np.exp(-t / tau))
        x += a * np.sin(2 * np.pi * np.cumsum(f) / SR) * env(t, d)
    return x


def room(x):
    y = x.copy()
    for d, g in ((0.007, 0.22), (0.013, 0.16), (0.021, 0.12), (0.034, 0.08)):
        k = int(d * SR)
        y[k:] += g * lp(x[:-k], 4000)
    return y


def sp1200(x):
    """26.04 kHz, 12 bits, a zero-order hold and no anti-alias filter (its grit), back at 44.1 kHz"""
    sr2 = 26040.0
    idx = np.floor(np.arange(len(x)) * sr2 / SR) * SR / sr2
    held = np.interp(idx, np.arange(len(x)), x)
    q = np.round(held * 2047) / 2047
    return lp(q, 11000)


def b_kick():
    t = t_of(0.40)
    x = modes(t, 56.0, (1.0, 1.59, 2.14, 2.30), (1.0, 0.35, 0.2, 0.12), (0.28, 0.12, 0.08, 0.06), drop=0.22)
    beater = lp(noise(len(t)) * env(t, 0.003), 2800)
    x = np.tanh(2.2 * (x + 0.35 * beater)) / np.tanh(2.2)
    return fade(sp1200(room(x)))


def b_snare():
    t = t_of(0.30)
    head = modes(t, 192.0, (1.0, 1.59, 2.14, 2.65), (1.0, 0.5, 0.35, 0.2), (0.11, 0.07, 0.05, 0.04), drop=0.05)
    wires = bp(noise(len(t)), 3600, 0.5) * env(t, 0.17)
    x = np.tanh(1.8 * (0.8 * head + 1.0 * wires))
    return fade(sp1200(room(x)))


def b_clap():
    t = t_of(0.28)
    n = noise(len(t))
    e = np.zeros_like(t)
    for d in (0.0, 0.008, 0.019):
        e += (t >= d) * np.exp(-np.clip(t - d, 0, None) / 0.007)
    tail = (t >= 0.019) * np.exp(-np.clip(t - 0.019, 0, None) / 0.09)
    x = bp(n, 1400, 0.9) * (e + 0.7 * tail)
    return fade(sp1200(room(np.tanh(1.5 * x))))


CYM = rng.uniform(3200, 11500, 14)


def b_hat(dec, length):
    t = t_of(length)
    x = sum(np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) * env(t, dec * rng.uniform(0.6, 1.2)) for f in CYM) / 6.0
    x = x + 0.5 * hp(noise(len(t)), 6500) * env(t, dec * 0.8)
    x = hp(x, 4500) * env(t, dec * 3, att=0.0003)
    return fade(sp1200(x))


def b_perc():
    t = t_of(0.20)
    x = modes(t, 520.0, (1.0, 3.17), (1.0, 0.6), (0.025, 0.015)) + 0.4 * bp(noise(len(t)), 2500, 1.0) * env(t, 0.008)
    return fade(sp1200(room(np.tanh(2.0 * x))))


KITS = [("Machine", [m_kick, m_snare, m_clap, lambda: m_hat(0.035, 0.07), lambda: m_hat(0.22, 0.35), m_perc]),
        ("Boombap", [b_kick, b_snare, b_clap, lambda: b_hat(0.03, 0.07), lambda: b_hat(0.2, 0.35), b_perc])]
DRUM = ["kick", "snare", "clap", "hat", "ohat", "perc"]

# ---------------------------------------------------------------- IMA ADPCM
STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
        209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
        2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
        20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def adpcm(x16):
    """the nibbles (low first) and the starting index; the predictor starts at 0"""
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
    by = bytes(nib[k] | nib[k + 1] << 4 for k in range(0, len(nib), 2))
    return by, i0


def down(x):
    return np.interp(np.arange(0, len(x), SR / OUT_SR), np.arange(len(x)), lp(x, 9500))


def main():
    wav_dir = sys.argv[sys.argv.index("--wav") + 1] if "--wav" in sys.argv else None
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only */",
             "/* PERF-1's built-in sampled kits (0.17.36), written by tools/smpkits.py: rendered offline by our own models (no samples from elsewhere),",
             " * 4-bit IMA ADPCM at 22.05 kHz, the low nibble first; each sound its samples, its first step index (the predictor starts at 0). */",
             "#define EN_NSKIT_BI %d" % len(KITS)]
    total = 0
    tab = []
    for k, (name, fns) in enumerate(KITS):
        ents = []
        for d, fn in enumerate(fns):
            x = norm(fn())
            y = down(x)
            x16 = np.clip(np.round(y * 32767), -32768, 32767).astype(np.int32)
            by, i0 = adpcm(x16)
            total += len(by)
            nm = f"SK{k}_{DRUM[d]}"
            lines.append(f"static const uint8_t {nm}[{len(by)}] = {{")
            for i in range(0, len(by), 24):
                lines.append("    " + ", ".join(str(b) for b in by[i:i + 24]) + ",")
            lines.append("};")
            ents.append(f"{{{nm}, {len(x16)}u, {i0}}}")
            if wav_dir:
                os.makedirs(wav_dir, exist_ok=True)
                with wave.open(os.path.join(wav_dir, f"{name}_{DRUM[d]}.wav"), "wb") as w:
                    w.setnchannels(1)
                    w.setsampwidth(2)
                    w.setframerate(OUT_SR)
                    w.writeframes(x16.astype("<i2").tobytes())
        tab.append(f"    {{\"{name}\", {{{', '.join(ents)}}}}},")
    lines.append("static const skit_t SKIT_BI[EN_NSKIT_BI] = {")
    lines += tab
    lines.append("};")
    lines.append(f"/* {total} bytes */")
    open("firmware/src/dsp/smpkits.h", "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    print(f"smpkits.h: {len(KITS)} kits, {total} bytes")


main()
