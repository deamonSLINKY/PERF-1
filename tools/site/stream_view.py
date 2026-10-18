#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Stream view, for livestreams, put into every emulator of the pack (web/emu, lof1, eco1, grav1):

  the page's emulator on a 16:9 stage (1920 x 1080, scaled to the window): the FM-1 on the left, its screen
  large and sharp on the right, the firmware's name above. Behind it all, the background breathes with the
  music: three soft glows in the firmware's colours, each following a band of what it plays (lows, mids,
  highs), their hues leaning warmer or cooler as the sound darkens or brightens. Subtle, slow, and still
  for those who ask their system for reduced motion. Whatever is pressed glows, whether clicked here
  or played on the real FM-1 through its remote (SEL + GLO); a knob turning gets a ring; a strip under the
  screen names the last few moves. 'FM-1 LINKED' shows while the remote is on.

  Open it with the Stream view button, or the address with ?stream. Its own small bar (Connect MIDI, Exit)
  shows only while the mouse is over the stage, so a window capture doesn't see it.

  python3 tools/site/stream_view.py      ->  the block put in (or replaced in) each emulator's index.html"""
from pathlib import Path

SRC = Path(__file__).resolve().parents[2]
PAGES = [SRC / "web/emu/index.html", SRC / "lof1/web/emu/index.html", SRC / "eco1/web/emu/index.html",
         SRC / "grav1/web/emu/index.html"]
A, B = "/* stream view: begin */", "/* stream view: end */"          # the old single marker (cleared)
CA, CB = "/* stream view css: begin */", "/* stream view css: end */"
JA, JB = "/* stream view js: begin */", "/* stream view js: end */"

CSS = CA + r"""
#stage { display: none; }
body.stream { overflow: hidden; padding: 0; background: #0c0b11; }
body.stream > main, body.stream > footer { display: none; }
body.stream #stage { display: block; position: fixed; left: 50%; top: 50%; width: 1920px; height: 1080px; transform-origin: 50% 50%;
  transform: translate(-50%, -50%) scale(var(--sv-s, .5)); overflow: hidden; color: #f2eade;
  background: radial-gradient(ellipse 62% 62% at 50% 42%, var(--sv-glow), transparent 70%),
              linear-gradient(#1a1824, #0c0b11); font-family: "Barlow Semi Condensed", Barlow, sans-serif; }
#stage .sv-bg { position: absolute; inset: 0; width: 1920px; height: 1080px; pointer-events: none; opacity: 0; transition: opacity 2s; }
#stage .sv-bg.live { opacity: 1; }
#stage .sv-dev { position: absolute; left: 56px; top: 190px; width: 1100px; }
#stage .sv-dev .device { width: 1100px; }
#stage .sv-big { position: absolute; left: 1174px; top: 128px; width: 704px; height: 704px; border-radius: 30px; background: #0b0a0e;
  border: 2px solid #5a5468; box-sizing: border-box; display: grid; place-items: center; }
#stage .sv-big canvas { width: 660px; height: 660px; image-rendering: pixelated; border-radius: 4px; background: #000; }
#stage .sv-title { position: absolute; left: 56px; top: 44px; display: flex; align-items: center; gap: 22px; }
#stage .sv-bars { display: flex; gap: 6px; } #stage .sv-bars i { display: block; height: 8px; border-radius: 2px; }
#stage .sv-name { font: 700 62px/1 "Barlow Semi Condensed", sans-serif; letter-spacing: .04em; color: var(--sv-acc); }
#stage .sv-tag { font: 500 32px/1.1 "Barlow Semi Condensed", sans-serif; color: #c9bfd8; }
#stage .sv-link { position: absolute; left: 1174px; top: 60px; font: 700 26px/1 "Barlow Semi Condensed", sans-serif; letter-spacing: .1em;
  color: #1a1206; background: #ff9a3c; border-radius: 999px; padding: 10px 22px; box-shadow: 0 0 24px #ff9a3c88; display: none; }
#stage.linked .sv-link { display: block; }
#stage .sv-moves { position: absolute; left: 1174px; top: 860px; width: 704px; display: flex; flex-wrap: nowrap; gap: 10px;
  justify-content: flex-end; overflow: hidden; }
#stage .sv-moves span { flex: none; white-space: nowrap; }
#stage .sv-moves span { font: 600 26px/1 "Barlow Semi Condensed", sans-serif; letter-spacing: .05em; padding: 9px 16px; border-radius: 10px;
  background: #22202c; border: 2px solid var(--sv-acc); color: #f2eade; transition: opacity .6s; }
#stage .sv-moves span.old { opacity: .35; }
#stage .sv-foot { position: absolute; right: 56px; bottom: 34px; font: 600 26px/1 "Barlow Semi Condensed", sans-serif; color: #85799a; letter-spacing: .04em; }
#stage .sv-kofi { position: absolute; right: 56px; bottom: 78px; display: flex; align-items: center; gap: 12px; padding: 10px 20px 10px 14px;
  border-radius: 999px; background: #ff5e5b; color: #fff; font: 700 28px/1 "Barlow Semi Condensed", sans-serif; letter-spacing: .03em;
  text-decoration: none; box-shadow: 0 0 22px #ff5e5b55; }
#stage .sv-kofi b { display: grid; place-items: center; width: 38px; height: 38px; border-radius: 50%; background: #fff; color: #ff5e5b; font-size: 22px; }
#stage .sv-ctl { position: absolute; left: 56px; bottom: 26px; display: flex; gap: 12px; opacity: 0; transition: opacity .25s; }
#stage:hover .sv-ctl { opacity: 1; }
#stage.idle .sv-ctl { opacity: 0; }
#stage.idle, #stage.idle * { cursor: none !important; }
#stage .sv-ctl button { font: 600 22px "Barlow Semi Condensed", sans-serif; color: #f2eade; background: #2a2734; border: 1px solid #6b6380;
  border-radius: 8px; padding: 6px 16px; cursor: pointer; }
body.stream .held .btnface { stroke: #ff9a3c; stroke-width: 6; filter: drop-shadow(0 0 7px #ff9a3c); }
body.stream .held .keyface { stroke: #ff9a3c; stroke-width: 6; filter: drop-shadow(0 0 8px #ff9a3c); }
body.stream .ctl.knob.sv-turn > circle:first-child { stroke: #ff9a3c; stroke-width: 6; filter: drop-shadow(0 0 8px #ff9a3c); }
""" + CB

