/* SPDX-License-Identifier: GPL-3.0-only */
/* LOF-1's CRT (included by ui.c): everything the UI shows goes out through here, as if on an old TV.
 *   the curve    a slight barrel warp, the picture bulging out of a rounded tube: its corners and edges
 *                fall away into black
 *   scanlines    every other row a little darker
 *   vignette     darker towards the edges and corners
 *   the roll     a soft bright band drifting slowly down the screen, round and round (crt_roll_to)
 * There is no frame buffer (no RAM for one): each strip is warped from its own pixels on its way out, in
 * slices of CRT_ROWS rows, two slice buffers taking turns with the panel's DMA. Near a strip's top and
 * bottom the warp may want rows from the strip beside it: it takes the strip's own edge row instead (the
 * UI's bands meet on their background, so it doesn't show). The screensaver gives it rows to spare. */

#define CRT_K 0.03f                    /* the curve: ~3.6 px in at the middle of each edge, ~7 at the corners */
#define CRT_ROWS 8
#define CRT_ROLL_H 26                  /* the roll's half height, rows */
static uint16_t crt_buf[2][240 * CRT_ROWS] __attribute__((section(".pool")));
static uint8_t crt_k;                  /* the slice buffer to fill next */
static uint8_t crt_ready;
static int32_t crt_kc[240];            /* CRT_K * d^2, Q16, d the distance from the centre (-1 .. 1) */
static int16_t crt_dh[240];            /* the distance from the centre in half pixels: 2i - 239 */
static uint16_t crt_vig[240];          /* the vignette across one axis, Q8 */
static uint8_t crt_rl[CRT_ROLL_H];     /* the roll's brightening, Q8 over 1, by distance from its centre */
static int32_t crt_roll = -100;        /* the roll's centre row (it starts off screen) */

static void crt_init(void)
{
    int i;
    for (i = 0; i < 240; i++) {
        float d = (float)(2 * i - 239) / 240.0f, t = ((float)i + 0.5f) / 240.0f, g;
        crt_dh[i] = (int16_t)(2 * i - 239);
        crt_kc[i] = (int32_t)(CRT_K * d * d * 65536.0f);
        g = fm_sqrtf(fm_sqrtf(4.0f * t * (1.0f - t)));          /* 1 in the middle, ~0.5 at the edge */
        crt_vig[i] = (uint16_t)(256.0f * (1.0f - 0.32f * (1.0f - g)));
    }
    for (i = 0; i < CRT_ROLL_H; i++) {
        float c = fm_sin_turns(0.25f + 0.5f * (float)i / (float)CRT_ROLL_H);   /* cos, 1 .. 0 */
        crt_rl[i] = (uint8_t)(40.0f * (0.5f + 0.5f * c) + 0.5f);              /* up to ~16% brighter */
    }
    crt_ready = 1;
}

/* a row's brightness, Q8: its vignette, its scanline, the roll */
static uint32_t crt_row_gain(int32_t y)
{
    uint32_t g = crt_vig[y] * ((y & 1) ? 212u : 256u) >> 8;
    int32_t d = y - crt_roll;
    if (d < 0)
        d = -d;
    if (d < CRT_ROLL_H)
        g = g * (256u + crt_rl[d]) >> 8;
    return g;
}

/* screen rows y0 .. y0+h-1, columns x0 .. x0+w-1, from a source w wide holding screen rows sy0 .. sy0+sh-1
 * (src 0: every source pixel is fc) */
static void crt_out(uint32_t x0, uint32_t w, uint32_t y0, uint32_t h, const uint16_t *src, uint32_t sy0,
                    uint32_t sh, uint16_t fc)
{
    uint32_t r, n, x, y;
    if (!crt_ready)
        crt_init();
    if (!w || !h)
        return;
    for (r = 0; r < h; r += n) {
        uint16_t *o = crt_buf[crt_k];
        n = h - r < CRT_ROWS ? h - r : CRT_ROWS;   /* this buffer's last transfer, two slices ago, is done:
                                                     * the next slice's window waited for it */
        for (y = y0 + r; y < y0 + r + n; y++) {
            int32_t kr = crt_kc[y], dyh = crt_dh[y];
            uint32_t gy = crt_row_gain((int32_t)y);
            for (x = x0; x < x0 + w; x++) {
                int32_t f = 65536 + kr + crt_kc[x], sx, sy;
                uint32_t p, c, g, cr, cg, cb;
                sx = ((((crt_dh[x] * f) >> 16) + 239 + 1024) >> 1) - 512;   /* floor((s + 239) / 2) */
                sy = ((((dyh * f) >> 16) + 239 + 1024) >> 1) - 512;
                if (sx < 0 || sx >= 240 || sy < 0 || sy >= 240) {
                    *o++ = 0;                       /* off the tube */
                    continue;
                }
                if (src) {
                    if (sx < (int32_t)x0)
                        sx = (int32_t)x0;
                    else if (sx >= (int32_t)(x0 + w))
                        sx = (int32_t)(x0 + w - 1u);
                    if (sy < (int32_t)sy0)
                        sy = (int32_t)sy0;
                    else if (sy >= (int32_t)(sy0 + sh))
                        sy = (int32_t)(sy0 + sh - 1u);
                    p = src[(uint32_t)(sy - (int32_t)sy0) * w + (uint32_t)sx - x0];
                } else {
                    p = fc;
                }
                c = (p >> 8 | p << 8) & 0xFFFFu;    /* stored byte-swapped */
                g = gy * crt_vig[x] >> 8;
                cr = (c >> 11) * g >> 8;
                cg = (c >> 5 & 63u) * g >> 8;
                cb = (c & 31u) * g >> 8;
                c = (cr > 31u ? 31u : cr) << 11 | (cg > 63u ? 63u : cg) << 5 | (cb > 31u ? 31u : cb);
                *o++ = (uint16_t)(c >> 8 | (c & 0xFFu) << 8);
            }
        }
        lcd_blit(x0, y0 + r, w, n, crt_buf[crt_k]);
        crt_k ^= 1u;
    }
}

static void cv_show(uint32_t x, uint32_t y) { crt_out(x, cv_w, y, cv_h, cv_px, y, cv_h, 0); }   /* the canvas, at x, y */

static void crt_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)   /* lcd_fill, on the tube */
{
    if (x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    crt_out(x, w, y, h, 0, y, h, swap16(c));
}

/* the roll, to row y: 1 if it moved (then the rows lo .. hi need drawing again) */
static int crt_roll_to(int32_t y, int32_t *lo, int32_t *hi)
{
    int32_t a = crt_roll;
    if (y == a)
        return 0;
    crt_roll = y;
    *lo = (a < y ? a : y) - CRT_ROLL_H;
    *hi = (a > y ? a : y) + CRT_ROLL_H;
    if (*lo < 0)
        *lo = 0;
    if (*hi > 239)
        *hi = 239;
    return *lo <= *hi;
}
