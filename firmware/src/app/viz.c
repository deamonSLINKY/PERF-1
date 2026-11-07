/* SPDX-License-Identifier: GPL-3.0-only */
/* PERF-1 visualizers: the main area as a light show that plays with the music (HOME double-tapped; HOME again leaves;
 * the SELECT dial chooses). Part of the unity build, included by ui.c just before draw_main. Fifteen frames a second, in
 * the theme's colours. The screensaver has the whole screen (240 high, drawn in two bands of 120: the state moves once a frame, the drawing is
 * done for each band); the page leaves 144 rows. Everything is drawn from what the engine already tells the screen (the drums' glows, the layers'
 * levels, the chord, the pattern, the section): no audio is analysed. The knobs stay the FX knobs, ALGORITHM the mood.
 *   PLASMA  a colour field that swirls faster with the energy and flashes on the kick
 *   KALEIDO the classic media-player pattern: curves from the spectrum, mirrored four ways, trailing and turning (it replaced the tunnel)
 *   BARS    an equaliser: lows from the kick and the drone, mids from the pad and the snare, highs from the hats and the acid
 *   FLUX    the classic media-player light show: the spectrum fed back on itself, a trippy bloom
 *   DISCO   the mascot dancing on a floor that lights with the drum pattern
 *   LIFE    a cellular automaton that the drums feed (the kick a cluster, the snare a spray, a hat a spark)
 *   ORBIT   a harmonograph drawn from the chord's notes: it morphs with every chord
 *   TRIP    a kaleidoscope you fall through: the kick throws you forward, the snare turns it, a build winds it tighter
 *   THEORY  the harmony: the circle of fifths, the chord's degree, name and notes, what comes next, the progression (not in AUTO)
 *   AUTO    one of them, a new one on every section change and every 32 bars */
#define VZ_W 240
#define VZ_H 144
#define VZ_CW 40                                /* the cell grids: 6 pixel cells */
#define VZ_CH 40                                /* (rows: 24 on the page, 40 on the whole screen) */
#define VZ_ROWS (vz.h / 6)

static struct {
    int32_t floor_y;                           /* where the dancer stands */
    int32_t h, y0;                             /* the picture's height (144 on the page, 240 whole screen) and, drawing the whole screen, the band's top row */
    uint8_t full, pass;                        /* the whole screen; which band (the state moves in the first) */
    float tt, ph, ph2, dt;                     /* time (tempo scaled), two phases, the frame's seconds */
    float kick, snare, hat, en;                /* the frame's signals */
    float bars[24], caps[24], spec[24];
    float cf[3];
    union {
        uint8_t life[2][VZ_CH][VZ_CW];
        uint8_t fb[2][60][60];                 /* FLUX's and KALEIDO's picture, two frames (60 x 36 cells on the page, 60 x 60 whole screen) */
    } buf;
    uint8_t fcur;
    uint8_t lcur, last_step, mode_seen, auto_cur, sec_seen, inited;
    uint32_t seed, last_ms, auto_bar, life_ms;
    uint16_t lut[64];
    float dh, dhv, lift, lv, kprev, sprev;     /* the dancer: the head's spring (px, + up) and its speed; the hop's height and speed */
    float conf[24][5];                         /* confetti: x, y, vx, vy, colour */
    uint8_t dsec, dmove, dleft, dother;        /* the section last seen; the move now; two-bar units left of it; 0 jumping, else the other move */
    uint32_t dunit;                            /* the two-bar unit last counted */
    uint32_t blink_until, blink_next;          /* the dancer's blink: when it ends; the next one */
    uint8_t dpos, dsince;                      /* the step last seen; steps since the last launch */
    uint32_t drop_bar;                         /* the sixteenth the drop came on */
} vz EN_POOL;

static int vz_vis(int32_t y, int32_t h) { return !vz.full || (y + h > vz.y0 && y < vz.y0 + 120); }   /* does a rectangle touch the band being drawn */

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
    const int rows = VZ_ROWS;
    int cx, cy;
    for (cy = 0; cy < rows; cy++) {
        if (!vz_vis(cy * 6, 6))
            continue;
        for (cx = 0; cx < VZ_CW; cx++) {
            const float x = (float)cx, y = (float)cy, dx = x - 20.0f, dy = (y - (float)rows * 0.5f) * 1.6f;
            float v = fm_sinf(x * 0.31f + t * 1.1f) + fm_sinf(y * 0.43f - t * 0.9f) + fm_sinf((x + y) * 0.21f + t * 0.7f) +
                      fm_sinf(fm_sqrtf(dx * dx + dy * dy) * 0.45f - t * 1.6f);
            cv_rect(cx * 6, cy * 6, 6, 6, vpal(v * 0.125f + 0.5f + vz.ph + vz.kick * 0.18f));
        }
    }
}

/* TRIP (0.16.10): a kaleidoscope you fall through. Each 4-pixel cell is turned into a tunnel's coordinates (depth from its distance to the
 * middle, angle folded into mirrored slices: six, or eight when the chord has four notes), and two interfering waves over them are coloured
 * from the theme. The kick throws you forward and the picture breathes in; the snare turns the twist the other way; a build winds it tighter
 * bar by bar; the hats strike sparks; a breakdown drifts slowly. */
static float vz_atan2(float y, float x)          /* atan2 to about 0.005 rad: enough for a picture */
{
    const float ax = x < 0.0f ? -x : x, ay = y < 0.0f ? -y : y, mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
    const float t = mx > 1e-6f ? mn / mx : 0.0f, s = t * t;
    float a = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * t + t;
    if (ay > ax)
        a = 1.5707963f - a;
    if (x < 0.0f)
        a = 3.14159265f - a;
    return y < 0.0f ? -a : a;
}
static float trip_z, trip_tw, trip_dir = 1.0f;   /* how far fallen; the twist; its way round */
static uint8_t trip_sn;
static void vz_trip(void)
{
    const int rows = (int)vz.h / 4, br = en_sec == EN_SEC_BREAK, bu = en_sec == EN_SEC_BUILD;
    const float k = en_chord_n >= 4 ? 8.0f : 6.0f, t = vz.tt, breathe = 1.0f - 0.22f * vz.kick;
    int cx, cy;
    if (!vz.pass) {
        const int sn = vz.snare > 0.6f;
        if (sn && !trip_sn)
            trip_dir = -trip_dir;
        trip_sn = (uint8_t)sn;
        trip_z += vz.dt * ((br ? 0.25f : 0.9f) + 3.5f * vz.kick) * (0.6f + (float)proj.par[P_TEMPO] * 0.004f);
        trip_tw += vz.dt * trip_dir * (0.3f + (bu ? 1.6f / (1.0f + (float)en_sec_bars) : 0.0f) + 0.5f * vz.en);
        if (trip_z > 1000.0f)
            trip_z -= 1000.0f;
        if (trip_tw > 1000.0f || trip_tw < -1000.0f)
            trip_tw = 0.0f;
    }
    for (cy = 0; cy < rows; cy++) {
        if (!vz_vis(cy * 4, 4))
            continue;
        for (cx = 0; cx < 60; cx++) {
            const float dx = (float)cx - 29.5f, dy = ((float)cy - (float)rows * 0.5f + 0.5f), r = fm_sqrtf(dx * dx + dy * dy) * breathe + 0.6f;
            const float depth = 18.0f / r + trip_z, a = vz_atan2(dy, dx) * (1.0f / 6.2831853f) + trip_tw * 0.08f + r * 0.012f * trip_dir;
            float f = a * k, v, w;
            f -= fm_floorf(f);
            f = f < 0.5f ? f : 1.0f - f;                /* the mirror: each slice the last one turned over */
            v = fm_sinf(depth * 2.1f + fm_sinf(f * 9.0f + t * 0.7f) * 1.6f) + fm_sinf(f * 14.0f - depth * 0.9f + t * 0.5f);
            w = v * 0.17f + depth * 0.045f + vz.ph + vz.kick * 0.12f;
            if (r < 3.0f + 4.0f * vz.kick)               /* the light at the end */
                cv_rect(cx * 4, cy * 4, 4, 4, mix(vpal(w), K_TEXT, (int)(16.0f - r * 3.0f) < 0 ? 0 : (int)(16.0f - r * 3.0f) > 16 ? 16 : (int)(16.0f - r * 3.0f)));
            else if (vz.hat > 0.5f && (vz_rnd() & 255u) < (uint32_t)(vz.hat * 10.0f))
                cv_rect(cx * 4, cy * 4, 4, 4, K_TEXT);  /* sparks on the hats */
            else
                cv_rect(cx * 4, cy * 4, 4, 4, mix(K_BG, vpal(w), 6 + (int)(10.0f * fm_clampf(1.4f - 9.0f / (r + 6.0f), 0.0f, 1.0f))));
        }
    }
}

