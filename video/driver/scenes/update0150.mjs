// The 0.15.0 update video in one take: the device as it ships, stopped, synthwave 1 chosen, the Synth theme, the intro set, Play; then
// everything new over that one piece, to the credits. Set beforehand, out of sight: the auto-DJ's groove (8 bars) and break (short), the
// FLUX visualizer for the credits.
import { Emu } from "../emu.mjs";
import { text, cam, ring, readout, at, tapB, tapK, turnK, fxSettings } from "./lib.mjs";

// the settings pages by dial position from the Gate (ui.c FX_ORDER): 6 TAPE, 19 AUTO DJ, 22 SET
const PG = { TAPE: 6, AUTODJ: 19, SET: 22 };

async function djToggle(s) {                                  // GLO + F5
  ring(s, "btn:GLO", 1.4, "GLO"); ring(s, "key:24", 1.2, "F5");
  s.press("GLO"); await s.wait(450); await s.tapKey(24); await s.wait(200); s.release("GLO"); await s.wait(600);
}

export default {
  seed: 0, fps: 30,

  async prepare(wasm) {
    const e = await Emu.create({ wasmPath: wasm, seed: 0 });
    await e.wait(3600);
    await fxSettings(e, PG.AUTODJ); await e.wait(500);
    await e.turn("KNOB2", -1, 150); await e.wait(300);          // groove: 8 bars
    await e.turn("KNOB3", -1, 150); await e.wait(300);          // break: short
    await e.tap("FX"); await e.wait(500);
    await e.tap("HOME", 60); await e.wait(120); await e.tap("HOME", 60); await e.wait(800);   // the visualizers (PLASMA first)
    await e.turn("SELECT", 3, 300); await e.wait(800);           // FLUX
    await e.tap("HOME"); await e.wait(1500);
    await e.wait(6000);                                          // (written to flash a moment after)
    const store = e.dumpStore();
    if (process.env.TOUR_DEBUG) console.log("prepared: writes", e.ex.web_store_writes(), Object.entries(store).map(([k, v]) => k + ":" + v.length).join(" "));
    return { store };
  },

  async run(s) {
    s.start(30);
    cam(s, "wide");
    // ---- 1. as it ships, then the stage is set on camera
    text(s, { label: "0.15.0", title: "The second update", body: "It boots playing, as always. This time we set the stage first.", chips: [] });
    await at(s, 5600);
    text(s, { label: "SET THE STAGE", title: "Stop", body: "", chips: ["tap", "*PLAY"] });
    await at(s, 6400);
    await tapB(s, "PLAY", "PLAY"); await s.wait(900);
    readout(s, "STOPPED", "", 2);
    text(s, { label: "SET THE STAGE", title: "Synthwave 1", body: "Hold GLO and turn PRESETS for the genre. One piece for the whole video.", chips: ["hold", "*GLO", "turn", "*PRESETS"] });
    await s.wait(600);
    ring(s, "btn:GLO", 5.2, "GLO"); ring(s, "knob:PRESETS", 5.0, "PRESETS");
    s.press("GLO"); await s.wait(500);
    await s.turn("PRESETS", 13, 300);                          // HYPNO -> SYNTHWAVE
    await s.wait(500); s.release("GLO"); await s.wait(800);
    readout(s, "SYNTHWAVE", "W01 · A AEOLIAN · 108", 3);
    await s.wait(1600);
    text(s, { label: "SET THE STAGE", title: "The Synth theme", body: "A look of its own now: a horned sun behind the sigil, a scanner in its centre, the imp in neon.", chips: ["*GLO", "then", "*KNOB2"] });
    await tapB(s, "GLO", "GLO"); await s.wait(700);
    await turnK(s, "KNOB2", 1, 200, "KNOB2"); await s.wait(600);
    readout(s, "THEME: SYNTH", "", 2.5);
    await s.tap("HOME"); await s.wait(2600);                   // the home: the night sky, the sun at rest
    text(s, { label: "SET THE STAGE", title: "The SET page", body: "New: the rounds a piece gets before the auto-DJ moves on, a tempo ramp over them, and an intro.", chips: ["*SEL", "+", "*FX", "turn", "*SELECT"] });
    ring(s, "btn:SEL", 1.2, "SEL"); ring(s, "btn:FX", 1.2, "FX");
    await fxSettings(s, PG.SET); await s.wait(1200);
    readout(s, "ROUNDS 2", "", 2); await turnK(s, "KNOB1", 1, 200, "KNOB1"); await s.wait(900);
    readout(s, "RAMP +4 BPM", "", 2); await turnK(s, "KNOB2", 2, 200, "KNOB2"); await s.wait(900);
    readout(s, "INTRO 1 MIN", "", 2.5); await turnK(s, "KNOB3", 1, 200, "KNOB3"); await s.wait(1400);
    await s.tap("FX"); await s.wait(1300);                   // (back on the home page: the night sky, the sun at rest)
    // ---- 2. Play: the intro
    text(s, { label: "INTRO", title: "Play", body: "The slow progression: from Play the tracks come in one by one. Mix in under a pad and a hat.", chips: ["tap", "*PLAY"] });
    await s.wait(700);
    await tapB(s, "PLAY", "PLAY");
    const t0 = s.time;
    readout(s, "INTRO", "THE PAD FIRST", 4);
    await s.wait(5000);
    readout(s, "HATS", "", 3);
    await s.wait(5000);
    text(s, { label: "THANK YOU", title: "Thank you", body: "To everyone who installed 0.14.0, wrote in, posted and asked for things: this update is mostly your ideas.", chips: [] });
    await s.wait(3000);
    readout(s, "PERC", "", 3);
    await s.wait(9000);
    readout(s, "KICK", "", 3);
    text(s, { label: "YOUR IDEAS", title: "Your ideas", body: "This intro was one of them. The chord loops, the pluck steps, the bass figures, the rounds: more of them.", chips: [] });
    await s.wait(7000);
    readout(s, "SNARE", "", 3);
    await s.wait(6000);
    readout(s, "BASS", "", 3);
    text(s, { label: "KEEP THEM COMING", title: "Keep them coming", body: "Issues on GitHub, the Firmware Hub, the comments. Tell me what the music should do next.", chips: [] });
    await s.wait(6000);
    readout(s, "PLUCK", "", 3);
    await s.wait(6000);
    readout(s, "ACID", "", 3);
    await s.wait(5000);
    // ---- 3. the pluck up a bit (the intro over)
    text(s, { label: "WHILE WE ARE HERE", title: "Let's turn the pluck up a bit", body: "The Pluck page: ARP six times. Level to 60.", chips: ["tap", "*ARP", "x6", "*KNOB1"] });
    await at(s, (t0 + 61) * 1000);
    for (let i = 0; i < 6; i++) { await tapB(s, "ARP", "ARP"); await s.wait(200); }
    await s.wait(900);
    readout(s, "PLUCK 60", "", 3.5);
    await turnK(s, "KNOB1", 30, 60, "KNOB1"); await s.wait(3000);
    await s.tap("HOME"); await s.wait(900);                    // back to Tempo
    // ---- 4. harmony
    text(s, { label: "HARMONY YOU CAN HEAR", title: "A chord every two bars", body: "Each genre changes chord every so many bars now: two in synthwave. Minor keys get a real dominant.", chips: ["*KNOB4"] });
    ring(s, "knob:KNOB4", 2.5, "KNOB4");
    readout(s, "DRIFT: 2 BARS", "THE HARMONIC RHYTHM", 4);
    await s.wait(6500);
    text(s, { label: "HARMONY YOU CAN HEAR", title: "A loop that starts from home", body: "Most genres loop a progression now: i, VI, III, VII over eight bars here. The Chords page's Loop.", chips: ["tap", "*ENV", "four times"] });
    await s.wait(600);
    for (let i = 0; i < 4; i++) { await tapB(s, "ENV", "ENV"); await s.wait(220); }
    await s.wait(800);
    ring(s, "knob:KNOB4", 2.5, "KNOB4"); readout(s, "LOOP: 4 CHORDS", "", 3.5);
    await s.wait(5500);
    // ---- 5. the pad
    text(s, { label: "A FULLER PAD", title: "A filter that breathes", body: "The saw sounds sweep a resonant filter with every breath and stab, with a sub under them.", chips: ["tap", "*ENV", "then", "*KNOB3"] });
    await tapB(s, "ENV", "ENV"); await s.wait(800);           // round to Pad
    readout(s, "TRANCE", "A SUPERSAW: FOUR SAWS", 4);
    await turnK(s, "KNOB3", -1, 200, "KNOB3"); await s.wait(7500);
    readout(s, "BRASS", "STABBED, THE FILTER OPEN", 4);
    await turnK(s, "KNOB3", 10, 60, "KNOB3"); await s.wait(7000);
    readout(s, "POLY", "BACK TO THE PIECE'S", 2.5);
    await turnK(s, "KNOB3", -9, 60, "KNOB3"); await s.wait(2000);
    // ---- 6. the bass line
    text(s, { label: "THE BASS LINE", title: "Nine new bass figures", body: "The Bass line page, Mode, after Riff: Octaves, Two-step, One-drop, Log drum, Skippy, Funk, Big beat, Dub, Gallop. In any genre.", chips: ["tap", "*LFO", "four times", "*KNOB1"] });
    for (let i = 0; i < 4; i++) { await tapB(s, "LFO", "LFO"); await s.wait(220); }
    await s.wait(800);
    for (const [n, name] of [[4, "OCTAVES"], [1, "TWO-STEP"], [2, "LOG DRUM"], [5, "GALLOP"]]) {
      readout(s, name, "", 4.2);
      await turnK(s, "KNOB1", n, 120, "KNOB1"); await s.wait(4600 - n * 120);
    }
    text(s, { label: "THE BASS LINE", title: "And it plays the fifth", body: "Where it played the seventh under ninth, dub and fourths chords. Back to the piece's eighths.", chips: ["*KNOB1"] });
    readout(s, "EIGHTHS", "", 2.5);
    await turnK(s, "KNOB1", -12, 60, "KNOB1"); await s.wait(3000);
    // ---- 7. the DY voices
    text(s, { label: "DY VOICES", title: "Human-made DX7 voices, by default", body: "Thirty of them, from public-domain banks, each under its own name. The pluck here is SHIMMERPAD.", chips: ["tap", "*ARP", "*KNOB2"] });
    await tapB(s, "ARP", "ARP"); await s.wait(1200);          // (ARP comes back to the page it left: the Pluck page)
    for (const [n, name] of [[-4, "TEMPLEBELL"], [2, "CLAVINET"], [1, "SOFTSTRNGS"], [1, "SHIMMERPAD"]]) {
      readout(s, name, "", 3.6);
      await turnK(s, "KNOB2", n, 150, "KNOB2"); await s.wait(3800 - Math.abs(n) * 150);
    }
    // ---- 8. pluck steps
    text(s, { label: "PLUCK STEPS", title: "Freeze the loop, write it", body: "Deja vu's loop as sixteen steps: a white key picks one, Rung and Level write it. Stored with the piece.", chips: ["tap", "*ARP", "three times"] });
    for (let i = 0; i < 3; i++) { await tapB(s, "ARP", "ARP"); await s.wait(200); }
    await s.wait(1500);
    readout(s, "FROZEN", "THE LOOP HOLDS", 3);
    ring(s, "key:" + [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26][4], 1.2, "KEY");
    await s.tapKey("w4"); await s.wait(300);
    await turnK(s, "KNOB2", 3, 160, "KNOB2"); await s.wait(2200);  // accent
    ring(s, "key:" + [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26][10], 1.2, "KEY");
    await s.tapKey("w10"); await s.wait(300);
    await turnK(s, "KNOB2", 3, 160, "KNOB2"); await s.wait(300);
    await turnK(s, "KNOB1", -3, 160, "KNOB1"); await s.wait(2200);
    ring(s, "key:" + [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26][13], 1.2, "KEY");
    await s.tapKey("w13"); await s.wait(300);
    await turnK(s, "KNOB2", 2, 160, "KNOB2"); await s.wait(300);
    await turnK(s, "KNOB1", 4, 160, "KNOB1"); await s.wait(3500);
    text(s, { label: "PLUCK STEPS", title: "And on MIDI", body: "The pluck goes out on channel 2 now, every note with its note-off, from every place a note starts.", chips: [] });
    await s.wait(4500);
    // ---- 9. the auto-DJ: rounds, the ramp, the drop
    await s.tap("HOME"); await s.wait(400);                    // Tempo
    await s.tap("HOME"); await s.wait(600);                    // Perform
    text(s, { label: "THE AUTO-DJ", title: "Rounds and the ramp", body: "The DJ gives this piece two rounds, the tempo climbing four beats over them. GLO + F5 starts it.", chips: ["hold", "*GLO", "+", "*F5"] });
    await s.wait(800);
    await djToggle(s);
    readout(s, "DJ ON", "ROUND 1 OF 2", 4);
    await s.wait(3000);
    text(s, { label: "THE AUTO-DJ", title: "The groove runs its course", body: "Eight bars, a fill, the break, the build and the drop, on their own.", chips: [] });
    if (!(await s.waitLit("SAVE", true, 60000))) throw new Error("no break");
    text(s, { label: "THE AUTO-DJ", title: "The break", body: "The bar before thinned out. The mix darkens and the pad rises.", chips: [] });
    readout(s, "BREAK", "", 5);
    await s.wait(4300);
    text(s, { label: "THE AUTO-DJ", title: "The build", body: "A snare climbing, the bass opening, a breath of silence, then...", chips: [] });
    readout(s, "BUILD", "", 6);
    await s.waitLit("SAVE", false, 30000);
    text(s, { label: "THE AUTO-DJ", title: "The drop", body: "Kick, clap and a crash on the one, home to the tonic chord. The second round begins a touch faster.", chips: [] });
    readout(s, "DROP", "", 3);
    await s.wait(5500);
    await djToggle(s);                                         // the DJ off
    await s.tap("HOME"); await s.wait(600);                    // Tempo
    // ---- 10. effects, editable
    text(s, { label: "EFFECTS", title: "Tap latches, hold plays", body: "The FX button turns every key into an effect. Hold SEL and tap a key: its page, and it plays while you edit.", chips: ["tap", "*FX"] });
    await s.wait(600);
    await tapB(s, "FX", "FX"); await s.wait(900);
    await tapK(s, 1, 120); readout(s, "GATE 16", "LATCHED", 2.5); await s.wait(4200);
    ring(s, "btn:SEL", 1.5, "SEL");
    s.press("SEL"); await s.wait(200); await s.tapKey(1, 150); s.release("SEL"); await s.wait(1500);
    text(s, { label: "EFFECTS", title: "The gate, by hand", body: "The white keys are its steps. Then depth, length, which tracks.", chips: ["tap", "*KEYS", "*KNOB4"] });
    for (const k of [2, 4, 6, 9, 11]) { await tapK(s, k, 120); await s.wait(260); }
    await s.wait(800);
    await turnK(s, "KNOB4", -3, 200, "KNOB4"); await s.wait(2500);
    text(s, { label: "EFFECTS", title: "The dial walks all of them", body: "Phaser, flanger, talk, wah, vinyl, tape, roll, pump, riser, scatter, brake and more.", chips: ["turn", "*SELECT"] });
    ring(s, "knob:SELECT", 1.6, "SELECT");
    await s.turn("SELECT", PG.TAPE, 160); await s.wait(1200);
    readout(s, "TAPE", "", 3);
    await turnK(s, "KNOB2", 4, 200, "KNOB2"); await s.wait(3500);
    await s.tap("FX"); await s.wait(500);                      // out of the page
    await s.tap("FX"); await s.wait(400);                      // the menu
    await s.tap("OCT-"); await s.wait(300);                    // everything off
    await s.tap("FX"); await s.wait(900);
    // ---- 11. a Flex Beat
    text(s, { label: "FLEX BEAT", title: "Flex Beat", body: "The third bank of the FX menu: twenty-seven one-bar patterns that rework the last stretch of the music on the grid.", chips: ["tap", "*FX", "turn", "*SELECT", "twice"] });
    await tapB(s, "FX", "FX"); await s.wait(300);
    await s.turn("SELECT", -3, 60); await s.wait(600);         // the first bank
    ring(s, "knob:SELECT", 1.4, "SELECT");
    s.spin("SELECT", 1); await s.wait(250); s.spin("SELECT", 1); await s.wait(1200);
    text(s, { label: "FLEX BEAT", title: "Tap: it waits for the bar", body: "Tresillo, then Beat repeat held until let go.", chips: ["tap", "*KEY", "hold", "*KEY"] });
    for (const [k, hold, held, gap, name] of [[22, 120, 0, 4800, "TRESILLO"], [20, 120, 0, 4800, "STUT UP"], [0, 2800, 1, 3800, "BT REPT"]]) {
      ring(s, "key:" + k, held ? 3.4 : 1.6, "KEY");
      readout(s, name, held ? "HOLDING" : "LATCHED", held ? 3.2 : 3);
      s.key(k, true); await s.wait(hold); s.key(k, false); await s.wait(gap - hold);
    }
    await s.tap("FX"); await s.wait(900);
    // ---- 12. and much, much more: the master bus first (not new, but worth the look), then a card a page, slow at first, faster
    text(s, { label: "AND", title: "...much, much more", body: "Starting with the master bus: a stereo compressor and a look-ahead limiter on every piece.", chips: ["turn", "*SELECT"] });
    await s.wait(800);
    ring(s, "knob:SELECT", 2.0, "SELECT");
    await s.turn("SELECT", 28, 60); await s.wait(1000);
    text(s, { label: "AND", title: "Compressor and limiter", body: "Ratio, threshold, make-up, ceiling. The page draws the curve, with live meters.", chips: ["*KNOB1", "*KNOB2", "*KNOB3", "*KNOB4"] });
    readout(s, "RATIO 6:1", "", 2.5); await turnK(s, "KNOB1", 2, 300, "KNOB1"); await s.wait(1000);
    readout(s, "THRESHOLD -23 dB", "", 2.5); await turnK(s, "KNOB2", -23, 60, "KNOB2"); await s.wait(1000);
    readout(s, "MAKE-UP +11.2 dB", "", 4.5); await turnK(s, "KNOB3", 62, 60, "KNOB3"); await s.wait(800);
    readout(s, "CEILING -10.1 dB", "THE LIMITER HOLDS", 6); await turnK(s, "KNOB4", -84, 60, "KNOB4"); await s.wait(3500);
    await s.turn("SELECT", -28, 25); await s.wait(600);        // back to Tempo
    {
      const MORE = [
        ["Pad", "Stabbing pads where the genre comps: every dub piece skanks"], ["Pad env", "Eleven pentatonic pieces moved to real modes"],
        ["Pad LFO", "The bass plays the fifth, not the seventh"], ["Chords", "A real dominant in minor keys"], ["Bass", "Diminished chords: rare, never the cadence"],
        ["Bass env", "DnB, breaks and garage start with a bass line"], ["Bass LFO", "The pluck on MIDI channel 2, note-offs and all"], ["Bass line", "Capture: play a bar on Jam, keep it"],
        ["Acid", "The acid line frozen and edited step by step"], ["Acid env", "Scanlines, for any theme"], ["Acid line", "DY voices named as their banks name them"],
        ["Acid answer", "Call and response: octave, fifth, mirror, sparse"], ["Acid steps", "Swing and looseness for every voice"], ["Pluck", "Your own DX7 voices, loaded over USB"],
        ["Pluck 2", "Arps: up, down, random, thirds"], ["Deja vu", "The pluck remembers and develops a motif"], ["Pluck steps", "Sixteen steps you write, stored with the piece"],
        ["Drums", "Fewer dropouts: a lost interrupt found, chunks no shorter than 128 frames"], ["Links", "The kick ducks the bass, the snare opens the pad"], ["Kit", "37 kits, every drum tunable"],
        ["Drum drift", "Patterns that change themselves"], ["Groove", "Per-lane density and source"], ["Steps", "Ratchets, push, ghost and accent on every step"],
        ["Feel", "Ghost notes, accents, comping, humanize"], ["Phrase", "Pattern chains, slices, half and double time"], ["Space", "Tempo-synced echo and reverb"],
        ["Reverb", "Plate, shimmer, gated"], ["Comp/Limit", "The master bus, as you saw"], ["Mixer", "Level, pan and send for every layer"], ["Global", "Store the piece; six themes"],
        ["Jam", "Play along with no wrong notes. Scenes. Backup and restore. A browser emulator and an editor."],
      ];
      let gap = 2000;
      for (const [title, body] of MORE) {
        ring(s, "knob:SELECT", gap / 1000, "");
        s.spin("SELECT", 1);
        text(s, { label: "AND", title, body, chips: [] });
        await s.wait(gap);
        gap = Math.max(380, Math.round(gap * 0.9));
      }
    }
    await s.wait(1200);
    // ---- 13. credits over FLUX
    text(s, { label: "0.15.0", title: "Out now", body: "The installer, the manual, the browser emulator: deamonslinky.github.io/PERF-1. Thank you for listening.", chips: [] });
    await s.wait(3500);
    await s.tap("HOME", 60); await s.wait(120); await s.tap("HOME", 60); await s.wait(1500);
    s.mark("credits", {});
    await s.wait(22000);
  },
};
