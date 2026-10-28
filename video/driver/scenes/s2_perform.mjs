// Drum change on, then the Perform screen and the mood.
import { text, cam, ring, readout, at, tapB, tapK, turnK } from "./lib.mjs";
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    cam(s, "btns");
    text(s, { label: "PERFORM", title: "Drums that change themselves", body: "Drum drift: the pattern moves on every few bars and the fill leads into it.", chips: ["TAP", "*SEQ", "four times"] });
    await s.wait(800);
    for (let i = 0; i < 4; i++) { await tapB(s, "SEQ", "SEQ"); await s.wait(250); }
    cam(s, "knobs");
    await s.wait(500);
    text(s, { label: "PERFORM", title: "Drums that change themselves", body: "Change: next. Every: 4 bars. Turn it on and leave it.", chips: ["turn", "*KNOB1", "*KNOB2"] });
    await turnK(s, "KNOB1", 1, 150, "KNOB1"); await s.wait(500);
    await turnK(s, "KNOB2", -1, 150, "KNOB2"); await s.wait(900);
    await tapB(s, "HOME", "HOME"); await s.wait(600);        // the Tempo page; HOME again is the Perform screen
    cam(s, "btns");
    await tapB(s, "HOME", "HOME"); await s.wait(500);
    await at(s, 11000);
    cam(s, "screen");
    text(s, { label: "PERFORM", title: "The Perform screen", body: "The section, the eight tracks, the scenes, what is on, and the mood: all on one screen, all on the keys.", chips: ["tap", "*HOME", "again"] });
    await at(s, 15500);
    cam(s, "knobL");
    text(s, { label: "PERFORM", title: "One knob: the mood", body: "From chill to peak. Acid, hats, pluck, bass, reverb and more move together.", chips: ["turn", "*ALGORITHM"] });
    ring(s, "knob:ALGORITHM", 15, "ALGORITHM");
    readout(s, "CHILL", "", 5);
    await s.turn("ALGORITHM", -25, 120);
    await s.wait(900);
    readout(s, "MOOD", "BACK TO MIDDLE", 3);
    await s.turn("ALGORITHM", 25, 70);
    await s.wait(500);
    readout(s, "PEAK", "", 6);
    await s.turn("ALGORITHM", 25, 140);
    await s.wait(1500);
    cam(s, "keysL");
    text(s, { label: "PERFORM", title: "Mutes and solos on the keys", body: "Drop the hats, bring them back.", chips: ["tap", "*KEY"] });
    await s.wait(800);
    await tapK(s, 2, 120, ""); await s.wait(2200);
    await tapK(s, 2, 120, ""); await s.wait(1500);
    await s.turn("ALGORITHM", -25, 40);
    await at(s, 33000);
  },
};
