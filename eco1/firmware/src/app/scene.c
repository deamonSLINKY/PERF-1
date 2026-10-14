/* SPDX-License-Identifier: GPL-3.0-only */
/* ECO-1 scenes: each plant its own body and its own place, drawn under the day's sky. Included by ui.c
 * (after its drawing helpers). The three leaves are the plant's three parts that play, the stem the part
 * that drones:
 *
 *   FERN    three fronds from the forest floor (left, right, the tall middle), a fiddlehead at the base
 *   ORCHID  on a windowsill: two broad leaves from a pot, a flower spike arching over (the top leaf: its
 *           flowers); the weather is outside the glass
 *   MOSS    a cushion on a boulder by a stream, three spore stalks rising from it
 *   CACTUS  a saguaro in the desert: two arms and the crown, which flowers; stiff in the wind
 *   WILLOW  a weeping willow by a lake: three curtains of hanging strands, the wind's favourite
 *   LOTUS   a pond: two lily pads and the flower, which closes at night; the rain rings the water
 *   IVY     three vines up a brick wall
 *   BAMBOO  three culms in the mist under mountains, bending with the wind
 *
 * Every part greens as it is watered and yellows as it dries, flashes as it fires, and its vein (or its
 * equivalent) warms as its charge builds. The night darkens everything but the flashes. */

#define GROUND 124
static float sc_dl;                              /* daylight, 0..1, for this frame */
static int32_t sc_t;                             /* the animation's clock: frames / 3 */
static uint16_t sc_top, sc_hor;                  /* the sky's colours, top and at the horizon */

static int32_t ri(float x) { return (int32_t)(x + (x < 0.0f ? -0.5f : 0.5f)); }
static uint16_t lit(uint16_t c) { return mix(RGB(8, 10, 22), c, q16(0.2f + 0.8f * sc_dl)); }
static uint16_t haze(uint16_t c, float f) { return mix(c, sc_hor, q16(f)); }   /* distance: toward the horizon */
static void blob(float x, float y, float r, uint16_t c)
{
    if (r <= 6.0f)
        cv_disc(ri(x), ri(y), ri(r), c);
    else
        disc(x - 0.5f, y - 0.5f, r, c);
}
static void seg(float x0, float y0, float x1, float y1, float w, uint16_t c)   /* a line w wide */
{
    float dx = x1 - x0, dy = y1 - y0, len = fm_sqrtf(dx * dx + dy * dy);
    int32_t k, n;
    if (w < 1.5f) {
        cv_line(ri(x0), ri(y0), ri(x1), ri(y1), c);
        return;
    }
    n = (int32_t)(len / fm_maxf(w * 0.45f, 1.0f)) + 1;
    for (k = 0; k <= n; k++) {
        float t = (float)k / (float)n;
        blob(x0 + dx * t, y0 + dy * t, w * 0.5f, c);
    }
}
static void oval(float cx, float cy, float rx, float ry, uint16_t c)
{
    int32_t y, y0 = ri(cy - ry), y1 = ri(cy + ry);
    for (y = y0; y <= y1; y++) {
        float d = ((float)y - cy) / ry, hw = 1.0f - d * d;
        if (hw <= 0.0f)
            continue;
        hw = rx * fm_sqrtf(hw);
        cv_rect(ri(cx - hw), y, ri(2.0f * hw) + 1, 1, c);
    }
}
static void oval_line(float cx, float cy, float rx, float ry, uint16_t c, uint32_t a)   /* an ellipse's outline */
{
    int32_t k, n = 8 + (int32_t)(rx * 1.5f);
    for (k = 0; k < n; k++) {
        float th = (float)k / (float)n;
        px_blend(ri(cx + rx * fm_cosf(th * FM_TWO_PI)), ri(cy + ry * fm_sinf(th * FM_TWO_PI)), c, a);
    }
}
static float hyd_of(int l) { return fm_clampf((en_leaf_water[l] - 0.08f) * (1.0f / 0.3f), 0.0f, 1.0f); }
/* a part's colour: green (its own) as it's watered, dry as it dries, lit by the day, flashing as it fires */
static uint16_t part(int l, uint16_t green, uint16_t dry)
{
    return mix(lit(mix(dry, green, q16(hyd_of(l)))), RGB(214, 255, 150), q16(en_leaf_glow[l] * 0.9f));
}
static uint16_t vein(int l, uint16_t base)
{
    return mix(lit(base), RGB(250, 236, 140), q16(en_leaf_charge[l] * en_leaf_charge[l]));
}
static void spark(float x, float y, int l)
{
    if (en_leaf_glow[l] > 0.4f)
        disc(x - 0.5f, y - 0.5f, 1.0f + 2.5f * en_leaf_glow[l], RGB(250, 236, 140));
}
static uint32_t hh(uint32_t a, uint32_t b) { return hash(hash(2166136261u, a), b); }

