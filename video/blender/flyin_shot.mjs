// node blender/flyin_shot.mjs <lcd frames dir (lcd-0001.png ..)> <outdir>   a shot.json for fm1.py --anim from ready LCD frames (no trace):
// the fly-in that ends straight down with the LCD 1054 px tall, 25 px from the top (where ChatGPT's launch card starts its screen inset).
import { readdirSync, copyFileSync, mkdirSync, writeFileSync } from "node:fs";
const [src, out] = process.argv.slice(2);
mkdirSync(out, { recursive: true });
const pngs = readdirSync(src).filter((f) => /^lcd-\d+\.png$/.test(f)).sort();
pngs.forEach((f, i) => copyFileSync(`${src}/${f}`, `${out}/scr_${String(i + 1).padStart(4, "0")}.png`));
const frames = pngs.map((_, i) => ({ png: `scr_${String(i + 1).padStart(4, "0")}.png`, keys: 0, btn: 0 }));
writeFileSync(`${out}/shot.json`, JSON.stringify({ fps: 30, frames, lcd_emit: 1.0,
  flyin: { end: frames.length - 14, lcd_px: 1054, lcd_top: 25, lens: [36, 50], start: [-0.30, -0.44, 0.30], via: [-0.04, -0.20, 0.32], fstop: 4.0 } }));
console.log(frames.length, "frames ->", out);
