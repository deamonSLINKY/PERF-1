// Switch on: the cards, then music. How quick it is, and that it is made for playing.
import { text, cam, ring, at, tapB, tapK } from "./lib.mjs";
export default {
  seed: 0, fps: 30,
  async run(s) {
    s.start(30);                                           // from power-on
    cam(s, "screen");
    text(s, { label: "SWITCH ON", title: "Music in four seconds", body: "No menus, no project to load. It boots straight into a piece that is already playing.", chips: [] });
    await at(s, 4600);
    cam(s, "wide");
    text(s, { label: "MADE FOR PLAYING", title: "Easy, then deep", body: "A first jam in a minute. And when you want to dig, every page, effect and step is yours.", chips: [] });
    await at(s, 9500);
    cam(s, "btns");
    text(s, { label: "TRY IT", title: "Tap and it answers", body: "The chord moves on at the next bar.", chips: ["TAP", "*EDIT"] });
    await at(s, 10500);
    await tapB(s, "EDIT", "EDIT");
    await at(s, 14500);
    cam(s, "keys");
    text(s, { label: "TRY IT", title: "Tap keys, change the scale", body: "Each key adds or removes its note. There are no wrong ones to find.", chips: ["TAP", "*KEYS"] });
    await at(s, 15500);
    for (const k of [3, 8, 10]) { await tapK(s, k, 140); await s.wait(500); }
    await at(s, 20000);
  },
};
