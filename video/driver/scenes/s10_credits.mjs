// The credits roll over the first piece, playing (no overlays: src/Credits.jsx draws them).
export default {
  seed: 0, fps: 30,
  async run(s) {
    await s.wait(3600);
    s.start(30);
    await s.wait(22000);
  },
};
