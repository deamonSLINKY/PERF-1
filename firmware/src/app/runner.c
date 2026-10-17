/* SPDX-License-Identifier: GPL-3.0-only */
/* HYP-1's secret: KICK RUNNER. Hold SEL and press EDIT (again, or HOME, to leave).
 *
 * A wireframe tunnel rushes at you, a ring a beat. You ride its floor: SELECT (or OCT- / OCT+) steps you
 * round its eight lanes, and the tunnel turns so you stay at the bottom. The kicks send what's coming:
 * on a kick a row is born at the far end and reaches you eight beats later, on the beat: walls to miss
 * and gates to take. The track starts stripped to the kick. Each gate brings a part back (the hats, the
 * snare, the acid, the drone, the pad: the performance mutes, lifted one by one); a wall takes the last
 * one away and the filters close for a bar. Crash with only the kick left and you lose a ship (three;
 * the first two bars send gates only). The best run lasts until the FM-1 is switched off.
 *
 * Included by ui.c: it draws with ui.c's helpers on the whole screen, two 240x120 halves a frame, and
 * sends the effects mask itself while it runs (ui.c's FX menu sleeps). */

#define KR_LANES 8
#define KR_DEPTH 9                               /* rings in view; a row is born KR_AHEAD beats out */
#define KR_AHEAD 8.0f
#define KR_NROW 16
#define KR_CX 120.0f
#define KR_CY 108.0f

static const uint8_t KR_ORDER[5] = {1, 2, 4, 3, 5};   /* the parts that come back: hats, snare, acid, drone, pad */
static const char *const KR_PART[5] = {"HATS", "SNARE", "ACID", "DRONE", "PAD"};

static struct {
    uint8_t on, over, lane, layer, ships;
    float start_beat;                            /* the run's first beat: two bars of gates only */
    float view;                                  /* the tunnel's turn, easing to the lane (turns) */
    float beat;                                  /* the beat now, smooth between sixteenths */
    uint32_t s16, s16_ms, step_ms;               /* the last sixteenth seen, when, and how long they last */
    float kick_prev, kick;                       /* the kick's glow last frame; the ship's pulse */
    float lp_until;                              /* a crash: the filters closed until this beat */
    float flash, crash;
    uint32_t score, best, gates, crashes, rows_born;
    uint32_t banner_until, start_ms, over_ms, rng;
    char banner[20];
    struct { float beat; uint8_t wall, gate, done, taken; } row[KR_NROW];
} kr;

static uint32_t kr_rnd(void)
{
    kr.rng ^= kr.rng << 13;
    kr.rng ^= kr.rng >> 17;
    kr.rng ^= kr.rng << 5;
    return kr.rng;
}
static float kr_frand(void) { return (float)(kr_rnd() >> 8) * (1.0f / 16777216.0f); }
static float kr_cos(float t) { return fm_sin_turns(t + 0.25f); }

static void kr_say(const char *s, uint32_t ms)
{
    int k = 0;
    while (s[k] && k < 19) {
        kr.banner[k] = s[k];
        k++;
    }
    kr.banner[k] = 0;
    kr.banner_until = plat_ms() + ms;
}

/* the mask: every part not yet back muted; the filters closed after a crash */
static void kr_fx(void)
{
    uint32_t m = 0;
    int k;
    if (kr.on && !kr.over) {
        for (k = kr.layer; k < 5; k++)
            m |= 1u << (FX_MUTE0 + KR_ORDER[k]);
        if (kr.beat < kr.lp_until)
            m |= 1u << FX_LP;
    }
    en_fx(kr.on ? m : ui.fx_mask);
}

static void kr_reset(void)
{
    memset(kr.row, 0, sizeof kr.row);
    kr.over = 0;
    kr.layer = 0;
    kr.ships = 3;
    kr.start_beat = kr.beat;
    kr.score = kr.gates = kr.crashes = 0;
    kr.lp_until = 0.0f;
    kr.flash = kr.crash = 0.0f;
    kr_say("KICK RUNNER", 2500u);
}

