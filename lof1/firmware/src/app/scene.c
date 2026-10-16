/* SPDX-License-Identifier: GPL-3.0-only */
/* LOF-1's scene (included by ui.c): what the music is playing from, at the left of the main band.
 *   VINYL     a record turning (33 1/3), grooves, the tape's colour on its label; scratches as it ages
 *   CASSETTE  a cassette, its two reels turning, the tape between them
 *   REEL      two big reels on a deck
 *   RADIO     a radio: its grille, its dial, the needle hunting for the station
 * The scene is 120 px wide, the band's height (144). */

static void s_line(int32_t cx, int32_t y, const char *s) { text_c(cx, y, &FONT_XS, s, K_DIM); }

static void ring_px(float cx, float cy, float r_in, float r_out, uint16_t c, uint32_t a16)   /* an anti-aliased ring */
{
    int32_t x0 = (int32_t)fm_floorf(cx - r_out - 1.0f), x1 = (int32_t)(cx + r_out + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r_out - 1.0f), y1 = (int32_t)(cy + r_out + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            float a = fm_clampf(r_out - d + 0.5f, 0.0f, 1.0f) * fm_clampf(d - r_in + 0.5f, 0.0f, 1.0f);
            if (a > 0.0f)
                px_blend(x, y, c, (uint32_t)(a * (float)a16 + 0.5f));
        }
}

static void spoke(float cx, float cy, float r0, float r1, float turn_, uint16_t c)
{
    float s = fm_sin_turns(turn_), co = fm_sin_turns(turn_ + 0.25f);
    cv_line((int32_t)(cx + co * r0), (int32_t)(cy + s * r0), (int32_t)(cx + co * r1), (int32_t)(cy + s * r1), c);
}

static void draw_vinyl(uint16_t label)
{
    const float cx = 60.0f, cy = 72.0f, R = 58.0f;
    int k;
    float w = en_wear;
    disc(cx, cy, R, K_VINYL);
    for (k = 0; k < 9; k++)                     /* the grooves */
        ring_px(cx, cy, 24.0f + (float)k * 3.8f, 24.6f + (float)k * 3.8f, K_GROOVE, 9u);
    ring_px(cx, cy, R - 2.5f, R - 1.6f, K_GROOVE, 12u);
    {   /* the light on it: two soft wedges that stay put while the record turns under them */
        int32_t x, y;
        for (y = -56; y <= 56; y++)
            for (x = -56; x <= 56; x++) {
                float d = fm_sqrtf((float)(x * x + y * y)), t;
                if (d < 22.0f || d > R - 2.0f)
                    continue;
                t = turns((float)y, (float)x);
                t = fm_fabsf(t - 0.125f) < 0.04f || fm_fabsf(t - 0.625f) < 0.04f ? 1.0f : 0.0f;
                if (t > 0.0f)
                    px_blend((int32_t)cx + x, (int32_t)cy + y, RGB(90, 84, 104), 4u);
            }
    }
    disc(cx, cy, 20.0f, label);                 /* the label */
    ring_px(cx, cy, 13.0f, 13.8f, mix(label, K_VINYL, 5), 12u);
    {   /* a mark on it, turning */
        float a = en_spin, s = fm_sin_turns(a), c = fm_sin_turns(a + 0.25f);
        disc(cx + c * 16.0f, cy + s * 16.0f, 2.2f, mix(label, K_TEXT, 10));
        disc(cx - c * 8.0f, cy - s * 8.0f, 1.4f, mix(label, K_VINYL, 9));
    }
    disc(cx, cy, 2.3f, K_BG);                   /* the spindle hole */
    for (k = 0; k < (int)(w * 7.0f); k++) {     /* scratches, as it ages */
        static const int8_t SC[7][4] = {{-40, -20, -18, -44}, {22, 30, 46, 14}, {-30, 34, -6, 50}, {34, -30, 48, -6},
                                        {-50, 6, -36, 30}, {6, -52, 30, -40}, {-20, 44, 10, 52}};
        cv_line((int32_t)cx + SC[k][0], (int32_t)cy + SC[k][1], (int32_t)cx + SC[k][2], (int32_t)cy + SC[k][3],
                RGB(70, 66, 80));
    }
    {   /* the tonearm, resting on the record while it plays */
        int32_t ax = 112, ay = 10, tx = en_playing ? 84 : 112, ty = en_playing ? 96 : 60;
        disc((float)ax, (float)ay, 4.0f, K_DIM);
        cv_line(ax, ay, tx, ty, RGB(200, 192, 182));
        cv_line(ax + 1, ay, tx + 1, ty, RGB(200, 192, 182));
        cv_round(tx - 3, ty - 2, 7, 9, 2, K_DIM);
    }
}

