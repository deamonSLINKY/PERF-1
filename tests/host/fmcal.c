/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the level of each counter-melody sound at the same settings, and the FM_GAIN each DX voice needs to sit at the
 * plain sounds' average. Prints a C initialiser for FM_GAIN in amb.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static float level(int sn)
{
    static int32_t buf[512];
    double s = 0.0, peak = 0.0;
    long cnt = 0;
    int b, j;
    en_init();
    en_set(P_PAD, 0);
    en_set(P_BASS, 0);
    en_set(P_ACID, 0);
    en_set(P_PULSE, 0);
    en_set(P_NOISE, 0);
    en_set(P_REVERB, 0);
    en_set(P_ECHO, 0);
    en_set(P_CM_LVL, 80);
    en_set(P_CM_SND, sn);
    en_set(P_CM_STYLE, 0);
    en_set(P_CM_RATE, 1);
    en_set(P_CM_DENS, 100);
    en_play(1);
    for (b = 0; b < 1400; b++) {
        en_render(buf, 256, 2048);
        if (b < 200)
            continue;
        for (j = 0; j < 512; j++) {
            double x = (double)buf[j] / 8388608.0;
            s += x * x;
            cnt++;
            if (fabs(x) > peak)
                peak = fabs(x);
        }
    }
    fprintf(stderr, "sound %2d  %-14s rms %.4f  peak %.3f\n", sn, EN_CM_SOUND[sn], sqrt(s / (double)cnt), peak);
    return (float)sqrt(s / (double)cnt);
}

int main(void)
{
    float r[EN_NCMSND], mean = 0.0f;
    int i;
    for (i = 0; i < EN_NCMSND; i++)
        r[i] = level(i);
    for (i = 0; i < 12; i++)
        mean += r[i];
    mean /= 12.0f;
    printf("static const float FM_GAIN[EN_NCMFM] = {");
    for (i = 12; i < EN_NCMSND; i++) {
        float g = r[i] > 1e-6f ? mean / r[i] : 1.0f;
        g = g < 0.15f ? 0.15f : g > 6.0f ? 6.0f : g;
        printf("%s%.2ff", i > 12 ? ", " : "", g);
    }
    printf("};\n");
    return 0;
}
