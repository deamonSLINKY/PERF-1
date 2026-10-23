#define _POSIX_C_SOURCE 200809L
/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the host CPU time to render 12 s of each genre's pieces, to see which genre costs the unit more. gcost [piece] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    const int piece = argc > 1 ? atoi(argv[1]) : 0;
    int g, p, b;
    for (g = 0; g < EN_NGENRE; g++) {
        int16_t par[P_NPARAMS];
        uint16_t pool, pats[EN_NPAT][EN_NLANE];
        struct timespec t0, t1;
        double ms;
        en_init();
        en_piece_g(g, piece, par, &pool);
        en_piece_pats_g(g, piece, pats);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, par[p]);
        en_pool(pool);
        {
            int pt, l;
            for (pt = 0; pt < EN_NPAT; pt++)
                for (l = 0; l < EN_NLANE; l++)
                    en_pats[pt][l] = pats[pt][l];
        }
        en_play(1);
        for (b = 0; b < 300; b++)
            en_render(buf, 256, 2048);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);
        for (b = 0; b < 2000; b++)
            en_render(buf, 256, 2048);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
        ms = (double)(t1.tv_sec - t0.tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
        printf("%-8s piece %d: %6.1f ms for 11.6 s\n", en_genre_name(g), piece, ms);
    }
    return 0;
}
