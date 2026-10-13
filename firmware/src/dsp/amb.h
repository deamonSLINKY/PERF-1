/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: HYP-1, hypnotic techno and acid for the FM-1 (a fork of AMB-1), built like a small
 * modular: generative voices you shape over time, over drums you sequence. The voices draw on one pool
 * of notes (a scale) and one chord that drifts slowly through it:
 *
 *   PAD     four breathing voices (Rhodes or Voices), each breath its own envelope (attack, hold, release,
 *           rest) and the chord forming and dissolving through them; an LFO on its filter, level, pan or pitch.
 *   NOISE   a bed of noise in eight colours (white to black), filtered, pumping with the kick.
 *   DRONE   three detuned saws and a sub-octave through a 24 dB low-pass, on the chord's root; an envelope
 *           on its filter (on each chord, or every 1 .. 8 bars) and an LFO (16 bars, a sine, by default).
 *   ACID    a generative 303-style line: a loop of 1 .. 16 sixteenths of chord tones with accents and
 *           slides, a resonant low-pass with an envelope; MUTATE re-rolls its steps as it goes.
 *   DRUMS   a sixteen-step sequencer, six lanes (kick, snare, clap, closed hat, open hat, perc), six
 *           patterns; a new pattern waits for the end of the bar. The kick ducks everything else.
 *
 * The sections are the player's: a breakdown, a build, the drop (en_section). Over it all the ARC, a slow
 * tide between sparse and full (MOTION). Then a tempo-synced tape echo and an Echoplex, Dattorro's plate,
 * and the held performance effects (en_fx).
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere (the drum patterns, en_pats, are the exception: the UI edits them, the render reads them).
 * Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NDRUM 6                     /* the drum lanes: kick, snare, clap, closed hat, open hat, perc */
