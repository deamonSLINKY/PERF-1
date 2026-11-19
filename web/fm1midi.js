// SPDX-License-Identifier: GPL-3.0-only
// A MIDI file into a PERF-1 piece (the studio, web/studio): the file read (SMF format 0 or 1), each track's part guessed, the key found, and the parts
// turned into what a piece can hold: up to six drum patterns of the six lanes with one bar's step tricks, a bar of bass steps, a bar of acid line, a bar
// of pluck line (each written in scale steps above the chord they play over, as the FM-1 keeps them), and the chords as the progression's eight slots.
// What does not fit is reported, not hidden. Nothing here touches the page, so tests/web_midi_test.js checks it in node.

// the modes' pools, as amb.c EN_MODE_POOL (bit i: the root + i semitones): Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, pentatonic major, minor,
// and (0.17.33) Locrian, harmonic minor, melodic minor, Phrygian dominant, Hungarian minor, double harmonic (the key guess below tries the first eight)
export const MIDI_MODES = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 4, 6, 7, 9, 11], [0, 2, 4, 5, 7, 9, 10],
                           [0, 2, 3, 5, 7, 8, 10], [0, 2, 4, 7, 9], [0, 3, 5, 7, 10], [0, 1, 3, 5, 6, 8, 10], [0, 2, 3, 5, 7, 8, 11],
                           [0, 2, 3, 5, 7, 9, 11], [0, 1, 4, 5, 7, 8, 10], [0, 2, 3, 6, 7, 8, 11], [0, 1, 4, 5, 7, 8, 11]];
export const midiPool = (mode) => MIDI_MODES[mode].reduce((a, s) => a | (1 << s), 0);
export const MIDI_ROLES = ["skip", "drums", "bass", "acid", "pluck", "chords"];

// ---- the file: tracks of notes (start and length in ticks, key, velocity, channel), the first tempo and time signature
export function midiParse(buf) {
  const b = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  const str = (o) => String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]);
  const u32 = (o) => ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0, u16 = (o) => (b[o] << 8) | b[o + 1];
  if (b.length < 14 || str(0) !== "MThd") throw new Error("not a MIDI file");
  const ntrk = u16(10), div = u16(12);
  if (!div) throw new Error("a MIDI file with no ticks a beat in its header");
  if (div & 0x8000) throw new Error("a MIDI file timed in SMPTE frames: save it with beats (PPQ) instead");
  const out = { format: u16(8), ppq: div, bpm: 120, sig: [4, 4], tracks: [] };
  let p = 8 + u32(4), seenTempo = false, seenSig = false;
  for (let t = 0; t < ntrk && p + 8 <= b.length; t++) {
    if (str(p) !== "MTrk") { p += 8 + u32(p + 4); t--; if (p >= b.length) break; continue; }
    const end = Math.min(b.length, p + 8 + u32(p + 4));
    let q = p + 8, tick = 0, run = 0;
    const tr = { name: "", notes: [], chans: new Set() }, open = new Map();
    const vlq = () => { let v = 0, c; do { c = b[q++]; v = (v << 7) | (c & 127); } while (c & 128 && q < end); return v; };
    while (q < end) {
      tick += vlq();
      let st = b[q];
      if (st & 0x80) q++; else st = run;
      if (st === 0xFF) {
        const type = b[q++], len = vlq(), d = b.subarray(q, q + len);
        if (type === 0x03 && !tr.name) tr.name = String.fromCharCode(...d).trim();
        if (type === 0x51 && len === 3 && !seenTempo) { out.bpm = 60000000 / ((d[0] << 16) | (d[1] << 8) | d[2]); seenTempo = true; }
        if (type === 0x58 && len >= 2 && !seenSig) { out.sig = [d[0], 1 << d[1]]; seenSig = true; }
        q += len;
        continue;
      }
      if (st === 0xF0 || st === 0xF7) { q += vlq(); continue; }
      run = st;
      const kind = st & 0xF0, ch = st & 15, d1 = b[q], d2 = kind === 0xC0 || kind === 0xD0 ? 0 : b[q + 1];
      q += kind === 0xC0 || kind === 0xD0 ? 1 : 2;
      if (kind === 0x90 && d2 > 0) {
        const k = ch * 128 + d1;
        if (!open.has(k)) open.set(k, []);
        open.get(k).push({ t: tick, key: d1, vel: d2, ch });
      } else if (kind === 0x80 || (kind === 0x90 && d2 === 0)) {
        const s = open.get(ch * 128 + d1);
        if (s && s.length) { const n = s.shift(); n.dur = Math.max(1, tick - n.t); tr.notes.push(n); tr.chans.add(ch); }
      }
    }
    for (const s of open.values()) for (const n of s) { n.dur = Math.max(1, tick - n.t); tr.notes.push(n); tr.chans.add(n.ch); }
    tr.notes.sort((x, y) => x.t - y.t || x.key - y.key);
    if (tr.notes.length) out.tracks.push(tr);
    p = end;
  }
  if (out.format === 0 && out.tracks.length === 1 && out.tracks[0].chans.size > 1) {   // format 0: a track a channel
    const all = out.tracks[0], by = new Map();
    for (const n of all.notes) { if (!by.has(n.ch)) by.set(n.ch, { name: (all.name ? all.name + " " : "") + "ch " + (n.ch + 1), notes: [], chans: new Set([n.ch]) }); by.get(n.ch).notes.push(n); }
    out.tracks = [...by.values()];
  }
  let last = 0;
  for (const tr of out.tracks) for (const n of tr.notes) if (n.t + n.dur > last) last = n.t + n.dur;
  out.stepTicks = out.ppq / 4;
  out.bars = Math.max(1, Math.ceil(last / (out.ppq * 4)));
  return out;
}

