/* SPDX-License-Identifier: GPL-3.0-only */
/* PERF-1 visualizers: the main area as a light show that plays with the music (HOME double-tapped; HOME again leaves;
 * the SELECT dial chooses). Part of the unity build, included by ui.c just before draw_main. Fifteen frames a second, in
 * the theme's colours. Everything is drawn from what the engine already tells the screen (the drums' glows, the layers'
 * levels, the chord, the pattern, the section): no audio is analysed. The knobs stay the FX knobs, ALGORITHM the mood.
 *   PLASMA  a colour field that swirls faster with the energy and flashes on the kick
 *   TUNNEL  rings flying out of the middle, a pulse in it on every kick
 *   BARS    an equaliser: lows from the kick and the drone, mids from the pad and the snare, highs from the hats and the acid
 *   STARS   a warp field: faster in a build, a flash on the kick
 *   DISCO   the mascot dancing on a floor that lights with the drum pattern
 *   LIFE    a cellular automaton that the drums feed (the kick a cluster, the snare a spray, a hat a spark)
 *   ORBIT   a harmonograph drawn from the chord's notes: it morphs with every chord
 *   AUTO    one of them, a new one on every section change and every 32 bars */
#define VZ_W 240
#define VZ_H 144
#define VZ_CW 40                                /* the cell grids: 6 pixel cells */
#define VZ_CH 24
#define VZ_NSTAR 64

static struct {
    float tt, ph, ph2, dt;                     /* time (tempo scaled), two phases, the frame's seconds */
    float kick, snare, hat, en;                /* the frame's signals */
    float bars[24], caps[24], spec[24];
    float sx[VZ_NSTAR], sy[VZ_NSTAR], sz[VZ_NSTAR];
    float cf[3];
    uint8_t life[2][VZ_CH][VZ_CW];
    uint8_t lcur, last_step, mode_seen, auto_cur, sec_seen, inited;
    uint32_t seed, last_ms, auto_bar, life_ms;
    uint16_t lut[64];
    float dh, dhv, lift, lv, kprev, sprev;     /* the dancer: the head's spring (px, + up) and its speed; the hop's height and speed */
    float conf[24][5];                         /* confetti: x, y, vx, vy, colour */
    uint8_t dsec, dmove, dleft, dother;        /* the section last seen; the move now; two-bar units left of it; 0 jumping, else the other move */
    uint32_t dunit;                            /* the two-bar unit last counted */
    uint32_t drop_bar;                         /* the sixteenth the drop came on */
} vz;

static uint32_t vz_rnd(void)
{
    vz.seed = vz.seed * 1664525u + 1013904223u;
    return vz.seed >> 8;
}
static float vz_rf(void) { return (float)(vz_rnd() & 0xFFFFu) * (1.0f / 65536.0f); }
static uint16_t vpal(float x)                   /* the theme's colours, round and round */
{
    return vz.lut[(int)((x - fm_floorf(x)) * 63.99f)];
}
static void vz_lut(void)
{
    const uint16_t stop[6] = {K_PAD_T, K_PAD, K_ACID, K_TEXT, K_ACC, K_PAD_T};
    int i;
    for (i = 0; i < 64; i++) {
        const int s = i * 5 / 64, t = (i * 5 % 64) * 16 / 64;
        vz.lut[i] = mix(stop[s], stop[s + 1], t);
    }
}

