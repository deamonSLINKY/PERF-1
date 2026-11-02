// SPDX-License-Identifier: GPL-3.0-only
// Sharing a piece (web/fm1piece.js): a piece out of a bank, into a file and a code and back, into another bank (an empty one, one saved by a firmware with
// fewer settings), its DX7 voice placed (already there, a free slot, another voice in its slot), and the about answer the firmware sends (dxproto_test.c builds
// the same bytes). With the simulator's path (node tests/web_piece_test.js build/host/amb_host), the whole way: the simulator saves a piece, this file shares
// it as an older firmware's piece with its own voice into another slot, and the simulator loads it.
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");
const src = fs.readFileSync(path.join(__dirname, "..", "web", "fm1piece.js"), "utf8").replace(/^export /gm, "");
const M = new Function(src + "; return { PC_HDR, PC_REC, PC_LEN, PC_PRG, PC_REC_PAR, PC_RECIPE, PC_VOX_LEN, pcBankCheck, pcSaved, pcFromBank, pcGet, pcSet, pcEncode, pcDecode, pcToCode, pcFromCode, pcIntoBank, pcVoiceName, pcUserSlot, pcVoices, pcVoicePlan, pcVoicesWith, pcParseAbout, pcAboutMsg };")();
let bad = 0;
const check = (name, ok) => { console.log((ok ? "  ok   " : "  FAIL ") + name); if (!ok) bad++; };
const eq = (a, b) => a.length === b.length && Array.from(a).every((x, i) => x === b[i]);
const throws = (f) => { try { f(); return false; } catch (_) { return true; } };
const P_TEMPO = 5, P_EVOLVE = 6, P_CM_SND = 99, NPLAIN = 12, NFMFACT = 30, NPAR = 176;

// a bank as the firmware writes it: HYP3, 2, genre, npar, saved, 376, 1, 1, 1; ten records, the progressions
function bankOf(genre, npar, saved, fill) {
  const b = new Uint8Array(M.PC_LEN);
  b.set([0x48, 0x59, 0x50, 0x33, 2, genre, npar & 255, npar >> 8, saved & 255, saved >> 8, M.PC_REC & 255, M.PC_REC >> 8, 1, 1, 1], 0);
  for (let i = 0; i < 10; i++) {
    const r = M.PC_HDR + i * M.PC_REC;
    b[r] = 0x01; b[r + 1] = 0x0A;                                       // a pool
    for (let p = 0; p < npar; p++) b[r + M.PC_REC_PAR + p] = (fill + i + p) % 90;
    b.fill(i + 1, M.PC_HDR + 10 * M.PC_REC + i * M.PC_PRG, M.PC_HDR + 10 * M.PC_REC + (i + 1) * M.PC_PRG);
  }
  return b;
}
const voiceOf = (name, k) => { const v = new Uint8Array(128).map((_, i) => (i * 5 + k) & 0x7F); v.set(Array.from(name.padEnd(10)).map((c) => c.charCodeAt(0)), 118); return v; };

