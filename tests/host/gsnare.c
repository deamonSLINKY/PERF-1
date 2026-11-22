/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): one drum hit of a kit, alone and dry, as a 16-bit mono WAV at 44.1 kHz, for tools/djsamples.py (0.17.87: the SNARE snip
 * is the Dusty kit's snare, the maker's pick). The engine as it starts, stopped, every level but the drums' at 0, no reverb, echo, drive or master
 * compressor; the drum struck by hand (en_drum_tap) at full velocity, 0.6 s recorded.
 *   gsnare kit lane out.wav   (kit: EN_KIT_NAME's index, 8 Dusty; lane 0 kick .. 5 perc) */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static void le16(FILE *f, unsigned v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void le32(FILE *f, unsigned long v) { le16(f, (unsigned)(v & 0xFFFFu)); le16(f, (unsigned)(v >> 16)); }

int main(int argc, char **argv)
{
    static const int KITP[6] = {P_KIT_KICK, P_KIT_SNARE, P_KIT_SNARE, P_KIT_HAT, P_KIT_HAT, P_KIT_PERC};
    const int kit = argc > 1 ? atoi(argv[1]) : 8, lane = argc > 2 ? atoi(argv[2]) : 1, n = 44100 * 6 / 10;
    static int32_t buf[512];
    static short out[44100];
    int16_t par[P_NPARAMS];
    uint16_t pool;
    int p, b, i, k = 0;
    FILE *f;
    if (argc < 4 || lane < 0 || lane > 5)
        return 1;
    en_init();
    en_piece_g(0, 0, par, &pool);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    en_set(KITP[lane], kit);
    for (p = 0; p < 3; p++)                       /* (the lane's level, tune and decay at their defaults: the kit as it is) */
        en_set(P_LANE + lane * 3 + p, en_param_info(P_LANE + lane * 3 + p)->def);
    en_set(P_PAD, 0); en_set(P_ACID, 0); en_set(P_BASS, 0); en_set(P_NOISE, 0); en_set(P_CM_LVL, 0);
    en_set(P_REVERB, 0); en_set(P_ECHO, 0); en_set(P_DRIVE, 0); en_set(P_MC_RATIO, 0);
    en_trim(0, -1);
    for (b = 0; b < 40; b++)                      /* (settled: the settings' glides done) */
        en_render(buf, 256, 4096);
    en_drum_tap(lane, 127);
    while (k < n) {
        en_render(buf, 256, 4096);
        for (i = 0; i < 256 && k < n; i++, k++) {
            const double v = 0.5 * ((double)buf[2 * i] + (double)buf[2 * i + 1]) / 8388608.0;
            out[k] = (short)(v > 1.0 ? 32767 : v < -1.0 ? -32768 : lrint(v * 32767.0));
        }
    }
    f = fopen(argv[3], "wb");
    if (!f)
        return 1;
    fwrite("RIFF", 1, 4, f); le32(f, 36ul + 2ul * (unsigned long)n); fwrite("WAVEfmt ", 1, 8, f);
    le32(f, 16); le16(f, 1); le16(f, 1); le32(f, 44100); le32(f, 88200); le16(f, 2); le16(f, 16);
    fwrite("data", 1, 4, f); le32(f, 2ul * (unsigned long)n);
    fwrite(out, 2, (size_t)n, f);
    fclose(f);
    return 0;
}
