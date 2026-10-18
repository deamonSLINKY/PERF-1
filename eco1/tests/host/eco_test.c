/* SPDX-License-Identifier: GPL-3.0-only */
/* ECO-1's engine on the host: the parameters and pieces, the plant's behaviour (it plays, it answers the
 * weather, each species its own way), MIDI clock and the grid, MIDI out's channels and CCs, the voices
 * apart, and the levels (nothing silent that shouldn't be, nothing past full scale, no NaN).
 *   tests/run_tests.sh (or: cc -DEN_HOST -Ifirmware/src/dsp tests/host/eco_test.c firmware/src/dsp/amb.c -lm) */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* MIDI out, as the device's glue would send it */
static uint32_t midi_n, midi_notes[16], midi_cc[16], midi_cc_num[128];
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d2;
    midi_n++;
    if ((st & 0xF0u) == 0x90u)
        midi_notes[st & 15u]++;
    if ((st & 0xF0u) == 0xB0u && d1 != 123u) {
        midi_cc[st & 15u]++;
        midi_cc_num[d1 & 127u]++;
    }
}
static void midi_reset(void)
{
    midi_n = 0;
    memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_cc, 0, sizeof midi_cc);
    memset(midi_cc_num, 0, sizeof midi_cc_num);
}

static int32_t blk[512];
static double peak, sumsq;
static long frames;
static int bad;
static void run(double secs)
{
    long b, n = (long)(secs * 44100.0 / 256.0);
    for (b = 0; b < n; b++) {
        int i;
        en_render(blk, 256, 2800);
        for (i = 0; i < 512; i++) {
            double x = (double)blk[i] / 8388608.0;
            if (x != x)
                bad = 1;
            sumsq += x * x;
            if (fabs(x) > peak)
                peak = fabs(x);
        }
        frames += 256;
    }
}
static void piece(int p)
{
    int16_t par[P_NPARAMS];
    uint16_t pool;
    int i;
    en_init();
    en_piece(p, par, &pool);
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, par[i]);
    en_pool(pool);
    en_seed(1234u + (uint32_t)p);
    en_regrow();
    en_play(1);
    peak = sumsq = 0.0;
    frames = 0;
    bad = 0;
}
static double rms_db(void) { return 10.0 * log10(sumsq / (double)(frames * 2 + 1) + 1e-20); }

