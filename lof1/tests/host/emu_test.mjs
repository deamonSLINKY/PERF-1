// SPDX-License-Identifier: GPL-3.0-only
// The browser emulator's WebAssembly (web/emu/build_clang.sh) under Node: it boots through the cards,
// plays, draws, takes MIDI notes in (a phrase on the lead), sends MIDI out, and gives the voices apart on
// channels 3-8 (lead, comp, bass, answer, drums, dust).
//   node tests/host/emu_test.mjs build/emu/amb.wasm
import fs from "fs";
const buf = fs.readFileSync(process.argv[2] || "build/emu/amb.wasm");
const stub = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(buf, { wasi_snapshot_preview1: stub, env: stub });
const ex = instance.exports;
if (ex._initialize) ex._initialize();
let fails = 0;
const check = (ok, what) => { if (!ok) { console.log("FAIL " + what); fails++; } };
ex.web_master(2800);
ex.web_boot();
ex.web_split(1);
const ms = new Array(8).fill(0);
let n = 0, frames = 0, tick = 0, nan = 0;
ex.web_midi_in(0xFA, 0, 0);
for (let b = 0; b < 44100 * 25 / 128; b++) {
  ex.web_render(128);
  frames += 128;
  while (frames >= tick * 44100 * 60 / (120 * 24)) { ex.web_midi_in(0xF8, 0, 0); tick++; }
  const t = Math.floor(frames / 4410);                // a phrase on the lead, 4 s in: D F A C, three tenths of a second apart
  const ph = [[40, 62], [43, 65], [46, 69], [49, 72]];
  for (const [at, note] of ph) {
    if (t === at && frames % 4410 < 128) ex.web_midi_in(0x90, note, 100);
    if (t === at + 2 && frames % 4410 < 128) ex.web_midi_in(0x80, note, 0);
  }
  for (let c = 0; c < 8; c++) {
    const a = new Float32Array(ex.memory.buffer, ex.web_out(c), 128);
    for (const x of a) { if (Number.isNaN(x)) nan++; ms[c] += x * x; }
  }
  n += 128;
}
const db = ms.map((s) => 10 * Math.log10(s / n + 1e-20));
console.log("  channels (dB):", db.map((d) => d.toFixed(1)).join(" "));
check(nan === 0, "NaN in the output");
check(db[0] > -40 && db[1] > -40, "the mix is heard");
for (const c of [2, 3, 4, 6, 7]) check(db[c] > -60, `channel ${c + 1} carries its voice`);
check(ex.web_blits() > 100, "the screen is drawn");
const w = ex.web_midi_written(), ring = new Uint32Array(ex.memory.buffer, ex.web_midi_ring(), 1024);
let notes = 0;
for (let i = Math.max(0, w - 1024); i < w; i++) if (((ring[i % 1024] >>> 8) & 0xF0) === 0x90) notes++;
check(notes > 5, `MIDI out: ${notes} notes`);
const fb = new Uint16Array(ex.memory.buffer, ex.web_fb(), 240 * 240);
let distinct = new Set(fb).size;
check(distinct > 40, `the screen has ${distinct} colours`);
console.log(fails ? `emu: ${fails} FAILED` : "emu: all ok");
process.exit(fails ? 1 : 0);