static void vz_signals(void)
{
    uint32_t now = plat_ms();
    float dt = (float)(now - vz.last_ms) * 0.001f;
    vz.last_ms = now;
    vz.dt = fm_clampf(dt, 0.001f, 0.12f);
    vz.kick = fm_clampf(en_drum_glow[0], 0.0f, 1.0f);
    vz.snare = fm_clampf(en_drum_glow[1], 0.0f, 1.0f);
    vz.hat = fm_clampf(en_drum_glow[3], 0.0f, 1.0f);
    vz.en = fm_clampf(0.3f * (en_pad_level + en_bass_level + en_acid_level) + 0.5f * fm_maxf(vz.kick, vz.snare), 0.0f, 1.0f);
    if (!en_playing)
        vz.kick = vz.snare = vz.hat = vz.en = 0.0f;
    vz.tt += vz.dt * (0.5f + (float)proj.par[P_TEMPO] * 0.008f) * (1.0f + vz.en);
    vz.ph += vz.dt * (0.02f + 0.08f * vz.en);
    vz.ph2 += vz.dt * (0.2f + 0.6f * vz.kick);
    if (vz.ph > 1000.0f)
        vz.ph -= 1000.0f;
    if (vz.ph2 > 1000.0f)
        vz.ph2 -= 1000.0f;
    if (vz.tt > 1000.0f)
        vz.tt -= 1000.0f;
}

static void vz_plasma(void)
{
    const float t = vz.tt;
    int cx, cy;
    for (cy = 0; cy < VZ_CH; cy++)
        for (cx = 0; cx < VZ_CW; cx++) {
            const float x = (float)cx, y = (float)cy, dx = x - 20.0f, dy = (y - 12.0f) * 1.6f;
            float v = fm_sinf(x * 0.31f + t * 1.1f) + fm_sinf(y * 0.43f - t * 0.9f) + fm_sinf((x + y) * 0.21f + t * 0.7f) +
                      fm_sinf(fm_sqrtf(dx * dx + dy * dy) * 0.45f - t * 1.6f);
            cv_rect(cx * 6, cy * 6, 6, 6, vpal(v * 0.125f + 0.5f + vz.ph + vz.kick * 0.18f));
        }
}

static void vz_tunnel(void)
{
    static float ring;
    int k, j;
    ring += vz.dt * (0.16f + 0.9f * vz.kick + (en_sec == EN_SEC_BUILD ? 0.5f : 0.0f));
    for (k = 0; k < 10; k++) {
        float f = ring + (float)k * 0.1f;
        float r, rot;
        int n, th;
        uint16_t c;
        f -= fm_floorf(f);
        r = f * f * 104.0f + 3.0f;
        n = 14 + (int)(r * 0.9f);
        th = 1 + (int)(f * 3.0f);
        c = vpal(f * 0.55f + vz.ph * 2.0f);
        rot = (float)k * 0.5f + vz.ph2 * 0.3f;
        for (j = 0; j < n; j++) {
            const float a = (float)j * (FM_TWO_PI / (float)n) + rot;
            cv_rect(120 + (int32_t)(fm_cosf(a) * r * 1.15f), 72 + (int32_t)(fm_sinf(a) * r * 0.85f), th, th, c);
        }
    }
    cv_disc(120, 72, 4 + (int32_t)(vz.kick * 16.0f), vpal(vz.ph * 3.0f + 0.7f));
    cv_disc(120, 72, 2 + (int32_t)(vz.kick * 8.0f), K_TEXT);
}

static void vz_bars(void)                       /* the real thing: the master's spectrum (en_spectrum), 24 bands */
{
    int i, j;
    en_spectrum(vz.spec, 24);
    for (i = 0; i < 24; i++) {
        const float u = (float)i * (1.0f / 23.0f);
        const float tg = vz.spec[i];
        vz.bars[i] = tg > vz.bars[i] ? tg : fm_maxf(vz.bars[i] - vz.dt * 1.8f, 0.0f);
        vz.caps[i] = vz.bars[i] > vz.caps[i] ? vz.bars[i] : fm_maxf(vz.caps[i] - vz.dt * 0.5f, 0.0f);
        for (j = 0; j < (int)(vz.bars[i] * 14.0f + 0.5f); j++)
            cv_rect(i * 10 + 1, 132 - j * 10, 8, 8, vpal((float)j * 0.035f + u * 0.3f + vz.ph));
        cv_rect(i * 10 + 1, 132 - (int32_t)(vz.caps[i] * 14.0f + 0.5f) * 10, 8, 3, K_TEXT);
    }
}

