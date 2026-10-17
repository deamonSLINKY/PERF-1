/* SPDX-License-Identifier: GPL-3.0-only */
/* ECO-1's secret: FIREFLIES. Half an hour with nobody touching it (ui_flies_ms) and the screen's words go:
 * the header and the knobs slip away, the plant's place fills the whole screen (its sky drawn on up, its
 * ground on down), and the fireflies come out, one by one. Each belongs to a leaf and flashes when its
 * leaf fires; between, they drift and glow on their own slow breath, faint by day and bright at night.
 * The panel's lights play along: a leaf's button and its note's key as it fires. Any button, key or knob
 * brings the screen back and does nothing else; the plant never stops.
 *
 * Included by ui.c, after scene.c: it draws the scene into the band, then the strips above and below. */

#define FF_N 16
#define FF_Y 58                                  /* the scene's band, lowered: more sky above, less ground */
uint32_t ui_flies_ms = 30u * 60000u;           /* the fireflies after this long untouched (0: never) */

static struct {
    uint8_t on, shown;                           /* the fireflies up; how many have come out */
    uint32_t start_ms, last_ms, rng;
    struct { float x, y, vx, vy, ph, rate; uint8_t leaf; } f[FF_N];
    uint16_t sky_row[240], ground_row[240];      /* the scene's top and bottom rows, drawn on up and down */
} ff;

static uint32_t ff_rnd(void)
{
    ff.rng ^= ff.rng << 13;
    ff.rng ^= ff.rng >> 17;
    ff.rng ^= ff.rng << 5;
    return ff.rng;
}
static float ff_frand(void) { return (float)(ff_rnd() >> 8) * (1.0f / 16777216.0f); }

static void ff_enter(void)
{
    int k;
    ff.on = 1;
    ff.shown = 0;
    ff.start_ms = ff.last_ms = plat_ms();
    ff.rng ^= plat_ms() * 2654435761u | 1u;
    for (k = 0; k < FF_N; k++) {                 /* they rise from the ground and the plant */
        ff.f[k].x = 30.0f + 180.0f * ff_frand();
        ff.f[k].y = 150.0f + 70.0f * ff_frand();
        ff.f[k].vx = ff.f[k].vy = 0.0f;
        ff.f[k].ph = ff_frand();
        ff.f[k].rate = 0.12f + 0.3f * ff_frand();
        ff.f[k].leaf = (uint8_t)(k % EN_NLEAF);
    }
    ui.help = 0;
}

static void ff_leave(void)
{
    ff.on = 0;
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;       /* everything drawn again */
    lcd_fill(0, 0, 240, 240, K_BG);
}

static void ff_update(void)
{
    uint32_t now = plat_ms();
    float dt = (float)(now - ff.last_ms) * 0.001f;
    int k;
    ff.last_ms = now;
    if (dt > 0.1f)
        dt = 0.1f;
    if (ff.shown < FF_N && now - ff.start_ms >= (uint32_t)ff.shown * 1800u)
        ff.shown++;                              /* one by one, a couple of seconds apart */
    for (k = 0; k < ff.shown; k++) {
        float sp;
        ff.f[k].vx += ((ff_frand() - 0.5f) * 60.0f + (120.0f - ff.f[k].x) * 0.05f) * dt;
        ff.f[k].vy += ((ff_frand() - 0.5f) * 60.0f + (140.0f - ff.f[k].y) * 0.05f - 1.5f) * dt;   /* drifting up */
        sp = fm_sqrtf(ff.f[k].vx * ff.f[k].vx + ff.f[k].vy * ff.f[k].vy);
        if (sp > 14.0f) {
            ff.f[k].vx *= 14.0f / sp;
            ff.f[k].vy *= 14.0f / sp;
        }
        ff.f[k].x = fm_clampf(ff.f[k].x + ff.f[k].vx * dt, 6.0f, 234.0f);
        ff.f[k].y = fm_clampf(ff.f[k].y + ff.f[k].vy * dt, 8.0f, 230.0f);
        ff.f[k].ph += ff.f[k].rate * dt;
        if (ff.f[k].ph >= 1.0f)
            ff.f[k].ph -= 1.0f;
    }
}

/* how bright one is now: its own slow breath, or its leaf firing */
static float ff_bright(int k)
{
    float b = 0.5f + 0.5f * fm_sin_turns(ff.f[k].ph);
    b *= b;
    b *= b;
    return fm_maxf(b * 0.8f, en_leaf_glow[ff.f[k].leaf]);
}

