// SPDX-License-Identifier: GPL-3.0-only
// A MIDI file into a piece (web/fm1midi.js): a file written here (A minor, 124 BPM: drums on channel 10, a chord track of i VI III VII a bar each,
// a bass line, an acid line with a slide, a pluck melody) is read back, its parts guessed, its key found, and turned into the piece's drum patterns,
// step tricks, bass steps, acid and pluck lines and progression slots, each checked against what the FM-1 would play.
const fs = require("fs");
const path = require("path");
const src = fs.readFileSync(path.join(__dirname, "..", "web", "fm1midi.js"), "utf8").replace(/^export /gm, "");
const M = new Function(src + "; return { MIDI_MODES, midiPool, midiParse, midiGuess, midiKey, midiToPiece };")();
let bad = 0;
const check = (name, ok) => { console.log((ok ? "  ok   " : "  FAIL ") + name); if (!ok) bad++; };

// ---- a small SMF writer: format 1, 96 ticks a beat
const PPQ = 96, ST = PPQ / 4, BAR = ST * 16;
const vlq = (v) => { const o = [v & 127]; while ((v >>= 7)) o.unshift((v & 127) | 128); return o; };
function track(name, notes, meta) {                          // notes: [tick, len, key, vel, ch]
  const ev = [];
  if (name) ev.push([0, [0xFF, 0x03, name.length, ...Buffer.from(name)]]);
  for (const m of meta || []) ev.push([0, m]);
  for (const [t, len, key, vel, ch] of notes) { ev.push([t, [0x90 | ch, key, vel]]); ev.push([t + len, [0x80 | ch, key, 0]]); }
  ev.sort((a, b) => a[0] - b[0] || (a[1][0] & 0xF0) - (b[1][0] & 0xF0));
  const body = [];
  let last = 0;
  for (const [t, d] of ev) { body.push(...vlq(t - last), ...d); last = t; }
  body.push(0, 0xFF, 0x2F, 0);
  return [0x4D, 0x54, 0x72, 0x6B, (body.length >>> 24) & 255, (body.length >> 16) & 255, (body.length >> 8) & 255, body.length & 255, ...body];
}
function smf(tracks) { return Uint8Array.from([0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0, tracks.length, 0, PPQ, ...tracks.flat()]); }

const A1 = 33, NOTES = { A: 9, B: 11, C: 0, D: 2, E: 4, F: 5, G: 7 };
const tempo = Math.round(60000000 / 124);
const conductor = track("", [], [[0xFF, 0x51, 3, tempo >> 16, (tempo >> 8) & 255, tempo & 255], [0xFF, 0x58, 4, 4, 2, 24, 8]]);
// drums: bar A (kick 4/4, snare 4 and 12, hats on the eighths, a ghost snare on 15, a flam kick on 8), bar B (the same, an open hat on 14); A B A B
const drums = [];
for (let b = 0; b < 4; b++) {
  const t0 = b * BAR;
  for (const s of [0, 4, 8, 12]) drums.push([t0 + s * ST, 10, 36, s === 0 ? 120 : 100, 9]);
  drums.push([t0 + 8 * ST + 6, 10, 36, 100, 9]);              // the flam: a second kick a quarter step late
  for (const s of [4, 12]) drums.push([t0 + s * ST, 10, 38, 100, 9]);
  drums.push([t0 + 15 * ST, 10, 38, 40, 9]);
  for (let s = 0; s < 16; s += 2) drums.push([t0 + s * ST, 10, 42, 90, 9]);
  if (b % 2) drums.push([t0 + 14 * ST, 10, 46, 100, 9]);
}
// chords: i VI III VII (Am F C G), a bar each, twice
const CH = [[57, 60, 64], [53, 57, 60], [48, 52, 55], [55, 59, 62]];
const chords = [];
for (let b = 0; b < 8; b++) for (const k of CH[b % 4]) chords.push([b * BAR, BAR - 4, k, 80, 0]);
// bass: on each chord's root: R on 0 (accent), 5th on 6, R+octave on 8, a glide into the 3rd on 11 (legato from 10)
const ROOTS = [A1, A1 - 4, A1 + 3, A1 - 2];                   // A1, F1, C2, G1
const bass = [];
for (let b = 0; b < 8; b++) {
  const r = ROOTS[b % 4], t0 = b * BAR, third = b % 4 === 0 ? 3 : 4, fifth = 7;
  bass.push([t0, ST * 2, r, 120, 1], [t0 + 6 * ST, ST, r + fifth, 90, 1], [t0 + 8 * ST, ST, r + 12, 90, 1], [t0 + 10 * ST, ST + 4, r, 90, 1], [t0 + 11 * ST, ST, r + third, 90, 1]);
}
// acid (bar 1 only; chord Am): A2 C3 E3 (slide into) G3, an F#3 out of the scale on 12
const acid = [[0, ST, 45, 110, 2], [2 * ST, ST, 48, 90, 2], [4 * ST, ST + 6, 52, 90, 2], [5 * ST, ST, 55, 90, 2], [12 * ST, ST, 54, 90, 2]];
// a pluck melody (bar 1, Am): A4 C5 E5 A5 on 0 4 8 12
const pluck = [[0, ST, 69, 100, 3], [4 * ST, ST, 72, 70, 3], [8 * ST, ST, 76, 50, 3], [12 * ST, ST, 81, 120, 3]];
const file = smf([conductor, track("Drums", drums), track("Chords", chords), track("Bass", bass), track("Acid 303", acid), track("Melody", pluck)]);

