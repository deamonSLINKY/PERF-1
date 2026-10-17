/* SPDX-License-Identifier: GPL-3.0-only */
/* GRAV-1's secret: ASTEROIDS. Hold OCT- and OCT+ together for two seconds (again, or HOME, to leave).
 *
 * The ship sits where the star was; SELECT turns it, any key fires. The music sends the rocks: each note a
 * planet plays may throw one in from the dark, in that planet's colour and as big as the planet is loud.
 * Shoot one and it rings its planet's voice; the big ones split. Every Great Year is a new wave, faster and
 * thicker. Three ships; the best score lasts until the FM-1 is switched off. The system plays on throughout,
 * and the lights play along as in the orbit view.
 *
 * Included by ui.c: it draws with ui.c's helpers on the whole screen, two 240x120 halves a frame. */

#define G_NROCK 22
#define G_NSHOT 6
#define G_NSPARK 64
#define G_CX 120.0f
#define G_CY 124.0f

typedef struct {
    float x, y, vx, vy, r, rot, spin;
    uint32_t seed;
    uint8_t on, pl, size;
} g_rock_t;

static struct {
    uint8_t on, over, lives, wave, back_view;
    float ang, ang_shown;                        /* turns; 0 up */
    uint32_t score, best, last_ms, over_ms, hurt_until, banner_until, start_ms;
    uint32_t greats0;
    float glow_prev[EN_NPL], kick, flash;
    uint32_t strike_ms[EN_NPL];                  /* its own notes don't send rocks */
    g_rock_t rock[G_NROCK];
    struct { float x, y, vx, vy, life; uint8_t on; } shot[G_NSHOT];
    struct { float x, y, vx, vy, life; uint16_t c; } spark[G_NSPARK];
    uint32_t rng;
    uint32_t fired, spawned, hits;               /* for the tests */
    char banner[16];
} gm;

static uint32_t g_rnd(void)
{
    gm.rng ^= gm.rng << 13;
    gm.rng ^= gm.rng >> 17;
    gm.rng ^= gm.rng << 5;
    return gm.rng;
}
static float g_frand(void) { return (float)(g_rnd() >> 8) * (1.0f / 16777216.0f); }

static void g_banner(const char *s, uint32_t ms)
{
    int k = 0;
    while (s[k] && k < 15) {
        gm.banner[k] = s[k];
        k++;
    }
    gm.banner[k] = 0;
    gm.banner_until = plat_ms() + ms;
}

static void g_reset(void)
{
    memset(gm.rock, 0, sizeof gm.rock);
    memset(gm.shot, 0, sizeof gm.shot);
    memset(gm.spark, 0, sizeof gm.spark);
    gm.over = 0;
    gm.lives = 3;
    gm.wave = 1;
    gm.score = 0;
    gm.greats0 = en_greats;
    gm.hurt_until = 0;
    gm.flash = 0.0f;
    g_banner("WAVE 1", 1800u);
}

static void game_start(void)
{
    gm.on = 1;
    gm.back_view = ui.view;
    gm.rng ^= plat_ms() * 2654435761u | 1u;
    gm.ang = gm.ang_shown = 0.0f;
    gm.last_ms = gm.start_ms = plat_ms();
    g_reset();
    g_banner("ASTEROIDS", 2500u);
    ui.full = 0;
    lcd_fill(0, 0, 240, 240, K_BG);
}

static void game_stop(void)
{
    gm.on = 0;
    ui.view = gm.back_view;
    ui.touched = -1;
    ui.act_t = plat_ms();
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
    lcd_fill(0, 0, 240, 240, K_BG);
}

static void g_sparks(float x, float y, uint16_t c, int n, float speed)
{
    int k;
    for (k = 0; k < G_NSPARK && n > 0; k++)
        if (gm.spark[k].life <= 0.0f) {
            float a = g_frand(), v = speed * (0.3f + g_frand());
            gm.spark[k].x = x;
            gm.spark[k].y = y;
            gm.spark[k].vx = v * cos_t(a);
            gm.spark[k].vy = v * fm_sin_turns(a);
            gm.spark[k].life = 0.5f + 0.6f * g_frand();
            gm.spark[k].c = c;
            n--;
        }
}

