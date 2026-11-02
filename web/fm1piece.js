// SPDX-License-Identifier: GPL-3.0-only
// Sharing a piece (the pieces page, web/pieces): a saved piece out of a genre's bank into a .piece file, a code or a link, and back into a bank, with its
// own DX7 voice when it uses one. Nothing here touches the browser or the FM-1, so a node test (tests/web_piece_test.js) checks it, and checks a bank it
// builds by loading it in the simulator. The bank's layout is firmware/src/app/project.c's; the voices' object is ui.c's vox_t.

export const PC_HDR = 16, PC_REC = 376, PC_NPIECES = 10, PC_PRG = 6, PC_REC_PAR = 2 + 2 * 6 * 6, PC_RECIPE = 0xFF;   // IMG_HDR, IMG_REC, NPIECES, EN_PRG_LEN, REC_PAR, REC_RECIPE
export const PC_TAIL = PC_HDR + PC_NPIECES * PC_REC, PC_LEN = PC_TAIL + PC_NPIECES * PC_PRG;
export const PC_NUSER = 16, PC_VOX_LEN = 8 + PC_NUSER * 128;
const IMG_MAGIC = 0x33505948, VOX_MAGIC = 0x31584F56;            // "HYP3", "VOX1"

const rd16 = (b, o) => b[o] | (b[o + 1] << 8);
const rd32 = (b, o) => (rd16(b, o) | (rd16(b, o + 2) << 16)) >>> 0;
const wr16 = (b, o, v) => { b[o] = v & 255; b[o + 1] = (v >> 8) & 255; };
const wr32 = (b, o, v) => { wr16(b, o, v & 0xFFFF); wr16(b, o + 2, v >>> 16); };

function crc32(a) {
  let c = ~0;
  for (let i = 0; i < a.length; i++) {
    c ^= a[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1));
  }
  return ~c >>> 0;
}

// ---- a genre's bank (the flash object): its header, and a piece out of it
// what is wrong with it for the pieces page (null: nothing), or "empty"
export function pcBankCheck(bank, genre) {
  if (!bank || !bank.length) return "empty";
  if (bank.length < PC_TAIL || rd32(bank, 0) !== IMG_MAGIC || bank[4] !== 2 || bank[5] !== genre || rd16(bank, 10) !== PC_REC) return "old";
  if (bank[12] !== 1 || bank[13] !== 1) return "old";             // the acid answer and the pluck's numbering as they are now (an older bank: save once first)
  return null;
}
export const pcSaved = (bank) => (pcBankCheck(bank, bank && bank.length > 5 ? bank[5] : -1) ? 0 : rd16(bank, 8));

// the piece in slot `slot` (0-9) of a genre's bank, as the file keeps it
export function pcFromBank(bank, genre, slot) {
  const bad = pcBankCheck(bank, genre);
  if (bad) throw new Error(bad === "empty" ? "nothing saved in this genre" : "this genre's bank is from an older PERF-1: save a piece in it once, then try again");
  if (!((rd16(bank, 8) >> slot) & 1)) throw new Error("that piece is not saved");
  const rec = bank.slice(PC_HDR + slot * PC_REC, PC_HDR + (slot + 1) * PC_REC);
  const tail = bank[14] === 1 && bank.length >= PC_LEN;
  const prg = tail ? bank.slice(PC_TAIL + slot * PC_PRG, PC_TAIL + (slot + 1) * PC_PRG) : new Uint8Array(PC_PRG);
  return { genre, slot, npar: rd16(bank, 6), rec, prg, voice: null, meta: {} };
}

// a setting of the piece (index p; lo: its lowest, params.json's min), or undefined when the piece predates it
export function pcGet(piece, p, lo) {
  if (p >= piece.npar || piece.rec[PC_REC_PAR + p] === PC_RECIPE) return undefined;
  return lo + piece.rec[PC_REC_PAR + p];
}
export function pcSet(piece, p, lo, v) { piece.rec[PC_REC_PAR + p] = v - lo; }

