// Beat FX (repeats, scatter, brake) and DJ snips (horns, sirens, risers).
import { text, cam, ring, readout, at, tapB, turnK } from "./lib.mjs";
const SNIPS = ["AIR HORN", "AIR BLAST", "HORN MIN", "RAVE HORN", "SIREN", "SIREN 2", "SIREN 3", "LASER", "LASER LO", "ZAP", "ZAP HI", "BEEP", "BEEP LO", "BEEP HI", "2 BEEP", "WHOOP", "WHOOP 2",
  "DUB SIREN", "DUB LOW", "DROP", "DROP LONG", "RISER", "REWIND", "CRASH", "RAVE STAB", "BELL", "SNR ROLL"];
const BEAT = { 1: "1/4", 3: "1/8", 4: "1/16", 5: "1/32", 8: "SCATTER", 9: "BRAKE", 10: "SPIN BACK", 11: "HALF TIME", 19: "DRUM ROLL" };
async function holdKey(s, k, ms, name, sub) {
  ring(s, "key:" + k, ms / 1000 + 0.3, ""); readout(s, name, sub, ms / 1000 + 0.6);
  s.key(k, true); await s.wait(ms); s.key(k, false);
}
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "btns");
    text(s, { label: "BEAT FX", title: "Beat repeat and friends", body: "The second bank: repeats from a half note to a 32nd, scatter, brake, spin back, half time, rolls.", chips: ["*FX", "then turn", "*SELECT"] });
    await s.wait(600);
    await tapB(s, "FX", "FX"); await s.wait(500);
    cam(s, "knobL");
    ring(s, "knob:SELECT", 1, "SELECT"); await s.turn("SELECT", 1, 100); await s.wait(1200);
    cam(s, "keys");
    text(s, { label: "BEAT FX", title: "Hold a key", body: "The last moment of the mix repeats on the grid, and lets go clean.", chips: ["hold", "*KEY"] });
    await holdKey(s, 3, 1500, "BEAT REPEAT", "1/8"); await s.wait(900);
    await holdKey(s, 5, 1500, "BEAT REPEAT", "1/32"); await s.wait(900);
    await holdKey(s, 9, 1700, "SCATTER", ""); await s.wait(900);
    await holdKey(s, 10, 1800, "BRAKE", "LIKE A VINYL STOP"); await s.wait(1200);
    await holdKey(s, 22, 1700, "SNARE ROLL", ""); await s.wait(900);
    cam(s, "knobL");
    text(s, { label: "DJ SNIPS", title: "Horns, sirens, risers", body: "The fourth bank: twenty-seven sounds a DJ throws in, made on the spot.", chips: ["turn", "*SELECT"] });
    ring(s, "knob:SELECT", 2, "SELECT"); await s.turn("SELECT", 2, 150); await s.wait(1000);
    cam(s, "keys");
    text(s, { label: "DJ SNIPS", title: "Play them like pads", body: "Up to four at once, with pitch and echo to taste.", chips: ["hold", "*KEY"] });
    await s.wait(500);
    for (const [k, ms] of [[1, 500], [4, 1400], [9, 400], [15, 900], [24, 500], [21, 2200], [23, 700]]) { await holdKey(s, k, ms, SNIPS[k], ""); await s.wait(550); }
    await s.wait(1000);
    await at(s, 33000);
  },
};