static float g_rock_r(int size) { return size >= 3 ? 16.0f : size == 2 ? 10.5f : 6.5f; }

static g_rock_t *g_new_rock(void)
{
    int k;
    for (k = 0; k < G_NROCK; k++)
        if (!gm.rock[k].on)
            return &gm.rock[k];
    return 0;
}

/* a planet played: maybe a rock, from the dark beyond the screen's edge, aimed near the ship */
static void g_spawn(int pl)
{
    int live = 0, k, lvl = proj.par[P_PL_LVL(pl)];
    float a = g_frand(), aim, sp;
    g_rock_t *r;
    for (k = 0; k < G_NROCK; k++)
        live += gm.rock[k].on;
    if (live >= 5 + 2 * gm.wave || g_frand() > 0.45f + 0.08f * (float)gm.wave)
        return;
    if (!(r = g_new_rock()))
        return;
    r->on = 1;
    r->pl = (uint8_t)pl;
    r->size = (uint8_t)(lvl >= 70 ? 3 : lvl >= 45 ? 2 : 1);
    r->r = g_rock_r(r->size);
    r->x = G_CX + 178.0f * cos_t(a);
    r->y = G_CY + 178.0f * fm_sin_turns(a);
    aim = a + 0.5f + (g_frand() - 0.5f) * 0.12f;
    sp = (11.0f + 4.0f * (float)gm.wave + 9.0f * g_frand()) * fm_clampf(en_bpm / 52.0f, 0.7f, 1.5f);
    r->vx = sp * cos_t(aim);
    r->vy = sp * fm_sin_turns(aim);
    r->rot = g_frand();
    r->spin = (g_frand() - 0.5f) * 0.5f;
    r->seed = g_rnd();
    gm.spawned++;
}

static void g_break(g_rock_t *r, int scored)
{
    uint16_t c = heat(en_pl_dist[r->pl]);
    g_sparks(r->x, r->y, c, 6 + 4 * r->size, 40.0f + 10.0f * (float)r->size);
    if (scored) {
        static const uint16_t PTS[4] = {0, 100, 50, 20};
        gm.score += PTS[r->size] * gm.wave;
        gm.hits++;
        en_strike(r->pl);                        /* the rock rings its planet */
        gm.strike_ms[r->pl] = plat_ms();
    }
    if (scored && r->size > 1) {                 /* the big ones split, across their path */
        int k;
        float v = fm_sqrtf(r->vx * r->vx + r->vy * r->vy), m = (v * 1.35f + 6.0f) / (v > 0.1f ? v : 0.1f);
        const float c = cos_t(0.17f), s = fm_sin_turns(0.17f);
        for (k = 0; k < 2; k++) {
            g_rock_t *n = g_new_rock();
            float sn = k ? s : -s;
            if (!n)
                break;
            *n = *r;
            n->size = (uint8_t)(r->size - 1);
            n->r = g_rock_r(n->size);
            n->vx = (r->vx * c - r->vy * sn) * m;
            n->vy = (r->vx * sn + r->vy * c) * m;
            n->spin = -r->spin * 1.5f;
            n->seed = g_rnd();
        }
    }
    r->on = 0;
}

static void game_fire(void)
{
    int k;
    if (gm.over) {                               /* again, after a moment */
        if (plat_ms() - gm.over_ms > 900u)
            g_reset();
        return;
    }
    for (k = 0; k < G_NSHOT; k++)
        if (!gm.shot[k].on) {
            float c = cos_t(gm.ang_shown - 0.25f), s = fm_sin_turns(gm.ang_shown - 0.25f);
            gm.shot[k].on = 1;
            gm.shot[k].x = G_CX + c * 11.0f;
            gm.shot[k].y = G_CY + s * 11.0f;
            gm.shot[k].vx = c * 190.0f;
            gm.shot[k].vy = s * 190.0f;
            gm.shot[k].life = 0.75f;
            gm.fired++;
            return;
        }
}