// ---- the file: "P1PC", 1, genre, slot, flags (1: a voice), the settings a record holds (2), the record's length (2), the note's length (2), then the record,
// the progression, the voice (128 bytes, when flagged), a little JSON (title, by, note, names) and the CRC-32 of all before it
export function pcEncode(piece) {
  const meta = new TextEncoder().encode(JSON.stringify(piece.meta || {}));
  const vlen = piece.voice ? 128 : 0;
  const out = new Uint8Array(14 + PC_REC + PC_PRG + vlen + meta.length + 4);
  out.set([0x50, 0x31, 0x50, 0x43, 1, piece.genre, piece.slot, piece.voice ? 1 : 0], 0);
  wr16(out, 8, piece.npar); wr16(out, 10, PC_REC); wr16(out, 12, meta.length);
  let p = 14;
  out.set(piece.rec, p); p += PC_REC;
  out.set(piece.prg, p); p += PC_PRG;
  if (piece.voice) { out.set(piece.voice, p); p += 128; }
  out.set(meta, p); p += meta.length;
  wr32(out, p, crc32(out.subarray(0, p)));
  return out;
}
export function pcDecode(buf) {
  if (buf.length < 18 || buf[0] !== 0x50 || buf[1] !== 0x31 || buf[2] !== 0x50 || buf[3] !== 0x43) throw new Error("not a PERF-1 piece");
  if (buf[4] !== 1) throw new Error("a piece from a newer PERF-1 page: open the latest pieces page");
  if (crc32(buf.subarray(0, buf.length - 4)) !== rd32(buf, buf.length - 4)) throw new Error("the piece is damaged");
  const reclen = rd16(buf, 10), ml = rd16(buf, 12), vlen = buf[7] & 1 ? 128 : 0;
  if (reclen !== PC_REC || 14 + reclen + PC_PRG + vlen + ml + 4 !== buf.length) throw new Error("the piece is damaged");
  let p = 14;
  const rec = buf.slice(p, p + reclen); p += reclen;
  const prg = buf.slice(p, p + PC_PRG); p += PC_PRG;
  const voice = vlen ? buf.slice(p, p + 128) : null; p += vlen;
  let meta = {};
  try { meta = JSON.parse(new TextDecoder().decode(buf.subarray(p, p + ml))) || {}; } catch (_) { meta = {}; }
  return { genre: buf[5], slot: buf[6], npar: rd16(buf, 8), rec, prg, voice, meta };
}

// ---- the code (and a link's #p=): the file, deflated, in base64url, after "P1."
const b64u = (u8) => { let s = ""; for (const c of u8) s += String.fromCharCode(c); return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, ""); };
const unb64u = (s) => Uint8Array.from(atob(s.replace(/-/g, "+").replace(/_/g, "/") + "===".slice((s.length + 3) % 4)), (c) => c.charCodeAt(0));
async function pipe(u8, stream) {
  const r = new Blob([u8]).stream().pipeThrough(stream);
  return new Uint8Array(await new Response(r).arrayBuffer());
}
export async function pcToCode(piece) { return "P1." + b64u(await pipe(pcEncode(piece), new CompressionStream("deflate-raw"))); }
export async function pcFromCode(text) {
  const m = /P1\.([A-Za-z0-9_-]+)/.exec(String(text || ""));
  if (!m) throw new Error("no piece code there");
  let raw;
  try { raw = await pipe(unb64u(m[1]), new DecompressionStream("deflate-raw")); } catch (_) { throw new Error("the code is damaged (cut short?)"); }
  return pcDecode(raw);
}