// a track's facts, and the part it most likely is
export function midiGuess(tr) {
  const n = tr.notes.length, keys = tr.notes.map((x) => x.key);
  const lo = keys.reduce((a, k) => Math.min(a, k), 127), hi = keys.reduce((a, k) => Math.max(a, k), 0), avg = keys.reduce((a, k) => a + k, 0) / n;
  let poly = 0;                                               // notes sounding with another
  tr.notes.forEach((x, i) => { const y = tr.notes[i + 1]; if (y && y.t < x.t + x.dur * 0.5) poly++; });
  const name = (tr.name || "").toLowerCase(), polyShare = poly / n;
  let role;
  if (tr.chans.has(9) || /drum|kit|beat|perc/.test(name)) role = "drums";
  else if (/bass|sub/.test(name)) role = "bass";
  else if (/acid|303|lead|arp/.test(name)) role = "acid";
  else if (/chord|pad|key|piano|stab|string/.test(name) || polyShare > 0.4) role = "chords";
  else if (avg < 52) role = "bass";
  else if (/pluck|bell|melod/.test(name) || avg >= 64) role = "pluck";
  else role = "acid";
  return { role, n, lo, hi, avg, poly: polyShare };
}

// the key: the root and mode whose scale holds the most of the notes (by length), the root weighted
export function midiKey(file, skip) {
  const w = new Array(12).fill(0), low = new Array(12).fill(0);
  file.tracks.forEach((tr, i) => {
    if (skip && skip(i)) return;
    for (const n of tr.notes) { w[n.key % 12] += n.dur; if (n.key < 55) low[n.key % 12] += n.dur; }
  });
  const total = w.reduce((a, x) => a + x, 0) || 1;
  let first = null;                                           // the lowest note on the first downbeat: most loops start on their home chord
  file.tracks.forEach((tr, i) => {
    if (skip && skip(i)) return;
    for (const n of tr.notes) if (!first || n.t < first.t - file.ppq / 8 || (Math.abs(n.t - first.t) <= file.ppq / 8 && n.key < first.key)) first = n;
  });
  let best = { root: 9, mode: 5, score: -Infinity };
  for (const mode of [5, 0, 1, 4, 2, 3, 7, 6])                // (ties: the common modes first)
    for (let root = 0; root < 12; root++) {
      let inS = 0;
      for (const s of MIDI_MODES[mode]) inS += w[(root + s) % 12];
      const score = (2 * inS - total) / total + 0.35 * (w[root] + low[root]) / total + 0.15 * (w[(root + 7) % 12]) / total +
                    (first && first.key % 12 === root ? 0.3 : 0);
      if (score > best.score + 1e-9) best = { root, mode, score };
    }
  return best;
}