static void runner_start(void)
{
    kr.on = 1;
    kr.rng ^= plat_ms() * 2654435761u | 1u;
    kr.lane = 0;
    kr.view = 0.25f - 0.5f / (float)KR_LANES;
    kr.s16 = en_bars;
    kr.s16_ms = plat_ms();
    if (!kr.step_ms)
        kr.step_ms = 125u;
    kr.beat = (float)kr.s16 * 0.25f;
    kr.start_ms = plat_ms();
    kr_reset();
    ui.fx_menu = 0;
    ui.help = 0;
    lcd_fill(0, 0, 240, 240, RGB(0, 0, 0));
    kr_fx();
}

static void runner_stop(void)
{
    kr.on = 0;
    kr_fx();                                     /* the effects back as the FX menu had them */
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
    ui.act_t = plat_ms();
    lcd_fill(0, 0, 240, 240, K_BG);
}

/* a kick: a row at the far end, to arrive on the beat KR_AHEAD beats on */
static void kr_spawn(void)
{
    float at = fm_floorf(kr.beat + 0.5f) + KR_AHEAD;
    int k, free_ = -1, slot = -1, n;
    uint8_t wall = 0, gate = 0;
    for (k = 0; k < KR_NROW; k++) {
        if (kr.row[k].beat == at && (kr.row[k].wall | kr.row[k].gate))
            return;                              /* one row a beat */
        if (slot < 0 && (kr.row[k].done || !(kr.row[k].wall | kr.row[k].gate)))
            slot = k;
    }
    if (slot < 0)
        return;
    if (at - KR_AHEAD >= kr.start_beat + 8.0f && kr_frand() < 0.42f + 0.06f * (float)kr.layer) {   /* walls: one to three lanes */
        int w = 1 + (int)(kr_frand() * (1.0f + 0.5f * (float)kr.layer)), s = (int)(kr_frand() * KR_LANES);
        if (w > 3)
            w = 3;
        for (n = 0; n < w; n++)
            wall |= (uint8_t)(1u << ((s + n) % KR_LANES));
    }
    if (kr_frand() < 0.34f) {                    /* a gate in a lane left open, near where you are */
        for (n = 0; n < 6; n++) {
            int l = (kr.lane + (int)(kr_frand() * 5.0f) - 2 + KR_LANES) % KR_LANES;
            if (!(wall >> l & 1u)) {
                free_ = l;
                break;
            }
        }
        if (free_ >= 0)
            gate = (uint8_t)(1u << free_);
    }
    if (!(wall | gate))
        return;
    kr.row[slot].beat = at;
    kr.row[slot].wall = wall;
    kr.row[slot].gate = gate;
    kr.row[slot].done = kr.row[slot].taken = 0;
    kr.rows_born++;
}

