// node driver/rms.mjs public/tour 5   the output's level (dB) in windows of N seconds: a quick look for a level that was left changed
import { readFileSync } from "node:fs";
const [dir, win = "5"] = process.argv.slice(2);
const b = readFileSync(`${dir}/audio.wav`), n = (b.length - 44) >> 1, w = Math.round(44100 * 2 * +win);
const out = [];
for (let o = 0; o + w <= n; o += w) {
  let s = 0, pk = 0;
  for (let i = o; i < o + w; i++) { const v = b.readInt16LE(44 + i * 2) / 32768; s += v * v; pk = Math.max(pk, Math.abs(v)); }
  out.push(`${(o / 88200).toFixed(0)}s ${(10 * Math.log10(s / w + 1e-12)).toFixed(1)}dB pk ${(20 * Math.log10(pk + 1e-9)).toFixed(1)}`);
}
console.log(out.join("\n"));
