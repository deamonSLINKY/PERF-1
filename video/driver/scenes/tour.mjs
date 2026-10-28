// The whole tour in one take: one device, one piece, from power-on to the credits (a different piece only for the genre tour).
// A few settings are made beforehand, out of sight (the firmware keeps them in its flash): the snips' level, the FLUX visualizer.
import { Emu } from "../emu.mjs";
import { text, cam, ring, readout, at, tapB, tapK, turnK, fxSettings } from "./lib.mjs";

const G = ["HYPNO", "LOFI", "HOUSE", "DUBSTEP", "DUB", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP", "DISCO", "FUNK", "REGGAE", "SYNTHWAVE", "AMAPIANO", "TRIPHOP"];
const BPM = ["118-138", "70-94", "118-126", "136-150", "64-76", "170-178", "100-135", "126-138", "118-130", "130-150", "112-126", "96-110", "70-84", "100-118", "108-115", "70-88"];
const NAMES = ["BT REPT", "HF REPT", "BT REV", "SKIP", "TRANCE", "GATE 8", "DOUBLE", "SHUFFLE", "BCK-FWD", "ROLL OUT", "BUILD", "SCRATCH", "DROPOUT",
  "JUMP BK", "HALVES", "SWAP", "MIRROR", "LAG", "SLOWMO", "SKIP 3", "STUT UP", "STUT DN", "TRESILLO", "OFFBEAT", "CUT 4", "REV 8", "HALF GAP"];
const PAGES = [
  ["Tempo", "Tempo, evolve, motion, and how long a chord lasts"], ["Pad", "A pad with 24 sounds"], ["Pad env", "An envelope you can draw"], ["Pad LFO", "An LFO for filter, level, pan or pitch"],
  ["Chords", "Triads, sevenths, ninths, sus, dub, fourths"], ["Bass", "24 bass sounds"], ["Bass env", "Bass envelope"], ["Bass LFO", "Wobble it"],
  ["Bass line", "The bass as a line: root, kick, walk, riff"], ["Acid", "A 303-style acid line, 31 sounds"], ["Acid env", "Accent, slide, decay"], ["Acid line", "Density, length, range, mutate"],
  ["Acid answer", "Call and response: octave, fifth, mirror, sparse"], ["Acid steps", "Edit the line step by step, and freeze it"], ["Pluck", "12 plucks, 30 DX7-style voices, and yours"], ["Pluck 2", "Arps: up, down, random, thirds"],
  ["Deja vu", "The pluck remembers, and brings it round again"], ["Drums", "Pattern, drive, swing"], ["Links", "The kick ducks the bass, the snare opens the pad"], ["Kit", "37 kits, every drum tunable"],
  ["Drum drift", "Patterns that change themselves"], ["Groove", "Per-lane density and source"], ["Steps", "Ratchets, push, ghost and accent on every step"], ["Feel", "Ghost notes, accents, comping, humanize"],
  ["Phrase", "Pattern chains, slices, half and double time"], ["Space", "Tempo-synced echo and reverb sends"], ["Reverb", "Plate, shimmer, gated"], ["Comp/Limit", "The master bus"],
  ["Mixer", "Level, pan and send for every layer"], ["Global", "Root, theme, tune, store the piece"], ["Jam", "Play along with no wrong notes"],
];

async function holdKey(s, k, ms, name, sub) {
  ring(s, "key:" + k, ms / 1000 + 0.3, ""); readout(s, name, sub, ms / 1000 + 0.6);
  s.key(k, true); await s.wait(ms); s.key(k, false);
}

export default {
  seed: 0, fps: 30,

  async prepare(wasm) {                                        // out of sight: the snips' level, the FLUX visualizer
    const e = await Emu.create({ wasmPath: wasm, seed: 0 });
    await e.wait(3600);
    await fxSettings(e, 21); await e.wait(500);
    await e.turn("KNOB4", -40, 60); await e.wait(500);          // the snips' level down
    await e.tap("FX"); await e.wait(500);
    await e.tap("HOME", 60); await e.wait(120); await e.tap("HOME", 60); await e.wait(800);   // the visualizers (PLASMA first)
    await e.turn("SELECT", 3, 300); await e.wait(800);           // FLUX
    await e.tap("HOME"); await e.wait(1500);                     // back
    await e.wait(6000);                                          // (the settings are written to flash a moment after)
    const store = e.dumpStore();
    if (process.env.TOUR_DEBUG) console.log("prepared: writes", e.ex.web_store_writes(), Object.entries(store).map(([k, v]) => k + ":" + v.length).join(" "));
    return { store };
  },

  async run(s) {
    s.start(30);                                               // from power-on
    // ---- 1. switch on
    cam(s, "wide");
    text(s, { label: "SWITCH ON", title: "Music in four seconds", body: "No menus, no project to load. It boots straight into a piece that is already playing.", chips: [] });
    await at(s, 4800);
    text(s, { label: "MADE FOR PLAYING", title: "Easy, then deep", body: "A first jam in a minute. And when you want to dig, every page, effect and step is yours.", chips: [] });
    await at(s, 9800);
    text(s, { label: "TRY IT", title: "Tap and it answers", body: "The chord moves on at the next bar.", chips: ["TAP", "*EDIT"] });
    await at(s, 10600);
    await tapB(s, "EDIT", "EDIT");
    await at(s, 14500);
    text(s, { label: "TRY IT", title: "Tap keys, change the scale", body: "Each key adds or removes its note. There are no wrong ones to find.", chips: ["TAP", "*KEYS"] });
    await at(s, 15400);
    for (const k of [3, 8, 10]) { await tapK(s, k, 140); await s.wait(500); }
    await s.wait(1400);

    // ---- 2. drum drift
    text(s, { label: "PERFORM", title: "Drums that change themselves", body: "Drum drift: the pattern moves on every few bars, and a fill leads into it.", chips: ["TAP", "*SEQ", "four times"] });
    await s.wait(800);
    for (let i = 0; i < 4; i++) { await tapB(s, "SEQ", "SEQ"); await s.wait(250); }
    await s.wait(500);
    text(s, { label: "PERFORM", title: "Drums that change themselves", body: "Change: next. Every: 4 bars. Turn it on and leave it.", chips: ["turn", "*KNOB1", "*KNOB2"] });
    await turnK(s, "KNOB1", 1, 150, "KNOB1"); await s.wait(500);
    await turnK(s, "KNOB2", -1, 150, "KNOB2"); await s.wait(1400);
    await s.tap("HOME"); await s.wait(500);                    // the Tempo page

    // ---- 3. the auto-DJ: set up and switch on
    text(s, { label: "AUTO-DJ", title: "It runs the show", body: "Groove, fill, breakdown, build, drop. You set how, then you play over it.", chips: ["*FX", "twice with", "*SEL"] });
    ring(s, "btn:FX", 1.2, "FX");
    await fxSettings(s, 18);                                   // the Auto DJ page
    await s.wait(900);
    ring(s, "knob:KNOB2", 1.4, ""); readout(s, "GROOVE: SYNC", "FOLLOWS THE DRIFT TIME", 2.8);
    await s.turn("KNOB2", 5, 150); await s.wait(1200);
    ring(s, "knob:KNOB3", 1.4, ""); readout(s, "BREAK: SHORT", "4 BARS", 2.4);
    await s.turn("KNOB3", -2, 150); await s.wait(1200);
    await s.tap("FX"); await s.wait(600);
    text(s, { label: "AUTO-DJ", title: "Drift sets the groove", body: "The Drift time on the Tempo page is how long a groove lasts.", chips: ["turn", "*KNOB4"] });
    ring(s, "knob:KNOB4", 2.4, "KNOB4");
    await s.turn("KNOB4", -30, 70); await s.wait(900);
    await s.tap("HOME"); await s.wait(900);                    // the Perform screen
    text(s, { label: "AUTO-DJ", title: "Switch it on", body: "Hold GLO and tap F5. DJ lights in the header.", chips: ["hold", "*GLO", "+", "*F5"] });
    ring(s, "btn:GLO", 1.4, "GLO"); ring(s, "key:24", 1.2, "F5");
    s.press("GLO"); await s.wait(500); await s.tapKey(24); await s.wait(200); s.release("GLO"); await s.wait(1200);

    // ---- 4. the Perform screen and the mood, while the groove runs
    text(s, { label: "PERFORM", title: "The Perform screen", body: "The section, the eight tracks, the scenes, what is on, and the mood: all on one screen, all on the keys.", chips: ["tap", "*HOME", "again"] });
    await s.wait(4200);
    text(s, { label: "PERFORM", title: "One knob: the mood", body: "From chill to peak. Acid, hats, pluck, bass and reverb move together.", chips: ["turn", "*ALGORITHM"] });
    ring(s, "knob:ALGORITHM", 15, "ALGORITHM");
    readout(s, "CHILL", "", 5);
    await s.turn("ALGORITHM", -25, 110); await s.wait(700);
    readout(s, "BACK TO MIDDLE", "", 2.5);
    await s.turn("ALGORITHM", 25, 60); await s.wait(400);
    readout(s, "PEAK", "", 6);
    await s.turn("ALGORITHM", 25, 130); await s.wait(1200);
    await s.turn("ALGORITHM", -25, 40); await s.wait(500);
    text(s, { label: "PERFORM", title: "Mutes and solos on the keys", body: "Drop the hats, bring them back.", chips: ["tap", "*KEY"] });
    await s.wait(800);
    await tapK(s, 2, 120, ""); await s.wait(2200);
    await tapK(s, 2, 120, ""); await s.wait(800);

    // ---- 5. the fill, the break, the build, the drop
    text(s, { label: "AUTO-DJ", title: "The groove runs its course", body: "Watch the header count the bars. The fill, the break, the build and the drop come on their own.", chips: [] });
    if (!(await s.waitLit("SAVE", true, 90000))) throw new Error("no break");
    text(s, { label: "AUTO-DJ", title: "The break", body: "The bar before thinned out. Now the mix darkens and the pad rises.", chips: [] });
    readout(s, "BREAK", "SHORT: 4 BARS", 7);
    await s.wait(7500);
    text(s, { label: "AUTO-DJ", title: "The build", body: "A snare climbing a fifth, the bass opening, a breath of silence before...", chips: [] });
    readout(s, "BUILD", "", 7);
    await s.waitLit("SAVE", false, 20000);
    text(s, { label: "AUTO-DJ", title: "The drop", body: "Kick, clap and a crash on the one, and home to the tonic chord.", chips: [] });
    readout(s, "DROP", "", 3);
    await s.wait(4500);
    ring(s, "btn:GLO", 1.4, ""); ring(s, "key:24", 1.2, "");
    s.press("GLO"); await s.wait(400); await s.tapKey(24); await s.wait(200); s.release("GLO"); await s.wait(800);   // the DJ off
    await s.tap("HOME"); await s.wait(700);                    // Tempo

    // ---- 6. the master bus
    text(s, { label: "THE MASTER BUS", title: "Compressor and limiter", body: "Stereo-linked, with a soft knee. The limiter sees the peak coming and holds the ceiling.", chips: ["turn", "*SELECT"] });
    await s.wait(1000);
    ring(s, "knob:SELECT", 2.4, "SELECT");
    await s.turn("SELECT", 27, 70); await s.wait(1200);
    text(s, { label: "THE MASTER BUS", title: "Ratio, threshold, make-up, ceiling", body: "The page draws the curve, with live meters for the compressor, the limiter and the output.", chips: ["*KNOB1", "*KNOB2", "*KNOB3", "*KNOB4"] });
    await s.wait(1500);
    await turnK(s, "KNOB1", 5, 260, "KNOB1"); await s.wait(1200);
    readout(s, "RATIO", "UP TO 10:1", 2.5);
    await turnK(s, "KNOB2", -28, 90, "KNOB2"); await s.wait(1500);
    readout(s, "THRESHOLD", "DOWN", 2.5);
    await turnK(s, "KNOB3", 8, 170, "KNOB3"); await s.wait(1200);
    await turnK(s, "KNOB4", -70, 25, "KNOB4"); await s.wait(2600);
    readout(s, "CEILING", "THE LIMITER HOLDS IT", 3);
    await turnK(s, "KNOB1", -5, 150, "KNOB1"); await s.wait(600);
    await turnK(s, "KNOB4", 70, 20, "KNOB4"); await s.wait(600);
    await turnK(s, "KNOB2", 28, 25, "KNOB2"); await turnK(s, "KNOB3", -8, 40, "KNOB3"); await s.wait(800);
    await s.tap("HOME"); await s.wait(600);

    // ---- 7. effects, and editing them
    text(s, { label: "EFFECTS", title: "Tap latches, hold plays", body: "The FX button turns every key into an effect, colour coded by kind.", chips: ["tap", "*FX"] });
    await s.wait(600);
    await tapB(s, "FX", "FX"); await s.wait(900);
    await tapK(s, 1, 120); readout(s, "GATE 16", "LATCHED", 2.5); await s.wait(2600);
    await tapK(s, 1, 120); await s.wait(500);                  // off again
    ring(s, "key:8", 1.8, ""); s.key(8, true); readout(s, "PHASER", "HELD", 1.8); await s.wait(1800); s.key(8, false); await s.wait(500);
    text(s, { label: "EFFECTS", title: "Every one is editable", body: "Hold SEL and tap its key: a page that draws it, and the effect plays while you edit.", chips: ["*SEL", "+", "*KEY"] });
    ring(s, "btn:SEL", 1.5, "SEL");
    s.press("SEL"); await s.wait(200); await s.tapKey(1, 150); s.release("SEL"); await s.wait(1500);
    text(s, { label: "EFFECTS", title: "Edit the gate by hand", body: "The white keys are its steps. Tap them, and the pattern is yours.", chips: ["tap", "*KEYS"] });
    for (const k of [2, 4, 6, 9, 11]) { await tapK(s, k, 120); await s.wait(260); }
    await s.wait(1200);
    text(s, { label: "EFFECTS", title: "Then shape it", body: "Pattern, length, which tracks it gates, how deep.", chips: ["*KNOB1", "*KNOB2", "*KNOB3", "*KNOB4"] });
    await turnK(s, "KNOB1", 3, 260, "KNOB1"); await s.wait(500);
    await turnK(s, "KNOB2", -4, 200, "KNOB2"); await s.wait(500);
    await turnK(s, "KNOB3", 2, 300, "KNOB3"); await s.wait(1500);
    text(s, { label: "EFFECTS", title: "The dial walks all of them", body: "Phaser, flanger, tape, roll, pump, riser, scatter, brake, and more.", chips: ["turn", "*SELECT"] });
    for (const [n, k] of [[1, "KNOB1"], [5, "KNOB2"], [1, "KNOB1"], [1, "KNOB3"]]) {
      ring(s, "knob:SELECT", 1, "SELECT");
      await s.turn("SELECT", n, 160); await s.wait(1300);
      await turnK(s, k, 3, 200, k); await s.wait(1100);
    }
    await s.tap("FX"); await s.wait(500);                      // out of the page
    await s.tap("FX"); await s.wait(400);                      // the menu
    await s.tap("OCT-"); await s.wait(300);                    // everything off
    await s.tap("FX"); await s.wait(700);

    // ---- 8. beat FX and snips
    text(s, { label: "BEAT FX", title: "Beat repeat and friends", body: "The second bank: repeats from a half note to a 32nd, scatter, brake, spin back, half time, rolls.", chips: ["*FX", "then turn", "*SELECT"] });
    await s.wait(600);
    await tapB(s, "FX", "FX"); await s.wait(400);
    await s.turn("SELECT", -3, 60); await s.wait(300);         // the first bank, wherever it was
    ring(s, "knob:SELECT", 1, "SELECT"); await s.turn("SELECT", 1, 100); await s.wait(1200);
    text(s, { label: "BEAT FX", title: "Hold a key", body: "The last moment of the mix repeats on the grid, and lets go clean.", chips: ["hold", "*KEY"] });
    await holdKey(s, 3, 1500, "BEAT REPEAT", "1/8"); await s.wait(900);
    await holdKey(s, 5, 1500, "BEAT REPEAT", "1/32"); await s.wait(900);
    await holdKey(s, 9, 1500, "SCATTER", ""); await s.wait(900);
    await holdKey(s, 10, 900, "BRAKE", ""); await s.wait(1300);
    await holdKey(s, 22, 1500, "SNARE ROLL", ""); await s.wait(900);
    text(s, { label: "DJ SNIPS", title: "Horns, sirens, risers", body: "The fourth bank: twenty-seven sounds a DJ throws in, made on the spot.", chips: ["turn", "*SELECT"] });
    ring(s, "knob:SELECT", 2, "SELECT"); await s.turn("SELECT", 2, 150); await s.wait(1000);
    text(s, { label: "DJ SNIPS", title: "Play them like pads", body: "Up to four at once, with pitch and echo to taste.", chips: ["hold", "*KEY"] });
    await s.wait(500);
    await holdKey(s, 15, 700, "WHOOP", ""); await s.wait(900);
    await holdKey(s, 3, 1600, "RAVE HORN", ""); await s.wait(1400);
    await s.tap("FX"); await s.wait(800);

    // ---- 9. flex beat
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "Twenty-seven one-bar patterns that rework the last stretch of the music on the grid.", chips: [] });
    await s.wait(1800);
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "The third bank of the FX menu: turn SELECT twice. One pattern on every key.", chips: ["tap", "*FX", "turn", "*SELECT", "twice"] });
    await tapB(s, "FX", "FX"); await s.wait(300);
    await s.turn("SELECT", -3, 60); await s.wait(400);
    await s.wait(900);
    ring(s, "knob:SELECT", 1.4, "SELECT");
    s.spin("SELECT", 1); await s.wait(250); s.spin("SELECT", 1); await s.wait(1500);
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "Tap a key: it latches and waits for the bar. Hold it: the bar repeats until you let go.", chips: ["tap", "*KEY", "latch · hold", "*KEY", "repeat"] });
    for (const [k, hold, held, gap] of [[20, 120, 0, 3300], [22, 120, 0, 3300], [0, 2600, 1, 3500]]) {
      ring(s, "key:" + k, held ? 3.2 : 1.6, "KEY");
      readout(s, NAMES[k], held ? "HOLDING" : "LATCHED", held ? 3 : 2.5);
      s.key(k, true); await s.wait(hold); s.key(k, false); await s.wait(gap - hold);
    }
    await s.tap("FX"); await s.wait(900);

    // ---- 10. genres
    text(s, { label: "GENRES", title: "Sixteen genres", body: "Hold GLO and turn PRESETS. Each brings its own drums, sounds, chords, effects and compressor.", chips: ["hold", "*GLO", "turn", "*PRESETS"] });
    readout(s, G[0], "BPM " + BPM[0], 3);
    await s.wait(3000);
    for (let i = 1; i < 16; i++) {
      if (i === 9) { await s.turn("PRESETS", -2, 200); await s.wait(300); }          // back to piece 1 after Electro's 03
      ring(s, "btn:GLO", 1.1, "GLO"); ring(s, "knob:PRESETS", 1.1, "PRESETS");
      s.press("GLO"); await s.wait(350); s.spin("PRESETS", 1); await s.wait(450); s.release("GLO");
      if (i === 8) {                                                     // Electro: piece 3
        await s.wait(300); ring(s, "knob:PRESETS", 1.0, "PRESETS"); await s.turn("PRESETS", 2, 220); await s.wait(300);
        readout(s, "ELECTRO", "E03 · BPM " + BPM[i], 3); await s.wait(2500);
      } else { readout(s, G[i], "BPM " + BPM[i], 2.7); await s.wait(2350); }
    }

    // ---- 11. and much, much more
    text(s, { label: "AND", title: "...much, much more", body: "Thirty-one pages, a step sequencer, scenes, backup and restore, your own DX7 voices, six skins, visualizers.", chips: [] });
    await s.wait(2200);
    for (let i = 1; i < PAGES.length; i++) {
      ring(s, "knob:SELECT", 0.6, "");
      s.spin("SELECT", 1);
      text(s, { label: "AND", title: PAGES[i][0], body: PAGES[i][1], chips: [] });
      await s.wait(660);
    }
    await s.wait(900);

    // ---- 12. credits over the FLUX visualizer (chosen beforehand)
    await s.tap("HOME", 60); await s.wait(120); await s.tap("HOME", 60); await s.wait(1500);
    s.mark("credits", {});
    await s.wait(22000);
  },
};
