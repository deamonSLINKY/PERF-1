// node driver/blendshot.mjs public/<trace> <outdir> <from s> <dur s> [camera.json]
// A slice of a take for Blender (blender/fm1.py --anim <outdir>/shot.json): one 240 x 240 PNG of the screen per frame, the held keys and
// buttons, the knob angles (counted from the take's start, as the video's panel does), and an optional camera path.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { deflateSync } from "node:zlib";
const [dir, outd, fs_, ds, camf] = process.argv.slice(2);
const tr = JSON.parse(readFileSync(`${dir}/trace.json`)), fb = readFileSync(`${dir}/fb.bin`);
const f0 = Math.round(+fs_ * tr.fps), n = Math.round(+ds * tr.fps);
mkdirSync(outd, { recursive: true });
const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
const crc = (b) => { let c = -1; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
function png(idx) {
  const W = 240, raw = Buffer.alloc((W * 3 + 1) * W);
  for (let y = 0; y < W; y++) for (let x = 0; x < W; x++) {
    const v = fb.readUInt16LE((idx * 57600 + y * 240 + x) * 2), p = ((v >> 8) | (v << 8)) & 0xffff, o = y * (W * 3 + 1) + 1 + x * 3;
    raw[o] = ((p >> 11) & 31) * 255 / 31; raw[o + 1] = ((p >> 5) & 63) * 255 / 63; raw[o + 2] = (p & 31) * 255 / 31;
  }
  const ih = Buffer.alloc(13); ih.writeUInt32BE(W, 0); ih.writeUInt32BE(W, 4); ih[8] = 8; ih[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ih), chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]);
}
const ROLE = { master: "MASTER", SELECT: "SELECT", PRESETS: "PRESETS", ALGORITHM: "ALGORITHM", KNOB1: "KNOB1", KNOB2: "KNOB2", KNOB3: "KNOB3", KNOB4: "KNOB4" };
let cur = 0; const acc = {}; const frames = [];
for (let i = 0; i < f0 + n && i < tr.frames.length; i++) {
  const f = tr.frames[i];
  if (f.fb !== undefined) cur = f.fb;
  for (const [r, k] of Object.entries(f.enc || {})) acc[ROLE[r] || r] = (acc[ROLE[r] || r] || 0) + k * 24;
  if (i < f0) continue;
  const name = `scr_${String(i - f0 + 1).padStart(4, "0")}.png`;
  writeFileSync(`${outd}/${name}`, png(cur));
  frames.push({ png: name, keys: f.keys || 0, btn: f.btn || 0, ang: { ...acc } });
}
const shot = { fps: tr.fps, from: +fs_, frames };
if (camf) shot.camera = JSON.parse(readFileSync(camf));
writeFileSync(`${outd}/shot.json`, JSON.stringify(shot));
console.log("wrote", frames.length, "frames to", outd);
