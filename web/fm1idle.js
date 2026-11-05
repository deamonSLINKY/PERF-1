// SPDX-License-Identifier: GPL-3.0-only
// Let go of the FM-1's MIDI ports once a page has been quiet a while (0.16.16). On Windows a USB MIDI port belongs to one
// program at a time, and a page that had talked to the FM-1 kept it until the tab was closed: M-UPGRADE then said "A USB
// MIDI device is present but did not answer" (its log: KsCreatePin error 1450), and music software could not open it either.
// Every MIDIAccess the page asks for goes through midiAccess(); idleRelease() closes their open ports after `ms` without
// FM-1 traffic (idleTouch() on each message), unless busy() says an install, a write or the live status is running. The
// next use opens them again (an input when its onmidimessage is set or open() is called, an output on send).
// (The site inlines this file into the pages, so every name here carries "idle" or "IDLE".)

const IDLE_ACC = new Set();
let idleLast = Date.now(), idleTimer = 0;

export function idleTouch() { idleLast = Date.now(); }

export async function midiAccess() {
  const m = await navigator.requestMIDIAccess({ sysex: true });
  IDLE_ACC.add(m);
  idleTouch();
  return m;
}

export function idleOpenPorts() {                 // how many ports the page holds open now (for the tests)
  let n = 0;
  for (const m of IDLE_ACC) for (const p of [...m.inputs.values(), ...m.outputs.values()]) if (p.connection === "open" || p.connection === "pending") n++;
  return n;
}

export function idleRelease(busy, onRelease, ms = 4000) {
  clearInterval(idleTimer);
  idleTimer = setInterval(() => {
    if (busy() || Date.now() - idleLast < ms) return;
    let closed = 0;
    for (const m of IDLE_ACC) {
      for (const p of [...m.inputs.values(), ...m.outputs.values()]) {
        if (p.connection !== "open" && p.connection !== "pending") continue;
        try { if (p.type === "input") p.onmidimessage = null; p.close(); closed++; } catch (_) { /* unplugged */ }
      }
    }
    if (closed && onRelease) onRelease(closed);
  }, 1000);
}
