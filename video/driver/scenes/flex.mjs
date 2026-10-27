// The proof of concept: Flex Beat, the FX menu's third bank. 20 s.
// Times are ms from the start of the recording; mark() leaves the cues the video's overlays follow.
const NAMES = ["BT REPT", "HF REPT", "BT REV", "SKIP", "TRANCE", "GATE 8", "DOUBLE", "SHUFFLE", "BCK-FWD", "ROLL OUT", "BUILD", "SCRATCH", "DROPOUT",
  "JUMP BK", "HALVES", "SWAP", "MIRROR", "LAG", "SLOWMO", "SKIP 3", "STUT UP", "STUT DN", "TRESILLO", "OFFBEAT", "CUT 4", "REV 8", "HALF GAP"];

export default {
  seed: 0,
  fps: 30,
  seconds: 20,
  async run(s) {
    await s.wait(3600);                         // power-on cards, then the first piece (not recorded)
    s.start(30);
    const at = async (ms) => { const d = ms - s.time * 1000; if (d > 0) await s.wait(d); };
    s.mark("scene", { name: "intro" });
    await at(2500);
    s.mark("press", { what: "FX" });
    await s.tap("FX");
    await at(3600);
    s.mark("turn", { what: "SELECT", n: 2 });
    s.spin("SELECT", 1); await s.wait(250); s.spin("SELECT", 1);
    await at(5200);
    s.mark("bank", { name: "FLEX BEAT" });
    for (const [t, k, hold, hi] of [[6000, 20, 120, 0], [9500, 22, 120, 0], [13000, 11, 2600, 1]]) {
      await at(t);
      s.mark("flex", { key: k, name: NAMES[k], hold: hi });
      s.key(k, true);
      await s.wait(hold);
      s.key(k, false);
    }
    await at(20000);
  },
};