#define EN_NLANE EN_NDRUM
#define EN_NPAT 6                      /* drum patterns */
#define EN_NSTEP 16
#define EN_NPAD 4                      /* pad voices */
#define EN_NTHEME 5                    /* the screen's colour themes (the UI's; kept with the settings) */
#define EN_NSTYLE 5                    /* the styles: hypnotic, house, techno, acid, breakcore */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const char *const EN_LANE_NAME[EN_NLANE];
extern const char *const EN_STYLE_NAME[EN_NSTYLE];
void en_style_tempo(int s, int *lo, int *hi);  /* a style's usual tempo range (switching style keeps to it) */

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_ACID, P_BASS, P_PULSE, P_NOISE,              /* the layers' levels */
    P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT,                 /* tempo */
    P_BRIGHT, P_PADTONE, P_DETUNE,                        /* pad */
    P_PE_ATK, P_PE_HOLD, P_PE_REL, P_PE_REST,             /* pad envelope (the breath) */
    P_PL_RATE, P_PL_DEPTH, P_PL_SHAPE, P_PL_DEST,         /* pad LFO */
    P_NZ_COLOR, P_NZ_TONE, P_NZ_PUMP,                     /* noise */
    P_BASSOCT, P_DR_CUT, P_DR_RES,                        /* drone */
    P_DE_TRIG, P_DE_ATK, P_DE_DEC, P_DE_AMT,              /* drone envelope */
    P_DL_RATE, P_DL_DEPTH, P_DL_SHAPE, P_DL_DEST,         /* drone LFO */
    P_AC_CUT, P_AC_RES, P_AC_ENV,                         /* acid */
    P_AC_DEC, P_AC_ACC, P_AC_SLIDE, P_AC_WAVE,            /* acid envelope */
    P_AC_DENS, P_AC_LEN, P_AC_RANGE, P_AC_MUT,            /* acid line */
    P_AC_OCT,                                             /* OCT- / OCT+ */
    P_DRIVE, P_SWING, P_PATTERN,                          /* drums */
    P_LANE,                                               /* each lane's level, tune, decay: P_LANE + 3 * lane + 0..2 */
    P_ECHO = P_LANE + 3 * EN_NLANE, P_ETIME, P_REVERB, P_SIZE,   /* space */
    P_ROOT, P_MODE, P_TUNE, P_MIDI, P_THEME,              /* global (the mode on ALGORITHM) */
    P_STYLE,                                              /* the piece's style (SEL + ALGORITHM) */
    P_LINKS, P_LK_KICK, P_LK_SNARE, P_LK_HAT, P_LK_PERC,  /* the drums into the voices: on / off (SEL + SEQ);
                                                           * kick > drone, snare and clap > pad, hats > acid
                                                           * accents, perc > the acid's clock */
    P_NPARAMS
};
#define P_LANE_LVL(l) (P_LANE + 3 * (l))
#define P_LANE_TUNE(l) (P_LANE + 3 * (l) + 1)
#define P_LANE_DEC(l) (P_LANE + 3 * (l) + 2)
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* play / stop (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */

/* the sections: a breakdown, a build, the drop, each taken on the next bar. A breakdown lasts until
 * a build (four bars, then the drop) or a drop is asked for; a build cut short by a drop drops. */
enum { EN_SEC_GROOVE, EN_SEC_BREAK, EN_SEC_BUILD };
enum { EN_CMD_BREAK, EN_CMD_BUILD, EN_CMD_DROP };
void en_section(int cmd);
extern volatile uint8_t en_sec;                /* the section now: EN_SEC_* (UI) */
extern volatile uint8_t en_sec_req;            /* a command waiting for the bar: EN_CMD_* + 1, or 0 (UI) */

/* performance effects, held: one bit each, all at once as a mask (the UI sends it every frame). The
 * tracks, for mute and solo: kick; hats (closed and open); snare (snare, clap, perc); drone; acid;
 * pad (with the noise). */
enum {
    FX_RPT2, FX_RPT4, FX_RPT6, FX_RPT8, FX_RPT16, FX_RPT32,   /* beat repeat: the last 1/2 .. 1/32 note, looped */
    FX_REV, FX_STOP,                                          /* reverse; tape stop */
    FX_MUTE0, FX_MUTE1, FX_MUTE2, FX_MUTE3, FX_MUTE4, FX_MUTE5,
    FX_LP, FX_HP,                                             /* the filters closing in */
    FX_GATE16, FX_GATE8,                                      /* a trance gate on the synths */
    FX_THROW, FX_CRUSH, FX_FILL,                              /* an echo throw; a bit crusher; a drum fill */
    FX_SOLO0, FX_SOLO1, FX_SOLO2, FX_SOLO3, FX_SOLO4, FX_SOLO5,
    EN_NFX
};
#define EN_NTRACK 6
extern const char *const EN_FX_NAME[EN_NFX];   /* short names, for the screen */
void en_fx(uint32_t mask);
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile float en_noise_level, en_acid_level;
extern volatile uint8_t en_acid_len, en_acid_pos;    /* the acid loop's length and step */
extern volatile uint16_t en_acid_gate, en_acid_acc;  /* its steps that play; those accented */
extern volatile float en_acid_glow;
extern volatile uint8_t en_drum_pos;           /* the step playing (0..15) */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint8_t en_pat_now, en_pat_next;      /* the pattern playing; the one waiting for the bar */
extern volatile uint16_t en_pats[EN_NPAT][EN_NLANE];   /* the patterns: bit s, a hit on step s (the UI edits) */
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is out (a breakdown) */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_par_global(int p);                      /* the player's, not a piece's: Tune, MIDI, Theme */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* the pieces: recipes for everything but Tune, MIDI out and Theme (the player's), and six drum patterns
 * each. Named by number, 01 .. 10. */
#define EN_NPIECES 10
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
void en_piece_pats(int i, uint16_t pats[EN_NPAT][EN_NLANE]);   /* its factory drum patterns */