(async () => {
  // out of a bank
  const bank = bankOf(18, NPAR, 0b10010, 7);
  check("a bank is checked", M.pcBankCheck(bank, 18) === null && M.pcBankCheck(bank, 17) === "old" && M.pcBankCheck(new Uint8Array(0), 18) === "empty");
  const old = bank.slice(); old[13] = 0;
  check("a bank with the old pluck numbering is old", M.pcBankCheck(old, 18) === "old" && throws(() => M.pcFromBank(old, 18, 1)));
  check("the saved pieces", M.pcSaved(bank) === 0b10010);
  check("an unsaved piece is not shared", throws(() => M.pcFromBank(bank, 18, 0)));
  const pc = M.pcFromBank(bank, 18, 4);
  check("a piece out of its bank: its record and progression", pc.npar === NPAR && eq(pc.rec, bank.slice(M.PC_HDR + 4 * M.PC_REC, M.PC_HDR + 5 * M.PC_REC)) && eq(pc.prg, [5, 5, 5, 5, 5, 5]));
  check("a setting", M.pcGet(pc, P_TEMPO, 70) === 70 + (7 + 4 + P_TEMPO) % 90);

  // the file and the code
  pc.voice = voiceOf("SCREAMER", 3);
  pc.meta = { t: "Night forest", a: "someone", g: "FOREST", p: "O05" };
  const file = M.pcEncode(pc);
  const back = M.pcDecode(file);
  check("a file comes back as it went", back.genre === 18 && back.slot === 4 && back.npar === NPAR && eq(back.rec, pc.rec) && eq(back.prg, pc.prg) && eq(back.voice, pc.voice) && back.meta.t === "Night forest");
  check("a file is " + file.length + " bytes", file.length < 600);
  const dmg = file.slice(); dmg[100] ^= 1;
  check("a damaged file is refused", throws(() => M.pcDecode(dmg)) && throws(() => M.pcDecode(new TextEncoder().encode("not a piece at all, no"))));
  const nov = M.pcDecode(M.pcEncode({ ...pc, voice: null }));
  check("a piece without its own voice", nov.voice === null && eq(nov.rec, pc.rec));
  const code = await M.pcToCode(pc);
  check("a code is P1. and URL-safe (" + code.length + " characters)", /^P1\.[A-Za-z0-9_-]+$/.test(code) && code.length < 800);
  const fromCode = await M.pcFromCode("look at this: https://x.example/pieces/#p=" + code + " nice");
  check("a code comes back, found in a link or a sentence", eq(fromCode.rec, pc.rec) && eq(fromCode.voice, pc.voice) && fromCode.meta.p === "O05");
  let threw = false; try { await M.pcFromCode(code.slice(0, 40)); } catch (_) { threw = true; }
  check("a code cut short is refused", threw);

  // into a bank
  const into = M.pcIntoBank(new Uint8Array(0), 18, 2, pc, NPAR);
  check("into an empty bank: a valid bank with only it saved", M.pcBankCheck(into.bank, 18) === null && M.pcSaved(into.bank) === 0b100 && !into.replaced);
  const again = M.pcFromBank(into.bank, 18, 2);
  check("and it comes out the same", eq(again.rec, pc.rec) && eq(again.prg, pc.prg));
  const over = M.pcIntoBank(bank, 18, 1, pc, NPAR);
  check("over a saved piece: said, the other kept", over.replaced && M.pcSaved(over.bank) === 0b10010 && eq(M.pcFromBank(over.bank, 18, 4).rec, pc.rec));
  const older = { ...pc, npar: 170, rec: pc.rec.slice() };
  older.rec.fill(0, M.PC_REC_PAR + 170, M.PC_REC_PAR + NPAR);
  const r1 = M.pcIntoBank(bank, 18, 0, older, NPAR), o1 = M.pcFromBank(r1.bank, 18, 0);
  check("a piece from a firmware with fewer settings: the missing ones are the recipe's", r1.filled === 6 && M.pcGet(o1, 171, 0) === undefined && M.pcGet(o1, 169, 0) === M.pcGet(pc, 169, 0));
  const bank170 = bankOf(18, 170, 0b1, 3);
  const r2 = M.pcIntoBank(bank170, 18, 5, pc, NPAR);
  const o2 = M.pcFromBank(r2.bank, 18, 0);
  check("a bank saved by a firmware with fewer: its other pieces' new settings are the recipe's", M.pcFromBank(r2.bank, 18, 5).npar === NPAR && M.pcGet(o2, 172, 0) === undefined && M.pcGet(o2, 10, 0) === (3 + 10) % 90);
  const r3 = M.pcIntoBank(bank, 18, 3, pc, 170);
  check("into an older firmware: the settings it lacks are dropped (and said)", r3.dropped === 6 && M.pcFromBank(r3.bank, 18, 3).npar === 170);
  check("into an older bank layout: refused", throws(() => M.pcIntoBank(old, 18, 0, pc, NPAR)));

  // the voice
  const user = pc.rec.slice();
  const vp = { ...pc, rec: user };
  M.pcSet(vp, P_CM_SND, 0, NPLAIN + NFMFACT + 2);
  check("the piece's voice slot", M.pcUserSlot(vp, P_CM_SND, NPLAIN, NFMFACT) === 2);
  M.pcSet(vp, P_CM_SND, 0, 14);
  check("a built-in sound is no slot", M.pcUserSlot(vp, P_CM_SND, NPLAIN, NFMFACT) < 0);
  const v0 = voiceOf("BRASS 1", 0), v1 = voiceOf("E.PIANO", 1);
  const vox = M.pcVoicesWith(M.pcVoicesWith(new Uint8Array(0), v0, 0), v1, 1);
  check("the voices' object", vox.length === M.PC_VOX_LEN && M.pcVoices(vox).length === 2 && M.pcVoiceName(M.pcVoices(vox)[1]) === "E.PIANO");
  check("a voice already loaded: nothing to write", JSON.stringify(M.pcVoicePlan(vox, v1, 0)) === JSON.stringify({ kind: "same", slot: 1 }));
  const plan = M.pcVoicePlan(vox, pc.voice, 1);
  check("another voice in its slot: a conflict, with a free slot offered", plan.kind === "conflict" && plan.slot === 1 && plan.other === "E.PIANO" && plan.free === 2);
  check("its slot empty: the next free one", M.pcVoicePlan(vox, pc.voice, 9).kind === "free" && M.pcVoicePlan(vox, pc.voice, 9).slot === 2);
  let full = new Uint8Array(0);
  for (let k = 0; k < 16; k++) full = M.pcVoicesWith(full, voiceOf("V" + k, k + 10), k);
  check("all sixteen in use: no free slot", M.pcVoicePlan(full, pc.voice, 4).free === -1 && throws(() => M.pcVoicesWith(full, pc.voice, 16)));
  check("a slot past the next free one is refused", throws(() => M.pcVoicesWith(vox, v0, 5)));

  // the about answer (tests/host/dxproto_test.c: bk_build_about(f, 176, 19, 5))
  const ab = M.pcParseAbout([0xF0, 0x7D, 0x50, 0x45, 0x01, 0x01, 0x30, 0x13, 0x05, 0xF7]);
  check("the firmware's about answer", ab && ab.ver === 1 && ab.npar === 176 && ab.genres === 19 && ab.voices === 5);
  check("something else is not", M.pcParseAbout([0xF0, 0x7D, 0x50, 0x44, 1, 2, 3, 0xF7]) === null);

  // the whole way, in the simulator: save 01 (Tempo 132, Evolve 43), share it as a piece made before the last six settings with its own voice, add it as 04
  const host = process.argv[2];
  if (host) {
    const dir = path.join("build", "webpiece");
    fs.mkdirSync(dir, { recursive: true });
    const run = (name, lines) => {
      const f = path.join(dir, name + ".amb");
      fs.writeFileSync(f, lines.join("\n") + "\n");
      try { execFileSync(host, [f, path.join(dir, name)], { stdio: "pipe" }); return true; } catch (e) { console.log(String(e.stdout || "").split("\n").filter((l) => /FAIL/.test(l)).join("\n")); return false; }
    };
    const bin = path.join(dir, "bank0.bin");
    check("the simulator saves 01", run("save", ["seed 0", "wait 3000", "tap GLO", "tap HOME", "wait 100", "spin K1 4", "spin K2 3", "wait 200", "expect tempo 132",
      "tap GLO", "tap SAVE", "wait 500", "expect saved 1", "dumpobj 0 " + bin]));
    const saved = new Uint8Array(fs.readFileSync(bin));
    const shared = await M.pcFromCode(await M.pcToCode({ ...M.pcFromBank(saved, 0, 0), voice: voiceOf("SCREAMER", 9) }));
    shared.npar = NPAR - 6;
    shared.rec.fill(0, M.PC_REC_PAR + NPAR - 6, M.PC_REC_PAR + NPAR);
    M.pcSet(shared, P_CM_SND, 0, NPLAIN + NFMFACT + 0);                    // its pluck: its own voice, slot 0 where it was made
    const vplan = M.pcVoicePlan(new Uint8Array(0), shared.voice, 0);
    const newBank = M.pcIntoBank(new Uint8Array(0), 0, 3, shared, NPAR).bank;
    fs.writeFileSync(path.join(dir, "bank_new.bin"), newBank);
    fs.writeFileSync(path.join(dir, "vox_new.bin"), M.pcVoicesWith(new Uint8Array(0), shared.voice, vplan.slot));
    check("the simulator plays it as 04, with its voice, the settings it predates its recipe's", vplan.kind === "free" && vplan.slot === 0 && run("load", [
      "loadobj 0 " + path.join(dir, "bank_new.bin"), "loadobj 3 " + path.join(dir, "vox_new.bin"), "seed 0", "reboot", "wait 2000", "expect saved 8", "expect uservoices 1",
      "turn PRESET 3", "wait 1500", "expect piece 3", "expect tempo 132", "expect par" + P_EVOLVE + " 43", "expect par" + P_CM_SND + " " + (NPLAIN + NFMFACT), "expect parsbad 0"]));
  }
  console.log(bad ? "web piece: FAILED" : "web piece: ok");
  process.exit(bad ? 1 : 0);
})();
