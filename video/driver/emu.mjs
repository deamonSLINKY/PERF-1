// SPDX-License-Identifier: GPL-3.0-only
// The FM-1 firmware (build/emu/amb.wasm: the whole app, as in the web emulator) run offline in Node on a virtual clock:
// the device advances only when audio is rendered, so a script gives the same audio and the same screens every run.
// Time is in samples (44.1 kHz); a script says milliseconds. Inputs take effect on the device's next millisecond.
import { readFileSync } from "node:fs";

export const SR = 44100;
export const BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, "OCT-": 12, "OCT+": 13 };
export const ENC = { SELECT: 0, ALGORITHM: 1, PRESETS: 2, KNOB1: 3, KNOB2: 4, KNOB3: 5, KNOB4: 6 };
const WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
export const keyOf = (s) => (typeof s === "number" ? s : s[0] === "w" ? WHITE_K[+s.slice(1) & 15] : s[0] === "b" ? BLACK_K[+s.slice(1) % 11] : +s);

export class Emu {
  constructor(ex) {
    this.ex = ex;
    this.mem = ex.memory;
    this.samples = 0;                 // rendered so far
    this.btn = 0;
    this.keys = 0;
    this.chunks = [];                 // stereo audio, Float32Array pairs
    this.recording = false;
    this.onframe = null;
    this.fps = 30;
    this.nextFrame = 0;               // sample at which the next video frame is taken
    this.frameNo = 0;
    this.encAcc = {};                 // knob detents since the last video frame
    this.events = [];                 // marks the script leaves for the video: {t (s, from the start of recording), kind, ...}
    this.t0 = 0;                      // sample at which recording started
  }

  static async create({ wasmPath, seed = 0, master = 2800 }) {
    let emu = null;
    const wasi = {
      clock_time_get: (id, prec, out) => { new DataView(emu.mem.buffer).setBigUint64(out, BigInt(Math.round((emu.samples / SR) * 1e9)), true); return 0; },
      fd_write: (fd, iov, n, out) => { new DataView(emu.mem.buffer).setUint32(out, 0, true); return 0; },
      proc_exit: () => {},
    };
    const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
    const env = new Proxy({}, { get: () => () => 0 });
    const { instance } = await WebAssembly.instantiate(readFileSync(wasmPath), { wasi_snapshot_preview1: stub, env });
    const ex = instance.exports;
    emu = new Emu(ex);
    emu.opts = { wasmPath, seed, master };
    if (ex._initialize) ex._initialize();
    ex.web_master(master);
    if (ex.web_seed) ex.web_seed(seed >>> 0);       // 0: the first piece, exact
    ex.web_boot();
    return emu;
  }

  // ---- the clock
  advance(n) {                         // n samples of device time
    const ex = this.ex;
    while (n > 0) {
      const k = Math.min(n, 1024);
      ex.web_render(k);
      if (this.recording) {
        const c = new Float32Array(2 * k), l = new Float32Array(this.mem.buffer, ex.web_out_l(), k), r = new Float32Array(this.mem.buffer, ex.web_out_r(), k);
        for (let i = 0; i < k; i++) { c[2 * i] = l[i]; c[2 * i + 1] = r[i]; }
        this.chunks.push(c);
      }
      this.samples += k;
      n -= k;
    }
  }
  // run for ms, taking a video frame every 1/fps while recording (frames land on exact samples)
  async wait(ms) {
    let left = Math.round((ms * SR) / 1000);
    while (left > 0) {
      let step = left;
      if (this.recording) step = Math.min(left, Math.max(1, Math.ceil(this.nextFrame - (this.samples - this.t0))));
      this.advance(step);
      left -= step;
      if (this.recording && this.samples - this.t0 >= this.nextFrame) this.takeFrame();
    }
  }
  get time() { return (this.samples - this.t0) / SR; }
  clone() { return Emu.create(this.opts); }                       // another device with the same start (to look ahead: it is deterministic)
  litButton(name) { return (this.ex.web_lit_buttons() >>> BTN[name]) & 1; }
  // run until a button's light is on (SAVE lights through a breakdown and a build); ms of device time at most. Returns the device's time (ms) when it came on, or -1
  async runUntilLit(name, maxMs, stepMs = 50) {
    const t = this.samples;
    for (let ms = 0; ms < maxMs; ms += stepMs) { if (this.litButton(name)) return (this.samples - t) / SR * 1000 + 0; this.advance(Math.round((stepMs * SR) / 1000)); }
    return -1;
  }
  // run for ms without recording (a cut: the video's clock does not move, the device's does)
  skip(ms) {
    const n = Math.round((ms * SR) / 1000), was = this.recording;
    this.recording = false;
    this.advance(n);
    this.recording = was;
    this.t0 += n;
    this.lastBlits = -1;
  }

  // ---- recording
  start(fps = 30) {
    this.recording = true;
    this.fps = fps;
    this.t0 = this.samples;
    this.nextFrame = 0;
    this.frameNo = 0;
    this.takeFrame();
  }
  takeFrame() {
    const ex = this.ex;
    const blits = ex.web_blits();
    const f = { n: this.frameNo, btn: this.btn, keys: this.keys, litBtn: ex.web_lit_buttons() >>> 0, litKeys: ex.web_lit_keys() >>> 0, enc: this.encAcc };
    if (blits !== this.lastBlits) { this.lastBlits = blits; f.fb = new Uint16Array(this.mem.buffer, ex.web_fb(), 240 * 240).slice(); }
    this.encAcc = {};
    this.frameNo++;
    this.nextFrame = (this.frameNo * SR) / this.fps;
    if (this.onframe) this.onframe(f);
  }
  stop() { this.recording = false; }
  mark(kind, data = {}) { this.events.push({ t: this.time, kind, ...data }); }

  // ---- input
  setButtons() { this.ex.web_buttons(this.btn >>> 0); }
  press(name) { this.btn |= 1 << BTN[name]; this.setButtons(); }
  release(name) { this.btn &= ~(1 << BTN[name]); this.setButtons(); }
  async tap(name, hold = 90) { this.press(name); await this.wait(hold); this.release(name); await this.wait(60); }
  key(k, down) { const b = 1 << keyOf(k); this.keys = down ? this.keys | b : this.keys & ~b; this.ex.web_keys(this.keys >>> 0); }
  async tapKey(k, hold = 120) { this.key(k, true); await this.wait(hold); this.key(k, false); await this.wait(60); }
  spin(role, n) { this.ex.web_enc(ENC[role], n); this.encAcc[role] = (this.encAcc[role] || 0) + n; }
  async turn(role, n, gap = 80) { const d = Math.sign(n); for (let i = 0; i < Math.abs(n); i++) { this.spin(role, d); await this.wait(gap); } }
  master(v) { this.ex.web_master(v); }

  audio() {                            // the recording as one interleaved Float32Array
    let total = 0;
    for (const c of this.chunks) total += c.length;
    const out = new Float32Array(total);
    let o = 0;
    for (const c of this.chunks) { out.set(c, o); o += c.length; }
    return out;
  }
}

export function wavBytes(inter, rate = SR) {   // 16-bit stereo PCM
  const n = inter.length, buf = Buffer.alloc(44 + n * 2);
  buf.write("RIFF", 0); buf.writeUInt32LE(36 + n * 2, 4); buf.write("WAVEfmt ", 8); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22);
  buf.writeUInt32LE(rate, 24); buf.writeUInt32LE(rate * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34); buf.write("data", 36); buf.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(inter[i] * 32767))), 44 + i * 2);
  return buf;
}
