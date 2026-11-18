/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): a piece rendered for the perf-sound-check skill (.claude/skills/perf-sound-check). The music as a
 * float WAV (stereo, 44.1 kHz, the engine's own scale: 1.0 = 2^23) and its MIDI out as a Standard MIDI File (format 0, 480 per
 * quarter, the piece's tempo), from the same run, with a fixed seed so two builds can be compared sample for sample.
 * Plays as the unit starts: FORM Song, the factory piece, every effect at its default, MIDI out on (the map's channels).
 *   gsound genre piece bars out_prefix   (piece 1 ..; writes out_prefix.wav and out_prefix.mid; LOOP=1 for FORM Loop) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "amb.h"

#define SR 44100
void en_test_reseed(uint32_t s);

typedef struct { long t; uint8_t st, d1, d2; } ev_t;
static ev_t *ev;
static int nev, cap;
static long now;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st >= 0xF0u)
        return;                                  /* (clock and the like: not in the file) */
    if (nev == cap) {
        cap = cap ? cap * 2 : 65536;
        ev = realloc(ev, (size_t)cap * sizeof *ev);
        if (!ev)
            exit(1);
    }
    ev[nev].t = now;
    ev[nev].st = (uint8_t)st;
    ev[nev].d1 = (uint8_t)(d1 & 0x7Fu);
    ev[nev].d2 = (uint8_t)(d2 & 0x7Fu);
    nev++;
}

static void le32(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); fputc((int)(v >> 16 & 255u), f); fputc((int)(v >> 24), f); }
static void le16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void be32(FILE *f, uint32_t v) { fputc((int)(v >> 24), f); fputc((int)(v >> 16 & 255u), f); fputc((int)(v >> 8 & 255u), f); fputc((int)(v & 255u), f); }
static int vlq(uint8_t *o, uint32_t v)
{
    uint8_t t[5];
    int n = 0, i;
    do {
        t[n++] = (uint8_t)(v & 0x7Fu);
        v >>= 7;
    } while (v);
    for (i = 0; i < n; i++)
        o[i] = (uint8_t)(t[n - 1 - i] | (i < n - 1 ? 0x80u : 0u));
    return n;
}