/* ------------------------------------------------------------------ the fern --- */
static void bg_forest(int dark)                 /* trunks in the mist; dark: closer, moss's wood */
{
    static const int16_t X[6] = {14, 46, 74, 172, 204, 232}, W[6] = {9, 6, 4, 5, 11, 7};
    int i;
    for (i = 0; i < 6; i++) {
        float far = (i == 2 || i == 3) ? 0.7f : (i == 1 || i == 4) ? 0.45f : 0.2f;
        cv_rect(X[i] - W[i] / 2, 0, W[i], GROUND, haze(lit(RGB(52, 40, 30)), far - (dark ? 0.15f : 0.0f)));
    }
    for (i = 0; i < 16; i++) {                   /* the canopy's edge */
        uint32_t h = hh(0xCA0Bu, (uint32_t)i);
        blob((float)(h % 240u), (float)(h >> 8 & 7u), 6.0f, lit(RGB(34, 62, 34)));
    }
}
static void ground_loam(void)
{
    int i;
    cv_rect(0, GROUND, 240, MAIN_H - GROUND, lit(mix(RGB(110, 84, 56), RGB(44, 32, 22), q16(en_soil))));
    for (i = 0; i < 22; i++) {                   /* leaf litter */
        uint32_t h = hh(0x11770u, (uint32_t)i);
        cv_rect((int32_t)(h % 238u), GROUND + 1 + (int32_t)(h >> 8 & 15u), 3, 1,
                lit((h >> 12 & 1u) ? RGB(150, 90, 40) : RGB(96, 70, 40)));
    }
}
static void frond(int l, float bx, float by, float a0, float len, float curl)
{
    float hyd = hyd_of(l), x = bx, y = by, a, lx = bx, ly = by;
    uint16_t c = part(l, RGB(66, 150, 64), RGB(170, 150, 70)), cv = vein(l, RGB(40, 88, 40));
    int32_t k, n = (int32_t)(len / 3.0f);
    curl *= 1.0f + 1.6f * (1.0f - hyd);          /* dry: it curls down */
    a = a0 + en_leaf_sway[l] * 0.05f;
    for (k = 0; k <= n; k++) {
        float t = (float)k / (float)n, ca, sa, pl;
        a += curl * (0.4f + 1.2f * t) + en_leaf_sway[l] * 0.004f * t;
        ca = fm_cosf(a * FM_TWO_PI);
        sa = fm_sinf(a * FM_TWO_PI);
        pl = 12.0f * (t < 0.18f ? t / 0.18f : (1.0f - t) / 0.82f) * (0.75f + 0.25f * hyd);
        if (k > 0 && pl > 0.8f) {                /* the pinnae, both sides, swept toward the tip */
            float pa = (a + 0.21f) * FM_TWO_PI, pb = (a - 0.21f) * FM_TWO_PI;
            seg(x, y, x + fm_cosf(pa) * pl, y + fm_sinf(pa) * pl - (1.0f - hyd) * pl * 0.4f, 2.2f, c);
            seg(x, y, x + fm_cosf(pb) * pl, y + fm_sinf(pb) * pl - (1.0f - hyd) * pl * 0.4f, 2.2f, c);
        }
        lx = x;
        ly = y;
        x += ca * 3.0f;
        y += sa * 3.0f;
        cv_line(ri(lx), ri(ly), ri(x), ri(y), cv);
    }
    spark(x, y, l);
}
static void draw_fern(void)
{
    float s = en_stem_level;
    int k;
    frond(0, 116.0f, (float)GROUND, 0.685f, 70.0f, -0.0095f);
    frond(1, 124.0f, (float)GROUND, 0.815f, 66.0f, 0.0095f);
    frond(2, 120.0f, (float)GROUND, 0.762f, 88.0f, -0.0035f);
    for (k = 0; k < 7; k++) {                    /* the fiddlehead: the stem, uncurling as it drones */
        float th = (float)k * (0.9f - 0.3f * s), r = 5.0f - 0.6f * (float)k;
        blob(132.0f + fm_cosf(th) * r, (float)GROUND - 6.0f + fm_sinf(th) * r, 1.6f, lit(mix(RGB(70, 120, 50), RGB(140, 200, 90), q16(s * 2.0f))));
    }
}

