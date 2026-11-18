/* SPDX-License-Identifier: GPL-3.0-only */
/* PERF-1 0.16.26, Form: Song moves a piece through its own drum patterns (the genre's bars apart, a fill before each change) while the player's
 * Drum Drift is off; Loop keeps the first pattern; the player's Drum Drift, when set, is the one that plays. Also says how many factory pieces have
 * more than one pattern to move through. gsong */
#include <stdio.h>
#include <stdlib.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static int distinct(uint16_t pats[EN_NPAT][EN_NLANE])
{
    int p, q, l, n = 1;
    for (p = 1; p < 4; p++) {
        int same = 0;
        for (q = 0; q < p && !same; q++) {
            for (l = 0; l < EN_NLANE && pats[p][l] == pats[q][l]; l++)
                ;
            same = l == EN_NLANE;
        }
        if (same)
            break;
        n++;
    }
    return n;
}

/* play genre g's piece pc for `bars` bars with Form `song` (and the player's Drum Drift Change `chg`, Every `every`); the pattern changes, and the
 * patterns seen */
static int run(int g, int pc, int song, int chg, int every, int bars, int *seen)
{
    static int32_t buf[512];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, l, b;
    const uint32_t c0 = en_dd_changes;
    en_init();
    en_genre(g);
    en_piece_g(g, pc, par, &pool);
    en_piece_pats_g(g, pc, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_set(P_DD_CHANGE, chg);
    en_set(P_DD_EVERY, every);
    en_pool(pool);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = pats[p][l];
    en_song = (uint8_t)song;
    en_play(1);
    *seen = 0;
    for (b = 0; b < (int)((float)bars * 4.0f * 60.0f / (float)par[P_TEMPO] * 44100.0f / 256.0f); b++) {
        en_render(buf, 256, 4096);
        *seen |= 1 << en_pat_now;
    }
    return (int)(en_dd_changes - c0);
}

/* (0.17.24) the song keeps its feel: SYNTHWV 02 walks into pattern 4 (index 3), the genre's half-time beat; its kick, snare and clap must play pattern 1's
 * (index 0) while its hats move on, and the kick is heard on every beat throughout. Returns 0 when right; *reached: the song got there */
static int run_feel(int *reached)
{
    static int32_t buf[512];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, l, b, bad = 0;
    const int g = 13, pc = 1;
    en_init();
    en_genre(g);
    en_piece_g(g, pc, par, &pool);
    en_piece_pats_g(g, pc, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = pats[p][l];
    en_song = 1;
    en_play(1);
    *reached = 0;
    for (b = 0; b < (int)(40.0f * 4.0f * 60.0f / (float)par[P_TEMPO] * 44100.0f / 256.0f); b++) {
        en_render(buf, 256, 4096);
        if (en_pat_now == 3) {
            *reached = 1;
            bad |= en_lane_src[0] != 0 || en_lane_src[1] != 0 || en_lane_src[2] != 0 || en_lane_src[3] != 3;
            if (en_lane_pos[0] == 15)            /* a whole bar played: the kick on all four beats */
                bad |= (en_played[0] & 0x1111u) != 0x1111u;
        }
    }
    return bad;
}

/* (0.17.70) FORM's rests: SYNTHWV 01 in Song for 160 bars; the blocks with a part resting (Song 2, Few rests 1, No rests 0) */
static int run_rests(int rests)
{
    extern int en_host_arrm(void);
    static int32_t buf[512];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, l, b, n = 0;
    const int g = 13, pc = 0;
    en_init();
    en_genre(g);
    en_piece_g(g, pc, par, &pool);
    en_piece_pats_g(g, pc, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_pool(pool);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = pats[p][l];
    en_song = 1;
    en_rests = (uint8_t)rests;
    en_play(1);
    for (b = 0; b < (int)(160.0f * 4.0f * 60.0f / (float)par[P_TEMPO] * 44100.0f / 256.0f); b++) {
        en_render(buf, 256, 4096);
        n += en_host_arrm() != 0;
    }
    en_rests = 2;
    return n;
}

int main(void)
{
    int g, pc, multi = 0, total = 0, fails = 0, seen, n, gm = -1, pm = 0;
    for (g = 0; g < EN_NGENRE; g++)
        for (pc = 0; pc < 10; pc++) {
            int16_t par[P_NPARAMS];
            uint16_t pool, pats[EN_NPAT][EN_NLANE];
            en_piece_g(g, pc, par, &pool);
            en_piece_pats_g(g, pc, pats);
            total++;
            if (distinct(pats) > 1) {
                multi++;
                if (gm < 0 && g == 2)
                    gm = g, pm = pc;
            }
        }
    printf("gsong: %d of %d factory pieces have more than one pattern to move through\n", multi, total);
    if (gm < 0)
        gm = 2, pm = 0;
    n = run(gm, pm, 1, 0, 2, 40, &seen);
    printf("gsong: Song, %s %02d, 40 bars: %d changes, patterns seen 0x%X\n", en_genre_name(gm), pm + 1, n, seen);
    fails += n < 3 || n > 6 || seen == 1;
    n = run(gm, pm, 0, 0, 2, 40, &seen);
    printf("gsong: Loop, 40 bars: %d changes, patterns seen 0x%X\n", n, seen);
    fails += n != 0 || seen != 1;
    n = run(gm, pm, 1, 2, 1, 40, &seen);                   /* the player's Drum Drift: Random every 4 bars */
    printf("gsong: Song with the player's Drum Drift (Random, 4 bars), 40 bars: %d changes\n", n);
    fails += n < 8;
    {
        int reached, bad = run_feel(&reached);
        printf("gsong: Song keeps its feel (SYNTHWV 02 into its half-time pattern): %s\n", !reached ? "NOT REACHED" : bad ? "WRONG" : "kick, snare, clap kept, hats moved");
        fails += !reached || bad;
    }
    {
        const int r2 = run_rests(2), r1 = run_rests(1), r0 = run_rests(0);
        printf("gsong: rests, SYNTHWV 01, 160 bars: blocks with a part resting: Song %d, Few rests %d, No rests %d\n", r2, r1, r0);
        fails += r2 <= 0 || r1 >= r2 || r0 > 20;   /* (No rests: none from the first bar line; before it, the run before's, under a bar) */
    }
    printf(fails ? "gsong: FAILED\n" : "gsong: ok\n");
    return fails ? 1 : 0;
}
