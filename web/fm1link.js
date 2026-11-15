// SPDX-License-Identifier: GPL-3.0-only
// The studio's link to the FM-1 over USB MIDI (the messages the Editor page sends; the unit's side: firmware/src/usb.c px_end and dx_end, app/main_fm1.c
// px_service, dx_proto.h). Every message is F0 7D 50 kind ... F7 with 7-bit data. Nothing here touches the page, so tests/web_link_test.js checks it in node.
// (The site inlines this file into the studio, so every name here carries "lk" or "LK".)
//   50 01 p-hi p-lo u-hi u-lo   a setting, as its knob sets it (u = the value + 8192)        50 02            every setting, please (answered in 51 frames)
//   51 first-hi first-lo n (u-hi u-lo)*n   up to 24 settings from `first`                  56 slot [128] sum  a DX7 voice (128 packed bytes) into a slot
//   56 7E slot                  a slot, please (answered 58 slot loaded [128 sum])           57 kind n        the answer to a voice (n 7F: not taken)

export const LK_FLUSH_N = 6, LK_FLUSH_MS = 25, LK_SLOTS = 16;   // (the editor's pace: six settings each 25 ms; the unit's ring holds 32)
export const lkSum = (a) => { let s = 0; for (const b of a) s += b & 127; return (128 - (s & 127)) & 127; };
export function lkSetMsg(p, v) {
  const u = (v + 8192) & 0x3FFF;
  return [0xF0, 0x7D, 0x50, 0x50, 0x01, (p >> 7) & 127, p & 127, u >> 7, u & 127, 0xF7];
}
export const lkDumpMsg = () => [0xF0, 0x7D, 0x50, 0x50, 0x02, 0xF7];
export function lkVoiceMsg(slot, b128) {
  if (b128.length !== 128 || slot < 0 || slot >= LK_SLOTS) throw new Error("a voice is 128 bytes, into slot 0 .. 15");
  const b = Array.from(b128, (x) => x & 127);
  return [0xF0, 0x7D, 0x50, 0x56, slot, ...b, lkSum(b), 0xF7];
}
export const lkSlotMsg = (slot) => [0xF0, 0x7D, 0x50, 0x56, 0x7E, slot & 127, 0xF7];
const ours = (d) => d.length >= 5 && d[0] === 0xF0 && d[1] === 0x7D && d[2] === 0x50 && d[d.length - 1] === 0xF7;
// what came back: {k: "dump", first, vals}, {k: "ack", kind, n}, {k: "slot", slot, loaded, data (128 bytes or null)}, or null (not ours, or broken)
export function lkParse(d) {
  if (!ours(d)) return null;
  if (d[3] === 0x51) {
    const first = (d[4] << 7) | d[5], n = d[6];
    if (d.length !== 8 + 2 * n) return null;
    const vals = [];
    for (let k = 0; k < n; k++) vals.push((((d[7 + 2 * k] << 7) | d[8 + 2 * k]) & 0x3FFF) - 8192);
    return { k: "dump", first, vals };
  }
  if (d[3] === 0x57 && d.length === 7) return { k: "ack", kind: d[4], n: d[5] };
  if (d[3] === 0x58) {
    if (d.length === 7) return { k: "slot", slot: d[4], loaded: d[5], data: null };
    if (d.length !== 136) return null;
    const data = Uint8Array.from(d.slice(6, 134));
    return lkSum(data) === d[134] ? { k: "slot", slot: d[4], loaded: d[5], data } : null;
  }
  return null;
}
// the settings to send so the unit's match `want` (an array of values by setting; `have`: the unit's, null when not read yet), leaving out `skip`
export function lkDiff(want, have, skip) {
  const out = [];
  for (let p = 0; p < want.length; p++) if (!(skip && skip.has(p)) && (!have || have[p] !== want[p])) out.push(p);
  return out;
}
