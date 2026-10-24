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
#define EN_NSTYLE 5                    /* the old styles (before 1.0.27): a stored piece's is turned into sounds
                                        * and a kit (en_style_sounds); they were kits 0..4 */
#define EN_NSOUND 24                   /* the sounds each of pad and drone can play */
#define EN_NACID 27                    /* the acid's (three more: a supersaw, a CZ, a mini) */
#define EN_NCMFM 22                    /* the counter-melody's six-operator FM sounds (the DX7 voices of fm6.inc): after the plain ones */
#define EN_NCMSND (12 + EN_NCMFM)      /* the counter-melody's sounds */
#define EN_NCMSTYLE 6                  /* its styles */
#define EN_NKIT 14                      /* the sounds each drum (kick, snare + clap, hats, perc) can play */
enum { EN_V_PAD, EN_V_DRONE, EN_V_ACID, EN_V_SAMPLE, EN_V_PLUCK };   /* the voices the keys can play (en_note) */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const char *const EN_LANE_NAME[EN_NLANE];
extern const char *const EN_PAD_SOUND[EN_NSOUND], *const EN_DRONE_SOUND[EN_NSOUND], *const EN_ACID_SOUND[EN_NACID];
extern const char *const EN_KIT_NAME[EN_NKIT];
extern const char *const EN_CM_SOUND[EN_NCMSND], *const EN_CM_STYLE[EN_NCMSTYLE], *const EN_CM_RATE[4];
extern volatile float en_cm_level;             /* the counter-melody's level, for the screen */
void en_style_sounds(int16_t *par);             /* a piece stored with a style (before 1.0.27): its sounds and
                                                 * kit set as the style had them, the style cleared */

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
    P_STYLE,                                              /* kept for the bank's layout: a piece stored before
                                                           * 1.0.27 has its style here (read once, then 0) */
    P_LINKS, P_LK_KICK, P_LK_SNARE, P_LK_HAT, P_LK_PERC,  /* the drums into the voices: on / off (SEL + SEQ);
                                                           * kick > drone, snare and clap > pad, hats > acid
                                                           * accents, perc > the acid's clock */
    P_OCT, P_PAD_OCT,                                     /* the whole song's octave; the pad's (OCT- / OCT+;
                                                           * the drone's is P_BASSOCT, the acid's P_AC_OCT) */
    P_DR_INST,                                            /* the drone's sound (the pad's: P_PADTONE, the acid's:
                                                           * P_AC_WAVE) */
    P_KIT_KICK, P_KIT_SNARE, P_KIT_HAT, P_KIT_PERC,       /* the kit: each drum's sound (snare: and the clap;
                                                           * hats: closed and open) */
    P_PAD_MAN, P_DR_MAN, P_AC_MAN,                        /* a voice off the algorithm (SEL + ENV on its page):
                                                           * silent but for the keys (en_note) */
    P_CHORD, P_VOICING,                                   /* the pad's chords: their kind (EN_CH_*), close or
                                                           * open (drop 2) */
    P_DD_CHANGE, P_DD_EVERY, P_DD_POOL, P_DD_MUTATE,      /* drum drift: the patterns change by themselves (Off,
                                                           * Next, Random), when (with the chord, or every 4-64
                                                           * bars), from patterns 1..pool, a fill the bar before;
                                                           * and mutate (Off, Shift, Morph): hits move, come, go */
    P_CM_LVL, P_CM_SND, P_CM_DENS, P_CM_OCT,              /* the counter-melody (PLUCK): level, sound, density, octave */
    P_CM_DEC, P_CM_STYLE, P_CM_RATE, P_CM_RANGE,          /* decay, style (up, down, ..), rate (1/8, 1/16, 1/4, 3/16), range in octaves */
    P_PROB, P_PROB1, P_PROB2, P_PROB3, P_PROB4, P_PROB5,  /* each drum lane's chance a hit sounds (P_PROB + lane), 100 always: thinned from the sequencer */
    P_PAN, P_PAN1, P_PAN2, P_PAN3, P_PAN4,                /* the mixer: each layer's place (pad, drone, acid, pluck, drums; 50 the middle) */
    P_SEND, P_SEND1, P_SEND2, P_SEND3, P_SEND4,           /* and how much of it goes to the echo and the reverb (50 as it was) */
    P_CM_MAN,                                             /* the pluck off the algorithm (the JAM page plays it by hand: engine-only, the page sets it) */
    P_RV_TYPE, P_RV_DAMP, P_RV_PRE, P_RV_X,               /* the reverb: Plate / Shimmer / Gated, its damping, its predelay, and the type's own amount (the shimmer's, the gate's hold) */
    P_PL_CHO,                                             /* the pad's ensemble chorus: three modulated delays a side, how much of them */
    P_MC_RATIO, P_MC_THR, P_MC_MAKE, P_ML_CEIL,           /* the master bus: compressor ratio (Off .. 10:1), threshold, make-up gain, and the limiter's ceiling */
    P_LLEN, P_LLEN1, P_LLEN2, P_LLEN3, P_LLEN4, P_LLEN5,  /* each drum lane's length in steps (P_LLEN + lane), 16 the bar: shorter ones run against it (polymeter) */
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
/* The experimental second core: the plate reverb as a job another core can run (see render_chunk). submit runs
 * fn(ctx) there and returns 1, or returns 0 (run it here); join returns 1 when the job is done, 0 when that core was
 * lost (its results are dropped, the reverb restarts). Without it, or after a loss, the same job runs in place. fence publishes
 * memory between the cores (the job's stages); alive says whether the other core still runs (0: join it, it is retired). */
