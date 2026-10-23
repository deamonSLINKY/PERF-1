#define _POSIX_C_SOURCE 200809L
/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the host CPU time per 4 s of a genre's piece, over a minute: does the cost creep up (denormal numbers, growing state)? gcreep genre piece */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    const int g = argc > 1 ? atoi(argv[1]) : 0, piece = argc > 2 ? atoi(argv[2]) : 0;
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, b, seg, pt, l;
    en_init();
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_play(1);
    printf("%s piece %d:", en_genre_name(g), piece);
    for (seg = 0; seg < 15; seg++) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);
        for (b = 0; b < 690; b++)
            en_render(buf, 256, 2048);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
        printf(" %.0f", (double)(t1.tv_sec - t0.tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
    }
    printf("  (ms per 4 s)\n");
    return 0;
}
