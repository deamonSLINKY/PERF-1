/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): how loud each track of a piece is against the whole mix. The piece plays from a fresh start (the same
 * notes each time) once whole and once with each track soloed; each run's RMS and peak after a warm-up, in dB.
 * With TRIM=1 in the environment the pieces play at their trimmed level (en_trim), to check the table.
 *   glevel genre [piece] [seconds]   one line a piece: the mix's RMS (dBFS), then each track's RMS against the mix (dB) and its peak (dBFS) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static const char *const TRK[EN_NTRACK] = {"kick", "hats", "snare", "perc", "bass", "acid", "pad", "pluck"};

static void run(int g, int piece, int solo, int secs, double *rms, double *peak)
{
    static int32_t buf[512];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    const int warm = 4 * 44100 / 256, n = secs * 44100 / 256;
    double s2 = 0.0, pk = 0.0;
    int p, b, i, pt, l;
    en_init();
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    en_trim(g, getenv("TRIM") ? piece : -1);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_trk(0, solo >= 0 ? 1u << solo : 0u);      /* (always: en_init keeps the last run's solo) */
    en_play(1);
    for (b = 0; b < warm; b++)
        en_render(buf, 256, 4096);
    for (b = 0; b < n; b++) {
        en_render(buf, 256, 4096);
        for (i = 0; i < 512; i++) {
            const double x = buf[i] * (1.0 / 8388608.0);
            s2 += x * x;
            if (fabs(x) > pk)
                pk = fabs(x);
        }
    }
    *rms = 10.0 * log10(s2 / (512.0 * n) + 1e-20);
    *peak = 20.0 * log10(pk + 1e-12);
}

int main(int argc, char **argv)
{
    const int g = argc > 1 ? atoi(argv[1]) : 0, one = argc > 2 ? atoi(argv[2]) : -1, secs = argc > 3 ? atoi(argv[3]) : 20;
    int piece, t;
    for (piece = one < 0 ? 0 : one; piece < (one < 0 ? 10 : one + 1); piece++) {
        double mr, mp, r, pk;
        run(g, piece, -1, secs, &mr, &mp);
        printf("%s %02d mix %6.1f pk %5.1f |", en_genre_name(g), piece + 1, mr, mp);
        for (t = 0; t < EN_NTRACK; t++) {
            run(g, piece, t, secs, &r, &pk);
            printf(" %s %+5.1f/%5.1f", TRK[t], r - mr, pk);
        }
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
