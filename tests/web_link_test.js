// SPDX-License-Identifier: GPL-3.0-only
// The studio's FM-1 link (web/fm1link.js): the messages it sends, read back as the firmware reads them (firmware/src/usb.c px_end and dx_end: their
// lengths and fields), and the frames the firmware sends (firmware/src/dx_proto.h px_build_dump, px_build_voice: rebuilt here) read as the page reads them.
// The firmware's length checks are read from usb.c, so a change there that the page does not follow fails here.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const src = fs.readFileSync(path.join(root, "web", "fm1link.js"), "utf8").replace(/^export /gm, "");
const L = new Function(src + "; return { LK_FLUSH_N, LK_FLUSH_MS, LK_SLOTS, lkSum, lkSetMsg, lkDumpMsg, lkVoiceMsg, lkSlotMsg, lkParse, lkDiff };")();
const usb = fs.readFileSync(path.join(root, "firmware", "src", "usb.c"), "utf8");
let bad = 0;
const check = (name, ok) => { console.log((ok ? "  ok   " : "  FAIL ") + name); if (!ok) bad++; };
const throws = (f) => { try { f(); return false; } catch (_) { return true; } };
const inner = (m) => m.slice(1, -1);                       // what usb.c's px_buf / dx_buf hold: the bytes between F0 and F7
const seven = (m) => m.every((b, i) => i === 0 || i === m.length - 1 || (b >= 0 && b < 128));

// ---- a setting: px_end's "n == 8u ... 0x50u ... 0x01u", p and the value + 8192 as two 7-bit bytes each
check("usb.c still reads a setting as 8 bytes", /n == 8u && m\[0\] == 0x7Du && m\[1\] == 0x50u && m\[2\] == 0x50u && m\[3\] == 0x01u/.test(usb));
let ok = true;
for (const [p, v] of [[0, 0], [5, 120], [99, 87], [175, -24], [130, 11], [127, -8192], [128, 8191], [64, -1]]) {
  const m = inner(L.lkSetMsg(p, v));
  const gp = (m[4] << 7 | m[5]) & 0x3FFF, gv = ((m[6] << 7 | m[7]) & 0x3FFF) - 8192;
  if (m.length !== 8 || gp !== p || gv !== v || !seven(L.lkSetMsg(p, v))) { ok = false; console.log("    setting", p, v, "->", gp, gv, m.length); }
}
check("a setting reads back as the firmware decodes it (p 0..175, values -8192..8191)", ok);
check("usb.c reads 'every setting' as 4 bytes", /n == 4u && m\[0\] == 0x7Du && m\[1\] == 0x50u && m\[2\] == 0x50u && m\[3\] == 0x02u/.test(usb) && inner(L.lkDumpMsg()).length === 4);
check("usb.c reads 'a slot, please' as 5 bytes, the slot last", /n == 5u && m\[0\] == 0x7Du && m\[1\] == 0x50u && m\[2\] == 0x56u && m\[3\] == 0x7Eu/.test(usb)
      && inner(L.lkSlotMsg(9)).length === 5 && inner(L.lkSlotMsg(9))[4] === 9);

// ---- a voice: dx_end's "n == 133u && m[3] < 16u"
check("usb.c takes our voice as 133 bytes, slot < 16", /n == 133u && m\[3\] < 16u/.test(usb));
const v128 = Uint8Array.from({ length: 128 }, (_, i) => (i * 37 + 5) & 127);
const vm = L.lkVoiceMsg(3, v128);
check("a voice: 133 bytes inside, slot 3, its bytes, 7-bit", inner(vm).length === 133 && vm[4] === 3 && v128.every((b, i) => vm[5 + i] === b) && seven(vm));
check("its checksum makes the bytes sum to 0 (mod 128), as the editor's", ((v128.reduce((a, b) => a + b, 0) + vm[133]) & 127) === 0);
check("a voice must be 128 bytes into slot 0..15", throws(() => L.lkVoiceMsg(16, v128)) && throws(() => L.lkVoiceMsg(0, v128.subarray(0, 127))) && !throws(() => L.lkVoiceMsg(15, v128)));

