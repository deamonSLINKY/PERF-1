// "...and much, much more": the SELECT dial through every page, a line each.
import { text, cam, ring, at } from "./lib.mjs";
const PAGES = [
  ["Tempo", "Tempo, evolve, motion, and how long a chord lasts"], ["Pad", "A pad with 24 sounds"], ["Pad env", "Envelope you can draw"], ["Pad LFO", "An LFO for the filter, level, pan or pitch"],
  ["Chords", "Triads, sevenths, ninths, sus, dub, fourths: and a chord loop"], ["Bass", "24 bass sounds"], ["Bass env", "Bass envelope"], ["Bass LFO", "Wobble it"],
  ["Bass line", "The bass as a line: root, kick, walk, riff"], ["Acid", "A 303-style acid line, 31 sounds"], ["Acid env", "Accent, slide, decay"], ["Acid line", "Density, length, range, mutate"],
  ["Acid answer", "Call and response: octave, fifth, mirror, sparse"], ["Acid steps", "Edit the line step by step, and freeze it"], ["Pluck", "12 plucks, 30 DX7-style voices, and yours"], ["Pluck 2", "Arps: up, down, random, thirds"],
  ["Deja vu", "The pluck remembers, and brings it round again"], ["Drums", "Pattern, drive, swing"], ["Links", "The kick ducks the bass, the snare opens the pad"], ["Kit", "37 kits, every drum tunable"],
  ["Drum drift", "Patterns that change themselves"], ["Groove", "Per-lane density and source"], ["Steps", "Ratchets, push, ghost and accent on every step"], ["Feel", "Ghost notes, accents, comping, humanize"],
  ["Phrase", "Pattern chains, slices, half and double time, conditions"], ["Space", "Tempo-synced echo and reverb sends"], ["Reverb", "Plate, shimmer, gated"], ["Comp/Limit", "The master bus"],
  ["Mixer", "Level, pan and send for every layer"], ["Global", "Root, theme, tune, store the piece"], ["Jam", "Play along with no wrong notes"],
];
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "knobL");
    text(s, { label: "AND", title: "...much, much more", body: "Thirty-one pages, a step sequencer, scenes, backup and restore, your own DX7 voices, six skins, visualizers.", chips: [] });
    await s.wait(2600);
    for (let i = 1; i < PAGES.length; i++) {
      ring(s, "knob:SELECT", 0.6, "");
      s.spin("SELECT", 1);
      text(s, { label: "AND", title: PAGES[i][0], body: PAGES[i][1], chips: [] });
      await s.wait(720);
    }
    await s.wait(800);
  },
};
