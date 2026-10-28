// The master bus: a real compressor and a look-ahead limiter.
import { text, cam, ring, readout, at, turnK } from "./lib.mjs";
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "knobL");
    text(s, { label: "THE MASTER BUS", title: "Compressor and limiter", body: "Stereo-linked, with a soft knee. The limiter sees the peak coming and holds the ceiling.", chips: ["turn", "*SELECT"] });
    await s.wait(1200);
    ring(s, "knob:SELECT", 2.4, "SELECT");
    await s.turn("SELECT", 27, 70);                          // Tempo ... Comp/Limit
    await s.wait(1200);
    cam(s, "knobs");
    text(s, { label: "THE MASTER BUS", title: "Ratio, threshold, make-up, ceiling", body: "The page draws the curve, with live meters for the compressor, the limiter and the output.", chips: ["*KNOB1", "*KNOB2", "*KNOB3", "*KNOB4"] });
    await s.wait(1500);
    await turnK(s, "KNOB1", 5, 260, "KNOB1"); await s.wait(1200);       // ratio up
    readout(s, "RATIO", "UP TO 10:1", 2.5);
    await turnK(s, "KNOB2", -28, 90, "KNOB2"); await s.wait(1500);     // threshold down: it works harder
    readout(s, "THRESHOLD", "DOWN", 2.5);
    await turnK(s, "KNOB3", 8, 170, "KNOB3"); await s.wait(1200);       // make-up
    await turnK(s, "KNOB4", -70, 25, "KNOB4"); await s.wait(2600);     // ceiling: the limiter
    readout(s, "CEILING", "THE LIMITER HOLDS IT", 3);
    await turnK(s, "KNOB1", -5, 150, "KNOB1"); await s.wait(1000);
    await turnK(s, "KNOB4", 70, 20, "KNOB4"); await s.wait(1500);
    await at(s, 28000);
  },
};
