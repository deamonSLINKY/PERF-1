#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""LOF-1 sample builder: fetch CC0 instruments, shape them for the FM-1, report the flash cost.

Sources (both CC0 1.0, safe to ship inside GPL firmware):
  https://github.com/sgossner/VCSL        Versilian Community Sample Library
  https://github.com/sgossner/VSCO-2-CE   Versilian Studios Chamber Orchestra 2 CE

For every instrument in MANIFEST this script:
  1. downloads only the start of each chosen WAV (an HTTP range request)
  2. mixes to mono, trims silence before the attack, resamples (22,050 Hz default)
  3. shapes it: ONESHOT (cut + fade), SUS (attack + a seamless crossfaded loop),
     DECAY (short attack + tiny loop; the engine's envelope does the decay)
  4. finds each zone's root note (pitch detection checked against the file name)
  5. encodes 4-bit IMA ADPCM, the format Felucca-based firmwares play (2 samples per byte)
  6. writes the ADPCM data + zone table, decoded WAVs (what the FM-1 will actually sound like,
     with loop points in a 'smpl' chunk), an audition WAV per instrument, and BUDGET.md

Usage:
  python3 lof1_samples.py                     # build everything into ./out
  python3 lof1_samples.py --only UPBASS,KIT   # just these
  python3 lof1_samples.py --rate 16000        # smaller, crunchier (overrides per-instrument rates)
  python3 lof1_samples.py --list              # print the manifest and exit

Needs: Python 3.9+, numpy, scipy, git (for listing the libraries' files).
Edit MANIFEST below to swap notes, lengths or instruments.
"""
import argparse
import json
import math
import re
import struct
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

VCSL, VSCO = "VCSL", "VSCO-2-CE"
RAW = "https://raw.githubusercontent.com/sgossner/{repo}/master/{path}"
GIT = "https://github.com/sgossner/{repo}.git"

DEFAULT_RATE = 22050      # Felucca stores its built-in sets at 22,050 Hz
FX_RATE = 16000           # page-2 one-shots: lower rate = more seconds, more grit
USER_POOL = 240 * 1024    # Felucca's user sample area: 0xA0000..0xDBFFF
ZONE_HDR = 28             # bytes per zone in a Felucca-style zone table

# ---------------------------------------------------------------------------------------------
# MANIFEST
#   kind:   ONESHOT  cut at `len` seconds, fade the last quarter
#           SUS      `att` seconds of attack, then a `loop`-second crossfaded loop (horns, reeds)
#           DECAY    like SUS with a short loop; for struck sounds the engine fades out
#           DRUM     ONESHOT, but keeps the file's own pitch (no root detection, no key range)
#   zones:  regexes matched against file names in `dir`; one zone per regex, first match wins.
#           A tuple (dir, regex) overrides the directory for that zone (used by kits).
#   oct:    octaves to add to the note in the file name (default 1: most of these sets call
#           middle C "C3"; checked by ear-equivalent spectrum analysis, see README)
#   voices: which LOF-1 voices may offer it (LEAD, COMP, BASS, ANSWER, DRUMS, PADS)
# ---------------------------------------------------------------------------------------------
MANIFEST = {
    # ---- keys -------------------------------------------------------------------------------
    "UPRIGHT": dict(repo=VCSL, dir="Chordophones/Zithers/Upright Piano, Knight/Sustains",
                    zones=[r"Player_vl2_rr1_A1\.", r"Player_vl2_rr1_A2\.", r"Player_vl2_rr1_A3\.",
                           r"Player_vl2_rr1_A4\."],
                    kind="DECAY", att=0.55, loop=0.12, voices="LEAD COMP",
                    desc="Knight upright piano, felt-soft velocity layer"),
    "UPRIGHT2": dict(repo=VSCO, dir="Keys/Upright Nr1",
                     zones=[r"UR1_C2_mf_RR1", r"UR1_C3_mf_RR1", r"UR1_C4_mf_RR1"],
                     kind="DECAY", att=0.55, loop=0.12, voices="LEAD COMP", oct=0,
                     desc="VSCO upright piano, brighter and older-sounding"),
    "VIBES": dict(repo=VCSL, dir="Idiophones/Struck Idiophones/Vibraphone/Soft Mallets",
                  zones=[r"Vibes_soft_C3_v1", r"Vibes_soft_D4_v1", r"Vibes_soft_C5_v1"],
                  kind="DECAY", att=0.45, loop=0.15, voices="LEAD COMP ANSWER",
                  desc="Vibraphone, soft mallets"),
    "KALIMBA": dict(repo=VCSL, dir="Idiophones/Plucked Idiophones/Kalimba, Kenya",
                    zones=[r"_B2_k8_", r"_B3_k12_", r"_F#4_k14_"],
                    kind="ONESHOT", len=0.7, voices="LEAD ANSWER",
                    desc="Kenyan kalimba"),
    "GLOCK": dict(repo=VCSL, dir="Idiophones/Struck Idiophones/Glockenspiel",
                  zones=[r"glock_medium_G4_01", r"glock_medium_G5_01", r"glock_medium_G6_01"],
                  kind="ONESHOT", len=0.6, voices="LEAD ANSWER",
                  desc="Glockenspiel, medium mallets"),
    "STRUM": dict(repo=VCSL, dir="Chordophones/Composite Chordophones/Strumstick/Finger",
                  zones=[r"Str1_Main_E2_vl2_rr1", r"Str2_Main_B2_vl2_rr1", r"Str3_Main_A3_vl2_rr1",
                         r"Str3_Main_A4_vl2_rr1"],
                  kind="ONESHOT", len=0.8, voices="COMP LEAD",
                  desc="Strumstick (dulcimer-like guitar), fingered"),
    # ---- bass -------------------------------------------------------------------------------
    "UPBASS": dict(repo=VSCO, dir="Strings/Solo Contrabass/Pizz",
                   zones=[r"Pizz_E0_v1_rr1", r"Pizz_D1_v1_rr1", r"Pizz_C#2_v1_rr1"],
                   kind="ONESHOT", len=0.9, voices="BASS",
                   desc="Upright jazz bass, pizzicato"),
    # ---- answer / horns ---------------------------------------------------------------------
    "MUTETPT": dict(repo=VSCO, dir="Brass/Trumpet/harmonM-sus",
                    zones=[r"harmonM-sus_C3_v1_rr1", r"harmonM-sus_G#3_v1_rr1", r"harmonM-sus_F4_v1_rr1"],
                    kind="SUS", att=0.30, loop=0.35, voices="ANSWER LEAD",
                    desc="Trumpet with harmon mute"),
    "TENORSAX": dict(repo=VCSL, dir="Aerophones/Reed Aerophones/Tenor Saxophone/Non-Vibrato",
                     zones=[r"NV_Main_A#1_vl2_rr1", r"NV_Main_A#2_vl2_rr1", r"NV_Main_A#3_vl2_rr1"],
                     kind="SUS", att=0.30, loop=0.35, voices="ANSWER LEAD",
                     desc="Tenor saxophone, no vibrato"),
    "FLUTE": dict(repo=VSCO, dir="Woodwinds/Flute/susvib",
                  zones=[r"susvib_C4_v1_1", r"susvib_C5_v1_1", r"susvib_C6_v1_1"],
                  kind="SUS", att=0.30, loop=0.40, voices="ANSWER LEAD",
                  desc="Flute with vibrato"),
    "HARMONICA": dict(repo=VCSL, dir="Aerophones/Free Aerophones/Harmonica-Hohner-Special20-C/Sustains/Normal",
                      zones=[r"Special20_Normal_C4\.", r"Special20_Normal_G4\.", r"Special20_Normal_C5\."],
                      kind="SUS", att=0.25, loop=0.30, voices="ANSWER LEAD",
                      desc="Hohner Special 20 harmonica in C"),
    # ---- more (LOF-1 0.1): FM keys, marimba, harp, clarinet, ocarina, cello pizz ----------------
    "FMPIANO": dict(repo=VCSL, dir="Electrophones/TX81Z/FM Piano",
                    zones=[r"FMPiano_C2_vl2", r"FMPiano_C3_vl2", r"FMPiano_C4_vl2", r"FMPiano_C5_vl2"],
                    kind="DECAY", att=0.50, loop=0.12, voices="LEAD COMP", oct=0,
                    desc="Yamaha TX81Z FM piano: the 80s keys of a thousand lofi loops"),
    "MARIMBA": dict(repo=VCSL, dir="Idiophones/Struck Idiophones/Marimba",
                    zones=[r"Outrigger_F3_med", r"Outrigger_G4_med", r"Outrigger_C6_med"],
                    kind="ONESHOT", len=0.6, voices="LEAD COMP ANSWER",
                    desc="Marimba, outrigger mallets"),
    "HARP": dict(repo=VSCO, dir="Strings/Harp",
                 zones=[r"KSHarp_E3_mf", r"KSHarp_D4_mf", r"KSHarp_C5_mf"],
                 kind="ONESHOT", len=0.9, voices="LEAD COMP", oct=0,
                 desc="Concert harp"),
    "CLARINET": dict(repo=VSCO, dir="Woodwinds/Clarinet/susLong",
                     zones=[r"susLong_F3_v2_rr1", r"susLong_D4_v2_rr1", r"susLong_A#4_v2_rr1"],
                     kind="SUS", att=0.30, loop=0.35, voices="ANSWER LEAD",
                     desc="Clarinet, long sustains"),
    "OCARINA": dict(repo=VCSL, dir="Aerophones/Edge-blown Aerophones/Ocarina, Typical/Sustains/Sus",
                    zones=[r"Sus_A3\.", r"Sus_E4\.", r"Sus_C5\."],
                    kind="SUS", att=0.25, loop=0.30, voices="ANSWER LEAD",
                    desc="Ocarina"),
    "CELLOPZ": dict(repo=VSCO, dir="Strings/Cello Section/pizzT",
                    zones=[r"pizzT_E1_v2_RR1", r"pizzT_D2_v2_RR1", r"pizzT_C3_v2_RR1"],
                    kind="ONESHOT", len=0.8, voices="BASS",
                    desc="Cello section, pizzicato"),
    # ---- drums: the "Dusty" kit ---------------------------------------------------------------
    "KIT": dict(repo=VCSL, dir=None, kind="DRUM", voices="DRUMS", desc="'Dusty' acoustic lofi kit",
                zones=[
                    ("Idiophones/Struck Idiophones/Cajon", r"Cajon_hit1_f_rr1", "KICK", 0.35),
                    ("Membranophones/Struck Membranophones/Bass Drum 1", r"BDrumNew_hit_v3_rr1", "BOOM", 0.5),
                    ("Membranophones/Struck Membranophones/Snare Drum, Rope Tension/Hi",
                     r"hi_sn_Main_vl2_rr1", "SNARE", 0.35),
                    ("Membranophones/Struck Membranophones/Snare Drum, Rope Tension/Low",
                     r"low_ns_Main_vl2_rr1", "TOMSN", 0.3),
                    ("Idiophones/Struck Idiophones/Woodblock", r"wood_click_mp", "RIM", 0.12),
                    ("Idiophones/Struck Idiophones/Claps", r"SoloClap_vl2", "CLAP", 0.3),
                    ("Idiophones/Struck Idiophones/Hi-Hat Cymbal", r"HiHat_HitC_v2_rr1", "HAT", 0.15),
                    ("Idiophones/Struck Idiophones/Hi-Hat Cymbal", r"HiHat_HitO_rr1", "OHAT", 0.5),
                    ("Idiophones/Struck Idiophones/Shaker, Small", r"ShakerDouble_Down_rr1", "SHAKE", 0.25),
                ]),
    # ---- page 2 one-shots ---------------------------------------------------------------------
    "PADS": dict(repo=VCSL, dir=None, kind="DRUM", voices="PADS", rate=FX_RATE,
                 desc="Page 2 starter one-shots (16 kHz)",
                 zones=[
                     ("Idiophones/Struck Idiophones/Mark Trees/Legacy", r"windchimes_slowDesc1", "CHIMES", 1.8),
                     ("Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow", r"glass1_D#4_Slow_1_Main", "GLASS", 1.4),
                     ("Idiophones/Struck Idiophones/Finger Cymbals", r"Fing_Cymb", "TING", 1.0),
                     ("Membranophones/Other Membranophones/Ocean Drum", r"OceanDrum_Sus_1_Mid", "OCEAN", 1.8),
                     ("Aerophones/Edge-blown Aerophones/Train Whistle, Toy", r"TrainLow_Double", "TRAIN", 1.2),
                 ]),
}

# ---------------------------------------------------------------------------------------------
# library access
# ---------------------------------------------------------------------------------------------

def file_list(repo, cache):
    """every WAV path in the repo, from a blob-less clone (no audio downloaded)"""
    d = cache / "repos" / repo
    if not d.exists():
        d.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none", "--no-checkout",
                        GIT.format(repo=repo), str(d)], check=True)
    out = subprocess.run(["git", "-C", str(d), "ls-tree", "-r", "--name-only", "HEAD"],
                         check=True, capture_output=True, text=True).stdout
    return [p for p in out.splitlines() if p.lower().endswith(".wav")]


def fetch(repo, path, cache, seconds):
    """the first `seconds` of a WAV (worst case 96 kHz / 32-bit stereo), cached"""
    dest = cache / "wav" / repo / path
    if dest.exists():
        return dest.read_bytes()
    nbytes = int(4096 + seconds * 96000 * 8)
    url = RAW.format(repo=repo, path=urllib.parse.quote(path))
    req = urllib.request.Request(url, headers={"Range": f"bytes=0-{nbytes - 1}"})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read()
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return data


def read_wav(data):
    """(mono float32 samples, rate); tolerates a data chunk cut short by the range request"""
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a WAV file")
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, sr = struct.unpack_from("<HHI", body, 0)
            bps = struct.unpack_from("<H", body, 14)[0]
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack_from("<H", body, 24)[0]
            fmt = (tag, ch, sr, bps)
        elif cid == b"data":
            raw = body
            break
        pos += 8 + size + (size & 1)
    if fmt is None or raw is None:
        raise ValueError("WAV has no fmt or data chunk")
    tag, ch, sr, bps = fmt
    width = bps // 8
    raw = raw[:len(raw) // (width * ch) * width * ch]
    if tag == 3 and bps == 32:
        x = np.frombuffer(raw, "<f4").astype(np.float32)
    elif tag == 3 and bps == 64:
        x = np.frombuffer(raw, "<f8").astype(np.float32)
    elif bps == 16:
        x = np.frombuffer(raw, "<i2").astype(np.float32) / 32768
    elif bps == 24:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = (np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)).astype(np.float32)
    elif bps == 32:
        x = np.frombuffer(raw, "<i4").astype(np.float32) / 2147483648
    elif bps == 8:
        x = (np.frombuffer(raw, np.uint8).astype(np.float32) - 128) / 128
    else:
        raise ValueError(f"unsupported WAV format {tag}/{bps}")
    return x.reshape(-1, ch).mean(axis=1), sr

# ---------------------------------------------------------------------------------------------
# pitch
# ---------------------------------------------------------------------------------------------
NOTE_RX = re.compile(r"(?<![A-Za-z])([A-G])(#|b)?(-?\d)(?!\d)")
PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(m):
    return f"{NAMES[m % 12]}{m // 12 - 1}"           # MIDI 60 = C4


def name_pitch(fname):
    """the note written in a file name, as (pitch class, octave number), or None"""
    hits = NOTE_RX.findall(Path(fname).stem)
    if not hits:
        return None
    letter, acc, octv = hits[-1]
    return (PC[letter] + (1 if acc == "#" else -1 if acc == "b" else 0)) % 12, int(octv)


def detect_hz(x, sr):
    """YIN on a slice past the attack; (hz, confidence 0..1)"""
    start = int(0.08 * sr)
    w = x[start:start + int(0.12 * sr)]
    if len(w) < 2048:
        w = x[:int(0.2 * sr)]
    w = w - w.mean()
    n = len(w) // 2
    if n < 64 or not np.any(w):
        return None, 0.0
    tmin, tmax = int(sr / 2000), min(n - 1, int(sr / 25))
    d = np.array([np.sum((w[:n] - w[t:t + n]) ** 2) for t in range(tmax + 1)])
    cmnd = np.ones_like(d)
    cmnd[1:] = d[1:] * np.arange(1, len(d)) / np.maximum(np.cumsum(d[1:]), 1e-12)
    cand = np.where(cmnd[tmin:] < 0.15)[0]
    t = (cand[0] + tmin) if len(cand) else int(np.argmin(cmnd[tmin:]) + tmin)
    while t + 1 <= tmax and cmnd[t + 1] < cmnd[t]:
        t += 1
    if 1 <= t < tmax:                                   # parabolic refinement
        a, b, c = cmnd[t - 1], cmnd[t], cmnd[t + 1]
        den = a - 2 * b + c
        t = t + (0.5 * (a - c) / den if den else 0)
    return sr / t, float(max(0.0, 1 - cmnd[int(round(t))]))


def find_root(x, sr, fname, octave):
    """MIDI root from the file name, shifted by the instrument's octave convention (`oct` in the
    manifest: the libraries number octaves differently from set to set, and pitch detection
    alone is unreliable on low and bell-like notes). Detection only fine-tunes the cents."""
    named = name_pitch(fname)
    hz, conf = detect_hz(x, sr)
    if named is None:
        if hz:
            return 69 + 12 * math.log2(hz / 440), "detected (no note in name)"
        return 60.0, "unknown, assumed C4"
    pc, octv = named
    root = float(pc + 12 * (octv + 1 + octave))
    if hz and conf >= 0.5:
        det = 69 + 12 * math.log2(hz / 440)
        cents = ((det - root + 6) % 12) - 6            # same pitch class, any octave
        if abs(cents) <= 0.5:
            return root + cents, f"name, tuned {cents * 100:+.0f}c"
    return root, "name"


# ---------------------------------------------------------------------------------------------
# shaping
# ---------------------------------------------------------------------------------------------

def trim_start(x, sr, rel=0.02, pre=0.002):
    pk = np.max(np.abs(x)) or 1.0
    idx = np.argmax(np.abs(x) > pk * rel)
    return x[max(0, idx - int(pre * sr)):]


def to_rate(x, sr, to):
    if sr == to:
        return x
    g = math.gcd(int(sr), int(to))
    return resample_poly(x, to // g, sr // g).astype(np.float32)


def fade_out(x, frac=0.25):
    n = max(1, int(len(x) * frac))
    x = x.copy()
    x[-n:] *= np.cos(np.linspace(0, math.pi / 2, n)) ** 2
    return x


def make_loop(x, sr, att, loop, period):
    """attack + loop; returns (samples, loop start, loop end). The end crossfades into the
    material just before the loop start, so the jump back is seamless."""
    le = min(len(x) - 1, int((att + loop) * sr))
    want = int(loop * sr)
    if period:
        want = max(int(round(period)), int(round(round(want / period) * period)))
    ls = max(int(0.04 * sr), le - want)
    win, best, best_ls = max(32, int(0.004 * sr)), None, ls
    span = int(period) if period else int(0.005 * sr)
    for cand in range(max(win, ls - span), min(le - win * 2, ls + span)):
        e = np.sum((x[cand - win:cand + win] - x[le - win:le + win]) ** 2)
        if best is None or e < best:
            best, best_ls = e, cand
    ls = best_ls
    xf = min(int(0.03 * sr), (le - ls) // 2, ls)
    y = x[:le].copy()
    if xf > 4:
        t = np.sin(np.linspace(0, math.pi / 2, xf)) ** 2
        y[le - xf:le] = y[le - xf:le] * (1 - t) + x[ls - xf:ls] * t
    return y, ls, le

# ---------------------------------------------------------------------------------------------
# IMA ADPCM (4 bit, low nibble first, predictor 0 / index 0 at the start)
# ---------------------------------------------------------------------------------------------
STEPS = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
         73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
         449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
         2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
         9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
         32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8]


def _step(code, pred, idx):
    st = STEPS[idx]
    d = st >> 3
    if code & 4: d += st
    if code & 2: d += st >> 1
    if code & 1: d += st >> 2
    pred = max(-32768, min(32767, pred - d if code & 8 else pred + d))
    return pred, max(0, min(88, idx + IDX[code & 7]))


def adpcm_encode(s16, loop_start):
    """bytes, and the decoder state (predictor, index) at loop_start"""
    pred, idx, out, nib, state = 0, 0, bytearray(), [], (0, 0)
    for i, v in enumerate(s16):
        if i == loop_start:
            state = (pred, idx)
        diff, st, code = int(v) - pred, STEPS[idx], 0
        if diff < 0:
            code, diff = 8, -diff
        if diff >= st: code |= 4; diff -= st
        if diff >= st >> 1: code |= 2; diff -= st >> 1
        if diff >= st >> 2: code |= 1
        pred, idx = _step(code, pred, idx)
        nib.append(code)
    if len(nib) & 1:
        nib.append(0)
    for a, b in zip(nib[::2], nib[1::2]):
        out.append(a | (b << 4))
    return bytes(out), state


def adpcm_decode(data, n):
    pred, idx, out = 0, 0, np.empty(n, np.int16)
    for i in range(n):
        code = (data[i >> 1] >> (4 * (i & 1))) & 15
        pred, idx = _step(code, pred, idx)
        out[i] = pred
    return out

# ---------------------------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------------------------

def write_wav(path, s16, sr, loop=None, root=None):
    """16-bit mono WAV; a 'smpl' chunk carries the root note and loop for samplers"""
    data = np.asarray(s16, "<i2").tobytes()
    chunks = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, 1, sr, sr * 2, 2, 16)
    if loop or root is not None:
        r = int(round(root)) if root is not None else 60
        cents = int(round(((root or r) - r) * 100)) if root is not None else 0
        frac = int(((-cents) % 100) / 100 * 0xFFFFFFFF) if cents < 0 else 0
        nl = 1 if loop else 0
        body = struct.pack("<9I", 0, 0, int(1e9 / sr), r if cents >= 0 else r - 1, frac, 0, 0, nl, 0)
        if loop:
            body += struct.pack("<6I", 0, 0, loop[0], loop[1] - 1, 0, 0)
        chunks += struct.pack("<4sI", b"smpl", len(body)) + body
    chunks += struct.pack("<4sI", b"data", len(data)) + data
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


def play_zone(z, s16, out_sr, semis, dur, decay):
    """render one note the way a simple sampler would (linear interpolation, loop, envelope)"""
    x = s16.astype(np.float32) / 32768
    step = 2 ** (semis / 12) * z["rate"] / out_sr
    n = int(dur * out_sr)
    pos = np.arange(n) * step
    if z["loop"]:
        ls, le = z["loop"]
        over = pos >= le
        pos[over] = ls + np.mod(pos[over] - ls, le - ls)
    else:
        pos = pos[pos < len(x) - 1]
    i = pos.astype(int)
    f = pos - i
    y = x[i] * (1 - f) + x[np.minimum(i + 1, len(x) - 1)] * f
    t = np.arange(len(y)) / out_sr
    env = np.exp(-t / decay) if decay else np.ones_like(t)
    rel = min(len(y), int(0.08 * out_sr))
    env[-rel:] *= np.linspace(1, 0, rel)
    return y * env


def audition(name, inst, zones, decoded, out_sr=22050):
    """a short phrase across the instrument's range (or a beat, for kits)"""
    if inst["kind"] == "DRUM":
        if name == "KIT":
            bpm, swing = 82, 0.58
            beat = 60 / bpm
            pat = {"KICK": [0, 2.5, 4, 6.75], "SNARE": [1, 3, 5, 7], "HAT": [i / 2 for i in range(16)],
                   "SHAKE": [i + 0.75 for i in range(8)], "RIM": [3.5, 7.5], "OHAT": [7.5]}
            total = np.zeros(int((8 * beat + 1) * out_sr), np.float32)
            for z, s in zip(zones, decoded):
                for b in pat.get(z["label"], []):
                    if b % 1 == 0.5:
                        b = math.floor(b) + swing
                    y = play_zone(z, s, out_sr, 0, len(s) / z["rate"], 0)
                    k = int(b * beat * out_sr)
                    total[k:k + len(y)] += y[:len(total) - k] * (0.5 if z["label"] in ("HAT", "SHAKE") else 0.9)
            return total
        parts = [np.concatenate([play_zone(z, s, out_sr, 0, len(s) / z["rate"], 0),
                                 np.zeros(int(0.25 * out_sr), np.float32)]) for z, s in zip(zones, decoded)]
        return np.concatenate(parts)
    lo, hi = min(z["root"] for z in zones), max(z["root"] for z in zones)
    base = int(round(lo)) - (int(round(lo)) % 12)            # a C at or below the lowest root
    scale = [0, 3, 5, 7, 10]                                 # minor pentatonic: lofi-safe
    notes = [base + 12 * o + s for o in range(4) for s in scale]
    notes = [m for m in notes if lo - 3 <= m <= hi + 7][:10] or [int(round(lo))]
    sus = inst["kind"] == "SUS"
    decay = None if sus else (0.35 if inst["kind"] == "ONESHOT" else 0.6)
    phrase = []
    for k, m in enumerate(notes + notes[-2::-1][:4]):
        z = min(zones, key=lambda q: (not (q["lo"] <= m <= q["hi"]), abs(q["root"] - m)))
        s = decoded[zones.index(z)]
        dur = 0.32 if k < len(notes) + 3 else 1.1
        phrase.append(play_zone(z, s, out_sr, m - z["root"], dur, decay))
    return np.concatenate(phrase)


def build(name, inst, files_by_repo, cache, out, rate_override):
    rate = rate_override or inst.get("rate", DEFAULT_RATE)
    kind = inst["kind"]
    zones, decoded, blob, credits = [], [], bytearray(), []
    for spec in inst["zones"]:
        if isinstance(spec, tuple):
            folder, rx, label, length = spec
        else:
            folder, rx, label, length = inst["dir"], spec, None, inst.get("len")
        cands = sorted(f for f in files_by_repo[inst["repo"]]
                       if f.rsplit("/", 1)[0] == folder and re.search(rx, f.rsplit("/", 1)[1]))
        if not cands:
            print(f"   ! {name}: nothing matches {rx!r} in {folder}")
            continue
        path = cands[0]
        need = (length or (inst.get("att", 0) + inst.get("loop", 0))) + 0.6
        x, sr = read_wav(fetch(inst["repo"], path, cache, need))
        x = trim_start(x, sr)
        root, how = (None, "drum") if kind == "DRUM" else find_root(x, sr, path, inst.get("oct", 1))
        y = to_rate(x, sr, rate)
        loop = None
        if kind in ("SUS", "DECAY"):
            period = rate / (440 * 2 ** ((root - 69) / 12)) if root else None
            y, ls, le = make_loop(y, rate, inst["att"], inst["loop"], period)
            loop = (ls, le)
        else:
            y = fade_out(y[:int(length * rate)])
        zones.append(dict(file=path, label=label, root=root, how=how, n=len(y), loop=loop, rate=rate))
        decoded.append(y)
        credits.append(path)
    if not zones:
        return None
    # one gain for the whole instrument (zones keep their relative levels), peak at -1 dBFS
    gain = 0.89 / max(np.max(np.abs(y)) for y in decoded)
    for k, (z, y) in enumerate(zip(zones, decoded)):
        s16 = np.clip(np.round(y * gain * 32767), -32768, 32767).astype(np.int16)
        data, (pred, idx) = adpcm_encode(s16, z["loop"][0] if z["loop"] else 0)
        z.update(off=len(blob), bytes=len(data), pred=pred, idx=idx)
        blob += data
        decoded[k] = adpcm_decode(data, len(s16))
    # key ranges: split halfway between neighbouring roots
    if kind != "DRUM":
        order = sorted(range(len(zones)), key=lambda k: zones[k]["root"])
        for j, k in enumerate(order):
            prev_r = zones[order[j - 1]]["root"] if j else None
            next_r = zones[order[j + 1]]["root"] if j + 1 < len(order) else None
            zones[k]["lo"] = 0 if prev_r is None else int(math.floor((prev_r + zones[k]["root"]) / 2)) + 1
            zones[k]["hi"] = 127 if next_r is None else int(math.floor((zones[k]["root"] + next_r) / 2))
    else:
        for k, z in enumerate(zones):
            z["lo"] = z["hi"] = 36 + k                      # one key per hit, from C2 up
            z["root"] = 36 + k
    # write
    sets, wavs, prev = out / "sets", out / "wav" / name, out / "preview"
    for d in (sets, wavs, prev):
        d.mkdir(parents=True, exist_ok=True)
    (sets / f"{name}.adpcm").write_bytes(bytes(blob))
    table = dict(name=name, kind=kind, voices=inst["voices"].split(), desc=inst["desc"], rate=rate,
                 bytes=len(blob) + ZONE_HDR * len(zones),
                 zones=[{k: z[k] for k in ("label", "file", "off", "n", "loop", "root", "lo", "hi", "pred",
                                           "idx", "how")} for z in zones])
    for z in table["zones"]:
        z["root_note"] = note_name(int(round(z["root"])))
        z["root"] = round(z["root"], 2)
    (sets / f"{name}.json").write_text(json.dumps(table, indent=1))
    for z, s in zip(zones, decoded):
        tag = z["label"] or note_name(int(round(z["root"])))
        write_wav(wavs / f"{name}_{tag}.wav", s, rate, z["loop"], z["root"])
    a = audition(name, inst, zones, decoded)
    a = a / (np.max(np.abs(a)) or 1) * 0.9
    write_wav(prev / f"{name}.wav", np.round(a * 32767).astype(np.int16), 22050)
    return table, credits


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", help="comma-separated instrument names")
    ap.add_argument("--rate", type=int, help="store every instrument at this rate (Hz)")
    ap.add_argument("--out", default="out")
    ap.add_argument("--cache", default=".cache")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    names = [n.strip().upper() for n in a.only.split(",")] if a.only else list(MANIFEST)
    if a.list:
        for n in names:
            m = MANIFEST[n]
            print(f"{n:10} {m['kind']:7} {len(m['zones'])} zones  {m['voices']:18} {m['desc']}")
        return
    out, cache = Path(a.out), Path(a.cache)
    files = {}
    for repo in sorted({MANIFEST[n]["repo"] for n in names}):
        print(f"listing {repo} ...")
        files[repo] = file_list(repo, cache)
    results, credits = [], {}
    for n in names:
        print(f"building {n} ...")
        r = build(n, MANIFEST[n], files, cache, out, a.rate)
        if r:
            results.append(r[0])
            credits[n] = (MANIFEST[n]["repo"], r[1])
    report(results, credits, out)
    reel = []                                       # every audition back to back
    for t in results:
        x, _ = read_wav((out / "preview" / f"{t['name']}.wav").read_bytes())
        reel += [x, np.zeros(int(0.7 * 22050), np.float32)]
    if reel:
        write_wav(out / "preview" / "ALL.wav", np.round(np.concatenate(reel) * 32767).astype(np.int16), 22050)
    print(f"\ndone: {out / 'BUDGET.md'}")


def report(results, credits, out):
    total = sum(t["bytes"] for t in results)
    L = ["# LOF-1 sample budget", "",
         "Flash cost of each built-in instrument as 4-bit IMA ADPCM (2 samples per byte), zone table included.",
         f"For scale: Felucca's whole user sample area is {USER_POOL // 1024} KB; built-in sets live in the "
         "firmware image instead, which has its own (smaller, untested) headroom.", "",
         "| Instrument | Kind | Voices | Zones | Rate | Seconds | KB |", "|---|---|---|---|---|---|---|"]
    for t in results:
        secs = sum(z["n"] for z in t["zones"]) / t["rate"]
        L.append(f"| **{t['name']}** — {t['desc']} | {t['kind']} | {' '.join(t['voices'])} | "
                 f"{len(t['zones'])} | {t['rate']} | {secs:.1f} | {t['bytes'] / 1024:.1f} |")
    L += ["", f"**Total: {total / 1024:.1f} KB** ({100 * total / USER_POOL:.0f}% of a 240 KB pool)", "",
          "## By voice", ""]
    by = {}
    for t in results:
        for v in t["voices"]:
            by.setdefault(v, []).append(t)
    for v in ("LEAD", "COMP", "BASS", "ANSWER", "DRUMS", "PADS"):
        if v in by:
            L.append(f"- **{v}:** " + ", ".join(f"{t['name']} ({t['bytes'] / 1024:.0f} KB)" for t in by[v]))
    L += ["", "Instruments offered on more than one voice are stored once.", "", "## Zones", ""]
    for t in results:
        L.append(f"**{t['name']}**")
        for z in t["zones"]:
            where = z["label"] or f"root {z['root_note']} ({z['how']}), keys {note_name(z['lo'])}–{note_name(z['hi'])}"
            loop = f", loop {z['loop'][1] - z['loop'][0]} samples" if z["loop"] else ""
            L.append(f"- {where}: {z['n']} samples{loop} — `{z['file'].rsplit('/', 1)[1]}`")
        L.append("")
    (out / "BUDGET.md").write_text("\n".join(L))
    C = ["# Sample credits", "",
         "All samples are CC0 1.0 (public domain) from Versilian Studios; credit is a courtesy, not a requirement.",
         "- VCSL: https://github.com/sgossner/VCSL", "- VSCO-2 CE: https://github.com/sgossner/VSCO-2-CE", ""]
    for n, (repo, files) in credits.items():
        C.append(f"**{n}** ({repo})")
        C += [f"- {f}" for f in files]
        C.append("")
    (out / "CREDITS.md").write_text("\n".join(C))


if __name__ == "__main__":
    sys.exit(main())
