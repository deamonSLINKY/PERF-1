// The auto-DJ: set up on its settings page (sync to the drift time, a short break), switch it on, cut ahead to the fill, the break, the build and the drop.
import { text, cam, ring, readout, at, tapB, tapK, turnK, fxSettings } from "./lib.mjs";

async function setup(s) {                                  // the device's moves (the same in the look-ahead run)
  await s.wait(3600);
  await s.wait(500);
  await fxSettings(s, 18);                                 // the Auto DJ page
  await s.wait(900);
  await s.turn("KNOB2", 5, 150); await s.wait(700);        // groove: sync
  await s.turn("KNOB3", -2, 150); await s.wait(900);       // break: short
  await s.tap("FX"); await s.wait(500);
  await s.turn("KNOB4", -30, 70); await s.wait(800);       // drift: the shortest
  await s.tap("HOME"); await s.wait(900);                  // perform
  s.press("GLO"); await s.wait(500); await s.tapKey(24); await s.wait(200); s.release("GLO"); await s.wait(300);
}

export default {
  seed: 0, fps: 30,
  async run(s) {
    const probe = await s.clone();
    await setup(probe);
    const T = await probe.runUntilLit("SAVE", 150000);    // ms after the setup when the break begins
    if (T < 0) throw new Error("no break found");
    // the same moves again, recorded, with cues
    await s.wait(3600);                                    // (the power-on cards are not shown here)
    s.start(30);
    cam(s, "btns");
    text(s, { label: "AUTO-DJ", title: "It runs the show", body: "Groove, fill, breakdown, build, drop. You set how, then you play over it.", chips: [] });
    await s.wait(500);
    text(s, { label: "AUTO-DJ", title: "Set it up", body: "Groove on Sync follows the drift time. Break on Short.", chips: ["*FX", "twice with", "*SEL"] });
    ring(s, "btn:FX", 1.2, "FX");
    await fxSettings(s, 18);
    cam(s, "knobs");
    await s.wait(900);
    ring(s, "knob:KNOB2", 1.4, ""); readout(s, "GROOVE: SYNC", "", 2.2);
    await s.turn("KNOB2", 5, 150); await s.wait(700);
    ring(s, "knob:KNOB3", 1.4, ""); readout(s, "BREAK: SHORT", "", 2.2);
    await s.turn("KNOB3", -2, 150); await s.wait(900);
    cam(s, "btns");
    await s.tap("FX"); await s.wait(500);
    text(s, { label: "AUTO-DJ", title: "Drift sets the groove", body: "The Drift time on the Tempo page is how long a groove lasts.", chips: ["turn", "*KNOB4"] });
    cam(s, "knobs");
    ring(s, "knob:KNOB4", 2.2, "KNOB4");
    await s.turn("KNOB4", -30, 70); await s.wait(800);
    cam(s, "btns");
    await s.tap("HOME"); await s.wait(900);
    text(s, { label: "AUTO-DJ", title: "Switch it on", body: "Hold GLO and tap F5. DJ lights in the header.", chips: ["hold", "*GLO", "+", "*F5"] });
    ring(s, "btn:GLO", 1.4, "GLO"); ring(s, "key:24", 1.2, "F5");
    s.press("GLO"); await s.wait(500); await s.tapKey(24); await s.wait(200); s.release("GLO"); await s.wait(300);
    const t1 = s.time * 1000;
    await s.wait(2500);
    cam(s, "screen");
    text(s, { label: "AUTO-DJ", title: "...and a groove later", body: "The fill, the break, the build and the drop come on their own.", chips: [] });
    s.skip(T - 7000 - 2800);                                // a cut: to seven seconds before the break
    await s.wait(3600);
    text(s, { label: "AUTO-DJ", title: "The fill, then the break", body: "The bar before it thins out. The break darkens the mix and the pad rises.", chips: [] });
    await s.wait(3600);
    readout(s, "BREAK", "SHORT: 4 BARS", 6);
    await s.wait(7500);
    text(s, { label: "AUTO-DJ", title: "The build", body: "A snare climbing a fifth, the bass opening, a breath of silence before...", chips: [] });
    readout(s, "BUILD", "", 7);
    await s.wait(7600);
    text(s, { label: "AUTO-DJ", title: "The drop", body: "Kick, clap and a crash on the one, and back home to the tonic chord.", chips: [] });
    readout(s, "DROP", "", 3);
    await s.wait(5200);
  },
};
