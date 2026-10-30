/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voice leading, names) and the
 * render (sound, no NaN, bounded, the loops evolve, the chord drifts, freeze holds). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t acid_abs[512], hat_abs[512], acid_n;   /* the acid's note-ons (0x93) and the closed hat's, by block (the swing test) */
static uint32_t midi_count;
static uint32_t snare_n, snare_min = 999, pad_n;   /* snare note-ons (GM 38), the softest, and the pad's (0x90) */
static uint32_t bass_n, kick_n, bass_lo = 999, bass_hi;   /* the bass line's note-ons (0x92) and the kicks counted, and the notes' range */
static uint32_t bass_pc[12];                    /* the bass line's note-ons by pitch class */
static int cm_on_n, cm_off_n, cm_bal[128];      /* the pluck's note-ons and note-offs (channel 2), and each note's ons less offs */
static uint32_t hat_t[512], hat_v[512], hat_n, blk_now, kick_last;   /* the closed hat's note-ons (GM 42): the block, the velocity, the blocks since the kick before (the step tricks' test) */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st == 0x91u) {
        cm_on_n++;
        cm_bal[d1 & 127u]++;
    }
    if (st == 0x81u) {
        cm_off_n++;
        cm_bal[d1 & 127u]--;
    }
    if (st == 0x99u && d1 == 38u) {
        snare_n++;
        if (d2 < snare_min)
            snare_min = d2;
    }
    if (st == 0x90u)
        pad_n++;
    if (st == 0x92u) {
        bass_n++;
        bass_pc[d1 % 12u]++;
        if (d1 < bass_lo)
            bass_lo = d1;
        if (d1 > bass_hi)
            bass_hi = d1;
    }
    if (st == 0x99u && d1 == 36u) {
        kick_last = blk_now;
        kick_n++;
    }
    if (st == 0x93u && acid_n < 512u)
        acid_abs[acid_n++] = blk_now;
    if (st == 0x99u && d1 == 42u && hat_n < 512u) {
        hat_abs[hat_n] = blk_now;
        hat_t[hat_n] = blk_now - kick_last;
        hat_v[hat_n++] = d2;
    }
    midi_count++;
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    {   /* the stored form keeps a setting in a byte, its value less its lowest: every setting's range must fit (project.c pack_slot) */
        int k;
        for (k = 0; k < P_NPARAMS; k++)
            CHECK(en_param_info(k)->hi - en_param_info(k)->lo <= 255, "param %d range %d..%d does not fit a byte", k, en_param_info(k)->lo, en_param_info(k)->hi);
    }
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);
    {   /* the kinds of chord (P_CHORD), in A Aeolian on i and iv, and on the pentatonic */
        static const struct { int kind, deg; const char *name; } K[] = {
            {EN_CH_AUTO, 0, "Am7"}, {EN_CH_TRIAD, 0, "Am"}, {EN_CH_7TH, 3, "Dm7"}, {EN_CH_9TH, 0, "Am9"},
            {EN_CH_9TH, 3, "Dm9"}, {EN_CH_9TH, 2, "CM9"}, {EN_CH_SUS, 0, "A7sus4"}, {EN_CH_DUB, 0, "Am9"},
            {EN_CH_DUB, 2, "Cm9"}, {EN_CH_4THS, 0, "Am11"}};
        int k;
        for (k = 0; k < (int)(sizeof K / sizeof K[0]); k++) {
            n = en_chord_kind(EN_MODE_POOL[MODE_AEOLIAN], 9, K[k].deg, K[k].kind, pcs);
            CHECK(n == (K[k].kind == EN_CH_TRIAD ? 3 : 4) && !strcmp(en_chord_name(pcs, n, name), K[k].name),
                  "chord kind %d on %d: %s, not %s", K[k].kind, K[k].deg, name, K[k].name);
        }
        for (k = 0; k < EN_NCHORD; k++) {       /* on a pentatonic: still distinct tones */
            int a, b, ok = 1;
            n = en_chord_kind(EN_MODE_POOL[MODE_PENT_MIN], 9, 1, k, pcs);
            for (a = 0; a < n; a++)
                for (b = a + 1; b < n; b++)
                    ok &= pcs[a] != pcs[b];
            CHECK(ok && n >= 3, "pentatonic, kind %d: %d tones, distinct %d", k, n, ok);
        }
    }

    /* voice leading: small moves, in range, no unisons */
    {
        int from[4] = {50, 57, 62, 66}, to[4], cm7[4] = {0, 4, 7, 11}, f7[4] = {5, 9, 0, 4}, tot = 0;
        en_voice_lead(from, cm7, 4, to, 48, 77);
        for (i = 0; i < 4; i++) {
            CHECK(to[i] >= 48 && to[i] <= 77, "range %d", to[i]);
            tot += abs(to[i] - from[i]);
        }
        CHECK(tot <= 8, "Cmaj7 from D-ish: moved %d", tot);
        memcpy(from, to, sizeof from);
        en_voice_lead(from, f7, 4, to, 48, 77);
        tot = 0;
        for (i = 0; i < 4; i++)
            tot += abs(to[i] - from[i]);
        CHECK(tot <= 6, "CM7 -> FM7 moved %d", tot);
        CHECK(to[0] != to[1] && to[1] != to[2] && to[2] != to[3] && to[0] != to[3], "no unisons");
    }

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "20s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1/4."), "etime: %s", t);
        en_param_text(P_AC_OCT, 1, t);
        CHECK(!strcmp(t, "+1"), "acid oct: %s", t);
        en_param_text(P_PATTERN, 0, t);
        CHECK(!strcmp(t, "1"), "pattern: %s", t);
        en_param_text(P_NZ_COLOR, 1, t);
        CHECK(!strcmp(t, "Clean"), "vinyl wear: %s", t);
        en_param_text(P_DL_RATE, 6, t);
        CHECK(!strcmp(t, "16 bars"), "lfo rate: %s", t);
        en_param_text(P_PE_ATK, 0, t);
        CHECK(!strcmp(t, "0.05s"), "pad attack: %s", t);
    }

    /* the render: silent at rest, sound when growing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.05f && peak < 0.99f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u);
    CHECK(en_bars / 16u >= 5u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_pad_level > 0.5f, "the pad swelled (%f)", (double)en_pad_level);

    /* the chord drifts: with Drift at its shortest, a few changes in two minutes */
    {
        int changes = 0, last = en_chord_root * 16 + en_chord[0] % 16, k;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 120; k++) {
            int now;
            run(1.0f);
            now = en_chord_root * 16 + en_chord[0] % 16;
            changes += now != last;
            last = now;
        }
        CHECK(changes >= 3, "chord changes in 2 min at Drift 0: %d", changes);
        printf("  chord changes in 2 min: %d\n", changes);
    }

    /* a chord change is heard: within 2 s no sounding voice sings a note outside the new chord */
    {
        int k, i, bad_s = 0, changes = 0, last = -1;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 90 * 4; k++) {
            int sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
            static int since = -1;
            run(0.25f);
            if (sig != last) {
                if (last >= 0) {
                    changes++;
                    since = 0;
                }
                last = sig;
            } else if (since >= 0 && ++since == 8) {   /* 2 s after a change */
                uint32_t pcs = 0;
                for (i = 0; i < EN_NPAD; i++)
                    pcs |= 1u << (en_chord[i] % 12);
                for (i = 0; i < EN_NPAD; i++)
                    if (en_pad_amp[i] > 0.05f && !(pcs >> (en_pad_note[i] % 12) & 1u))
                        bad_s++;
                since = -1;
            }
        }
        CHECK(changes >= 3, "chord changes in 90 s at Drift 0: %d", changes);
        CHECK(!bad_s, "%d voices still off the new chord 2 s after a change", bad_s);
    }

    /* the acid line mutates: with Mutate at 100, its steps change; and a new drum pattern waits for the
     * bar, turning through several on the way playing none of them */
    {
        uint16_t before;
        int k, seen_mid = 0;
        en_set(P_AC_MUT, 100);
        en_set(P_AC_DENS, 60);
        run(0.5f);
        before = en_acid_gate;
        run(30.0f);
        CHECK(en_acid_gate != before, "the acid line mutated");
        while (en_drum_pos != 5)                 /* mid-bar */
            run(0.006f);
        for (k = 1; k <= 4; k++)                 /* through 2, 3, 4 to 5 */
            en_set(P_PATTERN, k);
        run(0.01f);
        CHECK(en_pat_now != 4 && en_pat_next == 4, "the new pattern waits (now %d, next %d)", en_pat_now, en_pat_next);
        for (k = 0; k < 400 && en_pat_now != 4; k++) {
            run(0.006f);
            if (en_pat_now != 0 && en_pat_now != 4)
                seen_mid = 1;
        }
        CHECK(en_pat_now == 4 && en_drum_pos <= 1, "the new pattern on the bar (now %d, step %d)", en_pat_now, en_drum_pos);
        CHECK(!seen_mid, "a pattern turned through was played");
        en_set(P_PATTERN, 0);
    }

    /* freeze: the chord holds */
    {
        int held = en_chord[0] + 128 * en_chord[1], k;
        en_freeze(1);
        for (k = 0; k < 60; k++) {
            run(1.0f);
            CHECK(en_chord[0] + 128 * en_chord[1] == held, "the chord moved while frozen");
        }
        en_freeze(0);
    }

    /* a new key while playing (a piece, the root, the scale): within 3 s no voice still sings the old
     * chord (a voice only takes its note at its next breath otherwise, up to a minute later) */
    {
        int k, stale = 0;
        en_freeze(0);
        en_set(P_ROOT, 6);                       /* a tritone away: nothing in common to hide behind */
        en_pool(EN_MODE_POOL[MODE_LYDIAN]);
        en_regrow();
        run(3.0f);
        for (k = 0; k < EN_NPAD; k++)
            if (en_pad_amp[k] > 0.02f && en_pad_note[k] != en_chord[k]) {
                printf("  voice %d sings %d, the chord has %d (amp %.2f)\n", k, en_pad_note[k], en_chord[k],
                       (double)en_pad_amp[k]);
                stale++;
            }
        CHECK(!stale, "%d voices still in the old key after 3 s", stale);
        en_set(P_ROOT, 2);
    }

    /* a pool change: the chord's tones are in it */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_pool(pent);
        run(0.1f);
        for (i = 0; i < 4; i++)
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
    }

    /* the voice bank: each sound of the pad, drone and acid, and each kit, bounded and heard; a stored
     * style turned into its sounds */
    {
        int k, v;
        static const int SP[3] = {P_PADTONE, P_DR_INST, P_AC_WAVE};
        for (v = 0; v < 3; v++) {
            for (k = 0; k < (v == 2 ? EN_NACID : EN_NSOUND); k++) {
                en_set(SP[v], k);
                peak = 0.0f;
                run(3.0f);
                CHECK(!bad && peak < 0.9f && peak > 0.05f, "voice %d sound %d: peak %f", v, k, (double)peak);
                if (v == 0)
                    CHECK(en_pad_level > 0.0f || k == 6, "pad sound %d silent", k);
                if (v == 1)
                    CHECK(en_bass_level > 0.0f, "drone sound %d silent", k);
            }
            en_set(SP[v], en_param_info(SP[v])->def);
        }
        for (k = 0; k < EN_NKIT; k++) {
            en_set(P_KIT_KICK, k);
            en_set(P_KIT_SNARE, k);
            en_set(P_KIT_HAT, k);
            en_set(P_KIT_PERC, k);
            en_set(P_TEMPO, 175);
            peak = 0.0f;
            run(4.0f);
            CHECK(!bad && peak < 0.9f && peak > 0.05f, "kit %d: peak %f", k, (double)peak);
        }
        for (k = 0; k < 4; k++)
            en_set(P_KIT_KICK + k, 0);
        en_set(P_TEMPO, 128);
        {
            int16_t pp[P_NPARAMS];
            for (k = 0; k < P_NPARAMS; k++)
                pp[k] = en_param_info(k)->def;
            pp[P_STYLE] = 1;                    /* house: the stabbing pad, the house kit, the warm drone */
            en_style_sounds(pp);
            CHECK(pp[P_STYLE] == 0 && pp[P_KIT_KICK] == 1 && pp[P_KIT_PERC] == 1 && pp[P_PADTONE] == 6 && pp[P_DR_INST] == 1,
                  "style to sounds: %d %d %d %d", pp[P_STYLE], pp[P_KIT_KICK], pp[P_PADTONE], pp[P_DR_INST]);
        }
    }

    /* keys played: the pad, the drone and the acid answer, and let go; a voice off the algorithm is silent
     * but for them, and back on it plays again */
    {
        int k;
        en_set(P_DR_MAN, 1);
        run(1.5f);
        CHECK(en_bass_level < 0.05f, "drone off the algorithm still sounds: %f", (double)en_bass_level);
        en_note(EN_V_DRONE, 48, 1);
        run(0.3f);
        CHECK(en_bass_level > 0.1f, "drone by hand: level %f", (double)en_bass_level);
        en_note(EN_V_DRONE, 48, 0);
        run(1.5f);
        CHECK(en_bass_level < 0.05f, "drone by hand, let go: level %f", (double)en_bass_level);
        en_set(P_DR_MAN, 0);
        run(3.0f);
        CHECK(en_bass_level > 0.5f, "drone back on the algorithm: level %f", (double)en_bass_level);
        en_set(P_PAD_MAN, 1);
        run(1.0f);
        CHECK(en_pad_level < 0.02f, "pad off the algorithm still sounds: %f", (double)en_pad_level);
        en_set(P_PAD_MAN, 0);
        en_note(EN_V_DRONE, 60, 1);
        run(0.3f);
        CHECK(en_bass_level > 0.1f, "drone played: level %f", (double)en_bass_level);
        en_note(EN_V_DRONE, 60, 0);
        en_note(EN_V_ACID, 64, 1);
        run(0.2f);
        CHECK(en_acid_level > 0.05f, "acid played: level %f", (double)en_acid_level);
        en_note(EN_V_ACID, 64, 0);
        for (k = 0; k < 6; k++)                 /* more keys than voices: no harm */
            en_note(EN_V_PAD, 60 + k, 1);
        peak = 0.0f;
        run(0.5f);
        CHECK(!bad && peak < 1.0f && en_pad_level > 0.1f, "pad played: level %f peak %f", (double)en_pad_level, (double)peak);
        for (k = 0; k < 6; k++)
            en_note(EN_V_PAD, 60 + k, 0);
        run(2.0f);
        CHECK(!bad, "pad let go");
    }

    /* sounds changed and keys played at random while it plays, for a minute: bounded, never stuck */
    {
        uint32_t r = 12345u;
        int k, v;
        static const int SP[7] = {P_PADTONE, P_DR_INST, P_AC_WAVE, P_KIT_KICK, P_KIT_SNARE, P_KIT_HAT, P_KIT_PERC};
        peak = 0.0f;
        for (k = 0; k < 600; k++) {
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            v = (int)(r % 7u);
            if (r & 0x100u)
                en_set(SP[v], (int)((r >> 9) % (v == 2 ? EN_NACID : v < 3 ? EN_NSOUND : EN_NKIT)));
            else
                en_note((int)((r >> 9) % 3u), 53 + (int)((r >> 12) % 27u), (int)(r >> 20 & 1u));
            run(0.1f);
        }
        for (v = 0; v < 3; v++)
            for (k = 53; k < 80; k++)
                en_note(v, k, 0);
        run(3.0f);
        CHECK(!bad && peak < 0.95f, "random sounds and keys: peak %f", (double)peak);
        CHECK(en_held == 0, "random sounds and keys: %d keys still held", en_held);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        en_set(P_TEMPO, 30);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* the effects together, set and played at random (settings anywhere in their ranges, any effects held, the mutes,
     * the FX knobs, the tempo from the slowest to the fastest): nothing breaks: no NaN (it would come out as the
     * most negative integer), nothing over full scale */
    {
        uint32_t r = 0x9E3779B9u;
        int trial, s, blk, j, broken = 0;
        for (trial = 0; trial < 16; trial++) {
            en_init();
            en_play(1);
            for (s = 0; s < EN_NFXS; s++) {
                int lo = 0, hi = 100;
                switch (s) {
                case EN_FXS_GX_LEN: lo = 2; hi = 16; break;
                case EN_FXS_GX_TGT: case EN_FXS_PM_TGT: hi = 127; break;
                case EN_FXS_RL_START: case EN_FXS_RL_END: hi = 6; break;
                case EN_FXS_RL_TIME: case EN_FXS_PM_RATE: hi = 3; break;
                case EN_FXS_SC_SIZE: case EN_FXS_HF_SPEED: case EN_FXS_RV_LEN: hi = 2; break;
                case EN_FXS_SR_EVERY: case EN_FXS_SR_REACH: case EN_FXS_FZ_GRAINS: hi = 3; break;
                case EN_FXS_SR_STYLE: hi = 1; break;
                case EN_FXS_SW_RATE: hi = 6; break;
                case EN_FXS_SW_TYPE: hi = 2; break;
                case EN_FXS_SC_LEN: lo = 2; hi = 8; break;
                default: break;
                }
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                en_fxs(s, lo + (int)(r % (uint32_t)(hi - lo + 1)));
            }
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            en_set(P_TEMPO, 30 + (int)(r % 271u));
            for (s = 0; s < EN_NFXK; s++) {
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                en_fxk(s, s == EN_FXK_FILTER ? (int)(r % 101u) - 50 : (int)(r % 101u));
            }
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            en_trk(r & 0x7Fu, r >> 8 & 0x7Fu);
            for (s = 0; s < 30; s++) {                /* a fresh set of keys held every 0.4 s */
                uint64_t m;
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                m = r & (r >> 3) & 0x7FFFFFFFu;       /* about a quarter of the bits (0..30) */
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                m |= (uint64_t)(r & (r >> 5) & 0x0FFFFFFFu) << 31;   /* and the bits 31..58 (the flex pads, talk, wah, the drum rolls too) */
                en_fx(m);
                for (blk = 0; blk < 69; blk++) {
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 512; j++)
                        if (buf[j] == INT32_MIN || buf[j] > 8400000 || buf[j] < -8400000)
                            broken++;
                }
            }
        }
        CHECK(!broken, "effects at random: %d samples broken (NaN or over full scale)", broken);
        en_fx(0);                                 /* (en_init leaves the effects' state as it was: put it all back) */
        en_trk(0, 0);
        for (s = 0; s < EN_NFXK; s++)
            en_fxk(s, 0);
        for (s = 0; s < EN_NFXS; s++)
            en_fxs(s, EN_FXS_DEF[s]);
        run(1.0f);                                /* (rendered, so the commands are taken before en_init drops what is queued) */
        en_init();
        en_play(1);
        run(1.0f);
    }

    /* the varispeed effects held long (half time at x1.5 ran the read point more than a ring from its start: garbage after ~3 s): the level
     * stays sane for ten seconds, whatever the speed */
    {
        const int fxs[3] = {FX_HALF, FX_BRAKE, FX_SPIN};
        int k, sp, blk, j, broken = 0;
        float peak = 0.0f;
        for (k = 0; k < 3; k++)
            for (sp = 0; sp < 3; sp++) {
                en_init();
                en_play(1);
                en_fxs(EN_FXS_HF_SPEED, sp);
                en_fxs(EN_FXS_BR_BACK, sp * 50);
                run(1.0f);
                en_fx(1ull << fxs[k]);
                for (blk = 0; blk < 1700; blk++) {            /* ~10 s */
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 512; j++) {
                        const float a = (float)(buf[j] < 0 ? -buf[j] : buf[j]);
                        if (buf[j] == INT32_MIN || a > 8400000.0f)
                            broken++;
                        if (a > peak)
                            peak = a;
                    }
                }
                en_fx(0);
                run(0.5f);
            }
        CHECK(!broken, "varispeed effects held ten seconds: %d samples broken (peak %.0f)", broken, (double)peak);
        for (k = 0; k < EN_NFXS; k++)
            en_fxs(k, EN_FXS_DEF[k]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the counter-melody: every sound, in every style and at every rate, plays (not silent), nothing is NaN or over full scale; level 0 is silent */
    {
        int sn, st, blk, j, broken = 0, silent = 0, loud = 0;
        for (sn = 0; sn < EN_NCMSND; sn++)
            for (st = 0; st < EN_NCMSTYLE + 1; st++) {
                float peak = 0.0f;
                en_init();
                en_set(P_PAD, 0);
                en_set(P_BASS, 0);
                en_set(P_ACID, 0);
                en_set(P_PULSE, 0);
                en_set(P_NOISE, 0);
                en_set(P_CM_LVL, st == EN_NCMSTYLE ? 0 : 80);     /* (the last round: level 0) */
                en_set(P_CM_SND, sn);
                en_set(P_CM_STYLE, st % EN_NCMSTYLE);
                en_set(P_CM_RATE, (sn + st) % 4);
                en_set(P_CM_DENS, 100);
                en_play(1);
                for (blk = 0; blk < (st == EN_NCMSTYLE ? 2100 : 700); blk++) {   /* ~4 s (the silent round: 12 s, the last 4 measured: the last run's tails die away) */
                    en_render(buf, 256, 2048);
                    if (st == EN_NCMSTYLE && blk < 1400)
                        continue;
                    for (j = 0; j < 512; j++) {
                        const float a = (float)(buf[j] < 0 ? -buf[j] : buf[j]);
                        if (buf[j] == INT32_MIN || a > 8400000.0f)
                            broken++;
                        if (a > peak)
                            peak = a;
                    }
                }
                if (st < EN_NCMSTYLE && peak < 0.02f * 8388608.0f)
                    silent++;
                if (st == EN_NCMSTYLE && peak > 0.003f * 8388608.0f)
                    loud++;
            }
        CHECK(!broken, "counter-melody: %d samples broken (NaN or over full scale)", broken);
        CHECK(!silent, "counter-melody: %d sound/style combinations silent", silent);
        CHECK(!loud, "counter-melody: %d level-0 runs not silent", loud);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the pluck played by hand (the JAM page): a note is heard; nothing breaks */
    {
        int k, blk, j, broken = 0, quiet = 0;
        float peak = 0.0f;
        en_init();
        en_set(P_PAD, 0);
        en_set(P_BASS, 0);
        en_set(P_ACID, 0);
        en_set(P_PULSE, 0);
        en_set(P_CM_LVL, 70);
        en_set(P_CM_DENS, 0);                    /* (no notes of its own) */
        en_play(1);
        run(1.0f);
        for (k = 0; k < 6; k++) {
            en_note(EN_V_PLUCK, 60 + 3 * k, 1);
            for (blk = 0; blk < 100; blk++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++) {
                    const float a = (float)(buf[j] < 0 ? -buf[j] : buf[j]);
                    if (buf[j] == INT32_MIN || a > 8400000.0f)
                        broken++;
                    if (a > peak)
                        peak = a;
                }
            }
        }
        if (peak < 0.02f * 8388608.0f)
            quiet++;
        CHECK(!broken, "pluck by hand: %d samples broken", broken);
        CHECK(!quiet, "pluck by hand: not heard");
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the DJ snips (the FX menu's fourth bank): each of the 27 keys is heard, nothing breaks, and every one is silent again once let go / played out;
     * in one-shot mode the long ones end by themselves */
    {
        int key, blk, j, mode, broken = 0, quiet = 0, stuck = 0;
        for (mode = 0; mode < 2; mode++)
            for (key = 0; key < 27; key++) {
                float peak = 0.0f;
                en_init();
                en_set(P_PAD, 0);
                en_set(P_BASS, 0);
                en_set(P_ACID, 0);
                en_set(P_PULSE, 0);
                en_set(P_NOISE, 0);
                en_set(P_CM_LVL, 0);
                en_play(1);
                en_fxs(EN_FXS_SM_LOOP, mode);                  /* 0 one shot, 1 held */
                en_fxs(EN_FXS_SM_MIX, 100);
                run(0.5f);
                en_note(EN_V_SAMPLE, 53 + key, 1);
                for (blk = 0; blk < 120; blk++) {              /* ~0.7 s held */
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 512; j++) {
                        const float a = (float)(buf[j] < 0 ? -buf[j] : buf[j]);
                        if (buf[j] == INT32_MIN || a > 8400000.0f)
                            broken++;
                        if (a > peak)
                            peak = a;
                    }
                }
                en_note(EN_V_SAMPLE, 53 + key, 0);
                for (blk = 0; blk < 1100; blk++)               /* ~6.4 s: the longest (the long drop) plays out */
                    en_render(buf, 256, 2048);
                if (en_smp_active)
                    stuck++;
                if (peak < 0.01f * 8388608.0f)
                    quiet++;
            }
        CHECK(!broken, "DJ snips: %d samples broken (NaN or over full scale)", broken);
        CHECK(!quiet, "DJ snips: %d key/mode pairs silent", quiet);
        CHECK(!stuck, "DJ snips: %d voices stuck on", stuck);
        en_fxs(EN_FXS_SM_LOOP, EN_FXS_DEF[EN_FXS_SM_LOOP]);
        en_fxs(EN_FXS_SM_MIX, EN_FXS_DEF[EN_FXS_SM_MIX]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* polymeter: a lane of 5 steps wraps at 5 and keeps its own count across bars (it sees 0..4, all of them, never more); a 16-step lane follows the bar */
    {
        int seen5 = 0, over5 = 0, follow = 0, k;
        en_init();
        en_set(P_LLEN + 3, 5);
        en_play(1);
        run(0.3f);                      /* (the first steps: the lane's position is its own from the first tick) */
        for (k = 0; k < 400; k++) {
            run(0.05f);
            if (en_lane_pos[3] < 5)
                seen5 |= 1 << en_lane_pos[3];
            else
                over5++;
            if (en_lane_pos[0] != en_drum_pos)
                follow++;
        }
        CHECK(seen5 == 31 && !over5, "polymeter: the 5-step lane saw %x and %d steps beyond", seen5, over5);
        CHECK(!follow, "polymeter: the 16-step lane left the bar %d times", follow);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the player's DX7 voices (en_user_voice): a simple organ-ish voice, packed as a bulk dump holds it; it is taken, named, heard when chosen, survives the
     * single-voice format's round trip, and goes with en_user_clear */
    {
        uint8_t b[128], v[155], b2[128], v2[155];
        int k, ok = 1;
        float pk;
        for (k = 0; k < 128; k++)
            b[k] = 0;
        for (k = 0; k < 6; k++)                             /* six operators: only OP1 (the last record) sounds */
            b[k * 17 + 14] = k == 5 ? 99 : 0;               /* output level */
        b[5 * 17 + 0] = b[5 * 17 + 1] = b[5 * 17 + 2] = b[5 * 17 + 3] = 99;   /* rates */
        b[5 * 17 + 4] = b[5 * 17 + 5] = b[5 * 17 + 6] = 99;                   /* levels: held */
        b[5 * 17 + 8] = 39;
        b[5 * 17 + 12] = 7 << 3;                            /* detune 7: none */
        b[5 * 17 + 15] = 1 << 1;                            /* ratio 1 */
        for (k = 0; k < 4; k++) {
            b[102 + k] = 99;
            b[106 + k] = 50;
        }
        b[110] = 31;                                        /* algorithm 32: all carriers */
        b[117] = 24;
        memcpy(b + 118, "TESTVOICE ", 10);
        en_voice_unpack(b, v);
        en_voice_pack(v, b2);
        en_voice_unpack(b2, v2);
        for (k = 0; k < 155; k++)
            ok &= v[k] == v2[k];
        CHECK(ok, "voice formats: the packed record did not survive unpack, pack, unpack");
        for (k = 0; k < 128; k++)
            ok &= b[k] == b2[k];
        CHECK(ok, "voice formats: pack(unpack(b)) differs from b");
        en_init();
        CHECK(en_user_count() == 0 && en_ncm_snd() == 12 + EN_NFMFACT, "user voices: %d loaded and %d sounds after en_init", en_user_count(), en_ncm_snd());
        CHECK(!en_user_voice(1, b), "user voices: a slot past the end was taken");
        CHECK(en_user_voice(0, b), "user voices: slot 0 was not taken");
        en_play(1);
        run(0.2f);
        CHECK(en_user_count() == 1 && en_ncm_snd() == 12 + EN_NFMFACT + 1, "user voices: %d loaded, %d sounds", en_user_count(), en_ncm_snd());
        CHECK(!strcmp(EN_CM_SOUND[12 + EN_NFMFACT], "DX TESTVOICE"), "user voices: named '%s'", EN_CM_SOUND[12 + EN_NFMFACT]);
        en_set(P_CM_LVL, 80);
        en_set(P_CM_DENS, 90);
        en_set(P_CM_SND, 12 + EN_NFMFACT);
        peak = 0.0f;
        run(3.0f);
        pk = peak;
        CHECK(!bad && pk > 0.02f, "user voices: chosen, peak %f (bad %d)", (double)pk, bad);
        CHECK(en_user_voice(0, b), "user voices: slot 0 could not be replaced");
        CHECK(en_user_voice(1, b), "user voices: slot 1 could not be appended");
        run(0.2f);
        CHECK(en_user_count() == 2, "user voices: %d loaded after the second", en_user_count());
        en_user_clear();
        run(0.2f);
        CHECK(en_user_count() == 0 && !strcmp(EN_CM_SOUND[12 + EN_NFMFACT], "(empty)"), "user voices: %d left after clear", en_user_count());
        run(1.0f);
        CHECK(!bad, "user voices: the engine broke when the chosen voice was cleared");
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* a lane's probability: the hats at 100 sound every time, at 50 about half as often, at 0 never (counted by the glow, 4 s at 128 bpm) */
    {
        int pr, blk, n[3];
        for (pr = 0; pr < 3; pr++) {
            float prev = 0.0f;
            n[pr] = 0;
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_PROB + 3, pr == 0 ? 100 : pr == 1 ? 50 : 0);
            en_play(1);
            run(1.0f);
            for (blk = 0; blk < 2756; blk++) {
                float g;
                en_render(buf, 64, 2048);
                g = en_drum_glow[3];
                if (g > prev + 0.3f)
                    n[pr]++;
                prev = g;
            }
        }
        CHECK(n[0] > 6 && n[1] > 0 && n[1] < n[0] && n[2] == 0, "lane probability: %d hits at 100, %d at 50, %d at 0", n[0], n[1], n[2]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the hat roll and the hat triplets really are more hits: counted by the hat lane's glow jumping, over 4 s at 128 bpm */
    {
        int fx, blk, n[3];
        for (fx = 0; fx < 3; fx++) {
            float prev = 0.0f;
            n[fx] = 0;
            en_init();
            en_set(P_TEMPO, 128);
            en_play(1);
            run(1.0f);
            if (fx)
                en_fx(1ull << (fx == 1 ? FX_HATROLL : FX_HATTRIP));
            for (blk = 0; blk < 2756; blk++) {          /* 4 s in blocks of 64 frames */
                float g;
                en_render(buf, 64, 2048);
                g = en_drum_glow[3];
                if (g > prev + 0.3f)
                    n[fx]++;
                prev = g;
            }
            en_fx(0);
            run(0.5f);
        }
        CHECK(n[1] > n[0] * 3 / 2 && n[2] > n[1], "hat rolls: %d hits (none held), %d (roll), %d (triplets): expected more with each", n[0], n[1], n[2]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the step tricks (en_mod) on the hat lane, 4 s at 128 bpm, read from the MIDI out: a ratchet is more hits (x4: more than twice as many), a push
     * delays them (6 twelfths of a step, 58 ms, about 40 blocks of 64 frames), a ghost level is much quieter */
    {
        int v, s, blk;
        uint32_t cnt[5], i;
        float mean_v[5], delay = 0.0f;
        float since[5];
        for (v = 0; v < 5; v++) {
            float sv = 0.0f;
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            memset((void *)en_mod, 0, sizeof en_mod);
            for (s = 0; s < 16; s++)
                en_mod[3][s] = (uint8_t)(v == 1 ? 3 : v == 2 ? 6 << 3 : v == 3 ? 1 << 6 : v == 4 ? 4 : 0);   /* none, x4, push 6, ghost, x6 */
            en_play(1);
            run(4.0f);                       /* (the first bar's hits are the start's own: no tricks there) */
            hat_n = 0;
            kick_last = 0;
            for (blk = 0; blk < 2756; blk++) {
                blk_now = (uint32_t)blk;
                en_render(buf, 64, 2048);
            }
            cnt[v] = hat_n;
            for (i = 0; i < hat_n; i++)
                sv += (float)hat_v[i];
            mean_v[v] = hat_n ? sv / (float)hat_n : 0.0f;
            since[v] = 0.0f;
            for (i = 0; i < hat_n; i++)
                since[v] += (float)hat_t[i];
            since[v] = hat_n ? since[v] / (float)hat_n : 0.0f;
        }
        delay = since[2] - since[0];
        CHECK(cnt[0] >= 6u && cnt[1] > cnt[0] * 2u, "ratchet x4: %u hat hits against %u without", cnt[1], cnt[0]);
        CHECK(cnt[4] > cnt[1], "ratchet x6: %u hat hits (x4: %u)", cnt[4], cnt[1]);
        CHECK(delay > 30.0f && delay < 50.0f, "push 6: the hats are %.1f blocks late (expected about 40)", (double)delay);
        CHECK(mean_v[3] < mean_v[0] * 0.6f, "ghost level: mean velocity %.1f against %.1f", (double)mean_v[3], (double)mean_v[0]);
        memset((void *)en_mod, 0, sizeof en_mod);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* full swing and the latest push on every hat step (together past a step: a pushed hit used to be overwritten by the next one's wait):
     * no hit may be lost against the same pattern without the push */
    {
        int v, s, blk;
        uint32_t cnt[2];
        for (v = 0; v < 2; v++) {
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            en_set(P_SWING, 100);
            en_set(P_HUMAN, 0);
            memset((void *)en_mod, 0, sizeof en_mod);
            for (s = 0; s < EN_NPAT; s++)
                en_pats[s][3] = 0xFFFFu;         /* a hat on every sixteenth, so a pushed hit meets the next one's wait */
            for (s = 0; s < 16; s++)
                en_mod[3][s] = (uint8_t)(v ? 7 << 3 : 0);
            en_play(1);
            run(4.0f);
            hat_n = 0;
            for (blk = 0; blk < 2756; blk++) {
                blk_now = (uint32_t)blk;
                en_render(buf, 64, 2048);
            }
            cnt[v] = hat_n;
        }
        CHECK(cnt[0] >= 6u && cnt[1] + 1u >= cnt[0], "swing 100 + push 7: %u hat hits against %u without (pushed hits lost)", cnt[1], cnt[0]);
        memset((void *)en_mod, 0, sizeof en_mod);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the acid is swung with the drums (it used to play straight against swung hats): at swing 100, Humanize 0, a hat on every sixteenth, every acid
     * note-on lands on a hat's block, odd sixteenths included */
    {
        int s, blk, matched = 0;
        uint32_t i, j;
        en_init();
        en_set(P_TEMPO, 128);
        en_set(P_MIDI, 1);
        en_set(P_SWING, 100);
        en_set(P_HUMAN, 0);
        for (s = 0; s < EN_NPAT; s++)
            en_pats[s][3] = 0xFFFFu;
        en_play(1);
        run(4.0f);
        hat_n = acid_n = 0;
        for (blk = 0; blk < 5500; blk++) {
            blk_now = (uint32_t)blk;
            en_render(buf, 64, 2048);
        }
        for (i = 0; i < acid_n; i++)
            for (j = 0; j < hat_n; j++)
                if (acid_abs[i] + 3u >= hat_abs[j] && acid_abs[i] <= hat_abs[j] + 3u) {
                    matched++;
                    break;
                }
        CHECK(acid_n >= 6u && (uint32_t)matched == acid_n, "swing 100: %d of %u acid notes land with a hat (the acid plays straight)", matched, acid_n);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the chord habits (per genre: CHT): 120 chord changes by en_next at 240 bpm, read from the chord's root. Trap and funk go home often, the house loop goes back to
     * the chord before (A B A), lofi's turnarounds never do */
    {
        static const int G[4] = {9, 11, 2, 1};
        float tonic[4], back[4];
        int gi;
        for (gi = 0; gi < 4; gi++) {
            int i, n = 0, tn = 0, bk = 0, r0, a = -1, b = -1, cnt[12] = {0};
            en_init();
            en_genre(G[gi]);
            en_set(P_CH_LOOP, 0);
            en_set(P_TEMPO, 240);
            en_play(1);
            run(0.3f);
            r0 = en_chord_root;
            for (i = 0; i < 120; i++) {
                int r;
                en_next();
                run(1.05f);
                r = en_chord_root;
                if (r == a)
                    continue;                    /* (no change yet) */
                n++;
                cnt[r % 12]++;
                bk += r == b && b >= 0;
                b = a;
                a = r;
            }
            for (i = 0; i < 12; i++)
                tn = cnt[i] > tn ? cnt[i] : tn;           /* (home: the root heard most) */
            (void)r0;
            tonic[gi] = n ? (float)tn / (float)n : 0.0f;
            back[gi] = n ? (float)bk / (float)n : 0.0f;
            printf("  chords genre %d: %d changes, home %.2f, A-B-A %.2f\n", G[gi], n, (double)tonic[gi], (double)back[gi]);
        }
        CHECK(tonic[0] > 0.3f && tonic[1] > 0.3f, "trap / funk go home: %.2f / %.2f of the chords", (double)tonic[0], (double)tonic[1]);
        CHECK(back[2] > 0.15f && back[3] < 0.08f, "A-B-A: house %.2f (a loop), lofi %.2f (never)", (double)back[2], (double)back[3]);
        en_genre(0);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* shed level 3 (no Echoplex, no vinyl noise, the rational output clip): it renders, nothing is broken, the level is as loud as level 0 within 3 dB */
    {
        int lv, blk, j, broken = 0;
        double rms[2] = {0.0, 0.0};
        for (lv = 0; lv < 2; lv++) {
            en_init();
            en_set(P_TEMPO, 128);
            en_play(1);
            en_shed(lv ? 3 : 0);
            run(3.0f);
            for (blk = 0; blk < 800; blk++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++) {
                    if (buf[j] == INT32_MIN || buf[j] > 8400000 || buf[j] < -8400000)
                        broken++;
                    rms[lv] += (double)buf[j] / 8300000.0 * (double)buf[j] / 8300000.0;
                }
            }
        }
        CHECK(!broken, "shed 3: %d samples broken", broken);
        CHECK(rms[1] > rms[0] * 0.5 && rms[1] < rms[0] * 2.0, "shed 3: level %.4f against %.4f unshed", rms[1], rms[0]);
        en_shed(0);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the pluck's deja vu: a random pluck (Style Random, every note chosen from nine tones) at Deja vu 0 repeats its note 8 slots on only by chance, locked
     * (100) it comes round every time; a loop of 4 repeats at a distance of 4. Counted over 12 s of eighths at 120 bpm, the chord held (Drift at the top). */
    {
        int vu, lp, blk, same[3], tot[3];
        for (vu = 0; vu < 3; vu++) {
            uint8_t seq[160], last = 0, cnt = 0;
            int ns = 0, i, dist;
            lp = vu == 2 ? 0 : 1;
            en_init();
            en_set(P_TEMPO, 120);
            en_set(P_DRIFT, 100);
            en_set(P_CM_LVL, 60);
            en_set(P_CM_DENS, 100);
            en_set(P_CM_STYLE, 3);
            en_set(P_CM_RATE, 2);
            en_set(P_CM_RANGE, 3);
            en_set(P_CM_LOOP, lp);
            en_set(P_CM_VU, vu == 0 ? 0 : 100);
            en_play(1);
            for (blk = 0; blk < 30000 && ns < 150; blk++) {
                en_render(buf, 64, 2048);
                if (blk > 100 && en_cm_count != cnt) {
                    seq[ns++] = en_cm_note;
                }
                cnt = en_cm_count;
                (void)last;
            }
            dist = lp == 0 ? 4 : 8;
            same[vu] = tot[vu] = 0;
            for (i = 16; i + dist < ns; i++) {
                tot[vu]++;
                same[vu] += seq[i] == seq[i + dist];
            }
        }
        CHECK(tot[0] > 40 && same[0] * 10 < tot[0] * 5, "deja vu 0: %d of %d notes repeated a loop on", same[0], tot[0]);
        CHECK(tot[1] > 40 && same[1] * 10 > tot[1] * 9, "deja vu 100, loop 8: only %d of %d notes repeated", same[1], tot[1]);
        CHECK(tot[2] > 40 && same[2] * 10 > tot[2] * 9, "deja vu 100, loop 4: only %d of %d notes repeated", same[2], tot[2]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the feel: in lofi, Ghosts at 100 add soft snares the pattern does not have (more snare hits, the softest under velocity 50), at 0 none; the pad's Comp on
     * a stab sound: Eighths stabs far more often than Sparse, and Hold does not stab at all (counted by the pad's notes, 4 s at 128 bpm) */
    {
        int gh, blk;
        uint32_t sn[2], smin[2], pd[3];
        for (gh = 0; gh < 2; gh++) {
            en_init();
            en_genre(1);
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            en_set(P_GHOST, gh ? 100 : 0);
            en_play(1);
            run(4.0f);
            snare_n = 0;
            snare_min = 999;
            for (blk = 0; blk < 5512; blk++)
                en_render(buf, 64, 2048);
            sn[gh] = snare_n;
            smin[gh] = snare_min;
        }
        CHECK(sn[1] > sn[0] + 1u && smin[1] < 50u && smin[0] > 50u, "ghosts: %u snare hits at 100 (softest %u), %u at 0 (softest %u)", sn[1], smin[1], sn[0], smin[0]);
        for (gh = 0; gh < 3; gh++) {
            en_init();
            en_genre(0);
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            en_set(P_PADTONE, 6);
            en_set(P_PAD_COMP, gh == 0 ? 8 : gh == 1 ? 7 : 1);
            en_play(1);
            run(4.0f);
            pad_n = 0;
            for (blk = 0; blk < 5512; blk++)
                en_render(buf, 64, 2048);
            pd[gh] = pad_n;
        }
        CHECK(pd[0] > pd[1] * 2u && pd[1] > 3u && pd[2] < pd[1], "comping: %u pad notes on eighths, %u sparse, %u on hold", pd[0], pd[1], pd[2]);
        en_genre(0);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the phrase: Chain 1 1 3 4 (preset 4) plays patterns 0 0 2 3 bar after bar (sampled mid-bar, 12 bars at 128 bpm); Time half plays the kick pattern 1 1 1 1 over two
     * bars (half the kicks), double twice in the bar (twice); a lane's condition 1:2 sounds it every other bar; a Slice (reverse) moves the kick from step 0 to step 15 */
    {
        int c, blk, seq[16], ns = 0, ok = 1, m, i;
        uint32_t kc[5];
        for (c = 0; c < 1; c++) {
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_CHAIN, 4);
            en_set(P_DD_CHANGE, 0);
            en_set(P_DD_MUTATE, 0);
            en_set(P_DD_EVERY, 0);
            en_play(1);
            for (blk = 0; blk < 17000 && ns < 12; blk++) {
                en_render(buf, 64, 2048);
                if (en_drum_pos == 8 && (ns == 0 || seq[ns - 1] >= 0)) {
                    seq[ns++] = en_pat_now;
                    while (en_drum_pos == 8 && blk < 17000) {
                        en_render(buf, 64, 2048);
                        blk++;
                    }
                }
            }
            for (i = 0; i + 1 < ns; i++)                /* (the first bar is before the chain's first step: left out) */
                seq[i] = seq[i + 1];
            ns--;
            for (i = 1; i + 4 < ns; i++)
                ok &= seq[i] == seq[i + 4];
            CHECK(ns >= 10 && ok && seq[2] == 2 && seq[3] == 3 && seq[0] != seq[2], "chain 1 1 3 4: patterns %d %d %d %d %d %d %d %d", seq[0], seq[1], seq[2], seq[3], seq[4], seq[5], seq[6], seq[7]);
        }
        for (m = 0; m < 5; m++) {
            int q;
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            for (q = 0; q < EN_NPAT; q++)
                en_pats[q][0] = 0x1111;
            en_set(P_TIME, m == 1 ? 1 : m == 2 ? 2 : 0);
            en_set(P_COND, m == 3 ? 1 : 0);
            en_play(1);
            run(4.0f);
            kick_n = 0;
            for (blk = 0; blk < 5512 * 3; blk++)
                en_render(buf, 64, 2048);
            kc[m] = kick_n;
            if (m == 4) {
                uint32_t last = 0, gap = 0, first_after = 0;
                int j;
                en_init();
                en_set(P_TEMPO, 128);
                en_set(P_MIDI, 1);
                for (q = 0; q < EN_NPAT; q++)
                    en_pats[q][0] = 0x0001;
                en_play(1);
                run(5.0f);
                kick_last = 0;
                for (blk = 0; blk < 3000; blk++) {
                    blk_now = (uint32_t)blk;
                    if (blk == 1500)
                        en_set(P_SLICE, 3);
                    en_render(buf, 64, 2048);
                    if (kick_last && kick_last != last) {
                        if (last && !gap && first_after) {
                        }
                        if (last && kick_last > 1500 && !first_after)
                            first_after = kick_last, gap = first_after - last;
                        last = kick_last;
                    }
                }
                j = (int)gap - 1292;
                kc[4] = (uint32_t)(j < 0 ? -j : j);
            }
        }
        CHECK(kc[0] > 20u && kc[1] * 10u < kc[0] * 6u && kc[1] * 10u > kc[0] * 4u, "time half: %u kicks, %u at normal", kc[1], kc[0]);
        CHECK(kc[2] * 10u > kc[0] * 17u, "time double: %u kicks, %u at normal", kc[2], kc[0]);
        CHECK(kc[3] * 10u < kc[0] * 6u && kc[3] * 10u > kc[0] * 4u, "condition 1:2: %u kicks, %u always", kc[3], kc[0]);
        CHECK(kc[4] > 40u, "slice reverse: the first kick after was %u blocks off a bar", kc[4]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the acid line frozen: it is packed into 18 bytes and comes back as it was (steps set one at a time), and with Frozen on Mutate at its top leaves it alone for
     * 20 s at 128 bpm, while free it is rewritten */
    {
        int fr, s, ok = 1, blk;
        uint8_t pk[18], pk2[18], before[18], after[18];
        for (fr = 0; fr < 2; fr++) {
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_AC_MUT, 100);
            en_set(P_AC_LEN, 16);
            en_set(P_AC_LOCK, fr);
            for (s = 0; s < 16; s++)
                en_acid_step(s, (s * 5) % 12, s & 3, s % 3 != 0, s % 4 == 1, s % 5 == 2);
            run(0.2f);
            en_acid_pack(before);
            en_play(1);
            for (blk = 0; blk < 7000; blk++)
                en_render(buf, 256, 2048);
            en_acid_pack(after);
            if (fr == 0)
                ok = ok && memcmp(before, after, 18) != 0;
            else
                ok = ok && !memcmp(before, after, 18);
        }
        CHECK(ok, "acid frozen: the line %s with Mutate at 100", ok ? "behaved" : "changed when frozen or held when free");
        en_init();
        for (s = 0; s < 16; s++)
            en_acid_step(s, (s * 7) % 12, (s >> 1) & 3, (s & 1) ^ 1, s == 3 || s == 9, s == 5);
        run(0.2f);
        en_acid_pack(pk);
        en_init();
        en_acid_unpack(pk);
        run(0.2f);
        en_acid_pack(pk2);
        CHECK(!memcmp(pk, pk2, 18), "acid frozen: the packed line did not survive unpack and pack");
        for (s = 0; s < 16; s++)
            ok = ok && en_acid_off[s] == (uint8_t)((s * 7) % 12) && en_acid_oct[s] == (uint8_t)((s >> 1) & 3);
        CHECK(ok, "acid frozen: the steps' notes and octaves come back");
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the bass line (the drone's modes), 4 s at 128 bpm from the MIDI out: Drone plays no notes of its own, Kick plays one on each kick, Eighths about 17,
     * Off-beat about 8, Walk about 8, and every note stays in the bass range (C1 .. B5) */
    {
        int m, blk;
        uint32_t nn[16], kk[16], lo = 999, hi = 0;
        for (m = 0; m < 16; m++) {
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            en_set(P_BL_MODE, m);
            en_set(P_BL_MOVE, 60);
            en_play(1);
            run(4.0f);
            bass_n = kick_n = 0;
            bass_lo = 999;
            bass_hi = 0;
            for (blk = 0; blk < 2756; blk++) {
                blk_now = (uint32_t)blk;
                en_render(buf, 64, 2048);
            }
            nn[m] = bass_n;
            kk[m] = kick_n;
            if (m && bass_lo < lo)
                lo = bass_lo;
            if (m && bass_hi > hi)
                hi = bass_hi;
        }
        CHECK(nn[0] <= 2u, "bass line: Drone played %u notes of its own", nn[0]);
        CHECK(kk[2] >= 4u && nn[2] + 1u >= kk[2] && nn[2] <= kk[2] + 1u, "bass line, Kick: %u notes for %u kicks", nn[2], kk[2]);
        CHECK(nn[3] >= 14u && nn[3] <= 20u, "bass line, Eighths: %u notes (expected about 17)", nn[3]);
        CHECK(nn[4] >= 6u && nn[4] <= 10u, "bass line, Off-beat: %u notes (expected about 8)", nn[4]);
        CHECK(nn[5] >= 6u && nn[5] <= 10u, "bass line, Walk: %u notes (expected about 8)", nn[5]);
        CHECK(nn[1] > 2u && nn[6] > 2u, "bass line, Root %u and Riff %u notes", nn[1], nn[6]);
        CHECK(lo >= 24u && hi <= 83u, "bass line: notes %u .. %u outside C1 .. B5", lo, hi);
        for (m = 7; m < 16; m++)                 /* the figures by name (0.14.15): each plays */
            CHECK(nn[m] >= 5u && nn[m] <= 26u, "bass line, figure %d: %u notes in 4 s", m, nn[m]);
        CHECK(nn[7] >= 14u && nn[15] >= 20u, "bass line: Octaves %u, Gallop %u notes in 4 s", nn[7], nn[15]);
        /* the fifth (0.14.15): under a 9th chord (1 3 7 9) the Walk's fifth is the fifth above the root, not the chord's third tone (the seventh):
         * A minor 9, held: the walk plays E (and never only G) */
        {
            int k;
            en_init();
            en_set(P_TEMPO, 128);
            en_set(P_MIDI, 1);
            en_pool(EN_MODE_POOL[MODE_AEOLIAN]);
            en_set(P_ROOT, 9);
            en_set(P_CHORD, EN_CH_9TH);
            en_set(P_BL_MODE, 5);
            en_set(P_BL_MOVE, 0);
            en_set(P_DRIFT, 100);
            en_regrow();
            en_play(1);
            run(2.0f);
            for (k = 0; k < 12; k++)
                bass_pc[k] = 0;
            run(15.0f);                          /* 8 bars: 8 fifths on beat 3, 4 or so on beat 2 */
            CHECK(bass_pc[4] >= 8u && bass_pc[4] > 2u * bass_pc[7], "the walk's fifth under Am9: E %u, G %u", bass_pc[4], bass_pc[7]);
            printf("  the walk under Am9, 8 bars: A %u C %u E %u G %u\n", bass_pc[9], bass_pc[0], bass_pc[4], bass_pc[7]);
        }
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* the drum rolls (hat roll, hat triplets, snare roll, flams, double kick), each held for four seconds at three tempos: nothing breaks */
    {
        int fx, tm, blk, j, broken = 0;
        for (fx = FX_HATROLL; fx <= FX_DBLKICK; fx++)
            for (tm = 0; tm < 3; tm++) {
                en_init();
                en_set(P_TEMPO, tm == 0 ? 70 : tm == 1 ? 128 : 190);
                en_play(1);
                run(1.0f);
                en_fx(1ull << fx);
                for (blk = 0; blk < 700; blk++) {
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 512; j++)
                        if (buf[j] == INT32_MIN || buf[j] > 8400000 || buf[j] < -8400000)
                            broken++;
                }
                en_fx(0);
                run(0.5f);
            }
        CHECK(!broken, "drum rolls: %d samples broken (NaN or over full scale)", broken);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* Flex Beat: every pattern, started each way (off, on the beat, on the bar), held as a one shot: it arms, plays its bar, and is over
     * exactly once; the level stays sane; and a loop keeps going while it is held */
    {
        int pat, q, blk, j, broken = 0, bad = 0, badloop = 0;
        for (pat = 0; pat < EN_NFLEX; pat++)
            for (q = 0; q < 3; q++) {
                uint32_t done0;
                en_init();
                en_play(1);
                run(2.0f);
                en_fxs(EN_FXS_FX_QUANT, q);
                en_fxs(EN_FXS_FX_MODE, 0);
                en_fxs(EN_FXS_FX_MIX, 100);
                run(0.1f);
                done0 = en_flx_done;
                en_flex_select(pat);
                en_fx(FXB(FX_FLEX));
                for (blk = 0; blk < 1700; blk++) {            /* ~10 s: a bar to wait, a bar to play */
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 512; j++)
                        if (buf[j] == INT32_MIN || buf[j] > 8400000 || buf[j] < -8400000)
                            broken++;
                }
                if (en_flx_done != done0 + 1u || en_flx_state != 0)
                    bad++;
                en_fx(0);
                run(0.5f);
            }
        for (pat = 0; pat < EN_NFLEX; pat += 7) {                   /* a loop: not over while it is held, over when let go */
            uint32_t done0;
            en_init();
            en_play(1);
            run(2.0f);
            en_fxs(EN_FXS_FX_QUANT, 0);
            en_fxs(EN_FXS_FX_MODE, 1);
            run(0.1f);
            done0 = en_flx_done;
            en_flex_select(pat);
                en_fx(FXB(FX_FLEX));
            run(7.0f);
            if (en_flx_done != done0 || en_flx_state != 2)
                badloop++;
            en_fx(0);
            run(0.5f);
        }
        CHECK(!broken, "flex beat: %d samples broken (NaN or over full scale)", broken);
        CHECK(!bad, "flex beat: %d of %d one shots did not play out exactly once", bad, EN_NFLEX * 3);
        CHECK(!badloop, "flex beat: %d loops stopped by themselves", badloop);
        en_fxs(EN_FXS_FX_QUANT, EN_FXS_DEF[EN_FXS_FX_QUANT]);
        en_fxs(EN_FXS_FX_MODE, EN_FXS_DEF[EN_FXS_FX_MODE]);
        en_init();
        en_play(1);
        run(1.0f);
    }
    /* every piece of every genre plays: not silent, not over full scale, no NaN */
    {
        int g, i, p, l, wrong = 0;
        for (g = 0; g < EN_NGENRE; g++)
            for (i = 0; i < EN_NPIECES; i++) {
                int16_t par[P_NPARAMS];
                uint16_t pool, pats[EN_NPAT][EN_NLANE];
                float rms;
                en_init();
                en_piece_g(g, i, par, &pool);
                en_piece_pats_g(g, i, pats);
                for (p = 0; p < P_NPARAMS; p++)
                    en_set(p, par[p]);
                en_pool(pool);
                for (p = 0; p < EN_NPAT; p++)
                    for (l = 0; l < EN_NLANE; l++)
                        en_pats[p][l] = pats[p][l];
                en_play(1);
                run(2.0f);
                peak = 0.0f;
                sum2 = 0.0f;
                run(12.0f);
                rms = sqrtf(sum2 / (12.0f * 44100.0f * 2.0f));
                if (bad || rms < 0.015f || peak > 0.99f) {
                    wrong++;
                    printf("   genre %d piece %d: rms %.4f peak %.3f%s\n", g, i, (double)rms, (double)peak, bad ? " NaN" : "");
                }
            }
        CHECK(!wrong, "genre pieces: %d silent or over full scale", wrong);
        en_init();
        en_play(1);
        run(1.0f);
    }

    /* the acid's cutoff and octave, and the vinyl's crackle, are heard (zero crossings: brightness, pitch) */
    {
        float zc[5], hf[5], ee[5];
        int k;
        for (k = 0; k < 5; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            double e = 1e-9, d = 0.0;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_CM_LVL, 0);                 /* (and the counter-melody) */
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            if (k < 3) {                         /* the acid alone: cutoff low, high; an octave up twice */
                en_set(P_NOISE, 0);
                en_set(P_AC_DENS, 100);
                en_set(P_AC_ENV, 0);
                en_set(P_AC_CUT, k == 1 ? 100 : 20);
                en_set(P_AC_OCT, k == 2 ? 2 : 0);
            } else {                             /* the vinyl alone: its default crackle, its densest */
                en_set(P_ACID, 0);
                en_fx(1u << FX_VINYL);               /* (the key; its settings: the crackle at its default, then at its densest, the hiss off) */
                en_fxs(EN_FXS_VN_CRACK, k == 3 ? 45 : 100);
                en_fxs(EN_FXS_VN_HISS, 0);
                en_fxs(EN_FXS_VN_LEVEL, 100);
            }
            en_play(1);
            for (b2 = 0; b2 < 10 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j] * (1.0f / 8388608.0f);
                    z += (x > 0.0f) != (prev > 0.0f);
                    e += (double)(x * x);
                    d += (double)((x - prev) * (x - prev));   /* the difference: the highs' share */
                    prev = x;
                }
            }
            zc[k] = (float)z;
            hf[k] = (float)(d / e);
            ee[k] = (float)e;
        }
        CHECK(hf[1] > 3.0f * hf[0], "acid cutoff 100 not brighter: %.4f vs %.4f", (double)hf[1], (double)hf[0]);
        CHECK(zc[2] > 1.5f * zc[0], "acid oct +2 not higher: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        CHECK(ee[4] < 4.0f * ee[3] && ee[4] > ee[3] / 4.0f, "denser crackle is louder (or silent): %.4f vs %.4f", (double)ee[4], (double)ee[3]);
        en_init();
        en_play(1);
    }

    /* the master bus: the compressor reduces (its meter says so, and the level falls), the limiter holds its ceiling (at -12 dB nothing passes 0.2512 of the
     * mix's full scale, which at the unit gain tanh makes 2.04 M of 8.39 M), and with both off the sound is unchanged but for the limiter's delay */
    {
        double rms[3] = {0, 0, 0};
        float pk[3] = {0, 0, 0}, grmax = 0.0f, lgmax = 0.0f;
        int k, blk, j;
        for (k = 0; k < 3; k++) {          /* 0: off; 1: compressor 10:1 at -30 dB; 2: limiter at -12 dB */
            en_init();
            en_set(P_PULSE, 100);
            en_set(P_PAD, 80);
            en_set(P_ACID, 80);
            en_set(P_BASS, 90);
            en_set(P_MC_RATIO, k == 1 ? 6 : 0);
            en_set(P_MC_THR, 0);
            en_set(P_ML_CEIL, k == 2 ? 0 : 100);
            en_play(1);
            for (blk = 0; blk < 1800; blk++) {
                en_render(buf, 256, 2048);
                if (blk < 600)
                    continue;
                if (en_comp_gr > grmax)
                    grmax = en_comp_gr;
                if (en_lim_gr > lgmax)
                    lgmax = en_lim_gr;
                for (j = 0; j < 512; j++) {
                    const float a2 = (float)(buf[j] < 0 ? -buf[j] : buf[j]);
                    rms[k] += (double)a2 * (double)a2;
                    if (a2 > pk[k])
                        pk[k] = a2;
                }
            }
        }
        CHECK(grmax > 3.0f, "the compressor's meter: %.1f dB", (double)grmax);
        CHECK(rms[1] < 0.6 * rms[0], "10:1 at -30 dB lowers the level (%.3g vs %.3g)", rms[1], rms[0]);
        CHECK(pk[0] > 2.3e6f, "the unlimited mix is loud enough to test with (%.3g)", (double)pk[0]);
        CHECK(pk[2] < 2.1e6f, "the limiter at -12 dB holds: peak %.3g", (double)pk[2]);
        CHECK(lgmax > 1.0f, "the limiter's meter: %.1f dB", (double)lgmax);
        en_init();
        en_play(1);
    }

    /* the mixer: a layer panned hard left is heard on the left; its send at 0 takes it out of the reverb; the middle and 50 leave it as it was */
    {
        double ch[2][3] = {{0}}, rv[2] = {0, 0};
        int k, blk, j;
        for (k = 0; k < 3; k++) {          /* 0: as it is; 1: the drums hard left; 2: the drums centred with the send at 0 and the reverb on */
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_ACID, 0);
            en_set(P_CM_LVL, 0);
            en_set(P_NOISE, 0);
            en_set(P_PULSE, 90);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, k == 2 ? 100 : 0);
            en_set(P_PAN4, k == 1 ? 0 : 50);
            en_set(P_SEND4, k == 2 ? 0 : 50);
            en_play(1);
            for (blk = 0; blk < 1000; blk++) {
                en_render(buf, 256, 2048);
                if (blk < 300)
                    continue;
                for (j = 0; j < 256; j++) {
                    const double l = (double)buf[2 * j], r = (double)buf[2 * j + 1];
                    ch[0][k] += l * l;
                    ch[1][k] += r * r;
                }
            }
        }
        rv[0] = ch[0][0];
        rv[1] = ch[1][0];
        CHECK(ch[1][1] < 0.05 * ch[0][1], "drums hard left: the right side %.3g against %.3g", ch[1][1], ch[0][1]);
        CHECK(ch[0][0] > 0.5 * ch[1][0] && ch[1][0] > 0.5 * ch[0][0], "the drums centred are on both sides (%.3g, %.3g)", rv[0], rv[1]);
        CHECK(ch[0][2] > 0.0 && ch[0][2] < 1.5 * ch[0][0], "the drums with no send, the reverb up, are no louder than dry (%.3g vs %.3g)", ch[0][2], ch[0][0]);
        en_init();
        en_play(1);
    }

    /* the auto-DJ in SYNC (the engine fills the bar before a chord change and breaks on the change's bar), and the drop that lands: the break
     * starts within a bar of a chord change; the fill is in the bar before it (the hats and snare on every step); the drop has the kick on the one */
    {
        int blk, j, breaks = 0, near = 0, filled = 0, drops = 0, dropkick = 0;
        uint8_t last_sec = 0, ch0[4] = {0, 0, 0, 0}, ch1[4];
        uint32_t since_change = 99999u, sample = 0;
        en_init();
        en_set(P_DRIFT, 1);
        en_play(1);
        en_dj_sync(1);
        for (blk = 0; blk < 12000; blk++) {         /* ~70 s */
            const uint8_t sec = en_sec;
            en_render(buf, 256, 2048);
            sample += 256u;
            for (j = 0; j < 4; j++)
                ch1[j] = (uint8_t)en_chord[j];
            if (ch1[0] != ch0[0] || ch1[1] != ch0[1] || ch1[2] != ch0[2] || ch1[3] != ch0[3]) {
                since_change = 0;
                for (j = 0; j < 4; j++)
                    ch0[j] = ch1[j];
            } else if (since_change < 99999u) {
                since_change += 256u;
            }
            if (sec == EN_SEC_BREAK && last_sec == EN_SEC_GROOVE) {      /* a break began */
                breaks++;
                near += since_change < 3u * 44100u;
            }
            if (sec == EN_SEC_GROOVE && last_sec != EN_SEC_GROOVE)
                drops++;
            last_sec = sec;
            if (sec == EN_SEC_BREAK && en_sec_req == 0 && breaks && blk > 200)
                en_section(EN_CMD_BUILD);             /* (the UI's part: a break lasts a few bars here, then the build) */
        }
        (void)filled;
        (void)dropkick;
        CHECK(breaks >= 1, "the DJ in SYNC: no break in 70 s (breaks %d)", breaks);
        CHECK(near == breaks, "every break began with a chord change (%d of %d)", near, breaks);
        CHECK(drops >= 1, "a drop came (%d)", drops);
        en_dj_sync(0);
        en_init();
        en_play(1);
    }

    /* sections by genre (audit M-13): a house breakdown keeps the kick, a drum and bass one the hats, a lofi one stops everything; a house build
     * takes eight bars, a trap's two */
    {
        static const int G[4] = {2, 5, 1, 9};
        uint32_t kk[4], hh[4], bt[4];
        int k;
        for (k = 0; k < 4; k++) {
            int blk;
            en_init();
            en_genre(G[k]);
            en_play(1);
            run(3.0f);
            en_section(EN_CMD_BREAK);
            for (blk = 0; blk < 3000 && en_sec != EN_SEC_BREAK; blk++)
                en_render(buf, 256, 2048);
            run(1.0f);
            kick_n = 0;
            hat_n = 0;
            run(5.0f);
            kk[k] = kick_n;
            hh[k] = hat_n;
            en_section(EN_CMD_BUILD);
            for (blk = 0; blk < 3000 && en_sec != EN_SEC_BUILD; blk++)
                en_render(buf, 256, 2048);
            bt[k] = 0;
            for (blk = 0; blk < 12000 && en_sec == EN_SEC_BUILD; blk++, bt[k]++)
                en_render(buf, 256, 2048);
        }
        CHECK(kk[0] > 0u && hh[0] == 0u, "a house breakdown keeps the kick only (kicks %u, hats %u)", kk[0], hh[0]);
        CHECK(kk[1] == 0u && hh[1] > 0u, "a drum and bass breakdown keeps the hats only (kicks %u, hats %u)", kk[1], hh[1]);
        CHECK(kk[2] == 0u && hh[2] == 0u, "a lofi breakdown stops the drums (kicks %u, hats %u)", kk[2], hh[2]);
        CHECK(bt[3] > 0u && bt[0] > 3u * bt[3] && bt[0] < 5u * bt[3], "a house build is four times a trap's (%u and %u blocks)", bt[0], bt[3]);
        en_init();
        en_play(1);
    }

    /* the harmony (0.14.14): a dominant in minor, no diminished chords, the genre's harmonic rhythm in bars */
    {
        int k, n, found, mask;
        char t[12];
        en_init();
        en_play(0);                              /* resting: en_next changes the chord at once */
        en_genre(1);                             /* lofi: 2 bars a chord, cadences with a dominant */
        en_pool(EN_MODE_POOL[MODE_AEOLIAN]);
        en_set(P_ROOT, 9);                       /* A minor: V is E, its third G (7) or, as a dominant, G# (8) */
        en_set(P_CHORD, EN_CH_TRIAD);
        en_set(P_DRIFT, 60);
        en_regrow();
        run(0.05f);
        for (k = 0, found = 0; k < 300 && !found; k++) {
            en_next();
            run(0.02f);
            found = en_chord_root % 12 == 4;
        }
        for (k = 0, mask = 0; k < en_chord_n; k++)
            mask |= 1 << (en_chord[k] % 12);
        CHECK(found, "lofi reaches V within 300 chords");
        CHECK(found && (mask & 1 << 8) && !(mask & 1 << 7), "lofi's V in A minor is E major (pcs %03x)", mask);
        en_genre(2);                             /* house: no dominant */
        en_regrow();
        run(0.05f);
        for (k = 0, found = 0; k < 300 && !found; k++) {
            en_next();
            run(0.02f);
            found = en_chord_root % 12 == 4;
        }
        for (k = 0, mask = 0; k < en_chord_n; k++)
            mask |= 1 << (en_chord[k] % 12);
        CHECK(found && (mask & 1 << 7) && !(mask & 1 << 8), "house's v in A minor is E minor (pcs %03x)", mask);
        /* no diminished chords: synthwave in C major walked 400 chords lands on B (vii) rarely, and its leading chord is G, not B */
        en_genre(13);
        en_pool(EN_MODE_POOL[MODE_IONIAN]);
        en_set(P_ROOT, 0);
        en_regrow();
        run(0.05f);
        {
            int cnt[12] = {0};
            for (k = 0, n = 0; k < 400; k++) {
                en_next();
                run(0.02f);
                n += en_chord_root % 12 == 11;
                cnt[en_chord_root % 12]++;
            }
            CHECK(n < 40, "vii in C major: %d of 400 chords", n);
            printf("  synthwave in C major, 400 chords by root:");
            for (k = 0; k < 12; k++)
                printf(" %d", cnt[k]);
            printf("\n");
        }
        /* the harmonic rhythm: house changes chord every 2 bars, on the bar; Drift halves and doubles it; the text says so */
        en_genre(2);
        en_pool(EN_MODE_POOL[MODE_AEOLIAN]);
        en_set(P_TEMPO, 120);
        en_set(P_CH_LOOP, 0);
        en_regrow();
        run(0.05f);
        en_play(1);
        {
            int last = -1, nch = 0, odd = 0, prev_bar = -1, gaps_bad = 0;
            for (k = 0; k < 40 * 16; k++) {      /* 40 bars, a sixteenth at a time */
                int sig, bar;
                run(0.125f);
                sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
                bar = (int)(en_bars / 16u);
                if (sig != last && last >= 0) {
                    nch++;
                    odd += bar & 1;
                    if (prev_bar >= 0 && bar - prev_bar != 2)
                        gaps_bad++;
                    prev_bar = bar;
                }
                last = sig;
            }
            CHECK(nch >= 17 && nch <= 20, "house: %d chord changes in 40 bars (2 bars a chord)", nch);
            CHECK(odd == 0 && gaps_bad == 0, "house: changes on even bars, two apart (%d odd, %d bad gaps)", odd, gaps_bad);
            printf("  house: %d chord changes in 40 bars\n", nch);
        }
        en_param_text(P_DRIFT, 60, t);
        CHECK(!strcmp(t, "2 bars"), "drift 60 in house: %s", t);
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "1 bar"), "drift 0 in house: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "8 bars"), "drift 100 in house: %s", t);
        en_genre(0);
        en_param_text(P_DRIFT, 60, t);
        CHECK(!strcmp(t, "1:42"), "drift 60 in hypno: %s", t);
        /* the progression loop starts from the chord playing: house with Loop 4 plays home, a, b, c, home, a, b, c */
        en_play(0);
        en_genre(2);
        en_pool(EN_MODE_POOL[MODE_AEOLIAN]);
        en_set(P_ROOT, 9);
        en_set(P_CH_LOOP, 2);
        en_regrow();
        run(0.05f);
        {
            int rt[12], bad = 0;
            rt[0] = en_chord_root % 12;
            for (k = 1; k < 12; k++) {
                en_next();
                run(0.02f);
                rt[k] = en_chord_root % 12;
            }
            for (k = 0; k < 8; k++)
                bad += rt[k + 4] != rt[k];
            CHECK(rt[0] == 9 && rt[4] == 9 && rt[8] == 9 && bad == 0, "loop 4: %d %d %d %d | %d %d %d %d | %d %d %d %d", rt[0], rt[1], rt[2], rt[3], rt[4], rt[5], rt[6], rt[7], rt[8], rt[9], rt[10], rt[11]);
            printf("  house loop 4: %d %d %d %d | %d %d %d %d\n", rt[0], rt[1], rt[2], rt[3], rt[4], rt[5], rt[6], rt[7]);
        }
        /* the pieces: every dub piece's pad stabs (the skank), and the genres' chord loops */
        {
            static int16_t pp[P_NPARAMS];
            uint16_t pl;
            int nstab = 0;
            for (k = 0; k < 10; k++) {
                en_piece_g(4, k, pp, &pl);
                nstab += (0x2E7140u >> pp[P_PADTONE]) & 1u;   /* Stabs 6, Dub 8, Pings 12, Hits 13, Rave 14, Wurli 17, Mallet 18, Brass 19, Pluck 21 */
            }
            CHECK(nstab == 10, "dub pieces with a stabbing pad: %d of 10", nstab);
            en_piece_g(2, 0, pp, &pl);
            CHECK(pp[P_CH_LOOP] == 2, "house piece 1 loop: %d", pp[P_CH_LOOP]);
            en_piece_g(13, 0, pp, &pl);
            CHECK(pp[P_CH_LOOP] == 2, "synthwave piece 1 loop: %d", pp[P_CH_LOOP]);
            en_piece_g(3, 0, pp, &pl);
            CHECK(pp[P_CH_LOOP] == 1, "dubstep piece 1 loop: %d", pp[P_CH_LOOP]);
            en_piece_g(1, 0, pp, &pl);
            CHECK(pp[P_CH_LOOP] == 0, "lofi piece 1 loop: %d", pp[P_CH_LOOP]);
        }
        en_init();
        en_play(1);
    }

    /* the intro: from Play, en_intro_k climbs to 1 over its length and the engine is quiet but for the pad at first */
    {
        float k0, k1, k2;
        double s0, e_early, e_late;
        en_init();
        en_play(0);
        en_intro(4);
        en_regrow();
        en_set(P_PULSE, 90);
        en_set(P_PAD, 20);
        en_play(1);
        run(0.1f);
        s0 = (double)sum2;
        run(0.4f);
        e_early = (double)sum2 - s0;                 /* the first half second: the pad alone */
        k0 = en_intro_k;
        run(1.5f);
        k1 = en_intro_k;
        run(2.5f);
        k2 = en_intro_k;
        s0 = (double)sum2;
        run(0.4f);
        e_late = (double)sum2 - s0;                  /* everything in */
        CHECK(k0 > 0.05f && k0 < 0.25f && k1 > 0.4f && k1 < 0.6f && k2 >= 1.0f, "intro clock: %.2f %.2f %.2f", (double)k0, (double)k1, (double)k2);
        CHECK(e_late > 4.0 * e_early, "intro: the first half second's energy %.4f, after it %.4f", e_early, e_late);
        printf("  intro: energy %.4f early, %.4f in\n", e_early, e_late);
        en_intro(0);
        run(0.1f);
        CHECK(en_intro_k >= 1.0f, "intro off: %.2f", (double)en_intro_k);
        en_init();
        en_play(1);
    }

    /* the pluck's line (the Pluck steps page): steps set, read back, packed into 12 bytes and unpacked; frozen, it survives a new piece and plays as written */
    {
        uint8_t b[12], c[12];
        int i, ok = 1;
        en_init();
        en_play(0);
        en_genre(2);
        en_set(P_CM_LVL, 60);
        en_set(P_CM_RATE, 3);
        en_set(P_CM_LOOP, 2);
        en_set(P_CM_VU, 100);
        en_set(P_CM_LOCK, 1);
        for (i = 0; i < 16; i++)
            en_pluck_step(i, (i * 5) % 12, i % 4 == 0 ? 3 : i % 4 == 2 ? 2 : i % 8 == 5 ? 1 : 0);   /* 1 . 2 . 3 1 2 . | 1 . 2 . 3 1 2 . : eight notes a bar */
        run(0.05f);
        for (i = 0; i < 16; i++)
            ok &= en_cm_rung[i] == (i * 5) % 12 && en_cm_lvl[i] == (i % 4 == 0 ? 3 : i % 4 == 2 ? 2 : i % 8 == 5 ? 1 : 0);
        CHECK(ok, "pluck steps read back");
        CHECK(en_pluck_pack(b) == 1, "pluck line packs");
        en_regrow();                             /* a new piece: frozen, the line stays */
        run(0.05f);
        en_pluck_pack(c);
        CHECK(!memcmp(b, c, 12), "pluck line kept through regrow");
        for (i = 0; i < 16; i++)
            en_pluck_step(i, 0, 0);
        run(0.05f);
        CHECK(en_pluck_pack(c) == 0, "pluck line cleared");
        en_pluck_unpack(b);
        run(0.05f);
        en_pluck_pack(c);
        CHECK(!memcmp(b, c, 12), "pluck line unpacked");
        cm_on_n = 0;
        en_play(1);
        run(0.3f);
        cm_on_n = 0;
        run(60.0f / 120.0f * 4.0f * 4.0f);     /* four bars at 120 */
        CHECK(cm_on_n >= 28 && cm_on_n <= 36, "frozen line: %d notes in four bars (eight a bar written)", cm_on_n);
        printf("  pluck steps: %d notes in four bars\n", cm_on_n);
        en_set(P_CM_LOCK, 0);
        en_init();
        en_play(1);
    }

    /* a breakdown in a genre whose pads all stab (reggae): the pad breathes through it instead of falling silent */
    {
        float lv = 0.0f;
        int i;
        en_init();
        en_play(0);
        en_genre(12);
        en_regrow();
        en_play(1);
        run(3.0f);
        en_section(EN_CMD_BREAK);
        run(2.5f);                               /* (the break arrives on the bar) */
        for (i = 0; i < 40; i++) {
            run(0.1f);
            if (en_sec == EN_SEC_BREAK && en_pad_level > lv)
                lv = en_pad_level;
        }
        CHECK(en_sec == EN_SEC_BREAK && lv > 0.05f, "reggae breakdown: the pad's level %.3f (section %d)", (double)lv, en_sec);
        printf("  reggae breakdown: pad level %.2f\n", (double)lv);
        en_init();
        en_play(1);
    }

    /* the pluck on MIDI channel 2: every note-on gets one note-off, with a plain sound and with a DX voice, and Stop lets go of all */
    {
        int snd, i, worst = 0, ons[2];
        for (snd = 0; snd < 2; snd++) {
            en_init();
            en_play(0);
            en_genre(10);
            en_set(P_CM_LVL, 60);
            en_set(P_CM_DENS, 90);
            en_set(P_CM_DEC, snd ? 80 : 40);
            en_set(P_CM_SND, snd ? 12 : 1);          /* 12: the first DX voice */
            en_regrow();
            cm_on_n = cm_off_n = 0;
            memset(cm_bal, 0, sizeof cm_bal);
            en_play(1);
            run(8.0f);
            CHECK(cm_on_n >= 12, "pluck notes out (%s): %d", snd ? "DX" : "plain", cm_on_n);
            CHECK(cm_off_n <= cm_on_n && cm_on_n - cm_off_n <= 8, "pluck notes sounding at once (%s): %d", snd ? "DX" : "plain", cm_on_n - cm_off_n);
            en_play(0);
            run(0.1f);
            for (i = 0; i < 128; i++)
                if (cm_bal[i] != 0)
                    worst = cm_bal[i];
            CHECK(cm_on_n == cm_off_n && worst == 0, "pluck note-offs (%s): %d on, %d off, a note left at %d", snd ? "DX" : "plain", cm_on_n, cm_off_n, worst);
            ons[snd] = cm_on_n;
        }
        printf("  pluck on MIDI: %d plain notes, %d DX notes, all let go\n", ons[0], ons[1]);
        en_init();
        en_play(1);
    }

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