static void vz_stars_init(void)
{
    int i;
    for (i = 0; i < VZ_NSTAR; i++) {
        vz.sx[i] = vz_rf() * 2.0f - 1.0f;
        vz.sy[i] = vz_rf() * 2.0f - 1.0f;
        vz.sz[i] = 0.1f + vz_rf() * 0.9f;
    }
}
static void vz_stars(void)
{
    const float sp = 0.25f + 1.1f * vz.en + (en_sec == EN_SEC_BUILD ? 0.9f : 0.0f) + vz.kick * 0.8f;
    int i;
    for (i = 0; i < VZ_NSTAR; i++) {
        const float z0 = vz.sz[i], z1 = z0 - vz.dt * sp * 0.5f;
        const int32_t x0 = 120 + (int32_t)(vz.sx[i] / z0 * 60.0f), y0 = 72 + (int32_t)(vz.sy[i] / z0 * 40.0f),
                      x1 = 120 + (int32_t)(vz.sx[i] / (z1 > 0.02f ? z1 : 0.02f) * 60.0f),
                      y1 = 72 + (int32_t)(vz.sy[i] / (z1 > 0.02f ? z1 : 0.02f) * 40.0f);
        const uint16_t c = vpal((1.0f - z0) * 0.5f + vz.ph * 2.0f + (i & 7) * 0.02f);
        cv_line(x0, y0, x1, y1, z0 < 0.35f ? K_TEXT : c);
        if (z0 < 0.4f)
            cv_rect(x1, y1, 2, 2, K_TEXT);
        vz.sz[i] = z1;
        if (z1 < 0.03f || x1 < -4 || x1 > 244 || y1 < -4 || y1 > 148) {
            vz.sx[i] = vz_rf() * 2.0f - 1.0f;
            vz.sy[i] = vz_rf() * 2.0f - 1.0f;
            vz.sz[i] = 1.0f;
        }
    }
    if (vz.kick > 0.5f)
        cv_disc(120, 72, (int32_t)(vz.kick * 10.0f), vpal(vz.ph2));
}


/* THE DANCER (DISCO): the mascot at three pixels a pixel on springs. The slinky stretches as he rises, squashes as he lands and
 * wobbles a few times after; the moves change every two bars (and with the section: asleep in a breakdown, shaking in a build,
 * a big jump with confetti on the drop). Rows are grouped: the head (0..10) and three coils; each group has its own offset,
 * width and sway, so the body can stretch, twist and ripple. */
enum { DM_BOUNCE, DM_SWAY, DM_SPIN, DM_WAVE, DM_BANG, DM_STOMP, DM_N, DM_SHAKE = DM_N, DM_SLEEP, DM_JUMP };
static const char *const DM_NAME[DM_JUMP + 1] = {"JUMP", "SWAY", "TWIRL", "WAVE", "HEADBANG", "STOMP", "SHAKE", "SLEEPING", "BIG JUMP"};
static const uint8_t DM_GRP[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3};

static void vz_confetti(void)
{
    int i;
    for (i = 0; i < 24; i++) {
        vz.conf[i][0] = 84.0f + (vz_rf() - 0.5f) * 40.0f;
        vz.conf[i][1] = 70.0f;
        vz.conf[i][2] = (vz_rf() - 0.5f) * 170.0f;
        vz.conf[i][3] = -60.0f - vz_rf() * 130.0f;
        vz.conf[i][4] = vz_rf();
    }
}