static void draw_cassette(uint16_t label)
{
    const int32_t x = 4, y = 30, w = 114, h = 76;
    float a = en_spin * 2.0f;
    int k;
    cv_round(x, y, w, h, 6, RGB(56, 50, 64));   /* the shell */
    cv_round(x + 6, y + 6, w - 12, 46, 4, label);   /* the label */
    cv_rect(x + 6, y + 14, w - 12, 2, mix(label, K_BG, 6));
    cv_round(x + 22, y + 22, w - 44, 22, 8, RGB(30, 26, 36));   /* the window */
    for (k = 0; k < 2; k++) {                   /* the reels, turning */
        float rx = (float)(x + 36 + k * 42), ry = (float)(y + 33);
        disc(rx, ry, 8.5f, RGB(214, 206, 196));
        disc(rx, ry, 3.2f, RGB(30, 26, 36));
        spoke(rx, ry, 3.5f, 8.0f, a, RGB(120, 112, 126));
        spoke(rx, ry, 3.5f, 8.0f, a + 0.3333f, RGB(120, 112, 126));
        spoke(rx, ry, 3.5f, 8.0f, a + 0.6667f, RGB(120, 112, 126));
    }
    cv_rect(x + 36, y + 41, 42, 2, RGB(90, 60, 50));   /* the tape */
    cv_round(x + 28, y + h - 18, w - 56, 14, 3, RGB(44, 38, 52));   /* the bottom, its holes */
    for (k = 0; k < 4; k++)
        disc((float)(x + 38 + k * 12), (float)(y + h - 11), 2.0f, RGB(24, 20, 28));
    s_line(60, y + h + 10, "SIDE A");
}

static void draw_reel(uint16_t label)
{
    float a = en_spin * 0.7f;
    int k, i;
    cv_round(2, 8, 116, 128, 6, RGB(40, 36, 48));   /* the deck */
    for (k = 0; k < 2; k++) {
        float rx = 34.0f + 52.0f * (float)k, ry = 46.0f;
        disc(rx, ry, 25.0f, RGB(176, 170, 164));
        disc(rx, ry, 17.0f + 4.0f * (float)k, RGB(70, 50, 44));   /* the tape wound on it */
        for (i = 0; i < 3; i++) {               /* three cut-outs, turning */
            float t = a * (k ? -1.0f : 1.0f) + (float)i / 3.0f, s = fm_sin_turns(t), c = fm_sin_turns(t + 0.25f);
            disc(rx + c * 11.0f, ry + s * 11.0f, 4.5f, RGB(40, 36, 48));
        }
        disc(rx, ry, 3.0f, K_DIM);
    }
    cv_rect(34, 72, 52, 2, RGB(70, 50, 44));    /* the tape across the heads */
    cv_round(48, 76, 24, 10, 2, label);         /* the heads */
    for (i = 0; i < 4; i++)                     /* the deck's buttons */
        cv_round(14 + i * 24, 104, 18, 12, 3, i == 1 && en_playing ? label : RGB(70, 64, 80));
}

static void draw_radio(uint16_t label)
{
    int32_t x = 4, y = 18, w = 114, h = 108, i, j;
    float wob = fm_sin_turns(en_spin * 0.37f) * 3.0f + fm_sin_turns(en_spin * 1.9f) * 1.0f;
    cv_round(x, y, w, h, 8, RGB(120, 72, 52));  /* the wooden box */
    cv_round(x + 6, y + 6, w - 12, 52, 5, RGB(48, 34, 30));   /* the grille */
    for (j = 0; j < 7; j++)
        for (i = 0; i < 14; i++)
            cv_rect(x + 12 + i * 7, y + 11 + j * 7, 3, 3, RGB(86, 62, 50));
    cv_round(x + 8, y + 64, w - 16, 22, 4, RGB(236, 220, 180));   /* the dial */
    for (i = 0; i < 12; i++)
        cv_rect(x + 14 + i * 8, y + 66 + (i & 1) * 4, 1, 8 - (i & 1) * 4, RGB(120, 100, 80));
    cv_rect(x + 58 + (int32_t)wob, y + 64, 2, 22, label);   /* the needle, hunting */
    disc((float)(x + 22), (float)(y + 97), 6.0f, RGB(60, 40, 34));   /* the knobs */
    disc((float)(x + w - 22), (float)(y + 97), 6.0f, RGB(60, 40, 34));
    disc((float)(x + w / 2), (float)(y + 97), 2.5f, en_playing ? K_ACC : RGB(60, 40, 34));   /* the lamp */
}

static void draw_scene(int ch, uint16_t label)
{
    switch (ch) {
    case DUST_CASSETTE: draw_cassette(label); break;
    case DUST_REEL: draw_reel(label); break;
    case DUST_RADIO: draw_radio(label); break;
    default: draw_vinyl(label); break;
    }
}
