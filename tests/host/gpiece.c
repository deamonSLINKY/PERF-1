/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): what a genre's piece sets: gpiece genre piece */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    const int g = argc > 1 ? atoi(argv[1]) : 0, piece = argc > 2 ? atoi(argv[2]) : 0;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    int p;
    en_init();
    en_piece_g(g, piece, par, &pool);
    printf("%s piece %d\n", en_genre_name(g), piece);
    printf("  pad %s | drone %s | acid %s | pluck %s (%d) | rate %d dens %d\n", EN_PAD_SOUND[par[P_PADTONE]], EN_DRONE_SOUND[par[P_DR_INST]],
           EN_ACID_SOUND[par[P_AC_WAVE]], EN_CM_SOUND[par[P_CM_SND]], par[P_CM_SND], par[P_CM_RATE], par[P_CM_DENS]);
    printf("  reverb type %d (0 plate, 1 shimmer, 2 gated), amount %d, chorus %d, tempo %d\n", par[P_RV_TYPE], par[P_REVERB], par[P_PL_CHO], par[P_TEMPO]);
    for (p = 0; p < 0; p++)
        ;
    return 0;
}
