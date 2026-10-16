/* SPDX-License-Identifier: GPL-3.0-only */
/* GRAV-1's engine on the host: the parameters and the ten systems, the orbits (Kepler, shared orbits,
 * dormant worlds), the years and the Great Year, the drums and their bindings, the weather, MIDI clock and
 * out, and the levels (nothing silent that shouldn't be, nothing past full scale, no NaN).
 *   tests/run_tests.sh (or: cc -DEN_HOST -Ifirmware/src/dsp tests/host/grav_test.c firmware/src/dsp/amb.c -lm) */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint32_t midi_n, midi_notes[16], midi_cc[16];
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d2;
    midi_n++;
    if ((st & 0xF0u) == 0x90u)
        midi_notes[st & 15u]++;
    if ((st & 0xF0u) == 0xB0u && d1 != 123u)
        midi_cc[st & 15u]++;
}
static void midi_reset(void)
{
    midi_n = 0;
    memset(midi_notes, 0, sizeof midi_notes);
    memset(midi_cc, 0, sizeof midi_cc);
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
static void meter_reset(void) { peak = sumsq = 0.0; frames = 0; }
static double rms_db(void) { return 10.0 * log10(sumsq / (double)(frames * 2 > 0 ? frames * 2 : 1) + 1e-12); }

static int16_t cur[P_NPARAMS];
static void set(int p, int v) { cur[p] = (int16_t)v; en_set(p, v); }
static void piece(int i)
{
    uint16_t pool;
    int p;
    en_piece(i, cur, &pool);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, cur[p]);
    en_pool(pool);
    en_regrow();
}

