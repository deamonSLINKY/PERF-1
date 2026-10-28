// A scene made from a trace (driver/capture.mjs): the firmware's own screen and panel, its audio, and overlays that follow the cues
// the scene's script left (events). Cues: text {label,title,body,chips}, ring {target,dur,chip}, cam {to}, readout {text,sub,dur}.
import React, { useEffect, useLayoutEffect, useRef, useState } from "react";
import { AbsoluteFill, Audio, Easing, interpolate, spring, staticFile, useCurrentFrame, useVideoConfig, delayRender, continueRender } from "remotion";
import { loadFont } from "@remotion/google-fonts/BarlowSemiCondensed";
import { Panel, LAYOUT, SCREEN } from "./Panel.jsx";

const { fontFamily } = loadFont("normal", { weights: ["400", "500", "600", "700"] });
export const ORANGE = "#ff8a2a", INK = "#e9e6ef", DIM = "#9a95a8", BG = "#1b1c21";
export const Brand = ({ opacity = 1 }) => (
  <div style={{ position: "absolute", right: 80, bottom: 26, fontSize: 24, letterSpacing: ".1em", color: "#6f6b7c", opacity, fontWeight: 600 }}>deamonSLINKY</div>
);

export function useTrace(dir) {
  const [d, setD] = useState(null);
  const [h] = useState(() => delayRender("trace " + dir));
  useEffect(() => {
    (async () => {
      const tr = await (await fetch(staticFile(`${dir}/trace.json`))).json();
      const fb = new Uint16Array(await (await fetch(staticFile(`${dir}/fb.bin`))).arrayBuffer());
      const lastFb = []; let cur = 0;
      tr.frames.forEach((f, i) => { if (f.fb !== undefined) cur = f.fb; lastFb[i] = cur; });
      const ang = []; const acc = {};
      tr.frames.forEach((f, i) => { for (const [r, n] of Object.entries(f.enc || {})) acc[r] = (acc[r] || 0) + n * 24; ang[i] = { ...acc }; });
      setD({ tr, fb, lastFb, ang });
      continueRender(h);
    })();
  }, [dir, h]);
  return d;
}