// ---- into a bank: the piece in slot `slot` of genre's bank (bank: what the FM-1 has, maybe empty), for a firmware with `npar` settings a piece
// (the about answer). Settings the piece predates are written as "the recipe's" (0xFF); so are those of the bank's other pieces when the bank was saved by a
// firmware with fewer. Settings the firmware does not know are dropped. The answer: the new bank, and what was lost or filled.
export function pcIntoBank(bank, genre, slot, piece, npar) {
  const bad = pcBankCheck(bank, genre);
  if (bad === "old") throw new Error("this genre's bank on the FM-1 is from an older PERF-1: save a piece in it once on the FM-1, then try again");
  const out = new Uint8Array(PC_LEN);
  let nb = npar;
  if (bad === "empty") {
    wr32(out, 0, IMG_MAGIC); out[4] = 2; out[5] = genre; wr16(out, 10, PC_REC);
  } else {
    out.set(bank.subarray(0, Math.min(bank.length, PC_LEN)));
    if (!(bank[14] === 1 && bank.length >= PC_LEN)) out.fill(0, PC_TAIL);   // a bank from before the progressions: none, as it loads
    nb = rd16(bank, 6);
  }
  const saved = rd16(out, 8);
  if (nb < npar)                                                  // the others: what they predate is their recipe's, as the firmware would read them
    for (let i = 0; i < PC_NPIECES; i++)
      if (i !== slot && ((saved >> i) & 1)) out.fill(PC_RECIPE, PC_HDR + i * PC_REC + PC_REC_PAR + nb, PC_HDR + i * PC_REC + PC_REC_PAR + npar);
  const rec = piece.rec.slice();
  if (piece.npar < npar) rec.fill(PC_RECIPE, PC_REC_PAR + piece.npar, PC_REC_PAR + npar);
  else if (piece.npar > npar) rec.fill(0, PC_REC_PAR + npar, PC_REC_PAR + piece.npar);
  out.set(rec, PC_HDR + slot * PC_REC);
  out.set(piece.prg, PC_TAIL + slot * PC_PRG);
  wr16(out, 6, npar); wr16(out, 8, saved | (1 << slot));
  out[12] = 1; out[13] = 1; out[14] = 1;
  return { bank: out, dropped: Math.max(0, piece.npar - npar), filled: Math.max(0, npar - piece.npar), replaced: !!((saved >> slot) & 1) };
}

// ---- the DX7 voice: the pluck's sound (P_CM_SND) past the plain ones (nplain) and the built-in DX voices (nfmfact) is one of the player's 16 slots
export const pcVoiceName = (v) => String.fromCharCode(...Array.from(v.slice(118, 128)).map((c) => (c < 32 || c > 126 ? 32 : c))).trimEnd() || "(no name)";
export function pcUserSlot(piece, iSnd, nplain, nfmfact) {
  const v = pcGet(piece, iSnd, 0);
  return v === undefined ? -1 : v - nplain - nfmfact;
}
// the voices' object (OBJ_VOICES; maybe empty): how many are loaded, and each
export function pcVoices(vox) {
  if (!vox || vox.length !== PC_VOX_LEN || rd32(vox, 0) !== VOX_MAGIC || rd32(vox, 4) > PC_NUSER) return [];
  const n = rd32(vox, 4), out = [];
  for (let k = 0; k < n; k++) out.push(vox.slice(8 + k * 128, 8 + (k + 1) * 128));
  return out;
}
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
// where the piece's voice goes: "same" (already loaded there: nothing to write), "free" (the next empty slot), or "conflict" (its own slot holds another
// voice: overwrite it, or take `free` when there is room, -1 when all 16 are in use)
export function pcVoicePlan(vox, voice, want) {
  const have = pcVoices(vox);
  const k = have.findIndex((v) => same(v, voice));
  if (k >= 0) return { kind: "same", slot: k };
  if (want < have.length) return { kind: "conflict", slot: want, other: pcVoiceName(have[want]), free: have.length < PC_NUSER ? have.length : -1 };
  return { kind: "free", slot: have.length };
}
// the voices' object with `voice` in `slot` (at most one past the loaded ones: slots fill in order)
export function pcVoicesWith(vox, voice, slot) {
  const have = pcVoices(vox);
  if (slot > have.length || slot >= PC_NUSER) throw new Error("no room for the voice");
  have[slot] = voice;
  const out = new Uint8Array(PC_VOX_LEN);
  wr32(out, 0, VOX_MAGIC); wr32(out, 4, have.length);
  have.forEach((v, i) => out.set(v, 8 + i * 128));
  return out;
}

// the answer to "about" (F0 7D 50 45 ver npar(2) genres voices F7, firmware/src/backup_proto.h), or null
export function pcParseAbout(d) {
  if (d.length !== 10 || d[0] !== 0xF0 || d[1] !== 0x7D || d[2] !== 0x50 || d[3] !== 0x45 || d[9] !== 0xF7) return null;
  return { ver: d[4], npar: (d[5] << 7) | d[6], genres: d[7], voices: d[8] };
}
export const pcAboutMsg = () => [0xF0, 0x7D, 0x50, 0x42, 0x06, 0xF7];