static void vz_bars(void)                       /* the real thing: the master's spectrum (en_spectrum), 24 bands */
{
    const int rows = (int)(vz.h - 12) / 10;
    const int32_t base = vz.h - 12;
    int i, j;
    if (!vz.pass) {
        en_spectrum(vz.spec, 24);
        for (i = 0; i < 24; i++) {
            const float tg = vz.spec[i];
            vz.bars[i] = tg > vz.bars[i] ? tg : fm_maxf(vz.bars[i] - vz.dt * 1.8f, 0.0f);
            vz.caps[i] = vz.bars[i] > vz.caps[i] ? vz.bars[i] : fm_maxf(vz.caps[i] - vz.dt * 0.5f, 0.0f);
        }
    }
    for (i = 0; i < 24; i++) {
        const float u = (float)i * (1.0f / 23.0f);
        for (j = 0; j < (int)(vz.bars[i] * (float)rows + 0.5f); j++)
            if (vz_vis(base - j * 10, 8))
                cv_rect(i * 10 + 1, base - j * 10, 8, 8, vpal((float)j * 0.035f * 14.0f / (float)rows + u * 0.3f + vz.ph));
        if (vz_vis(base - (int32_t)(vz.caps[i] * (float)rows + 0.5f) * 10, 3))
            cv_rect(i * 10 + 1, base - (int32_t)(vz.caps[i] * (float)rows + 0.5f) * 10, 8, 3, K_TEXT);
    }
}

/* FLUX: the classic media-player light show. A low-resolution picture (60 x 36 cells) is fed back on itself every frame, zoomed out
 * and turned a little, and fading; the spectrum is drawn into it as a ring around the middle and a ribbon across it, and the kick adds a
 * ripple. Colour comes from how bright a cell still is and where it is, so the trails bloom outwards in the theme's colours. */