static void vz_dancer(void)
{
    const int PX = 4;
    const int CXD = 84;                        /* his middle; the pattern is on the right */
    const float dt = vz.dt;
    const int playing = en_playing;
    int mv, g, r, c;
    float dx[4] = {0, 0, 0, 0}, sx[4] = {1, 1, 1, 1}, dyg[4], shear = 0.0f, sleepy = 0.0f, wide = 0.0f;
    const int kedge = vz.kick > 0.6f && vz.kprev <= 0.6f, sedge = vz.snare > 0.6f && vz.sprev <= 0.6f;
    const float t = vz.tt;
    vz.kprev = vz.kick, vz.sprev = vz.snare;
    /* which move */
    if (en_sec != vz.dsec) {
        if (en_sec == EN_SEC_GROOVE && vz.dsec != EN_SEC_GROOVE) {      /* the drop */
            vz.drop_bar = en_bars;
            vz_confetti();
        }
        vz.dsec = en_sec;
    }
    if (!playing)
        mv = DM_SWAY;
    else if (en_sec == EN_SEC_BREAK)
        mv = DM_SLEEP;
    else if (en_sec == EN_SEC_BUILD)
        mv = DM_SHAKE;
    else if (en_bars - vz.drop_bar < 32u)
        mv = DM_JUMP;
    else
    {   /* mostly jumping: the jump for 2 .. 8 bars (random), then one other move for 2 bars */
        const uint32_t unit = en_bars / 32u;
        if (unit != vz.dunit) {
            vz.dunit = unit;
            if (vz.dleft)
                vz.dleft--;
            if (!vz.dleft) {
                if (vz.dother) {
                    vz.dother = 0;
                    vz.dleft = (uint8_t)(1u + vz_rnd() % 4u);
                } else {
                    vz.dother = (uint8_t)(1u + vz_rnd() % (DM_N - 1));
                    vz.dleft = 1;
                }
            }
        }
        mv = vz.dother ? vz.dother : DM_BOUNCE;
    }
    vz.dmove = (uint8_t)mv;
    /* the hop: a launch on the kick (a big one for the drop), the head's spring left behind, a squash on landing */
    if (kedge && vz.lift <= 0.0f && (mv == DM_BOUNCE || mv == DM_JUMP || mv == DM_SWAY || mv == DM_SPIN || mv == DM_WAVE || mv == DM_BANG || mv == DM_STOMP)) {
        const float v0 = mv == DM_JUMP ? 150.0f : mv == DM_BOUNCE ? 100.0f : 70.0f;
        vz.lv = v0;
        vz.lift = 0.01f;
        vz.dhv -= v0 * 0.9f;                       /* anticipation: the head lags */
    }
    if (sedge && mv == DM_STOMP)
        vz.dhv -= 170.0f;                          /* a stomp: squashed flat, then up */
    {   /* physics, in four small steps */
        int s;
        for (s = 0; s < 4; s++) {
            const float h = dt * 0.25f;
            if (vz.lift > 0.0f || vz.lv > 0.0f) {
                vz.lv -= 560.0f * h;
                vz.lift += vz.lv * h;
                if (vz.lift <= 0.0f && vz.lv < 0.0f) {
                    vz.dhv += vz.lv * 2.3f;        /* landing: the body squashes (lv < 0) */
                    vz.lift = 0.0f;
                    vz.lv = 0.0f;
                }
            }
            {
                float tg = (vz.lift > 0.0f ? fm_fabsf(vz.lv) * 0.11f : 0.0f);   /* stretched by the speed */
                if (mv == DM_SLEEP)
                    tg = -5.0f + fm_sinf(t * 1.6f) * 1.5f;
                else if (mv == DM_SHAKE)
                    tg = 3.0f + fm_sinf(t * 22.0f) * 3.0f;
                vz.dhv += (-300.0f * (vz.dh - tg) - 10.0f * vz.dhv) * h;
                vz.dh += vz.dhv * h;
            }
        }
        vz.dh = fm_clampf(vz.dh, -16.0f, 26.0f);
    }
    /* each move's sway / twist / ripple per group (0 head, 1..3 the coils down to the floor) */
    for (g = 0; g < 4; g++) {
        const float w = g == 3 ? 0.0f : g == 2 ? 0.4f : g == 1 ? 0.7f : 1.0f;
        dyg[g] = vz.dh * 1.3f * (float)(3 - g) * (1.0f / 3.0f);
        switch (mv) {
        case DM_SWAY: dx[g] = fm_sinf(t * 2.2f - (float)g * 0.7f) * 20.0f * w; break;
        case DM_SPIN: sx[g] = fm_cosf(t * 3.4f + (float)g * 0.55f); break;
        case DM_WAVE:
            dx[g] = fm_sinf(t * 3.0f - (float)g * 1.1f) * 12.0f * (g == 3 ? 0.0f : 1.0f);
            dyg[g] += fm_sinf(t * 5.0f - (float)g * 1.2f) * 3.0f * (float)g;
            break;
        case DM_BANG: dx[g] = fm_sinf(t * 6.0f) * 3.0f * w; break;
        case DM_SHAKE: dx[g] = (vz_rf() - 0.5f) * 5.0f * w; break;
        case DM_SLEEP: dx[g] = fm_sinf(t * 1.0f) * 2.0f * w; break;
        default: break;
        }
    }
    if (mv == DM_BANG)
        shear = fm_sinf(t * 6.0f + 1.0f) * 2.1f * (0.5f + vz.kick);
    if (mv == DM_SLEEP)
        sleepy = 1.0f;
    if (mv == DM_SHAKE || mv == DM_JUMP)
        wide = 1.0f;
    if (vz.dh < -3.0f)                                  /* squashed: wider */
        for (g = 0; g < 4; g++)
            sx[g] *= 1.0f - vz.dh * 0.028f;
    {   /* the floor's shadow, smaller as he rises */
        const int sw = (int)(72.0f - vz.lift * 0.9f);
        cv_rect(CXD - sw / 2, 119, sw, 3, K_PANEL);
    }
    for (g = 3; g >= 0; g--)                           /* coils first, the head last (a squash overlaps) */
        for (r = 0; r < 20; r++) {
            if (DM_GRP[r] != g)
                continue;
            {
                const int32_t y = 118 - 20 * PX + r * PX - (int32_t)(vz.lift + dyg[g]);
                float rowdx = dx[g];
                int hide = sx[g] < 0.0f;
                if (g == 0)
                    rowdx += shear * (float)(r - 6);
                for (c = 0; c < 23; c++) {
                    char ch = MASCOT[r][c];
                    float xx;
                    int32_t w;
                    if (ch == '.')
                        continue;
                    if (ch == 'Y' && (sleepy > 0.5f || hide))
                        ch = hide ? 'R' : 'K';
                    if (ch == 'T' || (c >= 17 && r >= 9))        /* the tail wags with the beat */
                        rowdx += fm_sinf(t * 7.0f + (float)r * 0.5f) * 1.5f * (float)(r - 8) * 0.25f;
                    xx = (float)(c - 11) * (float)PX * sx[g];
                    w = (int32_t)(fm_fabsf(sx[g]) * (float)PX + 0.99f);
                    if (w < 1)
                        w = 1;
                    cv_rect(CXD + (int32_t)(xx + rowdx) - (sx[g] < 0.0f ? w : 0), y, w, PX, ch == 'K' || ch == 'T' ? RGB(27, 16, 34) :
                            ch == 'Y' && wide > 0.5f ? RGB(255, 255, 255) : mascot_color(ch));
                }
            }
        }
    if (mv == DM_SLEEP) {                               /* Z z z, drifting up */
        int k;
        for (k = 0; k < 3; k++) {
            const float u = (t * 0.35f + (float)k * 0.33f);
            const float f = u - fm_floorf(u);
            cv_text(CXD + 40 + k * 10 + (int32_t)(f * 8.0f), 70 - (int32_t)(f * 50.0f), &FONT_XS, k == 0 ? "Z" : "z", mix(K_BG, K_TEXT, (int)((1.0f - f) * 16.0f)));
        }
    }
    {   /* confetti, from the drop */
        int k;
        for (k = 0; k < 24; k++) {
            float *p = vz.conf[k];
            if (p[1] > 150.0f || p[0] < -4.0f || p[0] > 164.0f)
                continue;
            p[3] += 330.0f * dt;
            p[0] += p[2] * dt;
            p[1] += p[3] * dt;
            cv_rect((int32_t)p[0], (int32_t)p[1], 3, 3, vpal(p[4] + vz.ph * 3.0f));
        }
    }
    cv_text(172, 76, &FONT_XS, DM_NAME[mv], K_DIM);     /* the move, under the pattern */
}