/* ---------------------------------------------------------------- the orchid --- */
#define WIN_X0 34
#define WIN_X1 206
#define WIN_Y0 6
static void mask_window(void)                    /* the room around the window, the frame, the glass bars */
{
    uint16_t wall = lit(RGB(206, 196, 170)), wood = lit(RGB(236, 232, 220)), sh = lit(RGB(170, 160, 136));
    cv_rect(0, 0, WIN_X0, GROUND, wall);
    cv_rect(WIN_X1, 0, 240 - WIN_X1, GROUND, wall);
    cv_rect(0, 0, 240, WIN_Y0, wall);
    cv_rect(WIN_X0, WIN_Y0, WIN_X1 - WIN_X0, 4, wood);
    cv_rect(WIN_X0, WIN_Y0, 4, GROUND - WIN_Y0, wood);
    cv_rect(WIN_X1 - 4, WIN_Y0, 4, GROUND - WIN_Y0, wood);
    cv_rect(118, WIN_Y0, 4, GROUND - WIN_Y0, wood);
    cv_rect(WIN_X0, 62, WIN_X1 - WIN_X0, 3, wood);
    cv_rect(WIN_X0 - 2, WIN_Y0 - 2, 2, GROUND - WIN_Y0 + 2, sh);
    cv_rect(WIN_X1, WIN_Y0 - 2, 2, GROUND - WIN_Y0 + 2, sh);
}
static void ground_sill(void)
{
    cv_rect(0, GROUND, 240, MAIN_H - GROUND, lit(RGB(196, 160, 112)));
    cv_rect(0, GROUND, 240, 2, lit(RGB(232, 206, 160)));
    cv_rect(0, GROUND + 9, 240, 1, lit(RGB(160, 124, 84)));
}
static void blade(int l, float bx, float by, float a0, float len, float w, uint16_t green, uint16_t dry, float curl)
{
    float hyd = hyd_of(l), x = bx, y = by, a = a0 + en_leaf_sway[l] * 0.04f;
    uint16_t c = part(l, green, dry), cv = vein(l, mix(green, RGB(20, 40, 20), 8));
    int32_t k, n = (int32_t)(len * 0.5f);
    curl *= 1.0f + 2.0f * (1.0f - hyd);
    for (k = 0; k <= n; k++) {
        float t = (float)k / (float)n, r = w * fm_sqrtf(fm_sin_turns(t * 0.5f)) * (1.0f - 0.3f * t);
        blob(x, y, r * (0.75f + 0.25f * hyd), c);
        a += curl;
        x += fm_cosf(a * FM_TWO_PI) * 2.0f;
        y += fm_sinf(a * FM_TWO_PI) * 2.0f;
    }
    a = a0 + en_leaf_sway[l] * 0.04f;
    x = bx;
    y = by;
    for (k = 0; k < n; k++) {
        float nx, ny;
        a += curl;
        nx = x + fm_cosf(a * FM_TWO_PI) * 2.0f;
        ny = y + fm_sinf(a * FM_TWO_PI) * 2.0f;
        cv_line(ri(x), ri(y), ri(nx), ri(ny), cv);
        x = nx;
        y = ny;
    }
    spark(x, y, l);
}
static void flower_orchid(float x, float y, float open, int l)
{
    float hyd = hyd_of(l), r = (3.0f + 2.5f * open) * (0.7f + 0.3f * hyd), d = r * 0.9f;
    uint16_t pet = part(l, RGB(250, 236, 246), RGB(196, 170, 120)), lip = lit(RGB(196, 50, 140));
    blob(x, y - d, r * 0.8f, pet);                /* the dorsal sepal */
    blob(x - d, y - 0.2f * d, r, pet);            /* the petals */
    blob(x + d, y - 0.2f * d, r, pet);
    blob(x - 0.6f * d, y + d * 0.9f, r * 0.7f, pet);
    blob(x + 0.6f * d, y + d * 0.9f, r * 0.7f, pet);
    blob(x, y + 0.3f * d, r * 0.55f, mix(lip, RGB(255, 160, 220), q16(en_leaf_glow[l])));
    blob(x, y - 0.1f * d, 1.0f, lit(RGB(250, 220, 90)));
}
static void draw_orchid(void)
{
    float px = 120.0f, top = (float)GROUND - 24.0f, k, sw = en_leaf_sway[2] * 3.0f;
    int32_t y;
    for (y = 0; y < 24; y++) {                   /* the pot */
        float w = 13.0f + 5.0f * (float)y / 24.0f;
        cv_rect(ri(px - w), ri(top) + y, ri(2.0f * w), 1, lit(y < 4 ? RGB(170, 86, 50) : RGB(196, 104, 62)));
    }
    blade(0, px - 4.0f, top - 1.0f, 0.55f, 44.0f, 8.5f, RGB(52, 112, 60), RGB(170, 150, 70), -0.0022f);
    blade(1, px + 4.0f, top - 2.0f, 0.95f, 40.0f, 8.0f, RGB(52, 112, 60), RGB(170, 150, 70), 0.0024f);
    {   /* the spike, arching over to the right, with the flowers on it */
        float x0 = px + 2.0f, y0 = top - 2.0f, cx = px + 8.0f, cy = 18.0f, x2 = 176.0f + sw, y2 = 44.0f, lx = x0, ly = y0;
        float open = 0.55f + 0.45f * en_leaf_charge[2] + 0.5f * en_leaf_glow[2];
        uint16_t sc = lit(mix(RGB(90, 80, 50), RGB(60, 110, 60), 8));
        for (k = 0.05f; k <= 1.001f; k += 0.05f) {
            float u = 1.0f - k, x = u * u * x0 + 2.0f * u * k * cx + k * k * x2, yy = u * u * y0 + 2.0f * u * k * cy + k * k * y2;
            seg(lx, ly, x, yy, 2.0f, sc);
            lx = x;
            ly = yy;
        }
        for (k = 0.55f; k <= 1.001f; k += 0.2f) {
            float u = 1.0f - k, x = u * u * x0 + 2.0f * u * k * cx + k * k * x2, yy = u * u * y0 + 2.0f * u * k * cy + k * k * y2;
            flower_orchid(x, yy + 8.0f, open, 2);
        }
        blob(x2 + 4.0f, y2 + 6.0f, 2.0f, lit(RGB(200, 210, 150)));   /* a bud */
        spark(x2, y2 + 8.0f, 2);
    }
    blob(px + 10.0f, top - 4.0f, 1.0f + 2.0f * en_stem_level, lit(RGB(120, 160, 90)));   /* an aerial root */
}

