// SPDX-License-Identifier: GPL-3.0-only
// The share card (the pieces and library pages): a 1200 x 630 picture of a piece or a set, to post with its link. The piece's drum pattern as rings (a lane
// a ring, the kick inside, sixteen steps round), its name, title, maker, what it is based on, its tempo and key; a set's places as one ring. In the site's colours.

const CARD_INK = "#f6ece0", CARD_MUTED = "#bba79a", CARD_RULE = "#4a2a35", CARD_GROUND = "#130a0e";
const CARD_LANES = ["#e0344f", "#ff8f8f", "#ffd447", "#f2e6c4", "#c9a7ff", "#7fd1c4"];   // kick, snare, clap, hat, open hat, perc

// c: {tag, title, by, based, line, lanes: [16-bit masks], url}
export async function drawCard(canvas, c) {
  if (document.fonts && document.fonts.load)                       // the faces the card uses (a page only fetches those its text uses)
    await Promise.all(['700 30px "Chakra Petch"', '600 30px "Chakra Petch"', '500 30px "Barlow Semi Condensed"'].map((f) => document.fonts.load(f).catch(() => null)));
  canvas.width = 1200; canvas.height = 630;
  const g = canvas.getContext("2d");
  const head = (w, px) => `${w} ${px}px "Chakra Petch", "Helvetica Neue", Arial, sans-serif`, body = (px) => `500 ${px}px "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif`;
  g.fillStyle = CARD_GROUND; g.fillRect(0, 0, 1200, 630);
  g.strokeStyle = "rgba(224, 52, 79, 0.08)"; g.lineWidth = 1;           // a floor of lines, as the Synth look's
  for (let y = 380; y < 630; y += 18 + (y - 380) * 0.18) { g.beginPath(); g.moveTo(0, y); g.lineTo(1200, y); g.stroke(); }
  for (let x = -600; x <= 1800; x += 120) { g.beginPath(); g.moveTo(600, 380); g.lineTo(x, 630); g.stroke(); }
  // the rings
  const cx = 905, cy = 315, n = c.lanes.length || 1;
  for (let l = 0; l < n; l++) {
    const r = n === 1 ? 230 : 104 + l * (150 / (n - 1));
    g.strokeStyle = CARD_RULE; g.lineWidth = 2;
    g.beginPath(); g.arc(cx, cy, r, 0, Math.PI * 2); g.stroke();
    for (let s = 0; s < 16; s++) {
      const a = -Math.PI / 2 + (s / 16) * Math.PI * 2, x = cx + r * Math.cos(a), y = cy + r * Math.sin(a);
      const on = (c.lanes[l] >> s) & 1;
      g.beginPath(); g.arc(x, y, on ? 11 : 4, 0, Math.PI * 2);
      g.fillStyle = on ? CARD_LANES[l % CARD_LANES.length] : CARD_RULE;
      if (on) { g.shadowColor = g.fillStyle; g.shadowBlur = 16; }
      g.fill(); g.shadowBlur = 0;
    }
  }
  g.fillStyle = CARD_INK; g.font = head(700, n === 1 ? 64 : 30); g.textAlign = "center";
  g.fillText(c.center || "", cx, cy + (n === 1 ? 22 : 11));
  g.textAlign = "left";
  // the words
  g.fillStyle = "#ffd447"; g.fillRect(64, 72, 26, 5); g.fillStyle = "#e0344f"; g.fillRect(90, 72, 42, 5);
  g.fillStyle = CARD_INK; g.font = head(700, 30); g.fillText("PERF-1", 146, 84);
  const fit = (txt, font, max) => { g.font = font; let t = txt || ""; while (t && g.measureText(t).width > max) t = t.slice(0, -2) + "…"; return t; };
  g.fillStyle = "#f2e6c4"; g.fillText(fit(c.tag, head(700, 76), 600), 64, 196);
  let y = 196;
  if (c.title) { y += 62; g.fillStyle = "#ffd447"; g.fillText(fit(c.title, head(600, 46), 610), 64, y); }
  if (c.by) { y += 44; g.fillStyle = CARD_INK; g.fillText(fit("by " + c.by, body(32), 610), 64, y); }
  if (c.based) { y += 36; g.fillStyle = CARD_MUTED; g.fillText(fit("based on " + c.based, body(26), 610), 64, y); }
  y += 52; g.fillStyle = CARD_INK; g.fillText(fit(c.line, body(30), 610), 64, y);
  g.fillStyle = CARD_MUTED; g.fillText(fit(c.url || "", body(24), 1080), 64, 586);
}