if (process.env.MIDI_OUT) fs.writeFileSync(process.env.MIDI_OUT, file);   // (the file itself, to try in the studio)
const F = M.midiParse(file);
check("parsed: 5 tracks with notes, 96 PPQ, 124 BPM, 4/4, 8 bars", F.tracks.length === 5 && F.ppq === 96 && Math.round(F.bpm) === 124 && F.sig[0] === 4 && F.sig[1] === 4 && F.bars === 8);
const roles = F.tracks.map((t) => M.midiGuess(t).role);
check("parts guessed: drums, chords, bass, acid, pluck (" + roles.join(" ") + ")", roles.join() === "drums,chords,bass,acid,pluck");
const K = M.midiKey(F, (i) => roles[i] === "drums");
check("key: A aeolian (" + K.root + " " + K.mode + ")", K.root === 9 && K.mode === 5);
check("a mode's pool as the firmware's (aeolian 0x5AD)", M.midiPool(5) === 0x5AD);
const R = M.midiToPiece(F, { roles, bar: 0, line: 0, root: K.root, mode: K.mode });
console.log("    " + R.report.join("\n    "));
check("chords: i VI III VII, a bar each, the repeat folded", R.prog && R.prog.map((s) => s.deg + ":" + s.bars).join(" ") === "1:1 6:1 3:1 7:1");
check("chords: triads", R.prog.every((s) => s.kind === 1));
const bs = R.bass.map((v) => ({ k: v & 7, n: (v >> 3) & 7, o: (v >> 6) & 3 }));
check("bass: the root accented on step 1", bs[0].k === 2 && bs[0].n === 0 && bs[0].o === 1);
check("bass: the fifth on 7, the octave on 9", bs[6].k === 1 && bs[6].n === 4 && bs[6].o === 1 && ((bs[8].n === 7 && bs[8].o === 1) || (bs[8].n === 0 && bs[8].o === 2)));
check("bass: a glide into the third on 12", (bs[11].k === 3 || bs[11].k === 4) && bs[11].n === 2);
check("bass: rests elsewhere", [1, 2, 3, 4, 5, 7, 9, 12, 13, 14, 15].every((s) => bs[s].k === 0));
check("acid: the root accented, a third, a fifth sliding into the seventh", R.acid[0].gate && R.acid[0].acc && R.acid[0].off === 0 && R.acid[2].off === 2 && R.acid[4].off === 4 && R.acid[4].slide && R.acid[5].off === 6 && !R.acid[5].slide);
check("acid: F# (out of the scale) snapped to a scale step", R.acid[12].gate && (R.acid[12].off === 5 || R.acid[12].off === 4) && R.report.some((l) => /outside the scale/.test(l)));
check("pluck: the triad's rungs from C up, as the engine's ladder counts them (Am: C E A; A4 C5 E5 A5 = rungs 2 3 4 5)", [0, 4, 8, 12].map((s) => R.pluck[s].rung).join() === "2,3,4,5");
check("pluck: the notes as written go with it (the studio puts them on the piece's own ladder)", R.pluckKeys && [0, 4, 8, 12].map((s) => R.pluckKeys[s].key).join() === "69,72,76,81");
check("pluck: levels from velocity (accent, note, soft, accent)", [0, 4, 8, 12].map((s) => R.pluck[s].lvl).join() === "2,2,1,3" || [0, 4, 8, 12].map((s) => R.pluck[s].lvl).join() === "3,2,1,3");
check("drums: two different bars, as P1..P6 in turn", R.pats && R.pats[0] !== R.pats[1] && R.pats[2] === R.pats[0] && R.pats[3] === R.pats[1]);
check("drums: kick 0 4 8 12, snare 4 12 15, hats on the eighths", R.pats[0][0] === 0x1111 && R.pats[0][1] === (0x1010 | 0x8000) && R.pats[0][3] === 0x5555);
check("drums: open hat on 15 in the second bar only", !R.pats[0][4] && R.pats[1][4] === 1 << 14);
check("tricks: the flam kick a ratchet, the first kick an accent, the ghost snare a ghost", (R.mods[0][8] & 7) === 1 && ((R.mods[0][0] >> 6) & 3) === 3 && ((R.mods[1][15] >> 6) & 3) === 1);
check("tempo 124", R.tempo === 124);
let threw = false;
try { M.midiParse(new Uint8Array([1, 2, 3])); } catch (_) { threw = true; }
check("not a MIDI file: refused", threw);
console.log(bad ? bad + " FAILED" : "all ok");
process.exit(bad ? 1 : 0);