/* ------------------------------------------------------------------ the moss --- */
static void ground_stream(void)
{
    int32_t i;
    cv_rect(0, GROUND, 240, MAIN_H - GROUND, lit(mix(RGB(70, 58, 40), RGB(36, 30, 22), q16(en_soil))));
    cv_rect(0, GROUND + 7, 240, 7, lit(RGB(70, 110, 130)));
    for (i = 0; i < 9; i++) {                    /* the stream running */
        uint32_t h = hh(0x57EAu, (uint32_t)i);
        int32_t x = (int32_t)((h % 260u + (uint32_t)(sc_t * 2)) % 260u) - 10;
        cv_rect(x, GROUND + 8 + (int32_t)(h >> 9 & 3u), 6 + (int32_t)(h >> 12 & 7u), 1, lit(RGB(170, 210, 220)));
    }
}
static void draw_moss(void)
{
    const float cx = 120.0f, cy = (float)GROUND + 2.0f;
    int i, l;
    oval(cx, cy, 64.0f, 30.0f, lit(RGB(112, 110, 104)));          /* the boulder */
    oval(cx - 14.0f, cy - 4.0f, 40.0f, 18.0f, lit(RGB(132, 130, 122)));
    for (i = 0; i < 70; i++) {                   /* the cushion over its top */
        uint32_t h = hh(0x5055u, (uint32_t)i);
        float u = (float)(h % 1000u) / 1000.0f * 2.0f - 1.0f, x = cx + u * 58.0f;
        float y = cy - 28.0f * fm_sqrtf(fm_maxf(1.0f - u * u, 0.0f)) + (float)(h >> 10 & 7u) * 0.8f;
        int l2 = u < -0.33f ? 0 : u > 0.33f ? 1 : 2;
        blob(x, y, 2.0f + (float)(h >> 14 & 3u) * 0.6f,
             mix(part(l2, (h >> 16 & 1u) ? RGB(104, 160, 48) : RGB(80, 136, 44), RGB(130, 120, 60)), RGB(200, 255, 130),
                 q16(en_stem_level * 0.5f)));
    }
    for (l = 0; l < EN_NLEAF; l++) {             /* the spore stalks: setae, capsules */
        static const float X[EN_NLEAF] = {98.0f, 144.0f, 120.0f}, H[EN_NLEAF] = {26.0f, 24.0f, 34.0f};
        float bx = X[l], by = cy - 27.0f + fm_fabsf(bx - cx) * 0.25f, hyd = hyd_of(l);
        float tx = bx + en_leaf_sway[l] * 4.0f + (1.0f - hyd) * 4.0f, ty = by - H[l] * (0.8f + 0.2f * hyd);
        seg(bx, by, tx, ty, 1.0f, vein(l, RGB(150, 70, 40)));
        cv_line(ri(bx) + 1, ri(by), ri(tx) + 1, ri(ty), lit(RGB(120, 56, 34)));
        oval(tx + 1.0f, ty - 2.0f, 2.0f, 4.0f, part(l, RGB(160, 120, 50), RGB(120, 90, 50)));
        if (en_leaf_glow[l] > 0.1f)              /* spores, drifting up */
            for (i = 0; i < 6; i++) {
                uint32_t h = hh(0x5904Eu + (uint32_t)l, (uint32_t)i);
                float rise = (1.0f - en_leaf_glow[l]) * 30.0f;
                px_blend(ri(tx + (float)(h % 13u) - 6.0f + en_wind * rise * 0.8f), ri(ty - 4.0f - rise - (float)(h >> 8 & 7u)),
                         RGB(230, 220, 150), (uint32_t)(16.0f * en_leaf_glow[l]));
            }
    }
}

/* ---------------------------------------------------------------- the cactus --- */
static void bg_desert(void)
{
    int32_t x;
    for (x = 0; x < 240; x++) {                   /* far mesas, then nearer ones */
        float f = fm_sin_turns((float)x / 300.0f) * 6.0f + fm_sin_turns((float)x / 70.0f + 0.3f) * 3.0f;
        int32_t h1 = (x > 20 && x < 90) ? 34 : (x > 150 && x < 230) ? 26 : 14 + ri(f);
        int32_t h2 = (x > 56 && x < 104) ? 20 : (x > 180) ? 16 : 6 + ri(f * 0.5f);
        cv_rect(x, GROUND - h1, 1, h1, haze(lit(RGB(170, 104, 80)), 0.55f));
        cv_rect(x, GROUND - h2, 1, h2, haze(lit(RGB(184, 96, 58)), 0.2f));
    }
}
static void ground_sand(void)
{
    int i;
    cv_rect(0, GROUND, 240, MAIN_H - GROUND, lit(mix(RGB(226, 190, 132), RGB(170, 120, 76), q16(en_soil))));
    for (i = 0; i < 14; i++) {
        uint32_t h = hh(0x5A2Du, (uint32_t)i);
        cv_rect((int32_t)(h % 236u), GROUND + 2 + (int32_t)(h >> 8 & 15u), 2, 1, lit(RGB(150, 110, 80)));
    }
}
static void column(float x, float y0, float y1, float w, uint16_t c, uint16_t rib)   /* upright, round-topped */
{
    cv_rect(ri(x - w * 0.5f), ri(y1), ri(w), ri(y0 - y1), c);
    blob(x, y1, w * 0.5f, c);
    cv_rect(ri(x - w * 0.18f), ri(y1), 1, ri(y0 - y1), rib);
    cv_rect(ri(x + w * 0.18f), ri(y1), 1, ri(y0 - y1), rib);
}
static void draw_cactus(void)
{
    const float x = 120.0f, g = (float)GROUND;
    float sw = en_leaf_sway[2] * 1.5f, w = 18.0f - 3.0f * (1.0f - hyd_of(2));
    uint16_t c2 = part(2, RGB(64, 128, 84), RGB(150, 150, 80)), rib2 = vein(2, RGB(96, 160, 112));
    int l, i;
    for (l = 0; l < 2; l++) {                    /* the arms: out, then up */
        float ay = g - (l ? 58.0f : 38.0f), dir = l ? 1.0f : -1.0f, ex = x + dir * (l ? 24.0f : 27.0f), top = ay - (l ? 30.0f : 36.0f);
        float aw = 12.0f - 2.0f * (1.0f - hyd_of(l));
        uint16_t c = part(l, RGB(64, 128, 84), RGB(150, 150, 80)), rib = vein(l, RGB(96, 160, 112));
        seg(x, ay, ex, ay - 3.0f, aw, c);
        blob(ex, ay - 3.0f, aw * 0.5f, c);
        column(ex, ay - 3.0f, top, aw, c, rib);
        spark(ex, top - 5.0f, l);
    }
    column(x + sw * 0.0f, g, g - 98.0f, w, c2, rib2);   /* the trunk (the crown is its top) */
    for (i = 0; i < 18; i++) {                   /* spines */
        uint32_t h = hh(0x5714u, (uint32_t)i);
        px_blend(ri(x - w * 0.5f + (float)(h % (uint32_t)w)), ri(g - 4.0f - (float)(h >> 8 & 63u) * 1.4f), RGB(240, 230, 200), 8);
    }
    {   /* the crown's flowers: buds as the charge builds, open as it fires (and after rain) */
        float o = en_leaf_charge[2] * 0.6f + en_leaf_glow[2];
        if (o > 0.15f)
            for (i = 0; i < 4; i++) {
                float fx = x - 6.0f + (float)i * 4.0f + sw, fy = g - 104.0f + (float)((i & 1) * 2);
                blob(fx, fy, 1.2f + 2.2f * fm_minf(o, 1.0f), lit(RGB(250, 248, 236)));
                blob(fx, fy, 0.8f, lit(RGB(240, 200, 80)));
            }
        spark(x, g - 108.0f, 2);
    }
}

