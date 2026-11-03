/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the chords a piece walks through, as scale degrees (I .. VII, by the chord's root against the piece's scale), and how often each
 * move happens. gwalk genre piece [minutes] */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    static const char *const R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    const int g = argc > 1 ? atoi(argv[1]) : 0, piece = argc > 2 ? atoi(argv[2]) - 1 : 0, mins = argc > 3 ? atoi(argv[3]) : 10;
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, b, pt, l, st[12], m, last = -1, prev = -1, mv[7][7] = {{0}}, n = 0, shown = 0;
    en_init();
    en_genre(g);
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_play(1);
    m = en_scale(pool, st);
    printf("%s %02d: root %d, mode %d, %d notes:", en_genre_name(g), piece + 1, par[P_ROOT], par[P_MODE], m);
    for (b = 0; b < mins * 60 * 44100 / 256; b++) {
        int d, k;
        en_render(buf, 256, 4096);
        k = (en_chord_root - par[P_ROOT] + 12) % 12;
        for (d = 0; d < m && st[d] != k; d++)
            ;
        if (d < m && d != last) {
            if (shown++ < 32)
                printf(" %s", d < 7 ? R[d] : "?");
            if (last >= 0 && last < 7 && d < 7)
                mv[last][d]++, n++;
            prev = last;
            last = d;
        }
    }
    (void)prev;
    printf("\n  moves (%d):", n);
    for (b = 0; b < 7; b++)
        for (p = 0; p < 7; p++)
            if (mv[b][p] * 20 >= n && n)
                printf(" %s>%s %d%%", R[b], R[p], 100 * mv[b][p] / n);
    printf("\n");
    return 0;
}
