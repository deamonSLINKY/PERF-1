/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): what a breakdown leaves audible in a genre's piece: the output's RMS in the break with every track, then with each
 * track muted in turn (the drop in RMS is that track's share). gbreak genre [piece] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static int32_t buf[2 * 256];
static double rms(float secs)
{
    const int blocks = (int)(secs * EN_SR / 256.0f);
    double e = 0.0;
    int b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            const double x = (double)buf[j] / 8388608.0;
            e += x * x;
        }
    }
    return sqrt(e / (blocks * 512.0));
}
int main(int argc, char **argv)
{
    static const char *const TR[8] = {"kick", "hats", "snare", "perc", "bass", "acid", "pad", "pluck"};
    const int genre = argc > 1 ? atoi(argv[1]) : 11;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    int t, p;
    double all;
    en_init();
    en_genre(genre);
    en_piece_g(genre, argc > 2 ? atoi(argv[2]) : 0, par, &pool);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    en_regrow();
    en_play(1);
    rms(4.0f);
    printf("genre %d: groove rms %.4f\n", genre, rms(4.0f));
    en_section(EN_CMD_BREAK);
    rms(3.0f);
    all = rms(4.0f);
    printf("  break (section %d): all tracks %.4f\n", en_sec, all);
    for (t = 0; t < 8; t++) {
        double m;
        en_trk(1u << t, 0);
        rms(0.3f);
        m = rms(3.0f);
        printf("  without %-5s %.4f  (%+.0f %%)\n", TR[t], m, (m - all) / (all > 1e-9 ? all : 1e-9) * 100.0);
        en_trk(0, 0);
        rms(0.3f);
    }
    return 0;
}