/* ---------------------------------------------------------------- the willow --- */
static void bg_lake(void)
{
    int32_t x, y;
    for (x = 0; x < 240; x++) {                   /* the far shore's trees */
        int32_t h = 6 + ri(fm_fabsf(fm_sin_turns((float)x / 23.0f)) * 5.0f + fm_sin_turns((float)x / 61.0f) * 3.0f);
        cv_rect(x, 92 - h, 1, h, haze(lit(RGB(50, 80, 56)), 0.5f));
    }
    for (y = 92; y < GROUND; y++)                 /* the lake, the sky in it */
        cv_rect(0, y, 240, 1, lit(mix(sc_hor, RGB(60, 100, 120), 7 + (y - 92) / 4)));
    for (x = 0; x < 12; x++) {
        uint32_t h = hh(0x1A4Eu, (uint32_t)x);
        cv_rect((int32_t)((h % 240u + (uint32_t)sc_t) % 240u), 94 + (int32_t)((h >> 8) % 28u), 8 + (int32_t)(h >> 13 & 7u), 1,
                mix(lit(RGB(110, 150, 170)), RGB(230, 240, 250), q16(sc_dl * 0.6f)));
    }
}
static void ground_grass(void)
{
    int i;
    cv_rect(0, GROUND, 240, MAIN_H - GROUND, lit(mix(RGB(120, 130, 70), RGB(56, 84, 40), q16(en_soil))));
    for (i = 0; i < 40; i++) {
        uint32_t h = hh(0x6A55u, (uint32_t)i);
        int32_t x = (int32_t)(h % 240u);
        cv_line(x, GROUND + 2, x + ri(en_wind * 2.0f), GROUND - 2 - (int32_t)(h >> 8 & 3u), lit(RGB(90, 120, 56)));
    }
}
static void curtain(int l, float x0, float x1)
{
    float hyd = hyd_of(l), len = (52.0f + 14.0f * hyd);
    uint16_t c = part(l, RGB(150, 190, 90), RGB(176, 160, 80)), cl = mix(c, RGB(230, 250, 180), 5);
    int s;
    for (s = 0; s < 7; s++) {
        float x = x0 + (x1 - x0) * (float)s / 6.0f, y = (float)GROUND - 66.0f + fm_fabsf(x - 120.0f) * 0.12f, lx = x, ly = y, k;
        float ph = (float)s * 0.37f + (float)l;
        for (k = 0.0f; k < len; k += 3.0f) {
            float u = k / len, nx = x + en_leaf_sway[l] * u * u * 22.0f + fm_sin_turns(ph + (float)sc_t * 0.01f + u) * u * 1.5f;
            float ny = y + k;
            seg(lx, ly, nx, ny, 1.0f, c);
            if (((int32_t)k / 3 + s) % 2 == 0)
                cv_rect(ri(nx) + 1, ri(ny), 1, 1, cl);
            lx = nx;
            ly = ny;
        }
        if (s == 3)
            spark(lx, ly, l);
    }
}
static void draw_willow(void)
{
    const float g = (float)GROUND;
    uint16_t bark = lit(RGB(92, 70, 50));
    int i;
    seg(120.0f, g, 117.0f, g - 70.0f, 10.0f, bark);
    seg(117.0f, g - 60.0f, 96.0f, g - 70.0f, 4.0f, bark);
    seg(118.0f, g - 56.0f, 146.0f, g - 70.0f, 4.0f, bark);
    for (i = 0; i < 110; i++) {                  /* the crown's dome */
        uint32_t h = hh(0x3170u, (uint32_t)i);
        float u = (float)(h % 1000u) / 1000.0f, r = (float)((h >> 10) % 1000u) / 1000.0f;
        blob(70.0f + 100.0f * u, g - 64.0f - fm_sin_turns(u * 0.5f) * 20.0f * fm_sqrtf(r), 6.0f,
             lit(mix((h >> 20 & 1u) ? RGB(90, 130, 60) : RGB(110, 150, 70), RGB(150, 210, 120), q16(en_stem_level * 0.6f))));
    }
    curtain(0, 72.0f, 100.0f);
    curtain(1, 140.0f, 168.0f);
    curtain(2, 104.0f, 136.0f);
}

