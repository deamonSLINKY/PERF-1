// SPDX-License-Identifier: GPL-3.0-only
// The editor's DX7 voice code (web/editor/index.html: unpackVoice, packVoice, sanitize) against the firmware's (fm6.inc), on the vectors tests/host/gvoice.c
// writes: 300 random packed voices. node tests/web_voice_test.js build/voices.json
const fs = require("fs");
const path = require("path");
const src = fs.readFileSync(path.join(__dirname, "..", "web", "editor", "index.html"), "utf8");
const i = src.indexOf("const FP = {"), j = src.indexOf("function initVoice()");
if (i < 0 || j < 0) { console.log("  FAIL the voice code was not found in the page"); process.exit(1); }
const { unpackVoice, packVoice, sanitize, FP } = new Function(src.slice(i, j) + "; return { unpackVoice, packVoice, sanitize, FP };")();
const vec = JSON.parse(fs.readFileSync(process.argv[2] || path.join(__dirname, "..", "build", "voices.json"), "utf8"));
let badU = 0, badR = 0, badP = 0;
for (const e of vec) {
  const u = unpackVoice(Uint8Array.from(e.p));
  if (u.length !== 155 || u.some((x, k) => x !== e.u[k])) badU++;
  const r = packVoice(u);
  if (r.some((x, k) => x !== e.r[k])) badR++;
  const back = unpackVoice(r);
  if (back.some((x, k) => x !== u[k])) badP++;
}
const check = (name, ok) => console.log((ok ? "  ok   " : "  FAIL ") + name);
check(vec.length + " voices: the page unpacks them as the firmware does (" + badU + " differ)", badU === 0);
check("the page packs them as the firmware does (" + badR + " differ)", badR === 0);
check("unpack(pack(unpack(x))) == unpack(x) (" + badP + " differ)", badP === 0);
console.log(badU + badR + badP ? "FAILED" : "web voice: ok");
process.exit(badU + badR + badP ? 1 : 0);
