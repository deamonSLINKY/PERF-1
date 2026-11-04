/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): where each track's notes land against the sixteenth grid. A piece plays from a fresh start once per
 * track, soloed; onsets are found in the audio (a 1 ms envelope rising past a third of the track's peak after a quiet gap), and each
 * is measured from the nearest sixteenth (the grid starts at the first kick). Per track: the count, the mean and spread of the
 * offset (ms), and the offset averaged over each 4 bars, to show a part that slides and comes back.
 *   gonset genre piece [seconds]   (piece 1 ..) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static const char *const TRK[EN_NTRACK] = {"kick", "hats", "snare", "perc", "bass", "acid", "pad", "pluck"};
#define SR 44100
#define MAXS (SR * 180)
static float env[MAXS / 44 + 8];

static int render(int g, int piece, int solo, int secs, int *bpm)
{
    static int32_t buf[512];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    const int n = secs * SR / 256;
    int p, b, i, pt, l, k = 0;
    double acc = 0.0;
    int cnt = 0;
    en_init();
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    *bpm = par[P_TEMPO];
    {   /* SET="p=v,p=v": parameters (by number) changed after the piece's, to try what moves the onsets */
        const char *e = getenv("SET");
        while (e && *e) {
            const int q = atoi(e);
            const char *v = e;
            while (*v && *v != '=')
                v++;
            if (!*v)
                break;
            en_set(q, atoi(v + 1));
            while (*v && *v != ',')
                v++;
            e = *v ? v + 1 : v;
        }
    }
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_trk(0, 1u << solo);
    en_play(1);
    for (b = 0; b < n; b++) {
        en_render(buf, 256, 4096);
        for (i = 0; i < 256; i++) {
            const double x = (buf[2 * i] + (double)buf[2 * i + 1]) * (0.5 / 8388608.0);
            acc += x * x;
            if (++cnt == 44) {                   /* about 1 ms */
                env[k++] = (float)sqrt(acc / 44.0);
                acc = 0.0;
                cnt = 0;
            }
        }
    }
    return k;
}

int main(int argc, char **argv)
{
    const int g = argc > 1 ? atoi(argv[1]) : 18, piece = (argc > 2 ? atoi(argv[2]) : 1) - 1, secs = argc > 3 ? atoi(argv[3]) : 60;
    static double on[EN_NTRACK][4096];
    int non[EN_NTRACK] = {0};
    int t, bpm = 120, k, i;
    double step, t0 = -1.0;
    for (t = 0; t < EN_NTRACK; t++) {
        float pk = 0.0f, lo;
        int quiet = 1;
        k = render(g, piece, t, secs, &bpm);
        for (i = 0; i < k; i++)
            if (env[i] > pk)
                pk = env[i];
        lo = pk / 3.0f;
        for (i = 1; i < k && non[t] < 4096; i++) {
            if (env[i] < pk / 8.0f)
                quiet = 1;
            if (quiet && env[i] >= lo && pk > 1e-4f) {
                on[t][non[t]++] = (i - 1 + (lo - env[i - 1]) / (env[i] - env[i - 1] + 1e-12f)) * 44.0 / SR;   /* seconds */
                quiet = 0;
            }
        }
    }
    step = 60.0 / bpm / 4.0;
    if (non[0])
        t0 = on[0][0];
    if (getenv("DUMP")) {   /* DUMP=track: its envelope, 4 ms a column, a beat a line, from second 30 ('|': a sixteenth of the grid) */
        const int tr = atoi(getenv("DUMP")), from = 30000;
        float pk = 0.0f;
        int ln, c, j;
        k = render(g, piece, tr, 40, &bpm);
        for (j = 0; j < k; j++)
            if (env[j] > pk)
                pk = env[j];
        for (ln = 0; ln < 16; ln++) {
            const double b0 = t0 * 1000.0 + (floor((from - t0 * 1000.0) / (step * 4000.0)) + ln) * step * 4000.0;
            int mark = 0;
            printf("%7.0f ", b0);
            for (c = 0; c < (int)(step * 4000.0 / 4.0); c++) {
                const int e = (int)((b0 + c * 4.0) * (SR / 44.0 / 1000.0));   /* (an envelope column is 44 samples, not quite 1 ms) */
                const float m = e + 3 < k ? (env[e] + env[e + 1] + env[e + 2] + env[e + 3]) * 0.25f : 0.0f;
                if (c * 4.0 >= mark * step * 1000.0) {
                    putchar('|');
                    mark++;
                } else
                    putchar(" .:-=+*#%@"[(int)(m / pk * 9.99f)]);
            }
            putchar('\n');
        }
    }
    printf("%s %02d, %d BPM, a sixteenth %.1f ms, grid from the first kick at %.3f s\n", en_genre_name(g), piece + 1, bpm, step * 1000.0, t0);
    for (t = 0; t < EN_NTRACK; t++) {
        double s = 0.0, s2 = 0.0, blk[64] = {0};
        int bn[64] = {0}, nb = 0;
        if (!non[t] || t0 < 0.0)
            continue;
        for (i = 0; i < non[t]; i++) {
            const double rel = (on[t][i] - t0) / step, d = (rel - floor(rel + 0.5)) * step * 1000.0;
            const int bar4 = (int)floor((on[t][i] - t0) / (step * 64.0));
            s += d;
            s2 += d * d;
            if (bar4 >= 0 && bar4 < 64) {
                blk[bar4] += d;
                bn[bar4]++;
                if (bar4 + 1 > nb)
                    nb = bar4 + 1;
            }
        }
        printf("%-6s n %4d  mean %+6.1f ms  sd %5.1f ms  |", TRK[t], non[t], s / non[t], sqrt(s2 / non[t] - (s / non[t]) * (s / non[t])));
        {   /* by the sixteenth in the beat: how many, and their mean */
            double ps[4] = {0}, pq[4] = {0};
            int pn[4] = {0}, q;
            for (i = 0; i < non[t]; i++) {
                const double rel = (on[t][i] - t0) / step, d = (rel - floor(rel + 0.5)) * step * 1000.0;
                const int sl = ((int)floor(rel + 0.5)) & 3;
                ps[sl] += d;
                pq[sl] += d * d;
                pn[sl]++;
            }
            for (q = 0; q < 4; q++)
                if (pn[q])
                    printf(" s%d:%d %+.1f/%.1f", q, pn[q], ps[q] / pn[q], sqrt(fabs(pq[q] / pn[q] - (ps[q] / pn[q]) * (ps[q] / pn[q]))));
            printf(" |");
        }
        for (i = 0; i < nb; i++)
            printf(bn[i] ? " %+5.1f" : "     .", bn[i] ? blk[i] / bn[i] : 0.0);
        printf("\n");
    }
    return 0;
}
