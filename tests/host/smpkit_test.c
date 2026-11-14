/* SPDX-License-Identifier: GPL-3.0-only */
/* (0.17.36) the sampled kits: each of their six drums sounds, on a fresh engine (amb_test's later checks leave the drums gated), a hit's peak well
 * above silence and at most a little past the synth's, so a sampled kit plays at the synth kits' level; and STOP thaws a freeze (a frozen reverb rang
 * on after the music stopped, the maker's log 2026-11-14). */
#include <math.h>
#include <stdio.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static int32_t buf[512];
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void)
{
    int k, d, blk, j;
    for (k = EN_NKIT; k < EN_NKITS; k++)
        for (d = 0; d < EN_NDRUM; d++) {
            float pk = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_ACID, 0);
            en_set(P_CM_LVL, 0);
            en_set(P_NOISE, 0);
            en_set(P_KIT_KICK, k);
            en_set(P_KIT_SNARE, k);
            en_set(P_KIT_HAT, k);
            en_set(P_KIT_PERC, k);
            en_set(P_LANE_LVL(d), 100);
            for (blk = 0; blk < 35; blk++)
                en_render(buf, 256, 2048);
            en_drum_tap(d, 127);
            for (blk = 0; blk < 60; blk++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++)
                    pk = fmaxf(pk, fabsf((float)buf[j] / 8388608.0f));
            }
            CHECK(pk > 0.01f && pk < 0.6f, "sampled kit %s, drum %d: peak %.4f", EN_KIT_NAME[k], d, (double)pk);
        }
    printf("the sampled kits: %d, every drum sounds\n", EN_NSKIT);
    en_init();
    en_play(1);
    for (blk = 0; blk < 200; blk++)
        en_render(buf, 256, 2048);
    en_freeze(1);
    for (blk = 0; blk < 20; blk++)
        en_render(buf, 256, 2048);
    CHECK(en_frozen == 1, "the freeze on");
    en_play(0);
    for (blk = 0; blk < 4; blk++)
        en_render(buf, 256, 2048);
    CHECK(en_frozen == 0, "STOP thaws the freeze");
    printf(fails ? "smpkit: %d FAILED\n" : "smpkit: ok\n", fails);
    return fails != 0;
}
