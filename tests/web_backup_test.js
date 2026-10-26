// SPDX-License-Identifier: GPL-3.0-only
// The installer page's backup and restore messages and file (web/fm1backup.js), against the firmware's (firmware/src/backup_proto.h; its host test builds the same
// frame): a frame parses, a damaged one does not, a chunk is what the firmware's parser takes, and a backup file comes back as it went. node tests/web_backup_test.js
const fs = require("fs");
const path = require("path");
const src = fs.readFileSync(path.join(__dirname, "..", "web", "fm1backup.js"), "utf8").replace(/^export /gm, "");
const M = new Function(src + "; return { BK_CHUNK, BK_FRAME, BK_MAX, BK_OBJECTS, bkName, bkSum, bkChunkMsg, bkReadMsg, bkCommitMsg, bkRestartMsg, bkParseFrame, bkParseAck, bkCrc32, bkEncode, bkDecode };")();
let bad = 0;
const check = (name, ok) => { console.log((ok ? "  ok   " : "  FAIL ") + name); if (!ok) bad++; };
const eq = (a, b) => JSON.stringify(Array.from(a)) === JSON.stringify(Array.from(b));

// the frame bk_build_frame(f, 3, 1, 100, {0x12, 0xAB}, 2) makes (tests/host/dxproto_test.c checks the same bytes)
const vec = [0xF0, 0x7D, 0x50, 0x43, 0x03, 0x00, 0x01, 0x00, 0x64, 0x00, 0x02, 0x01, 0x02, 0x0A, 0x0B, 0x7E, 0xF7];
const f = M.bkParseFrame(vec);
check("the firmware's frame parses", f && f.obj === 3 && f.idx === 1 && f.len === 100 && eq(f.data, [0x12, 0xAB]));
const dmg = vec.slice(); dmg[13] ^= 1;
check("a damaged frame does not", M.bkParseFrame(dmg) === null);
check("a short frame does not", M.bkParseFrame(vec.slice(0, 12)) === null && M.bkParseFrame([]) === null);

// a chunk, as the firmware's parser wants it: 7D 50 42 03 obj idx(2) len(2) n(2) [2n] chk, F0 .. F7 around it
const data = new Uint8Array(300).map((_, i) => (i * 7 + 1) & 255);
const msg = M.bkChunkMsg(9, 1, 3000, data.subarray(0, 256));
check("a chunk is F0 7D 50 42 03 .. F7, all 7-bit between", msg[0] === 0xF0 && msg[msg.length - 1] === 0xF7 && msg.slice(1, -1).every((b) => b < 128) && eq(msg.slice(1, 5), [0x7D, 0x50, 0x42, 0x03]));
check("a chunk of 256 is 12 + 512 + 1 bytes longer than F0 .. F7's two", msg.length === 2 + 12 + 512);
const m = msg.slice(1, -1);
const n = (m[9] << 7) | m[10];
check("its n, idx and len", n === 256 && m[4] === 9 && ((m[5] << 7) | m[6]) === 1 && ((m[7] << 7) | m[8]) === 3000);
let back = [];
for (let k = 0; k < n; k++) back.push((m[11 + 2 * k] << 4) | m[12 + 2 * k]);
check("its bytes come out", eq(back, data.subarray(0, 256)));
check("its checksum (the data bytes plus it add up to 0 modulo 128)", (m.slice(4, 11 + 2 * n).reduce((a, b) => a + b, 0) + m[11 + 2 * n]) % 128 === 0);

check("the short messages", eq(M.bkReadMsg(5), [0xF0, 0x7D, 0x50, 0x42, 0x01, 5, 0xF7]) && eq(M.bkCommitMsg(5, 3776), [0xF0, 0x7D, 0x50, 0x42, 0x04, 5, 29, 64, 0xF7]) &&
  eq(M.bkRestartMsg(), [0xF0, 0x7D, 0x50, 0x42, 0x05, 0xF7]));
const ack = M.bkParseAck([0xF0, 0x7D, 0x50, 0x44, 7, 0x7F, 0, 0xF7]);
check("an answer", ack && ack.obj === 7 && ack.idx === 0x7F && ack.status === 0 && M.bkParseAck([0xF0, 0x7D, 0x50, 0x57, 1, 2, 3, 0xF7]) === null);

// the objects: the panel's calibration (1) is never among them, every other store is, each once
const ids = M.BK_OBJECTS.map((o) => o.id);
check("the objects: 19, no panel, none twice", ids.length === 19 && !ids.includes(1) && new Set(ids).size === 19 && ids.every((i) => i >= 0 && i <= 19));
check("their names", M.bkName(0) === "HYPNO" && M.bkName(5) === "LOFI" && M.bkName(19) === "TRIPHOP");

// the file
const objs = [{ id: 0, data: new Uint8Array(3776).map((_, i) => i & 255) }, { id: 4, data: new Uint8Array(12).fill(7) }, { id: 12, data: new Uint8Array(0) }];
const file = M.bkEncode(objs, { id: "FM-1_7001307", created: "2026-10-25" });
const dec = M.bkDecode(file);
check("a backup file comes back as it went", dec.objs.length === 3 && dec.objs.every((o, i) => o.id === objs[i].id && eq(o.data, objs[i].data)) && dec.info.id === "FM-1_7001307");
const rot = file.slice(); rot[100] ^= 0x10;
let threw = false; try { M.bkDecode(rot); } catch (_) { threw = true; }
check("a damaged file is refused", threw);
threw = false; try { M.bkDecode(new TextEncoder().encode("hello, not a backup, really")); } catch (_) { threw = true; }
check("a file that is not a backup is refused", threw);
console.log(bad ? "web backup: FAILED" : "web backup: ok");
process.exit(bad ? 1 : 0);