static void vz_disco(void)
{
    int i, l, s;
    const int kk = (int)(vz.kick * 5.0f), step4 = en_playing ? (int)(en_drum_pos >> 1) : 0;
    const int32_t bx = 84, by = 13;
    {   /* the back wall, breathing with the kick; a disco floor below it, tiles chasing along with the pattern */
        int c, r;
        cv_rect(0, 0, 240, 120, mix(K_BG, vpal(vz.ph * 2.0f), 1 + kk / 2));
        for (r = 0; r < 2; r++)
            for (c = 0; c < 12; c++) {
                const int lit = ((c + r * 3 + step4) & 3) == 0 && en_playing;
                const uint16_t col = lit ? mix(K_PANEL, vpal(vz.ph * 2.0f + (float)c * 0.07f + (float)r * 0.12f), 9 + kk) :
                                           mix(K_BG, K_PANEL, ((c + r) & 1) ? 12 : 7);
                cv_rect(c * 20, 123 + r * 11, 19, 10, col);
            }
    }
    {   /* the beams from the ball, sweeping; their spots where they meet the floor */
        int k;
        for (k = 0; k < 6; k++) {
            const float a = -1.15f + (float)k * 0.46f + fm_sinf(vz.tt * 0.7f + (float)k * 1.3f) * 0.3f;
            const uint16_t col = mix(K_BG, vpal(vz.ph * 3.0f + (float)k * 0.17f), 5 + kk);
            const int32_t ex = bx + (int32_t)(fm_sinf(a) * 140.0f), ey = by + (int32_t)(fm_cosf(a) * 140.0f);
            cv_line(bx, by, ex, ey, col);
            cv_line(bx + 1, by, ex + 1, ey, col);
            cv_line(bx - 1, by, ex - 1, ey, col);
            if (a > -1.3f && a < 1.3f && fm_cosf(a) > 0.2f) {
                const int32_t sx = bx + (int32_t)(fm_sinf(a) / fm_cosf(a) * 108.0f);
                cv_disc(sx, 122, 4, mix(K_BG, vpal(vz.ph * 3.0f + (float)k * 0.17f), 8 + kk));
            }
        }
    }
    for (i = 0; i < 12; i++) {                     /* sparkles on the wall */
        const uint32_t h = hash(2166136261u, (uint32_t)i * 977u + (plat_ms() >> 8));
        cv_rect((int32_t)(h % 164u), (int32_t)(h >> 8) % 100, 2, 2, (h >> 20 & 1u) ? K_TEXT : vpal((float)i * 0.07f));
    }
    cv_line(bx, 0, bx, by - 8, K_LINE);            /* the ball: a rope, a sphere of mirrors turning */
    cv_disc(bx, by, 9, K_LINE);
    for (i = 0; i < 16; i++) {
        const float a = (float)i * (FM_TWO_PI / 16.0f) + vz.tt * 1.4f, r = (i & 1) ? 3.0f : 6.0f;
        const int on = (int)((hash(77u, (uint32_t)i + (uint32_t)(vz.tt * 3.0f)) >> 9) & 3u);
        cv_rect(bx + (int32_t)(fm_cosf(a) * r) - 1, by + (int32_t)(fm_sinf(a) * r * 0.8f) - 1, 3, 3, on == 0 ? K_TEXT : on == 1 ? vpal(vz.ph * 3.0f + (float)i * 0.06f) : K_DIM);
    }
    vz_dancer();
    {   /* the drum pattern, small, at the right (as on the home screen), the step playing lit */
        const int pos = en_drum_pos, pat = en_pat_now;
        char tt[8] = "PAT ";
        cv_round(168, 6, 70, 62, 3, K_PANEL);
        for (l = 0; l < EN_NLANE; l++) {
            const uint16_t bits = en_pats[pat][l];
            const int g = q16(en_drum_glow[l]);
            for (s = 0; s < 16; s++) {
                const int32_t x = 172 + s * 4, y = 10 + l * 9;
                const int on = (int)(bits >> s & 1u), cur = en_playing && s == pos;
                if (on)
                    cv_rect(x, y, 3, 6, en_pulse_rest ? K_LINE : cur ? K_TEXT : mix(K_LINE, LANE_COL(l), 8 + g / 2));
                else
                    cv_rect(x + 1, y + 2, 1, 2, cur ? K_DIM : K_LINE);
            }
        }
        itoa_u((uint32_t)pat + 1u, tt + 4);
        cv_text(172, 64 - 2, &FONT_XS, tt, K_DIM);
    }
}

