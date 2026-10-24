/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): one genre's piece for 8 s, for valgrind --tool=callgrind (where the instructions go). gprof genre piece [pluck sound] */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    const int g = argc > 1 ? atoi(argv[1]) : 0, piece = argc > 2 ? atoi(argv[2]) : 0;
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, b, pt, l;
    en_init();
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    if (argc > 3)
        par[P_CM_SND] = (int16_t)atoi(argv[3]);
    if (argc > 4)
        par[P_RV_TYPE] = (int16_t)atoi(argv[4]);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_play(1);
    for (b = 0; b < 1400; b++)
        en_render(buf, 256, 2048);
    printf("%s piece %d, pluck sound %s (%d)\n", en_genre_name(g), piece, EN_CM_SOUND[par[P_CM_SND]], par[P_CM_SND]);
    return 0;
}
