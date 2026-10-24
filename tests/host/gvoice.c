/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool for tests/web_voice_test.js: pseudo-random packed DX7 voices (128 bytes, 7-bit values), each with the firmware's unpacked form (155 bytes: the
 * values clamped to their ranges, the name made printable) and what the firmware packs that back to. gvoice > voices.json */
#include <stdio.h>
#include <stdint.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static uint32_t rng = 0x12345678u;
static uint32_t next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

int main(void)
{
    int n, k;
    printf("[\n");
    for (n = 0; n < 300; n++) {
        uint8_t b[128], v[156], b2[128];
        for (k = 0; k < 128; k++)
            b[k] = (uint8_t)(next() & 0x7Fu);
        if (n & 1)                                   /* half of them plausible: the small fields small */
            for (k = 0; k < 6; k++) {
                b[k * 17 + 11] &= 0x0F;
                b[k * 17 + 12] &= 0x7F;
                b[k * 17 + 13] &= 0x1F;
                b[k * 17 + 14] %= 100;
            }
        en_voice_unpack(b, v);
        en_voice_pack(v, b2);
        printf("%s{\"p\":[", n ? ",\n" : "");
        for (k = 0; k < 128; k++)
            printf("%s%d", k ? "," : "", b[k]);
        printf("],\"u\":[");
        for (k = 0; k < 155; k++)
            printf("%s%d", k ? "," : "", v[k]);
        printf("],\"r\":[");
        for (k = 0; k < 128; k++)
            printf("%s%d", k ? "," : "", b2[k]);
        printf("]}");
    }
    printf("\n]\n");
    return 0;
}