static void vz_life_seed(void)
{
    int x, y;
    for (y = 0; y < VZ_CH; y++)
        for (x = 0; x < VZ_CW; x++)
            vz.life[vz.lcur][y][x] = (vz_rnd() & 7u) == 0u ? 1u : 0u;
}
static void vz_life_step(void)
{
    uint8_t (*a)[VZ_CW] = vz.life[vz.lcur];
    uint8_t (*b)[VZ_CW] = vz.life[vz.lcur ^ 1u];
    int x, y, pop = 0;
    for (y = 0; y < VZ_CH; y++)
        for (x = 0; x < VZ_CW; x++) {
            const int xm = (x + VZ_CW - 1) % VZ_CW, xp = (x + 1) % VZ_CW, ym = (y + VZ_CH - 1) % VZ_CH, yp = (y + 1) % VZ_CH;
            const int n = (a[ym][xm] > 0) + (a[ym][x] > 0) + (a[ym][xp] > 0) + (a[y][xm] > 0) + (a[y][xp] > 0) + (a[yp][xm] > 0) +
                          (a[yp][x] > 0) + (a[yp][xp] > 0);
            if (a[y][x])
                b[y][x] = (n == 2 || n == 3) ? (uint8_t)(a[y][x] < 250 ? a[y][x] + 1 : 250) : 0;
            else
                b[y][x] = n == 3 ? 1u : 0u;
            pop += b[y][x] > 0;
        }
    vz.lcur ^= 1u;
    if (pop < 14)                                   /* gone quiet: the drums start it again */
        vz_life_seed();
}
static void vz_life(void)
{
    int x, y, k;
    const uint32_t now = plat_ms();
    uint8_t (*a)[VZ_CW] = vz.life[vz.lcur];
    if (en_playing ? (vz.last_step != en_drum_pos) : (now - vz.life_ms > 220u)) {   /* a step for every sixteenth */
        vz.last_step = en_drum_pos;
        vz.life_ms = now;
        if (vz.kick > 0.6f) {                      /* a kick: a cluster in the middle, nudged about */
            const int cx = 14 + (int)(vz_rnd() % 12u), cy = 8 + (int)(vz_rnd() % 8u);
            for (k = 0; k < 7; k++)
                a[cy + (int)(vz_rnd() % 3u)][cx + (int)(vz_rnd() % 3u)] = 1u;
        }
        if (vz.snare > 0.6f)
            for (k = 0; k < 10; k++)
                a[vz_rnd() % VZ_CH][vz_rnd() % VZ_CW] = 1u;
        if (vz.hat > 0.6f)
            a[vz_rnd() % VZ_CH][vz_rnd() % VZ_CW] = 1u;
        vz_life_step();
        a = vz.life[vz.lcur];
    }
    for (y = 0; y < VZ_CH; y++)
        for (x = 0; x < VZ_CW; x++)
            if (a[y][x])
                cv_rect(x * 6, y * 6, 5, 5, vpal((float)a[y][x] * 0.025f + vz.ph + (float)x * 0.004f));
            else if (((x + y) & 1) == 0)
                cv_rect(x * 6 + 2, y * 6 + 2, 1, 1, K_LINE);
}

