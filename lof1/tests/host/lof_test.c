/* SPDX-License-Identifier: GPL-3.0-only */
/* LOF-1's engine on the host, without the UI: every preset plays (the band, the changes, the drums), the
 * band follows the chord the lead implies, MIDI out goes on its channels (lead 1, comp 2, bass 3, answer 4,
 * drums 10), Daydream dreams, every effect is safe, and the levels (nothing silent that shouldn't be,
 * nothing past full scale, no NaN).
 *   tests/run_tests.sh (or: cc -DEN_HOST -Ifirmware/src/dsp tests/host/lof_test.c firmware/src/dsp/amb.c -lm) */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint32_t midi_notes[16];
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d1;
    if ((st & 0xF0u) == 0x90u && d2)
        midi_notes[st & 15u]++;
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
static void meter_reset(void)
{
    peak = sumsq = 0.0;
    frames = 0;
    bad = 0;
}
static double rms_db(void) { return 10.0 * log10(sumsq / (double)(frames * 2 + 1) + 1e-20); }
static double peak_db(void) { return 20.0 * log10(peak + 1e-20); }

static void preset(int p)
{
    int16_t par[P_NPARAMS];
    int i;
    en_piece(p, par);
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, par[i]);
}

int main(void)
{
    int p, f;
    uint32_t n0, h0, c0;
    en_init();
    en_pats_default();

    /* every preset plays on its own: drums, chord changes, at a sane level */
    for (p = 0; p < EN_NPIECES; p++) {
        preset(p);
        en_play(1);
        meter_reset();
        h0 = en_hits;
        c0 = en_changes;
        run(10.0);
        CHECK(!bad, "preset %d: NaN", p + 1);
        CHECK(rms_db() > -45.0, "preset %d: too quiet (%.1f dB rms)", p + 1, rms_db());
        CHECK(peak < 1.0, "preset %d: clips (peak %.2f dBFS)", p + 1, peak_db());
        CHECK(en_hits - h0 > 20, "preset %d: %u drum hits", p + 1, (unsigned)(en_hits - h0));
        CHECK(en_changes - c0 >= 1, "preset %d: no chord change in 10 s", p + 1);
        printf("  preset %02d: %.1f dB rms, peak %.1f dBFS, %u hits, %u changes\n", p + 1, rms_db(), peak_db(),
               (unsigned)(en_hits - h0), (unsigned)(en_changes - c0));
        en_play(0);
        run(1.0);
    }

    /* the band follows: preset 01 (D Dorian), hold G B D F and the chord goes to G */
    preset(0);
    en_set(P_DD_ON, 0);
    en_play(1);
    run(2.0);
    n0 = en_notes;
    memset(midi_notes, 0, sizeof midi_notes);
    en_note(55, 1, 100);
    en_note(59, 1, 100);
    en_note(62, 1, 100);
    en_note(65, 1, 100);
    run(4.0);
    CHECK(en_chord_root == 7, "held G B D F: the chord's root is %d, not G", en_chord_root);
    CHECK(en_notes > n0, "the lead played no notes");
    en_note(55, 0, 0);
    en_note(59, 0, 0);
    en_note(62, 0, 0);
    en_note(65, 0, 0);
    run(2.0);
    CHECK(midi_notes[0] > 0, "MIDI out: no lead notes on channel 1");
    CHECK(midi_notes[1] > 0, "MIDI out: no comp notes on channel 2");
    CHECK(midi_notes[2] > 0, "MIDI out: no bass notes on channel 3");
    CHECK(midi_notes[9] > 0, "MIDI out: no drums on channel 10");

    /* chord hold: the chord stays put */
    en_hold(1);
    run(0.5);
    c0 = en_changes;
    run(8.0);
    CHECK(en_changes == c0, "chord hold: %u changes", (unsigned)(en_changes - c0));
    en_hold(0);

    /* Daydream, always: the ghost plays */
    en_set(P_DD_ON, 1);
    en_set(P_DD_WAIT, 3);
    n0 = en_dream_notes;
    run(8.0);
    CHECK(en_dreaming, "Daydream (Always) isn't dreaming");
    CHECK(en_dream_notes > n0 + 3, "Daydream: %u notes", (unsigned)(en_dream_notes - n0));

    /* every instrument on every voice that offers it: heard, in tune enough to have energy, no NaN */
    {
        static const struct { int par, n; } V[4] = {{P_LEAD_INST, EN_NLEAD}, {P_COMP_INST, EN_NCOMP},
                                                    {P_BASS_INST, EN_NBASS}, {P_ANS_INST, EN_NANS}};
        int v, i;
        en_hold(0);
        en_set(P_DD_ON, 0);
        for (v = 0; v < 4; v++)
            for (i = 0; i < V[v].n; i++) {
                en_set(V[v].par, i);
                meter_reset();
                if (v == 0) {
                    en_note(60, 1, 100);
                    run(0.6);
                    en_note(60, 0, 0);
                    run(0.4);
                } else {
                    run(2.0);
                }
                CHECK(!bad, "voice %d instrument %d: NaN", v, i);
                CHECK(rms_db() > -50.0, "voice %d instrument %d: silent (%.1f dB)", v, i, rms_db());
                CHECK(peak < 1.0, "voice %d instrument %d: clips", v, i);
            }
        en_set(P_LEAD_INST, 0);
        en_set(P_COMP_INST, 0);
        en_set(P_BASS_INST, 0);
        en_set(P_ANS_INST, 0);
    }

    /* every effect, alone, then all the space at full: no NaN, no clipping */
    for (f = 0; f < EN_NFX; f++) {
        meter_reset();
        en_fx(1u << f);
        run(1.5);
        en_fx(0);
        run(0.5);
        CHECK(!bad, "%s: NaN", EN_FX_NAME[f]);
        CHECK(peak < 1.0, "%s: clips (peak %.2f dBFS)", EN_FX_NAME[f], peak_db());
    }
    meter_reset();
    en_set(P_ROOM, 100);
    en_set(P_ECHO, 100);
    en_set(P_TAPE, 100);
    en_set(P_GRAIN, 100);
    run(10.0);
    CHECK(!bad, "full space: NaN");
    CHECK(peak < 1.0, "full space: clips (peak %.2f dBFS)", peak_db());

    /* stopped: quiet within a few seconds (the run-out may crackle on) */
    en_set(P_DUST_LVL, 0);
    en_play(0);
    run(6.0);
    meter_reset();
    run(1.0);
    CHECK(rms_db() < -60.0, "stopped: %.1f dB rms", rms_db());

    printf(fails ? "lof: %d FAILED\n" : "lof: all ok\n", fails);
    return fails ? 1 : 0;
}