void en_core2_attach(int (*submit)(void (*fn)(void *), void *ctx), int (*join)(void), void (*fence)(void), int (*alive)(void));
extern volatile float en_comp_gr, en_lim_gr, en_out_peak;   /* the master bus now: the compressor's and the limiter's gain reduction (dB, positive), the output's peak (0..1 of full scale) */
void en_shed(int level);                       /* the load is too high: 0 none, 1 the DX voices to four and no chorus, 2 to two (the UI watches the CPU and says) */
extern volatile uint8_t en_shed_level;
void en_flx_hold(int on);                      /* a Flex Beat pad is held down (not latched): a one shot goes on, bar after bar, while it is, and ends on the bar line after it is let go */
void en_fill_next(void);                       /* a drum fill in the next bar (the auto-DJ asks one bar ahead: the fill is the last bar before the break) */
void en_dj_sync(int on);                       /* the auto-DJ follows the chord changes: the engine fills the bar before one and breaks on it */
extern volatile float en_drift_bars;           /* bars to the next chord change (the drift's), 99 when none is coming */
extern uint32_t en_c2_jobs, en_c2_lost;          /* chunks reverbed on the other core; times it was lost */
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* play / stop (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_note(int voice, int note, int on);      /* a key played on the page of a voice off the algorithm (P_*_MAN):
                                                 * EN_V_*, MIDI note, on / off */
void en_mute(int on);                          /* remote: silent, no MIDI out (1) or back (0), in turn */
void en_hush(int on);                          /* a store while playing: fade out (1), back in (0) */
extern volatile uint8_t en_hushed;             /* faded right out (the flash can be written) */
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
 * tracks, for mute and solo: kick; hats (closed and open); snare (snare and clap); perc; drone; acid;
 * pad (and the vinyl). The FX menu holds the four drums' mutes; every track's mute and solo, latched,
 * is en_trk (GLO held, on the keys). Bits 59..62 are spare; 63 is the UI's "no effect". */
enum {
    FX_RPT2, FX_RPT4, FX_RPT6, FX_RPT8, FX_RPT16, FX_RPT32,   /* beat repeat: the last 1/2 .. 1/32 note, looped */
    FX_REV, FX_STOP,                                          /* reverse; tape stop */
    FX_MUTE_KICK, FX_MUTE_HATS, FX_MUTE_SNARE, FX_MUTE_PERC,  /* the drums' mutes (momentary) */
    FX_LP, FX_HP,                                             /* the filters closing in */
    FX_GATE16, FX_GATE8, FX_GATEX,                            /* a trance gate on the synths: 1/16, 1/8, a pattern */
    FX_THROW, FX_FILL,                                        /* an echo throw; a drum fill */
    FX_PHASER, FX_FLANGER, FX_TAPE,                           /* a phaser; a flanger; tape wow and flutter */
    FX_ROLL, FX_PUMP, FX_RISER, FX_VINYL,                     /* a roll that speeds up; a sidechain pump; a build; vinyl dust */
    FX_SCATTER, FX_BRAKE, FX_SPIN, FX_HALF,                   /* the last slices shuffled; a vinyl brake; a spin-back; half time */
    FX_SWEEP,                                                 /* a filter swept in time with the tempo */
    FX_SCRATCH, FX_FREEZE, FX_RVSWELL, FX_RING,               /* an automatic scratch; a granular freeze; a reverse swell; a ring modulator */
    FX_RPT8T, FX_RPT16T,                                      /* beat repeats in triplets: 1/8, 1/16 */
    FX_FLEX1, FX_FLEX2, FX_FLEX3, FX_FLEX4, FX_FLEX5, FX_FLEX6, FX_FLEX7, FX_FLEX8,   /* Flex Beat: fifteen one-bar patterns (en_flex_step) */
    FX_FLEX9, FX_FLEX10, FX_FLEX11, FX_FLEX12, FX_FLEX13, FX_FLEX14, FX_FLEX15,
    FX_TALK, FX_WAH,                                          /* a vowel filter sweeping across everything; an auto-wah that follows the loudness */
    FX_HATROLL, FX_HATTRIP, FX_SNROLL, FX_FLAM, FX_DBLKICK,   /* drum rolls (held): the hats in 32nds; in triplets; a snare roll; flams on snare and clap; a double kick */
    EN_NFX = 64                                               /* the mask is 64 bits: FXB(bit) */
};
#define FXB(x) (1ull << (x))
enum { EN_TR_KICK, EN_TR_HATS, EN_TR_SNARE, EN_TR_PERC, EN_TR_DRONE, EN_TR_ACID, EN_TR_PAD, EN_TR_PLUCK, EN_NTRACK };
extern const char *const EN_FX_NAME[EN_NFX];   /* short names, for the screen */
void en_fx(uint64_t mask);
void en_trk(uint32_t mute, uint32_t solo);     /* the tracks' mutes and solos, a bit a track (EN_TR_*), latched */
void en_gate_pat(uint32_t steps);              /* the pattern gate (FX_GATEX): bit s, step s of its loop open */
/* the effects'' settings (the player''s, not a piece''s; not stored): en_fxs(i, v). Gate: the loop''s length in
 * sixteenths (2..16; the pattern, en_gate_pat, repeats every that many: 12 or 10 against a bar of 16 is a
 * polymeter), the tracks it closes (a bit a track, EN_TR_*), how far it closes them (0..100); the rest 0..100:
 * the phaser''s rate, depth, feedback and mix; the flanger''s; the vinyl''s level, crackle (pops a second),
 * hiss and tone. */
enum {
    EN_FXS_GX_LEN, EN_FXS_GX_TGT, EN_FXS_GX_DEPTH,
    EN_FXS_PH_RATE, EN_FXS_PH_DEPTH, EN_FXS_PH_FB, EN_FXS_PH_MIX,
    EN_FXS_FL_RATE, EN_FXS_FL_DEPTH, EN_FXS_FL_FB, EN_FXS_FL_MIX,
    EN_FXS_VN_LEVEL, EN_FXS_VN_CRACK, EN_FXS_VN_HISS, EN_FXS_VN_TONE,
    EN_FXS_TP_WOW, EN_FXS_TP_FLUT, EN_FXS_TP_DRIVE, EN_FXS_TP_DULL,      /* the tape: slow wobble, flutter, drive, dullness */
    EN_FXS_RL_START, EN_FXS_RL_END, EN_FXS_RL_TIME, EN_FXS_RL_SWELL,     /* the roll: its first and last rate (0..6: 1/2 .. 1/32, 1/64),
                                                                          * beats at each (0..3: 1/2, 1, 2, 4), its swell */
    EN_FXS_PM_DEPTH, EN_FXS_PM_REL, EN_FXS_PM_RATE, EN_FXS_PM_TGT,       /* the pump: depth, release, its period (0..3: 1/2 note,
                                                                          * 1/4, 1/8, 1/16), the tracks (a bit a track) */
    EN_FXS_RS_TIME, EN_FXS_RS_FILT, EN_FXS_RS_WASH, EN_FXS_RS_ECHO,      /* the riser: time to the top, high-pass, reverb, echo */
    EN_FXS_SC_CHAOS, EN_FXS_SC_REV, EN_FXS_SC_SIZE, EN_FXS_SC_LEN,       /* the scatter: how many slices move, how many play backwards,
                                                                          * the slice (0..2: 1/32, 1/16, 1/8), how many slices (2..8) */
    EN_FXS_BR_TIME, EN_FXS_BR_BACK, EN_FXS_BR_RET, EN_FXS_BR_DULL,       /* the brake and the spin-back: time to stop, the spin-back's speed,
                                                                          * time to spin back up, how dull it goes as it slows */
    EN_FXS_HF_SPEED, EN_FXS_HF_SLIDE,                                    /* half time: its speed (0..2: x1.5, x2, x4), the slide into it */
    EN_FXS_SW_RATE, EN_FXS_SW_DEPTH, EN_FXS_SW_RESO, EN_FXS_SW_TYPE,     /* the sweep: its period (0..4: 2 bars, 1 bar, 1/2, 1/4, 1/8),
                                                                          * how far (to 7 octaves), resonance, low-pass / high-pass / band-pass (0..2) */
    EN_FXS_SR_EVERY, EN_FXS_SR_REACH, EN_FXS_SR_STYLE,                   /* the scratch: its period (0..3: 1/4, 1/8, 1/16, 1/32 note), how much of
                                                                          * the past it works (0..3: 1/16, 1/8, 1/4, 1/2 note), baby or pull (0..1) */
    EN_FXS_FZ_SIZE, EN_FXS_FZ_SPREAD, EN_FXS_FZ_GRAINS,                  /* the freeze: grain size, how far back it draws, how many grains (0..3: 1..4) */
    EN_FXS_RV_LEN, EN_FXS_RV_MIX,                                        /* the reverse swell: its length (0..2: 1/2, 1, 2 beats), how much it replaces */
    EN_FXS_RM_FREQ, EN_FXS_RM_MIX,                                       /* the ring mod: its tone (40 .. 800 Hz), its mix */
    EN_FXS_FX_QUANT, EN_FXS_FX_MODE, EN_FXS_FX_MIX,                      /* Flex Beat: when it starts (0 off, 1 beat, 2 bar), one shot (0) or loop (1), its mix (0..100) */
    EN_FXS_TK_RATE, EN_FXS_TK_DEPTH, EN_FXS_TK_RESO, EN_FXS_TK_MIX,      /* talk: its period (0..4: 2 bars .. 1/8), how far through the vowels, resonance, mix */
    EN_FXS_WH_SENS, EN_FXS_WH_RANGE, EN_FXS_WH_RESO, EN_FXS_WH_MIX,      /* wah: how it follows the loudness, how far it sweeps, resonance, mix */
    EN_FXS_SM_MODE, EN_FXS_SM_LOOP, EN_FXS_SM_LEN, EN_FXS_SM_MIX,        /* the sampler: pitch (0) or slice (1), one shot (0) or loop (1), the window (0 full, 1 half, 2 quarter), its mix */
    EN_NFXS
};
#define EN_FXS_DEFAULTS {16, 0xF0, 100, 40, 80, 55, 100, 35, 50, 50, 70, 50, 45, 25, 80, \
                         70, 45, 20, 50, 3, 5, 1, 0, 88, 35, 1, 0xF0, 56, 100, 100, 0, \
                         60, 25, 1, 4, 55, 50, 30, 50, 1, 30, 3, 70, 55, 0, \
                         1, 2, 0, 50, 60, 2, 1, 70, 45, 100, 2, 0, 100, 1, 70, 60, 100, 60, 60, 55, 100, 0, 1, 1, 80}
