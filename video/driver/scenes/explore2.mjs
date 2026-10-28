// exploration: the auto-DJ's cycle with the shortest drift
export default {
  seed: 0, fps: 2,
  async run(s) {
    await s.wait(3600);
    await s.tap("FX");
    s.press("SEL"); await s.wait(100); s.press("FX"); await s.wait(150); s.release("FX"); s.release("SEL");
    await s.wait(500);
    await s.turn("SELECT", 18, 120); await s.wait(300);
    await s.turn("KNOB2", 5, 150); await s.turn("KNOB3", -2, 150);
    await s.tap("FX"); await s.wait(300);
    await s.wait(300);
    await s.turn("KNOB4", -30, 70);                         // drift to its shortest
    s.start(2);
    await s.tap("HOME"); await s.wait(600);                 // perform
    s.press("GLO"); await s.wait(500); await s.tapKey(24); await s.wait(200); s.release("GLO");
    s.mark("djon");
    await s.wait(70000);
  },
};
