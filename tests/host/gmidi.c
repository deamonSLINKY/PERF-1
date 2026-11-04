/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): when each part's notes start, from the engine's MIDI out, rendered 16 samples at a time (0.36 ms).
 * Each note-on is placed on the sixteenth grid (from the start of play): its step and how far from it (ms). Prints, per channel and
 * note, how many and their offsets' mean, spread and extremes, then the first bars note by note.
 *   gmidi genre piece [seconds] [bars to list]   (piece 1 ..) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

#define SR 44100
static long now;                                 /* samples rendered */
static double step;
typedef struct { long t; uint8_t st, d1, d2; } ev_t;
static ev_t ev[200000];
static float aenv[SR * 180 / 16];   /* SOLO: the soloed track's level, 16 samples a value */
static int nev;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (nev < 200000 && (st & 0xF0u) == 0x90u && d2) {
        ev[nev].t = now;
        ev[nev].st = (uint8_t)st;
        ev[nev].d1 = (uint8_t)d1;
        ev[nev].d2 = (uint8_t)d2;
        nev++;
    }
}

int main(int argc, char **argv)
{
    const int g = argc > 1 ? atoi(argv[1]) : 18, piece = (argc > 2 ? atoi(argv[2]) : 1) - 1, secs = argc > 3 ? atoi(argv[3]) : 60,
              list = argc > 4 ? atoi(argv[4]) : 2;
    static int32_t buf[64];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, pt, l, i, c;
    en_init();
    en_piece_g(g, piece, par, &pool);
    en_piece_pats_g(g, piece, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_set(P_MIDI, 1);
    en_pool(pool);
    for (pt = 0; pt < EN_NPAT; pt++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[pt][l] = pats[pt][l];
    en_trk(0, getenv("SOLO") ? 1u << atoi(getenv("SOLO")) : 0u);
    {   /* SET="p=v,.." (parameter numbers) after the piece's */
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
    en_play(1);
    step = 60.0 / par[P_TEMPO] / 4.0 * SR;
    for (; now < (long)secs * SR; now += 16) {
        double a = 0.0;
        en_render(buf, 16, 4096);
        for (i = 0; i < 32; i++)
            a += (double)buf[i] * buf[i];
        aenv[now / 16] = (float)sqrt(a / 32.0);
    }
    if (getenv("SOLO") && getenv("CH")) {   /* each note of channel CH: from its note-on to half its own peak (in the next sixteenth), and the peak; by 4 bars */
        const int ch = atoi(getenv("CH")) - 1, w = (int)(step / 16.0);
        double sb[64] = {0}, pb[64] = {0};
        int nb[64] = {0}, j, b, top = 0;
        for (i = 0; i < nev; i++) {
            const long k0 = ev[i].t / 16;
            float pk = 0.0f;
            if ((ev[i].st & 15) != ch)
                continue;
            for (j = 0; j < w; j++)
                if (aenv[k0 + j] > pk)
                    pk = aenv[k0 + j];
            for (j = 0; j < w && aenv[k0 + j] < 0.5f * pk; j++)
                ;
            b = (int)(ev[i].t / (step * 64.0));
            if (b < 64) {
                sb[b] += j * 16.0 * 1000.0 / SR;
                pb[b] += pk;
                nb[b]++;
                top = b + 1 > top ? b + 1 : top;
            }
        }
        printf("ch %d, track %s: to half its peak (ms) / its peak (dB), each 4 bars:\n", ch + 1, getenv("SOLO"));
        for (b = 0; b < top; b++)
            if (nb[b])
                printf(" %4.1f/%+.0f", sb[b] / nb[b], 20.0 * log10(pb[b] / nb[b] / 8388608.0 + 1e-12));
        printf("\n");
        return 0;
    }
    printf("%s %02d, %d BPM, a sixteenth %.1f ms; %d note-ons\n", en_genre_name(g), piece + 1, par[P_TEMPO], step * 1000.0 / SR, nev);
    for (c = 0; c < 16; c++) {               /* per channel (and, for the drums' channel, per note) */
        int notes[128] = {0}, nn = 0, n;
        for (i = 0; i < nev; i++)
            if ((ev[i].st & 15) == c)
                notes[c == 9 ? ev[i].d1 : 0]++;
        for (n = 0; n < 128; n++) {
            double s = 0, s2 = 0, mn = 1e9, mx = -1e9;
            int k = 0;
            if (!notes[n])
                continue;
            for (i = 0; i < nev; i++)
                if ((ev[i].st & 15) == c && (c != 9 || ev[i].d1 == n)) {
                    const double rel = ev[i].t / step, d = (rel - floor(rel + 0.5)) * step * 1000.0 / SR;
                    s += d;
                    s2 += d * d;
                    mn = d < mn ? d : mn;
                    mx = d > mx ? d : mx;
                    k++;
                }
            printf("ch %2d%s%-3d n %5d  offset mean %+6.2f sd %5.2f  min %+6.2f max %+6.2f ms\n", c + 1, c == 9 ? " note " : "", c == 9 ? n : 0, k,
                   s / k, sqrt(fabs(s2 / k - (s / k) * (s / k))), mn, mx);
            nn++;
        }
        (void)nn;
    }
    {   /* the bars at second 30, note by note: step.offset per channel */
        const long b0 = (long)(floor(30.0 * SR / (step * 16.0)) * step * 16.0);
        for (c = 0; c < 16; c++) {
            int any = 0;
            for (i = 0; i < nev; i++)
                if ((ev[i].st & 15) == c && ev[i].t >= b0 && ev[i].t < b0 + (long)(step * 16.0 * list)) {
                    const double rel = (ev[i].t - b0) / step;
                    if (!any)
                        printf("ch %2d:", c + 1);
                    printf(" %d%+.1f%s", (int)floor(rel + 0.5), (rel - floor(rel + 0.5)) * step * 1000.0 / SR, c == 9 ? (ev[i].d1 == 36 ? "K" : "") : "");
                    any = 1;
                }
            if (any)
                printf("\n");
        }
    }
    return 0;
}
