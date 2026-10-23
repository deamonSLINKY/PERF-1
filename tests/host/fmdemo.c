/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): a demo of the counter-melody's sounds, one after the other (each 4 s: the pluck alone over a chord, a little
 * reverb), as a 16-bit stereo WAV. fmdemo out.wav [first last] */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(int argc, char **argv)
{
    static int32_t buf[512];
    static int16_t pcm[512];
    const int first = argc > 3 ? atoi(argv[2]) : 0, last = argc > 3 ? atoi(argv[3]) : EN_NCMSND - 1;
    FILE *f = fopen(argc > 1 ? argv[1] : "fm_demo.wav", "wb");
    uint32_t n = 0, bytes;
    int sn, b, j;
    unsigned char h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
    if (!f)
        return 1;
    fwrite(h, 1, 44, f);
    for (sn = first; sn <= last; sn++) {
        en_init();
        en_set(P_PAD, 22);
        en_set(P_BASS, 0);
        en_set(P_ACID, 0);
        en_set(P_PULSE, 0);
        en_set(P_NOISE, 0);
        en_set(P_REVERB, 25);
        en_set(P_ECHO, 15);
        en_set(P_CM_LVL, 70);
        en_set(P_CM_SND, sn);
        en_set(P_CM_STYLE, 2);
        en_set(P_CM_RATE, 0);
        en_set(P_CM_DENS, 75);
        en_play(1);
        for (b = 0; b < 690; b++) {                 /* 4 s */
            en_render(buf, 256, 2048);
            for (j = 0; j < 512; j++) {
                int32_t v = buf[j] >> 8;
                pcm[j] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
            }
            fwrite(pcm, 2, 512, f);
            n += 256;
        }
        fprintf(stderr, "%2d %s\n", sn, EN_CM_SOUND[sn]);
    }
    bytes = n * 4;
    fseek(f, 4, SEEK_SET);
    {
        uint32_t r = bytes + 36;
        fwrite(&r, 4, 1, f);
    }
    fseek(f, 40, SEEK_SET);
    fwrite(&bytes, 4, 1, f);
    fclose(f);
    return 0;
}
