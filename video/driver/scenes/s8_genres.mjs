// The genre tour: GLO + PRESETS through all sixteen banks, a few seconds each.
import { text, cam, ring, readout, at } from "./lib.mjs";
const G = ["HYPNO", "LOFI", "HOUSE", "DUBSTEP", "DUB", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP", "DISCO", "FUNK", "REGGAE", "SYNTHWAVE", "AMAPIANO", "TRIPHOP"];
const BPM = ["118-138", "70-94", "118-126", "136-150", "64-76", "170-178", "100-135", "126-138", "118-130", "130-150", "112-126", "96-110", "70-84", "100-118", "108-115", "70-88"];
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "knobL");
    text(s, { label: "GENRES", title: "Sixteen genres", body: "Hold GLO and turn PRESETS. Each brings its own drums, sounds, chords, effects and compressor.", chips: ["hold", "*GLO", "turn", "*PRESETS"] });
    readout(s, G[0], "BPM " + BPM[0], 3.2);
    await s.wait(3300);
    for (let i = 1; i < 16; i++) {
      ring(s, "btn:GLO", 1.1, "GLO"); ring(s, "knob:PRESETS", 1.1, "PRESETS");
      s.press("GLO"); await s.wait(350); s.spin("PRESETS", 1); await s.wait(450); s.release("GLO");
      readout(s, G[i], "BPM " + BPM[i], 3.0);
      await s.wait(2900);
    }
  },
};