/* ----------------------------------------------------------------- the lotus --- */
static void bg_hills(void)
{
    int32_t x;
    for (x = 0; x < 240; x++) {                   /* hills over the far bank */
        int32_t h = 10 + ri(fm_sin_turns((float)x / 180.0f + 0.1f) * 7.0f + fm_sin_turns((float)x / 50.0f) * 2.0f);
        cv_rect(x, 80 - h, 1, h, haze(lit(RGB(70, 96, 76)), 0.55f));
    }
}
static void bg_pond(void)
{
    int32_t y;
    for (y = 80; y < MAIN_H; y++)                 /* the pond, down through the ground band */
        cv_rect(0, y, 240, 1, lit(mix(sc_hor, RGB(40, 90, 96), 6 + (y - 80) / 7)));
    {   /* the sun or the moon in it */
        float u = en_day >= 0.25f && en_day < 0.75f ? (en_day - 0.25f) * 2.0f : (en_day >= 0.75f ? en_day - 0.75f : en_day + 0.25f) * 2.0f;
        int32_t sx = ri(14.0f + 212.0f * u);
        for (y = 84; y < MAIN_H; y += 3)
            cv_rect(sx - 3 + ri(fm_sin_turns((float)(y + sc_t) * 0.07f) * 2.0f), y, 6, 1,
                    mix(lit(RGB(60, 110, 120)), sc_dl > 0.2f ? RGB(255, 230, 160) : RGB(200, 210, 230), 7));
    }
    if (en_rain > 0.02f) {                       /* the rain rings the water */
        int i, n = 4 + (int)(en_rain * 18.0f);
        for (i = 0; i < n; i++) {
            uint32_t h = hh(0x919Eu + (uint32_t)((sc_t + i * 5) / 12), (uint32_t)i);
            float age = (float)((sc_t + i * 5) % 12) / 12.0f;
            oval_line((float)(h % 240u), 86.0f + (float)((h >> 8) % 54u), 1.0f + 7.0f * age, 0.5f + 2.2f * age,
                 RGB(220, 236, 240), (uint32_t)(12.0f * (1.0f - age)));
        }
    }
}
static void pad(int l, float cx, float cy, float rx)
{
    float bob = fm_sin_turns((float)sc_t * 0.02f + (float)l * 0.4f) * en_wind * 1.5f + en_leaf_sway[l] * 1.5f;
    uint16_t c = part(l, RGB(56, 130, 60), RGB(150, 140, 70)), rim = vein(l, RGB(90, 170, 90)), water = lit(mix(sc_hor, RGB(40, 90, 96), 9));
    int32_t k;
    cy += bob;
    oval(cx, cy + 1.0f, rx + 1.0f, rx * 0.24f + 1.0f, rim);
    oval(cx, cy, rx, rx * 0.24f, c);
    for (k = 0; k < 4; k++)                       /* its notch */
        cv_line(ri(cx), ri(cy), ri(cx + rx * 0.95f), ri(cy - rx * 0.06f + (float)k * 0.6f), water);
    spark(cx - rx * 0.4f, cy - 1.0f, l);
}
static void draw_lotus(void)
{
    const float g = (float)GROUND;
    float sw = en_leaf_sway[2] * 4.0f, fx = 120.0f + sw, fy = g - 44.0f;
    /* the flower closes at night and opens as its charge builds and it fires */
    float open = fm_clampf((0.25f + 0.75f * sc_dl) * (0.6f + 0.4f * en_leaf_charge[2]) + 0.4f * en_leaf_glow[2], 0.15f, 1.0f);
    uint16_t pet = part(2, RGB(250, 160, 196), RGB(200, 160, 150)), pet2 = mix(pet, RGB(255, 236, 244), 6);
    int k;
    pad(0, 76.0f, g + 2.0f, 30.0f);
    pad(1, 168.0f, g - 4.0f, 24.0f);
    seg(121.0f, g + 4.0f, fx, fy + 4.0f, 2.0f, lit(RGB(70, 120, 60)));          /* the stalk */
    for (k = -2; k <= 2; k++) {                   /* the petals, back row then front */
        float a = (float)k * 0.065f * (0.3f + 0.7f * open), len = 12.0f + 4.0f * open;
        float tx = fx + fm_sin_turns(a) * len, ty = fy - fm_cosf(a * FM_TWO_PI) * len;
        seg(fx, fy, tx, ty, 6.0f, pet);
    }
    for (k = -1; k <= 1; k += 2) {
        float a = (float)k * 0.12f * open, len = 9.0f;
        seg(fx, fy + 2.0f, fx + fm_sin_turns(a) * len, fy + 2.0f - fm_cosf(a * FM_TWO_PI) * len, 6.0f, pet2);
    }
    blob(fx, fy - 2.0f, 2.0f + 1.5f * open, lit(RGB(240, 210, 80)));
    spark(fx, fy - 18.0f, 2);
    blob(150.0f, g - 6.0f, 1.5f + 2.5f * en_stem_level, lit(RGB(120, 170, 110)));   /* a bud: the stem */
}

