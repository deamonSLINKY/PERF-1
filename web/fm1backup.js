// SPDX-License-Identifier: GPL-3.0-only
// Backup and restore of PERF-1's stored data over USB MIDI (the installer page): the messages, the file, and nothing that touches the browser, so a node test can check
// them against the firmware's (firmware/src/backup_proto.h, which describes the protocol).

export const BK_CHUNK = 256, BK_FRAME = 64, BK_MAX = 3840;

// the flash objects worth keeping (plat.h OBJ_*; the panel's calibration, 1, is the unit's own and is never saved), in the order they are read and written
const BK_GENRES = ["HYPNO", "LOFI", "HOUSE", "DUBSTEP", "DUB", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP", "DISCO", "FUNK", "REGGAE", "SYNTHWAVE", "AMAPIANO", "TRIPHOP"];
export const BK_OBJECTS = [
  { id: 2, name: "settings" }, { id: 3, name: "voices" }, { id: 4, name: "globals" }, { id: 0, name: BK_GENRES[0] },
  ...BK_GENRES.slice(1).map((n, i) => ({ id: 5 + i, name: n })),
];
export const bkName = (id) => (BK_OBJECTS.find((o) => o.id === id) || { name: "object " + id }).name;

// the checksum: 128 minus the sum of the data bytes, modulo 128
export function bkSum(a) {
  let s = 0;
  for (const b of a) s += b;
  return (128 - (s & 0x7F)) & 0x7F;
}

// a chunk for the FM-1 to write: raw bytes idx * BK_CHUNK .. of an object len bytes long (7D 50 42 03 obj idx(2) len(2) n(2) [2n nibbles] chk)
export function bkChunkMsg(obj, idx, len, bytes) {
  const n = bytes.length, body = [obj & 0x7F, (idx >> 7) & 0x7F, idx & 0x7F, (len >> 7) & 0x7F, len & 0x7F, (n >> 7) & 0x7F, n & 0x7F];
  for (const b of bytes) body.push(b >> 4, b & 15);
  return [0xF0, 0x7D, 0x50, 0x42, 0x03, ...body, bkSum(body), 0xF7];
}
export const bkReadMsg = (obj) => [0xF0, 0x7D, 0x50, 0x42, 0x01, obj & 0x7F, 0xF7];
export const bkCommitMsg = (obj, len) => [0xF0, 0x7D, 0x50, 0x42, 0x04, obj & 0x7F, (len >> 7) & 0x7F, len & 0x7F, 0xF7];
export const bkRestartMsg = () => [0xF0, 0x7D, 0x50, 0x42, 0x05, 0xF7];

// a frame from the FM-1 (F0 7D 50 43 obj idx(2) len(2) n(2) [2n nibbles] chk F7): its object, place, the object's length and its bytes; null when it is damaged
export function bkParseFrame(d) {
  if (d.length < 13 || d[0] !== 0xF0 || d[1] !== 0x7D || d[2] !== 0x50 || d[3] !== 0x43 || d[d.length - 1] !== 0xF7) return null;
  const n = (d[9] << 7) | d[10];
  if (d.length !== 13 + 2 * n) return null;
  if (bkSum(Array.from(d.slice(4, 11 + 2 * n))) !== d[11 + 2 * n]) return null;
  const data = new Uint8Array(n);
  for (let k = 0; k < n; k++) {
    const hi = d[11 + 2 * k], lo = d[12 + 2 * k];
    if (hi > 15 || lo > 15) return null;
    data[k] = (hi << 4) | lo;
  }
  return { obj: d[4], idx: (d[5] << 7) | d[6], len: (d[7] << 7) | d[8], data };
}

// an answer (F0 7D 50 44 obj idx status F7): the chunk's place, or 0x7F for the write; status 0 is well
export function bkParseAck(d) {
  if (d.length !== 8 || d[0] !== 0xF0 || d[1] !== 0x7D || d[2] !== 0x50 || d[3] !== 0x44) return null;
  return { obj: d[4], idx: d[5], status: d[6] };
}

export function bkCrc32(a) {
  let c = ~0;
  for (let i = 0; i < a.length; i++) {
    c ^= a[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1));
  }
  return ~c >>> 0;
}

// the file: "P1BK", version 1, the number of objects, a little JSON about it, then each object (id, length, bytes), and the CRC-32 of all that comes before
export function bkEncode(objs, info) {
  const meta = new TextEncoder().encode(JSON.stringify(info || {}));
  let size = 4 + 1 + 1 + 2 + meta.length + 4;
  for (const o of objs) size += 3 + o.data.length;
  const out = new Uint8Array(size);
  let p = 0;
  out.set([0x50, 0x31, 0x42, 0x4B, 1, objs.length, meta.length & 255, meta.length >> 8], p); p += 8;
  out.set(meta, p); p += meta.length;
  for (const o of objs) {
    out.set([o.id, o.data.length & 255, o.data.length >> 8], p); p += 3;
    out.set(o.data, p); p += o.data.length;
  }
  const crc = bkCrc32(out.subarray(0, p));
  out.set([crc & 255, (crc >>> 8) & 255, (crc >>> 16) & 255, crc >>> 24], p);
  return out;
}
export function bkDecode(buf) {
  if (buf.length < 12 || buf[0] !== 0x50 || buf[1] !== 0x31 || buf[2] !== 0x42 || buf[3] !== 0x4B) throw new Error("not a PERF-1 backup");
  if (buf[4] !== 1) throw new Error("a backup of a newer kind");
  const crc = (buf[buf.length - 4] | (buf[buf.length - 3] << 8) | (buf[buf.length - 2] << 16) | (buf[buf.length - 1] << 24)) >>> 0;
  if (bkCrc32(buf.subarray(0, buf.length - 4)) !== crc) throw new Error("the backup is damaged");
  const count = buf[5], ml = buf[6] | (buf[7] << 8);
  let p = 8, info = {};
  try { info = JSON.parse(new TextDecoder().decode(buf.subarray(p, p + ml))); } catch (_) { info = {}; }
  p += ml;
  const objs = [];
  for (let i = 0; i < count; i++) {
    if (p + 3 > buf.length - 4) throw new Error("the backup is damaged");
    const id = buf[p], len = buf[p + 1] | (buf[p + 2] << 8);
    p += 3;
    if (len > BK_MAX || p + len > buf.length - 4) throw new Error("the backup is damaged");
    objs.push({ id, data: buf.slice(p, p + len) });
    p += len;
  }
  return { info, objs };
}
