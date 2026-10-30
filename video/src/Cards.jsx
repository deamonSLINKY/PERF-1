// The title and the credits.
import React, { useLayoutEffect, useRef } from "react";
import { AbsoluteFill, Audio, Easing, interpolate, spring, staticFile, useCurrentFrame, useVideoConfig } from "remotion";
import { loadFont } from "@remotion/google-fonts/BarlowSemiCondensed";
import { useTrace, drawFb, ORANGE, INK, DIM, BG } from "./EmuScene.jsx";

const { fontFamily } = loadFont("normal", { weights: ["400", "500", "600", "700"] });

const Slinky = ({ size = 64, color = "#8f8aa0" }) => (          // a small coil: the mark
  <svg width={size} height={size} viewBox="0 0 64 64" style={{ display: "block" }}>
    {[0, 1, 2, 3, 4, 5, 6].map((i) => <ellipse key={i} cx={32} cy={52 - i * 7} rx={22 - Math.abs(i - 3) * 1.5} ry={5} fill="none" stroke={color} strokeWidth={3} />)}
  </svg>
);

export function Title() {
  const frame = useCurrentFrame(), { fps, durationInFrames } = useVideoConfig(), t = frame / fps;
  const a = spring({ frame: frame - 8, fps, config: { damping: 20, stiffness: 90 } });
  const fade = interpolate(frame, [0, 10, durationInFrames - 10, durationInFrames], [0, 1, 1, 0], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
  const line = interpolate(t, [0.8, 2.0], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp", easing: Easing.inOut(Easing.cubic) });
  return (
    <AbsoluteFill style={{ background: `radial-gradient(ellipse at 50% 40%, #2a2b33, ${BG} 70%)`, fontFamily, color: INK, opacity: fade, alignItems: "center", justifyContent: "center" }}>
      <div style={{ textAlign: "center", transform: `scale(${0.94 + 0.06 * a})`, opacity: a }}>
        <div style={{ fontSize: 220, fontWeight: 700, letterSpacing: ".06em", lineHeight: 1 }}>PERF-1</div>
        <div style={{ width: 900 * line, height: 4, background: ORANGE, margin: "26px auto" }} />
        <div style={{ fontSize: 48, letterSpacing: ".08em", color: "#cfcbd8" }}>custom firmware for the M-VAVE FM-1</div>
        <div style={{ fontSize: 34, letterSpacing: ".12em", color: DIM, marginTop: 14 }}>a live-performance, generative instrument</div>
        <div style={{ fontSize: 40, letterSpacing: ".18em", color: ORANGE, marginTop: 30, fontWeight: 700 }}>THE 0.15.0 UPDATE</div>
      </div>
      <div style={{ position: "absolute", bottom: 70, display: "flex", alignItems: "center", gap: 18, opacity: interpolate(t, [2, 3], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" }) }}>
        <Slinky size={56} />
        <span style={{ fontSize: 34, letterSpacing: ".16em", color: "#8f8aa0", fontWeight: 600 }}>deamonSLINKY</span>
      </div>
    </AbsoluteFill>
  );
}

const LINES = [
  ["PERF-1", "by deamonSLINKY · free software, GPL-3.0 · github.com/deamonSLINKY/PERF-1"],
  ["Based on", "HYP-1 by RedtailWorks, a fork of AMB-1 by Charles Vestal"],
  ["Platform", "Felucca by Leo Kuroshita (Hügelton Instruments), through X0X and FoMni"],
  ["FM engine, DX7 voices", "Melodee by Kerem Kilic (Ellic Studio), after Dexed (Pascal Gauthier) on MSFA (Raph Levien / Google)"],
  ["Plate reverb", "after Dattorro, through FoMni"],
  ["Ideas", "Tuna by delirioux, and Marbles by Émilie Gillet"],
  ["The hardware", "FM-1-RE; rescue protocol from FM-1-transporter and kagaimiq's jl-uboot-tool"],
  ["The browser emulator", "an idea from Groove OS by Péter Gombos"],
  ["Type", "Chakra Petch and Barlow Semi Condensed, SIL OFL"],
  ["", "M-VAVE and FM-1 are trademarks of their owners. PERF-1 is independent firmware, not affiliated with or endorsed by them."],
];

export function Credits({ dir }) {
  const frame = useCurrentFrame(), { fps, durationInFrames } = useVideoConfig(), t = frame / fps;
  const d = useTrace(dir), ref = useRef(null);
  const n = d ? Math.min(frame, d.tr.frames.length - 1) : 0;
  useLayoutEffect(() => { if (d) drawFb(ref.current, d.fb, d.lastFb[n]); });
  const fade = interpolate(frame, [0, 10, durationInFrames - 15, durationInFrames], [0, 1, 1, 0], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
  return (
    <AbsoluteFill style={{ background: BG, fontFamily, color: INK, opacity: fade }}>
      <Audio src={staticFile(`${dir}/audio.wav`)} volume={fade} />
      <div style={{ position: "absolute", right: 90, top: 150, width: 640, height: 640, borderRadius: 34, background: "#0b0b0d", border: "3px solid #3a3d42", opacity: 0.9 }}>
        <canvas ref={ref} width={240} height={240} style={{ position: "absolute", left: 32, top: 32, width: 576, height: 576, imageRendering: "pixelated", borderRadius: 6 }} />
      </div>
      <div style={{ position: "absolute", left: 100, top: 80, fontSize: 40, letterSpacing: ".2em", color: ORANGE, fontWeight: 700 }}>CREDITS</div>
      <div style={{ position: "absolute", left: 100, top: 160, width: 1000 }}>
        {LINES.map(([k, v], i) => {
          const o = interpolate(t, [0.6 + i * 1.5, 1.2 + i * 1.5], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
          return (
            <div key={i} style={{ opacity: o, transform: `translateY(${(1 - o) * 14}px)`, marginBottom: 20 }}>
              {k && <div style={{ fontSize: 24, letterSpacing: ".16em", color: DIM, fontWeight: 600 }}>{k.toUpperCase()}</div>}
              <div style={{ fontSize: i === LINES.length - 1 ? 26 : 34, color: i === LINES.length - 1 ? DIM : "#e4e0ec", lineHeight: 1.25 }}>{v}</div>
            </div>
          );
        })}
      </div>
      <div style={{ position: "absolute", right: 90, bottom: 60, display: "flex", alignItems: "center", gap: 14, opacity: 0.9 }}>
        <Slinky size={44} />
        <span style={{ fontSize: 30, letterSpacing: ".16em", color: "#8f8aa0", fontWeight: 600 }}>deamonSLINKY</span>
      </div>
    </AbsoluteFill>
  );
}