static void runner_update(void)
{
    uint32_t now = plat_ms();
    int k;
    if (en_bars != kr.s16) {                     /* the clock: a sixteenth went by */
        uint32_t d = now - kr.s16_ms, steps = en_bars - kr.s16;
        if (steps && steps < 8u && d > 20u && d < 2000u)
            kr.step_ms = (kr.step_ms * 3u + d / steps) / 4u;
        kr.s16 = en_bars;
        kr.s16_ms = now;
    }
    {
        float frac = en_playing ? fm_minf((float)(now - kr.s16_ms) / (float)(kr.step_ms ? kr.step_ms : 125u), 0.999f) : 0.0f;
        float b = ((float)kr.s16 + frac) * 0.25f;
        if (b > kr.beat || b < kr.beat - 4.0f)
            kr.beat = b;
    }
    {   /* the tunnel turns so the lane you're in is at the bottom */
        float want = 0.25f - ((float)kr.lane + 0.5f) / (float)KR_LANES, d = want - kr.view;
        d -= fm_floorf(d + 0.5f);
        kr.view += d * 0.35f;
    }
    if (en_drum_glow[0] > 0.8f && kr.kick_prev < 0.6f) {   /* a kick */
        kr.kick = 1.0f;
        if (en_playing && !kr.over)
            kr_spawn();
    }
    kr.kick_prev = en_drum_glow[0];
    kr.kick *= 0.82f;
    kr.flash *= 0.85f;
    kr.crash *= 0.9f;
    for (k = 0; k < KR_NROW; k++) {              /* rows reaching you */
        if (kr.row[k].done || !(kr.row[k].wall | kr.row[k].gate) || kr.beat < kr.row[k].beat)
            continue;
        kr.row[k].done = 1;
        if (kr.over)
            continue;
        if (kr.row[k].wall >> kr.lane & 1u) {
            kr.crashes++;
            kr.crash = 1.0f;
            kr.lp_until = kr.beat + 4.0f;
            if (!kr.layer) {                     /* only the kick: a ship lost */
                if (kr.ships)
                    kr.ships--;
                if (!kr.ships) {
                    kr.over = 1;
                    kr.over_ms = now;
                    if (kr.score > kr.best)
                        kr.best = kr.score;
                } else {
                    kr_say(kr.ships == 1 ? "LAST SHIP" : "SHIP LOST", 1400u);
                }
            } else {
                kr.layer--;
                {
                    char t[20] = "LOST THE ", *q = t + 9;
                    const char *w = KR_PART[kr.layer];
                    while (*w)
                        *q++ = *w++;
                    *q = 0;
                    kr_say(t, 1400u);
                }
            }
        } else if (kr.row[k].gate >> kr.lane & 1u) {
            kr.row[k].taken = 1;
            kr.gates++;
            kr.flash = 1.0f;
            kr.score += 10u * (uint32_t)(kr.layer + 1);
            if (kr.layer < 5) {
                char t[20], *q = t;
                const char *w = KR_PART[kr.layer];
                kr.layer++;
                while (*w)
                    *q++ = *w++;
                w = kr.layer == 5 ? " - FULL TRACK" : " IN";
                while (*w)
                    *q++ = *w++;
                *q = 0;
                kr_say(t, 1400u);
            } else {
                kr.score += 40u;                 /* the whole track going: gates are worth more */
            }
        } else {
            kr.score += (uint32_t)kr.layer;      /* a row behind you */
        }
    }
    kr_fx();
}

/* ------------------------------------------------------------ drawing --- */
static void kr_glow(float cx, float cy, float r0, float r1, uint16_t c, float s)   /* c fading out from r0 to r1 */
{
    int32_t x0 = (int32_t)fm_floorf(cx - r1), x1 = (int32_t)(cx + r1 + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r1), y1 = (int32_t)(cy + r1 + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy), a;
            if (d >= r1)
                continue;
            a = d <= r0 ? 1.0f : 1.0f - (d - r0) / (r1 - r0);
            px_blend(x, y, c, (uint32_t)(a * a * s * 16.0f + 0.5f));
        }
}
static void kr_seg(float x0, float y0, float x1, float y1, uint16_t c, float s)
{
    float dx = x1 - x0, dy = y1 - y0, ax = dx < 0.0f ? -dx : dx, ay = dy < 0.0f ? -dy : dy, t, step;
    int n, k;
    if (s <= 0.02f)
        return;
    if (ax >= ay) {
        if (x1 < x0) {
            t = x0, x0 = x1, x1 = t;
            t = y0, y0 = y1, y1 = t;
        }
        n = (int)(x1 - x0) + 1;
        step = ax > 0.0f ? (y1 - y0) / (x1 - x0) : 0.0f;
        for (k = 0; k <= n; k++) {
            float x = x0 + (float)k, y = y0 + step * (float)k, f;
            int32_t iy;
            if (x > x1 + 0.5f)
                break;
            iy = (int32_t)fm_floorf(y);
            f = y - (float)iy;
            px_blend((int32_t)x, iy, c, (uint32_t)((1.0f - f) * s * 16.0f + 0.5f));
            px_blend((int32_t)x, iy + 1, c, (uint32_t)(f * s * 16.0f + 0.5f));
        }
    } else {
        if (y1 < y0) {
            t = x0, x0 = x1, x1 = t;
            t = y0, y0 = y1, y1 = t;
        }
        n = (int)(y1 - y0) + 1;
        step = (x1 - x0) / (y1 - y0);
        for (k = 0; k <= n; k++) {
            float y = y0 + (float)k, x = x0 + step * (float)k, f;
            int32_t ix;
            if (y > y1 + 0.5f)
                break;
            ix = (int32_t)fm_floorf(x);
            f = x - (float)ix;
            px_blend(ix, (int32_t)y, c, (uint32_t)((1.0f - f) * s * 16.0f + 0.5f));
            px_blend(ix + 1, (int32_t)y, c, (uint32_t)(f * s * 16.0f + 0.5f));
        }
    }
}

