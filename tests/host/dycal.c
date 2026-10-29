/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the level of DX7 voices given as packed 128-byte records (a file of them, or a 32-voice VMEM .syx) against the plain
 * sounds' average, measured as fmcal does (the pluck arpeggiating alone), and the FM gain each needs. dycal FILE: a line per voice. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static double level(int sn, double *peak)
{
    static int32_t buf[512];
    double s = 0.0, pk = 0.0;
    long cnt = 0;
    int b, j;
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
            if (fabs(x) > pk)
                pk = fabs(x);
        }
    }
    *peak = pk;
    return sqrt(s / (double)cnt);
}

int main(int argc, char **argv)
{
    static uint8_t data[1 << 16];
    FILE *f;
    size_t n, off = 0, nv, i;
    double mean = 0.0, pk;
    int k;
    if (argc < 2 || !(f = fopen(argv[1], "rb")))
        return fprintf(stderr, "dycal FILE (128-byte records, or a VMEM .syx)\n"), 1;
    n = fread(data, 1, sizeof data, f);
    fclose(f);
    if (n >= 4104 && data[0] == 0xF0 && data[3] == 0x09)
        off = 6, n = 4096;
    nv = n / 128;
    en_init();
    for (k = 0; k < 12; k++)
        mean += level(k, &pk);
    mean /= 12.0;
    fprintf(stderr, "plain mean rms %.4f\n", mean);
    for (i = 0; i < nv; i += EN_NUSER) {
        size_t j;
        en_init();
        en_user_clear();
        for (j = 0; j < EN_NUSER && i + j < nv; j++)
            if (!en_user_voice((int)j, data + off + 128 * (i + j)))
                return fprintf(stderr, "voice %u not taken\n", (unsigned)(i + j)), 1;
        for (j = 0; j < EN_NUSER && i + j < nv; j++) {
            const uint8_t *r = data + off + 128 * (i + j);
            char name[11];
            double rms, g;
            for (k = 0; k < 10; k++)
                name[k] = r[118 + k] >= 32 && r[118 + k] < 127 ? (char)r[118 + k] : '?';
            name[10] = 0;
            rms = level(12 + EN_NFMFACT + (int)j, &pk);
            g = rms > 1e-6 ? mean / rms : 1.0;
            g = g < 0.15 ? 0.15 : g > 6.0 ? 6.0 : g;
            printf("%2u %-10s alg %2d  rms %.4f peak %.3f  gain %.2f%s\n", (unsigned)(i + j), name, (r[110] & 31) + 1, rms, pk, g, rms < 1e-4 ? "  SILENT" : "");
        }
    }
    return 0;
}
