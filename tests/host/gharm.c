/* SPDX-License-Identifier: GPL-3.0-only */
/* PERF-1 0.16.26, the musicality pass: every genre's pieces, played for a while each, must never walk to a diminished chord other than home (a genre's
 * own move once could, house's i-VI in Dorian; counted inside the engine, en_host_dimwalk, as the chord moves), and the acid must never stress a major
 * chord's fourth (accented: velocity 120 on its channel). A progression set by hand with a diminished chord must still play it (the count's own check).
 * gharm [seconds a piece] [pieces a genre] */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

extern int en_host_dimwalk;
static int maj_root = -1, bad_acid, acid_acc;          /* the chord now: its root if it is major (else -1) */

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st == 0x93u && d2 == 120u) {                   /* an accented acid note */
        acid_acc++;
        if (maj_root >= 0 && ((int)d1 - maj_root + 120) % 12 == 5)
            bad_acid++;
    }
}

static void load(int g, int pc, int16_t *par, uint16_t *pool)
{
    uint16_t pats[EN_NPAT][EN_NLANE];
    int p, l;
    en_init();
    en_genre(g);
    en_piece_g(g, pc, par, pool);
    en_piece_pats_g(g, pc, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_set(P_MIDI, 1);
    en_pool(*pool);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = pats[p][l];
}

int main(int argc, char **argv)
{
    static int32_t buf[512];
    const int secs = argc > 1 ? atoi(argv[1]) : 40, npc = argc > 2 ? atoi(argv[2]) : 10;
    int g, pc, b, fails = 0, walked = 0;
    for (g = 0; g < EN_NGENRE; g++)
        for (pc = 0; pc < npc; pc++) {
            int16_t par[P_NPARAMS];
            uint16_t pool;
            load(g, pc, par, &pool);
            en_play(1);
            en_host_dimwalk = 0;
            for (b = 0; b < secs * 44100 / 256; b++) {
                int k, has3 = 0, has4 = 0;
                const int r = en_chord_root % 12;
                en_render(buf, 256, 4096);
                for (k = 0; k < EN_NPAD; k++) {
                    const int iv = (en_chord[k] - r + 120) % 12;
                    has3 |= iv == 3;
                    has4 |= iv == 4;
                }
                maj_root = has4 && !has3 ? r : -1;
            }
            if (en_host_dimwalk && walked++ < 8)
                printf("  %s %02d walked to a diminished chord %d times\n", en_genre_name(g), pc + 1, en_host_dimwalk);
        }
    printf("gharm: %d pieces walked to a diminished chord; %d accented acid notes, %d on a major chord's fourth\n", walked, acid_acc, bad_acid);
    fails += walked > 0 || bad_acid > 0 || acid_acc < 100;
    {   /* the count's own check: HOUSE 01 (A Dorian's vi is F#-A-C) with a progression of i and vi set by hand, one bar each, must reach vi */
        int16_t par[P_NPARAMS];
        uint16_t pool;
        uint8_t prg[EN_PRG_LEN] = {0};
        load(2, 0, par, &pool);
        en_set(P_MODE, 1);
        en_pool(0x6ADu);                               /* Dorian: 0 2 3 5 7 9 10 */
        en_prg_put(prg, 0, 3, 1);
        en_prg_put(prg, 3, 3, 6);
        en_prg_put(prg, EN_PRG_PLAY, 1, 1);
        en_prg_put(prg, EN_PRG_UNIT, 2, 1);
        en_prog(prg);
        en_play(1);
        en_host_dimwalk = 0;
        for (b = 0; b < 20 * 44100 / 256; b++)
            en_render(buf, 256, 4096);
        printf("gharm: a progression set by hand with vi in Dorian: %d moves to it\n", en_host_dimwalk);
        fails += en_host_dimwalk == 0;
    }
    printf(fails ? "gharm: FAILED\n" : "gharm: ok\n");
    return fails ? 1 : 0;
}
