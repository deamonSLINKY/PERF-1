/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): where a slow half's time goes. Renders one piece as the device does (two 256-frame chunks a half) with the
 * stage clock installed, and for every half measures the whole against the sum of its render_chunk stages: the rest is the control
 * path (drain, control, clock_pulse / tick, drums_wait). Prints the distribution and the worst halves with their step and stages.
 *   gspike genre piece [seconds] [worst] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static uint32_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u);
}
static int cmp(const void *a, const void *b) { const double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

struct half { double tot, st[9], stages, outside; uint8_t step, chunks; uint32_t bar; };
static const char *SN[9] = {"clear", "bassacid", "pluck", "w_pad", "merge", "drums", "echo", "w_rev", "master"};

int main(int argc, char **argv)
{
    static int32_t buf[1024];
    const int g = argc > 1 ? atoi(argv[1]) : 0, piece = argc > 2 ? atoi(argv[2]) : 0, secs = argc > 3 ? atoi(argv[3]) : 60, worst = argc > 4 ? atoi(argv[4]) : 12;
    const int n = secs * 44100 / 512;
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    struct half *h = calloc((size_t)n, sizeof *h);
    double *s = malloc(sizeof(double) * (size_t)n), *o = malloc(sizeof(double) * (size_t)n);
    int p, b, pt, l, k, hot[16] = {0}, hot_out[16] = {0};
    en_init();
    en_prof_clock = now_us;
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_play(1);
    for (b = 0; b < 200; b++)
        en_render(buf, 512, 2048);
    if (argc > 5) {                             /* the cost of a chunk against its size: the piece rendered in blocks of BLK frames */
        const int blk = atoi(argv[5]);
        const int frames = secs * 44100;
        uint32_t before[9], t0 = now_us(), tot;
        int f;
        for (k = 0; k < 9; k++)
            before[k] = en_prof[k];
        for (f = 0; f < frames; f += blk)
            en_render(buf, (uint32_t)blk, 2048);
        tot = now_us() - t0;
        printf("%s piece %d in blocks of %3d: %6.1f us per 512 frames |", en_genre_name(g), piece, blk, (double)tot * 512.0 / (double)frames);
        for (k = 0; k < 9; k++)
            printf(" %s %.1f", SN[k], (double)(en_prof[k] - before[k]) * 512.0 / (double)frames);
        printf("\n");
        return 0;
    }
    for (b = 0; b < n; b++) {
        uint32_t before[9], chunks0 = en_prof[9], t0;
        for (k = 0; k < 9; k++)
            before[k] = en_prof[k];
        t0 = now_us();
        en_render(buf, 256, 2048);
        en_render(buf + 512, 256, 2048);
        h[b].tot = (double)(now_us() - t0);
        h[b].stages = 0.0;
        for (k = 0; k < 9; k++) {
            h[b].st[k] = (double)(en_prof[k] - before[k]);
            h[b].stages += h[b].st[k];
        }
        h[b].outside = h[b].tot - h[b].stages;
        h[b].step = en_drum_pos;
        h[b].chunks = (uint8_t)(en_prof[9] - chunks0);
        h[b].bar = en_bars;
        s[b] = h[b].tot;
        o[b] = h[b].outside;
    }
    qsort(s, (size_t)n, sizeof(double), cmp);
    qsort(o, (size_t)n, sizeof(double), cmp);
    {
        const double med = s[n / 2], omed = o[n / 2];
        printf("%s piece %d, %d halves: total median %.0f us, p99 %.0f (%.2fx), max %.0f (%.2fx); outside stages: median %.0f, p99 %.0f, max %.0f\n",
               en_genre_name(g), piece, n, med, s[n * 99 / 100], s[n * 99 / 100] / med, s[n - 1], s[n - 1] / med, omed, o[n * 99 / 100], o[n - 1]);
        for (b = 0; b < n; b++) {
            if (h[b].tot > 1.5 * med)
                hot[h[b].step & 15]++;
            if (h[b].outside > 4.0 * omed && h[b].outside > 0.1 * med)
                hot_out[h[b].step & 15]++;
        }
        {
            int ch[17] = {0}, mx = 0;
            for (b = 0; b < n; b++) {
                ch[h[b].chunks > 16 ? 16 : h[b].chunks]++;
                if (h[b].chunks > mx)
                    mx = h[b].chunks;
            }
            printf("chunks a half (count of halves by chunks 1..%d):", mx);
            for (p = 1; p <= (mx > 16 ? 16 : mx); p++)
                printf(" %d:%d", p, ch[p]);
            printf("\n");
        }
        printf("halves > 1.5x median by step: ");
        for (p = 0; p < 16; p++)
            printf(" %d", hot[p]);
        printf("\nhalves whose outside time > 4x its median and > 10%% of a median half, by step: ");
        for (p = 0; p < 16; p++)
            printf(" %d", hot_out[p]);
        printf("\n");
        /* the worst halves by total */
        for (p = 0; p < worst; p++) {
            int bi = -1;
            for (b = 0; b < n; b++)
                if (h[b].tot >= 0.0 && (bi < 0 || h[b].tot > h[bi].tot))
                    bi = b;
            if (bi < 0)
                break;
            printf("  bar %4u step %2d chunks %d: total %5.0f  stages %5.0f  outside %5.0f  |", h[bi].bar, h[bi].step, h[bi].chunks, h[bi].tot, h[bi].stages, h[bi].outside);
            for (k = 0; k < 9; k++)
                printf(" %s %.0f", SN[k], h[bi].st[k]);
            printf("\n");
            h[bi].tot = -1.0;
        }
    }
    return 0;
}
