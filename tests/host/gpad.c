/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): a pad sound on its own for some seconds (the drums, bass, acid and pluck muted): the level each half second and a brightness
 * (the RMS of the signal's first difference over its RMS: higher is brighter), so a filter sweep shows as numbers. gpad padtone [seconds] [stabs: genre] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[2 * 256];
    const int tone = argc > 1 ? atoi(argv[1]) : 2, secs = argc > 2 ? atoi(argv[2]) : 6, genre = argc > 3 ? atoi(argv[3]) : 0;
    double e = 0.0, d = 0.0, pk = 0.0;
    float last = 0.0f;
    uint32_t n = 0, blocks = 0;
    int i;
    en_init();
    en_genre(genre);
    en_set(P_PADTONE, tone);
    en_set(P_PULSE, 0);
    en_set(P_BASS, 0);
    en_set(P_ACID, 0);
    en_set(P_CM_LVL, 0);
    en_set(P_NOISE, 0);
    en_set(P_PAD, 80);
    en_set(P_REVERB, 0);
    en_set(P_ECHO, 0);
    en_set(P_PL_CHO, 0);
    en_regrow();
    en_play(1);
    printf("pad %d (%s)%s\n", tone, EN_PAD_SOUND[tone], genre ? " stabs" : "");
    for (blocks = 0; blocks < (uint32_t)secs * EN_SR / 256u; blocks++) {
        en_render(buf, 256, 2048);
        for (i = 0; i < 256; i++) {
            const float x = (float)buf[2 * i] * (1.0f / 8388608.0f);
            e += (double)x * x;
            d += (double)(x - last) * (x - last);
            last = x;
            if (fabs((double)x) > pk)
                pk = fabs((double)x);
        }
        n += 256;
        if (n >= (uint32_t)EN_SR) {
            const double rms = sqrt(e / n), bri = e > 0.0 ? sqrt(d / e) : 0.0;
            printf("  %5.1f s  rms %6.4f  peak %6.4f  bright %5.3f\n", (double)(blocks + 1) * 256.0 / EN_SR, rms, pk, bri);
            e = d = pk = 0.0;
            n = 0;
        }
    }
    return 0;
}
