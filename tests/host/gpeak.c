/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the cost of each 256-frame render for one genre's piece, as a ratio to the median: where the peaks are
 * (the telemetry's xruns were single slow renders). gpeak genre piece [seconds] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec * 1e-3;
}
static int cmp(const void *a, const void *b) { const double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    const int g = argc > 1 ? atoi(argv[1]) : 0, piece = argc > 2 ? atoi(argv[2]) : 0, secs = argc > 3 ? atoi(argv[3]) : 30;
    const int n = secs * 44100 / 256;
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    double *t = malloc(sizeof(double) * (size_t)n), *s = malloc(sizeof(double) * (size_t)n);
    uint8_t *step = malloc((size_t)n);
    int p, b, pt, l, hot[16] = {0};
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
    for (b = 0; b < 400; b++)
        en_render(buf, 256, 2048);
    for (b = 0; b < n; b++) {
        double a = now_us();
        en_render(buf, 256, 2048);
        t[b] = now_us() - a;
        s[b] = t[b];
        step[b] = en_drum_pos;
    }
    qsort(s, (size_t)n, sizeof(double), cmp);
    {
        const double med = s[n / 2], p99 = s[n * 99 / 100], mx = s[n - 1];
        double pair = 0.0;
        for (b = 0; b + 1 < n; b++)
            if (t[b] + t[b + 1] > pair)
                pair = t[b] + t[b + 1];
        printf("%s piece %d: median %.0f us, p99 %.0f (%.2fx), max %.0f (%.2fx); worst pair %.0f (%.2fx of 2 medians)\n", en_genre_name(g), piece, med, p99,
               p99 / med, mx, mx / med, pair, pair / (2 * med));
        for (b = 0; b < n; b++)
            if (t[b] > 1.25 * med)
                hot[step[b] & 15]++;
        printf("renders > 1.25x median by step:");
        for (p = 0; p < 16; p++)
            printf(" %d", hot[p]);
        printf("\n");
    }
    return 0;
}
