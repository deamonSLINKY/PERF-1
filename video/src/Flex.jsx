// Flex Beat, 20 s: the firmware's own screen and panel from the trace (driver/capture.mjs), the audio it rendered, and the overlays.
import React, { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { AbsoluteFill, Audio, Easing, interpolate, spring, staticFile, useCurrentFrame, useVideoConfig, delayRender, continueRender } from "remotion";
import { loadFont } from "@remotion/google-fonts/BarlowSemiCondensed";
import { Panel, LAYOUT, SCREEN, center } from "./Panel.jsx";

const { fontFamily } = loadFont("normal", { weights: ["400", "500", "600", "700"] });
const ORANGE = "#ff8a2a", INK = "#e9e6ef", DIM = "#9a95a8", BG = "#1b1c21";

function useTrace(dir) {
  const [d, setD] = useState(null);
  const [h] = useState(() => delayRender("trace"));
  useEffect(() => {
    (async () => {
      const tr = await (await fetch(staticFile(`${dir}/trace.json`))).json();
      const fb = new Uint16Array(await (await fetch(staticFile(`${dir}/fb.bin`))).arrayBuffer());
      const lastFb = []; let cur = 0;
      tr.frames.forEach((f, i) => { if (f.fb !== undefined) cur = f.fb; lastFb[i] = cur; });
      const ang = []; const acc = {}; const roles = {};
      tr.frames.forEach((f, i) => { for (const [r, n] of Object.entries(f.enc || {})) acc[r] = (acc[r] || 0) + n * 24; ang[i] = { ...acc }; });
      setD({ tr, fb, lastFb, ang, roles });
      continueRender(h);
    })();
  }, [dir, h]);
  return d;
}

function drawFb(canvas, fb, n) {
  if (!canvas) return;
  const ctx = canvas.getContext("2d"), img = ctx.createImageData(240, 240), o = n * 57600;
  for (let i = 0; i < 57600; i++) {
    const v = fb[o + i], p = ((v >> 8) | (v << 8)) & 0xffff;
    img.data[i * 4] = ((p >> 11) & 31) * 255 / 31; img.data[i * 4 + 1] = ((p >> 5) & 63) * 255 / 63; img.data[i * 4 + 2] = (p & 31) * 255 / 31; img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

// the panel camera: where it looks (panel units) and how far in, by second
const CAM = [[0, 500, 320, 1.0], [1.6, 500, 320, 1.0], [2.2, 700, 235, 1.75], [3.3, 700, 235, 1.75], [3.6, 215, 150, 2.0], [4.8, 215, 150, 2.0], [5.5, 560, 440, 1.35], [16.2, 560, 440, 1.35], [17.4, 500, 320, 1.0], [20, 500, 320, 1.0]];
function camAt(t) {
  let i = 0;
  while (i < CAM.length - 2 && t > CAM[i + 1][0]) i++;
  const a = CAM[i], b = CAM[i + 1], k = interpolate(t, [a[0], b[0]], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.inOut(Easing.cubic) });
  return { cx: a[1] + (b[1] - a[1]) * k, cy: a[2] + (b[2] - a[2]) * k, z: a[3] + (b[3] - a[3]) * k };
}

const Chip = ({ children, on }) => (
  <span style={{ display: "inline-block", padding: "4px 18px", margin: "0 8px", borderRadius: 12, fontWeight: 700, fontSize: 34, letterSpacing: ".04em",
    color: on ? "#fff" : INK, background: on ? "#d8452d" : "#2a2c33", border: `2px solid ${on ? "#ff7a5c" : "#44474f"}`, transition: "none" }}>{children}</span>
);

export function Flex({ dir }) {
  const frame = useCurrentFrame(), { fps } = useVideoConfig(), t = frame / fps;
  const d = useTrace(dir);
  const small = useRef(null), big = useRef(null);
  const n = d ? Math.min(frame, d.tr.frames.length - 1) : 0;
  useLayoutEffect(() => { if (d) { drawFb(small.current, d.fb, d.lastFb[n]); drawFb(big.current, d.fb, d.lastFb[n]); } });
  if (!d) return <AbsoluteFill style={{ background: BG }} />;
  const f = d.tr.frames[n], ev = d.tr.events;
  const flexEv = ev.filter((e) => e.kind === "flex"), cur = [...flexEv].reverse().find((e) => e.t <= t);
  const when = (a, b) => t >= a && t < b;
  const rise = (t0, len = 0.5) => interpolate(t, [t0, t0 + len], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.out(Easing.cubic) });
  const stage = t < 2.4 ? 0 : t < 5.2 ? 1 : 2;
  const stageT = [0.3, 2.4, 5.2][stage];
  const st = spring({ frame: frame - stageT * fps, fps, config: { damping: 18, stiffness: 120 } });
  const cam = camAt(t), s = 0.9 * cam.z;
  const ring = [];
  if (when(2.4, 3.4)) ring.push({ ...LAYOUT.buttons.FX, r: 10 });
  if (when(3.5, 4.6)) { const k = LAYOUT.knobs.SELECT; ring.push({ x: k.x - k.r - 10, y: k.y - k.r - 10, w: 2 * k.r + 20, h: 2 * k.r + 20, r: k.r + 10 }); }
  for (const e of flexEv) { const end = e.t + (e.hold ? 2.9 : 0.9); if (t >= e.t - 0.05 && t < end) ring.push({ ...LAYOUT.keys[e.key], r: 22 }); }
  const pulse = 0.65 + 0.35 * Math.sin(t * 9);
  const title = ["Flex Beat", "Flex Beat", "Flex Beat"][stage];
  const body = [
    "Twenty-seven one-bar patterns that rework the last stretch of the music on the grid.",
    "The FX menu has a third bank: turn SELECT twice. One pattern on every key.",
    "Tap a key: it latches and waits for the bar. Hold it: the bar repeats until you let go.",
  ][stage];
  const outro = rise(17.2, 0.8);
  return (
    <AbsoluteFill style={{ background: `radial-gradient(ellipse at 30% 20%, #25262d, ${BG} 70%)`, fontFamily, color: INK }}>
      <Audio src={staticFile(`${dir}/audio.wav`)} />
      <div style={{ position: "absolute", left: 80, top: 56, fontSize: 28, letterSpacing: ".16em", fontWeight: 600, color: DIM }}>
        <span style={{ color: ORANGE }}>NEW IN 0.14</span> &nbsp; FX MENU
      </div>
      <div style={{ position: "absolute", right: 80, top: 56, fontSize: 24, letterSpacing: ".08em", color: DIM }}>PERF-1 · HYPNO · 128 BPM</div>
      <div style={{ position: "absolute", left: 80, top: 110, opacity: Math.min(1, st * 1.4), transform: `translateY(${(1 - st) * 24}px)`, width: 880 }}>
        <div style={{ fontSize: 104, fontWeight: 700, lineHeight: 1.05 }}>{title}</div>
        <div style={{ margin: "22px -8px", fontSize: 34, height: 54, whiteSpace: "nowrap" }}>
          {stage < 2 ? <><span style={{ color: DIM, margin: "0 8px" }}>TAP</span><Chip on={when(2.4, 3.4)}>FX</Chip><span style={{ color: DIM, margin: "0 8px" }}>turn</span><Chip on={when(3.5, 4.6)}>SELECT</Chip><span style={{ color: DIM, margin: "0 8px" }}>twice</span></>
            : <><span style={{ color: DIM, margin: "0 8px" }}>TAP</span><Chip on={!!cur && !cur.hold && t < cur.t + 0.9}>KEY</Chip><span style={{ color: DIM, margin: "0 8px" }}>latch ·  HOLD</span><Chip on={!!cur && !!cur.hold && t < cur.t + 2.9}>KEY</Chip><span style={{ color: DIM, margin: "0 8px" }}>repeat</span></>}
        </div>
        <div style={{ fontSize: 36, lineHeight: 1.4, color: "#cfcbd8", fontWeight: 400 }}>{body}</div>
      </div>

      {/* the screen, large */}
      <div style={{ position: "absolute", left: 1040, top: 110, width: 800, height: 800, borderRadius: 40, background: "#0b0b0d", border: "3px solid #3a3d42", boxShadow: "0 30px 80px #0008" }}>
        <canvas ref={big} width={240} height={240} style={{ position: "absolute", left: 40, top: 40, width: 720, height: 720, imageRendering: "pixelated", borderRadius: 6 }} />
      </div>
      <div style={{ position: "absolute", left: 1040, top: 930, width: 800, textAlign: "center", opacity: cur ? Math.min(1, rise(cur.t, 0.25)) : 0 }}>
        {cur && <>
          <span style={{ fontSize: 64, fontWeight: 700, letterSpacing: ".06em", color: ORANGE }}>{cur.name}</span>
          <span style={{ fontSize: 28, letterSpacing: ".14em", color: DIM, marginLeft: 22 }}>{cur.hold ? (t < cur.t + 2.9 ? "HOLDING" : "RELEASED") : t < cur.t + 0.9 ? "LATCHED" : "PLAYING"}</span>
        </>}
      </div>

      {/* the panel, with its camera */}
      <div style={{ position: "absolute", left: 60, top: 462, width: 900, height: 576, borderRadius: 30, overflow: "hidden", background: "#16171b", border: "2px solid #2f3138" }}>
        <div style={{ position: "absolute", left: 0, top: 0, width: 1000, height: 640, transformOrigin: "0 0", transform: `translate(${450 - cam.cx * s}px, ${288 - cam.cy * s}px) scale(${s})` }}>
          <Panel frame={f} angles={d.ang[n]}>
            {ring.map((r, i) => <rect key={i} x={r.x - 5} y={r.y - 5} width={r.w + 10} height={r.h + 10} rx={r.r} fill="none" stroke={ORANGE} strokeWidth={5} opacity={pulse} />)}
          </Panel>
          <canvas ref={small} width={240} height={240} style={{ position: "absolute", left: SCREEN.x, top: SCREEN.y, width: SCREEN.w, height: SCREEN.h, imageRendering: "pixelated" }} />
        </div>
      </div>

      <AbsoluteFill style={{ background: BG, opacity: 0.0 }} />
      <div style={{ position: "absolute", left: 80, top: 640, width: 880, opacity: outro, transform: `translateY(${(1 - outro) * 20}px)`, display: "none" }} />
      <div style={{ position: "absolute", right: 80, bottom: 30, fontSize: 26, color: DIM, letterSpacing: ".06em", opacity: outro }}>deamonSLINKY / PERF-1 · the firmware itself, not a recording</div>
    </AbsoluteFill>
  );
}
