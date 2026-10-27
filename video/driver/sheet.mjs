// node driver/sheet.mjs public/flex out.png t1 t2 ...   a contact sheet of the device's screen at those times (s), for a look
import { readFileSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
const [dir, outp, ...ts] = process.argv.slice(2);
const tr = JSON.parse(readFileSync(`${dir}/trace.json`)), fb = readFileSync(`${dir}/fb.bin`);
const cols = Math.min(4, ts.length), rows = Math.ceil(ts.length / cols), W = cols * 242, H = rows * 242;
const px = Buffer.alloc(W * H * 3, 40);
ts.forEach((t, i) => {
  let f = tr.frames[Math.round(+t * tr.fps)], n = f.fb;
  for (let k = Math.round(+t * tr.fps); n === undefined; k--) n = tr.frames[k].fb;
  const ox = (i % cols) * 242, oy = Math.floor(i / cols) * 242;
  for (let y = 0; y < 240; y++) for (let x = 0; x < 240; x++) {
    const v = fb.readUInt16LE((n * 57600 + y * 240 + x) * 2), p = ((v >> 8) | (v << 8)) & 0xffff, o = ((oy + y) * W + ox + x) * 3;
    px[o] = ((p >> 11) & 31) * 255 / 31; px[o + 1] = ((p >> 5) & 63) * 255 / 63; px[o + 2] = (p & 31) * 255 / 31;
  }
});
const raw = Buffer.alloc((W * 3 + 1) * H);
for (let y = 0; y < H; y++) { raw[y * (W * 3 + 1)] = 0; px.copy(raw, y * (W * 3 + 1) + 1, y * W * 3, (y + 1) * W * 3); }
const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
const crc = (b) => { let c = -1; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
writeFileSync(outp, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