static void ff_draw_flies(void)
{
    float night = fm_clampf(1.15f - en_sun * 1.4f, 0.45f, 1.0f);
    int k;
    for (k = 0; k < ff.shown; k++) {
        float b = ff_bright(k), x = ff.f[k].x, y = ff.f[k].y;
        int32_t gx0, gx1, gy0, gy1, px, py;
        float r1 = 2.5f + 5.5f * b;
        gx0 = (int32_t)(x - r1);
        gx1 = (int32_t)(x + r1 + 1.0f);
        gy0 = (int32_t)(y - r1);
        gy1 = (int32_t)(y + r1 + 1.0f);
        for (py = gy0; py <= gy1; py++)          /* the glow round it */
            for (px = gx0; px <= gx1; px++) {
                float dx = (float)px + 0.5f - x, dy = (float)py + 0.5f - y, d = fm_sqrtf(dx * dx + dy * dy), a;
                if (d >= r1)
                    continue;
                a = 1.0f - d / r1;
                px_blend(px, py, RGB(190, 255, 90), (uint32_t)(a * a * b * night * 14.0f + 0.5f));
            }
        if (b > 0.2f)
            disc(x, y, 0.6f + 0.8f * b, mix(RGB(150, 200, 60), RGB(250, 255, 200), q16(b)));
    }
}

static void ff_draw(void)
{
    const int32_t below = 240 - FF_Y - MAIN_H;
    int32_t y, x;
    uint32_t i;
    cv_begin(240, MAIN_H, K_BG);                 /* the band: the scene, as on the Plant page but with no words */
    draw_scene(proj.par[P_SPECIES]);
    for (i = 0; i < 240u; i++) {
        ff.sky_row[i] = swap16(cv_px[i]);
        ff.ground_row[i] = swap16(cv_px[(MAIN_H - 1u) * 240u + i]);
    }
    cv_oy = -FF_Y;
    ff_draw_flies();
    cv_blit(0, FF_Y);
    cv_begin(240, FF_Y, K_BG);                   /* above: the sky, on up, deepening */
    for (y = 0; y < FF_Y; y++)
        for (x = 0; x < 240; x++)
            cv_px[y * 240 + x] = swap16(mix(ff.sky_row[x], RGB(4, 6, 16), q16((float)(FF_Y - y) / (float)FF_Y * 0.55f)));
    ff_draw_flies();
    cv_blit(0, 0);
    cv_begin(240, (uint32_t)below, K_BG);        /* below: the ground, on down, into the dark */
    for (y = 0; y < below; y++)
        for (x = 0; x < 240; x++)
            cv_px[y * 240 + x] = swap16(mix(ff.ground_row[x], RGB(6, 5, 4), q16((float)y / (float)below * 0.8f)));
    cv_oy = -(FF_Y + MAIN_H);
    ff_draw_flies();
    cv_blit(0, (uint32_t)(FF_Y + MAIN_H));
}

/* the lights: PLAY; each leaf's button and its note's key as it fires */
static void ff_leds(void)
{
    static const uint8_t LEAF_B[EN_NLEAF] = {B_ENV, B_ARP, B_LFO};
    uint32_t b = 0, k = 0, i;
    if (en_playing)
        b |= 1u << B_PLAY;
    for (i = 0; i < EN_NLEAF; i++)
        if (en_leaf_glow[i] > 0.45f) {
            int n = en_leaf_note[i];
            b |= 1u << LEAF_B[i];
            while (n < 53)
                n += 12;
            while (n > 79)
                n -= 12;
            if (proj.leds)
                k |= 1u << (n - 53);             /* key 0 is F3 */
        }
    plat_glow(0);
    plat_leds(b, k);
}

/* the fireflies up: a touch only wakes the screen (the button's release does nothing either) */
static int ff_input(uint32_t btn, uint32_t keys)
{
    int any = btn != ui.btn || keys != ui.keys, r;
    for (r = 0; r < NE; r++)
        any |= plat_enc(r) != 0;
    if (!any)
        return 0;
    ui.btn_used |= btn;
    ui.btn = btn;
    ui.keys = keys;
    ui.act_t = plat_ms();
    ff_leave();
    return 1;
}