// the parts as a piece holds them. opts: roles (one a track, MIDI_ROLES), bar (the first bar to read, 0 ..), line (the bar the lines come from;
// -1: the busiest of the first eight), root, mode. The answer: what to write (each part only when a track gave it) and a report.
export function midiToPiece(file, opts) {
  const report = [], ST = file.stepTicks, BAR = ST * 16, N = MIDI_MODES[opts.mode].length, scl = MIDI_MODES[opts.mode], root = opts.root;
  const tracksOf = (role) => file.tracks.filter((_, i) => opts.roles[i] === role);
  const notesOf = (role) => tracksOf(role).flatMap((t) => t.notes).sort((a, b) => a.t - b.t || a.key - b.key);
  const b0 = opts.bar || 0, nbars = Math.max(0, file.bars - b0);
  if (file.sig[0] !== 4 || file.sig[1] !== 4) report.push("The file is in " + file.sig[0] + "/" + file.sig[1] + ": read as bars of four beats.");
  // a key as a scale index (up from the root's lowest octave), snapped to the scale when it is not in it
  let snapped = 0;
  const idxOf = (key, quiet) => {
    const rel = key - root, oct = Math.floor(rel / 12), pc = ((rel % 12) + 12) % 12;
    let best = 0, bd = 99;
    scl.forEach((s, i) => { const d = Math.abs(s - pc); if (d < bd) { bd = d; best = i; } });
    if (pc >= scl[N - 1] + (12 - scl[N - 1]) / 2) { if (12 - pc < bd) { bd = 12 - pc; best = N; } }   // nearer the next root up
    if (bd && !quiet) snapped++;
    return oct * N + best;
  };
  const out = { report };

  // ---- the chords: a degree a bar (the chord tracks', else every pitched part's), repeats folded, as slots of 1, 2, 4, 8 bars
  const chordNotes = notesOf("chords").length ? notesOf("chords") : file.tracks.flatMap((t, i) => (opts.roles[i] === "drums" || opts.roles[i] === "skip" ? [] : t.notes));
  const degs = [];
  const degreeIn = (from, to) => {                          // the scale degree whose triad the notes in [from, to) weigh most on (-1: none)
    const w = new Array(N).fill(0);
    let any = false;
    for (const n of chordNotes) {
      const a = Math.max(from, n.t), z = Math.min(to, n.t + n.dur);
      if (z <= a) continue;
      const i = ((idxOf(n.key, true) % N) + N) % N;
      w[i] += (z - a) * (n.t <= from + ST ? 1.5 : 1); any = true;
    }
    if (!any) return -1;
    let best = 0, bs = -1;
    for (let d = 0; d < N; d++) { const s = w[d] * 1.2 + w[(d + 2) % N] * 0.8 + w[(d + 4) % N] * 0.7; if (s > bs) { bs = s; best = d; } }
    return best;
  };
  const sevenths = (from, to, d) => {                        // the seventh is there (a seventh chord), or the third is not and the fourth is (sus)
    const w = new Array(N).fill(0);
    for (const n of chordNotes) { const a = Math.max(from, n.t), z = Math.min(to, n.t + n.dur); if (z > a) w[((idxOf(n.key, true) % N) + N) % N] += z - a; }
    const tot = w.reduce((x, y) => x + y, 0) || 1;
    if (N === 7 && w[(d + 6) % 7] / tot > 0.12) return 2;
    if (N === 7 && w[(d + 2) % 7] / tot < 0.04 && w[(d + 3) % 7] / tot > 0.12) return 4;
    return 1;
  };
  const MAXB = Math.min(nbars, 64);
  for (let b = 0; b < MAXB; b++) degs.push(degreeIn((b0 + b) * BAR, (b0 + b + 1) * BAR));
  for (let b = 0; b < degs.length; b++) if (degs[b] < 0) degs[b] = b ? degs[b - 1] : 0;   // an empty bar: the chord before goes on
  let period = degs.length;
  for (const p of [1, 2, 4, 8, 16, 32]) if (p < degs.length && degs.every((d, i) => d === degs[i % p])) { period = p; break; }
  const chordAt = (b) => (degs.length ? degs[b % Math.max(1, period)] : 0);
  if (tracksOf("chords").length || notesOf("bass").length) {
    const seq = degs.slice(0, period), slots = [];
    let i = 0, covered = 0;
    while (i < seq.length && slots.length < 8) {
      let j = i;
      while (j < seq.length && seq[j] === seq[i]) j++;
      let left = j - i, first = true;
      for (const c of [8, 4, 2, 1]) while (left >= c && slots.length < 8) {
        slots.push({ deg: first ? seq[i] % 7 + 1 : 0, bars: c, kind: first ? sevenths((b0 + i) * BAR, (b0 + j) * BAR, seq[i]) : 0 });
        first = false; left -= c; covered += c;
      }
      i = j;
    }
    out.prog = slots;
    report.push("Chords: " + slots.filter((s) => s.deg).length + " in " + slots.length + " slots, " + covered + " bars" + (period < degs.length ? " (the file's chords repeat every " + period + " bars)" : "") +
                (covered < period ? "; " + (period - covered) + " more bars do not fit the eight slots" : "") + ".");
  }

  // ---- a line's bar: the one asked for, else the busiest of the first eight
  const lineBar = (notes) => {
    if (opts.line >= 0) return opts.line;
    let best = 0, bn = -1;
    for (let b = 0; b < Math.min(8, nbars); b++) { const c = notes.filter((n) => n.t >= (b0 + b) * BAR && n.t < (b0 + b + 1) * BAR).length; if (c > bn) { bn = c; best = b; } }
    return best;
  };
  // one note a step (the lowest for the bass, the highest otherwise); notes that land on a taken step, or between steps, are counted
  const monoBar = (notes, b, low) => {
    const steps = new Array(16).fill(null), from = (b0 + b) * BAR;
    let dropped = 0;
    for (const n of notes) {
      if (n.t < from - ST / 2 || n.t >= from + BAR - ST / 2) continue;
      const s = Math.round((n.t - from) / ST);
      if (s < 0 || s > 15) continue;
      if (steps[s]) { dropped++; if (low ? n.key < steps[s].key : n.key > steps[s].key) steps[s] = n; } else steps[s] = n;
    }
    return { steps, dropped, from };
  };
  const velLoud = (notes) => { const v = notes.map((n) => n.vel).sort((a, c) => a - c); return Math.max(96, v[Math.floor(v.length * 0.75)] || 100); };
  const shiftFor = (ds, lo, hi) => {                         // the octave shift (a multiple of N) that puts the most scale steps in [lo, hi], the lowest first
    let best = 0, bc = -1, bm = Infinity;
    for (let k = -12; k <= 12; k++) {
      const c = ds.filter((d) => d - k * N >= lo && d - k * N <= hi).length, m = Math.abs(Math.min(...ds) - k * N - lo);
      if (c > bc || (c === bc && m < bm)) { bc = c; best = k; bm = m; }
    }
    return best * N;
  };
  const legato = (a, b) => a && b && a.t + a.dur >= b.t - file.stepTicks * 0.25;
  const lineFrom = (role, low) => {
    const notes = notesOf(role);
    if (!notes.length) return null;
    const b = lineBar(notes), m = monoBar(notes, b, low), ri = chordAt(b);
    const ds = m.steps.map((n) => (n ? idxOf(n.key) - ri : null)), loud = velLoud(notes);
    return { b, m, ri, ds, loud, n: m.steps.filter((x) => x).length };
  };
  let fold = 0;
  const L = lineFrom("bass", true);
  if (L) {                                                   // bass: a scale step 0 .. 7 above the root, an octave down, as written, up
    const B = shiftFor(L.ds.filter((d) => d !== null), 0, 7) - N;   // (most notes as written: oct 1)
    out.bass = L.m.steps.map((n, s) => {
      if (!n) return 0;
      let v = L.ds[s] - B, oct = Math.floor(v / N);
      if (oct < 0 || oct > 2 || v - oct * N > 7) { fold++; oct = Math.max(0, Math.min(2, oct)); }
      const note = Math.max(0, Math.min(7, v - oct * N)), acc = n.vel >= L.loud, glide = s > 0 && legato(L.m.steps[s - 1], n);
      return (acc ? (glide ? 4 : 2) : glide ? 3 : 1) | note << 3 | oct << 6;
    });
    report.push("Bass: bar " + (b0 + L.b + 1) + ", " + L.n + " notes" + (L.m.dropped ? ", " + L.m.dropped + " left out (one a step)" : "") + ".");
  }
  const A = lineFrom("acid", false);
  if (A) {                                                   // acid: 0 .. 15 scale steps above the chord, an octave down .. two up
    const B = shiftFor(A.ds.filter((d) => d !== null), 0, 15);
    out.acid = A.m.steps.map((n, s) => {
      if (!n) return { off: 0, oct: 1, gate: 0, acc: 0, slide: 0 };
      let v = A.ds[s] - B, oct = 1;
      while (v < 0 && oct > 0) { v += N; oct--; }
      while (v > 15 && oct < 3) { v -= N; oct++; }
      if (v < 0 || v > 15) { fold++; v = Math.max(0, Math.min(15, v)); }
      return { off: v, oct, gate: 1, acc: n.vel >= A.loud ? 1 : 0, slide: s < 15 && legato(n, A.m.steps[s + 1]) ? 1 : 0 };
    });
    report.push("Acid: bar " + (b0 + A.b + 1) + ", " + A.n + " notes" + (A.m.dropped ? ", " + A.m.dropped + " left out (one a step)" : "") + ".");
  }
  const Pl = lineFrom("pluck", false);
  if (Pl) {                                                  // pluck: the nearest rung of the chord's ladder (its triad's tones from C up, as amb.c cm_ladder counts them)
    const pcs = [0, 2, 4].map((k) => (root + scl[(Pl.ri + k) % N]) % 12).sort((a, c) => a - c);
    const rungOf = (key) => { let best = 0, bd = 99; for (let o = Math.floor(key / 12) - 1; o <= Math.floor(key / 12) + 1; o++) pcs.forEach((pc, i) => { const d = Math.abs(12 * o + pc - key); if (d < bd) { bd = d; best = 3 * o + i; } }); return best; };
    const rungs = Pl.m.steps.map((n) => (n ? rungOf(n.key) : null));
    const live = rungs.filter((r) => r !== null), lo = Math.min(...live), hi = Math.max(...live), sh = -3 * Math.floor(lo / 3) - (hi - 3 * Math.floor(lo / 3) > 11 ? 3 * Math.ceil((hi - 3 * Math.floor(lo / 3) - 11) / 3) : 0);   // the lowest note's octave at rung 0
    out.pluck = Pl.m.steps.map((n, s) => {
      if (!n) return { rung: 0, lvl: 0 };
      let r = rungs[s] + sh;
      if (r < 0 || r > 11) { fold++; r = Math.max(0, Math.min(11, r)); }
      return { rung: r, lvl: n.vel >= Pl.loud ? 3 : n.vel < 60 ? 1 : 2 };
    });
    out.pluckKeys = Pl.m.steps.map((n) => (n ? { key: n.key, lvl: n.vel >= Pl.loud ? 3 : n.vel < 60 ? 1 : 2 } : null));   // (the notes as written: the studio puts them on the piece's own ladder)
    out.pluckRi = Pl.ri;
    report.push("Pluck: bar " + (b0 + Pl.b + 1) + ", " + Pl.n + " notes on the chord's ladder (its tones only: notes between them move to the nearest).");
  }

  // ---- drums: up to six different bars as the patterns, the first one's velocities, flams and pushes as the step tricks
  const dn = notesOf("drums");
  if (dn.length) {
    const laneOf = (k) => (k === 35 || k === 36 ? 0 : k === 38 || k === 40 ? 1 : k === 39 || k === 37 ? 2 : k === 42 || k === 44 || k === 51 || k === 59 || k === 53 ? 3 : k === 46 || k === 49 || k === 57 || k === 55 || k === 52 ? 4 : 5);
    const barMasks = [];
    for (let b = 0; b < Math.min(nbars, 32); b++) {
      const m = [0, 0, 0, 0, 0, 0], from = (b0 + b) * BAR;
      for (const n of dn) { const s = Math.round((n.t - from) / ST); if (s >= 0 && s < 16 && n.t >= from - ST / 2) m[laneOf(n.key)] |= 1 << s; }
      barMasks.push(m);
    }
    const distinct = [];
    for (const m of barMasks) if (m.some((x) => x) && !distinct.some((d) => d.every((x, i) => x === m[i]))) distinct.push(m);
    if (distinct.length) {
      out.pats = [0, 1, 2, 3, 4, 5].map((p) => distinct[p % distinct.length]);
      const mods = Array.from({ length: 6 }, () => new Array(16).fill(0)), first = barMasks.findIndex((m) => m.some((x) => x)), from = (b0 + first) * BAR;
      const per = {};
      for (const n of dn) {
        const s = Math.round((n.t - from) / ST), l = laneOf(n.key);
        if (s < 0 || s > 15 || n.t < from - ST / 2) continue;
        (per[l * 16 + s] = per[l * 16 + s] || []).push(n);
      }
      for (const [k, hits] of Object.entries(per)) {
        const l = Math.floor(k / 16), s = k % 16, v = Math.max(...hits.map((h) => h.vel)), late = hits[0].t - (from + s * ST);
        const lvl = v < 50 ? 1 : v < 85 ? 2 : v >= 115 ? 3 : 0, rat = hits.length >= 4 ? 3 : hits.length === 3 ? 2 : hits.length === 2 ? 1 : 0;
        const push = late > 0 ? Math.max(0, Math.min(7, Math.round(late / (ST / 12)))) : 0;
        mods[l][s] = rat | push << 3 | lvl << 6;
      }
      out.mods = mods;
      report.push("Drums: " + Math.min(6, distinct.length) + " pattern" + (distinct.length > 1 ? "s" : "") + " from " + barMasks.length + " bars" + (distinct.length > 6 ? " (" + (distinct.length - 6) + " more different bars left out)" : "") +
                  "; the step tricks from bar " + (b0 + first + 1) + ".");
    }
  }
  if (snapped) report.push(snapped + " note" + (snapped > 1 ? "s" : "") + " outside the scale moved to the nearest scale step.");
  if (fold) report.push(fold + " line note" + (fold > 1 ? "s" : "") + " past the range a piece keeps moved by octaves.");
  if (L || A || Pl) report.push("The lines are one bar each and follow the chords, as the FM-1 plays them; a longer melody would need a new stored field.");
  out.tempo = Math.round(file.bpm);
  return out;
}