static void game_update(void)
{
    uint32_t now = plat_ms();
    float dt = (float)(now - gm.last_ms) * 0.001f;
    int i, k;
    gm.last_ms = now;
    if (dt > 0.1f)
        dt = 0.1f;
    {   /* the ship turns smoothly to where SELECT points it */
        float d = gm.ang - gm.ang_shown;
        gm.ang_shown += d * fm_minf(dt * 18.0f, 1.0f);
    }
    if (!gm.over && en_greats - gm.greats0 + 1u != gm.wave) {    /* a Great Year: the next wave */
        char t[12] = "WAVE ";
        gm.wave = (uint8_t)(en_greats - gm.greats0 + 1u > 9u ? 9u : en_greats - gm.greats0 + 1u);
        t[5] = (char)('0' + gm.wave);
        t[6] = 0;
        g_banner(t, 1800u);
        if (gm.lives < 3)
            gm.lives++;                          /* and a ship back */
    }
    for (i = 0; i < nplanets(); i++) {           /* the music sends the rocks */
        float g = en_pl_glow[i];
        if (!gm.over && g > 0.85f && gm.glow_prev[i] < 0.6f && now - gm.strike_ms[i] > 400u)
            g_spawn(i);
        gm.glow_prev[i] = g;
    }
    if (en_drum_glow[0] > 0.8f)
        gm.kick = 1.0f;
    gm.kick *= 1.0f - fm_minf(dt * 6.0f, 1.0f);
    gm.flash *= 1.0f - fm_minf(dt * 3.0f, 1.0f);
    for (k = 0; k < G_NROCK; k++) {
        g_rock_t *r = &gm.rock[k];
        float dx, dy;
        if (!r->on)
            continue;
        r->x += r->vx * dt;
        r->y += r->vy * dt;
        r->rot += r->spin * dt;
        dx = r->x - G_CX;
        dy = r->y - G_CY;
        if (dx * dx + dy * dy > 200.0f * 200.0f && dx * r->vx + dy * r->vy > 0.0f) {
            r->on = 0;                           /* gone by, out into the dark */
            continue;
        }
        if (!gm.over && now >= gm.hurt_until && dx * dx + dy * dy < (r->r + 6.0f) * (r->r + 6.0f)) {
            g_break(r, 0);                       /* it hits the ship */
            g_sparks(G_CX, G_CY, K_TEXT, 18, 60.0f);
            gm.flash = 1.0f;
            gm.hurt_until = now + 2200u;
            if (gm.lives)
                gm.lives--;
            if (!gm.lives) {
                gm.over = 1;
                gm.over_ms = now;
                if (gm.score > gm.best)
                    gm.best = gm.score;
            }
        }
    }
    for (k = 0; k < G_NSHOT; k++) {
        int j;
        if (!gm.shot[k].on)
            continue;
        gm.shot[k].x += gm.shot[k].vx * dt;
        gm.shot[k].y += gm.shot[k].vy * dt;
        gm.shot[k].life -= dt;
        if (gm.shot[k].life <= 0.0f) {
            gm.shot[k].on = 0;
            continue;
        }
        for (j = 0; j < G_NROCK; j++) {
            g_rock_t *r = &gm.rock[j];
            float dx = gm.shot[k].x - r->x, dy = gm.shot[k].y - r->y;
            if (r->on && dx * dx + dy * dy < (r->r + 1.5f) * (r->r + 1.5f)) {
                gm.shot[k].on = 0;
                g_break(r, 1);
                break;
            }
        }
    }
    for (k = 0; k < G_NSPARK; k++)
        if (gm.spark[k].life > 0.0f) {
            gm.spark[k].x += gm.spark[k].vx * dt;
            gm.spark[k].y += gm.spark[k].vy * dt;
            gm.spark[k].life -= dt;
        }
}

