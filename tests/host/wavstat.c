/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): what a fmdemo WAV (4 s a sound, 16-bit stereo) sounds like in numbers: the peak, the level every 250 ms (the decay), and the spectral centroid
 * at the start and a second in (a plucked string gets duller). wavstat file.wav name1,name2,... */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double centroid(const double *x, int n, double sr)
{
    double num = 0.0, den = 0.0;
    int k, i;
    for (k = 1; k < n / 2; k++) {
        double re = 0.0, im = 0.0;
        for (i = 0; i < n; i++) {
            const double w = 0.5 - 0.5 * cos(6.283185307179586 * i / n), a = 6.283185307179586 * k * i / n;
            re += x[i] * w * cos(a);
            im -= x[i] * w * sin(a);
        }
        {
            const double m = sqrt(re * re + im * im);
            num += m * k * sr / n;
            den += m;
        }
    }
    return den > 0 ? num / den : 0.0;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    const double sr = 44100.0;
    char names[512];
    char *nm[32];
    int nn = 0, s;
    long total;
    short *pcm;
    double *mono;
    unsigned char h[44];
    if (!f || argc < 3)
        return 1;
    strncpy(names, argv[2], sizeof names - 1);
    for (char *p = strtok(names, ","); p && nn < 32; p = strtok(0, ","))
        nm[nn++] = p;
    fread(h, 1, 44, f);
    fseek(f, 0, SEEK_END);
    total = (ftell(f) - 44) / 4;
    fseek(f, 44, SEEK_SET);
    pcm = malloc((size_t)total * 4);
    fread(pcm, 4, (size_t)total, f);
    mono = malloc((size_t)total * sizeof *mono);
    for (long i = 0; i < total; i++)
        mono[i] = ((double)pcm[2 * i] + pcm[2 * i + 1]) * 0.5 / 32768.0;
    for (s = 0; s < nn && (long)(s + 1) * 4 * 44100 <= total + 44100; s++) {
        const double *x = mono + (long)s * 4 * 44100;
        double pk = 0.0;
        printf("%-12s", nm[s]);
        for (long i = 0; i < 4 * 44100 && (long)s * 4 * 44100 + i < total; i++)
            if (fabs(x[i]) > pk)
                pk = fabs(x[i]);
        printf(" peak %.3f  level/250ms:", pk);
        for (int w = 0; w < 12; w++) {
            double e = 0.0;
            for (int i = 0; i < 11025; i++)
                e += x[w * 11025 + i] * x[w * 11025 + i];
            printf(" %.3f", sqrt(e / 11025.0));
        }
        printf("  centroid %.0f -> %.0f Hz\n", centroid(x + 4410, 2048, sr), centroid(x + 44100 + 4410, 2048, sr));
    }
    return 0;
}
