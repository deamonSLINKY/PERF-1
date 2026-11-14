// SPDX-License-Identifier: GPL-3.0-only
// (0.17.38) web/fm1kit.js: a kit's image as the firmware reads it (header, CRC, places), its ADPCM decoding back close to what went in, the slot's sectors
// header last, and the image fed to the host build (argv[2]) playing: node tests/web_kit_test.js build/host/amb_host
const fs = require("fs");
const src = fs.readFileSync(__dirname + "/../web/fm1kit.js", "utf8").replace(/export /g, "");
const K = new Function(src + "; return { KIT_SR, KIT_SLOT, KIT_HDR, KIT_RAW0, adpcmEncode, adpcmDecode, kitCrc32, kitImage, kitSectors, kitClear };")();
let fails = 0;
function check(what, ok) { if (!ok) { console.log("FAIL " + what); fails++; } }

const sr = K.KIT_SR, mk = (secs, f) => Float32Array.from({ length: Math.round(secs * sr) }, (_, i) => f(i / sr));
const kick = mk(0.6, (t) => Math.sin(2 * Math.PI * (50 * t + 4 * (1 - Math.exp(-t / 0.02)))) * Math.exp(-t / 0.25));
let r = 1;
const nz = () => ((r = (r * 1103515245 + 12345) >>> 0) / 4294967296) * 2 - 1;
const snare = mk(0.4, (t) => (0.5 * Math.sin(2 * Math.PI * 190 * t) + nz()) * Math.exp(-t / 0.1));
const hat = mk(0.1, (t) => nz() * Math.exp(-t / 0.02));
const sounds = [kick, snare, snare, hat, mk(0.5, (t) => nz() * Math.exp(-t / 0.15)), null];
const { img, cut, levels } = K.kitImage("My Kit", sounds);
const dv = new DataView(img.buffer);
check("the magic, version, six", img[0] === 0x53 && img[1] === 0x4B && img[2] === 0x55 && img[3] === 0x31 && img[4] === 1 && img[5] === 6);
check("the name", String.fromCharCode(...img.subarray(8, 14)) === "My Kit" && img[14] === 0);
check("its length, within a slot", dv.getUint32(20, true) === img.length && img.length <= K.KIT_SLOT);
check("its CRC", dv.getUint32(24, true) === K.kitCrc32(img.subarray(K.KIT_HDR)));
let place = K.KIT_HDR, ok = true;
for (let d = 0; d < 6; d++) {
  const off = dv.getUint32(28 + 12 * d, true), n = dv.getUint32(32 + 12 * d, true);
  ok = ok && off === place && n >= 2 && img[36 + 12 * d] <= 88;
  place += (n + 1) >> 1;
}
check("the six places, end to end", ok && place === img.length);
check("nothing cut (it fits)", cut === 0);
check("the levels within 12 dB", levels.slice(0, 5).every((v) => Math.abs(v) <= 12));
// the kick decoded: close to what went in
{
  const n = dv.getUint32(32, true), i0 = img[36], back = K.adpcmDecode(img.subarray(K.KIT_HDR), n, i0);
  let e = 0, s = 0;
  const ref = Int16Array.from(kick.subarray(0, n), (v) => Math.round(v * (0.89 / Math.max(...kick.map(Math.abs))) * 32767));
  for (let i = 0; i < n; i++) { e += (back[i] - ref[i]) ** 2; s += ref[i] ** 2; }
  const snr = 10 * Math.log10(s / (e || 1));
  check("the kick decoded back, SNR " + snr.toFixed(1) + " dB over 25", snr > 25);
}
// too long: cut to fit
{
  const long = Array.from({ length: 6 }, () => mk(3.0, (t) => Math.sin(2 * Math.PI * 300 * t)));
  const big = K.kitImage("Long", long);
  check("six long sounds still fit a slot (each held to its longest)", big.img.length <= K.KIT_SLOT && big.secs.every((s) => s <= 0.46));
}
// the sectors: 64 + 5 slot + k, the header's last
{
  const secs = K.kitSectors(1, img);
  check("the sectors: slot 2's, the header last", secs.length >= 1 && secs[secs.length - 1].obj === K.KIT_RAW0 + 5 && secs.every((s) => s.obj >= 69 && s.obj <= 73));
  check("an empty slot: its header zeroed", K.kitClear(2)[0].obj === K.KIT_RAW0 + 10 && K.kitClear(2)[0].data.every((b) => b === 0));
}
// the host plays it: the image written to slot 1 through the firmware's own loader, the kit's snare hit
if (process.argv[2]) {
  const { execFileSync } = require("child_process"), os = require("os"), path = require("path");
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "kit-"));
  fs.writeFileSync(path.join(dir, "kit.bin"), img);
  const sc = path.join(dir, "kit.amb");
  fs.writeFileSync(sc, "wait 500\ntap PLAY\nwait 300\nexpect kitempty0 1\nkitload 0 " + path.join(dir, "kit.bin") + "\nwait 100\nexpect kitempty0 0\nexpect kitnamelen0 6\nexpect kitempty1 1\n");
  let out = "";
  try { out = execFileSync(process.argv[2], [sc, dir], { encoding: "utf8" }); } catch (e) { out = (e.stdout || "") + (e.stderr || ""); }
  check("the host takes it (" + out.trim().split("\n").filter((l) => /FAIL/.test(l)).join("; ") + ")", !/FAIL/.test(out));
}
console.log(fails ? "web kit: " + fails + " FAILED" : "web kit: ok");
process.exit(fails ? 1 : 0);
