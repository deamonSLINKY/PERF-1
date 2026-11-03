/* SPDX-License-Identifier: GPL-3.0-only */
/* Test and tool (run_tests.sh "twins": none at 7, 0.16.7): pieces of a genre too alike (the 2026-10-29 audit's "twins"). Each pair scores what it shares: the pad sound, the mode and the
 * chord kind 2 each, the bass sound, the acid sound, the bass line's mode and a tempo within 6 BPM 1 each; 7 or more is a twin.
 *   gtwin [threshold]   the twins of every genre, then the count */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    const int thr = argc > 1 ? atoi(argv[1]) : 7;
    static int16_t par[EN_NGENRE][EN_NPIECES][P_NPARAMS];
    int g, i, j, n = 0;
    uint16_t pool;
    en_init();
    for (g = 1; g < EN_NGENRE; g++)
        for (i = 0; i < EN_NPIECES; i++)
            en_piece_g(g, i, par[g][i], &pool);
    for (g = 1; g < EN_NGENRE; g++)
        for (i = 0; i < EN_NPIECES; i++)
            for (j = i + 1; j < EN_NPIECES; j++) {
                const int16_t *a = par[g][i], *b = par[g][j];
                const int dt = a[P_TEMPO] - b[P_TEMPO];
                const int s = 2 * (a[P_PADTONE] == b[P_PADTONE]) + 2 * (a[P_MODE] == b[P_MODE]) + 2 * (a[P_CHORD] == b[P_CHORD]) +
                              (a[P_DR_INST] == b[P_DR_INST]) + (a[P_AC_WAVE] == b[P_AC_WAVE]) + (a[P_BL_MODE] == b[P_BL_MODE]) + (dt >= -6 && dt <= 6);
                if (s >= thr) {
                    printf("%-8s %02d/%02d %d: pad %d/%d mode %d/%d chord %d/%d tempo %d/%d bass %d/%d acid %d/%d line %d/%d\n", en_genre_name(g), i + 1, j + 1, s,
                           a[P_PADTONE], b[P_PADTONE], a[P_MODE], b[P_MODE], a[P_CHORD], b[P_CHORD], a[P_TEMPO], b[P_TEMPO], a[P_DR_INST], b[P_DR_INST],
                           a[P_AC_WAVE], b[P_AC_WAVE], a[P_BL_MODE], b[P_BL_MODE]);
                    n++;
                }
            }
    printf("%d pairs at %d or more\n", n, thr);
    if (getenv("SEARCH")) {                     /* each genre's best pad shift for pieces 6-10 (vary(): pad[(i + k * (i / 5)) % 5]; pieces 1-5 give the palette's order) */
        int k;
        for (g = 1; g < EN_NGENRE; g++) {
            int best = -1, bk = 0;
            for (k = 0; k < 5; k++) {
                int pad[EN_NPIECES], c = 0;
                for (i = 0; i < EN_NPIECES; i++)
                    pad[i] = par[g][(i + k * (i / 5)) % 5][P_PADTONE];
                for (i = 0; i < EN_NPIECES; i++)
                    for (j = i + 1; j < EN_NPIECES; j++) {
                        const int16_t *a = par[g][i], *b = par[g][j];
                        const int dt = a[P_TEMPO] - b[P_TEMPO];
                        c += 2 * (pad[i] == pad[j]) + 2 * (a[P_MODE] == b[P_MODE]) + 2 * (a[P_CHORD] == b[P_CHORD]) + (a[P_DR_INST] == b[P_DR_INST]) +
                             (a[P_AC_WAVE] == b[P_AC_WAVE]) + (a[P_BL_MODE] == b[P_BL_MODE]) + (dt >= -6 && dt <= 6) >= thr;
                    }
                if (best < 0 || c < best)
                    best = c, bk = k;
            }
            printf("%-8s best pad shift %d: %d twins\n", en_genre_name(g), bk, best);
        }
    }
    return 0;
}