// ---- the frames back: px_build_dump and px_build_voice, as dx_proto.h builds them
function buildDump(first, vals) {
  const f = [0xF0, 0x7D, 0x50, 0x51, (first >> 7) & 0x7F, first & 0x7F, vals.length];
  for (const v of vals) { const u = (v + 8192) & 0x3FFF; f.push(u >> 7, u & 0x7F); }
  return [...f, 0xF7];
}
function buildVoice(slot, loaded, d) {
  const f = [0xF0, 0x7D, 0x50, 0x58, slot & 0x7F, loaded & 0x7F];
  if (d) { let s = 0; for (const b of d) { f.push(b & 0x7F); s += b & 0x7F; } f.push((128 - (s & 0x7F)) & 0x7F); }
  return [...f, 0xF7];
}
const proto = fs.readFileSync(path.join(root, "firmware", "src", "dx_proto.h"), "utf8");
check("dx_proto.h still builds the frames rebuilt here", /f\[o\+\+\] = 0x51;/.test(proto) && /f\[o\+\+\] = 0x58;/.test(proto) && /v\[k\] \+ 8192/.test(proto));
const vals = Array.from({ length: 24 }, (_, k) => (k * 53) % 200 - 60);
const d = L.lkParse(buildDump(152, vals));
check("a dump frame: first 152, 24 values back", d && d.k === "dump" && d.first === 152 && d.vals.length === 24 && d.vals.every((x, i) => x === vals[i]));
// the whole dump, 176 settings in frames of 24, into an array as the studio fills it
const want = Array.from({ length: 176 }, (_, p) => (p * 7) % 101 - 3), have = new Array(176).fill(null);
for (let i = 0; i < 176; i += 24) { const r = L.lkParse(buildDump(i, want.slice(i, i + 24))); r.vals.forEach((v, k) => (have[r.first + k] = v)); }
check("the whole dump: 176 settings in 8 frames", have.every((x, i) => x === want[i]));
check("a broken dump frame (short) is not taken", L.lkParse(buildDump(0, [1, 2, 3]).filter((_, i) => i !== 8)) === null);
const s = L.lkParse(buildVoice(4, 7, v128));
check("a voice back: slot 4, 7 held, its 128 bytes", s && s.k === "slot" && s.slot === 4 && s.loaded === 7 && s.data.length === 128 && s.data.every((b, i) => b === v128[i]));
const e = L.lkParse(buildVoice(9, 7, null));
check("an empty slot back: no bytes", e && e.k === "slot" && e.slot === 9 && e.loaded === 7 && e.data === null);
const bent = buildVoice(4, 7, v128); bent[20] ^= 1;
check("a voice with a wrong checksum is not taken", L.lkParse(bent) === null);
const a = L.lkParse([0xF0, 0x7D, 0x50, 0x57, 3, 2, 0xF7]), no = L.lkParse([0xF0, 0x7D, 0x50, 0x57, 3, 0x7F, 0xF7]);
check("the answer to a voice: kind and slot, 7F not taken", a && a.k === "ack" && a.kind === 3 && a.n === 2 && no && no.n === 0x7F);
check("usb.c's answer is 7 bytes: F0 7D 50 57 kind n F7", /ack\[3\] = 0x57, ack\[4\] = \(uint8_t\)kind, ack\[5\] = r < 0 \? 0x7Fu : \(uint8_t\)r, ack\[6\] = 0xF7/.test(fs.readFileSync(path.join(root, "firmware", "src", "app", "main_fm1.c"), "utf8")));
check("others' messages are not ours", L.lkParse([0xF0, 0x43, 0, 9, 0xF7]) === null && L.lkParse([0x90, 60, 100]) === null && L.lkParse([0xF0, 0x7D, 0x50, 0x45, 2, 1, 48, 22, 3, 0xF7]) === null);

// ---- what goes: the settings that differ, the player's own left out
const skip = new Set([2, 4]);
check("the diff: the ones that differ, not the skipped", JSON.stringify(L.lkDiff([1, 2, 3, 4, 5], [1, 0, 0, 0, 0], skip)) === JSON.stringify([1, 3]));
check("the diff when the unit's are not read: all but the skipped", L.lkDiff([1, 2, 3, 4, 5], null, skip).length === 3);
check("the pace fits the unit's ring (6 of 32 a turn)", L.LK_FLUSH_N <= 32 / 4 && /#define PX_N 32u/.test(usb));

// ---- the studio uses what it needs (it inlines this file; make_vis_site.py, make_dev_artifact.py)
const st = fs.readFileSync(path.join(root, "web", "studio", "index.html"), "utf8");
for (const n of ["lkSetMsg", "lkDumpMsg", "lkVoiceMsg", "lkSlotMsg", "lkParse", "lkDiff"]) check("the studio calls " + n, st.includes(n + "("));
check("the dev page inlines fm1link.js", fs.readFileSync(path.join(root, "tools", "make_dev_artifact.py"), "utf8").includes('"fm1link.js"'));

if (bad) { console.log(bad + " failed"); process.exit(1); }
console.log("web link: all ok");