/* ------------------------------------------------------------ drawing --- */
/* a line, its pixels shared across the two nearest rows (or columns) */
static void g_seg(float x0, float y0, float x1, float y1, uint16_t c, float s)
{
    float dx = x1 - x0, dy = y1 - y0, ax = fm_fabsf(dx), ay = fm_fabsf(dy), t, step;
    int n, k;
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

static void g_rock_draw(const g_rock_t *r)
{
    float px[9], py[9];
    uint16_t c = heat(en_pl_dist[r->pl]);
    uint32_t s = r->seed;
    int k;
    for (k = 0; k < 9; k++) {                    /* a lumpy outline, its own for each rock */
        float a = r->rot + (float)k / 9.0f, rr;
        s = s * 1664525u + 1013904223u;
        rr = r->r * (0.72f + 0.3f * (float)(s >> 24) / 255.0f);
        px[k] = r->x + rr * cos_t(a);
        py[k] = r->y + rr * fm_sin_turns(a);
    }
    glow(r->x, r->y, r->r * 0.3f, r->r * 1.25f, c, 0.12f);
    for (k = 0; k < 9; k++)
        g_seg(px[k], py[k], px[(k + 1) % 9], py[(k + 1) % 9], c, 1.0f);
}

static void g_ship_draw(float x, float y, float ang, float scale, uint16_t c, float s)
{
    float a = ang - 0.25f;
    float nx = x + 11.0f * scale * cos_t(a), ny = y + 11.0f * scale * fm_sin_turns(a);
    float lx = x + 8.0f * scale * cos_t(a + 0.38f), ly = y + 8.0f * scale * fm_sin_turns(a + 0.38f);
    float rx = x + 8.0f * scale * cos_t(a - 0.38f), ry = y + 8.0f * scale * fm_sin_turns(a - 0.38f);
    float bx = x + 3.5f * scale * cos_t(a + 0.5f), by = y + 3.5f * scale * fm_sin_turns(a + 0.5f);
    g_seg(nx, ny, lx, ly, c, s);
    g_seg(nx, ny, rx, ry, c, s);
    g_seg(lx, ly, bx, by, c, s);
    g_seg(rx, ry, bx, by, c, s);
}

static void game_draw(int half)
{
    uint32_t now = plat_ms();
    const float sky = (float)proj.par[P_SKY] * 0.01f;
    uint16_t star = STAR_C[(unsigned)proj.par[P_COLOUR] < EN_NCOLOUR ? proj.par[P_COLOUR] : 0];
    int k;
    cv_begin(240, 120, K_BG);
    cv_oy = half ? -120 : 0;
    cv_y0 = 0;
    cv_y1 = 120;
    if (sky > 0.0f)
        for (k = 0; k < NSKY; k++)
            px_blend(sky_x[k], (int32_t)((float)sky_y[k] * 240.0f / (float)MAIN_H), K_SKY, (uint32_t)((float)sky_b[k] * sky + 0.5f));
    glow(G_CX, G_CY, 2.0f, 30.0f + 8.0f * en_star, star, 0.18f + 0.1f * en_flare);   /* the star, under the ship */
    for (k = 0; k < G_NROCK; k++)
        if (gm.rock[k].on)
            g_rock_draw(&gm.rock[k]);
    for (k = 0; k < G_NSPARK; k++)
        if (gm.spark[k].life > 0.0f)
            px_blend((int32_t)gm.spark[k].x, (int32_t)gm.spark[k].y, gm.spark[k].c, (uint32_t)q16(gm.spark[k].life * 1.6f));
    for (k = 0; k < G_NSHOT; k++)
        if (gm.shot[k].on) {
            float tx = gm.shot[k].x - gm.shot[k].vx * 0.03f, ty = gm.shot[k].y - gm.shot[k].vy * 0.03f;
            g_seg(tx, ty, gm.shot[k].x, gm.shot[k].y, K_ACC, 0.6f);
            disc(gm.shot[k].x, gm.shot[k].y, 1.4f, K_TEXT);
        }
    if (!gm.over && (now >= gm.hurt_until || (now / 120u) % 2u)) {
        float a = gm.ang_shown + 0.25f;          /* the flame, with the kick */
        if (gm.kick > 0.05f)
            g_seg(G_CX + 4.0f * cos_t(a), G_CY + 4.0f * fm_sin_turns(a),
                  G_CX + (5.0f + 7.0f * gm.kick) * cos_t(a), G_CY + (5.0f + 7.0f * gm.kick) * fm_sin_turns(a), K_ACC,
                  gm.kick);
        g_ship_draw(G_CX, G_CY, gm.ang_shown, 1.0f, K_TEXT, 1.0f);
    }
    if (gm.flash > 0.02f)
        ring_px(G_CX, G_CY, 8.0f + (1.0f - gm.flash) * 40.0f, RGB(255, 74, 46), gm.flash);
    {   /* the score, the wave, the ships left */
        char t[24] = "SCORE ";
        itoa_u(gm.score, t + 6);
        if (!gm.over)
            cv_text(8, 4, &FONT_S, t, K_TEXT);
        for (k = 0; k < gm.lives; k++)
            g_ship_draw(226.0f - (float)k * 14.0f, 14.0f, 0.0f, 0.6f, K_ACC, 1.0f);
    }
    if (!en_playing && !gm.over)
        text_c(120, 214, &FONT_S, "THE SYSTEM RESTS: PLAY", K_DIM);
    else if (now - gm.start_ms < 4000u)
        text_c(120, 214, &FONT_S, "SELECT TURNS  KEYS FIRE", K_DIM);
    if (gm.over) {
        char t[32] = "SCORE ", *q;
        text_c(120, 84, &FONT_B, "GAME OVER", K_ACC);
        itoa_u(gm.score, t + 6);
        for (q = t; *q; q++)
            ;
        q[0] = ' ', q[1] = ' ', q[2] = 'B', q[3] = 'E', q[4] = 'S', q[5] = 'T', q[6] = ' ';
        itoa_u(gm.best, q + 7);
        text_c(120, 150, &FONT_S, t, K_TEXT);
        text_c(120, 176, &FONT_S, "A KEY: AGAIN   HOME: BACK", K_DIM);
    } else if (now < gm.banner_until) {
        text_c(120, 84, &FONT_B, gm.banner, K_ACC);
    }
}

/* ------------------------------------------------------------ input --- */
/* with the game on, input() hands everything here: SELECT turns, keys fire, HOME (or the secret again)
 * leaves, PLAY still grows and rests the system */
static void game_input(uint32_t btn, uint32_t keys)
{
    uint32_t press = btn & ~ui.btn, i;
    const uint32_t both = 1u << B_OCTDN | 1u << B_OCTUP;
    int32_t e;
    int r;
    for (i = 0; i < NB; i++)
        if (press >> i & 1u)
            ui.down_t[i] = plat_ms();
    if (keys & ~ui.keys)
        game_fire();
    if ((e = plat_enc(EN_SELECT)) != 0)
        gm.ang += (float)e / 32.0f;
    for (r = 0; r < NE; r++)                     /* the other knobs do nothing here */
        if (r != EN_SELECT)
            (void)plat_enc(r);
    if (btn || keys)
        ui.act_t = plat_ms();
    if (press >> B_PLAY & 1u) {
        if (en_playing) {
            en_play(0);
        } else {
            en_seed(plat_ms());
            en_play(1);
        }
    }
    ui.keys = keys;
    if ((press >> B_HOME & 1u) ||
        ((btn & both) == both && !(ui.btn_used & both) && plat_ms() - ui.down_t[B_OCTDN] >= 1500u &&
         plat_ms() - ui.down_t[B_OCTUP] >= 1500u)) {
        ui.btn_used |= btn;                      /* their release does nothing */
        ui.btn = btn;
        game_stop();
        midi_in();
        return;
    }
    if ((btn & both) != both)
        ui.btn_used &= ~both;
    ui.btn = btn;
    midi_in();
}
