// Flex Beat: the third bank. (BT REPT instead of Scratch.)
import { text, cam, ring, readout, at, tapB } from "./lib.mjs";
const NAMES = ["BT REPT", "HF REPT", "BT REV", "SKIP", "TRANCE", "GATE 8", "DOUBLE", "SHUFFLE", "BCK-FWD", "ROLL OUT", "BUILD", "SCRATCH", "DROPOUT",
  "JUMP BK", "HALVES", "SWAP", "MIRROR", "LAG", "SLOWMO", "SKIP 3", "STUT UP", "STUT DN", "TRESILLO", "OFFBEAT", "CUT 4", "REV 8", "HALF GAP"];
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "btns");
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "Twenty-seven one-bar patterns that rework the last stretch of the music on the grid.", chips: [] });
    await at(s, 1800);
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "It is the third bank of the FX menu: turn SELECT twice. One pattern on every key.", chips: ["tap", "*FX", "turn", "*SELECT", "twice"] });
    await tapB(s, "FX", "FX");
    cam(s, "knobL");
    await at(s, 3300);
    ring(s, "knob:SELECT", 1.4, "SELECT");
    s.spin("SELECT", 1); await s.wait(250); s.spin("SELECT", 1);
    await at(s, 4800);
    cam(s, "keys");
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "Tap a key: it latches and waits for the bar. Hold it: the bar repeats until you let go.", chips: ["tap", "*KEY", "latch · hold", "*KEY", "repeat"] });
    for (const [t, k, hold, held] of [[6000, 20, 120, 0], [9500, 22, 120, 0], [13000, 0, 2600, 1]]) {
      await at(s, t);
      ring(s, "key:" + k, held ? 3.2 : 1.6, "KEY");
      readout(s, NAMES[k], held ? "HOLDING" : "LATCHED", held ? 3 : 2.5);
      s.key(k, true); await s.wait(hold); s.key(k, false);
    }
    await at(s, 18500);
  },
};