int main(void)
{
    int i, p;
    char t[16];
    en_init();

    /* the parameters: defaults in range, names and texts */
    for (p = 0; p < P_NPARAMS; p++) {
        const en_param_t *pi = en_param_info(p);
        CHECK(pi->name && pi->lo <= pi->def && pi->def <= pi->hi, "param %d %s", p, pi->name);
        en_param_text(p, pi->def, t);
        CHECK(strlen(t) < 11, "param %d text %s", p, t);
    }
    en_param_text(P_PL_INST(1), 1, t);
    CHECK(!strcmp(t, "Glass bell"), "instrument name %s", t);
    en_param_text(P_PL_ECC(0), 25, t);
    CHECK(!strcmp(t, "0.25"), "ellipse %s", t);
    en_param_text(P_COLOUR, 2, t);
    CHECK(!strcmp(t, "Brown"), "colour %s", t);
    CHECK(en_period_bars(2) == 16.0f && en_period_bars(16) == 2.0f && en_period_bars(0) == 64.0f, "periods");

    /* every system: in range, it plays, it isn't too loud or too quiet */
    for (i = 0; i < EN_NPIECES; i++) {
        uint16_t pool;
        int16_t pp[P_NPARAMS];
        en_piece(i, pp, &pool);
        for (p = 0; p < P_NPARAMS; p++)
            CHECK(pp[p] >= en_param_info(p)->lo && pp[p] <= en_param_info(p)->hi, "piece %d param %d = %d", i, p, pp[p]);
        CHECK(pool & 1u, "piece %d pool", i);
        piece(i);
        en_play(1);
        meter_reset();
        {
            uint32_t n0 = en_notes, h0 = en_hits;
            run(40.0);
            printf("  %-8s notes %3u hits %3u  rms %5.1f dB  peak %.2f\n", en_piece_name(i), en_notes - n0, en_hits - h0, rms_db(), peak);
            CHECK(en_notes - n0 > 2, "%s: only %u notes", en_piece_name(i), en_notes - n0);
            CHECK(en_hits - h0 > 10, "%s: only %u hits", en_piece_name(i), en_hits - h0);
        }
        CHECK(!bad, "%s: NaN", en_piece_name(i));
        CHECK(peak < 0.99, "%s: peak %.3f", en_piece_name(i), peak);
        CHECK(rms_db() > -42.0 && rms_db() < -12.0, "%s: rms %.1f dB", en_piece_name(i), rms_db());
        en_play(0);
        run(1.0);
    }

    /* every instrument: it sounds, not too loud, not too quiet, no NaN */
    for (i = 0; i < EN_NINST; i++) {
        int k;
        piece(0);
        for (k = 0; k < EN_NPL; k++)
            set(P_PL_INST(k), i);
        set(P_STAR, 0);
        set(P_DRUMS, 0);
        set(P_RADIO, 0);
        en_play(1);
        run(6.0);
        meter_reset();
        run(24.0);
        en_param_text(P_PL_INST(0), i, t);
        printf("  %-10s rms %5.1f dB  peak %.2f\n", t, rms_db(), peak);
        CHECK(!bad, "%s: NaN", t);
        CHECK(peak < 0.99, "%s: peak %.3f", t, peak);
        CHECK(rms_db() > -40.0 && rms_db() < -14.0, "%s: rms %.1f dB", t, rms_db());
        en_play(0);
        run(1.0);
    }

    /* the orbits: more hits, a faster year, closer in */
    piece(0);
    en_play(1);
    run(8.0);                                    /* the orbits settle */
    CHECK(en_pl_a[5] < en_pl_a[0], "Sol: the shaker's planet (8 hits) inside the kick's (2): %.2f vs %.2f", (double)en_pl_a[5], (double)en_pl_a[0]);
    CHECK(en_pl_a[4] > en_pl_a[2], "Sol: the dormant planet outside the outermost: %.2f vs %.2f", (double)en_pl_a[4], (double)en_pl_a[2]);
    /* the same hits: the same orbit, a sixth of a turn apart */
    set(P_PAT + 1, 0x0101);                      /* two hits, as the kick */
    run(30.0);
    {
        float d0 = sqrtf(en_pl_x[0] * en_pl_x[0] + en_pl_y[0] * en_pl_y[0]), d1 = sqrtf(en_pl_x[1] * en_pl_x[1] + en_pl_y[1] * en_pl_y[1]);
        float sep = (float)fabs(atan2((double)en_pl_y[0], (double)en_pl_x[0]) - atan2((double)en_pl_y[1], (double)en_pl_x[1]));
        if (sep > 3.14159f)
            sep = 6.28318f - sep;
        CHECK(fabsf(en_pl_a[0] - en_pl_a[1]) < 0.01f, "shared orbit: %.3f %.3f", (double)en_pl_a[0], (double)en_pl_a[1]);
        CHECK(sep > 0.5f, "shared orbit: %.2f rad apart (d %.2f %.2f)", (double)sep, (double)d0, (double)d1);
    }

    /* years and the Great Year */
    piece(9);                                    /* Nova: quick */
    set(P_TEMPO, 90);
    set(P_GY, 1);                                /* the Great Year on the active planets (Nova has a dormant one) */
    en_play(1);
    {
        uint32_t y0 = en_years, g0 = en_greats;
        run(150.0);
        CHECK(en_years - y0 >= 6, "years: %u", en_years - y0);
        CHECK(en_greats - g0 >= 1, "great years: %u", en_greats - g0);
    }
    en_touch();                                  /* EDIT: the Great Year now */
    {
        uint32_t g0 = en_greats;
        run(0.1);
        CHECK(en_greats == g0 + 1, "EDIT: great year");
    }

    /* a dormant planet wakes when a big neighbour passes */
    piece(8);                                    /* Kuiper: three dormant worlds */
    set(P_TIDE, 100);
    set(P_REACH, 40);
    set(P_WAKE, 100);
    en_play(1);
    {
        uint32_t w0 = en_wakes;
        run(240.0);
        CHECK(en_wakes > w0, "Kuiper: no planet woke in 4 minutes");
    }

    /* the weather, called up (a cloud already there: called off first) */
    set(P_DUST, 0);
    run(0.1);
    if (en_dust > 0.05f) {
        en_summon(0);
        run(40.0);
    }
    en_summon(0);
    en_summon(1);
    en_summon(2);
    run(0.2);
    CHECK(en_rock_on, "an asteroid crossing");
    CHECK(en_comet_on, "a comet falling");
    run(10.0);
    CHECK(en_dust > 0.2f, "a dust cloud thickening: %.2f", (double)en_dust);
    {
        uint32_t b0 = 0;
        int k;
        for (k = 0; k < 400 && en_comet_on; k++)
            run(0.25);
        CHECK(!en_comet_on, "the comet burned up");
        (void)b0;
    }
    CHECK(!bad, "weather: NaN");

    /* eclipse: the orbits hold */
    en_freeze(1);
    run(0.1);
    {
        float x = en_pl_x[0];
        uint32_t h0 = en_hits;
        run(3.0);
        CHECK(fabsf(en_pl_x[0] - x) < 1e-6f, "eclipse: planet 1 moved");
        CHECK(en_hits == h0, "eclipse: drums played");
    }
    en_freeze(0);

    /* MIDI out: planets on their channels, the drums on the seventh, CCs on the eighth */
    piece(0);
    set(P_MIDI, EN_MIDI_BOTH);
    set(P_MCH, 2);
    en_play(1);
    midi_reset();
    run(30.0);
    CHECK(midi_notes[2 + 6] > 0, "drums on channel 9: %u", midi_notes[8]);
    CHECK(midi_notes[2] + midi_notes[3] + midi_notes[4] + midi_notes[5] > 0, "planet notes");
    CHECK(midi_cc[2 + 7] > 0, "CCs on channel 10");

    /* MIDI clock: the steps follow the ticks */
    set(P_SYNC, 1);
    en_clock(EN_CLK_START);
    {
        uint32_t h0, k;
        run(0.05);
        h0 = en_hits;
        for (k = 0; k < 96 * 4; k++) {           /* four bars at 120 */
            en_clock(EN_CLK_TICK);
            int r;
            for (r = 0; r < 7; r++)              /* 919 samples a tick: 120 bpm */
                en_render(blk, 128, 2800);
            en_render(blk, 23, 2800);
        }
        CHECK(en_ext, "on the clock");
        CHECK(en_bpm > 118.0f && en_bpm < 122.0f, "the clock's tempo %.1f", (double)en_bpm);
        CHECK(en_hits - h0 > 70 && en_hits - h0 < 90, "Sol's drums over four bars of clock: %u", en_hits - h0);
    }
    en_clock(EN_CLK_STOP);
    run(0.1);
    CHECK(!en_playing, "clock STOP rests it");

    /* resting is silent in the end */
    run(20.0);
    meter_reset();
    run(2.0);
    CHECK(peak < 0.003, "resting: peak %.4f", peak);

    printf(fails ? "grav: %d FAILED\n" : "grav: all passed\n", fails);
    return fails != 0;
}
