// SPDX-License-Identifier: GPL-3.0-only
// What the pieces and library pages share (on top of fm1piece.js): a piece's or a set's names and its line of text from the firmware's own parameter data
// (params.json), the share card's contents, putting a piece or a set into the browser emulator's store, and the origins this browser remembers (the pieces
// it added from a share, so a piece changed from one says what it is based on).

export function shareNames(D) {
  const P = D.params, E = D.enum, N = D.names;
  const NOTE = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
  const genreName = (g) => N.genres[g] || "genre " + g;
  const pieceName = (g, i) => (N.pieces[g] && N.pieces[g][i]) || String(i + 1).padStart(2, "0");
  const get = (pc, k) => pcGet(pc, E[k], P[E[k]].min);
  const userSlot = (pc) => pcUserSlot(pc, E.P_CM_SND, N.nplain, N.nfmfact);
  function describe(pc) {
    const tempo = get(pc, "P_TEMPO"), root = get(pc, "P_ROOT"), mode = get(pc, "P_MODE"), snd = get(pc, "P_CM_SND");
    const modeName = mode !== undefined && P[E.P_MODE].t ? P[E.P_MODE].t[mode - P[E.P_MODE].min] : "";
    const bits = [];
    if (tempo !== undefined) bits.push(tempo + " BPM");
    if (root !== undefined) bits.push(NOTE[((root % 12) + 12) % 12] + " " + modeName.toLowerCase());
    if (snd !== undefined) {
      const k = userSlot(pc);
      bits.push("pluck: " + (k >= 0 ? (pc.voice ? pcVoiceName(pc.voice) + " (its own DX7 voice)" : "your DX7 voice " + (k + 1)) : (P[E.P_CM_SND].t || [])[snd] || snd));
    }
    return bits.join(" · ");
  }
  const tagOf = (pc) => genreName(pc.genre) + " " + pieceName(pc.genre, pc.slot);
  const placeName = (x) => { const q = pcSetPlaces([x])[0]; return q ? genreName(q.genre) + " " + pieceName(q.genre, q.slot) : "-"; };
  const basedText = (m) => (m && m.b ? (m.b.t || "a shared piece") + (m.b.a ? " by " + m.b.a : "") : "");
  // the card's contents: a piece's pattern (the one it plays: P_PATTERN), or a set's places
  function cardOf(x, url) {
    const m = x.meta || {};
    if (x.kind === "set") {
      const used = x.list.filter((v) => v), mask = x.list.reduce((a, v, i) => a | (v ? 1 << i : 0), 0);
      const names = used.slice(0, 4).map(placeName).join(", ") + (used.length > 4 ? " …" : "");
      return { tag: "SET · " + used.length + " PIECES", title: m.t, by: m.a, based: basedText(m), line: names, lanes: [mask], center: String(used.length), url };
    }
    const pat = Math.max(0, Math.min(5, get(x, "P_PATTERN") || 0)), lanes = [];
    for (let l = 0; l < 6; l++) lanes.push(x.rec[2 + 2 * (pat * 6 + l)] | (x.rec[3 + 2 * (pat * 6 + l)] << 8));
    const tempo = get(x, "P_TEMPO");
    return { tag: tagOf(x), title: m.t, by: m.a, based: basedText(m), line: describe(x), lanes, center: tempo !== undefined ? tempo + " BPM" : "", url };
  }
  return { P, E, N, genreName, pieceName, describe, tagOf, placeName, basedText, cardOf, userSlot,
           objOf: (g) => (g === 0 ? 0 : 4 + g), OBJ_VOICES: 3, OBJ_GLOB: 4,
           globDefs: { tune: P[E.P_TUNE].def, midi: P[E.P_MIDI].def, theme: P[E.P_THEME].def } };
}

// the emulator keeps its flash in this browser (web/emu: localStorage "hyp1-emu-store", an object's bytes in base64 by its number): a piece (at `slot`)
// or a set goes in there, with its voices. The answer: what to do in the emulator.
export function emuPut(S, x, slot) {
  const KEY = "hyp1-emu-store";
  const raw = JSON.parse(localStorage.getItem(KEY) || "{}");
  const get = (o) => (raw[o] ? Uint8Array.from(atob(raw[o]), (c) => c.charCodeAt(0)) : new Uint8Array(0));
  const put = (o, u8) => { let s = ""; for (const c of u8) s += String.fromCharCode(c); raw[o] = btoa(s); };
  const npar = S.P.length, banks = {};
  const bank = (g) => { if (!banks[g]) { banks[g] = get(S.objOf(g)); if (pcBankCheck(banks[g], g) === "old") banks[g] = new Uint8Array(0); } return banks[g]; };
  const pieces = x.kind === "set" ? x.pieces.map((p) => ({ ...p, rec: p.rec.slice() })) : [{ ...x, rec: x.rec.slice(), slot }];
  const voices = x.kind === "set" ? x.voices : pieces[0].voice && S.userSlot(pieces[0]) >= 0 ? [pieces[0].voice] : [];
  if (voices.length) {
    const r = pcSetVoices(get(S.OBJ_VOICES), voices);
    if (r.vox) put(S.OBJ_VOICES, r.vox);
    for (const pc of pieces) {
      const vi = x.kind === "set" ? pc.vidx : 0;
      if (vi >= 0 && r.map[vi] >= 0 && S.userSlot(pc) >= 0) pcSet(pc, S.E.P_CM_SND, S.P[S.E.P_CM_SND].min, S.N.nplain + S.N.nfmfact + r.map[vi]);
    }
  }
  for (const pc of pieces) banks[pc.genre] = pcIntoBank(bank(pc.genre), pc.genre, pc.slot, pc, npar).bank;
  for (const g of Object.keys(banks)) put(S.objOf(+g), banks[g]);
  if (x.kind === "set") put(S.OBJ_GLOB, pcGlobWith(get(S.OBJ_GLOB), x, S.globDefs));
  localStorage.setItem(KEY, JSON.stringify(raw));
  if (x.kind === "set") return "The set is in the emulator: switch it on, then GLO twice for the set list (Play on).";
  return "In the emulator as " + S.tagOf({ ...x, slot }) + ": switch it on, GLO + PRESETS to " + S.genreName(x.genre) + ", PRESETS to " + S.pieceName(x.genre, slot) + ".";
}

// the origins: the pieces this browser added from a share ("genre:place" -> title, maker, the record's CRC as it went in)
const ORIGIN_KEY = "perf1-pieces-origin";
export function originsGet() { try { return JSON.parse(localStorage.getItem(ORIGIN_KEY) || "{}") || {}; } catch (_) { return {}; } }
export function originsSet(list) {                                 // list: [{genre, slot, t, a, rec}]
  const o = originsGet();
  for (const e of list) o[e.genre + ":" + e.slot] = { t: e.t || "", a: e.a || "", h: pcHash(e.rec) };
  try { localStorage.setItem(ORIGIN_KEY, JSON.stringify(o)); } catch (_) {}
}
// what a piece about to be shared came from: {same: true, t, a} when it is as it was added, {b: {t, a}} when it was changed since, null when not from a share
export function originOf(pc) {
  const o = originsGet()[pc.genre + ":" + pc.slot];
  if (!o) return null;
  return pcHash(pc.rec) === o.h ? { same: true, t: o.t, a: o.a } : { b: { t: o.t, a: o.a } };
}