export function drawFb(canvas, fb, n) {
  if (!canvas) return;
  const ctx = canvas.getContext("2d"), img = ctx.createImageData(240, 240), o = n * 57600;
  for (let i = 0; i < 57600; i++) {
    const v = fb[o + i], p = ((v >> 8) | (v << 8)) & 0xffff;
    img.data[i * 4] = ((p >> 11) & 31) * 255 / 31; img.data[i * 4 + 1] = ((p >> 5) & 63) * 255 / 63; img.data[i * 4 + 2] = (p & 31) * 255 / 31; img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

// the panel camera: where it looks (panel units) and how far in
const CAMS = { wide: [500, 320, 1.0], fx: [700, 235, 1.75], btns: [760, 230, 1.5], knobs: [740, 110, 1.55], knobL: [215, 150, 2.0], keys: [560, 440, 1.35], keysL: [330, 460, 1.6], keysR: [760, 460, 1.6],
  octs: [165, 290, 2.0], screen: [418, 162, 2.3] };
function camAt(events, t) {
  const cs = events.filter((e) => e.kind === "cam");
  let prev = CAMS.wide, cur = CAMS.wide, t0 = -9;
  for (const e of cs) { if (e.t > t) break; prev = cur; cur = CAMS[e.to] || CAMS.wide; t0 = e.t; }
  const k = interpolate(t, [t0, t0 + 0.9], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.inOut(Easing.cubic) });
  return { cx: prev[0] + (cur[0] - prev[0]) * k, cy: prev[1] + (cur[1] - prev[1]) * k, z: prev[2] + (cur[2] - prev[2]) * k };
}

function target(tg) {                                 // "btn:FX", "knob:SELECT", "key:20", "keys:20,22"
  const [k, v] = tg.split(":");
  if (k === "btn") return [{ ...LAYOUT.buttons[v], r: 10 }];
  if (k === "knob") { const o = LAYOUT.knobs[v]; return [{ x: o.x - o.r - 10, y: o.y - o.r - 10, w: 2 * o.r + 20, h: 2 * o.r + 20, r: o.r + 10 }]; }
  if (k === "key" || k === "keys") return v.split(",").map((x) => ({ ...LAYOUT.keys[+x], r: 22 }));
  return [];
}

const Chip = ({ children, on }) => (
  <span style={{ display: "inline-block", padding: "4px 18px", margin: "0 8px", borderRadius: 12, fontWeight: 700, fontSize: 34, letterSpacing: ".04em",
    color: on ? "#fff" : INK, background: on ? "#d8452d" : "#2a2c33", border: `2px solid ${on ? "#ff7a5c" : "#44474f"}` }}>{children}</span>
);

export function EmuScene({ dir }) {
  const frame = useCurrentFrame(), { fps, durationInFrames } = useVideoConfig(), t = frame / fps;
  const fade = interpolate(frame, [0, 8, durationInFrames - 8, durationInFrames], [0, 1, 1, 0], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
  const d = useTrace(dir);
  const small = useRef(null), big = useRef(null);
  const n = d ? Math.min(frame, d.tr.frames.length - 1) : 0;
  useLayoutEffect(() => { if (d) { drawFb(small.current, d.fb, d.lastFb[n]); drawFb(big.current, d.fb, d.lastFb[n]); } });
  if (!d) return <AbsoluteFill style={{ background: BG }} />;
  const f = d.tr.frames[n], ev = d.tr.events;
  const text = [...ev].reverse().find((e) => e.kind === "text" && e.t <= t) || { title: "", body: "", chips: [] };
  const textT = text.t ?? 0;
  const st = spring({ frame: frame - Math.round(textT * fps), fps, config: { damping: 18, stiffness: 120 } });
  const rings = ev.filter((e) => e.kind === "ring" && t >= e.t - 0.05 && t < e.t + (e.dur ?? 1));
  const readout = [...ev].reverse().find((e) => e.kind === "readout" && e.t <= t && t < e.t + (e.dur ?? 3));
  const cam = camAt(ev, t), s = 0.9 * cam.z;
  const pulse = 0.65 + 0.35 * Math.sin(t * 9);
  const lit = new Set(rings.map((r) => r.chip).filter(Boolean));
  return (
    <AbsoluteFill style={{ background: `radial-gradient(ellipse at 30% 20%, #25262d, ${BG} 70%)`, fontFamily, color: INK, opacity: fade }}>
      <Audio src={staticFile(`${dir}/audio.wav`)} volume={fade} />
      <div style={{ position: "absolute", left: 80, top: 56, fontSize: 28, letterSpacing: ".16em", fontWeight: 600, color: DIM }}>
        <span style={{ color: ORANGE }}>{text.label || ""}</span>
      </div>
      <div style={{ position: "absolute", left: 80, top: 110, opacity: Math.min(1, st * 1.4), transform: `translateY(${(1 - st) * 24}px)`, width: 880 }}>
        <div style={{ fontSize: !text.title || text.title.length <= 20 ? 104 : text.title.length <= 27 ? 84 : 64, fontWeight: 700, lineHeight: 1.05 }}>{text.title}</div>
        <div style={{ margin: "22px -8px", fontSize: 34, height: 54, whiteSpace: "nowrap" }}>
          {(text.chips || []).map((c, i) => c.startsWith("*")
            ? <Chip key={i} on={lit.has(c.slice(1))}>{c.slice(1)}</Chip>
            : <span key={i} style={{ color: DIM, margin: "0 8px" }}>{c}</span>)}
        </div>
        <div style={{ fontSize: 36, lineHeight: 1.4, color: "#cfcbd8", fontWeight: 400 }}>{text.body}</div>
      </div>

      <div style={{ position: "absolute", left: 1040, top: 110, width: 800, height: 800, borderRadius: 40, background: "#0b0b0d", border: "3px solid #3a3d42", boxShadow: "0 30px 80px #0008" }}>
        <canvas ref={big} width={240} height={240} style={{ position: "absolute", left: 40, top: 40, width: 720, height: 720, imageRendering: "pixelated", borderRadius: 6 }} />
      </div>
      {readout && (
        <div style={{ position: "absolute", left: 1040, top: 930, width: 800, textAlign: "center", opacity: Math.min(1, interpolate(t, [readout.t, readout.t + 0.25], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" })) }}>
          <span style={{ fontSize: 64, fontWeight: 700, letterSpacing: ".06em", color: ORANGE }}>{readout.text}</span>
          {readout.sub && <span style={{ fontSize: 28, letterSpacing: ".14em", color: DIM, marginLeft: 22 }}>{readout.sub}</span>}
        </div>
      )}

      <div style={{ position: "absolute", left: 60, top: 462, width: 900, height: 576, borderRadius: 30, overflow: "hidden", background: "#16171b", border: "2px solid #2f3138" }}>
        <div style={{ position: "absolute", left: 0, top: 0, width: 1000, height: 640, transformOrigin: "0 0", transform: `translate(${450 - cam.cx * s}px, ${288 - cam.cy * s}px) scale(${s})` }}>
          <Panel frame={f} angles={d.ang[n]}>
            {rings.flatMap((r, j) => target(r.target).map((o, i) => <rect key={`${j}-${i}`} x={o.x - 5} y={o.y - 5} width={o.w + 10} height={o.h + 10} rx={o.r} fill="none" stroke={ORANGE} strokeWidth={5} opacity={pulse} />))}
          </Panel>
          <canvas ref={small} width={240} height={240} style={{ position: "absolute", left: SCREEN.x, top: SCREEN.y, width: SCREEN.w, height: SCREEN.h, imageRendering: "pixelated" }} />
        </div>
      </div>
      <Brand />
    </AbsoluteFill>
  );
}
