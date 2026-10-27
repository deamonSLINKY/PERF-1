// SPDX-License-Identifier: GPL-3.0-only
// node driver/capture.mjs scenes/flex.mjs [outdir]  ->  outdir/audio.wav, trace.json, fb.bin (the screens that changed)
import { mkdirSync, writeFileSync, createWriteStream } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { createHash } from "node:crypto";
import { Emu, wavBytes } from "./emu.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const scenePath = resolve(process.argv[2] || "scenes/flex.mjs");
const out = resolve(process.argv[3] || resolve(here, "../public/flex"));
const wasm = resolve(here, "../../build/emu/amb.wasm");
const scene = (await import(pathToFileURL(scenePath).href)).default;

mkdirSync(out, { recursive: true });
const emu = await Emu.create({ wasmPath: wasm, seed: scene.seed ?? 0, master: scene.master ?? 2800 });
const frames = [], fbs = [];
let fbCount = 0;
emu.onframe = (f) => {
  const rec = { n: f.n, btn: f.btn, keys: f.keys, litBtn: f.litBtn, litKeys: f.litKeys, enc: f.enc };
  if (f.fb) { rec.fb = fbCount++; fbs.push(Buffer.from(f.fb.buffer)); }
  frames.push(rec);
};
const fps = scene.fps ?? 30;
await scene.run(emu, { fps });
emu.stop();

writeFileSync(resolve(out, "audio.wav"), wavBytes(emu.audio()));
writeFileSync(resolve(out, "fb.bin"), Buffer.concat(fbs));
writeFileSync(resolve(out, "trace.json"), JSON.stringify({ fps, width: 240, height: 240, frames, events: emu.events, fbCount }));
const h = createHash("sha1").update(Buffer.from(emu.audio().buffer)).digest("hex").slice(0, 12);
console.log(`${frames.length} frames (${(frames.length / fps).toFixed(1)} s), ${fbCount} screens, audio sha1 ${h} -> ${out}`);
