// exploration: where the pages are (not a video scene)
export default {
  seed: 0, fps: 10,
  async run(s) {
    await s.wait(3600);
    s.start(10);
    await s.wait(500);
    await s.tap("FX");
    await s.wait(300);
    s.press("SEL"); await s.wait(100); s.press("FX"); await s.wait(150); s.release("FX"); s.release("SEL");
    await s.wait(500);
    await s.turn("SELECT", 18, 120);          // to Auto DJ?
    await s.wait(600);                        // t ~ 6.1
    s.mark("autodj");
    await s.turn("KNOB2", 5, 150); await s.wait(300);   // Groove -> ?
    await s.turn("KNOB3", -2, 150); await s.wait(300);  // Break -> ?
    await s.wait(300);
    s.mark("autodj2");
    await s.tap("FX"); await s.wait(400);
    await s.tap("HOME"); await s.wait(700); await s.tap("HOME"); await s.wait(1000);
    s.mark("perform");
    await s.turn("ALGORITHM", -6, 100); await s.wait(500);
    s.mark("mood");
    await s.tap("GLO"); await s.wait(300);
    s.press("GLO"); await s.wait(500); await s.tapKey(24); await s.wait(200); s.release("GLO"); await s.wait(1500);   // GLO + F5 (key 24 = F5? keys F3=0 -> F5=24)
    s.mark("djon");
    await s.wait(1000);
  },
};