JS = JA + r"""
(function streamView() {
  const PACK = { "HYP-1": ["#b06cff", "hypnotic techno + acid"], "LOF-1": ["#e8873a", "a lofi groovebox: you play, the band follows"],
                 "ECO-1": ["#76c35e", "a plant plays the FM-1"], "GRAV-1": ["#f2b84b", "a star and its planets play the FM-1"] };
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const name = (document.querySelector("h1").textContent.match(/[A-Z]+-1/) || ["FM-1"])[0];
  const [acc, tag] = PACK[name] || ["#e8873a", ""];
  const dev = document.getElementById("device"), home = dev.parentNode, after = dev.nextSibling;
  const stage = document.createElement("div");
  stage.id = "stage";
  stage.style.setProperty("--sv-acc", acc);
  stage.style.setProperty("--sv-glow", "rgba(" + [1, 3, 5].map((i) => parseInt(acc.slice(i, i + 2), 16)).join(",") + ",0.24)");
  stage.innerHTML = `<canvas class="sv-bg" width="192" height="108" aria-hidden="true"></canvas><div class="sv-title"><div class="sv-bars"><i style="width:70px;background:${acc}"></i><i style="width:34px;background:#7eccc0"></i></div>
    <div><div class="sv-name">${name}</div><div class="sv-tag">${tag}</div></div></div>
    <div class="sv-link">FM-1 LINKED</div><div class="sv-dev"></div><div class="sv-big"><canvas width="240" height="240"></canvas></div>
    <div class="sv-moves"></div><a class="sv-kofi" href="https://ko-fi.com/redtailworks" target="_blank" rel="noopener"><b>☕</b>Support me on ko-fi.com/redtailworks</a>
    <div class="sv-foot">${name}  ·  RedtailWorks  ·  rustypawz.github.io/fm1-HYP1</div>
    <div class="sv-ctl"><button type="button" data-a="midi">Connect MIDI</button><button type="button" data-a="exit">Exit stream view</button></div>`;
  document.body.appendChild(stage);
  const big = stage.querySelector(".sv-big canvas"), bctx = big.getContext("2d"), screen = document.getElementById("screen");
  const moves = stage.querySelector(".sv-moves");
  bctx.imageSmoothingEnabled = false;
  let on = false;
  function fit() { stage.style.setProperty("--sv-s", Math.min(innerWidth / 1920, innerHeight / 1080)); }
  function set(v) {
    on = v;
    document.body.classList.toggle("stream", on);
    if (on) stage.querySelector(".sv-dev").appendChild(dev); else home.insertBefore(dev, after);
    fit();
    try { const u = new URL(location.href); if (on) u.searchParams.set("stream", ""); else u.searchParams.delete("stream"); history.replaceState(null, "", u); } catch {}
  }
  addEventListener("resize", fit);
  let idleT = 0;                                  // the mouse still a moment: its bar and pointer gone (for captures)
  function woke() {
    stage.classList.remove("idle");
    clearTimeout(idleT);
    idleT = setTimeout(() => stage.classList.add("idle"), 2000);
  }
  stage.addEventListener("pointermove", woke);
  stage.addEventListener("pointerdown", woke);
  // the background, moving with the music: an analyser on the emulator's output (its first two channels, the
  // mix), three bands each lighting a slow, drifting glow; their hues lean with the sound's brightness
  const bg = stage.querySelector(".sv-bg"), gctx = bg.getContext("2d");
  const still = matchMedia("(prefers-reduced-motion: reduce)");
  const hue0 = (function (h) {                    // the accent's hue, in degrees
    const r = parseInt(h.slice(1, 3), 16) / 255, g = parseInt(h.slice(3, 5), 16) / 255, b = parseInt(h.slice(5, 7), 16) / 255;
    const mx = Math.max(r, g, b), mn = Math.min(r, g, b), d = mx - mn;
    if (!d) return 0;
    const x = mx === r ? (g - b) / d + (g < b ? 6 : 0) : mx === g ? (b - r) / d + 2 : (r - g) / d + 4;
    return x * 60;
  })(acc);
  let an = null, spec = null, binHz = 0, lastBg = 0;
  const lvl = [0, 0, 0], peak = [1e-4, 1e-4, 1e-4];
  let bright = 0.5;
  function listen() {
    if (an || typeof node === "undefined" || !node) return;
    try {
      an = node.context.createAnalyser();
      an.fftSize = 2048;
      an.smoothingTimeConstant = 0.6;
      an.channelCount = 2;
      an.channelCountMode = "explicit";
      an.channelInterpretation = "discrete";     // the mix only, never the voices apart (channels 3-8)
      node.connect(an);
      spec = new Float32Array(an.frequencyBinCount);
      binHz = node.context.sampleRate / an.fftSize;
    } catch { an = null; }
  }
  function glow(x, y, r, hue, a) {
    const g = gctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, `hsla(${hue}, 70%, 55%, ${a})`);
    g.addColorStop(0.5, `hsla(${hue}, 70%, 45%, ${a * 0.35})`);
    g.addColorStop(1, `hsla(${hue}, 70%, 40%, 0)`);
    gctx.fillStyle = g;
    gctx.fillRect(0, 0, 192, 108);
  }
  function drawBg(t) {
    if (t - lastBg < 33) return;                  // 30 a second is plenty for something this slow
    const dt = Math.min((t - lastBg) / 1000, 0.1);
    lastBg = t;
    listen();
    if (!an) return;
    an.getFloatFrequencyData(spec);
    const band = [0, 0, 0];
    let sum = 0, wsum = 0;
    for (let i = 1; i < spec.length; i++) {
      const f = i * binHz, m = Math.pow(10, spec[i] / 20);
      if (f < 30 || f > 12000) continue;
      band[f < 250 ? 0 : f < 2000 ? 1 : 2] += m * m;
      sum += m;
      wsum += m * Math.log2(f);
    }
    if (sum > 1e-6) {                             // brightness: the spectrum's centre, 200 Hz .. 3 kHz -> 0 .. 1
      const c = Math.min(Math.max((wsum / sum - Math.log2(200)) / Math.log2(15), 0), 1);
      bright += (c - bright) * Math.min(dt * 0.8, 1);
    }
    for (let k = 0; k < 3; k++) {                 // each band against its own recent loudest: quiet music moves too
      const v = Math.sqrt(band[k]);
      peak[k] = Math.max(v, peak[k] * Math.pow(0.5, dt / 8), 1e-4);
      const tgt = Math.min(v / peak[k], 1);
      lvl[k] += (tgt - lvl[k]) * Math.min(dt * (tgt > lvl[k] ? 5 : 0.9), 1);   // quick to swell, slow to fade
    }
    bg.classList.toggle("live", sum > 1e-4);
    const s = t / 1000, calm = still.matches ? 0 : 1, lean = (bright - 0.5) * 50;
    gctx.globalCompositeOperation = "source-over";
    gctx.clearRect(0, 0, 192, 108);
    gctx.globalCompositeOperation = "lighter";
    glow(70 + calm * 16 * Math.sin(s * 0.07), 78 + calm * 8 * Math.sin(s * 0.05 + 1), 80 + 18 * lvl[0], hue0 - 25 + lean, 0.06 + 0.22 * lvl[0]);
    glow(110 + calm * 22 * Math.sin(s * 0.06 + 2), 42 + calm * 12 * Math.sin(s * 0.08), 70 + 14 * lvl[1], hue0 + lean, 0.05 + 0.2 * lvl[1]);
    glow(150 + calm * 18 * Math.sin(s * 0.09 + 4), 30 + calm * 10 * Math.sin(s * 0.07 + 3), 52 + 12 * lvl[2], hue0 + 40 + lean, 0.04 + 0.16 * lvl[2]);
  }
  (function tick(t) {                             // the screen, large; the remote's badge; the background
    if (on) {
      bctx.drawImage(screen, 0, 0);
      stage.classList.toggle("linked", dev.classList.contains("remote"));
      drawBg(t || 0);
    }
    requestAnimationFrame(tick);
  })();
  // the last few moves, under the screen
  function say(s, same) {
    const last = moves.lastElementChild;
    if (same && last && last.dataset.k === same && Date.now() - last.dataset.t < 900) {
      last.dataset.n = +last.dataset.n + 1;
      last.textContent = s + (last.dataset.n > 1 ? " ×" + last.dataset.n : "");
      last.dataset.t = Date.now();
      return;
    }
    const e = document.createElement("span");
    e.textContent = s;
    e.dataset.k = same || "";
    e.dataset.n = 1;
    e.dataset.t = Date.now();
    moves.appendChild(e);
    while (moves.children.length > 7) moves.firstElementChild.remove();
  }
  setInterval(() => { for (const e of moves.children) e.classList.toggle("old", Date.now() - e.dataset.t > 3000); }, 400);
  const BTN_NAME = {};
  for (const [n, b] of Object.entries(buttons)) BTN_NAME[b.bit] = n;
  let seenB = 0, seenK = 0;
  const _sb = syncButtons, _sk = syncKeys, _turn = turn;
  syncButtons = function () {                     // what goes down, named; held ones glow (the page's .held)
    _sb.apply(this, arguments);
    let m = 0;
    for (const b of btnHeldBy.values()) m |= 1 << b;
    const down = m & ~seenB;
    seenB = m;
    if (down) {
      const ns = Object.keys(BTN_NAME).filter((b) => down & (1 << b)).map((b) => BTN_NAME[b]);
      const held = Object.keys(BTN_NAME).filter((b) => (m & ~down) & (1 << b)).map((b) => BTN_NAME[b]);
      say((held.length ? held.join(" + ") + " + " : "") + ns.join(" + "));
    }
  };
  syncKeys = function () {
    _sk.apply(this, arguments);
    let m = 0;
    for (const k of keyHeldBy.values()) m |= 1 << k;
    const down = m & ~seenK;
    seenK = m;
    for (let i = 0; i < 27; i++) if (down & (1 << i)) { const n = 53 + i; say("KEY " + NOTE[n % 12] + (Math.floor(n / 12) - 1)); }
  };
  turn = function (k, n) {                        // a knob turning: a ring for a moment, and its name
    _turn.apply(this, arguments);
    if (!n) return;
    const label = Object.keys(knobs).find((x) => knobs[x] === k) || "KNOB";
    k.g.classList.add("sv-turn");
    clearTimeout(k.svT);
    k.svT = setTimeout(() => k.g.classList.remove("sv-turn"), 700);
    say(label.replace(/^KNOB(\d)/, "KNOB $1") + (n > 0 ? " ▲" : " ▼"), label + (n > 0 ? "+" : "-"));
  };
  stage.querySelector('[data-a="exit"]').addEventListener("click", () => set(false));
  stage.querySelector('[data-a="midi"]').addEventListener("click", () => {
    const b = document.getElementById("midi-go") || document.getElementById("midiOn");
    if (b) b.click();
  });
  const btn = document.createElement("button");
  btn.type = "button";
  btn.textContent = "Stream view";
  btn.title = "For livestreams: the FM-1 and its screen large on a 16:9 stage (or open this page with ?stream)";
  const bar = document.querySelector(".bar");
  if (bar) bar.appendChild(btn);
  btn.addEventListener("click", () => set(true));
  if (new URLSearchParams(location.search).has("stream")) set(true);
})();
""" + JB


def put(text, block, a, b, before):
    if a in text:
        i, j = text.index(a), text.index(b) + len(b)
        return text[:i] + block + text[j:]
    k = text.rindex(before)
    return text[:k] + block + "\n" + text[k:]


def main():
    for p in PAGES:
        s = p.read_text()
        while A in s:                            # the first version's block, one marker for both
            i, j = s.index(A), s.index(B) + len(B)
            s = s[:i] + s[j:].lstrip("\n")
        s = put(s, CSS, CA, CB, "</style>")
        s = put(s, JS, JA, JB, "</script>")
        p.write_text(s)
        print("stream view:", p.relative_to(SRC))


if __name__ == "__main__":
    main()
