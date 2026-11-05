// SPDX-License-Identifier: GPL-3.0-only
// web/fm1idle.js: a page lets go of the FM-1's ports after a quiet spell, not while busy, and opens them again on use.
let fakeNow = 0;
Date.now = () => fakeNow;
const ticks = [];
globalThis.setInterval = (f) => { ticks.push(f); return ticks.length; };
globalThis.clearInterval = () => { ticks.length = 0; };
const tick = (ms) => { fakeNow += ms; ticks.forEach((f) => f()); };

function port(type) {
  return { type, connection: "closed", onmidimessage: null, open() { this.connection = "open"; }, close() { this.connection = "closed"; } };
}
const inp = port("input"), out = port("output");
Object.defineProperty(globalThis, "navigator", { configurable: true,
  value: { requestMIDIAccess: async () => ({ inputs: new Map([[1, inp]]), outputs: new Map([[1, out]]) }) } });

const { midiAccess, idleRelease, idleTouch, idleOpenPorts } = await import("../web/fm1idle.js");
let bad = 0;
const check = (what, c) => { console.log((what + " ").padEnd(64, ".") + (c ? " ok" : " FAIL")); if (!c) bad++; };

let busy = false, released = 0;
idleRelease(() => busy, () => released++);
const m = await midiAccess();
m.inputs.get(1).open(); m.outputs.get(1).open();
inp.onmidimessage = () => {};
check("two ports open after use", idleOpenPorts() === 2);
tick(1000); tick(1000);
check("still open 2 s later", idleOpenPorts() === 2 && released === 0);
idleTouch();
tick(3000);
check("traffic keeps them open (3 s after the last message)", idleOpenPorts() === 2);
busy = true;
tick(10000);
check("busy (an install, a write, the live status): kept for 10 s", idleOpenPorts() === 2 && released === 0);
busy = false;
tick(1000);
check("quiet and not busy: both closed, the page told", idleOpenPorts() === 0 && released === 1 && inp.onmidimessage === null);
tick(5000);
check("nothing more to close: told once", released === 1);
inp.open(); out.open();
check("opened again on the next use", idleOpenPorts() === 2);
if (bad) { console.log(bad + " failed"); process.exit(1); }
console.log("web idle tests passed");