static float kr_r(float z) { return 118.0f / (1.0f + 0.62f * z); }   /* a ring's radius at depth z (beats away) */
static void kr_pt(float z, float lane, float *x, float *y)
{
    float a = kr.view + lane / (float)KR_LANES, r = kr_r(z);
    *x = KR_CX + r * kr_cos(a);
    *y = KR_CY + r * fm_sin_turns(a);
}

static void runner_draw(int half)
{
    uint32_t now = plat_ms();
    float frac = kr.beat - fm_floorf(kr.beat);
    uint16_t line = K_PULSE, wallc = RGB(255, 64, 48), gatec = RGB(70, 255, 160), bg = RGB(0, 0, 0);   /* walls red, gates green, whatever the theme */
    int k, i;
    if (kr.crash > 0.05f)
        bg = mix(bg, RGB(255, 64, 48), q16(kr.crash * 0.25f));
    cv_begin(240, 120, bg);
    cv_oy = half ? -120 : 0;
    cv_y0 = 0;
    cv_y1 = 120;
    for (i = 0; i < KR_LANES; i++) {             /* the lane lines, out to the far end */
        float x0, y0, x1, y1;
        kr_pt(0.0f, (float)i, &x0, &y0);
        kr_pt((float)KR_DEPTH, (float)i, &x1, &y1);
        kr_seg(x0, y0, x1, y1, line, 0.35f);
    }
    for (k = KR_DEPTH; k >= 0; k--) {            /* the rings, a beat apart, coming at you */
        float z = (float)k - frac, s;
        if (z < 0.0f)
            continue;
        s = (1.0f - z / (float)(KR_DEPTH + 1)) * (0.45f + 0.55f * (k == 0 ? kr.kick : 0.0f) + 0.2f);
        for (i = 0; i < KR_LANES; i++) {
            float x0, y0, x1, y1;
            kr_pt(z, (float)i, &x0, &y0);
            kr_pt(z, (float)(i + 1), &x1, &y1);
            kr_seg(x0, y0, x1, y1, line, fm_minf(s, 1.0f));
        }
    }
    for (k = 0; k < KR_NROW; k++) {              /* walls and gates, far ones first */
        float z = kr.row[k].beat - kr.beat;
        if (!(kr.row[k].wall | kr.row[k].gate) || z < -0.3f || z > (float)KR_DEPTH)
            continue;
        for (i = 0; i < KR_LANES; i++) {
            float x0, y0, x1, y1, fade = 1.0f - fm_maxf(z, 0.0f) / (float)(KR_DEPTH + 1);
            int t;
            if (kr.row[k].wall >> i & 1u) {
                for (t = 0; t < 6; t++) {        /* a slab: six edges across its depth */
                    float zz = fm_maxf(z + 0.07f * (float)t, 0.0f);
                    kr_pt(zz, (float)i + 0.04f, &x0, &y0);
                    kr_pt(zz, (float)i + 0.96f, &x1, &y1);
                    kr_seg(x0, y0, x1, y1, wallc, fade);
                }
                kr_pt(fm_maxf(z, 0.0f), (float)i + 0.5f, &x0, &y0);   /* and its height, toward the middle */
                {
                    float zz = fm_maxf(z, 0.0f), r = kr_r(zz), a = kr.view + ((float)i + 0.5f) / (float)KR_LANES;
                    x1 = KR_CX + r * 0.72f * kr_cos(a);
                    y1 = KR_CY + r * 0.72f * fm_sin_turns(a);
                }
                kr_seg(x0, y0, x1, y1, wallc, fade * 0.8f);
            }
            if ((kr.row[k].gate >> i & 1u) && !kr.row[k].taken) {
                float zz = fm_maxf(z, 0.0f);
                kr_pt(zz, (float)i + 0.12f, &x0, &y0);
                kr_pt(zz, (float)i + 0.88f, &x1, &y1);
                kr_seg(x0, y0, x1, y1, gatec, fade);
                {   /* a hoop: the gate's arch */
                    float mx, my, r = kr_r(zz), a = kr.view + ((float)i + 0.5f) / (float)KR_LANES;
                    mx = KR_CX + r * 0.8f * kr_cos(a);
                    my = KR_CY + r * 0.8f * fm_sin_turns(a);
                    kr_seg(x0, y0, mx, my, gatec, fade);
                    kr_seg(x1, y1, mx, my, gatec, fade);
                    kr_glow(mx, my, 1.0f, 3.0f + 5.0f * fade, gatec, 0.3f * fade);
                }
            }
        }
    }
    if (!kr.over) {                              /* you: a claw on the floor of the nearest ring */
        float a = kr.view + ((float)kr.lane + 0.5f) / (float)KR_LANES, r = kr_r(0.12f), c = kr_cos(a), s = fm_sin_turns(a);
        float px = KR_CX + r * c, py = KR_CY + r * s, ux = -c, uy = -s, vx = -s, vy = c;   /* u: inward, v: across */
        uint16_t sc = kr.flash > 0.1f ? mix(K_TEXT, gatec, q16(kr.flash)) : K_TEXT;
        kr_seg(px + vx * 11.0f, py + vy * 11.0f, px + ux * 9.0f, py + uy * 9.0f, sc, 1.0f);
        kr_seg(px - vx * 11.0f, py - vy * 11.0f, px + ux * 9.0f, py + uy * 9.0f, sc, 1.0f);
        kr_seg(px + vx * 11.0f, py + vy * 11.0f, px + vx * 4.0f + ux * 2.0f, py + vy * 4.0f + uy * 2.0f, sc, 1.0f);
        kr_seg(px - vx * 11.0f, py - vy * 11.0f, px - vx * 4.0f + ux * 2.0f, py - vy * 4.0f + uy * 2.0f, sc, 1.0f);
        if (kr.kick > 0.05f)
            kr_glow(px, py, 1.0f, 6.0f + 8.0f * kr.kick, K_ACC, 0.35f * kr.kick);
    }
    {   /* the parts back so far, top left; the score, top right */
        static const char *const TAG[6] = {"K", "H", "S", "A", "D", "P"};
        char t[24] = "";
        for (k = 0; k < 6; k++) {
            int lit = k == 0 || k <= kr.layer;
            cv_round(6 + k * 15, 5, 13, 13, 3, lit ? K_ACC : RGB(30, 30, 34));
            text_c(12 + k * 15, 5, &FONT_XS, TAG[k], lit ? RGB(0, 0, 0) : K_DIM);
        }
        itoa_u(kr.score, t);
        cv_text(234 - text_w(&FONT_S, t), 3, &FONT_S, t, K_TEXT);
        for (k = 0; k < kr.ships; k++) {         /* the ships left, under the score */
            float x = 228.0f - (float)k * 13.0f, y = 30.0f;
            kr_seg(x - 5.0f, y + 3.0f, x, y - 4.0f, K_TEXT, 1.0f);
            kr_seg(x + 5.0f, y + 3.0f, x, y - 4.0f, K_TEXT, 1.0f);
        }
    }
    if (!en_playing && !kr.over)
        text_c(120, 222, &FONT_S, "PRESS PLAY", K_DIM);
    else if (now - kr.start_ms < 4500u && !kr.over)
        text_c(120, 222, &FONT_S, "SELECT STEERS  TAKE THE GATES", K_DIM);
    if (kr.over) {
        char t[32] = "SCORE ", *q;
        text_c(120, 70, &FONT_B, "GAME OVER", RGB(255, 64, 48));
        itoa_u(kr.score, t + 6);
        for (q = t; *q; q++)
            ;
        q[0] = ' ', q[1] = ' ', q[2] = 'B', q[3] = 'E', q[4] = 'S', q[5] = 'T', q[6] = ' ';
        itoa_u(kr.best, q + 7);
        text_c(120, 104, &FONT_S, t, K_TEXT);
        text_c(120, 128, &FONT_S, "A KEY: AGAIN   HOME: BACK", K_DIM);
    } else if (now < kr.banner_until) {
        text_c(120, 70, &FONT_B, kr.banner, K_ACC);
    }
}