/* ------------------------------------------------------------------- the ivy --- */
#define WALL_TOP 30
static void bg_wall(void)
{
    int32_t r, x;
    uint16_t brick = lit(RGB(156, 72, 52)), brick2 = lit(RGB(136, 62, 46)), mortar = lit(RGB(190, 176, 156));
    cv_rect(0, WALL_TOP, 240, GROUND - WALL_TOP, mortar);
    cv_rect(0, WALL_TOP - 4, 240, 4, lit(RGB(170, 160, 150)));   /* the coping */
    for (r = 0; WALL_TOP + r * 8 < GROUND; r++)
        for (x = (r & 1) ? -8 : 0; x < 240; x += 17) {
            uint32_t h = hh(0xB41Cu + (uint32_t)r, (uint32_t)x);
            cv_rect(x + 1, WALL_TOP + r * 8 + 1, 15, 6, (h & 3u) ? brick : brick2);
        }
}
static void ground_path(void)
{
    cv_rect(0, GROUND, 240, MAIN_H - GROUND, lit(mix(RGB(150, 140, 120), RGB(90, 84, 72), q16(en_soil))));
    cv_rect(0, GROUND, 240, 1, lit(RGB(70, 60, 50)));
}
static void ivy_leaf(float x, float y, float s, uint16_t c, uint16_t cv)
{
    blob(x, y - s * 0.7f, s * 0.75f, c);
    blob(x - s * 0.7f, y + s * 0.1f, s * 0.65f, c);
    blob(x + s * 0.7f, y + s * 0.1f, s * 0.65f, c);
    blob(x, y + s * 0.3f, s * 0.7f, c);
    cv_rect(ri(x), ri(y - s * 0.5f), 1, ri(s), cv);
}
static void vine(int l, float x0, float x1, float top)
{
    float hyd = hyd_of(l), lx = x0, ly = (float)GROUND, k, len = (float)GROUND - top;
    uint16_t c = part(l, RGB(46, 110, 50), RGB(150, 130, 60)), cv = vein(l, RGB(30, 70, 30)), stem = lit(RGB(96, 80, 50));
    int i = 0;
    for (k = 3.0f; k <= len; k += 3.0f) {
        float u = k / len, x = x0 + (x1 - x0) * u + fm_sin_turns(u * 2.2f + (float)l * 0.3f) * 7.0f, y = (float)GROUND - k;
        seg(lx, ly, x, y, 1.0f, stem);
        if (((int32_t)k % 9) == 0) {             /* a leaf, alternate sides, fluttering in the wind */
            float side = (i++ & 1) ? 1.0f : -1.0f, fl = en_leaf_sway[l] * fm_sin_turns((float)sc_t * 0.08f + (float)i * 0.3f) * 2.0f;
            ivy_leaf(x + side * 5.0f + fl, y + fl * 0.5f, (2.6f + 1.6f * hyd) * (1.0f - 0.3f * u), c, cv);
        }
        lx = x;
        ly = y;
    }
    spark(lx, ly, l);
}
static void draw_ivy(void)
{
    vine(0, 66.0f, 92.0f, 46.0f);
    vine(1, 176.0f, 152.0f, 54.0f);
    vine(2, 120.0f, 126.0f, 28.0f);
    blob(120.0f, (float)GROUND - 3.0f, 2.0f + 2.0f * en_stem_level, lit(RGB(96, 80, 50)));   /* the root */
}

/* ---------------------------------------------------------------- the bamboo --- */
static void bg_mountains(void)
{
    int32_t x, layer;
    for (layer = 0; layer < 3; layer++)
        for (x = 0; x < 240; x++) {
            float p = (float)x / (60.0f - 14.0f * (float)layer) + (float)layer * 0.37f, f = p - fm_floorf(p);
            int32_t h = (int32_t)((f < 0.5f ? f : 1.0f - f) * (50.0f - 12.0f * (float)layer)) + 18 + layer * 4 - layer * 10;
            cv_rect(x, GROUND - 30 + layer * 8 - h, 1, h + 40, haze(lit(RGB(70, 90, 86)), 0.75f - 0.22f * (float)layer));
        }
    for (x = 0; x < 30; x++) {                    /* the far grove */
        uint32_t h = hh(0xBA4Bu, (uint32_t)x);
        cv_rect((int32_t)(h % 240u), 40 + (int32_t)(h >> 8 & 31u), 2, GROUND, haze(lit(RGB(70, 110, 70)), 0.45f));
    }
}
static void culm(int l, float x0, float h)
{
    const float g = (float)GROUND;
    float hyd = hyd_of(l), bend = en_leaf_sway[l] * 0.22f * h, lx = x0, ly = g, k;
    uint16_t c = part(l, RGB(110, 160, 70), RGB(196, 180, 90)), node = vein(l, RGB(70, 110, 50));
    int n = 0;
    for (k = 2.0f; k <= h; k += 2.0f) {
        float u = k / h, x = x0 + bend * u * u, y = g - k;
        seg(lx, ly, x, y, 5.0f, c);
        if ((int32_t)k % 14 == 0) {               /* a node; near the top, a tuft of leaves */
            cv_rect(ri(x) - 3, ri(y), 7, 2, node);
            if (u > 0.55f) {
                float j;
                for (j = -1.0f; j <= 1.0f; j += 1.0f) {
                    float a = (0.5f + 0.5f * (float)(n & 1)) + j * 0.12f + en_leaf_sway[l] * 0.06f, len = 10.0f + 4.0f * hyd;
                    float droop = (1.0f - hyd) * 4.0f;
                    seg(x, y, x + fm_cosf(a * FM_TWO_PI) * len * ((n & 1) ? -1.0f : 1.0f) * -1.0f, y + fm_sinf(a * FM_TWO_PI) * len * 0.4f + 3.0f + droop,
                        2.0f, mix(c, RGB(60, 120, 50), 5));
                }
                n++;
            }
        }
        lx = x;
        ly = y;
    }
    spark(lx, ly, l);
}
static void draw_bamboo(void)
{
    culm(0, 92.0f, 92.0f);
    culm(1, 150.0f, 84.0f);
    culm(2, 120.0f, 112.0f);
    blob(106.0f, (float)GROUND - 2.0f, 1.0f + 2.5f * en_stem_level, lit(RGB(120, 160, 80)));   /* a shoot: the stem */
}