#define FL_W 60
#define FL_H (vz.h / 4)                        /* 36 on the page, 60 whole screen */
static void fl_plot(uint8_t (*b)[FL_W], int x, int y, int v)
{
    if ((unsigned)x < FL_W && (unsigned)y < (unsigned)FL_H && b[y][x] < v)
        b[y][x] = (uint8_t)v;
}
static void fl_line(uint8_t (*b)[FL_W], int x0, int y0, int x1, int y1, int v)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, guard = 200;
    while (guard--) {
        const int e2 = 2 * err;
        fl_plot(b, x0, y0, v);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
static void vz_flux_init(void)
{
    memset(vz.buf.fb, 0, sizeof vz.buf.fb);
    vz.fcur = 0;
}
static void vz_flux(void)
{
    uint8_t (*src)[FL_W] = vz.buf.fb[vz.fcur], (*dst)[FL_W] = vz.buf.fb[vz.fcur ^ 1u];
    const float mx = (float)FL_W * 0.5f, my = (float)FL_H * 0.5f;
    int x, y, k;
    if (!vz.pass) {
        const float ang = 0.05f + 0.035f * fm_sinf(vz.tt * 0.4f) + vz.kick * 0.05f, ca = fm_cosf(ang), sa = fm_sinf(ang),
                    zoom = 0.955f - 0.025f * vz.kick;
        for (y = 0; y < FL_H; y++)
            for (x = 0; x < FL_W; x++) {
                const float dx = (float)x + 0.5f - mx, dy = (float)y + 0.5f - my;
                const float fx = mx - 0.5f + (dx * ca - dy * sa) * zoom, fy = my - 0.5f + (dx * sa + dy * ca) * zoom;   /* where it was a frame ago (cell centres) */
                const int ix = (int)fm_floorf(fx), iy = (int)fm_floorf(fy), tx = (int)((fx - (float)ix) * 16.0f), ty = (int)((fy - (float)iy) * 16.0f);
                int v = 0;
                if (ix >= 0 && iy >= 0 && ix < FL_W - 1 && iy < FL_H - 1) {      /* smooth: the four cells round it */
                    const int a = src[iy][ix] * (16 - tx) + src[iy][ix + 1] * tx, b = src[iy + 1][ix] * (16 - tx) + src[iy + 1][ix + 1] * tx;
                    v = (a * (16 - ty) + b * ty) >> 8;
                }
                dst[y][x] = (uint8_t)((v * 237) >> 8);
            }
        vz.fcur ^= 1u;
        en_spectrum(vz.spec, 24);
        {   /* the ring: the spectrum round the middle, turning */
            int px = 0, py = 0;
            const float rs = (float)FL_H * (1.0f / 36.0f);
            for (k = 0; k <= 24; k++) {
                const float s = vz.spec[k % 24], a = (float)k * (FM_TWO_PI / 24.0f) + vz.tt * 0.35f, r = (4.0f + s * 13.0f + (en_playing ? 0.0f : 1.0f + fm_sinf(vz.tt) * 0.8f)) * rs;
                const int cx = (int)(mx + fm_cosf(a) * r), cy = (int)(my + fm_sinf(a) * r * 0.95f);
                if (k)
                    fl_line(dst, px, py, cx, cy, 255);
                px = cx, py = cy;
            }
        }
        {   /* the ribbon: a wave across, as tall as the spectrum under it */
            int px = 0, py = (int)my;
            const float rs = (float)FL_H * (1.0f / 36.0f);
            for (x = 0; x < FL_W; x += 2) {
                const float u = (float)x * (24.0f / (float)FL_W), s = vz.spec[(int)u > 23 ? 23 : (int)u];
                const int yy = (int)(my + fm_sinf((float)x * 0.28f + vz.tt * 2.2f) * (1.5f + s * 12.0f) * rs);
                if (x)
                    fl_line(dst, px, py, x, yy, 190);
                px = x, py = yy;
            }
        }
        if (vz.kick > 0.6f)                              /* a kick: a ripple */
            for (k = 0; k < 24; k++)
                fl_plot(dst, (int)(mx + fm_cosf((float)k * (FM_TWO_PI / 24.0f)) * 6.0f), (int)(my + fm_sinf((float)k * (FM_TWO_PI / 24.0f)) * 6.0f), 255);
    }
    dst = vz.buf.fb[vz.fcur];                            /* (the one just made) */
    for (y = 0; y < FL_H; y++) {
        if (!vz_vis(y * 4, 4))
            continue;
        for (x = 0; x < FL_W; x++) {
            const int v = dst[y][x];
            if (v > 10) {
                const float dx = (float)x - mx, dy = (float)y - my;
                cv_rect(x * 4, y * 4, 4, 4, mix(K_BG, vpal(vz.ph * 2.0f + (dx * dx + dy * dy) * 0.0009f + (float)v * 0.0018f), (v + 8) >> 4));
            }
        }
    }
}

/* KALEIDO: the other classic: curves swept out by the spectrum (a rose whose petals are the bands, and a ring of the highs), drawn into the
 * same feedback picture four times, mirrored left-right and top-bottom, while the picture slowly turns and spreads and fades: ribbons that
 * unfold from the middle. The kick throws a ring. */
static void vz_kaleido(void)
{
    uint8_t (*src)[FL_W] = vz.buf.fb[vz.fcur], (*dst)[FL_W] = vz.buf.fb[vz.fcur ^ 1u];
    const float mx = (float)FL_W * 0.5f, my = (float)FL_H * 0.5f;
    int x, y, k;
    if (!vz.pass) {
        const float ang = 0.035f + 0.025f * fm_sinf(vz.tt * 0.23f), ca = fm_cosf(ang), sa = fm_sinf(ang), zoom = 0.965f - 0.02f * vz.kick;
        const float rs = (float)FL_H * (1.0f / 36.0f);
        for (y = 0; y < FL_H; y++)
            for (x = 0; x < FL_W; x++) {
                const float dx = (float)x + 0.5f - mx, dy = (float)y + 0.5f - my;
                const float fx = mx - 0.5f + (dx * ca - dy * sa) * zoom, fy = my - 0.5f + (dx * sa + dy * ca) * zoom;
                const int ix = (int)fm_floorf(fx), iy = (int)fm_floorf(fy), tx = (int)((fx - (float)ix) * 16.0f), ty = (int)((fy - (float)iy) * 16.0f);
                int v = 0;
                if (ix >= 0 && iy >= 0 && ix < FL_W - 1 && iy < FL_H - 1) {
                    const int a = src[iy][ix] * (16 - tx) + src[iy][ix + 1] * tx, b = src[iy + 1][ix] * (16 - tx) + src[iy + 1][ix + 1] * tx;
                    v = (a * (16 - ty) + b * ty) >> 8;
                }
                dst[y][x] = (uint8_t)((v * 218) >> 8);
            }
        vz.fcur ^= 1u;
        en_spectrum(vz.spec, 24);
        {   /* the rose, mirrored four ways */
            const int N = 72;
            int px = 0, py = 0;
            for (k = 0; k <= N; k++) {
                const float u = (float)k * (1.0f / (float)N), s = vz.spec[(int)(u * 23.99f)], th = u * (FM_TWO_PI * 2.0f) + vz.tt * 0.4f;
                const float r = (6.0f + (6.0f + 16.0f * s) * (0.4f + 0.6f * fm_sinf(th * 2.5f + vz.tt * 0.9f))) * rs,
                            qx = fm_cosf(th) * r, qy = fm_sinf(th) * r * 0.9f;
                const int ix = (int)(mx + qx), iy = (int)(my + qy);
                if (k) {
                    fl_line(dst, px, py, ix + 1, iy, 170);
                    fl_line(dst, px, py, ix, iy, 255);
                    fl_line(dst, FL_W - 1 - px, py, FL_W - 1 - ix, iy, 255);
                    fl_line(dst, px, FL_H - 1 - py, ix, FL_H - 1 - iy, 255);
                    fl_line(dst, FL_W - 1 - px, FL_H - 1 - py, FL_W - 1 - ix, FL_H - 1 - iy, 255);
                }
                px = ix, py = iy;
            }
        }
        {   /* the highs: a small ring that breathes with the hats */
            int px = 0, py = 0;
            const float rr = (5.0f + 9.0f * vz.hat + 4.0f * vz.spec[18]) * (float)FL_H * (1.0f / 36.0f);
            for (k = 0; k <= 12; k++) {
                const float a = (float)k * (FM_TWO_PI / 12.0f) - vz.tt * 0.8f;
                const int ix = (int)(mx + fm_cosf(a) * rr), iy = (int)(my + fm_sinf(a) * rr);
                if (k)
                    fl_line(dst, px, py, ix, iy, 210);
                px = ix, py = iy;
            }
        }
        if (vz.kick > 0.6f)                              /* a kick: a ring */
            for (k = 0; k < 32; k++)
                fl_plot(dst, (int)(mx + fm_cosf((float)k * (FM_TWO_PI / 32.0f)) * 9.0f * (float)FL_H * (1.0f / 36.0f)),
                        (int)(my + fm_sinf((float)k * (FM_TWO_PI / 32.0f)) * 9.0f * (float)FL_H * (1.0f / 36.0f)), 255);
    }
    dst = vz.buf.fb[vz.fcur];
    for (y = 0; y < FL_H; y++) {
        if (!vz_vis(y * 4, 4))
            continue;
        for (x = 0; x < FL_W; x++) {
            const int v = dst[y][x];
            if (v > 10) {
                const float dx = (float)x - mx, dy = (float)y - my;
                cv_rect(x * 4, y * 4, 4, 4, mix(K_BG, vpal(vz.ph * 2.5f + fm_sqrtf(dx * dx + dy * dy) * 0.07f + (float)v * 0.0016f), (v + 8) >> 4));
            }
        }
    }
}

/* THE DANCER (DISCO): the mascot at three pixels a pixel on springs. The slinky stretches as he rises, squashes as he lands and
 * wobbles a few times after; the moves change every two bars (and with the section: asleep in a breakdown, shaking in a build,
 * a big jump with confetti on the drop). Rows are grouped: the head (0..10) and three coils; each group has its own offset,
 * width and sway, so the body can stretch, twist and ripple. */
enum { DM_BOUNCE, DM_SWAY, DM_SPIN, DM_WAVE, DM_BANG, DM_STOMP, DM_N, DM_SHAKE = DM_N, DM_SLEEP, DM_JUMP };
static const char *const DM_NAME[DM_JUMP + 1] = {"JUMP", "SWAY", "TWIRL", "WAVE", "HEADBANG", "STOMP", "SHAKE", "SLEEPING", "BIG JUMP"};
static const uint8_t DM_GRP[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4};

static void vz_confetti(void)
{
    const float k = vz.full ? 1.75f : 1.0f, cx = vz.full ? 104.0f : 84.0f;
    int i;
    for (i = 0; i < 24; i++) {
        vz.conf[i][0] = cx + (vz_rf() - 0.5f) * 40.0f * k;
        vz.conf[i][1] = (float)vz.h - 74.0f * k;
        vz.conf[i][2] = (vz_rf() - 0.5f) * 170.0f * k;
        vz.conf[i][3] = (-60.0f - vz_rf() * 130.0f) * k;
        vz.conf[i][4] = vz_rf();
    }
}

static void vz_dancer(void)
{
    const int PX = vz.full ? 7 : 4;              /* a pixel of him: 4 on the page, 7 on the whole screen */
    const float ks = (float)PX * 0.25f;          /* the scale of everything he does, in pixels */
    const int CXD = vz.full ? 104 : 84;        /* his middle; the pattern is on the right */
    const int32_t vfloor = vz.floor_y;         /* where he stands (disco's floor) */
    const float dt = vz.dt;
    const int playing = en_playing;
    int g, r, c;
    static float dx[5], sx[5], dyg[5], shear, sleepy, wide;   /* (kept for the second band of the whole screen) */
    static int mouth, blink, mv;
    int kedge = vz.kick > 0.6f && vz.kprev <= 0.6f;
    const int sedge = vz.snare > 0.6f && vz.sprev <= 0.6f;
    const float t = vz.tt;
    if (!vz.pass) {
    for (g = 0; g < 5; g++)
        dx[g] = 0.0f, sx[g] = 1.0f;
    shear = sleepy = wide = 0.0f;
    mouth = 0;
    vz.kprev = vz.kick, vz.sprev = vz.snare;
    if (playing && en_drum_pos != vz.dpos) {           /* a jump on every kick of the pattern (the glow can be missed between frames), and at least one a beat */
        vz.dpos = en_drum_pos;
        vz.dsince++;
        if ((en_pats[en_pat_now][0] >> en_drum_pos & 1u) || vz.dsince >= 4u)
            kedge = 1;
    }
    if (kedge)
        vz.dsince = 0;
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
    if (kedge && (vz.lift <= 0.0f || (vz.lv < 0.0f && vz.lift < 6.0f)) && (mv == DM_BOUNCE || mv == DM_JUMP || mv == DM_SWAY || mv == DM_SPIN || mv == DM_WAVE || mv == DM_BANG || mv == DM_STOMP)) {
        const float v0 = mv == DM_JUMP ? 105.0f : mv == DM_BOUNCE ? 100.0f : 70.0f;
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
    for (g = 0; g < 5; g++) {
        const float w = g == 4 ? 0.0f : g == 3 ? 0.3f : g == 2 ? 0.5f : g == 1 ? 0.75f : 1.0f;
        dyg[g] = vz.dh * 1.3f * (float)(4 - g) * 0.25f;
        switch (mv) {
        case DM_SWAY: dx[g] = fm_sinf(t * 2.2f - (float)g * 0.7f) * 20.0f * w; break;
        case DM_SPIN: sx[g] = fm_cosf(t * 3.4f + (float)g * 0.55f); break;
        case DM_WAVE:
            dx[g] = fm_sinf(t * 3.0f - (float)g * 1.1f) * 12.0f * (g == 4 ? 0.0f : 1.0f);
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
    {   /* the face: blinks now and then; the mouth opens in a jump, on a snare, in a headbang, and sleeping he snores */
        const uint32_t now = plat_ms();
        if (now >= vz.blink_next) {
            vz.blink_until = now + 150u;
            vz.blink_next = now + 1800u + vz_rnd() % 3400u;
        }
        blink = now < vz.blink_until;
        if (mv == DM_SLEEP)
            mouth = fm_sinf(t * 1.4f) > 0.7f ? 1 : 0;
        else if (vz.lift > 12.0f)
            mouth = 2;
        else if (vz.lift > 3.0f || vz.snare > 0.4f || vz.dh < -5.0f || (mv == DM_BANG && fm_sinf(t * 12.0f) > 0.2f))
            mouth = 1;
    }
    if (vz.dh < -3.0f)                                  /* squashed: wider */
        for (g = 0; g < 5; g++)
            sx[g] *= 1.0f - vz.dh * 0.028f;
    }
    {   /* the floor's shadow, smaller as he rises */
        const int sw = (int)((72.0f - vz.lift * 0.9f) * ks);
        cv_rect(CXD - sw / 2, vfloor + 1, sw, 3, K_PANEL);
    }
    for (g = 4; g >= 0; g--)                           /* coils first, the head last (a squash overlaps) */
        for (r = 0; r < 23; r++) {
            if (DM_GRP[r] != g)
                continue;
            {
                const int32_t y = vfloor - 23 * PX + r * PX - (int32_t)((vz.lift + dyg[g]) * ks);
                float rowdx = dx[g] * ks;
                int hide = sx[g] < 0.0f;
                if (!vz_vis(y, PX))
                    continue;
                if (g == 0)
                    rowdx += shear * (float)(r - 6) * ks;
                for (c = 0; c < 23; c++) {
                    char ch = MASCOT[r][c];
                    float xx;
                    int32_t w;
                    if (ch == '.')
                        continue;
                    if (ch == 'Y' && (sleepy > 0.5f || hide || blink))
                        ch = hide ? 'R' : 'K';
                    if (mouth && !hide && r == 8 && c >= 7 && c <= 10)      /* the mouth open: a dark hole between the fangs */
                        ch = 'K';
                    if (mouth == 2 && !hide && r == 9 && (c == 8 || c == 9))   /* wide: a tongue */
                        ch = 'L';
                    if (ch == 'T' || (c >= 17 && r >= 9))        /* the tail wags with the beat */
                        rowdx += fm_sinf(t * 7.0f + (float)r * 0.5f) * 1.5f * (float)(r - 8) * 0.25f * ks;
                    xx = (float)(c - 11) * (float)PX * sx[g];
                    w = (int32_t)(fm_fabsf(sx[g]) * (float)PX + 0.99f);
                    if (w < 1)
                        w = 1;
                    {   /* a coil parts along the spring's winding (down across its width), not straight across; the Synth look's imp is lit, not dark */
                        int32_t yc = y;
                        uint16_t col;
                        if (g > 0 && c < 16) {
                            const float fr = fm_clampf((float)(c - 2) * (1.0f / 13.0f), 0.0f, 1.0f);
                            const int gn = g < 4 ? g + 1 : 4;
                            yc = vfloor - 23 * PX + r * PX - (int32_t)((vz.lift + dyg[g] + (dyg[gn] - dyg[g]) * fr) * ks);
                        }
                        col = ch == 'K' || ch == 'T' ? (LOOK_SYNTH ? mascot_color('K') : RGB(27, 16, 34)) :
                              ch == 'Y' && wide > 0.5f ? RGB(255, 255, 255) :
                              LOOK_SYNTH && ch == 'R' ? RGB(150, 40, 120) : LOOK_SYNTH && ch == 'r' ? RGB(104, 24, 88) : mascot_color(ch);
                        cv_rect(CXD + (int32_t)(xx + rowdx) - (sx[g] < 0.0f ? w : 0), yc, w, PX, col);
                    }
                }
            }
        }
    if (mv == DM_SLEEP) {                               /* Z z z, drifting up */
        int k;
        for (k = 0; k < 3; k++) {
            const float u = (t * 0.35f + (float)k * 0.33f);
            const float f = u - fm_floorf(u);
            cv_text(CXD + (int32_t)((40.0f + (float)(k * 10) + f * 8.0f) * ks), vfloor - (int32_t)((50.0f + f * 50.0f) * ks), &FONT_XS, k == 0 ? "Z" : "z", mix(K_BG, K_TEXT, (int)((1.0f - f) * 16.0f)));
        }
    }
    {   /* confetti, from the drop */
        int k;
        for (k = 0; k < 24; k++) {
            float *p = vz.conf[k];
            if (p[1] > (float)vz.h + 6.0f || p[0] < -4.0f || p[0] > 164.0f)
                continue;
            if (!vz.pass) {
                p[3] += 330.0f * ks * dt;
                p[0] += p[2] * dt;
                p[1] += p[3] * dt;
            }
            cv_rect((int32_t)p[0], (int32_t)p[1], 3, 3, vpal(p[4] + vz.ph * 3.0f));
        }
    }
    cv_text(172, 76, &FONT_XS, DM_NAME[mv], K_DIM);     /* the move, under the pattern */
}

static void vz_disco(void)
{
    int i, l, s;
    const int kk = (int)(vz.kick * 5.0f), step4 = en_playing ? (int)(en_drum_pos >> 1) : 0;
    const int frows = vz.full ? 4 : 2;                         /* the floor's rows of tiles */
    const int32_t flo = vz.h - frows * 11 - 1, bx = vz.full ? 104 : 84, by = vz.full ? 22 : 13;
    const float hs = (float)vz.h * (1.0f / 144.0f);
    vz.floor_y = flo - 1;
    {   /* the back wall, breathing with the kick; a disco floor below it, tiles chasing along with the pattern */
        int c, r;
        cv_rect(0, 0, 240, flo - 1, mix(K_BG, vpal(vz.ph * 2.0f), 1 + kk / 2));
        for (r = 0; r < frows; r++)
            for (c = 0; c < 12; c++) {
                const int lit = ((c + r * 3 + step4) & 3) == 0 && en_playing;
                const uint16_t col = lit ? mix(K_PANEL, vpal(vz.ph * 2.0f + (float)c * 0.07f + (float)r * 0.12f), 9 + kk) :
                                           mix(K_BG, K_PANEL, ((c + r) & 1) ? 12 : 7);
                cv_rect(c * 20, flo + 2 + r * 11, 19, 10, col);
            }
    }
    {   /* the beams from the ball, sweeping; their spots where they meet the floor */
        int k;
        for (k = 0; k < 6; k++) {
            const float a = -1.15f + (float)k * 0.46f + fm_sinf(vz.tt * 0.7f + (float)k * 1.3f) * 0.3f;
            const uint16_t col = mix(K_BG, vpal(vz.ph * 3.0f + (float)k * 0.17f), 5 + kk);
            const int32_t ex = bx + (int32_t)(fm_sinf(a) * 140.0f * hs), ey = by + (int32_t)(fm_cosf(a) * 140.0f * hs);
            cv_line(bx, by, ex, ey, col);
            cv_line(bx + 1, by, ex + 1, ey, col);
            cv_line(bx - 1, by, ex - 1, ey, col);
            if (a > -1.3f && a < 1.3f && fm_cosf(a) > 0.2f) {
                const int32_t sx = bx + (int32_t)(fm_sinf(a) / fm_cosf(a) * (float)(flo + 1 - by));
                cv_disc(sx, flo + 1, 4, mix(K_BG, vpal(vz.ph * 3.0f + (float)k * 0.17f), 8 + kk));
            }
        }
    }
    for (i = 0; i < 12; i++) {                     /* sparkles on the wall */
        const uint32_t h = hash(2166136261u, (uint32_t)i * 977u + (plat_ms() >> 8));
        cv_rect((int32_t)(h % 164u), (int32_t)((h >> 8) % (uint32_t)(flo - 20)), 2, 2, (h >> 20 & 1u) ? K_TEXT : vpal((float)i * 0.07f));
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
            vz.buf.life[vz.lcur][y][x] = y < VZ_ROWS && (vz_rnd() & 7u) == 0u ? 1u : 0u;
}
static void vz_life_step(void)
{
    uint8_t (*a)[VZ_CW] = vz.buf.life[vz.lcur];
    uint8_t (*b)[VZ_CW] = vz.buf.life[vz.lcur ^ 1u];
    int x, y, pop = 0;
    const int rows = VZ_ROWS;
    for (y = 0; y < rows; y++)
        for (x = 0; x < VZ_CW; x++) {
            const int xm = (x + VZ_CW - 1) % VZ_CW, xp = (x + 1) % VZ_CW, ym = (y + rows - 1) % rows, yp = (y + 1) % rows;
            const int n = (a[ym][xm] > 0) + (a[ym][x] > 0) + (a[ym][xp] > 0) + (a[y][xm] > 0) + (a[y][xp] > 0) + (a[yp][xm] > 0) +
                          (a[yp][x] > 0) + (a[yp][xp] > 0);
            if (a[y][x])
                b[y][x] = (n == 2 || n == 3) ? (uint8_t)(a[y][x] < 250 ? a[y][x] + 1 : 250) : 0;
            else
                b[y][x] = n == 3 ? 1u : 0u;
            pop += b[y][x] > 0;
        }
    vz.lcur ^= 1u;
    if (pop < 14 + (VZ_ROWS - 24) / 2)              /* gone quiet: the drums start it again */
        vz_life_seed();
}
static void vz_life(void)
{
    int x, y, k;
    const uint32_t now = plat_ms();
    const int rows = VZ_ROWS;
    uint8_t (*a)[VZ_CW] = vz.buf.life[vz.lcur];
    if (!vz.pass && (en_playing ? (vz.last_step != en_drum_pos) : (now - vz.life_ms > 220u))) {   /* a step for every sixteenth */
        vz.last_step = en_drum_pos;
        vz.life_ms = now;
        if (vz.kick > 0.6f) {                      /* a kick: a cluster in the middle, nudged about */
            const int cx = 14 + (int)(vz_rnd() % 12u), cy = rows / 2 - 4 + (int)(vz_rnd() % 8u);
            for (k = 0; k < 7; k++)
                a[cy + (int)(vz_rnd() % 3u)][cx + (int)(vz_rnd() % 3u)] = 1u;
        }
        if (vz.snare > 0.6f)
            for (k = 0; k < 10; k++)
                a[vz_rnd() % (uint32_t)rows][vz_rnd() % VZ_CW] = 1u;
        if (vz.hat > 0.6f)
            a[vz_rnd() % (uint32_t)rows][vz_rnd() % VZ_CW] = 1u;
        vz_life_step();
        a = vz.buf.life[vz.lcur];
    }
    for (y = 0; y < rows; y++)
        for (x = 0; x < VZ_CW; x++)
            if (!vz_vis(y * 6, 6))
                continue;
            else if (a[y][x])
                cv_rect(x * 6, y * 6, 5, 5, vpal((float)a[y][x] * 0.025f + vz.ph + (float)x * 0.004f));
            else if (((x + y) & 1) == 0)
                cv_rect(x * 6 + 2, y * 6 + 2, 1, 1, K_LINE);
}

static void vz_orbit(void)
{
    const int N = 360;
    const float sy = (float)vz.h * (1.0f / 144.0f);
    int i;
    for (i = 0; i < 3 && !vz.pass; i++) {         /* the chord's notes (their pitch classes) as the curve's frequencies, eased to */
        const float tg = 1.0f + (float)(en_chord[i] % 12u) * (1.0f / 12.0f);
        vz.cf[i] += (tg - vz.cf[i]) * fm_minf(vz.dt * 2.0f, 1.0f);
    }
    {
        int32_t px = 0, py = 0;
        for (i = 0; i <= N; i++) {
            const float th = (float)i * (FM_TWO_PI * 3.0f / (float)N), amp = 1.0f + vz.kick * 0.25f;
            const int32_t x = 118 + (int32_t)(100.0f * fm_sinf(th * vz.cf[0] * 2.0f + vz.ph2 * 0.7f) * amp + 14.0f * fm_sinf(th * vz.cf[2] * 3.0f)),
                          y = vz.h / 2 - 2 + (int32_t)(sy * (58.0f * fm_sinf(th * vz.cf[1] * 2.0f + vz.tt * 0.3f) + 12.0f * fm_cosf(th * vz.cf[2] * 3.0f)));
            if (i)
                cv_line(px, py, x, y, vpal((float)i * (1.0f / (float)N) + vz.ph * 2.0f));
            px = x, py = y;
        }
    }
}
/* THEORY (0.16.10, redrawn 0.16.19 for issue #2: a player learning to read music, after the THEORY mockup): the harmony as it plays.
 * Left, the circle of fifths as it is printed: C at the top, clockwise in fifths. Inner ring: every key's signature (0, 1# .. 6#,
 * 5b .. 1b), the key's own lit, so "E = 4 sharps" reads straight off it. The scale lies on the circle as one arc (seven neighbours
 * for a mode), its Roman numerals on it: the chord playing filled, the next one ringed, an arrow from one to the other while the
 * Progression page's loop plays. Outer ring: the notes, each named as the key spells it (G# in E major, Cb in Eb minor; theory.h),
 * the chord's tones marked, a borrowed one (minor's raised seventh on V) in red. Right: the chord's numeral, name, function and
 * notes (spelled), what comes next and how the root moves (a fifth down, a step up ..), the eight slots, the key and its signature.
 * The engine's names elsewhere stay as they were (EN_NOTE_NAME); only this screen spells by key. */
#include "theory.h"
static void cv_text2(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c);   /* (ui.c, below) */
#define TH_RED 0xE8E4u                           /* a borrowed note: red on every theme */
static int th_degree(int pc, int m)              /* the scale degree (0 ..) whose chord has this root, or -1 */
{
    int d, pcs[4];
    for (d = 0; d < m; d++)
        if (en_prog_chord(d, EN_CH_TRIAD, pcs) > 0 && pcs[0] == pc)
            return d;
    return -1;
}
/* a numeral: num the degree by letter (0 I .. 6 VII, from the key's spelling, so a pentatonic's degrees count right), its case and
 * mark from the triad on pool degree d */
static const char *th_roman(int num, int d, char *b)
{
    static const char *const R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    int pcs[4], k = 0;
    const int n = en_prog_chord(d, EN_CH_TRIAD, pcs), t = n > 2 ? (pcs[1] - pcs[0] + 12) % 12 : 4, f = n > 2 ? (pcs[2] - pcs[0] + 12) % 12 : 7;
    const char *r = R[((num % 7) + 7) % 7];
    while (*r) {
        b[k++] = (char)(t == 3 ? *r - 'A' + 'a' : *r);
        r++;
    }
    if (f == 6)
        b[k++] = 'o';
    else if (f == 8)
        b[k++] = '+';
    b[k] = 0;
    return b;
}
static const char *th_respell(const th_key_t *k, int root, const char *name, char *b)   /* "AbM7" -> "G#M7": the key's root name */
{
    const char *suf = name + (EN_NOTE_NAME[root][1] ? 2 : 1);
    char *o = b;
    th_spell(k, root, o);
    o += th_len(o);
    while (*suf && o < b + 30)
        *o++ = *suf++;
    *o = 0;
    return b;
}
static const char *th_motion(int a, int b, int key)   /* how the root moves to the next chord's (home: down a fifth to the tonic) */
{
    switch ((b - a + 12) % 12) {
    case 0: return "SAME ROOT";
    case 5: return b == key ? "5TH DOWN: HOME" : "5TH DOWN";
    case 7: return "5TH UP";
    case 2: return "STEP UP";
    case 10: return "STEP DOWN";
    case 1: return "HALF STEP UP";
    case 11: return "HALF STEP DOWN";
    case 3: case 4: return "3RD UP";
    case 8: case 9: return "3RD DOWN";
    default: return "TRITONE";
    }
}
/* a swoop from (x0, y0) to (x1, y1), bowed in toward (cx, cy) as a printed circle's arrows are (a quadratic curve, its control
 * point between the chord's middle and the circle's centre); thick: two pixels; head: an arrowhead along the curve's end */
static void th_swoop_q(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float qx, float qy, uint16_t c, int thick, float head);
static void th_swoop(int32_t cx, int32_t cy, int pa, int pb, float r, uint16_t c, int thick, int head)
{
    int d = (pb - pa + 12) % 12;
    if (d > 6)
        d -= 12;                                 /* the shorter way round (a tritone: clockwise) */
    {
        const float a0 = (float)pa * (FM_TWO_PI / 12.0f), a1 = (float)pb * (FM_TWO_PI / 12.0f), am = a0 + (float)d * (FM_TWO_PI / 24.0f);
        const float h = (float)(d < 0 ? -d : d) * (FM_TWO_PI / 48.0f), rc = r * (0.55f * fm_cosf(h) + 0.4f * fm_sinf(h));
        const int32_t x0 = cx + (int32_t)(fm_sinf(a0) * r), y0 = cy - (int32_t)(fm_cosf(a0) * r),
                      x1 = cx + (int32_t)(fm_sinf(a1) * r), y1 = cy - (int32_t)(fm_cosf(a1) * r);
        const float qx = (float)cx + fm_sinf(am) * rc, qy = (float)cy - fm_cosf(am) * rc;
        const float span = fm_sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
        th_swoop_q(x0, y0, x1, y1, qx, qy, c, thick, head ? (span * 0.4f < 9.0f ? span * 0.4f : 9.0f) : 0.0f);
    }
}
static void th_swoop_q(int32_t x0, int32_t y0, int32_t x1, int32_t y1, float qx, float qy, uint16_t c, int thick, float head)
{
    int32_t px = x0, py = y0;
    int j;
    for (j = 1; j <= 16; j++) {
        const float t = (float)j * (1.0f / 16.0f), u = 1.0f - t;
        const int32_t x = (int32_t)(u * u * (float)x0 + 2.0f * u * t * qx + t * t * (float)x1 + 0.5f),
                      y = (int32_t)(u * u * (float)y0 + 2.0f * u * t * qy + t * t * (float)y1 + 0.5f);
        if (thick) {                             /* three pixels across (0.16.20: two were hard to see) */
            sg_line2(px, py, x, y, c);
            sg_line2(px + 1, py + 1, x + 1, y + 1, c);
        } else {
            cv_line(px, py, x, y, c);
        }
        px = x, py = y;
    }
    if (head > 2.0f) {                           /* along the curve's last direction (the control point to the end): a filled head, its size
                                                 * from the move's (a short move, a small head) */
        const float dx = (float)x1 - qx, dy = (float)y1 - qy, l = fm_sqrtf(dx * dx + dy * dy) + 0.001f, ux = dx / l, uy = dy / l,
                    w = head * 0.55f;
        const float ax = (float)x1 - ux * head - uy * w, ay = (float)y1 - uy * head + ux * w,
                    bx = (float)x1 - ux * head + uy * w, by = (float)y1 - uy * head - ux * w;
        int q;
        for (q = 0; q <= 8; q++) {               /* lines from the tip across the base */
            const float t = (float)q * 0.125f;
            cv_line(x1, y1, (int32_t)(ax + (bx - ax) * t + 0.5f), (int32_t)(ay + (by - ay) * t + 0.5f), c);
        }
    }
}
static uint16_t th_fcol(int num)                 /* a degree's job as a colour: the tonic's (I iii vi) the pad's, the subdominant's (ii IV) the
                                                 * bass's, the dominant's (V vii) the acid's, as theory books group them */
{
    num = ((num % 7) + 7) % 7;
    return num == 1 || num == 3 ? K_BASS : num == 4 || num == 6 ? K_ACID : K_PAD;
}
static void vz_theory(void)
{
    static const char *const FN[7] = {"TONIC", "SUPERTONIC", "MEDIANT", "SUBDOMINANT", "DOMINANT", "SUBMEDIANT", "LEADING TONE"};
    const int key = sound_root() % 12, m = pg_scale(), root = en_chord_root % 12;
    const uint32_t scale = pool_abs();
    const int32_t cx = 73, cy = 72;
    th_key_t k;
    uint32_t chord = 0;
    float sn[12], cs[12];
    int i, d, nd = -1, ns = 0, nroot = -1, tl, num;
    char tx[40];
    uint8_t tmp[EN_PRG_LEN];
    const uint8_t *pb = pg_view(tmp);
    const int loop = pb == proj.prg && en_prg_get(pb, EN_PRG_PLAY, 1) && en_prog_pos >= 0;
    const uint16_t band = mix(K_BG, K_PAD, 5), pad = mix(K_PAD_T, K_PAD, 10 + q16(en_pad_level) * 6 / 16);
    th_key(scale, key, EN_NOTE_NAME[key][1] == 'b', &k);
    tl = th_letter(&k, key);
    for (i = 0; i < EN_NPAD; i++)
        chord |= 1u << (en_chord[i] % 12u);
    for (i = 0; i < 12; i++) {                   /* position i (C at the top, a fifth further each step clockwise) */
        const float a = (float)i * (FM_TWO_PI / 12.0f);
        sn[i] = fm_sinf(a);
        cs[i] = fm_cosf(a);
    }
#define TH_POS(pc) ((pc) * 7 % 12)
#define TH_PC(p) ((p) * 7 % 12)                  /* (7 is its own inverse mod 12) */
#define TH_X(p, r) (cx + (int32_t)(sn[p] * (float)(r) + (sn[p] < 0.0f ? -0.5f : 0.5f)))
#define TH_Y(p, r) (cy - (int32_t)(cs[p] * (float)(r) + (cs[p] < 0.0f ? -0.5f : 0.5f)))
    {   /* the scale's arc on the band (seven neighbours for a mode, five for a pentatonic); a scale that is not one run: its notes alone */
        int run = -1, n = 0, p, s;
        for (p = 0; p < 12; p++)
            n += (int)(scale >> TH_PC(p) & 1u);
        for (s = 0; s < 12 && run < 0; s++) {    /* the run's first position: in the scale, the one before it not */
            int j, ok = (scale >> TH_PC(s) & 1u) && !(scale >> TH_PC((s + 11) % 12) & 1u);
            for (j = 0; ok && j < n; j++)
                ok = (int)(scale >> TH_PC((s + j) % 12) & 1u);
            if (ok)
                run = s;
        }
        if (run >= 0 && n < 12) {
            const float a0 = (float)run * (FM_TWO_PI / 12.0f), a1 = (float)(run + n - 1) * (FM_TWO_PI / 12.0f);
            float a;
            for (a = a0; a <= a1 + 0.01f; a += 0.07f)
                cv_disc(cx + (int32_t)(fm_sinf(a) * 44.0f), cy - (int32_t)(fm_cosf(a) * 44.0f), 7, band);
        } else {
            for (p = 0; p < 12; p++)
                if (scale >> TH_PC(p) & 1u)
                    cv_disc(TH_X(p, 44), TH_Y(p, 44), 7, band);
        }
    }
    for (i = 0; i < 12; i++) {                   /* the inner ring: each major key's signature, this key's lit */
        const int pc = TH_PC(i);
        th_sig_text(th_sig_of(pc, k.sig < 0), tx);
        text_c(TH_X(i, 27), TH_Y(i, 27) - 5, &FONT_XS, tx, pc == k.parent ? K_ACC : mix(K_BG, K_DIM, 9));
    }
    {   /* the move just made (the progression jumping round the circle): the last root to this one, fading over three seconds */
        static int8_t cur = -1, prev = -1;
        static uint32_t t0;
        const uint32_t now = plat_ms();
        uint32_t life;
        if (root != cur) {
            prev = cur;
            cur = (int8_t)root;
            t0 = now;
        }
        {   /* half the chord's length: its slot's bars while the loop plays, else four bars (0.16.20: it faded before the next came) */
            const int bars = loop ? 1 << en_prg_get(pb, 24 + 2 * (en_prog_pos & 7), 2) : 4;
            life = (uint32_t)bars * 120000u / (uint32_t)(proj.par[P_TEMPO] > 0 ? proj.par[P_TEMPO] : 120);
            if (life < 2000u)
                life = 2000u;
        }
        if (prev >= 0 && prev != root && now - t0 < life) {
            const int f = 13 - (int)((now - t0) * 13u / life);
            if (f > 0)
                th_swoop(cx, cy, TH_POS(prev), TH_POS(root), 36.0f, mix(K_BG, th_fcol((th_letter(&k, prev) - tl + 7) % 7), f), 1, 0);
        }
    }
    d = th_degree(root, m);
    if (loop) {                                  /* the next slot that holds a chord, round to the first */
        for (i = 1; i <= EN_PRG_N; i++) {
            const int s = (en_prog_pos + i) % EN_PRG_N, v = en_prg_get(pb, 3 * s, 3);
            if (v) {
                int pcs[4];
                nd = v - 1;
                ns = s;
                if (en_prog_chord(nd, EN_CH_TRIAD, pcs) > 0)
                    nroot = pcs[0];
                break;
            }
        }
    } else if (en_walk_next >= 0 && en_playing) {   /* the genre's walk: its next, chosen a chord ahead (0.16.27: the arrow showed only for a progression) */
        nd = en_walk_next;
        nroot = en_walk_next_root % 12;
    }
    if (nroot >= 0 && nroot != root)             /* the move: an arrow inside the band, from this chord's root to the next one's */
        th_swoop(cx, cy, TH_POS(root), TH_POS(nroot), 36.0f, K_ACC, 1, 1);
    for (i = 0; i < 12; i++) {                   /* the band's numerals; the chord playing filled, the next one ringed */
        const int pc = TH_PC(i), dd = th_degree(pc, m);
        if (!(scale >> pc & 1u) || dd < 0)
            continue;
        {
            int L;
            char mk;
            uint16_t c;
            int32_t w, x0, y0;
            th_roman((th_letter(&k, pc) - tl + 7) % 7, dd, tx);
            L = th_len(tx);
            mk = L > 1 && (tx[L - 1] == 'o' || tx[L - 1] == '+') ? tx[L - 1] : 0;
            if (mk)                              /* on the band the mark is drawn small, raised, as printed (vii°, III+): the numeral fits */
                tx[L - 1] = 0;
            {
                const uint16_t fc = th_fcol((th_letter(&k, pc) - tl + 7) % 7);
                if (pc == root && en_playing) {
                    cv_disc(TH_X(i, 44), TH_Y(i, 44), 9, fc);
                    c = K_BG;
                } else {
                    if (pc == nroot) {
                        cv_disc(TH_X(i, 44), TH_Y(i, 44), 9, fc);
                        cv_disc(TH_X(i, 44), TH_Y(i, 44), 7, band);
                    }
                    c = pc == nroot ? fc : mix(band, fc, 12);
                }
            }
            w = text_w(&FONT_XS, tx);
            x0 = TH_X(i, 44) - (w + (mk ? 4 : 0)) / 2;
            y0 = TH_Y(i, 44) - 5;
            cv_text(x0, y0, &FONT_XS, tx, c);
            if (mk == 'o') {                     /* a ring of three by three */
                cv_rect(x0 + w + 1, y0 + 1, 3, 1, c);
                cv_rect(x0 + w + 1, y0 + 3, 3, 1, c);
                cv_rect(x0 + w + 1, y0 + 2, 1, 1, c);
                cv_rect(x0 + w + 3, y0 + 2, 1, 1, c);
            } else if (mk == '+') {
                cv_rect(x0 + w + 1, y0 + 2, 3, 1, c);
                cv_rect(x0 + w + 2, y0 + 1, 1, 3, c);
            }
        }
    }
    for (i = 0; i < 12; i++) {                   /* the outer ring: the notes as the key names them; the chord's marked, a borrowed one red */
        const int pc = TH_PC(i), in = (int)(scale >> pc & 1u), ch = (int)(chord >> pc & 1u);
        const int borrowed = ch && !in;
        if (borrowed)
            th_spell(&k, pc, tx);
        else
            th_label(&k, pc, tx);
        if (ch)
            cv_disc(TH_X(i, 55), TH_Y(i, 55), pc == root ? 3 : 2, borrowed ? TH_RED : pad);
        text_c(TH_X(i, 64), TH_Y(i, 64) - 5, &FONT_XS, tx, borrowed ? TH_RED : ch ? K_TEXT : in ? mix(K_DIM, K_TEXT, 8) : mix(K_BG, K_LINE, 12));
    }
    th_spell(&k, key, tx);                       /* the middle: the key's tonic */
    text_c(cx, cy - 9, &FONT_B, tx, K_TEXT);
    {   /* the panel */
        const int32_t x = 142;
        int pcs[EN_NPAD + 1], j;
        num = (th_letter(&k, root) - tl + 7) % 7;
        pcs[0] = root;
        for (j = 0; j < EN_NPAD; j++)
            pcs[j + 1] = en_chord[j] % 12;
        {   /* the numeral, the chord's name from the notes sounding (the slot's own type, the dominant's raised third and all), spelled */
            char nm[32];
            cv_text2(x, 0, &FONT_B, d >= 0 ? th_roman(num, d, tx) : "--", d >= 0 ? th_fcol(num) : K_DIM);
            en_chord_name(pcs, EN_NPAD + 1, nm);
            cv_text(x, 30, &FONT_B, th_respell(&k, root, nm, tx), K_TEXT);
        }
        cv_text(x, 46, &FONT_XS, en_chord_alt == 1 ? "V OF THE NEXT" : en_chord_alt == 2 ? "BORROWED" : d < 0 ? "OUTSIDE THE SCALE" :
                num == 6 && (scale >> ((key + 10) % 12) & 1u) ? "SUBTONIC" : FN[num], en_chord_alt ? TH_RED : d < 0 ? K_DIM : th_fcol(num));
        {   /* its notes, from the root up, spelled; a borrowed one red */
            int32_t xx = x;
            for (j = 0; j < 12; j++) {
                const int pc = (root + j) % 12;
                if (!(chord >> pc & 1u))
                    continue;
                th_spell(&k, pc, tx);
                xx = cv_text(xx, 57, &FONT_XS, tx, (scale >> pc & 1u) ? K_TEXT : TH_RED) + 5;
            }
        }
        if (nd >= 0) {                           /* what comes next, and how the root gets there */
            char r[8], nm[32];
            int32_t xn;
            cv_text(x, 70, &FONT_XS, "NEXT", K_DIM);
            xn = x + text_w(&FONT_XS, "NEXT") + 5;
            xn = cv_text(xn, 70, &FONT_XS, th_roman((th_letter(&k, nroot >= 0 ? nroot : key) - tl + 7) % 7, nd, r), K_ACC) + 5;
            cv_text(xn, 70, &FONT_XS, nroot >= 0 ? th_respell(&k, nroot, pg_name(nd + 1, (loop ? pk_get(ns) : 0), nm), tx) : pg_name(nd + 1, (loop ? pk_get(ns) : 0), tx), K_TEXT);
            if (nroot >= 0)
                cv_text(x, 81, &FONT_XS, th_motion(root, nroot, key), mix(K_DIM, K_ACC, 10));
        } else if (en_playing && en_drift_bars < 98.5f) {
            const int b = (int)(en_drift_bars + 0.99f);
            int q = 0;
            const char *s = "NEXT IN ";
            while (*s)
                tx[q++] = *s++;
            if (b >= 10)
                tx[q++] = (char)('0' + b / 10 % 10);
            tx[q++] = (char)('0' + b % 10);
            s = b == 1 ? " BAR" : " BARS";
            while (*s)
                tx[q++] = *s++;
            tx[q] = 0;
            cv_text(x, 70, &FONT_XS, tx, K_DIM);
        } else {
            cv_text(x, 70, &FONT_XS, en_frozen ? "FROZEN" : "HELD", K_DIM);
        }
        for (i = 0; i < EN_PRG_N; i++) {         /* the eight slots: the progression as a chord chart (the walk as heard, loop off) */
            const int v = en_prg_get(pb, 3 * i, 3);
            const int32_t sx = x + (i & 3) * 24, sy = 93 + (i >> 2) * 12;
            const int now = loop ? i == en_prog_pos : v && v - 1 == d;
            cv_rect(sx, sy, 22, 11, now ? mix(K_BG, K_PAD, 6) : K_PANEL);
            if (v) {
                int p2[4];
                const int rp = en_prog_chord(v - 1, EN_CH_TRIAD, p2) > 0 ? p2[0] : key;
                text_c(sx + 11, sy + 1, &FONT_XS, th_roman((th_letter(&k, rp) - tl + 7) % 7, v - 1, tx), now ? K_ACC : loop ? K_TEXT : K_DIM);
            }
        }
        {   /* the key, and its signature with the major key it is written in */
            char *o = tx;
            const char *mn = mode_name();
            th_spell(&k, key, o);
            o += th_len(o);
            *o++ = ' ';
            while (*mn && o < tx + 20) {
                *o++ = (char)(*mn >= 'a' && *mn <= 'z' ? *mn - 'a' + 'A' : *mn);
                mn++;
            }
            *o = 0;
            cv_text(x, 118, &FONT_XS, tx, K_TEXT);
            o = th_sig_text(k.sig, tx);
            *o++ = ':';
            *o++ = ' ';
            th_spell(&k, k.parent, o);
            o += th_len(o);
            {
                const char *s = " MAJOR";
                while (*s)
                    *o++ = *s++;
                *o = 0;
            }
            cv_text(x, 129, &FONT_XS, tx, K_DIM);
        }
    }
#undef TH_POS
#undef TH_PC
#undef TH_X
#undef TH_Y
}

static void draw_viz(void)
{
    int mode = ui.viz % VZ_N;
    const int full = ui.saver_on;                  /* the screensaver: the whole screen, in two bands of 120 rows */
    const int32_t h = full ? 240 : VZ_H;
    int band;
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
    if (vz.h != h) {                               /* the page and the whole screen differ in size: the state starts fresh */
        vz.h = h;
        vz.mode_seen = 255;
    }
    vz.full = (uint8_t)full;
    vz_signals();
    if (mode == VZ_AUTO) {                         /* a new one on a section change and every 32 bars */
        if (en_sec != vz.sec_seen || en_bars - vz.auto_bar >= 512u) {
            vz.sec_seen = en_sec;
            vz.auto_bar = en_bars;
            vz.auto_cur = (uint8_t)((vz.auto_cur + 1u) % (VZ_N - 2));   /* (THEORY is not a light show: not in the turn) */
        }
        mode = vz.auto_cur;
    }
    if (mode != vz.mode_seen) {                    /* a new one: its state fresh */
        vz.mode_seen = (uint8_t)mode;
        if (mode == VZ_FLUX || mode == VZ_KALEIDO)
            vz_flux_init();
        if (mode == VZ_LIFE)
            vz_life_seed();
    }
    vz_lut();
    for (band = 0; band < (full ? 2 : 1); band++) {
        vz.pass = (uint8_t)band;
        vz.y0 = band * 120;
        if (full) {
            cv_begin(240, 120, K_BG);
            cv_oy = -vz.y0;
        } else {
            cv_begin(VZ_W, VZ_H, K_BG);
        }
        switch (mode) {
        case VZ_PLASMA: vz_plasma(); break;
        case VZ_KALEIDO: vz_kaleido(); break;
        case VZ_BARS: vz_bars(); break;
        case VZ_FLUX: vz_flux(); break;
        case VZ_DISCO: vz_disco(); break;
        case VZ_LIFE: vz_life(); break;
        case VZ_TRIP: vz_trip(); break;
        case VZ_THEORY: vz_theory(); break;
        default: vz_orbit(); break;
        }
        cv_blit(0, full ? (uint32_t)vz.y0 : MAIN_Y);
    }
    vz.pass = 0;
}