extern const int16_t EN_FXS_DEF[EN_NFXS];
void en_fxs(int i, int v);
uint8_t en_flex_step(int pat, int step);       /* Flex Beat pattern pat (0..14), step 0..15: e | rev << 4 | mute << 5 | div << 6 (e 15: live) */
extern volatile uint8_t en_flx_state;          /* 0 idle, 1 armed, 2 playing */
extern volatile uint32_t en_flx_done;          /* one shots played out so far */
extern volatile uint8_t en_smp_active;       /* the DJ snips sounding */
const char *en_dj_name(int key);              /* the name of the snip on a key (0..26) */
void en_spectrum(float *out, int nb);          /* the master's spectrum, nb bands (70 Hz .. 10 kHz), each 0..1: for the screen */
/* the FX knobs: 0 filter (-50 low-pass .. +50 high-pass, 0 off), 1 crush, 2 throw, 3 wash (0..100) */
enum { EN_FXK_FILTER, EN_FXK_CRUSH, EN_FXK_THROW, EN_FXK_WASH, EN_NFXK };
void en_fxk(int k, int v);
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
extern volatile float en_acid_level;
extern volatile uint8_t en_acid_len, en_acid_pos;    /* the acid loop's length and step */
extern volatile uint16_t en_acid_gate, en_acid_acc;  /* its steps that play; those accented */
extern volatile float en_acid_glow;
extern volatile uint8_t en_held;              /* keys held playing the voices (en_note) */
#define EN_NPROF 10                            /* the first core's stages (amb.c render_chunk PT): ticks summed since cleared, and the chunk count last */
extern volatile uint8_t en_bass_c1;            /* 1: the drone and the acid are rendered by the other core */
extern volatile uint8_t en_chorus_c0;          /* 1: the pad's chorus runs on the first core (it decides, from its waits); */
void en_balance_force(int v);                  /* tests: -1 it decides, 0 / 1 as told */
extern uint32_t (*en_prof_clock)(void);        /* the firmware's tick counter, for them (0: not measured) */
extern volatile uint32_t en_prof[EN_NPROF];
extern volatile uint8_t en_drum_pos;           /* the step playing (0..15) */
extern volatile uint8_t en_lane_pos[EN_NLANE]; /* each lane's step (its own length: P_LLEN + lane) */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint8_t en_pat_now, en_pat_next;
extern volatile int8_t en_dd_left;             /* drum drift: bars to the change (1: the fill bar), -1 none */
extern volatile uint32_t en_dd_changes, en_dd_mutations;   /* changes made and mutations since start */      /* the pattern playing; the one waiting for the bar */
extern volatile uint16_t en_pats[EN_NPAT][EN_NLANE];   /* the patterns: bit s, a hit on step s (the UI edits) */
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is out (a breakdown) */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_par_global(int p);                      /* the player's, not a piece's: Tune, MIDI, Theme */
int en_par_now(int p);                         /* the engine's own value of a parameter (what the UI has sent it: the mood's, the JAM page's hand switch) */
int en_par_sticky(int p);                      /* kept across pieces and genres: the player's, and the drum drift's settings */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* the kinds of chord (P_CHORD): as the scale has it (sevenths on a 7-note scale, else triads); triads;
 * sevenths; ninths (1 3 7 9); sus (1 4 5 b7); dub (a minor 9 shape on each degree, the scale or not);
 * fourths (stacked fourths: an m11 sound) */
enum { EN_CH_AUTO, EN_CH_TRIAD, EN_CH_7TH, EN_CH_9TH, EN_CH_SUS, EN_CH_DUB, EN_CH_4THS, EN_NCHORD };
extern const char *const EN_CHORD_NAME[EN_NCHORD];
int en_chord_kind(uint32_t pool, int root, int deg, int kind, int *pcs);   /* en_chord_tones, of a kind */
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* the pieces: recipes for everything but Tune, MIDI out and Theme (the player's), and six drum patterns
 * each. Named by number, 01 .. 10. */
#define EN_NPIECES 10
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
/* genres: banks of ten pieces, each with its own recipes and drum patterns (the first, 0, is the original techno and acid
 * set; the others are factory sets, kept in RAM while the power is on). */
#define EN_NGENRE 10
const char *en_genre_name(int g);
const char *en_piece_name_g(int g, int i);
void en_piece_g(int g, int i, int16_t *par, uint16_t *pool);
void en_piece_pats_g(int g, int i, uint16_t pats[EN_NPAT][EN_NLANE]);
void en_piece_pats(int i, uint16_t pats[EN_NPAT][EN_NLANE]);   /* its factory drum patterns */