/* ---------------------------------------------------------------- the scene --- */
static void sky(void)
{
    int32_t y, i;
    float sunv = fm_sin_turns(en_day - 0.25f);
    sc_dl = fm_clampf((sunv + 0.12f) * 1.6f, 0.0f, 1.0f);
    sc_top = sky_colour(en_day, 1);
    sc_hor = sky_colour(en_day, 0);
    for (y = 0; y < GROUND; y++)
        cv_rect(0, y, 240, 1, mix(sc_top, sc_hor, y * 16 / GROUND));
    if (sc_dl < 0.7f)                            /* stars */
        for (i = 0; i < 18; i++) {
            uint32_t h = hh(0x5EED0000u, (uint32_t)i);
            int tw = (int)((1.0f - sc_dl / 0.7f) * (1.0f - en_rain) * 12.0f) - (int)((h >> 20) & 3u) * ((sc_t + i) % 9 == 0);
            px_blend((int32_t)(h % 236u) + 2, (int32_t)((h >> 9) % 70u) + 2, RGB(220, 226, 236), (uint32_t)(tw < 0 ? 0 : tw));
        }
    {   /* the sun by day, the moon by night, each on its arc */
        int day = en_day >= 0.25f && en_day < 0.75f;
        float u = day ? (en_day - 0.25f) * 2.0f : (en_day >= 0.75f ? en_day - 0.75f : en_day + 0.25f) * 2.0f;
        float sx = 14.0f + 212.0f * u, sy = (float)GROUND - 6.0f - 96.0f * fm_sin_turns(u * 0.5f);
        if (day) {
            disc(sx, sy, 10.0f, mix(sc_hor, RGB(255, 206, 96), 5));
            disc(sx, sy, 7.0f, mix(RGB(255, 206, 96), RGB(150, 150, 150), q16(en_rain * 0.6f)));
        } else {
            disc(sx, sy, 6.0f, RGB(220, 226, 236));
            disc(sx + 2.5f, sy - 1.5f, 5.0f, mix(sc_top, sc_hor, (int32_t)sy * 16 / GROUND));
        }
    }
}
static void weather(void)
{
    int32_t i, n;
    uint16_t c = mix(sc_hor, RGB(240, 244, 250), 6);
    n = ri(en_wind * 8.0f);
    for (i = 0; i < n; i++) {                    /* wind: streaks drifting across */
        uint32_t h = hh(0x77100000u, (uint32_t)i);
        int32_t sp = 3 + (int32_t)(en_wind * 9.0f), wl = 6 + (int32_t)(en_wind * 16.0f);
        cv_rect((int32_t)((h % 300u + (uint32_t)(sc_t * sp)) % 300u) - 30, 8 + (int32_t)((h >> 8) % 100u), wl, 1, c);
    }
    n = ri(en_rain * 56.0f);
    c = mix(sc_hor, RGB(150, 190, 230), 11);
    for (i = 0; i < n; i++) {                    /* rain */
        uint32_t h = hh(0x4A1A0000u, (uint32_t)i);
        int32_t slant = (int32_t)(en_wind * 4.0f), sy = (int32_t)(((h >> 8) % (uint32_t)GROUND + (uint32_t)(sc_t * 9)) % (uint32_t)GROUND);
        int32_t sx = (int32_t)((h % 240u + (uint32_t)(sy * slant / 12)) % 240u);
        cv_line(sx, sy, sx + slant / 2, sy + 4, c);
    }
}

static void draw_scene(int s)
{
    sc_t = (int32_t)(ui.frame / 3u);
    sky();
    switch (s) {                                 /* the place behind */
    case EN_SP_FERN: bg_forest(0); break;
    case EN_SP_MOSS: bg_forest(1); break;
    case EN_SP_CACTUS: bg_desert(); break;
    case EN_SP_WILLOW: bg_lake(); break;
    case EN_SP_LOTUS: bg_hills(); break;
    case EN_SP_IVY: bg_wall(); break;
    case EN_SP_BAMBOO: bg_mountains(); break;
    default: break;
    }
    weather();
    switch (s) {                                 /* what stands in front of the weather, then the ground */
    case EN_SP_ORCHID: mask_window(); ground_sill(); break;
    case EN_SP_FERN: ground_loam(); break;
    case EN_SP_MOSS: ground_stream(); break;
    case EN_SP_CACTUS: ground_sand(); break;
    case EN_SP_WILLOW: ground_grass(); break;
    case EN_SP_LOTUS: bg_pond(); break;
    case EN_SP_IVY: ground_path(); break;
    default: ground_loam(); break;
    }
    switch (s) {                                 /* the plant */
    case EN_SP_FERN: draw_fern(); break;
    case EN_SP_ORCHID: draw_orchid(); break;
    case EN_SP_MOSS: draw_moss(); break;
    case EN_SP_CACTUS: draw_cactus(); break;
    case EN_SP_WILLOW: draw_willow(); break;
    case EN_SP_LOTUS: draw_lotus(); break;
    case EN_SP_IVY: draw_ivy(); break;
    case EN_SP_BAMBOO: draw_bamboo(); break;
    default: draw_fern(); break;
    }
}
