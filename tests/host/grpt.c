/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): does a beat repeat repeat? Renders a piece, holds one repeat key for two bars, and for each repeat-length
 * segment prints its RMS and its correlation with the first segment of the hold (a working repeat: correlation near 1, RMS steady).
 *   grpt [genre] [piece] [key: 0 RPT 1/2 .. 5 RPT 1/32]  (settings: PITCH DECAY GATE REFRESH as en_fxs values, optional) */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    static float mono[44100 * 4];
    const int g = argc > 1 ? atoi(argv[1]) : 1, piece = argc > 2 ? atoi(argv[2]) : 6, key = argc > 3 ? atoi(argv[3]) : 3;
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, b, pt, l, n = 0, seg, tempo, step, len, k;
    en_init();
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    if (argc > 7) {
        en_fxs(EN_FXS_RP_PITCH, atoi(argv[4]));
        en_fxs(EN_FXS_RP_DECAY, atoi(argv[5]));
        en_fxs(EN_FXS_RP_GATE, atoi(argv[6]));
        en_fxs(EN_FXS_RP_REFRESH, atoi(argv[7]));
    }
    en_play(1);
    tempo = par[P_TEMPO];
    step = (int)(44100.0 * 60.0 / tempo / 4.0 + 0.5);             /* a sixteenth */
    for (b = 0; b < 44100 * 3 / 256; b++)                          /* three seconds in */
        en_render(buf, 256, 2048);
    en_fx((uint64_t)1 << (FX_RPT2 + key));
    for (b = 0; b < 44100 * 4 / 256 && n + 256 <= 44100 * 4; b++) {
        en_render(buf, 256, 2048);
        for (k = 0; k < 256; k++)
            mono[n++] = ((float)buf[2 * k] + (float)buf[2 * k + 1]) * (0.5f / 8300000.0f);
    }
    en_fx(0);
    {
        static const double BEATS[6] = {2.0, 1.0, 2.0 / 3.0, 0.5, 0.25, 0.125};
        len = (int)(BEATS[key] * 4.0 * step);                      /* the repeat's length in samples */
    }
    /* skip the first sixteenth (the repeat starts on the sixteenth) and measure the segments after */
    printf("repeat key %d: %d-sample segments (%.1f ms), tempo %d\n", key, len, 1000.0 * len / 44100.0, tempo);
    {
        const int start = step + len;                              /* the second full segment: the first after the engage is partly live */
        double e0 = 0.0;
        for (seg = 0; seg < 12 && start + (seg + 2) * len <= n; seg++) {
            double rms = 0.0, dot = 0.0, e1 = 0.0;
            const float *a = mono + start, *c = mono + start + seg * len;
            for (k = 0; k < len; k++) {
                rms += (double)c[k] * c[k];
                dot += (double)a[k] * c[k];
                e1 += (double)c[k] * c[k];
            }
            if (seg == 0)
                e0 = e1;
            printf("  seg %2d  rms %.4f  corr %.3f\n", seg, sqrt(rms / len), e0 > 0 && e1 > 0 ? dot / sqrt(e0 * e1) : 0.0);
        }
    }
    return 0;
}
