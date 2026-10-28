// The effects, and editing them: every effect has a page that draws it.
import { text, cam, ring, readout, at, tapB, tapK, turnK } from "./lib.mjs";
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "btns");
    text(s, { label: "EFFECTS", title: "Tap latches, hold plays", body: "The FX button turns every key into an effect, colour coded by kind.", chips: ["tap", "*FX"] });
    await s.wait(700);
    await tapB(s, "FX", "FX"); await s.wait(900);
    cam(s, "keys");
    await tapK(s, 1, 120); readout(s, "GATE 16", "LATCHED", 2.5); await s.wait(2400);
    await tapK(s, 24, 120); readout(s, "LOW-PASS", "LATCHED", 2.5); await s.wait(2000);
    ring(s, "key:8", 1.8, ""); s.key(8, true); readout(s, "PHASER", "HELD", 1.8); await s.wait(1800); s.key(8, false); await s.wait(300);
    cam(s, "btns");
    text(s, { label: "EFFECTS", title: "Every one is editable", body: "Hold SEL and tap its key: a page that draws it, and the effect plays while you edit.", chips: ["*SEL", "+", "*KEY"] });
    ring(s, "btn:SEL", 1.5, "SEL");
    s.press("SEL"); await s.wait(200); await s.tapKey(1, 150); s.release("SEL"); await s.wait(1500);          // the Gate's page
    cam(s, "keys");
    text(s, { label: "EFFECTS", title: "Edit the gate by hand", body: "The white keys are its steps. Tap them, and the pattern is yours.", chips: ["tap", "*KEYS"] });
    for (const k of [2, 4, 6, 9, 11]) { await tapK(s, k, 120); await s.wait(260); }
    await s.wait(1200);
    cam(s, "knobs");
    text(s, { label: "EFFECTS", title: "Then shape it", body: "Pattern, length, which tracks it gates, how deep.", chips: ["*KNOB1", "*KNOB2", "*KNOB3", "*KNOB4"] });
    await turnK(s, "KNOB1", 3, 260, "KNOB1"); await s.wait(500);
    await turnK(s, "KNOB2", -4, 200, "KNOB2"); await s.wait(500);
    await turnK(s, "KNOB3", 2, 300, "KNOB3"); await s.wait(1500);
    cam(s, "knobL");
    text(s, { label: "EFFECTS", title: "The dial walks all of them", body: "Phaser, flanger, tape, roll, pump, riser, scatter, brake, and more.", chips: ["turn", "*SELECT"] });
    for (const [n, k] of [[1, "KNOB1"], [5, "KNOB2"], [1, "KNOB1"], [1, "KNOB3"]]) {   // Phaser, then Tape (5 on), Roll (1 on), Pump (1 on)
      ring(s, "knob:SELECT", 1, "SELECT");
      await s.turn("SELECT", n, 160); await s.wait(1500);
      cam(s, "knobs");
      await turnK(s, k, 3, 200, k); await s.wait(1200);
      cam(s, "knobL");
    }
    await at(s, 40000);
  },
};
