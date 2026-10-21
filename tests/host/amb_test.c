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

static uint32_t midi_count;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
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
            for (k = 0; k < EN_NSOUND; k++) {
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
                en_set(SP[v], (int)((r >> 9) % (v < 3 ? EN_NSOUND : EN_NKIT)));
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
                case EN_FXS_RL_START: case EN_FXS_RL_END: hi = 5; break;
                case EN_FXS_RL_TIME: case EN_FXS_PM_RATE: hi = 3; break;
                case EN_FXS_SC_SIZE: case EN_FXS_HF_SPEED: case EN_FXS_RV_LEN: hi = 2; break;
                case EN_FXS_SR_EVERY: case EN_FXS_SR_REACH: case EN_FXS_FZ_GRAINS: hi = 3; break;
                case EN_FXS_SR_STYLE: hi = 1; break;
                case EN_FXS_SW_RATE: hi = 4; break;
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
                m |= (uint64_t)(r & (r >> 5) & 0x3Fu) << 31;   /* and the bits 31..36 */
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
        CHECK(ee[4] < 3.0f * ee[3] && ee[4] > ee[3] / 3.0f, "denser crackle is louder (or silent): %.4f vs %.4f", (double)ee[4], (double)ee[3]);
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