/* the lights: PLAY; black keys 1-6 the drums; white keys 1-8 the lanes, yours lit, walls coming blinking */
static void runner_leds(void)
{
    uint32_t b = 0, k = 0, i;
    if (en_playing)
        b |= 1u << B_PLAY;
    for (i = 0; i < EN_NDRUM && i < 11u; i++)
        if (en_drum_glow[i] > 0.35f)
            k |= 1u << BLACK_KEY[i];
    if (!kr.over)
        k |= 1u << WHITE_KEY[kr.lane];
    for (i = 0; i < KR_NROW; i++) {
        float z = kr.row[i].beat - kr.beat;
        if (!kr.row[i].done && z >= 0.0f && z < 2.0f && (ui.frame / 6u) % 2u) {
            uint32_t l;
            for (l = 0; l < KR_LANES; l++)
                if (kr.row[i].wall >> l & 1u)
                    k |= 1u << WHITE_KEY[l];
        }
    }
    if (kr.layer >= 5)
        b |= 1u << B_FX;
    plat_glow(0);
    plat_leds(b, k);
}

/* with the game on, input() hands everything here: SELECT and OCT- / OCT+ steer, a key starts again after
 * a game over, HOME (or SEL + EDIT again) leaves, PLAY still plays and stops */
static void runner_input(uint32_t btn, uint32_t keys)
{
    uint32_t press = btn & ~ui.btn, i;
    int32_t e;
    int r;
    for (i = 0; i < NB; i++)
        if (press >> i & 1u)
            ui.down_t[i] = plat_ms();
    if ((keys & ~ui.keys) && kr.over && plat_ms() - kr.over_ms > 900u)
        kr_reset();
    if ((e = plat_enc(EN_SELECT)) != 0)
        kr.lane = (uint8_t)(((int)kr.lane + (e > 0 ? 1 : -1) + KR_LANES) % KR_LANES);
    for (r = 0; r < NE; r++)
        if (r != EN_SELECT)
            (void)plat_enc(r);
    if (press >> B_OCTDN & 1u)
        kr.lane = (uint8_t)((kr.lane + KR_LANES - 1) % KR_LANES);
    if (press >> B_OCTUP & 1u)
        kr.lane = (uint8_t)((kr.lane + 1) % KR_LANES);
    if (btn || keys)
        ui.act_t = plat_ms();
    if (press >> B_PLAY & 1u)
        en_play(!en_playing);
    ui.keys = keys;
    ui.shift_keys = keys;                        /* held keys, once back: not notes */
    if ((press >> B_HOME & 1u) || ((press >> B_EDIT & 1u) && (btn >> B_SEL & 1u))) {
        ui.btn_used |= btn;                      /* their release does nothing */
        ui.btn = btn;
        runner_stop();
        midi_in();
        return;
    }
    ui.btn = btn;
    midi_in();
}