int main(int argc, char **argv)
{
    static int32_t buf[512];
    int16_t par[P_NPARAMS];
    uint16_t pool, pats[EN_NPAT][EN_NLANE];
    int p, l, f, i;
    char path[512];
    FILE *w, *m;
    long frames, fr = 0, dn = 0, dc1 = 0, dl1 = 0;
    double bpm, dc = 0.0, dl = 0.0, dp = 0.0;
    float dcm = 0.0f, dlm = 0.0f, dpm = 0.0f;
    if (argc < 5) {
        fprintf(stderr, "gsound genre piece bars out_prefix\n");
        return 2;
    }
    const int g = atoi(argv[1]), pc = atoi(argv[2]) - 1, bars = atoi(argv[3]);
    for (f = 0; f < EN_NFXS; f++)
        en_fxs(f, EN_FXS_DEF[f]);
    en_init();
    en_test_reseed(7);
    en_genre(g);
    en_piece_g(g, pc, par, &pool);
    en_piece_pats_g(g, pc, pats);
    for (p = 0; p < P_NPARAMS; p++)
        en_set(p, par[p]);
    en_set(P_MIDI, 1);
    if (getenv("FXS")) {                         /* FXS="index:value,...": the unit's settings (EN_FXS_*) changed, e.g. the CHORUS page (after en_init: it drops what was posted before) */
        const char *s = getenv("FXS");
        while (*s) {
            char *e;
            const long k = strtol(s, &e, 10);
            if (*e != ':')
                break;
            en_fxs((int)k, (int)strtol(e + 1, &e, 10));
            s = *e == ',' ? e + 1 : e;
        }
    }
    if (getenv("SET")) {                         /* SET="param:value,param:value": parameters changed after the piece's (an A/B of one setting) */
        const char *s = getenv("SET");
        while (*s) {
            char *e;
            const long k = strtol(s, &e, 10);
            if (*e != ':')
                break;
            par[k] = (int16_t)strtol(e + 1, &e, 10);
            en_set((int)k, par[k]);
            s = *e == ',' ? e + 1 : e;
        }
    }
    en_pool(pool);
    en_trim(g, getenv("NOTRIM") ? -1 : pc);      /* the piece's output level, as the unit sets it (NOTRIM=1: the raw mix, to measure it) */
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = pats[p][l];
    en_song = (uint8_t)(getenv("LOOP") ? 0 : 1);
    if (getenv("RD")) {                          /* (0.17.72) RD=mode,depth,release: the RETURNS page (0 Kit, 1 Off, 2 Custom) */
        int rm = 0, rdp = 60, rr = 2;
        if (sscanf(getenv("RD"), "%d,%d,%d", &rm, &rdp, &rr) >= 1) {
            en_fxs(EN_FXS_RD_MODE, rm);
            en_fxs(EN_FXS_RD_DEPTH, rdp);
            en_fxs(EN_FXS_RD_REL, rr);
        }
    }
    en_regrow();
    en_play(1);
    bpm = par[P_TEMPO];
    frames = (long)((double)bars * 4.0 * 60.0 / bpm * SR);
    snprintf(path, sizeof path, "%s.wav", argv[4]);
    w = fopen(path, "wb");
    if (!w)
        return 1;
    fwrite("RIFF", 1, 4, w); le32(w, 0); fwrite("WAVEfmt ", 1, 8, w); le32(w, 16);
    le16(w, 3); le16(w, 2); le32(w, SR); le32(w, SR * 8); le16(w, 8); le16(w, 32);   /* IEEE float, stereo */
    fwrite("data", 1, 4, w); le32(w, 0);
    while (fr < frames) {
        const int n = frames - fr < 256 ? (int)(frames - fr) : 256;
        now = fr;
        if (getenv("SNIP")) {                    /* SNIP=k: snip k (the sampler's key) on each beat, let go half a beat later */
            const long beat = (long)(60.0 / bpm * SR), ph = fr % beat;
            if (ph < n)
                en_note_v(EN_V_SAMPLE, 53 + atoi(getenv("SNIP")), 100);
            else if (ph >= beat / 2 && ph - n < beat / 2)
                en_note_v(EN_V_SAMPLE, 53 + atoi(getenv("SNIP")), 0);
        }
        if (getenv("BRK")) {                     /* BRK=bar: a breakdown asked for half a bar before that bar (0-based), as SAVE would */
            const long at = (long)((atof(getenv("BRK")) - 0.5) * 4.0 * 60.0 / bpm * SR);
            if (fr <= at && fr + n > at)
                en_section(EN_CMD_BREAK);
        }
        en_render(buf, (uint32_t)n, 4096);
        for (i = 0; i < 2 * n; i++) {
            const float x = (float)buf[i] / 8388608.0f;
            fwrite(&x, 4, 1, w);
        }
        fr += n;
        if (getenv("DYN")) {                    /* DYN=1: the master's gain reduction, a block at a time, summed for the line at the end */
            dn++;
            dc += en_comp_gr, dl += en_lim_gr;
            dc1 += en_comp_gr > 1.0f, dl1 += en_lim_gr > 0.5f;
            if (en_comp_gr > dcm) dcm = en_comp_gr;
            if (en_lim_gr > dlm) dlm = en_lim_gr;
            dp += en_out_peak;
            if (en_out_peak > dpm) dpm = en_out_peak;
        }
    }
    if (dn)
        printf("DYN %s comp mean %.2f >1dB %.0f%% max %.1f  limit mean %.2f >0.5dB %.0f%% max %.1f  peak mean %.1f max %.1f dB  thr %.1f ratio %d make %.1f\n",
               argv[4], dc / dn, 100.0 * dc1 / dn, dcm, dl / dn, 100.0 * dl1 / dn, dlm, 20.0 * log10(dp / dn + 1e-9), 20.0 * log10(dpm + 1e-9),
               -30.0 + 0.3 * par[P_MC_THR], par[P_MC_RATIO], 0.12 * par[P_MC_MAKE]);
    {
        const uint32_t data = (uint32_t)frames * 8u;
        fseek(w, 4, SEEK_SET); le32(w, 36u + data);
        fseek(w, 40, SEEK_SET); le32(w, data);
    }
    fclose(w);
    en_play(0);                                  /* stopped after the last bar: its note-offs end the MIDI (not in the WAV) */
    for (i = 0; i < 2 * SR / 256; i++) {
        now = frames + (long)i * 256;
        en_render(buf, 256, 4096);
    }
    /* the MIDI: one track, the tempo, then each event at its sample (480 per quarter) */
    snprintf(path, sizeof path, "%s.mid", argv[4]);
    m = fopen(path, "wb");
    if (!m)
        return 1;
    {
        uint8_t *trk = malloc((size_t)nev * 9u + 64u), *o = trk;
        const uint32_t us = (uint32_t)(60000000.0 / bpm + 0.5);
        long last = 0;
        *o++ = 0; *o++ = 0xFF; *o++ = 0x51; *o++ = 3; *o++ = (uint8_t)(us >> 16); *o++ = (uint8_t)(us >> 8); *o++ = (uint8_t)us;
        *o++ = 0; *o++ = 0xFF; *o++ = 0x58; *o++ = 4; *o++ = 4; *o++ = 2; *o++ = 24; *o++ = 8;
        for (i = 0; i < nev; i++) {
            const long tk = (long)((double)ev[i].t / SR * bpm / 60.0 * 480.0 + 0.5);
            const int two = (ev[i].st & 0xF0u) == 0xC0u || (ev[i].st & 0xF0u) == 0xD0u;
            o += vlq(o, (uint32_t)(tk - last));
            last = tk;
            *o++ = ev[i].st;
            *o++ = ev[i].d1;
            if (!two)
                *o++ = ev[i].d2;
        }
        *o++ = 0; *o++ = 0xFF; *o++ = 0x2F; *o++ = 0;
        fwrite("MThd", 1, 4, m); be32(m, 6); fputc(0, m); fputc(0, m); fputc(0, m); fputc(1, m); fputc(480 >> 8, m); fputc(480 & 255, m);
        fwrite("MTrk", 1, 4, m); be32(m, (uint32_t)(o - trk)); fwrite(trk, 1, (size_t)(o - trk), m);
        fclose(m);
        free(trk);
    }
    printf("%s %02d: %d BPM, %d bars, %.1f s, %d MIDI events -> %s.wav/.mid\n", en_genre_name(g), pc + 1, (int)bpm, bars, (double)frames / SR, nev, argv[4]);
    return 0;
}
