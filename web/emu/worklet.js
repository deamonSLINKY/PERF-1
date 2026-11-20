// SPDX-License-Identifier: GPL-3.0-only
// AMB-1 in an AudioWorklet: amb.wasm is the whole FM-1 app (web/emu/amb_web.c). Each render
// quantum asks it for 128 frames, which runs the device's clock forward; between quanta the page's
// input goes in and, about 30 times a second, the screen, the lights and any saved objects go out.

const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();

class Amb extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.input = null;
    this.sentBlits = -1;
    this.sentWrites = -1;
    this.lastFrame = 0;
    this.busy = 0;
    this.frames = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    try { await this.handle(m); }
    catch (err) {                                     // (a failed load says so, a failed request answers: the page never waits for ever)
      if (m && m.type === "load") this.port.postMessage({ type: "ready", error: "the emulator would not load: " + (err && err.message || err) });
      else if (m && m.id) this.port.postMessage({ type: "st", id: m.id, ok: 0, error: String(err && err.message || err) });
    }
  }

  stHere(m) {                                         // the piece a studio edit names is the one playing (web_st_live: 113 the genre, 114 the piece)
    const b = new Uint8Array(this.mem.buffer, this.ex.web_st_live(), 141);
    return b[113] === (m.g | 0) && b[114] === ((m.type === "st_par" ? m.pc : m.p) | 0);
  }

  async handle(m) {
    if (m.type === "load") {
      const now = () => BigInt(Math.round(currentTime * 1e9));
      const wasi = {
        clock_time_get: (id, prec, out) => {
          new DataView(this.mem.buffer).setBigUint64(out, now(), true);
          return 0;
        },
        fd_write: (fd, iov, n, out) => { new DataView(this.mem.buffer).setUint32(out, 0, true); return 0; },
        proc_exit: () => {},
      };
      const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
      const env = new Proxy({}, { get: () => () => 0 });
      const { instance } = await WebAssembly.instantiate(m.wasm, { wasi_snapshot_preview1: stub, env });
      this.ex = instance.exports;
      this.mem = this.ex.memory;
      if (this.ex._initialize) this.ex._initialize();
      const max = this.ex.web_store_max();
      for (const [obj, bytes] of Object.entries(m.store || {})) {
        const b = new Uint8Array(bytes).subarray(0, max);
        new Uint8Array(this.mem.buffer, this.ex.web_store(+obj), b.length).set(b);
        this.ex.web_store_set_len(+obj, b.length);
      }
      this.sentWrites = this.ex.web_store_writes();
      this.ex.web_master(m.master ?? 2800);
      if (this.ex.web_seed) this.ex.web_seed(m.seed >>> 0);   // a different start each time
      this.studio = !!m.studio && !!this.ex.web_st_live;      // the studio (web/studio): its live view, a short boot card
      if (this.studio) this.ex.web_st_boot(); else this.ex.web_boot();
      this.port.postMessage({ type: "ready", studio: this.studio });
    } else if (!this.ex) {
      return;
    } else if (m.type === "buttons") {
      this.ex.web_buttons(m.mask >>> 0);
    } else if (m.type === "keys") {
      this.ex.web_keys(m.mask >>> 0);
    } else if (m.type === "enc") {
      this.ex.web_enc(m.role, m.n | 0);
    } else if (m.type === "master") {
      this.ex.web_master(m.value | 0);
    } else if (this.studio && m.g !== undefined && (m.type === "st_put" || m.type === "st_par" || m.type === "st_save") && this.ex.web_st_live && !this.stHere(m)) {
      this.port.postMessage({ type: "st", id: m.id, ok: 0, moved: true });   // (the song moved on before it came: not into the next piece)
    } else if (this.studio && m.type === "st_note" && this.ex.web_st_note) {
      this.ex.web_st_note(m.voice | 0, m.note | 0, m.vel | 0);    // a note heard (no answer)
    } else if (this.studio && m.type === "st_save" && this.ex.web_st_save) {
      this.port.postMessage({ type: "st", id: m.id, ok: this.ex.web_st_save() });
    } else if (this.studio && m.type === "st_set" && this.ex.web_st_set) {
      const n = this.ex.web_st_sl_len ? this.ex.web_st_sl_len() : 17, sl = new Uint8Array(this.mem.buffer, this.ex.web_st_sl(), n);   // the set list: places, Play, bars, parts, end (written when m.sl comes)
      if (m.sl) sl.set(new Uint8Array(m.sl).subarray(0, n));
      this.ex.web_st_set(m.sl ? 1 : 0);
      this.port.postMessage({ type: "st", id: m.id, ok: 1, sl: new Uint8Array(this.mem.buffer, this.ex.web_st_sl(), n).slice() });
    } else if (this.studio && m.type === "st_roll" && this.ex.web_st_roll) {
      this.ex.web_st_roll();                                  // a new take: the engine's own parts anew, the piece as it is
      this.port.postMessage({ type: "st", id: m.id, ok: 1 });
    } else if (this.studio && (m.type === "st_code" || m.type === "st_code_now") && this.ex.web_st_txt) {
      // a track code started (its text), or the playing track's code asked for; the answer: the result, the code, and the piece's bytes
      const txt = new Uint8Array(this.mem.buffer, this.ex.web_st_txt(), 16);
      let ok;
      if (m.type === "st_code") { txt.fill(0); String(m.text || "").slice(0, 15).split("").forEach((c, i) => { txt[i] = c.charCodeAt(0) & 127; }); /* (no TextEncoder in a worklet) */ ok = this.ex.web_st_code_play(); }
      else ok = this.ex.web_st_code_now();
      const s = String.fromCharCode(...txt.slice(0, txt.indexOf(0) < 0 ? 15 : txt.indexOf(0)));
      this.port.postMessage({ type: "st", id: m.id, ok, code: s, io: new Uint8Array(this.mem.buffer, this.ex.web_st_io(), this.ex.web_st_io_len()).slice() });
    } else if (this.studio && m.type === "st_vox" && this.ex.web_st_vox) {
      // the DX7 voice editor: a slot read ("get"), a voice put in a slot ("put"), a built-in DX sound read ("dx"); the answer: the result, how many
      // slots are loaded, the bytes
      const vb = () => new Uint8Array(this.mem.buffer, this.ex.web_st_vox(), 160);
      let ok = 0;
      if (m.op === "get") ok = this.ex.web_st_vox_get(m.slot | 0);
      else if (m.op === "dx") ok = this.ex.web_st_vox_dx(m.idx | 0);
      else if (m.op === "put") { vb().set(new Uint8Array(m.vox).subarray(0, 128)); ok = this.ex.web_st_vox_put(m.slot | 0, m.keep ? 1 : 0); }
      this.port.postMessage({ type: "st", id: m.id, ok, n: this.ex.web_st_vox_n(), vox: vb().slice() });
    } else if (this.studio && m.type === "st_par" && this.ex.web_st_par) {
      this.port.postMessage({ type: "st", id: m.id, ok: 1, v: this.ex.web_st_par(m.p | 0, m.v | 0) });
    } else if (this.studio && m.type === "st_fx" && this.ex.web_st_fx_lock) {
      // the FX view: the effects latched (set when m.lo/m.hi come), a page's setting set (m.page, m.i, m.v), and (m.all) the menu's keys, the names
      // and every page; the answer: the latched mask and every page's settings as they are now
      const ex = this.ex, str = (ptr) => { const b = new Uint8Array(this.mem.buffer, ptr, 64); let s = ""; for (let i = 0; i < 64 && b[i]; i++) s += String.fromCharCode(b[i]); return s; };
      if (m.page !== undefined) ex.web_st_fxp_set(m.page | 0, m.i | 0, m.v | 0);
      const lo = ex.web_st_fx_lock(m.lo !== undefined ? 1 : 0, m.lo | 0, m.hi | 0) >>> 0, hi = ex.web_st_fx_lock_hi() >>> 0, pages = [];
      const desc = (p) => {
        const c = [];
        for (let i = 0; i < 4; i++) {
          const kind = ex.web_st_fxp_num(p, i, 2);
          c.push(kind < 0 ? null : { label: str(ex.web_st_fxp_str(p, i, 1)), kind, lo: ex.web_st_fxp_num(p, i, 3), hi: ex.web_st_fxp_num(p, i, 4), v: ex.web_st_fxp_num(p, i, 5), t: str(ex.web_st_fxp_str(p, i, 2)) });
        }
        return { name: str(ex.web_st_fxp_str(p, 0, 0)), bit: ex.web_st_fxp_num(p, 0, 0), hold: ex.web_st_fxp_num(p, 0, 1), c };
      };
      if (m.light) {                                    // (light: the page set, or the mask alone: little work on this thread while it plays)
        const r = { type: "st", id: m.id, ok: 1, lo, hi };
        if (m.page !== undefined) r.one = { page: m.page | 0, desc: desc(m.page | 0) };
        this.port.postMessage(r);
        return;
      }
      for (let p = 0; p < ex.web_st_fxp_n(); p++) {
        const c = [];
        for (let i = 0; i < 4; i++) {
          const kind = ex.web_st_fxp_num(p, i, 2);
          c.push(kind < 0 ? null : { label: str(ex.web_st_fxp_str(p, i, 1)), kind, lo: ex.web_st_fxp_num(p, i, 3), hi: ex.web_st_fxp_num(p, i, 4), v: ex.web_st_fxp_num(p, i, 5), t: str(ex.web_st_fxp_str(p, i, 2)) });
        }
        pages.push({ name: str(ex.web_st_fxp_str(p, 0, 0)), bit: ex.web_st_fxp_num(p, 0, 0), hold: ex.web_st_fxp_num(p, 0, 1), c });
      }
      const r = { type: "st", id: m.id, ok: 1, lo, hi, pages };
      if (m.all) {
        r.keys = [0, 1].map((b) => Array.from({ length: ex.web_st_fx_nkeys() }, (_, k) => ex.web_st_fx_key(b, k)));
        r.names = Array.from({ length: 64 }, (_, b) => str(ex.web_st_fx_name(b)));
        r.pageOf = Array.from({ length: 64 }, (_, b) => ex.web_st_fx_page(b));
      }
      this.port.postMessage(r);
    } else if (this.studio && m.type === "st_clock") {            // (no answer: 24 a beat while the FM-1 plays)
      if (this.ex.web_st_clock) this.ex.web_st_clock(m.a | 0);
    } else if (this.studio && m.type === "st_trk") {
      this.port.postMessage({ type: "st", id: m.id, ok: this.ex.web_st_trk ? this.ex.web_st_trk(m.mute | 0, m.solo | 0) : 0 });
    } else if (this.studio && (m.type === "st_open" || m.type === "st_go" || m.type === "st_put" || m.type === "st_read")) {
      // the studio: a piece taken up (genre, piece), an edited one put into play (its bytes), or the playing one read out; the answer is its bytes
      const io = () => new Uint8Array(this.mem.buffer, this.ex.web_st_io(), this.ex.web_st_io_len());
      let ok;
      if (m.type === "st_open") ok = this.ex.web_st_open(m.genre | 0, m.piece | 0);
      else if (m.type === "st_go") ok = this.ex.web_st_go ? this.ex.web_st_go(m.place | 0) : 0;
      else if (m.type === "st_put") { io().set(new Uint8Array(m.io).subarray(0, this.ex.web_st_io_len())); ok = this.ex.web_st_put(); }
      else ok = this.ex.web_st_read();
      this.port.postMessage({ type: "st", id: m.id, ok, io: io().slice() });
    } else if (this.studio && m.id && typeof m.type === "string" && m.type.startsWith("st")) {
      this.port.postMessage({ type: "st", id: m.id, ok: 0, error: "not in this emulator build: " + m.type });
    }
  }

  publish() {
    const ex = this.ex;
    const blits = ex.web_blits();
    const msg = { type: "frame", buttons: ex.web_lit_buttons() >>> 0, keys: ex.web_lit_keys() >>> 0 };
    if (ex.web_bg) msg.bg = Array.from(new Float32Array(this.mem.buffer, ex.web_bg(), 12));   // for the stream view's background
    if (this.studio) msg.live = new Uint8Array(this.mem.buffer, ex.web_st_live(), 141).slice();   // the studio's: what plays now
    if (this.frames >= 44100) {                       // the share of real time spent running the device
      msg.load = Math.round(100 * this.busy / (this.frames / 44.1));
      this.busy = this.frames = 0;
    }
    const transfer = [];
    if (blits !== this.sentBlits) {
      this.sentBlits = blits;
      msg.fb = new Uint16Array(this.mem.buffer, ex.web_fb(), 240 * 240).slice();
      transfer.push(msg.fb.buffer);
    }
    const writes = ex.web_store_writes();
    if (writes !== this.sentWrites) {
      this.sentWrites = writes;
      msg.store = {};
      for (let o = 0; o < ex.web_nobj(); o++) {
        const n = ex.web_store_len(o);
        if (n >= 0) msg.store[o] = new Uint8Array(this.mem.buffer, ex.web_store(o), n).slice();
      }
    }
    this.port.postMessage(msg, transfer);
  }

  process(inputs, outputs) {
    if (!this.ex) return true;
    const out = outputs[0];
    const n = out[0].length;
    const t0 = clock();
    this.ex.web_render(n);
    this.busy += clock() - t0;
    this.frames += n;
    out[0].set(new Float32Array(this.mem.buffer, this.ex.web_out_l(), n));
    if (out[1]) out[1].set(new Float32Array(this.mem.buffer, this.ex.web_out_r(), n));
    if (currentTime - this.lastFrame >= 1 / 30) {
      this.lastFrame = currentTime;
      this.publish();
    }
    return true;
  }
}

registerProcessor("amb", Amb);
