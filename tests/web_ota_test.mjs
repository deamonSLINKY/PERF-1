// SPDX-License-Identifier: GPL-3.0-only
// The web installer's handshake (web/fm1ota.js) against fake FM-1s: what each firmware answers.
// PERF-1 0.16.13 on answers the plain query with "FM-1_700" (M-UPGRADE 3.11 takes NAME_ and 1-3 digits only) and its
// whole identity to the query with the body 01; the installer has to see the whole one, and every older firmware as before.
import { Updater, pack7, unpack7 } from "../web/fm1ota.js";

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const HS_FULL = [0xF0, ...pack7([0, 0x59, 0x11, 1, 0, 0, 1, 0xFE]), 0xF7];
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const idPkt = (t) => [0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...[...t].map((c) => c.charCodeAt(0)), ...new Array(28 - t.length).fill(0)]), 0xF7];

// one port: answer(query) -> the identity text to send back, or null
function access(name, answer) {
  const input = { name, state: "connected", onmidimessage: null, open: async () => {} };
  const output = {
    name, state: "connected", open: async () => {}, sent: [],
    send(b) {
      this.sent.push(Array.from(b));
      const t = answer(Array.from(b));
      if (t !== null) setTimeout(() => input.onmidimessage && input.onmidimessage({ data: idPkt(t) }), 5);
    },
  };
  return { inputs: new Map([[1, input]]), outputs: new Map([[1, output]]), output };
}

let bad = 0;
function check(what, cond) { console.log((what + " ").padEnd(70, ".") + (cond ? " ok" : " FAIL")); if (!cond) bad++; }

const cases = [
  ["PERF-1 0.16.13 on: FM-1_700, then the whole identity", (q) => same(q, HS_QUERY) ? "FM-1_700" : same(q, HS_FULL) ? "FM-1_7001613" : null, "FM-1_7001613", true],
  ["PERF-1 before 0.16.13: the whole identity to the plain query", (q) => same(q, HS_QUERY) ? "FM-1_7001612" : null, "FM-1_7001612", false],
  ["AMB-1 / HYP-1: FM-1_700, no answer to 01", (q) => same(q, HS_QUERY) ? "FM-1_700" : null, "FM-1_700", true],
  ["the stock firmware: FM-1_015, never asked with 01", (q) => same(q, HS_QUERY) ? "FM-1_015" : null, "FM-1_015", false],
  ["the update loader: ota-FM-1_015, never asked with 01", (q) => same(q, HS_QUERY) ? "ota-FM-1_015" : null, "ota-FM-1_015", false],
];
for (const [what, answer, want, asksFull] of cases) {
  const a = access("FM-1", answer);
  const dev = await new Updater(a).find();
  if (dev) dev.link.close();
  check(what + ": " + want, dev && dev.id.text === want);
  check(what + ": " + (asksFull ? "asks with 01" : "never asks with 01"), a.output.sent.some((q) => same(q, HS_FULL)) === asksFull);
}
// the bytes of the query with 01, as the firmware checks them (ota_take: header, length, checksum)
const u = unpack7(HS_FULL.slice(1, -1));
check("HS_FULL decodes to 00 59 11, length 1, body 01, checksum FE", u.length >= 8 && same(u.slice(0, 8), [0, 0x59, 0x11, 1, 0, 0, 1, 0xFE]));
if (bad) { console.log(bad + " failed"); process.exit(1); }
console.log("web ota tests passed");