int main(void)
{
    int p, i;
    char t[16];

    /* parameters: every default and every piece within range, every value has text */
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *pi = en_param_info(i);
        CHECK(pi->lo <= pi->def && pi->def <= pi->hi, "param %d (%s) default out of range", i, pi->name);
        en_param_text(i, pi->lo, t);
        CHECK(t[0] != 0 && strlen(t) < 10, "param %d text '%s'", i, t);
        en_param_text(i, pi->hi, t);
        CHECK(t[0] != 0 && strlen(t) < 10, "param %d text '%s'", i, t);
    }
    for (p = 0; p < EN_NPIECES; p++) {
        int16_t par[P_NPARAMS];
        uint16_t pool;
        en_piece(p, par, &pool);
        for (i = 0; i < P_NPARAMS; i++)
            CHECK(par[i] >= en_param_info(i)->lo && par[i] <= en_param_info(i)->hi, "piece %s param %s = %d",
                  en_piece_name(p), en_param_info(i)->name, par[i]);
        CHECK(par[P_SPECIES] == p, "piece %s is species %d", en_piece_name(p), par[P_SPECIES]);
        CHECK(pool & 1u, "piece %s: the root in its scale", en_piece_name(p));
    }
    {
        int steps[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], steps) == 7 && steps[2] == 3, "Dorian's scale");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], steps) == 5, "the minor pentatonic has five notes");
    }

    /* every plant plays, at a sane level, for two minutes */
    for (p = 0; p < EN_NPIECES; p++) {
        uint32_t f0;
        piece(p);
        f0 = en_fires;
        run(120.0);
        CHECK(!bad, "%s: NaN", en_piece_name(p));
        CHECK(en_fires - f0 > 20u, "%s fired %u times in 2 min", en_piece_name(p), en_fires - f0);
        CHECK(peak < 0.99, "%s: peak %.3f", en_piece_name(p), peak);
        CHECK(rms_db() > -40.0 && rms_db() < -12.0, "%s: rms %.1f dB", en_piece_name(p), rms_db());
        printf("  %-7s fires %4u  rms %5.1f dB  peak %5.1f dB  soil %.2f\n", en_piece_name(p), en_fires - f0, rms_db(),
               20.0 * log10(peak), (double)en_soil);
    }

    /* drought: no rain, a fern wilts and goes quiet; a cactus is still playing */
    {
        uint32_t fern, cactus;
        piece(EN_SP_FERN);
        en_set(P_RAIN, 0);
        en_set(P_DAY, 20);
        run(600.0);
        fern = en_fires;
        run(60.0);
        fern = en_fires - fern;
        piece(EN_SP_CACTUS);
        en_set(P_RAIN, 0);
        en_set(P_DAY, 20);
        run(600.0);
        cactus = en_fires;
        run(60.0);
        cactus = en_fires - cactus;
        CHECK(fern < 3u, "a fern in drought still fires %u a minute", fern);
        CHECK(cactus > 5u, "a cactus in drought fires only %u a minute", cactus);
        printf("  drought, after 10 min: fern %u fires a minute, cactus %u\n", fern, cactus);
    }
    /* the Sky switch: always night is mellow, not dead; always day is the busiest; the sky stays put */
    {
        uint32_t f[3];
        double r[3];
        int sk;
        for (sk = 0; sk < 3; sk++) {
            piece(EN_SP_IVY);
            en_set(P_SKY, sk);
            en_set(P_RAIN, 60);                       /* water enough: only the light differs */
            en_regrow();
            run(20.0);
            peak = sumsq = 0.0;
            frames = 0;
            f[sk] = en_fires;
            run(180.0);
            f[sk] = en_fires - f[sk];
            r[sk] = rms_db();
        }
        printf("  sky: night %u fires in 3 min (%.1f dB), cycle %u (%.1f dB), day %u (%.1f dB)\n", f[0], r[0], f[1], r[1], f[2], r[2]);
        CHECK(f[0] > 40u, "always night: only %u fires in 3 min", f[0]);
        CHECK(f[0] < f[2], "night (%u) not calmer than day (%u)", f[0], f[2]);
        CHECK(r[0] < r[2], "night (%.1f dB) not softer than day (%.1f dB)", r[0], r[2]);
        CHECK(en_sun > 0.5f && en_day > 0.49f && en_day < 0.51f, "always day: noon, sun %.2f", (double)en_sun);
        en_set(P_SKY, EN_SKY_NIGHT);              /* from noon to midnight: a dusk of a few seconds */
        run(12.0);
        CHECK(en_day < 0.001f || en_day > 0.999f, "Night: the sky at %.3f", (double)en_day);
        CHECK(en_sun < 0.01f, "Night: the sun still %.2f", (double)en_sun);
        run(30.0);
        CHECK(en_day < 0.001f || en_day > 0.999f, "Night: the sky moved on to %.3f", (double)en_day);
    }

    /* the lotus never dries: it lives in a pond */
    piece(EN_SP_LOTUS);
    en_set(P_RAIN, 0);
    run(300.0);
    CHECK(en_soil >= 0.84f, "the lotus's pond fell to %.2f", (double)en_soil);
    /* the wind sways a willow far more than a cactus */
    {
        float sw_willow = 0.0f, sw_cactus = 0.0f;
        int k;
        piece(EN_SP_WILLOW);
        en_set(P_WIND, 100);
        en_set(P_GUSTS, 100);
        run(5.0);
        for (k = 0; k < 200; k++) {
            run(0.05);
            sw_willow += fabsf(en_leaf_sway[2]);
        }
        piece(EN_SP_CACTUS);
        en_set(P_WIND, 100);
        en_set(P_GUSTS, 100);
        run(5.0);
        for (k = 0; k < 200; k++) {
            run(0.05);
            sw_cactus += fabsf(en_leaf_sway[2]);
        }
        CHECK(sw_willow > 10.0f * sw_cactus, "sway: willow %.1f, cactus %.1f", (double)sw_willow, (double)sw_cactus);
    }

    /* MIDI clock and the grid: at 120 BPM, a 1/16 grid, every firing lands on a sixteenth (6 ticks) */
    {
        const double tick = 44100.0 * 60.0 / (120.0 * 24.0);
        double next = 0.0, pos = 0.0;
        uint32_t ticks = 0, f0, offgrid = 0, ongrid = 0, last = en_fires;
        piece(EN_SP_IVY);
        en_set(P_SYNC, 1);
        en_set(P_GRID, 3);
        en_clock(EN_CLK_START);
        f0 = en_fires;
        while (pos < 44100.0 * 30.0) {
            while (pos >= next) {
                en_clock(EN_CLK_TICK);
                next += tick;
                ticks++;
            }
            en_render(blk, 64, 2800);
            pos += 64.0;
            if (en_fires != last) {               /* fired during this block: was the tick just sent a sixteenth? */
                if (ticks % 6u == 0u)
                    ongrid++;
                else
                    offgrid++;
                last = en_fires;
            }
        }
        CHECK(en_ext, "the clock is followed");
        CHECK(en_bpm > 119.0f && en_bpm < 121.0f, "the tempo read %.2f", (double)en_bpm);
        CHECK(en_fires - f0 > 10u && offgrid == 0u, "on the grid: %u on, %u off", ongrid, offgrid);
        en_clock(EN_CLK_STOP);
        run(0.1);
        CHECK(!en_playing, "STOP rests the plant");
        run(1.0);
        CHECK(!en_ext, "without the clock, its own time");
    }

    /* MIDI out: notes on the leaves' channels and the stem's; CCs on the fifth */
    piece(EN_SP_FERN);
    en_set(P_MIDI, EN_MIDI_BOTH);
    en_set(P_MCH, 4);                             /* channels 5..9 */
    run(0.1);
    midi_reset();
    run(30.0);
    CHECK(midi_notes[4] + midi_notes[5] + midi_notes[6] > 5u, "leaf notes on 5-7: %u %u %u", midi_notes[4], midi_notes[5], midi_notes[6]);
    CHECK(midi_notes[0] == 0u && midi_notes[1] == 0u, "nothing left on 1-2");
    CHECK(midi_cc[8] > 50u, "CCs on 9: %u", midi_cc[8]);
    CHECK(midi_cc_num[EN_CC_CHARGE] > 0u && midi_cc_num[EN_CC_SPARK] > 0u && midi_cc_num[EN_CC_SOIL] > 0u, "the CCs vary");
    CHECK(midi_n < 30u * 200u, "MIDI out: %u messages in 30 s", midi_n);
    en_set(P_MIDI, EN_MIDI_CC);
    run(0.1);
    midi_reset();
    run(10.0);
    CHECK(midi_notes[4] + midi_notes[5] + midi_notes[6] + midi_notes[7] == 0u, "CC only: no notes");
    en_set(P_MIDI, EN_MIDI_OFF);
    run(0.1);
    midi_reset();
    run(10.0);
    CHECK(midi_n == 0u, "MIDI off: %u messages", midi_n);

    /* the voices apart: each leaf, the stem, the weather and the reverb carry something */
    {
        double e[EN_NBUS] = {0};
        int b, k;
        piece(EN_SP_BAMBOO);
        en_bus_on = 1;
        for (k = 0; k < 44100 * 40 / 256; k++) {
            en_render(blk, 256, 2800);
            for (i = 0; i < 256; i++)
                for (b = 0; b < EN_NBUS; b++)
                    e[b] += (double)en_bus[i * EN_NBUS + b] * (double)en_bus[i * EN_NBUS + b];
        }
        en_bus_on = 0;
        for (b = 0; b < EN_NBUS; b++)
            CHECK(e[b] > 1e-3, "bus %d silent (%g)", b, e[b]);
    }

    printf(fails ? "eco: %d FAILED\n" : "eco: all ok\n", fails);
    return fails != 0;
}
