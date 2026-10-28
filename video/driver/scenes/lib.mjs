// helpers the scenes share: cues for the overlays, and the moves of the panel
export const text = (s, o) => s.mark("text", o);
export const ring = (s, target, dur = 1, chip) => s.mark("ring", { target, dur, chip });
export const cam = (s, to) => s.mark("cam", { to });
export const readout = (s, t, sub, dur = 3) => s.mark("readout", { text: t, sub, dur });

export async function at(s, ms) { const d = ms - s.time * 1000; if (d > 0) await s.wait(d); }
export async function tapB(s, name, chip, hold = 90) { ring(s, "btn:" + name, 0.9, chip === undefined ? name : chip); await s.tap(name, hold); }
export async function holdB(s, name, ms, chip) { ring(s, "btn:" + name, ms / 1000 + 0.4, chip === undefined ? name : chip); s.press(name); await s.wait(ms); s.release(name); await s.wait(80); }
export async function tapK(s, k, hold = 120, chip) { ring(s, "key:" + k, 0.9, chip); await s.tapKey(k, hold); }
export async function turnK(s, knob, n, gap = 110, chip) { ring(s, "knob:" + knob, Math.abs(n) * gap / 1000 + 0.5, chip === undefined ? knob : chip); await s.turn(knob, n, gap); }

// open the FX menu's settings pages (SEL + FX), then walk the dial to page n (0 = Gate)
export async function fxSettings(s, n = 0) {
  await s.tap("FX"); await s.wait(250);
  s.press("SEL"); await s.wait(100); s.press("FX"); await s.wait(150); s.release("FX"); s.release("SEL");
  await s.wait(400);
  if (n) await s.turn("SELECT", n, 110);
  await s.wait(300);
}
