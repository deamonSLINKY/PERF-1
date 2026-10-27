// The FM-1's panel as the web emulator draws it (web/emu/index.html), as a component: the picture only, driven by a frame of the trace.
import React from "react";

export const BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, "OCT-": 12, "OCT+": 13 };
export const WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
export const BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];
const BODY = "#3a3d42", TRAY = "#26282c", KEY_W = "#d9dadc", KEY_B = "#c9cacd", KNOB = "#1f2124", BTNC = "#2e3135", LABEL = "#e8e8ea";
export const FONT = "'Barlow Semi Condensed', 'Arial Narrow', Arial, sans-serif";

export const SCREEN = { x: 318, y: 62, w: 200, h: 200 };      // the LCD, in panel units (the emulator's canvas: 31.8 %, 9.69 %, 20 %, 31.25 % of 1000 x 640)

// where the controls are, in panel units: overlays and the camera point at these
const pitch = (928 - 24) / 16, kw = pitch * 0.78;
export const LAYOUT = { buttons: {}, keys: {}, knobs: {} };
const ROW1 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ROW2 = ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"];
ROW1.forEach((n, i) => { LAYOUT.buttons[n] = { x: 582 + i * 63, y: 174, w: 54, h: 48 }; });
ROW2.forEach((n, i) => { LAYOUT.buttons[n] = { x: 582 + i * 63, y: 236, w: 54, h: 48 }; });
LAYOUT.buttons["OCT-"] = { x: 72, y: 268, w: 82, h: 38 };
LAYOUT.buttons["OCT+"] = { x: 170, y: 268, w: 82, h: 38 };
WHITE_K.forEach((k, i) => { LAYOUT.keys[k] = { x: 36 + 12 + i * pitch + (pitch - kw) / 2, y: 340 + 270 * 0.43, w: kw, h: 270 * 0.52, black: false }; });
BLACK_K.forEach((k, i) => { LAYOUT.keys[k] = { x: 36 + 12 + (BLACK_AFTER[i] + 1) * pitch - kw / 2, y: 340 + 270 * 0.06, w: kw, h: 270 * 0.33, black: true }; });
const KNOBS = [["MASTER", 105, 100, 24, "master"], ["SELECT", 215, 100, 24, "SELECT"], ["PRESETS", 105, 205, 24, "PRESETS"], ["ALGORITHM", 215, 205, 24, "ALGORITHM"],
  ...[0, 1, 2, 3].map((i) => [`KNOB${i + 1}`, 610 + i * 104, 100, 22, `KNOB${i + 1}`])];
KNOBS.forEach(([n, x, y, r, role]) => { LAYOUT.knobs[n] = { x, y, r, role }; });
export const center = (c) => ({ x: c.x + (c.w || 0) / 2, y: c.y + (c.h || 0) / 2 });

function Text({ x, y, s, size, fill = LABEL }) {
  return <text x={x} y={y} textAnchor="middle" fontFamily={FONT} fontSize={size} fontWeight={600} fill={fill} letterSpacing=".05em">{s}</text>;
}

// frame: { btn, keys, litBtn, litKeys }; angles: { SELECT: degrees, ... }
export function Panel({ frame, angles = {}, children }) {
  const bit = (m, b) => !!(m & (1 << b));
  return (
    <svg viewBox="0 0 1000 640" width={1000} height={640} style={{ display: "block", overflow: "visible" }}>
      <rect x={4} y={4} width={992} height={632} rx={44} fill={BODY} />
      {KNOBS.map(([n, x, y, r, role]) => (
        <g key={n}>
          <circle cx={x} cy={y} r={r + 5} fill={TRAY} />
          <circle cx={x} cy={y} r={r} fill={KNOB} stroke="#44474c" strokeWidth={2} />
          <line x1={x} y1={y - r + 5} x2={x} y2={y - r + 15} stroke={LABEL} strokeWidth={3} strokeLinecap="round" transform={`rotate(${angles[role] ?? 0} ${x} ${y})`} />
          <Text x={x} y={y - r - 12} s={n} size={17} />
        </g>
      ))}
      <rect x={58} y={258} width={210} height={58} rx={14} fill={TRAY} />
      <rect x={300} y={44} width={236} height={236} rx={26} fill="#111214" />
      <rect x={568} y={160} width={400} height={140} rx={18} fill={TRAY} />
      {Object.entries(LAYOUT.buttons).map(([n, b]) => {
        const lit = bit(frame.litBtn, BTN[n]), held = bit(frame.btn, BTN[n]);
        const two = n === "PLAY" ? "STOP" : null, tc = lit ? "#241005" : LABEL;
        return (
          <g key={n}>
            <rect x={b.x} y={b.y} width={b.w} height={b.h} rx={9} fill={lit ? "#ffd447" : BTNC} stroke={held ? "#ffd447" : "#4a4d52"} strokeWidth={held ? 3 : 1.5} />
            {two ? <><Text x={b.x + b.w / 2} y={b.y + b.h / 2 - 1} s={n} size={12} fill={tc} /><Text x={b.x + b.w / 2} y={b.y + b.h / 2 + 12} s={two} size={12} fill={tc} /></>
              : <Text x={b.x + b.w / 2} y={b.y + b.h / 2 + 5} s={n} size={15} fill={tc} />}
          </g>
        );
      })}
      <rect x={36} y={340} width={928} height={270} rx={18} fill={TRAY} />
      {[...WHITE_K, ...BLACK_K].map((k) => {
        const c = LAYOUT.keys[k], lit = bit(frame.litKeys, k), held = bit(frame.keys, k);
        const fill = lit ? (c.black ? "#9a3cff" : "#c99cff") : c.black ? KEY_B : KEY_W;
        return <rect key={k} x={c.x} y={c.y} width={c.w} height={c.h} rx={c.w / 2} fill={fill} stroke={held ? "#ffd447" : "none"} strokeWidth={held ? 3 : 0} />;
      })}
      {children}
    </svg>
  );
}
