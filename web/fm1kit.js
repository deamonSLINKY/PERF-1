// SPDX-License-Identifier: GPL-3.0-only
// PERF-1's sample kits you load (0.17.38): six sounds (kick, snare, clap, hat, open hat, perc) made into a slot's image, as firmware/src/app/ui.c reads
// it: a 128-byte header ("SKU1", version 1, six sounds, the name, the length, a CRC-32 of the sounds, then each sound's place, length in samples, first
// IMA ADPCM step index and level in half dB), then the sounds, 4-bit IMA ADPCM at 22.05 kHz (the low nibble first; the predictor starts at 0), as
// tools/smpkits.py writes the built-in ones. No browser here (a node test checks it); the page decodes and resamples the files and calls kitImage.

export const KIT_SR = 22050, KIT_SLOT = 5 * 4096, KIT_HDR = 128, KIT_RAW0 = 64;
export const KIT_DRUMS = ["kick", "snare", "clap", "hat", "ohat", "perc"];
const KIT_SECS = [0.45, 0.32, 0.32, 0.1, 0.4, 0.25];                 // each sound's longest (cut shorter to fit the slot)
const KIT_TARGET = [-3.6, -6.3, -8.4, -12.8, -8.4, -5.2];            // the loudest 50 ms (dB) each drum plays at (build/kitcal.py, the built-in kits)
const STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
  209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767];
const IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8];

export function adpcmEncode(x16) {                                     // {bytes, i0}: the nibbles (low first) and the first step index
  let i0 = 0;
  while (i0 < 88 && STEP[i0] * 4 < Math.abs(x16[0] | 0)) i0++;
  let pred = 0, ix = i0;
  const out = new Uint8Array((x16.length + 1) >> 1);
  for (let i = 0; i < x16.length; i++) {
    let d = (x16[i] | 0) - pred, code = 0;
    if (d < 0) { code = 8; d = -d; }
    let st = STEP[ix], diff = st >> 3;
    if (d >= st) { code |= 4; d -= st; diff += st; }
    st >>= 1;
    if (d >= st) { code |= 2; d -= st; diff += st; }
    st >>= 1;
    if (d >= st) { code |= 1; diff += st; }
    pred = code & 8 ? pred - diff : pred + diff;
    pred = pred < -32768 ? -32768 : pred > 32767 ? 32767 : pred;
    ix = Math.max(0, Math.min(88, ix + IDX[code]));
    out[i >> 1] |= i & 1 ? code << 4 : code;
  }
  return { bytes: out, i0 };
}
export function adpcmDecode(bytes, n, i0) {                            // (the test's: as the firmware's smp_next)
  const out = new Int16Array(n);
  let pred = 0, ix = i0;
  for (let i = 0; i < n; i++) {
    const c = bytes[i >> 1] >> ((i & 1) * 4) & 15;
    let st = STEP[ix], df = st >> 3;
    if (c & 4) df += st;
    if (c & 2) df += st >> 1;
    if (c & 1) df += st >> 2;
    pred += c & 8 ? -df : df;
    pred = pred < -32768 ? -32768 : pred > 32767 ? 32767 : pred;
    ix = Math.max(0, Math.min(88, ix + IDX[c]));
    out[i] = pred;
  }
  return out;
}
export function kitCrc32(a) {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < a.length; i++) {
    c ^= a[i];
    for (let k = 0; k < 8; k++) c = c >>> 1 ^ (0xEDB88320 & -(c & 1));
  }
  return (c ^ 0xFFFFFFFF) >>> 0;
}
function loud(x) {                                                     // the loudest 50 ms, RMS dB (the mean taken out)
  let m = 0;
  for (const v of x) m += v;
  m /= x.length || 1;
  const w = Math.min(1102, x.length);
  let s = 0, best = 0;
  for (let i = 0; i < x.length; i++) {
    s += (x[i] - m) ** 2;
    if (i >= w) s -= (x[i - w] - m) ** 2;
    if (i >= w - 1 && s / w > best) best = s / w;
  }
  return 10 * Math.log10(best + 1e-20);
}
function prep(x, secs) {                                               // a sound as stored: its silence before trimmed, cut to secs with a 6 ms fade, peak at 0.89
  let a = 0, pk = 0;
  for (const v of x) pk = Math.max(pk, Math.abs(v));
  while (a < x.length && Math.abs(x[a]) < pk * 0.02) a++;
  const n = Math.max(2, Math.min(x.length - a, Math.round(secs * KIT_SR)));
  const y = new Float32Array(n), f = Math.min(n, Math.round(0.006 * KIT_SR));
  const g = pk > 0 ? 0.89 / pk : 0;
  for (let i = 0; i < n; i++) y[i] = (x[a + i] || 0) * g * (i >= n - f ? (n - i) / f : 1);
  return y;
}
// sounds: six Float32Arrays (mono, KIT_SR; null: a sound left silent). The image (at most KIT_SLOT bytes) and what was cut.
export function kitImage(name, sounds) {
  const budget = (KIT_SLOT - KIT_HDR) * 2;                             // samples (two a byte)
  let secs = KIT_SECS.slice(), want = 0;
  const len = sounds.map((x, d) => (x ? Math.min(x.length / KIT_SR, secs[d]) : 0.01));
  for (const v of len) want += Math.round(v * KIT_SR) + 1;
  const k = want > budget ? budget / want : 1;                         // all cut alike to fit
  const enc = sounds.map((x, d) => {
    const y = prep(x || new Float32Array(Math.round(0.01 * KIT_SR)), len[d] * k);
    const lv = x ? Math.max(-24, Math.min(24, Math.round(2 * (KIT_TARGET[d] - loud(y))))) : -24;
    const x16 = Int16Array.from(y, (v) => Math.max(-32768, Math.min(32767, Math.round(v * 32767))));
    return { ...adpcmEncode(x16), n: x16.length, lv };
  });
  let total = KIT_HDR;
  for (const e of enc) total += e.bytes.length;
  const img = new Uint8Array(total), dv = new DataView(img.buffer);
  img.set([0x53, 0x4B, 0x55, 0x31, 1, 6], 0);                           // "SKU1", version 1, six sounds
  const nm = String(name || "").replace(/[^\x20-\x7E]/g, "").slice(0, 12);
  for (let i = 0; i < nm.length; i++) img[8 + i] = nm.charCodeAt(i);
  let off = KIT_HDR;
  enc.forEach((e, d) => {
    dv.setUint32(28 + 12 * d, off, true);
    dv.setUint32(32 + 12 * d, e.n, true);
    img[36 + 12 * d] = e.i0;
    dv.setInt8(37 + 12 * d, e.lv);
    img.set(e.bytes, off);
    off += e.bytes.length;
  });
  dv.setUint32(20, total, true);
  dv.setUint32(24, kitCrc32(img.subarray(KIT_HDR)), true);
  return { img, cut: k < 1 ? 1 - k : 0, secs: enc.map((e) => e.n / KIT_SR), levels: enc.map((e) => e.lv / 2) };
}
// the image as the slot's sectors to send: [{obj, data}], the header's sector last (the FM-1 takes the kit when it comes); an empty slot: its header zeroed
export function kitSectors(slot, img) {
  const out = [];
  for (let s = 0; s * 4096 < img.length; s++) out.push({ obj: KIT_RAW0 + 5 * slot + s, data: img.subarray(s * 4096, Math.min(img.length, (s + 1) * 4096)) });
  return out.slice(1).concat(out.slice(0, 1));
}
export const kitClear = (slot) => [{ obj: KIT_RAW0 + 5 * slot, data: new Uint8Array(KIT_HDR) }];