static void vz_orbit(void)
{
    const int N = 360;
    int i;
    for (i = 0; i < 3; i++) {                      /* the chord's notes (their pitch classes) as the curve's frequencies, eased to */
        const float tg = 1.0f + (float)(en_chord[i] % 12u) * (1.0f / 12.0f);
        vz.cf[i] += (tg - vz.cf[i]) * fm_minf(vz.dt * 2.0f, 1.0f);
    }
    {
        int32_t px = 0, py = 0;
        for (i = 0; i <= N; i++) {
            const float th = (float)i * (FM_TWO_PI * 3.0f / (float)N), amp = 1.0f + vz.kick * 0.25f;
            const int32_t x = 118 + (int32_t)(100.0f * fm_sinf(th * vz.cf[0] * 2.0f + vz.ph2 * 0.7f) * amp + 14.0f * fm_sinf(th * vz.cf[2] * 3.0f)),
                          y = 70 + (int32_t)(58.0f * fm_sinf(th * vz.cf[1] * 2.0f + vz.tt * 0.3f) + 12.0f * fm_cosf(th * vz.cf[2] * 3.0f));
            if (i)
                cv_line(px, py, x, y, vpal((float)i * (1.0f / (float)N) + vz.ph * 2.0f));
            px = x, py = y;
        }
    }
}
static void draw_viz(void)
{
    int mode = ui.viz % VZ_N;
    if (!vz.inited) {
        vz.inited = 1;
        vz.seed = 0x9E3779B9u ^ plat_ms();
        vz.last_ms = plat_ms();
        vz.cf[0] = 1.0f, vz.cf[1] = 1.3f, vz.cf[2] = 1.6f;
        vz.mode_seen = 255;
        vz.dleft = (uint8_t)(1u + vz_rnd() % 4u);
        vz.dunit = en_bars / 32u;
        {
            int q;
            for (q = 0; q < 24; q++)
                vz.conf[q][1] = 999.0f;
        }
        vz.sec_seen = en_sec;
    }
    vz_signals();
    if (mode == VZ_AUTO) {                         /* a new one on a section change and every 32 bars */
        if (en_sec != vz.sec_seen || en_bars - vz.auto_bar >= 512u) {
            vz.sec_seen = en_sec;
            vz.auto_bar = en_bars;
            vz.auto_cur = (uint8_t)((vz.auto_cur + 1u) % (VZ_N - 1));
        }
        mode = vz.auto_cur;
    }
    if (mode != vz.mode_seen) {                    /* a new one: its state fresh */
        vz.mode_seen = (uint8_t)mode;
        if (mode == VZ_STARS)
            vz_stars_init();
        if (mode == VZ_LIFE)
            vz_life_seed();
    }
    vz_lut();
    cv_begin(VZ_W, VZ_H, K_BG);
    switch (mode) {
    case VZ_PLASMA: vz_plasma(); break;
    case VZ_TUNNEL: vz_tunnel(); break;
    case VZ_BARS: vz_bars(); break;
    case VZ_STARS: vz_stars(); break;
    case VZ_DISCO: vz_disco(); break;
    case VZ_LIFE: vz_life(); break;
    default: vz_orbit(); break;
    }
    cv_blit(0, MAIN_Y);
}
