/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): the DX patches: name, algorithm, feedback, each operator's coarse / fine / level, and how alike each pair is (the bytes of the voice that match). */
#include "../../firmware/src/dsp/amb.c"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

int main(void)
{
    static uint8_t v[FM6_NPATCH][FP_SIZE + 1];
    int i, j, k;
    fm_init();
    for (i = 0; i < (int)FM6_NPATCH; i++) {
        fm_cur = -1;
        fm_set_patch(i);
        for (k = 0; k < FP_SIZE + 1; k++)
            v[i][k] = fm_ed[k];
        printf("%2d %-10.10s alg %2d fb %d  ops(coarse/OL):", i, (const char *)&v[i][FP_NAME], v[i][FP_ALG] + 1, v[i][FP_FB]);
        for (k = 0; k < 6; k++)
            printf(" %d/%d", v[i][k * FP_OP + FP_FC], v[i][k * FP_OP + FP_OL]);
        printf("\n");
    }
    printf("\nalike (matching voice bytes, of 126 operator bytes):\n");
    for (i = 0; i < (int)FM6_NPATCH; i++)
        for (j = i + 1; j < (int)FM6_NPATCH; j++) {
            int same = 0;
            for (k = 0; k < 126; k++)
                same += v[i][k] == v[j][k];
            if (same > 70 || v[i][FP_ALG] == v[j][FP_ALG] && same > 55)
                printf("  %2d %-10.10s with %2d %-10.10s: %d, alg %d / %d\n", i, (const char *)&v[i][FP_NAME], j, (const char *)&v[j][FP_NAME], same, v[i][FP_ALG] + 1, v[j][FP_ALG] + 1);
        }
    return 0;
}
