/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the engine under storms. Genres switched quickly, effects thrown on and off, pieces changed mid-note: does the output stay finite and
 * does it come back after each storm? gstress [seconds_per_step] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static int32_t buf[512];
static double sum2;
static int nan_n, zero_blocks;
static float peak;

static void run(float secs)
{
    const int blocks = (int)(secs * 44100.0f / 256.0f);
    int b, j;
    for (b = 0; b < blocks; b++) {
        double s = 0;
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            const float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                nan_n++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            s += (double)x * x;
        }
        sum2 += s;
        if (s < 1e-9)
            zero_blocks++;
    }
}

static void piece(int g, int i)
{
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    uint8_t mod[EN_NLANE][16];
    int p, l;
    en_piece_g(g, i, par, &pool);
    en_piece_pats_g(g, i, pats);
    en_piece_mods_g(g, i, mod);
    en_genre(g);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = pats[p][l];
    for (l = 0; l < EN_NLANE; l++)
        for (p = 0; p < 16; p++)
            en_mod[l][p] = mod[l][p];
}

int main(int argc, char **argv)
{
    const float step = argc > 1 ? (float)atof(argv[1]) : 0.4f;
    int g, i, f, bad = 0, round;
    unsigned seed = 12345u;
    en_init();
    en_play(1);
    for (round = 0; round < 6; round++) {
        /* a storm: genre and piece every `step` seconds, an effect or two latched */
        for (i = 0; i < 40; i++) {
            seed = seed * 1664525u + 1013904223u;
            g = (int)(seed >> 24) % EN_NGENRE;
            piece(g, (int)(seed >> 16) % 10);
            en_fx(seed & 1u ? (1ull << FX_TAPE) : 0ull);
            if (seed & 2u)
                en_fx((1ull << FX_TAPE) | (1ull << FX_VINYL) | (1ull << FX_PUMP));
            run(step);
        }
        en_fx(0);
        piece(13, 3);
        peak = 0;
        sum2 = 0;
        zero_blocks = 0;
        run(6.0f);
        printf("round %d: nan %d, rms %.4f, peak %.3f, silent blocks %d of %d\n", round, nan_n, sqrt(sum2 / (6.0 * 44100.0 * 2.0)), (double)peak, zero_blocks, (int)(6.0f * 44100.0f / 256.0f));
        if (nan_n || sqrt(sum2 / (6.0 * 44100.0 * 2.0)) < 0.01)
            bad++;
    }
    /* each effect alone on each new genre for 3 s, then off for 3 s: it must come back */
    for (g = 10; g < EN_NGENRE; g++)
        for (f = 0; f < 30; f++) {
            double r;
            piece(g, 1);
            en_fx(1ull << f);
            run(2.0f);
            en_fx(0);
            peak = 0;
            sum2 = 0;
            run(3.0f);
            r = sqrt(sum2 / (3.0 * 44100.0 * 2.0));
            if (nan_n || r < 0.01) {
                printf("genre %d fx %d: nan %d rms %.4f\n", g, f, nan_n, r);
                bad++;
                nan_n = 0;
            }
        }
    printf(bad ? "gstress: %d problems\n" : "gstress: ok\n", bad);
    return bad != 0;
}
