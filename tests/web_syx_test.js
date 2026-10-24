// SPDX-License-Identifier: GPL-3.0-only
// The installer page's DX7 SysEx handling (web/amb_installer.html: nameOf, parseSyx, vmsg), taken from the page itself: a 32-voice bank, a single
// voice, a bare 4096-byte bank, and the message the firmware takes. node tests/web_syx_test.js
const fs = require("fs");
const path = require("path");
const src = fs.readFileSync(path.join(__dirname, "..", "web", "amb_installer.html"), "utf8");
function grab(start, endMarker) {
  const i = src.indexOf(start);
  if (i < 0) throw new Error("missing: " + start);
  const j = src.indexOf(endMarker, i);
  return src.slice(i, j);
}
const code = grab("const nameOf =", "let voices = [];") + "\n" + grab("const vmsg =", "$(\"vsend\")");
const { nameOf, parseSyx, vmsg } = new Function(code + "; return { nameOf, parseSyx, vmsg };")();
let bad = 0;
const check = (name, ok) => { console.log((ok ? "  ok   " : "  FAIL ") + name); if (!ok) bad++; };

// a bank: F0 43 00 09 20 00, 32 x 128 bytes (names "VOICE 01" ..), the checksum, F7
const bank = [0xF0, 0x43, 0x00, 0x09, 0x20, 0x00];
const body = [];
for (let v = 0; v < 32; v++) {
  const r = new Array(128).fill(0);
  const nm = ("VOICE " + String(v + 1).padStart(2, "0") + "  ").slice(0, 10);
  for (let k = 0; k < 10; k++) r[118 + k] = nm.charCodeAt(k);
  r[0] = v;
  body.push(...r);
}
let s = 0; for (const b of body) s += b;
bank.push(...body, (128 - (s & 0x7F)) & 0x7F, 0xF7);
check("a bank is 4104 bytes", bank.length === 4104);
let v = parseSyx(new Uint8Array(bank), "bank.syx");
check("32 voices found in a bank", v.length === 32);
check("the names: VOICE 01 .. VOICE 32", v[0].name === "VOICE 01" && v[31].name === "VOICE 32");
check("each voice is its 128 bytes", v[5].packed.length === 128 && v[5].packed[0] === 5);

v = parseSyx(new Uint8Array(body), "raw.syx");
check("a bare 4096-byte bank is read too", v.length === 32 && v[1].name === "VOICE 02");

const single = [0xF0, 0x43, 0x00, 0x00, 0x01, 0x1B];
const sv = new Array(155).fill(0);
const nm2 = "FLUTE     ";
for (let k = 0; k < 10; k++) sv[145 + k] = nm2.charCodeAt(k);
let s2 = 0; for (const b of sv) s2 += b;
single.push(...sv, (128 - (s2 & 0x7F)) & 0x7F, 0xF7);
check("a single voice is 163 bytes", single.length === 163);
v = parseSyx(new Uint8Array(single), "one.syx");
check("a single voice: its name, its bytes kept whole", v.length === 1 && v[0].name === "FLUTE" && v[0].single.length === 163 && v[0].single[0] === 0xF0);

v = parseSyx(new Uint8Array([...bank, ...single]), "two.syx");
check("a bank then a single voice: 33 voices", v.length === 33);
check("nothing found in noise", parseSyx(new Uint8Array([1, 2, 3, 0xF0, 1, 2, 0xF7]), "x").length === 0);

const m = vmsg(3, v[0].packed);
check("the message for the firmware: F0 7D 50 56, slot, 128 bytes, a checksum, F7 (135 bytes)", m.length === 135 && m[0] === 0xF0 && m[1] === 0x7D && m[2] === 0x50 && m[3] === 0x56 && m[4] === 3 && m[134] === 0xF7);
let dsum = 0; for (let i = 5; i < 133; i++) dsum += m[i];
check("its checksum: (sum + checksum) & 0x7F == 0, as the firmware checks", ((dsum + m[133]) & 0x7F) === 0);
check("every byte between F0 and F7 is 7-bit clean", m.slice(1, 134).every((b) => b < 128));
console.log(bad ? "FAILED" : "web syx: ok");
process.exit(bad ? 1 : 0);
