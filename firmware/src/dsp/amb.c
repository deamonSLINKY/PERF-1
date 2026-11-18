/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"

#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min", "Locrian", "Harm.min", "Mel.min", "Phr.dom", "Hung.min", "Dbl.harm"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10),
    /* (0.17.33, GitHub issue 2) Locrian; harmonic minor (Aeolian with the major seventh: a real dominant, V, in minor); melodic minor (the jazz
     * minor: Dorian with the major seventh); Phrygian dominant (harmonic minor's fifth mode: the flamenco, the Middle East, psytrance); Hungarian
     * minor (harmonic minor with a raised fourth); double harmonic (the "Arabic" or Byzantine: two augmented seconds) */
    B(0, 1, 3, 5, 6, 8, 10), B(0, 2, 3, 5, 7, 8, 11), B(0, 2, 3, 5, 7, 9, 11), B(0, 1, 4, 5, 7, 8, 10), B(0, 2, 3, 6, 7, 8, 11),
    B(0, 1, 4, 5, 7, 8, 11)};
#undef B
const char *const EN_LANE_NAME[EN_NLANE] = {"Kick", "Snare", "Clap", "Hat", "Open hat", "Perc"};

static const en_param_t PARAMS[P_NPARAMS] = {
    [P_PAD] = {"Pad", 0, 100, 45},          [P_ACID] = {"Acid", 0, 100, 55},       [P_BASS] = {"Bass", 0, 100, 65},
    [P_PULSE] = {"Drums", 0, 100, 75},      [P_NOISE] = {"Vinyl", 0, 100, 20},
    [P_TEMPO] = {"Tempo", 70, 190, 128},   [P_EVOLVE] = {"Evolve", 0, 100, 40},   [P_MOTION] = {"Motion", 0, 100, 60},
    [P_DRIFT] = {"Drift", 0, 100, 60},
    [P_BRIGHT] = {"Bright", 0, 100, 35},    [P_PADTONE] = {"Sound", 0, EN_NSOUND - 1, 1},     [P_DETUNE] = {"Detune", 0, 100, 30},
    [P_PE_ATK] = {"Attack", 0, 100, 45},    [P_PE_HOLD] = {"Hold", 0, 100, 35},    [P_PE_REL] = {"Release", 0, 100, 55},
    [P_PE_REST] = {"Rest", 0, 100, 20},
    [P_PL_RATE] = {"Rate", 0, 15, 4},        [P_PL_DEPTH] = {"Depth", 0, 100, 25},  [P_PL_SHAPE] = {"Shape", 0, 5, 0},
    [P_PL_DEST] = {"Target", 0, 7, 0},
    [P_NZ_COLOR] = {"Wear", 0, 7, 1},     [P_AC_EVERY] = {"Every", 0, 7, 0},    [P_NZ_PUMP] = {"Pump", 0, 100, 60},
    [P_BASSOCT] = {"Octave", -3, 3, 0},     [P_DR_CUT] = {"Cutoff", 0, 100, 45},   [P_DR_RES] = {"Reso", 0, 100, 45},
    [P_DE_TRIG] = {"Trigger", 0, 5, 0},     [P_DE_ATK] = {"Attack", 0, 100, 25},   [P_DE_DEC] = {"Decay", 0, 100, 55},
    [P_DE_AMT] = {"Amount", 0, 100, 35},
    [P_DL_RATE] = {"Rate", 0, 15, 6},        [P_DL_DEPTH] = {"Depth", 0, 100, 55},  [P_DL_SHAPE] = {"Shape", 0, 5, 0},
    [P_DL_DEST] = {"Target", 0, 7, 0},
    [P_AC_CUT] = {"Cutoff", 0, 100, 35},    [P_AC_RES] = {"Reso", 0, 100, 70},     [P_AC_ENV] = {"Env mod", 0, 100, 55},
    [P_AC_DEC] = {"Decay", 0, 100, 40},     [P_AC_ACC] = {"Accent", 0, 100, 50},   [P_AC_SLIDE] = {"Slide", 0, 100, 30},
    [P_AC_WAVE] = {"Sound", 0, EN_NACID - 1, 0},
    [P_AC_DENS] = {"Density", 0, 100, 60},  [P_AC_LEN] = {"Length", 1, 16, 16},    [P_AC_RANGE] = {"Range", 1, 3, 1},
    [P_AC_MUT] = {"Mutate", 0, 100, 25},    [P_AC_OCT] = {"Acid oct", -3, 3, 0},
    [P_DRIVE] = {"Drive", 0, 100, 40},      [P_SWING] = {"Swing", 0, 100, 0},      [P_PATTERN] = {"Pattern", 0, EN_NPAT - 1, 0},
    [P_LANE_LVL(0)] = {"Level", 0, 150, 85}, [P_LANE_TUNE(0)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(0)] = {"Decay", 0, 100, 50},
    [P_LANE_LVL(1)] = {"Level", 0, 150, 60}, [P_LANE_TUNE(1)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(1)] = {"Decay", 0, 100, 45},
    [P_LANE_LVL(2)] = {"Level", 0, 150, 65}, [P_LANE_TUNE(2)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(2)] = {"Decay", 0, 100, 45},
    [P_LANE_LVL(3)] = {"Level", 0, 150, 55}, [P_LANE_TUNE(3)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(3)] = {"Decay", 0, 100, 35},
    [P_LANE_LVL(4)] = {"Level", 0, 150, 50}, [P_LANE_TUNE(4)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(4)] = {"Decay", 0, 100, 50},
    [P_LANE_LVL(5)] = {"Level", 0, 150, 50}, [P_LANE_TUNE(5)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(5)] = {"Decay", 0, 100, 40},
    [P_ECHO] = {"Echo", 0, 100, 45},        [P_ETIME] = {"Echo time", 0, 5, 1},    [P_REVERB] = {"Reverb", 0, 100, 40},
    [P_SIZE] = {"Size", 0, 100, 70},
    [P_ROOT] = {"Root", 0, 11, 9},          [P_MODE] = {"Mode", 0, EN_NMODES - 1, 5}, [P_TUNE] = {"Tune", -50, 50, 0},
    [P_MIDI] = {"MIDI out", 0, 1, 1},       [P_THEME] = {"Theme", 0, EN_NTHEME - 1, 0},
    [P_STYLE] = {"Sync", 0, 1, 0},            /* (was the style, kept to read old banks; now the acid answer's sync, P_AC_CHANCE: 0 the bar, 1 the loop) */
    [P_LINKS] = {"Links", 0, 1, 0},         [P_LK_KICK] = {"Kick", 0, 100, 50},    [P_LK_SNARE] = {"Snare", 0, 100, 40},
    [P_LK_HAT] = {"Hats", 0, 100, 50},      [P_LK_PERC] = {"Perc", 0, 1, 0},
    [P_OCT] = {"Song oct", -3, 3, 0},       [P_PAD_OCT] = {"Pad oct", -3, 3, 0},
    [P_DR_INST] = {"Sound", 0, EN_NSOUND - 1, 0},
    [P_KIT_KICK] = {"Kick", 0, EN_NKITS - 1, 0}, [P_KIT_SNARE] = {"Snare", 0, EN_NKITS - 1, 0},
    [P_KIT_HAT] = {"Hats", 0, EN_NKITS - 1, 0},  [P_KIT_PERC] = {"Perc", 0, EN_NKITS - 1, 0},
    [P_PAD_MAN] = {"Pad hand", 0, 1, 0},     [P_DR_MAN] = {"Bass hand", 0, 1, 0}, [P_AC_MAN] = {"Acid hand", 0, 1, 0},
    [P_CHORD] = {"Chord", 0, EN_NCHORD - 1, 0}, [P_VOICING] = {"Voicing", 0, 1, 0},
    [P_DD_CHANGE] = {"Change", 0, 2, 0}, [P_DD_EVERY] = {"Every", 0, 5, 2}, [P_DD_POOL] = {"Pool", 2, EN_NPAT, 4},
    [P_DD_MUTATE] = {"Mutate", 0, 2, 0},
    [P_CM_LVL] = {"Pluck", 0, 100, 50},   /* (0.15.17: 50, was 30: it got lost in every piece) */     [P_CM_SND] = {"Sound", 0, EN_NCMSND - 1, 1}, [P_CM_DENS] = {"Density", 0, 100, 45},
    [P_CM_OCT] = {"Pluck oct", -2, 2, 0},   [P_CM_DEC] = {"Decay", 0, 100, 45},          [P_CM_STYLE] = {"Style", 0, EN_NCMSTYLE - 1, 0},
    [P_CM_RATE] = {"Rate", 0, 3, 2},        [P_CM_RANGE] = {"Range", 1, 3, 2},
    [P_PROB] = {"Prob", 0, 100, 100},     [P_PROB1] = {"Prob", 0, 100, 100},   [P_PROB2] = {"Prob", 0, 100, 100},
    [P_PROB3] = {"Prob", 0, 100, 100},    [P_PROB4] = {"Prob", 0, 100, 100},   [P_PROB5] = {"Prob", 0, 100, 100},
    [P_PAN] = {"Pan", 0, 100, 50},          [P_PAN1] = {"Pan", 0, 100, 50},        [P_PAN2] = {"Pan", 0, 100, 50},       [P_PAN3] = {"Pan", 0, 100, 50},
    [P_PAN4] = {"Pan", 0, 100, 50},         [P_SEND] = {"Send", 0, 100, 50},       [P_SEND1] = {"Send", 0, 100, 50},     [P_SEND2] = {"Send", 0, 100, 50},
    [P_SEND3] = {"Send", 0, 100, 50},       [P_SEND4] = {"Send", 0, 100, 50},
    [P_CM_MAN] = {"Pluck hand", 0, 1, 0},
    [P_RV_TYPE] = {"Type", 0, 2, 0},       [P_RV_DAMP] = {"Damping", 0, 100, 47}, [P_RV_PRE] = {"Predelay", 0, 100, 100}, [P_RV_X] = {"Amount", 0, 100, 30},
    [P_PL_CHO] = {"Chorus", 0, 100, 0},
    [P_MC_RATIO] = {"Ratio", 0, 6, 1},     [P_MC_THR] = {"Thresh", 0, 100, 60}, [P_MC_MAKE] = {"Make-up", 0, 100, 10}, [P_ML_CEIL] = {"Ceiling", 0, 100, 100},
    [P_LLEN] = {"Length", 1, 16, 16},      [P_LLEN1] = {"Length", 1, 16, 16},   [P_LLEN2] = {"Length", 1, 16, 16},
    [P_LLEN3] = {"Length", 1, 16, 16},     [P_LLEN4] = {"Length", 1, 16, 16},   [P_LLEN5] = {"Length", 1, 16, 16},
    [P_DENS] = {"Density", 0, 100, 50},    [P_DENS1] = {"Density", 0, 100, 50}, [P_DENS2] = {"Density", 0, 100, 50},
    [P_DENS3] = {"Density", 0, 100, 50},   [P_DENS4] = {"Density", 0, 100, 50}, [P_DENS5] = {"Density", 0, 100, 50},
    [P_LSRC] = {"Source", 0, 6, 0},        [P_LSRC1] = {"Source", 0, 6, 0},     [P_LSRC2] = {"Source", 0, 6, 0},
    [P_LSRC3] = {"Source", 0, 6, 0},       [P_LSRC4] = {"Source", 0, 6, 0},     [P_LSRC5] = {"Source", 0, 6, 0},
    [P_CH_LOOP] = {"Loop", 0, 6, 0},
    [P_AC_RESP] = {"Answer", 0, 44, 0},
    [P_CM_VU] = {"Deja vu", 0, 100, 0},   [P_CM_LOOP] = {"Loop", 0, 3, 2},       [P_CM_LEAP] = {"Leaps", 0, 100, 0},   [P_AC_CHG] = {"Mutates", 0, 3, 0},
    [P_CM_MOTIF] = {"Motif", 0, 100, 60},   [P_CM_LOCK] = {"Frozen", 0, 1, 0},
    [P_PAD_COMP] = {"Comp", 0, 8, 0},     [P_GHOST] = {"Ghosts", 0, 100, 0},   [P_ACCENT] = {"Accents", 0, 100, 50}, [P_HUMAN] = {"Humanize", 0, 100, 0},
    [P_CHAIN] = {"Chain", 0, 9, 0},         [P_SLICE] = {"Slice", 0, 9, 0},       [P_TIME] = {"Time", 0, 2, 0},
    [P_COND] = {"Cond", 0, 7, 0},           [P_COND1] = {"Cond", 0, 7, 0},        [P_COND2] = {"Cond", 0, 7, 0},          [P_COND3] = {"Cond", 0, 7, 0},
    [P_COND4] = {"Cond", 0, 7, 0},          [P_COND5] = {"Cond", 0, 7, 0},
    [P_AC_LOCK] = {"Frozen", 0, 1, 0},
    [P_BL_MODE] = {"Mode", 0, EN_BL_STEPS + 4, 0},       [P_BL_LEN] = {"Length", 0, 100, 50},  [P_BL_MOVE] = {"Move", 0, 100, 30},  [P_BL_GLIDE] = {"Glide", 0, 100, 0},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }
int en_par_global(int p) { return p == P_TUNE || p == P_MIDI || p == P_THEME; }
int en_par_sticky(int p) { return en_par_global(p); }   /* (0.16.28: the drum drift is the piece's, no longer the player's) */

/* the pieces' factory drum patterns: six for each piece (tools/gen_pats.py writes them) */
#include "pats.h"
volatile uint16_t en_pats[EN_NPAT][EN_NLANE];
volatile uint8_t en_mod[EN_NLANE][16];          /* a piece's step tricks, a byte a lane step: ratchet (bits 0-2), push (3-5), level (6-7); 0: none (the UI edits) */

#define EN_NBLFIG 13                            /* the bass line's figures by name (modes 7 .. 15, then (0.17.69) 17 .. 20 after Steps (16), so a saved
                                                 * piece's mode keeps its meaning; the table BL_FIG is with the bass line) */
#define BL_FIG_OF(m) ((m) < EN_BL_STEPS ? (m) - 7 : (m) - 8)   /* a figure's mode to its place in BL_FIG */
static const char *const BL_FIG_NAME[EN_NBLFIG] = {"Octaves", "Two-step", "One-drop", "Log drum", "Skippy", "Funk", "Big beat", "Dub", "Gallop",
                                                   "Rolling", "Tresillo", "Push", "Bounce"};

/* the pieces: each the defaults above with a few changed (a parameter, its value; -1 ends), its own
 * kit's sound (the lanes) among them. Numbered on the screen; the names are for reading here. */
#define LV(l) P_LANE_LVL(l)
#define TU(l) P_LANE_TUNE(l)
#define DE(l) P_LANE_DEC(l)
#define KIT(k) P_KIT_KICK, k, P_KIT_SNARE, k, P_KIT_HAT, k, P_KIT_PERC, k   /* all four drums from kit k */
static const int16_t PC_HYPNOS[] = {-1};
static const int16_t PC_UNDERTOW[] = {P_TEMPO, 124, P_ROOT, 2, P_MODE, 1, P_PAD, 30, P_ACID, 45, P_BASS, 80, P_NOISE, 25,
    P_NZ_COLOR, 2, P_BASSOCT, -1, P_DR_RES, 60, P_DL_DEPTH, 70, P_AC_CUT, 25, P_AC_DENS, 45, P_AC_LEN, 13,
    P_PATTERN, 1, P_ECHO, 55, P_BRIGHT, 20, TU(0), -10, DE(0), 65, LV(5), 60, -1};
static const int16_t PC_MONOLITH[] = {P_KIT_KICK, 2, P_TEMPO, 132, P_ROOT, 4, P_MODE, 2, P_PAD, 20, P_ACID, 60, P_BASS, 75,
    P_AC_RES, 80, P_AC_ENV, 70, P_AC_DENS, 70, P_DRIVE, 60, P_NOISE, 15, P_REVERB, 25, P_SIZE, 60, P_PATTERN, 1,
    LV(0), 90, LV(2), 70, DE(3), 25, -1};
static const int16_t PC_ASCENSION[] = {P_PADTONE, 9, P_AC_WAVE, 8, P_TEMPO, 138, P_ROOT, 7, P_PAD, 60, P_BRIGHT, 60, P_ACID, 45, P_BASS, 60,
    P_AC_LEN, 12, P_PATTERN, 1, P_REVERB, 55, P_SIZE, 85, P_MOTION, 80, P_NZ_COLOR, 0, P_NOISE, 15, P_PL_DEPTH, 40,
    LV(4), 60, LV(2), 70, -1};
static const int16_t PC_NIGHT_DRIVE[] = {P_KIT_PERC, 3, P_TEMPO, 130, P_ROOT, 9, P_MODE, 1, P_PAD, 35, P_ACID, 65, P_AC_CUT, 45,
    P_AC_RES, 75, P_AC_WAVE, 1, P_AC_SLIDE, 50, P_AC_DENS, 55, P_AC_LEN, 14, P_PATTERN, 1, P_NZ_COLOR, 4, P_NOISE, 12,
    P_SWING, 15, TU(5), 20, LV(5), 55, -1};
static const int16_t PC_DUB_CHAMBER[] = {P_DR_INST, 12, P_TEMPO, 120, P_ROOT, 5, P_MODE, 7, P_PAD, 70, P_CHORD, EN_CH_DUB, P_PADTONE, 8, P_ACID, 40, P_AC_CUT, 20,
    P_AC_DEC, 60, P_ECHO, 80, P_SIZE, 90, P_AC_LEN, 7, P_AC_DENS, 35, P_PATTERN, 1, P_NZ_COLOR, 6, P_NOISE, 35,
    P_DE_TRIG, 1, P_SWING, 20, TU(1), 25, DE(1), 25, DE(0), 70, -1};
static const int16_t PC_GLASS_TUNNEL[] = {P_TEMPO, 134, P_ROOT, 11, P_MODE, 6, P_PAD, 30, P_BRIGHT, 55, P_ACID, 55,
    P_AC_LEN, 10, P_AC_DENS, 65, P_AC_MUT, 40, P_PATTERN, 1, P_NZ_COLOR, 7, P_NOISE, 20, P_PL_RATE, 3, P_PL_DEPTH, 50,
    P_PL_DEST, 2, TU(5), 30, DE(5), 25, DE(3), 25, -1};
static const int16_t PC_TIDAL[] = {P_KIT_PERC, 10, P_TEMPO, 126, P_ROOT, 10, P_MODE, 4, P_PAD, 50, P_ACID, 50, P_BASS, 75,
    P_DL_DEPTH, 85, P_DL_RATE, 5, P_MOTION, 95, P_EVOLVE, 75, P_AC_LEN, 15, P_PATTERN, 1, P_NZ_COLOR, 3, P_NOISE, 15,
    P_DE_TRIG, 3, P_DE_AMT, 60, TU(5), -25, DE(5), 60, LV(5), 60, -1};
static const int16_t PC_WAREHOUSE[] = {P_TEMPO, 136, P_ROOT, 0, P_MODE, 2, P_PAD, 15, P_ACID, 55, P_BASS, 80,
    P_DRIVE, 75, P_AC_RES, 85, P_AC_ENV, 75, P_AC_DENS, 75, P_EVOLVE, 55, P_MOTION, 50, P_DL_DEPTH, 65,
    P_NOISE, 20, P_NZ_COLOR, 5, P_REVERB, 30, P_SIZE, 55, P_ECHO, 30, P_PATTERN, 1, KIT(2), P_PADTONE, 7, P_DR_INST, 2,
    LV(0), 90, DE(0), 60, LV(4), 60, DE(4), 60, -1};
static const int16_t PC_DEEP_FIELD[] = {P_TEMPO, 118, P_ROOT, 3, P_PAD, 60, P_BRIGHT, 30, P_PE_ATK, 60, P_PE_REL, 70,
    P_ACID, 35, P_AC_CUT, 25, P_AC_DENS, 40, P_AC_LEN, 11, P_AC_MUT, 40, P_BASS, 60, P_NOISE, 30, P_NZ_COLOR, 1,
    P_REVERB, 60, P_SIZE, 85, P_ECHO, 60, P_ETIME, 2, P_PATTERN, 1, P_SWING, 25, P_MOTION, 85, P_DRIFT, 75,
    TU(1), -10, DE(1), 60, DE(0), 60, -1};
/* LOFI: ten pieces, 68 .. 94 beats a minute, swung, the Rhodes in sevenths and ninths over a warm bass, a soft sparse lead, the
 * dusty kit; each piece the base with a few changed (its key, mode, sounds and tempo). The patterns are made below. */
#define LOFI_BASE P_CM_VU, 60, P_GHOST, 60, P_HUMAN, 35,  P_BL_MODE, 1, P_BL_LEN, 65, P_BL_MOVE, 20, P_PADTONE, 0, P_PAD, 54, P_BRIGHT, 30, P_DETUNE, 35, P_CHORD, EN_CH_7TH, P_VOICING, 1, P_PE_ATK, 25, P_PE_REL, 50, \
    P_ACID, 28, P_AC_WAVE, 15, P_AC_CUT, 30, P_AC_RES, 18, P_AC_ENV, 25, P_AC_DEC, 55, P_AC_SLIDE, 35, P_AC_DENS, 28, \
    P_AC_LEN, 12, P_AC_MUT, 15, P_BASS, 58, P_DR_INST, 1, P_DR_CUT, 30, P_DR_RES, 10, P_PULSE, 68, KIT(8), P_DRIVE, 20, \
    P_SWING, 38, P_REVERB, 34, P_SIZE, 55, P_ECHO, 32, P_ETIME, 3, P_EVOLVE, 30, P_MOTION, 30, P_PATTERN, 0, LV(0), 92, LV(1), 55, \
    LV(3), 42, LV(4), 38, LV(5), 40, P_PL_CHO, 50, P_RV_DAMP, 70, P_MC_RATIO, 2, P_MC_THR, 55, P_MC_MAKE, 45
static const int16_t PL_01[] = {LOFI_BASE, P_TEMPO, 72, P_ROOT, 2, P_MODE, 1, P_CHORD, EN_CH_9TH, -1};                       /* D dorian, ninths */
static const int16_t PL_02[] = {LOFI_BASE, P_TEMPO, 78, P_ROOT, 9, P_MODE, 5, P_DR_INST, 9, P_AC_WAVE, 4, -1};                /* A aeolian, sub bass, plucks */
static const int16_t PL_03[] = {LOFI_BASE, P_TEMPO, 84, P_ROOT, 7, P_MODE, 4, P_CHORD, EN_CH_9TH, P_PADTONE, 5, P_AC_WAVE, 11, -1};   /* G mixolydian, warm pad, bells */
static const int16_t PL_04[] = {LOFI_BASE, P_TEMPO, 90, P_ROOT, 0, P_MODE, 0, P_SWING, 42, P_AC_DENS, 35, -1};                /* C major, sevenths */
static const int16_t PL_05[] = {LOFI_BASE, P_TEMPO, 70, P_ROOT, 5, P_MODE, 1, P_CHORD, EN_CH_9TH, P_PADTONE, 2, P_AC_WAVE, 4, P_REVERB, 42, -1};   /* F dorian, strings, slow */
static const int16_t PL_06[] = {LOFI_BASE, P_TEMPO, 80, P_ROOT, 10, P_MODE, 5, P_CHORD, EN_CH_9TH, P_CH_LOOP, 1, P_PADTONE, 4, P_AC_WAVE, 11, P_PAD, 52, -1};  /* Bb aeolian, organ, a two-chord 9ths loop */
static const int16_t PL_07[] = {LOFI_BASE, P_TEMPO, 86, P_ROOT, 3, P_MODE, 5, P_AC_DENS, 22, P_SWING, 34, -1};                /* Eb Aeolian, sparse (minor pentatonic until 0.14.15: it lost the turnaround) */
static const int16_t PL_08[] = {LOFI_BASE, P_TEMPO, 76, P_ROOT, 4, P_MODE, 1, P_CHORD, EN_CH_11TH, P_PADTONE, 3, P_BRIGHT, 24, P_ECHO, 40, -1};   /* E dorian, glass */
static const int16_t PL_09[] = {LOFI_BASE, P_TEMPO, 74, P_ROOT, 1, P_MODE, 3, P_DR_INST, 9, P_REVERB, 46, P_SIZE, 70, -1};   /* C# lydian, dreamy */
static const int16_t PL_10[] = {LOFI_BASE, P_TEMPO, 94, P_ROOT, 8, P_MODE, 4, P_CHORD, EN_CH_9TH, P_SWING, 45, P_AC_DENS, 38, P_LANE_LVL(0), 90, -1};   /* G# mixolydian, busier */
/* HOUSE: ten pieces, 118 .. 126 beats a minute, four on the floor, a clap on 2 and 4, open hats on the off-beats, stabs and
 * organ chords, a sub or square bass, a plucked line rolling in sixteenths; the pump (latched with the genre) ducks the chords. */
#define HOUSE_BASE P_CH_LOOP, 2, P_CM_VU, 60, P_GHOST, 25, P_HUMAN, 10,  P_CM_LOOP, 1, P_BL_MODE, 4, P_BL_LEN, 40, P_BL_MOVE, 25, P_PADTONE, 6, P_PAD, 52, P_BRIGHT, 40, P_CHORD, EN_CH_7TH, P_VOICING, 0, P_ACID, 38, P_AC_WAVE, 4, P_AC_CUT, 45, \
    P_AC_RES, 40, P_AC_ENV, 40, P_AC_DENS, 45, P_AC_LEN, 16, P_AC_MUT, 25, P_BASS, 70, P_DR_INST, 9, P_DR_CUT, 40, P_PULSE, 80, \
    KIT(1), P_DRIVE, 35, P_SWING, 12, P_ECHO, 30, P_ETIME, 3, P_REVERB, 30, P_SIZE, 50, P_EVOLVE, 45, P_MOTION, 50, P_PATTERN, 0, \
    LV(0), 90, LV(2), 72, LV(3), 50, LV(4), 55, LV(5), 45, P_PL_CHO, 35, P_RV_DAMP, 30, P_RV_PRE, 60, P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 35
static const int16_t PH_01[] = {HOUSE_BASE, P_TEMPO, 122, P_ROOT, 9, P_MODE, 1, -1};
static const int16_t PH_02[] = {HOUSE_BASE, P_TEMPO, 124, P_ROOT, 0, P_MODE, 5, P_PADTONE, 4, P_DR_INST, 10, P_AC_WAVE, 9, -1};
static const int16_t PH_03[] = {HOUSE_BASE, P_TEMPO, 120, P_ROOT, 5, P_MODE, 1, P_PADTONE, 0, P_CHORD, EN_CH_9TH, -1};
static const int16_t PH_04[] = {HOUSE_BASE, P_TEMPO, 126, P_ROOT, 2, P_MODE, 5, P_PADTONE, 10, P_DR_INST, 5, -1};
static const int16_t PH_05[] = {HOUSE_BASE, P_TEMPO, 118, P_ROOT, 7, P_MODE, 4, P_PADTONE, 8, P_CHORD, EN_CH_9TH, P_DR_INST, 1, P_AC_WAVE, 15, -1};
static const int16_t PH_06[] = {HOUSE_BASE, P_TEMPO, 124, P_ROOT, 11, P_MODE, 5, P_AC_WAVE, 9, P_CHORD, EN_CH_9TH, -1};
static const int16_t PH_07[] = {HOUSE_BASE, P_TEMPO, 122, P_ROOT, 3, P_MODE, 7, P_PADTONE, 4, P_DR_INST, 10, -1};
static const int16_t PH_08[] = {HOUSE_BASE, P_TEMPO, 125, P_ROOT, 10, P_MODE, 1, P_PADTONE, 1, P_CHORD, EN_CH_9TH, -1};
static const int16_t PH_09[] = {HOUSE_BASE, P_TEMPO, 121, P_ROOT, 4, P_MODE, 5, P_PADTONE, 14, P_DR_INST, 5, P_AC_WAVE, 8, P_CHORD, EN_CH_SUS, -1};
static const int16_t PH_10[] = {HOUSE_BASE, P_TEMPO, 126, P_ROOT, 8, P_MODE, 1, P_SWING, 20, P_CHORD, EN_CH_11TH, -1};
static const int16_t *const HOUSE_PIECES[EN_NPIECES] = {PH_01, PH_02, PH_03, PH_04, PH_05, PH_06, PH_07, PH_08, PH_09, PH_10};
static const char *const HOUSE_NAME[EN_NPIECES] = {"H01", "H02", "H03", "H04", "H05", "H06", "H07", "H08", "H09", "H10"};
/* DUBSTEP: ten pieces at 136 .. 150, the beat in half time (the snare on the third beat), a deep sub kick, the drone as the wobble
 * bass (its LFO, a quarter or half note, on its cutoff), a harsh lead, dark pads in the minor modes. */
#define DUB_BASE P_CH_LOOP, 1, P_CM_VU, 40, P_BL_MODE, 2, P_BL_LEN, 85, P_BL_MOVE, 10, P_BL_GLIDE, 20, P_PADTONE, 7, P_PAD, 32, P_BRIGHT, 25, P_CHORD, EN_CH_4THS, P_VOICING, 1, P_ACID, 42, P_AC_WAVE, 14, P_AC_CUT, 55, \
    P_AC_RES, 60, P_AC_ENV, 60, P_AC_DENS, 40, P_AC_LEN, 8, P_AC_MUT, 30, P_BASS, 85, P_DR_INST, 4, P_DR_CUT, 40, P_DR_RES, 55, \
    P_DL_RATE, 0, P_DL_SHAPE, 1, P_DL_DEPTH, 85, P_DL_DEST, 0, P_PULSE, 85, KIT(9), P_DRIVE, 60, P_SWING, 0, P_ECHO, 45, P_ETIME, 3, \
    P_REVERB, 45, P_SIZE, 70, P_EVOLVE, 35, P_MOTION, 40, P_PATTERN, 0, LV(0), 95, LV(1), 70, LV(3), 40, LV(4), 45, P_PL_CHO, 30, P_RV_DAMP, 65, P_RV_PRE, 80, P_MC_RATIO, 4, P_MC_THR, 40, P_MC_MAKE, 40
static const int16_t PD_01[] = {DUB_BASE, P_TEMPO, 140, P_ROOT, 4, P_MODE, 2, -1};
static const int16_t PD_02[] = {DUB_BASE, P_TEMPO, 140, P_ROOT, 5, P_MODE, 5, P_DR_INST, 9, P_DL_RATE, 1, P_DL_SHAPE, 0, -1};
static const int16_t PD_03[] = {DUB_BASE, P_TEMPO, 142, P_ROOT, 2, P_MODE, 2, P_DR_INST, 11, P_DL_SHAPE, 3, -1};
static const int16_t PD_04[] = {DUB_BASE, P_TEMPO, 138, P_ROOT, 7, P_MODE, 5, P_DL_SHAPE, 0, -1};
static const int16_t PD_05[] = {DUB_BASE, P_TEMPO, 140, P_ROOT, 9, P_MODE, 7, P_DR_INST, 6, P_DL_RATE, 1, -1};
static const int16_t PD_06[] = {DUB_BASE, P_TEMPO, 145, P_ROOT, 1, P_MODE, 2, P_DR_INST, 13, P_DL_SHAPE, 4, -1};
static const int16_t PD_07[] = {DUB_BASE, P_TEMPO, 140, P_ROOT, 0, P_MODE, 5, P_DR_INST, 3, -1};
static const int16_t PD_08[] = {DUB_BASE, P_TEMPO, 140, P_ROOT, 10, P_MODE, 2, P_DR_INST, 8, P_DL_RATE, 1, P_DL_SHAPE, 0, P_CHORD, EN_CH_DUB, -1};
static const int16_t PD_09[] = {DUB_BASE, P_TEMPO, 136, P_ROOT, 6, P_MODE, 5, P_PADTONE, 11, P_CHORD, EN_CH_SUS, -1};
static const int16_t PD_10[] = {DUB_BASE, P_TEMPO, 150, P_ROOT, 11, P_MODE, 2, P_DR_INST, 11, P_DL_SHAPE, 3, -1};
static const int16_t *const DUB_PIECES[EN_NPIECES] = {PD_01, PD_02, PD_03, PD_04, PD_05, PD_06, PD_07, PD_08, PD_09, PD_10};
static const char *const DUB_NAME[EN_NPIECES] = {"D01", "D02", "D03", "D04", "D05", "D06", "D07", "D08", "D09", "D10"};
/* the other six genres (4 .. 9: DUB, DNB, BREAKS, GARAGE, ELECTRO, TRAP): a base recipe each, ten rows of tempo, root, mode, chord, pad, drone and acid, six
 * drum patterns from their classic masks (kick, snare, clap as the ghosts, hat, open, perc), the piece choosing which comes first */
static const int16_t X_DUBR[] = {P_CM_VU, 45, P_GHOST, 0, P_HUMAN, 25, P_PULSE, 78, P_DRIVE, 15, P_SWING, 12, P_BASSOCT, -1, P_PAD, 40, P_ACID, 22, P_BASS, 92, P_AC_DENS, 15, P_AC_LEN, 12, P_ECHO, 72, P_ETIME, 3, P_REVERB, 45, P_SIZE, 65,
    KIT(8), P_PATTERN, 0, P_EVOLVE, 25, P_PL_CHO, 15, P_RV_DAMP, 55, P_BL_MODE, 1, P_BL_LEN, 85, P_BL_MOVE, 8, P_BL_GLIDE, 30, P_MC_RATIO, 2, P_MC_THR, 50, P_MC_MAKE, 38, -1};
static const int16_t X_DNB[] = {P_CH_LOOP, 1, P_BL_MODE, 1, P_CM_VU, 40, P_GHOST, 40, P_HUMAN, 10, P_PULSE, 78, P_DRIVE, 40, P_SWING, 5, P_BASSOCT, -1, P_DE_TRIG, 2, P_PAD, 36, P_ACID, 42, P_BASS, 82, P_AC_DENS, 40, P_AC_LEN, 12, P_ECHO, 35, P_REVERB, 40,
    KIT(12), DE(0), 20, P_PATTERN, 0, P_EVOLVE, 40, P_PL_CHO, 45, P_RV_TYPE, 1, P_RV_X, 25, P_RV_DAMP, 40, P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 30, -1};
static const int16_t X_BREAKS[] = {P_BL_MODE, 6, P_CM_VU, 50, P_GHOST, 70, P_HUMAN, 25, P_PULSE, 80, P_DRIVE, 70, P_SWING, 14, P_PAD, 40, P_ACID, 55, P_BASS, 70, P_AC_RES, 70, P_AC_ENV, 65, P_AC_DENS, 55, P_ECHO, 40, P_REVERB, 30,
    KIT(4), P_PATTERN, 0, P_EVOLVE, 45, P_PL_CHO, 20, P_RV_TYPE, 2, P_RV_X, 35, P_MC_RATIO, 4, P_MC_THR, 42, P_MC_MAKE, 35, -1};
static const int16_t X_GARAGE[] = {P_CH_LOOP, 2, P_BL_MODE, 6, P_BL_MOVE, 30, P_CM_VU, 60, P_GHOST, 45, P_HUMAN, 20, P_PULSE, 70, P_DRIVE, 25, P_SWING, 64, P_PAD, 42, P_ACID, 42, P_BASS, 70, P_AC_DENS, 45, P_AC_LEN, 12, P_ECHO, 40, P_REVERB, 40,
    P_KIT_KICK, 1, P_KIT_SNARE, 5, P_KIT_HAT, 1, P_KIT_PERC, 5, P_PATTERN, 0, P_EVOLVE, 35, P_PL_CHO, 40, P_RV_DAMP, 45, P_MC_RATIO, 2, P_MC_THR, 50, P_MC_MAKE, 40, -1};
static const int16_t X_ELECTRO[] = {P_CH_LOOP, 1, P_CM_VU, 65, P_CM_LOOP, 1, P_BL_MODE, 3, P_BL_LEN, 30, P_BL_MOVE, 35, P_PULSE, 78, P_DRIVE, 45, P_SWING, 3, P_PAD, 40, P_ACID, 55, P_BASS, 70, P_AC_DENS, 60, P_AC_LEN, 12, P_ECHO, 45, P_REVERB, 30,
    KIT(11), P_PATTERN, 0, P_EVOLVE, 40, P_PL_CHO, 40, P_RV_TYPE, 2, P_RV_X, 30, P_MC_RATIO, 3, P_MC_THR, 48, P_MC_MAKE, 30, -1};
static const int16_t X_TRAP[] = {P_CH_LOOP, 2, P_CM_VU, 40, P_BL_MODE, 2, P_BL_LEN, 90, P_BL_MOVE, 8, P_BL_GLIDE, 25, P_PULSE, 85, P_DRIVE, 35, P_SWING, 0, P_BASSOCT, -1, P_DE_TRIG, 2, P_PAD, 40, P_ACID, 35, P_BASS, 88, P_AC_DENS, 20, P_AC_LEN, 8, P_ECHO, 40, P_REVERB, 35,
    KIT(13), LV(0), 95, P_PATTERN, 0, P_EVOLVE, 35, P_PL_CHO, 30, P_RV_TYPE, 1, P_RV_X, 30, P_RV_DAMP, 40, P_MC_RATIO, 4, P_MC_THR, 40, P_MC_MAKE, 40, -1};
static const int16_t X_DISCO[] = {P_CH_LOOP, 2, P_PULSE, 80, P_DRIVE, 20, P_SWING, 8, P_PAD, 50, P_ACID, 45, P_BASS, 75, P_AC_DENS, 55, P_AC_LEN, 16, P_ECHO, 25, P_REVERB, 30,
    KIT(1), P_PATTERN, 0, P_EVOLVE, 35, P_PL_CHO, 40, P_BL_MODE, 6, P_BL_LEN, 35, P_BL_MOVE, 10, P_CM_VU, 60, P_MC_RATIO, 3, P_MC_THR, 48, P_MC_MAKE, 32, -1};
static const int16_t X_FUNK[] = {P_PULSE, 78, P_DRIVE, 25, P_SWING, 20, P_PAD, 40, P_ACID, 50, P_BASS, 78, P_AC_DENS, 60, P_AC_LEN, 16, P_AC_MUT, 20, P_ECHO, 20, P_REVERB, 20,
    KIT(4), P_PATTERN, 0, P_EVOLVE, 35, P_BL_MODE, 6, P_BL_LEN, 28, P_BL_MOVE, 25, P_CM_VU, 45, P_GHOST, 65, P_HUMAN, 25, P_MC_RATIO, 3, P_MC_THR, 48, P_MC_MAKE, 30, -1};
static const int16_t X_REGGAE[] = {P_PULSE, 72, P_DRIVE, 15, P_SWING, 30, P_PAD, 45, P_ACID, 30, P_BASS, 85, P_AC_DENS, 35, P_AC_LEN, 16, P_ECHO, 55, P_REVERB, 35,
    KIT(8), P_PATTERN, 0, P_EVOLVE, 30, P_BL_MODE, 6, P_BL_LEN, 55, P_BL_MOVE, 15, P_CM_VU, 50, P_HUMAN, 30, P_MC_RATIO, 2, P_MC_THR, 50, P_MC_MAKE, 35, -1};
static const int16_t X_SYNTHWAVE[] = {P_CH_LOOP, 2, P_PULSE, 78, P_DRIVE, 35, P_SWING, 0, P_PAD, 55, P_ACID, 50, P_BASS, 72, P_AC_DENS, 70, P_AC_LEN, 16, P_ECHO, 35, P_REVERB, 45, P_SIZE, 70,
    KIT(11), P_PATTERN, 0, P_EVOLVE, 35, P_PL_CHO, 60, P_RV_TYPE, 2, P_RV_X, 40, P_BL_MODE, 3, P_BL_LEN, 28, P_BL_MOVE, 12, P_CM_VU, 60, P_MC_RATIO, 3, P_MC_THR, 46, P_MC_MAKE, 32, -1};
static const int16_t X_AMAPIANO[] = {P_CH_LOOP, 1, P_PULSE, 70, P_DRIVE, 15, P_SWING, 25, P_PAD, 45, P_ACID, 40, P_BASS, 80, P_AC_DENS, 40, P_AC_LEN, 12, P_ECHO, 30, P_REVERB, 40, P_PL_CHO, 25,
    KIT(5), P_PATTERN, 0, P_EVOLVE, 35, P_BL_MODE, 6, P_BL_LEN, 45, P_BL_MOVE, 12, P_BL_GLIDE, 35, P_CM_VU, 50, P_HUMAN, 20, P_MC_RATIO, 2, P_MC_THR, 50, P_MC_MAKE, 35, -1};
static const int16_t X_TRIPHOP[] = {P_CH_LOOP, 1, P_PULSE, 72, P_DRIVE, 40, P_SWING, 36,   /* (swing 36, 59 %, 0.15.13: the MPC shuffle; was 18) */ P_PAD, 50, P_ACID, 25, P_BASS, 80, P_AC_DENS, 25, P_AC_LEN, 12, P_ECHO, 45, P_REVERB, 50, P_SIZE, 70, P_RV_DAMP, 55,
    P_NOISE, 20, P_NZ_COLOR, 7, KIT(8), P_PATTERN, 0, P_EVOLVE, 30, P_BL_MODE, 1, P_BL_LEN, 75, P_BL_MOVE, 12, P_CM_VU, 55, P_CM_LOOP, 1, P_GHOST, 40, P_HUMAN, 30,
    P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 32, -1};
/* INDUSTRIAL (0.15.14, genre 17): industrial techno, EBM and industrial breakbeat, 118-145 BPM, phrygian and minor, suspended and fourths chords; driven,
 * harsh and detuned drums (the snare a tritone up, the perc a semitone off), a gated reverb, a 3/16 echo, the tape latched */
static const int16_t X_INDUS[] = {P_CH_LOOP, 1, P_BL_MODE, 1, P_BL_LEN, 40, P_BL_MOVE, 15, P_BL_GLIDE, 10, P_CM_VU, 40, P_GHOST, 50, P_HUMAN, 10, P_PULSE, 85, P_DRIVE, 75,
    P_SWING, 4, P_DETUNE, 40, P_PAD, 40, P_ACID, 50, P_BASS, 80, P_AC_DENS, 55, P_AC_LEN, 16, P_AC_RES, 65, P_ECHO, 30, P_ETIME, 1, P_REVERB, 30, P_RV_TYPE, 2, P_RV_X, 30,
    KIT(6), TU(1), 25, TU(5), 4, TU(0), -8, P_PATTERN, 0, P_EVOLVE, 45, P_MC_RATIO, 4, P_MC_THR, 38, P_MC_MAKE, 35, -1};
/* PSY (0.16.2, genre 18), from a YouTube commenter ("psytrance ... from cosmic psy-ambient into trance and back"): progressive, full-on and goa, 132-146 BPM,
 * the four-on-the-floor kick with the rolling bass between its beats (K-B-B-B), the 16th hats soft, the open hat off the beat, the acid as the squelching
 * lead, a 3/16 echo, phrygian and minor, a chord held for 8 bars; and three slow pieces (01, 02, 10: psy-ambient / psychill, 92-112) with a downtempo
 * beat, held bass, a wide shimmer (psy_slow), so a set can go from the cosmic into the trance and back. The Mix glide (SET page) carries the tempo. */
static const int16_t X_PSY[] = {P_CH_LOOP, 1, DE(0), 8, P_BL_MODE, 1, P_BL_LEN, 12, P_BL_MOVE, 4, P_BL_GLIDE, 0, P_CM_VU, 65, P_CM_LOOP, 1, P_GHOST, 10, P_HUMAN, 5, P_PULSE, 85,
    P_DRIVE, 30, P_SWING, 0, P_DETUNE, 30, P_PAD, 45, P_ACID, 60, P_BASS, 85, P_AC_DENS, 70, P_AC_LEN, 16, P_AC_RES, 70, P_AC_ENV, 60, P_AC_CUT, 40, P_AC_MUT, 30,
    P_ECHO, 35, P_ETIME, 1, P_REVERB, 40, P_SIZE, 70, P_RV_TYPE, 1, P_RV_X, 35, KIT(2), P_PATTERN, 0, P_EVOLVE, 50, P_MOTION, 55, P_MC_RATIO, 4, P_MC_THR, 40, P_MC_MAKE, 35, -1};
/* DARK FOREST (0.16.2, genre 19): forest and dark psy, 148-165 BPM, the same K-B-B-B under a dense, glitching percussion (3-against-4 perc, rolls, triplets),
 * driven hard drums, phrygian and fourths, the chord barely moving (i and the flat II), a dry room and a short echo */
static const int16_t X_FOREST[] = {P_CH_LOOP, 1, DE(0), 2, P_BL_MODE, 1, P_BL_LEN, 10, P_BL_MOVE, 6, P_BL_GLIDE, 0, P_CM_VU, 65, P_CM_LOOP, 1, P_GHOST, 40, P_HUMAN, 5, P_PULSE, 88,
    P_DRIVE, 55, P_SWING, 0, P_DETUNE, 45, P_PAD, 35, P_ACID, 55, P_BASS, 72, P_AC_DENS, 85, P_AC_LEN, 16, P_AC_RES, 80, P_AC_ENV, 70, P_AC_CUT, 35, P_AC_MUT, 45,
    P_ECHO, 30, P_ETIME, 0, P_REVERB, 35, P_SIZE, 55, P_RV_TYPE, 0, P_RV_DAMP, 55, KIT(6), TU(5), 7, P_PATTERN, 0, P_EVOLVE, 60, P_MOTION, 60, P_MC_RATIO, 4, P_MC_THR, 38,
    P_MC_MAKE, 35, -1};
/* HIPHOP (0.17.35, genre 20 on the dial): boom-bap instrumentals, 86-98 BPM, an MPC swing; the kick off the one and the snare loud on 2 and 4, eighth
 * hats, the bass on the kick; minor loops of one and four, harmonic minor's real V; Rhodes, strings, organ, brass and a muted trumpet over it; pieces 6
 * and 8 G-funk (Mixolydian, the acid a whistle), 5 and 9 dark (Phrygian). The Boom kit. */
static const int16_t X_HIPHOP[] = {P_CH_LOOP, 1, P_BL_MODE, 2, P_BL_LEN, 70, P_BL_MOVE, 10, P_BL_GLIDE, 15, P_CM_VU, 55, P_GHOST, 55, P_HUMAN, 25, P_PULSE, 85,
    P_DRIVE, 35, P_SWING, 30, P_PAD, 42, P_ACID, 22, P_BASS, 82, P_AC_DENS, 25, P_AC_LEN, 16, P_ECHO, 25, P_ETIME, 3, P_REVERB, 28, P_SIZE, 45, P_NOISE, 8,
    KIT(14), LV(1), 75, LV(3), 60, P_PATTERN, 0, P_EVOLVE, 30, P_PL_CHO, 20, P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 35, -1};
/* TRANCE (0.17.35): classic and uplifting, 136-140, straight; four on the floor, the open hat off the beat, the clap on 2 and 4, the bass off the beat,
 * the supersaw lead, the Trance pad, minor four-chord loops of four bars, long breakdowns and eight-bar builds (PSY covers psytrance, not this). */
static const int16_t X_TRANCE[] = {P_CH_LOOP, 2, P_BL_MODE, 4, P_BL_LEN, 30, P_BL_MOVE, 4, P_BL_GLIDE, 0, P_CM_VU, 60, P_CM_LOOP, 1, P_GHOST, 0, P_HUMAN, 5,
    P_PULSE, 85, P_DRIVE, 30, P_SWING, 0, P_DETUNE, 35, P_PAD, 55, P_ACID, 55, P_BASS, 80, P_AC_DENS, 65, P_AC_LEN, 16, P_AC_RES, 55, P_ECHO, 40, P_ETIME, 1,
    P_REVERB, 50, P_SIZE, 80, P_RV_TYPE, 0, KIT(10), P_PATTERN, 0, P_EVOLVE, 40, P_MOTION, 50, P_PL_CHO, 30,   /* (the plate and less chorus: the shimmer and
                                                 * the ensemble had core 2 at its limit, TRANCE 1 at 81 % with late renders, the maker's log 2026-11-14) */
    P_MC_RATIO, 4, P_MC_THR, 40,
    P_MC_MAKE, 35, -1};
/* AMBIENT (0.17.35): 70-88, beatless or nearly (a soft kick now and then, a shaker, a ride), long pads, slow chords in the bright modes, the bass held, a
 * long shimmer and echo: a set's opener and its rest. */
static const int16_t X_AMBIENT[] = {P_CH_LOOP, 0, P_BL_MODE, 0, P_CM_VU, 40, P_CM_DENS, 30, P_GHOST, 0, P_HUMAN, 15, P_PULSE, 45, P_DRIVE, 5, P_SWING, 10,
    P_PAD, 62, P_BRIGHT, 40, P_ACID, 18, P_BASS, 55, P_AC_DENS, 20, P_AC_LEN, 16, P_ECHO, 38, P_ETIME, 3, P_REVERB, 45, P_SIZE, 80, P_RV_TYPE, 1, P_RV_X, 22,
    P_PE_ATK, 70, P_PE_REL, 80, KIT(5), LV(0), 60, P_PATTERN, 0, P_EVOLVE, 35, P_MOTION, 40, P_PL_CHO, 30, P_MC_RATIO, 2, P_MC_THR, 50, P_MC_MAKE, 30, -1};
static const int16_t *const XBASE[18] = {X_DUBR, X_DNB, X_BREAKS, X_GARAGE, X_ELECTRO, X_TRAP, X_DISCO, X_FUNK, X_REGGAE, X_SYNTHWAVE, X_AMAPIANO, X_TRIPHOP, X_INDUS, X_PSY, X_FOREST,
                                         X_HIPHOP, X_TRANCE, X_AMBIENT};
static const uint8_t XROW[18][10][7] = {
    {{70, 9, 5, 5, 0, 0, 0}, {66, 2, 1, 5, 0, 0, 0}, {72, 7, 2, 5, 0, 0, 0}, {68, 0, 5, 5, 0, 0, 0}, {74, 4, 1, 5, 0, 0, 0}, {64, 5, 5, 2, 0, 0, 0}, {76, 10, 1, 3, 0, 0, 0}, {70, 3, 2, 5, 0, 0, 0}, {66, 6, 5, 5, 0, 0, 0}, {72, 11, 1, 2, 0, 0, 0}},
    {{174, 9, 1, 3, 0, 9, 4}, {172, 2, 5, 2, 1, 1, 15}, {174, 5, 3, 3, 3, 9, 11}, {175, 4, 2, 6, 7, 4, 14}, {174, 0, 5, 4, 11, 3, 2}, {176, 7, 1, 2, 5, 9, 4}, {170, 10, 5, 3, 2, 1, 15}, {175, 6, 2, 4, 7, 13, 13}, {172, 11, 1, 2, 0, 9, 11}, {178, 3, 5, 6, 11, 2, 14}},
    {{100, 2, 1, 1, 4, 3, 2}, {110, 4, 4, 4, 14, 2, 0}, {118, 9, 5, 1, 6, 11, 3}, {125, 0, 4, 1, 4, 3, 1}, {130, 7, 2, 6, 7, 4, 14}, {104, 5, 1, 2, 10, 2, 2}, {135, 11, 5, 4, 14, 11, 13}, {120, 3, 1, 1, 6, 3, 0}, {112, 10, 4, 1, 4, 2, 6}, {128, 6, 5, 4, 13, 13, 2}},
    {{130, 5, 1, 3, 0, 9, 4}, {132, 9, 5, 2, 4, 5, 11}, {128, 0, 4, 3, 1, 9, 12}, {134, 2, 1, 2, 5, 10, 4}, {130, 7, 5, 3, 0, 4, 15}, {136, 10, 1, 7, 1, 9, 11}, {126, 4, 5, 7, 4, 5, 12}, {132, 1, 4, 2, 5, 9, 4}, {130, 6, 1, 3, 3, 1, 15}, {138, 10, 5, 2, 0, 10, 11}},
    {{118, 9, 5, 1, 2, 10, 13}, {120, 2, 2, 6, 3, 6, 3}, {122, 0, 1, 4, 14, 11, 9}, {124, 4, 5, 1, 2, 10, 13}, {125, 7, 2, 6, 3, 6, 3}, {126, 5, 1, 4, 14, 11, 9}, {127, 11, 5, 1, 2, 10, 13}, {128, 9, 2, 6, 3, 6, 3}, {129, 6, 1, 4, 14, 11, 9}, {130, 2, 5, 1, 2, 10, 13}},
    {{130, 1, 5, 6, 7, 9, 11}, {134, 5, 5, 4, 12, 9, 4}, {136, 9, 2, 1, 2, 9, 15}, {138, 3, 5, 6, 13, 3, 11}, {140, 7, 5, 4, 7, 9, 4}, {140, 11, 5, 1, 12, 9, 15}, {142, 6, 2, 6, 2, 9, 11}, {144, 0, 5, 4, 13, 3, 4}, {146, 4, 2, 1, 7, 9, 15}, {150, 10, 5, 6, 12, 9, 11}},
    {{118, 9, 1, 2, 2, 10, 21}, {120, 2, 4, 3, 19, 9, 4}, {122, 7, 1, 2, 17, 1, 17}, {116, 4, 5, 2, 2, 5, 19}, {124, 0, 0, 3, 13, 10, 21}, {112, 5, 1, 3, 19, 17, 4}, {120, 10, 4, 2, 0, 9, 12}, {126, 3, 5, 3, 2, 10, 21}, {114, 6, 1, 2, 17, 5, 17}, {122, 11, 4, 2, 19, 1, 19}},
    {{100, 4, 4, 2, 17, 17, 21}, {104, 9, 1, 3, 4, 5, 4}, {96, 2, 7, 2, 13, 10, 19}, {108, 7, 4, 2, 0, 9, 21}, {102, 0, 1, 3, 19, 17, 17}, {98, 5, 5, 2, 17, 5, 21}, {106, 10, 4, 2, 4, 10, 4}, {110, 3, 1, 3, 13, 9, 19}, {100, 11, 1, 2, 19, 17, 21}, {104, 6, 4, 3, 0, 5, 17}},
    {{76, 9, 5, 1, 13, 9, 12}, {72, 4, 1, 2, 6, 1, 21}, {80, 2, 4, 1, 17, 8, 15}, {74, 7, 5, 2, 21, 7, 4}, {78, 0, 0, 1, 19, 9, 16}, {70, 5, 1, 1, 13, 5, 12}, {82, 10, 4, 2, 6, 9, 21}, {84, 3, 5, 1, 17, 1, 15}, {74, 6, 5, 2, 21, 8, 4}, {78, 11, 1, 1, 19, 7, 16}},
    {{108, 9, 5, 1, 10, 0, 26}, {112, 4, 5, 2, 20, 10, 24}, {104, 0, 0, 1, 2, 5, 25}, {116, 7, 1, 2, 9, 1, 30}, {100, 2, 5, 1, 5, 22, 26}, {110, 5, 1, 3, 10, 0, 24}, {118, 10, 5, 1, 20, 10, 25}, {106, 3, 0, 2, 2, 5, 28}, {114, 6, 5, 1, 9, 1, 30}, {102, 11, 1, 2, 5, 22, 26}},
    {{112, 9, 5, 3, 0, 9, 16}, {110, 4, 1, 3, 5, 1, 4}, {108, 2, 1, 2, 16, 8, 18}, {113, 7, 5, 3, 2, 21, 15}, {111, 0, 0, 2, 23, 9, 16}, {109, 5, 1, 7, 0, 1, 4}, {114, 10, 5, 3, 5, 9, 18}, {112, 3, 4, 3, 16, 8, 15}, {110, 6, 5, 2, 2, 21, 16}, {115, 11, 1, 3, 23, 9, 4}},
    {{84, 9, 5, 2, 7, 9, 15}, {78, 4, 2, 3, 2, 1, 17}, {72, 2, 1, 2, 16, 8, 4}, {88, 7, 5, 3, 5, 3, 20}, {80, 0, 5, 2, 1, 9, 11}, {70, 5, 2, 2, 7, 1, 15}, {86, 10, 1, 3, 2, 8, 17}, {76, 3, 5, 2, 16, 3, 4}, {82, 6, 2, 2, 5, 9, 20}, {74, 11, 5, 2, 7, 1, 11}},
    {{128, 4, 2, 4, 7, 13, 2}, {132, 9, 5, 6, 13, 20, 14}, {135, 2, 2, 6, 14, 4, 3}, {118, 7, 7, 4, 6, 2, 27}, {130, 0, 2, 4, 7, 3, 2}, {140, 5, 2, 6, 13, 13, 14},
     {122, 11, 5, 4, 14, 20, 3}, {145, 1, 2, 6, 7, 4, 2}, {126, 10, 7, 4, 6, 3, 27}, {142, 6, 2, 6, 13, 2, 14}},   /* industrial: 118-145 (techno at 135, breakbeat at 140-145) */
    {{92, 2, 1, 3, 23, 1, 15}, {104, 9, 5, 4, 1, 9, 11}, {132, 6, 5, 1, 9, 9, 0}, {136, 9, 2, 4, 3, 10, 3}, {140, 4, 2, 1, 2, 5, 8},
     {142, 7, 5, 6, 9, 10, 13}, {145, 6, 2, 2, 23, 9, 14}, {145, 4, 5, 2, 10, 0, 24}, {146, 1, 2, 4, 7, 6, 2}, {112, 4, 1, 3, 11, 1, 4}},   /* psy: 01, 02 psychill,
                                                 * 03-04 progressive, 05-09 full-on and goa, 10 psychill again */
    {{150, 4, 2, 6, 7, 9, 13}, {152, 5, 2, 4, 11, 13, 14}, {155, 1, 5, 1, 23, 20, 2}, {158, 7, 2, 6, 22, 9, 27}, {160, 2, 2, 5, 7, 8, 9},
     {148, 9, 5, 4, 16, 6, 13}, {162, 6, 2, 6, 11, 9, 14}, {165, 11, 2, 1, 7, 20, 2}, {156, 8, 7, 4, 23, 13, 27}, {160, 0, 2, 6, 22, 9, 13}},   /* dark forest: 148-165 */
    {{90, 9, 5, 2, 0, 9, 15}, {94, 4, 1, 2, 0, 9, 15}, {88, 2, 9, 1, 2, 8, 17}, {92, 7, 5, 3, 0, 9, 22}, {86, 0, 1, 2, 4, 8, 15},
     {96, 5, 4, 2, 17, 10, 26}, {90, 10, 1, 3, 0, 9, 17}, {98, 3, 4, 2, 17, 10, 26}, {88, 11, 2, 1, 2, 8, 30}, {92, 6, 9, 2, 4, 9, 15}},   /* hiphop: 86-98; 06, 08
                                                 * G-funk (Mixolydian), 05, 09 dark (Phrygian), 03, 10 harmonic minor */
    {{138, 9, 5, 1, 9, 0, 24}, {134, 4, 1, 2, 9, 0, 8}, {138, 2, 9, 1, 2, 5, 24}, {140, 7, 5, 4, 23, 0, 27}, {136, 0, 2, 1, 9, 15, 24},
     {132, 5, 5, 3, 10, 0, 8}, {140, 10, 10, 1, 9, 5, 24}, {138, 3, 1, 4, 2, 0, 30}, {137, 11, 5, 2, 9, 15, 27}, {135, 6, 9, 4, 23, 0, 24}},   /* trance: 132-140,
                                                 * Aeolian, Dorian, Phrygian, harmonic and melodic minor; triads, 7ths, 9ths, sus */
    {{80, 2, 3, 3, 23, 23, 15}, {76, 7, 0, 4, 22, 9, 11}, {84, 9, 1, 3, 3, 1, 18}, {72, 4, 3, 4, 11, 21, 15}, {78, 0, 6, 1, 23, 23, 16},
     {88, 5, 5, 3, 5, 9, 22}, {74, 10, 0, 3, 22, 12, 11}, {82, 3, 1, 4, 3, 1, 18}, {70, 11, 3, 3, 23, 21, 15}, {86, 6, 5, 2, 11, 9, 16}},   /* ambient: 70-88,
                                                 * Lydian, Ionian, Dorian, Aeolian, the major pentatonic; 9ths and suspended chords */
};
static const uint16_t XPAT[18][6][6] = {
    {{0x0100, 0x0100, 0x0000, 0x4444, 0x0000, 0x0000}, {0x1111, 0x0100, 0x0000, 0x5555, 0x0000, 0x0000}, {0x2111, 0x1010, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0100, 0x0100, 0x0000, 0x0000, 0x4000, 0x0000}, {0x0101, 0x0100, 0x0100, 0x4444, 0x0000, 0x2020}, {0x1111, 0x1010, 0x0000, 0x4444, 0x4040, 0x0000}},
    {{0x0401, 0x1010, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0401, 0x1010, 0x8080, 0xFF7F, 0x0000, 0x4040}, {0x0405, 0x1010, 0x8080, 0xFFFF, 0x0000, 0x0000}, {0x0401, 0x1010, 0x0000, 0x4444, 0x0000, 0x8008}, {0x0409, 0x1010, 0x0000, 0x5555, 0x0000, 0x4080}, {0x4041, 0x0100, 0x0100, 0x5555, 0x1000, 0x0000}},
    {{0x2445, 0x1010, 0x8A80, 0xDF7F, 0x2080, 0x0000}, {0x4181, 0x1010, 0x0000, 0x51D5, 0x0400, 0x0000}, {0x0C83, 0x1010, 0x0000, 0x0000, 0x5555, 0x0000}, {0x0501, 0x9090, 0x0000, 0x5555, 0x0000, 0x0000}, {0x4405, 0x1010, 0x1010, 0x5555, 0x8000, 0x0080}, {0x0005, 0xF510, 0x1000, 0x0155, 0x0000, 0x0000}},
    {{0x0401, 0x1010, 0x0000, 0x5D5D, 0x4044, 0x0000}, {0x0421, 0x1010, 0x0000, 0x1313, 0x4044, 0x8880}, {0x1111, 0x8080, 0x1010, 0x4444, 0x0000, 0x0000}, {0x0449, 0x1010, 0x0000, 0xDDDD, 0x4044, 0x0000},   /* (0.15.13: two rows of shuffled 16ths) */ {0x0401, 0x9244, 0x1010, 0x0111, 0x4000, 0x0000}, {0x0401, 0xF010, 0x0000, 0x1111, 0x0044, 0x0000}},
    {{0x2449, 0x0000, 0x1010, 0x4444, 0x0000, 0x0200}, {0x0909, 0x0000, 0x1010, 0x5555, 0x0000, 0x4040}, {0x0505, 0x1010, 0x1010, 0x5555, 0x0000, 0x8040}, {0x2449, 0x1000, 0x0010, 0x4444, 0x0000, 0x1449}, {0x050B, 0x0000, 0x1010, 0xFFFF, 0x0000, 0x0000}, {0x0109, 0xF000, 0x1110, 0x0055, 0x0000, 0x0000}},
    {{0x1001, 0x0000, 0x0100, 0xFFFF, 0x0000, 0x0820}, {0x0481, 0x0100, 0x0100, 0x5555, 0x8000, 0x0000}, {0x0009, 0x0100, 0x0100, 0xF555, 0x0000, 0x2008}, {0x2041, 0x0100, 0x0100, 0x7575, 0x8000, 0x0000}, {0x0401, 0x0100, 0x0100, 0x5555, 0x0000, 0x0820}, {0x0801, 0xD100, 0x0100, 0xFF55, 0x0000, 0x0000}},   /* (0.15.13: hats in 16ths and 8ths that can roll, an open hat on the last 16th, a rim: the genre audit) */
    {{0x1111, 0x1010, 0x1010, 0x5555, 0x4444, 0x0000}, {0x1111, 0x1010, 0x1010, 0xFFFF, 0x4444, 0x2222}, {0x1111, 0x1010, 0x1010, 0x7777, 0x4444, 0x0A0A}, {0x1151, 0x1010, 0x1010, 0x5555, 0x4444, 0x2020}, {0x1111, 0x1010, 0x1010, 0xDDDD, 0x4040, 0x0000}, {0x9111, 0x1010, 0x3030, 0xFFFF, 0x4444, 0x2222}},
    {{0x0405, 0x1010, 0x0000, 0xFFFF, 0x0000, 0x0000}, {0x0485, 0x1010, 0x0000, 0xFFFF, 0x2000, 0x0220}, {0x4411, 0x1010, 0x0000, 0x7777, 0x0020, 0x0000}, {0x0525, 0x9010, 0x0000, 0xFFFF, 0x0000, 0x8888}, {0x0409, 0x1090, 0x0000, 0xDDDD, 0x4000, 0x0000}, {0x2485, 0x1210, 0x0000, 0xFFFF, 0x0020, 0x2424}},
    {{0x0100, 0x0100, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0100, 0x0100, 0x0000, 0xFFFF, 0x0000, 0x0000}, {0x1111, 0x0100, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0101, 0x0100, 0x0000, 0x5555, 0x4000, 0x0000}, {0x1111, 0x1010, 0x0000, 0xFFFF, 0x0000, 0x0000}, {0x0100, 0x0100, 0x0100, 0x5D5D, 0x0000, 0x2020}},
    {{0x1111, 0x1010, 0x1010, 0x5555, 0x0000, 0x0000}, {0x1111, 0x1010, 0x1010, 0xFFFF, 0x0000, 0x0808}, {0x1151, 0x1010, 0x1010, 0x5555, 0x4000, 0x0000}, {0x1111, 0x1010, 0x1010, 0x5D5D, 0x0000, 0x8080}, {0x0101, 0x0100, 0x0100, 0x5555, 0x0000, 0x0000}, {0x1115, 0x1010, 0x1010, 0xFFFF, 0x4040, 0x0000}},
    {{0x1111, 0x0000, 0x1010, 0x5555, 0x4444, 0x2424}, {0x1101, 0x0000, 0x1010, 0x7777, 0x4444, 0x2A2A}, {0x1111, 0x0100, 0x1010, 0xFFFF, 0x4444, 0x8484}, {0x1151, 0x0000, 0x1010, 0x5D5D, 0x4000, 0x2424}, {0x1111, 0x1000, 0x1010, 0x5555, 0x4444, 0x0A0A}, {0x1131, 0x0000, 0x1010, 0xDDDD, 0x0000, 0x2222}},   /* (0.15.13: the open hat on the off-beats in four rows) */
    {{0x0401, 0x1010, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0481, 0x1010, 0x0000, 0x5D5D, 0x0000, 0x0220}, {0x0109, 0x1010, 0x0000, 0x5555, 0x4000, 0x0000}, {0x0421, 0x9010, 0x0000, 0xDDDD, 0x0000, 0x8080}, {0x0201, 0x1010, 0x1010, 0x5555, 0x0040, 0x0000}, {0x4401, 0x1090, 0x0000, 0xFFFF, 0x0000, 0x2020}},
    {{0x0405, 0x9290, 0x0000, 0x5555, 0x4000, 0x2020}, {0x2449, 0x1010, 0x8000, 0xDDDD, 0x0404, 0x2222}, {0x0421, 0x0100, 0x4100, 0xDDDD, 0x0000, 0x4924},
     {0x1249, 0x1010, 0x0000, 0x5555, 0x8000, 0x0000}, {0x1111, 0x1010, 0x4444, 0xFFFF, 0x0000, 0x0808}, {0x1405, 0xED10, 0x1000, 0x0055, 0x4000, 0x9200}},   /* industrial: broken
                                                 * (Amen ghosts), off-beat kick, half-time grind (perc 3 against 4), dotted, EBM stomp, a fill */
    {{0x1111, 0x0000, 0x1010, 0xAAAA, 0x4444, 0x0000}, {0x1111, 0x1010, 0x0000, 0xEEEE, 0x4444, 0x0808}, {0x1111, 0x0000, 0x1010, 0xAAAA, 0x4444, 0x2222},
     {0x1111, 0x0000, 0x0000, 0xEEEE, 0x4444, 0x8888}, {0x1111, 0x1010, 0x1010, 0xAAAA, 0x0000, 0x4924}, {0x1111, 0x1000, 0x1010, 0xEEEE, 0x4444, 0xA888}},   /* psy: the kick on
                                                 * every beat, 16th hats between, the open hat off the beat, a clap or a snare on 2 and 4, the perc rolling */
    {{0x1111, 0x0000, 0x1010, 0xEEEE, 0x4444, 0x5A5A}, {0x1111, 0x1010, 0x0000, 0xFFFF, 0x0000, 0x2492}, {0x1111, 0x0000, 0x0000, 0xEEEE, 0x4444, 0xB6DB},
     {0x1111, 0x8010, 0x1010, 0xAAAA, 0x4444, 0x6D6D}, {0x1111, 0x0000, 0x1010, 0xFFFF, 0x4040, 0x9249}, {0x1111, 0x1012, 0x0000, 0xEEEE, 0x4444, 0xDB6D}},   /* dark forest: the
                                                 * same kick under every-16th hats and a perc in threes (3 against 4), glitching */
    {{0x0481, 0x1010, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0221, 0x1010, 0x0000, 0x5555, 0x0000, 0x0000}, {0x2481, 0x1010, 0x0000, 0x5D5D, 0x0000, 0x8000},
     {0x0109, 0x1010, 0x0000, 0x5555, 0x4000, 0x0000}, {0x0485, 0x9010, 0x0000, 0x5555, 0x0000, 0x0808}, {0x0441, 0x1010, 0x0000, 0xDDDD, 0x0000, 0x2020}},   /* hiphop:
                                                 * boom-bap kicks off the one (1 8 11, 1 6 10, 1 4 9 ...), the snare on 2 and 4, eighth or swung 16th hats, a rim */
    {{0x1111, 0x0000, 0x1010, 0xEEEE, 0x4444, 0x0000}, {0x1111, 0x0000, 0x1010, 0x5555, 0x4444, 0x8888}, {0x1111, 0x1010, 0x1010, 0xFFFF, 0x4444, 0x0000},
     {0x1111, 0x0000, 0x1010, 0xAAAA, 0x4444, 0x2222}, {0x1111, 0x0000, 0x1010, 0xEEEE, 0x4444, 0x0808}, {0x1111, 0x1000, 0x1010, 0xFFFF, 0x4444, 0xA0A0}},   /* trance:
                                                 * four on the floor, the open hat off every beat, the clap on 2 and 4, 16th hats */
    {{0x0001, 0x0000, 0x0000, 0x4444, 0x0000, 0x0000}, {0x0101, 0x0000, 0x1000, 0x5555, 0x0000, 0x0000}, {0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0808},
     {0x0001, 0x1000, 0x0000, 0x4444, 0x0000, 0x2020}, {0x0101, 0x0000, 0x1010, 0x5555, 0x4000, 0x0000}, {0x0000, 0x0000, 0x0000, 0x1111, 0x0000, 0x8888}},   /* ambient:
                                                 * a kick on the one now and then, soft hats or a ride, a shaker, and bars with no drums at all */
};
/* psy's slow pieces (01, 02, 10: psy-ambient, psychill): a downtempo beat instead (the kick on 1 and 3 or a slow four, the snare on 2 and 4, eighth hats) */
static const uint16_t PSY_DOWN[6][6] = {
    {0x0101, 0x1010, 0x0000, 0x5555, 0x0000, 0x0820}, {0x0901, 0x1010, 0x0000, 0x5555, 0x4000, 0x0000}, {0x1111, 0x0000, 0x1010, 0x4444, 0x0000, 0x2020},
    {0x0121, 0x1010, 0x0000, 0xDDDD, 0x0000, 0x0808}, {0x0101, 0x0000, 0x1010, 0x5555, 0x0040, 0x8080}, {0x1111, 0x1010, 0x0000, 0x5555, 0x4444, 0x0000}};
static int psy_slow(int g, int i) { return g == 17 && (i == 0 || i == 1 || i == 9); }
static const char *const XNAME[18][10] = {
    {"V01", "V02", "V03", "V04", "V05", "V06", "V07", "V08", "V09", "V10"},
    {"N01", "N02", "N03", "N04", "N05", "N06", "N07", "N08", "N09", "N10"},
    {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10"},
    {"G01", "G02", "G03", "G04", "G05", "G06", "G07", "G08", "G09", "G10"},
    {"E01", "E02", "E03", "E04", "E05", "E06", "E07", "E08", "E09", "E10"},
    {"T01", "T02", "T03", "T04", "T05", "T06", "T07", "T08", "T09", "T10"},
    {"S01", "S02", "S03", "S04", "S05", "S06", "S07", "S08", "S09", "S10"},
    {"F01", "F02", "F03", "F04", "F05", "F06", "F07", "F08", "F09", "F10"},
    {"R01", "R02", "R03", "R04", "R05", "R06", "R07", "R08", "R09", "R10"},
    {"W01", "W02", "W03", "W04", "W05", "W06", "W07", "W08", "W09", "W10"},
    {"A01", "A02", "A03", "A04", "A05", "A06", "A07", "A08", "A09", "A10"},
    {"P01", "P02", "P03", "P04", "P05", "P06", "P07", "P08", "P09", "P10"},
    {"I01", "I02", "I03", "I04", "I05", "I06", "I07", "I08", "I09", "I10"},
    {"Y01", "Y02", "Y03", "Y04", "Y05", "Y06", "Y07", "Y08", "Y09", "Y10"},
    {"O01", "O02", "O03", "O04", "O05", "O06", "O07", "O08", "O09", "O10"},
    {"M01", "M02", "M03", "M04", "M05", "M06", "M07", "M08", "M09", "M10"},
    {"U01", "U02", "U03", "U04", "U05", "U06", "U07", "U08", "U09", "U10"},
    {"Z01", "Z02", "Z03", "Z04", "Z05", "Z06", "Z07", "Z08", "Z09", "Z10"},
};
static void x_pats(int g, int i, uint16_t pats[EN_NPAT][EN_NLANE])
{
    int p, l;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            pats[p][l] = (psy_slow(g + 4, i) ? PSY_DOWN : XPAT[g])[(l == 0 || l == 5) && i >= 6 ? (i + p + 2) % 6 : (i + p) % 6][l];   /* (psy's slow pieces: the downtempo rows) */   /* pieces 7..10: the kick and perc of another row (no two pieces play the same six) */
}
#undef LV
#undef TU
#undef DE
#undef KIT
static const int16_t *const LOFI_PIECES[EN_NPIECES] = {PL_01, PL_02, PL_03, PL_04, PL_05, PL_06, PL_07, PL_08, PL_09, PL_10};
static const char *const LOFI_NAME[EN_NPIECES] = {"L01", "L02", "L03", "L04", "L05", "L06", "L07", "L08", "L09", "L10"};
/* the lofi drum patterns: six for each piece, put together from small palettes (the piece and the pattern choose which),
 * boom-bap: the kick off the one, the snare on 2 and 4 (some with a ghost), swung eighths or sixteenths on the hats */
static void lofi_pats(int i, uint16_t pats[EN_NPAT][EN_NLANE])
{
    static const uint16_t KICK[12] = {0x0401, 0x0481, 0x0109, 0x0921, 0x0101, 0x0141, 0x0449, 0x4401, 0x0905, 0x1111, 0x0421, 0x0201};
    static const uint16_t SNARE[4] = {0x1010, 0x9010, 0x1090, 0x1010};
    static const uint16_t HAT[6] = {0x5555, 0xFFFF, 0x5D5D, 0xF5F5, 0xDDDD, 0x5757};
    static const uint16_t OPEN[5] = {0x0000, 0x4000, 0x0040, 0x4040, 0x0000};
    static const uint16_t PERC[6] = {0x0000, 0x2020, 0x0808, 0x8888, 0x0000, 0x0220};
    int p;
    for (p = 0; p < EN_NPAT; p++) {
        pats[p][0] = KICK[(i * 5 + p * 3) % 12];
        pats[p][1] = SNARE[(i + p) % 4];
        pats[p][2] = (i + p) % 3 == 0 ? (uint16_t)0x1010 : (uint16_t)0;
        pats[p][3] = HAT[(i * 2 + p) % 6];
        pats[p][4] = OPEN[(i + p * 2) % 5];
        pats[p][5] = PERC[(i * 3 + p) % 6];
    }
}
/* the house patterns: the kick four on the floor (a few with an extra), the clap on 2 and 4, the open hats on the off-beats
 * (some pieces: every other), closed hats in eighths or sixteenths, a shaker or conga ghosting between */
static void house_pats(int i, uint16_t pats[EN_NPAT][EN_NLANE])
{
    static const uint16_t KICK[6] = {0x1111, 0x1111, 0x1111, 0x1151, 0x9111, 0x1115};
    static const uint16_t CLAP[4] = {0x1010, 0x1010, 0x1210, 0x5010};
    static const uint16_t HAT[6] = {0x5555, 0xFFFF, 0xEEEE, 0x7777, 0xDDDD, 0x5D5D};
    static const uint16_t OPEN[4] = {0x4444, 0x4444, 0x4040, 0x4444};
    static const uint16_t PERC[6] = {0x0000, 0x0A0A, 0x2828, 0x8421, 0x4A4A, 0x2424};
    int p;
    for (p = 0; p < EN_NPAT; p++) {
        pats[p][0] = KICK[(i + p * 2) % 6];
        pats[p][1] = (i + p) % 5 == 0 ? (uint16_t)0x1010 : (uint16_t)0;
        pats[p][2] = CLAP[(i + p) % 4];
        pats[p][3] = HAT[(i * 3 + p) % 6];
        pats[p][4] = OPEN[(i + p) % 4];
        pats[p][5] = PERC[(i * 2 + p) % 6];
    }
}
/* the dubstep patterns: half time, the kick on the one (and one more, off the beat), the snare and clap on the third beat,
 * sparse hats, now and then an open hat or a perc hit before the snare */
static void dub_pats(int i, uint16_t pats[EN_NPAT][EN_NLANE])
{
    static const uint16_t KICK[7] = {0x0001, 0x0401, 0x0041, 0x0081, 0x0801, 0x0021, 0x0A01};
    static const uint16_t SNARE[3] = {0x0100, 0x4100, 0x8100};   /* 3; 3 and 4+; 3 and 4a (0.15.13: 0x0110 put a hit on 2 and blurred the half time) */
    static const uint16_t HAT[6] = {0x5555, 0x4444, 0x7777, 0x5D5D, 0xFFFF, 0x1515};
    static const uint16_t OPEN[4] = {0x0000, 0x0400, 0x0040, 0x4000};
    static const uint16_t PERC[4] = {0x0000, 0x2000, 0x0010, 0x0220};
    int p;
    for (p = 0; p < EN_NPAT; p++) {
        pats[p][0] = KICK[(i * 3 + p * 2) % 7];
        pats[p][1] = SNARE[(i + p) % 3];
        pats[p][2] = 0x0100;
        pats[p][3] = HAT[(i + p * 2) % 6];
        pats[p][4] = OPEN[(i + p) % 4];
        pats[p][5] = PERC[(i * 2 + p) % 4];
    }
}
static const int16_t *const PIECES[EN_NPIECES] = {
    PC_HYPNOS, PC_UNDERTOW, PC_MONOLITH, PC_ASCENSION, PC_NIGHT_DRIVE,
    PC_DUB_CHAMBER, PC_GLASS_TUNNEL, PC_TIDAL, PC_WAREHOUSE, PC_DEEP_FIELD,
};
static const char *const PIECE_NAME[EN_NPIECES] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10"};

const char *en_piece_name(int i) { return PIECE_NAME[(unsigned)i < EN_NPIECES ? i : 0]; }
static const int16_t *const *const GENRE_PIECES[EN_NGENRE] = {PIECES, LOFI_PIECES, HOUSE_PIECES, DUB_PIECES};
static const char *const GENRE_NAME[EN_NGENRE] = {"HYPNO", "LOFI", "HOUSE", "DUBSTEP", "DUB", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP", "DISCO", "FUNK", "REGGAE", "SYNTHWV", "AMAPI", "TRIPHOP", "INDUS", "PSY", "FOREST", "HIPHOP", "TRANCE", "AMBIENT"};
const char *en_genre_name(int g) { return GENRE_NAME[(unsigned)g < EN_NGENRE ? g : 0]; }
const char *en_piece_name_g(int g, int i)
{
    i = (unsigned)i < EN_NPIECES ? i : 0;
    if (g >= 4 && g < EN_NGENRE)
        return XNAME[g - 4][i];
    return g == 1 ? LOFI_NAME[i] : g == 2 ? HOUSE_NAME[i] : g == 3 ? DUB_NAME[i] : PIECE_NAME[i];
}
/* Each genre's palette of sounds, five of the pad, drone, acid and pluck (the pluck's from the plain sounds and the DX voices) and three of each drum's kit: a piece takes one of each by its number, with a
 * different stride for each (1, 2, 3, 4 of five; 1, 2 of three), so the pieces of a genre differ in sound as well as in key and tempo, and no two
 * neighbours share them. The pad, drone and acid sounds are the genres' own choices (the original genre's keep theirs). */
static const struct { uint8_t pad[5], drone[5], acid[5], pluck[5], kick[3], snare[3], hat[3], perc[3]; } PALETTE[EN_NGENRE] = {   /* (0.15.5: no piece starts its bass on Bite, Tine or Clang (0.17.69: now Solid and Growl), too high and jarring for a bass; 0.17.42: nor on FM or Metal, too harsh and piercing; they stay to choose by hand) */
    {{0}, {0}, {0}, {1, 4, 13, 28, 16}, {0}, {0}, {0}, {0}},
    {{0, 17, 5, 2, 3}, {1, 9, 7, 3, 23}, {15, 17, 18, 22, 4}, {12, 2, 17, 23, 28}, {8, 8, 7}, {8, 8, 1}, {8, 5, 8}, {8, 5, 8}},
    {{6, 13, 0, 10, 14}, {9, 10, 5, 1, 12}, {4, 9, 25, 8, 5}, {24, 7, 27, 20, 9}, {1, 10, 2}, {1, 10, 11}, {1, 10, 5}, {5, 1, 11}},
    {{7, 8, 11, 22, 1}, {16, 4, 20, 3, 10}, {14, 2, 24, 3, 20}, {31, 6, 22, 33, 8}, {9, 7, 6}, {9, 6, 4}, {9, 5, 2}, {9, 4, 6}},
    {{13, 8, 6, 17, 21}, {9, 1, 5, 8, 20}, {12, 4, 16, 21, 15}, {18, 24, 12, 30, 36}, {8, 8, 7}, {8, 5, 8}, {8, 5, 8}, {5, 8, 11}},
    {{3, 23, 5, 22, 10}, {4, 9, 16, 21, 20}, {8, 13, 23, 15, 9}, {28, 11, 2, 3, 5}, {12, 10, 12}, {12, 4, 10}, {12, 5, 10}, {12, 4, 5}},
    {{6, 14, 19, 10, 0}, {11, 2, 22, 5, 3}, {2, 1, 24, 3, 19}, {20, 24, 27, 9, 18}, {4, 6, 3}, {4, 6, 10}, {4, 10, 2}, {4, 11, 6}},
    {{13, 17, 0, 20, 21}, {9, 5, 21, 1, 10}, {4, 15, 25, 16, 9}, {12, 19, 28, 2, 29}, {1, 5, 8}, {5, 1, 8}, {1, 5, 10}, {5, 11, 1}},
    {{10, 14, 9, 3, 12}, {5, 10, 14, 0, 9}, {9, 6, 26, 23, 5}, {27, 10, 8, 22, 31}, {11, 10, 3}, {11, 2, 10}, {11, 2, 3}, {11, 10, 2}},
    {{7, 15, 22, 3, 18}, {9, 8, 22, 1, 23}, {11, 18, 16, 4, 22}, {13, 22, 33, 3, 29}, {13, 7, 9}, {13, 7, 10}, {13, 7, 11}, {13, 11, 7}},
    {{2, 19, 17, 0, 13}, {10, 9, 1, 5, 3}, {21, 4, 17, 19, 12}, {35, 24, 39, 15, 25}, {1, 10, 1}, {10, 1, 10}, {1, 10, 10}, {10, 1, 5}},
    {{17, 21, 13, 19, 0}, {1, 5, 10, 9, 20}, {21, 4, 19, 17, 12}, {36, 24, 35, 41, 12}, {8, 10, 8}, {4, 8, 11}, {4, 8, 4}, {4, 11, 8}},   /* (funk's kick 0.15.13: Dusty and 909, not Break: its chops and crush broke the pocket) */
    {{13, 6, 17, 21, 19}, {9, 1, 8, 7, 5}, {12, 21, 15, 4, 16}, {34, 35, 18, 30, 31}, {8, 5, 7}, {8, 5, 8}, {8, 5, 8}, {5, 8, 5}},
    {{10, 20, 2, 9, 5}, {0, 10, 5, 1, 22}, {26, 24, 25, 30, 28}, {39, 27, 12, 26, 25}, {11, 9, 11}, {9, 11, 9}, {11, 11, 6}, {11, 9, 11}},
    {{0, 5, 17, 2, 18}, {9, 1, 8, 21, 5}, {7, 4, 16, 18, 15}, {17, 23, 40, 5, 29}, {5, 1, 8}, {1, 5, 1}, {5, 1, 8}, {5, 8, 1}},
    {{7, 2, 16, 5, 1}, {9, 1, 8, 3, 7}, {15, 17, 4, 20, 11}, {35, 12, 5, 28, 4}, {8, 8, 9}, {8, 4, 8}, {8, 8, 4}, {8, 5, 8}},
    {{7, 13, 14, 6, 11}, {4, 20, 16, 2, 3}, {2, 14, 3, 27, 0}, {33, 22, 6, 10, 8}, {6, 4, 6}, {4, 6, 2}, {2, 6, 4}, {6, 2, 6}},   /* industrial (0.15.14): Dark, Hits, Rave, Stabs,
                                                 * Hollow; Reese, Gnarl, Wub, Grit, Dirty; Driven, Fold, Sync, Hyper, Saw; TUBUL-BELL, TEMPLEBELL, Gong, Bleep, Digital; the Hard
                                                 * kick (the Break kit's chop in a third of the pieces), Break and Hard snares, Techno hats, a Hard clank */
    {{9, 23, 1, 3, 2}, {9, 5, 10, 0, 1}, {0, 8, 13, 3, 14}, {10, 8, 11, 4, 13}, {3, 10, 0}, {10, 2, 11}, {2, 5, 11}, {3, 11, 5}},   /* psy (0.16.2): Trance, Air,
                                                 * Voices, Glass, Strings; Sub, Pulse, Square, Saws, Warm; Saw, Trance, Laser, Sync, Fold; Bleep, Digital, Glass, Bell, DX Glass Bell; Acid, 909 and Hypno kicks, short (0.17.43: no rumble kick, the bass is the rumble),
                                                 * 909 and Electro snares, Techno and Minimal hats, the Acid perc */
    {{7, 11, 23, 22, 16}, {9, 4, 20, 8, 3}, {13, 14, 2, 27, 9}, {6, 10, 8, 33, 11}, {10, 3, 0}, {11, 6, 4}, {5, 11, 2}, {11, 6, 5}},   /* dark forest (0.16.2): Dark,
                                                 * Hollow, Air, Chapel, Ooh; Sub, Reese, Gnarl, Rumble, Dirty; Laser, Fold, Driven, Hyper, Bleep; Gong, Bleep, Digital, DX Tubular, Glass; 909, Acid and Hypno kicks, short (0.17.43: the Hard kick's rumble hid the bass after it) */
    {{0, 2, 4, 19, 17}, {9, 1, 8, 10, 5}, {15, 17, 22, 26, 30}, {12, 41, 40, 36, 5}, {14, 8, 7}, {14, 16, 8}, {14, 8, 10}, {14, 20, 8}},   /* hiphop (0.17.35):
                                                 * Rhodes, Strings, Organ, Brass, Wurli; Sub, Warm, Rumble, Square, Pulse; Soft, E.Piano, Vibes, Mini, Lead; DX Tine EP, Mute Trumpet, Sax,
                                                 * Muted Gtr, Vibes; the Boom kit, Dusty and 808 kicks, Snap snares, 909 hats, a shaker */
    {{9, 2, 23, 10, 1}, {0, 5, 15, 9, 4}, {24, 8, 0, 27, 30}, {13, 28, 22, 0, 8}, {10, 2, 3}, {10, 3, 1}, {10, 2, 15}, {3, 10, 20}},   /* trance: Trance, Strings,
                                                 * Air, Poly, Voices; Saws, Pulse, Bright, Sub, Reese; Super, Trance, Saw, Hyper, Lead; DX Glass Bell, Harp, Bells, Pluck, Digital; 909,
                                                 * Techno and Acid, the Ride's hats, a shaker */
    {{23, 22, 3, 11, 5}, {23, 9, 1, 21, 12}, {15, 11, 18, 16, 22}, {29, 28, 13, 26, 11}, {5, 8, 15}, {5, 8, 16}, {15, 5, 20}, {20, 5, 15}},   /* ambient: Air,
                                                 * Chapel, Glass, Hollow, Warm; Air, Sub, Warm, Wide, Fifths; Soft, Bell, Kalimba, Marimba, Vibes; DX Kalimba, Harp, Glass Bell, Glass
                                                 * Pad, Glass; Minimal and Dusty, the Ride, the Shaker */
};
static const uint8_t PLUCK_RATE[EN_NGENRE] = {2, 2, 2, 0, 0, 3, 2, 2, 3, 0, 2, 3, 0, 2, 3, 0, 3, 3, 3, 2, 3, 0};   /* the pluck's rate by genre: 1/8 (2), 1/16 (3), 1/4 (0) */
/* pieces 6-10's pad: the palette shifted by PAD_K[g] from pieces 1-5's (0.16.7: it was 2 for all, which gave 1 and 9, 3 and 6, 4 and 7 one pad over the
 * same mode and chord kind: the audit's "twins"; each genre's shift chosen by tests/host/gtwin.c, SEARCH=1, for the fewest pairs too alike) */
static const uint8_t PAD_K[EN_NGENRE] = {2, 4, 2, 4, 4, 0, 2, 0, 0, 0, 0, 0, 2, 0, 0, 4, 4, 2, 0, 2, 0, 4};
/* (0.17.28) THE PIECES' ROLES. The ten pieces of a genre had differed in key, mode, chord, tempo and their sounds, but not in how they were arranged:
 * the same voice in front, the same levels, line, echo and density in each, so they sounded like one track in ten keys (an outside review, 2026-11-12).
 * Now each piece but the first takes a role over the genre's recipe: a nudge to which voice leads and how the space and the groove sit. The genre keeps
 * what makes it the genre (its kit, swing, drum patterns, tempo, scale, chords, sounds), and each genre also locks the parts that define it (GPROT):
 * its bass line, its acid, its pad, its drum lanes' lengths. Measured on the host (all 180 pieces, 32 bars: the audio's bands, loudness, width, and each
 * voice's notes a bar and register): see the CHANGELOG. */
enum { PR_BASS = 1, PR_ACID = 2, PR_PAD = 4, PR_LANES = 8 };
enum { RL_CLASSIC, RL_BASS, RL_CHORDS, RL_ACID, RL_PLUCK, RL_SPACE, RL_PEAK, RL_DARK, RL_SYNC, RL_LEFT, EN_NROLE };
static const uint8_t GPROT[EN_NGENRE] = {
    0, PR_PAD, PR_BASS, PR_BASS, PR_BASS, PR_LANES | PR_BASS,                  /* HYPNO (no roles), LOFI, HOUSE, DUBSTEP, DUB, DNB */
    PR_LANES, PR_LANES | PR_BASS, 0, PR_BASS | PR_LANES, PR_BASS | PR_LANES,   /* BREAKS, GARAGE, ELECTRO, TRAP, DISCO */
    PR_BASS, PR_BASS | PR_LANES, PR_PAD, PR_BASS | PR_LANES, PR_PAD | PR_LANES, /* FUNK, REGGAE, SYNTHWV, AMAPI, TRIPHOP */
    PR_LANES, PR_BASS | PR_ACID, PR_BASS | PR_ACID,                           /* INDUS, PSY, FOREST */
    PR_BASS | PR_LANES, PR_BASS, PR_PAD};                                     /* HIPHOP, TRANCE, AMBIENT */
static void pset(int16_t *par, int p, int v) { par[p] = (int16_t)(v < PARAMS[p].lo ? PARAMS[p].lo : v > PARAMS[p].hi ? PARAMS[p].hi : v); }
static void padd(int16_t *par, int p, int d) { pset(par, p, par[p] + d); }
static void pscl(int16_t *par, int p, int pct) { pset(par, p, (par[p] * pct + 50) / 100); }
int en_piece_role(int g, int i) { return g <= 0 || (unsigned)g >= EN_NGENRE || i <= 0 || i >= EN_NPIECES ? RL_CLASSIC : 1 + (i - 1 + g) % (EN_NROLE - 1); }
static void role(int g, int i, int16_t *par)
{
    const int pr = GPROT[g];
    switch (en_piece_role(g, i)) {
    case RL_BASS:                               /* the bass line carries it: the pad and the pluck back, the line moving */
        if (!(pr & PR_PAD))
            pscl(par, P_PAD, 60);
        pscl(par, P_CM_LVL, 70);
        pscl(par, P_CM_DENS, 60);
        pscl(par, P_BASS, 112);
        if (!(pr & PR_BASS)) {
            par[P_BL_MODE] = 6;                 /* (Riff) */
            padd(par, P_BL_MOVE, 20);
        }
        break;
    case RL_CHORDS:                             /* the chords in front, comping; the line simple under them */
        pscl(par, P_PAD, 125);
        if (!par[P_PAD_COMP])
            par[P_PAD_COMP] = par[P_SWING] > 20 ? 6 : 2;   /* (Syncop where it swings, else Off-beat) */
        if (!(pr & PR_BASS))
            par[P_BL_MODE] = 1;                 /* (Root) */
        if (!(pr & PR_ACID))
            pscl(par, P_ACID, 70);
        pscl(par, P_CM_DENS, 70);
        padd(par, P_BRIGHT, 8);
        break;
    case RL_ACID:                               /* the acid leads, answering itself; the pluck up and out of its way */
        pset(par, P_ACID, par[P_ACID] * 135 / 100 < 50 ? 50 : par[P_ACID] * 135 / 100 > 68 ? 68 : par[P_ACID] * 135 / 100);   /* (50 .. 68: ELECTRO's went to 74, 2.6 dB over the rest) */
        padd(par, P_AC_DENS, 15);
        if (!par[P_AC_RESP])
            par[P_AC_RESP] = 7;                 /* (an octave, the same way) */
        if (par[P_CM_OCT] < 0)
            padd(par, P_CM_OCT, 1);             /* (a low pluck up to the middle, out of the acid's way) */
        pscl(par, P_CM_LVL, 80);
        if (!(pr & PR_PAD))
            pscl(par, P_PAD, 85);
        break;
    case RL_PLUCK:                              /* the pluck's riff is the hook: up, held (Deja vu), the pad darker under it */
        pscl(par, P_CM_LVL, 130);
        pset(par, P_CM_VU, par[P_CM_VU] > 85 ? par[P_CM_VU] : 85);
        pset(par, P_CM_MOTIF, 85);
        padd(par, P_BRIGHT, -12);
        if (!(pr & PR_ACID))
            pscl(par, P_ACID, 70);
        break;
    case RL_SPACE:                              /* dubbed out: more echo (dotted quarter) and room, fewer notes */
        padd(par, P_ECHO, 16);
        par[P_ETIME] = 3;
        padd(par, P_REVERB, 8);
        padd(par, P_SIZE, 10);
        pscl(par, P_CM_DENS, 70);
        pscl(par, P_AC_DENS, 80);
        if (!(pr & PR_LANES)) {
            pset(par, P_DENS3, 38);             /* (the hats and the perc thinned) */
            pset(par, P_DENS5, 38);
        }
        break;
    case RL_PEAK:                               /* peak time: driven, brighter, the pluck busier, the drums up (0.17.29: not faster: twice the DX notes ran the
                                                 * second core at 54-58 %, the pieces 80-85 % with late renders on the unit) */
        padd(par, P_DRIVE, 12);
        padd(par, P_CM_DENS, 10);
        padd(par, P_BRIGHT, 12);
        padd(par, P_AC_CUT, 8);
        padd(par, P_DR_CUT, 6);
        padd(par, P_PULSE, 5);
        if (!(pr & PR_LANES))
            pset(par, P_DENS3, 62);             /* (the hats busier) */
        break;
    case RL_DARK:                               /* dark: closed filters, a duller room (the registers stay: an octave down muddied it; more reverb widened the lows) */
        padd(par, P_BRIGHT, -14);
        padd(par, P_DR_CUT, -8);
        padd(par, P_AC_CUT, -10);
        padd(par, P_RV_DAMP, 15);
        pscl(par, P_CM_LVL, 85);
        break;
    case RL_SYNC:                               /* across the bar: the perc in 12 against it (0.17.29: 10 drifted oddly), the acid's loop shorter, the pluck skipping */
        if (!(pr & PR_LANES))
            par[P_LLEN5] = 12;
        par[P_AC_LEN] = (int16_t)(par[P_AC_LEN] >= 16 ? 12 : 10);
        par[P_CM_STYLE] = 4;                    /* (Skip) */
        break;
    case RL_LEFT:                               /* left field: the phrase sliced, leaps in the pluck, a stranger acid, the pad moving */
        if (!par[P_SLICE])
            par[P_SLICE] = 9;                   /* (Auto) */
        padd(par, P_CM_LEAP, 40);
        if (!(pr & PR_ACID))
            par[P_AC_WAVE] = (int16_t)(i & 1 ? 14 : 25);   /* (Fold, CZ) */
        padd(par, P_PL_DEPTH, 20);
        padd(par, P_DETUNE, 15);
        break;
    default:
        break;
    }
}
static void vary(int g, int i, int16_t *par)
{
    const int pw = g * 3 + i, h = i / 5;
    par[P_CM_SND] = PALETTE[g].pluck[(i * 4 + g) % 5];
    par[P_CM_STYLE] = (int16_t)((i * 2 + g) % EN_NCMSTYLE);
    par[P_CM_RATE] = PLUCK_RATE[g];
    if (g == 0)
        return;                                 /* the original pieces keep their pad, drone, acid and kit */
    par[P_PADTONE] = PALETTE[g].pad[(i + PAD_K[g] * h) % 5];
    par[P_DR_INST] = PALETTE[g].drone[(i * 2 + g + 2 * h) % 5];
    par[P_AC_WAVE] = PALETTE[g].acid[(i * 3 + pw + 2 * h) % 5];
    par[P_KIT_KICK] = PALETTE[g].kick[(i + g) % 3];
    par[P_KIT_SNARE] = PALETTE[g].snare[(i * 2 + g + i / 3) % 3];
    par[P_KIT_HAT] = PALETTE[g].hat[(i + 2 * g + i / 9) % 3];
    par[P_KIT_PERC] = PALETTE[g].perc[(i * 2 + 2 * g) % 3];
    par[P_PAN3] = (int16_t)(50 + ((i & 1) ? -14 : 14));    /* the pluck a little off the middle, the acid the other way */
    par[P_PAN2] = (int16_t)(50 + ((i & 1) ? 8 : -8));
    role(g, i, par);
}
void en_piece_g(int g, int i, int16_t *par, uint16_t *pool)
{
    int p;
    const int16_t *c;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    g = (unsigned)g < EN_NGENRE ? g : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = PARAMS[p].def;
    if (g >= 4) {                               /* the other six: the genre's base, then the piece's row */
        const uint8_t *r = XROW[g - 4][i];
        for (c = XBASE[g - 4]; c[0] >= 0; c += 2)
            par[c[0]] = c[1];
        par[P_TEMPO] = r[0];
        par[P_ROOT] = r[1];
        par[P_MODE] = r[2];
        par[P_CHORD] = r[3];
        par[P_PADTONE] = r[4];
        par[P_DR_INST] = r[5];
        par[P_AC_WAVE] = r[6];
    } else {
        for (c = GENRE_PIECES[g][i]; c[0] >= 0; c += 2)
            par[c[0]] = c[1];
    }
    vary(g, i, par);
    if (psy_slow(g, i)) {                       /* psy-ambient, psychill: the bass held, the pluck in eighths, the acid thin, a wide shimmer, the pad up */
        par[P_BL_MODE] = 0;
        par[P_CM_RATE] = 2;
        par[P_AC_DENS] = 35;
        par[P_ACID] = 40;
        par[P_PAD] = 60;
        par[P_REVERB] = 60;
        par[P_SIZE] = 85;
        par[P_ECHO] = 50;
        par[P_DRIVE] = 15;
        par[P_PULSE] = 70;
        par[P_SWING] = 10;
        par[P_EVOLVE] = 35;
        par[P_BASS] = 68;                       /* (0.16.6: held, at 85 it was as loud as the whole mix) */
    }
    *pool = EN_MODE_POOL[par[P_MODE]];
}
void en_piece(int i, int16_t *par, uint16_t *pool) { en_piece_g(0, i, par, pool); }
void en_piece_pats_g(int g, int i, uint16_t pats[EN_NPAT][EN_NLANE])
{
    int p, l;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    if (g >= 1) {
        if (g >= 4)
            x_pats(g - 4, i, pats);
        else if (g == 1)
            lofi_pats(i, pats);
        else if (g == 2)
            house_pats(i, pats);
        else
            dub_pats(i, pats);
        return;
    }
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            pats[p][l] = EN_PIECE_PATS[i][p][l];
}
void en_piece_pats(int i, uint16_t pats[EN_NPAT][EN_NLANE]) { en_piece_pats_g(0, i, pats); }
/* A genre's factory step tricks (en_mod; the same for each of its pieces): the feel a genre has that a grid of hits cannot say. Byte: ratchet | push << 3 | level << 6.
 * Lofi: the snare and clap laid back, the off-beat hats soft. House and garage: the off-beat hats soft (garage: the snare a touch late). Trap: triplets and a swell
 * on the hat's last beat. The other genres are as the pattern says. A trick on a step with no hit does nothing. */
void en_piece_mods_g(int g, int i, uint8_t mod[EN_NLANE][16])
{
    int s, l;
    for (l = 0; l < EN_NLANE; l++)
        for (s = 0; s < 16; s++)
            mod[l][s] = 0;
    for (s = 0; s < 16; s++) {
        if (g == 1) {
            mod[1][s] = (uint8_t)(2u << 3);
            mod[2][s] = (uint8_t)(2u << 3);
        }
        if (g == 7)
            mod[1][s] = (uint8_t)(1u << 3);
        if ((g == 1 || g == 2 || g == 7 || (g >= 4 && g <= 6) || g == 9) && (s & 1))
            mod[3][s] = (uint8_t)(2u << 6);                 /* the off-beat hats soft */
        if ((g == 2 || g == 8) && (s & 3) == 2)
            mod[3][s] = (uint8_t)(3u << 6);                 /* house and electro: the hats' off-beat accented */
        if (g >= 5 && g <= 6 && (s == 4 || s == 12))
            mod[1][s] = (uint8_t)(3u << 6);                 /* DnB, breaks: the backbeat accented */
        if (g == 4)
            mod[1][s] = (uint8_t)(1u << 3);                 /* dub: the rim a touch late */
        if (g == 8 && s == 0)
            mod[0][s] = (uint8_t)(3u << 6);
        if (g == 10 && (s & 3) == 2)
            mod[3][s] = (uint8_t)(3u << 6);                 /* disco: the open off-beat hats accented */
        if (((g >= 11 && g <= 15 && g != 10) || g == 17) && (s & 1))
            mod[3][s] = (uint8_t)(2u << 6);                 /* funk, reggae, synthwave, amapiano, trip-hop: the off-beat hats soft */
        if ((g == 11 || g == 13) && (s == 4 || s == 12))
            mod[1][s] = (uint8_t)(3u << 6);                 /* funk and synthwave: the backbeat accented */
        if (g == 12)
            mod[1][s] = (uint8_t)(1u << 3);                 /* reggae: the rim a touch late */
        if (g == 15)
            mod[1][s] = (uint8_t)(2u << 3);                 /* trip-hop: the snare laid back */
        if (g == 14)
            mod[5][s] = (uint8_t)(2u << 6);                 /* amapiano: the shakers soft */
        if (g == 18 && (s & 3) == 2)
            mod[5][s] = (uint8_t)(3u << 6);                 /* dark forest: the perc accented off the beat */
        if (g == 19) {                                      /* (0.17.35) hiphop: the snare a touch late, the off-beat hats soft */
            mod[1][s] = (uint8_t)(1u << 3);
            if (s & 1)
                mod[3][s] = (uint8_t)(2u << 6);
        }
        if (g == 20 && (s & 3) == 2)
            mod[4][s] = (uint8_t)(3u << 6);                 /* trance: the open hat off the beat accented */
    }
    if (g == 18) {                                      /* dark forest: the perc's last 16th of beats 2 and 4 in triplets, the backbeat on 4 a flam */
        mod[5][7] = 2u;
        mod[5][15] = 2u;
        mod[1][12] = 5u;
    }
    if (g == 16) {                                      /* industrial: the snare's last 16th in triplets, the perc doubled before it */
        mod[1][15] = 2u;
        mod[5][13] = 1u;
    }
    if (g == 9 || g == 16 || g == 18) {                 /* trap's (and industrial's, dark forest's) hat rolls (0.15.13: two a piece, on steps its hats play; x3 on step 8 never sounded) */
        static const uint8_t ROLL[5][2] = {{14, 3}, {6, 2}, {10, 1}, {12, 4}, {14, 7}};   /* step, ratchet: x4, x3, x2, x6, swell 4 */
        const int a = i % 5, b = (i + 2) % 5;
        mod[3][ROLL[a][0]] = (uint8_t)(mod[3][ROLL[a][0]] & ~7u) | ROLL[a][1];
        mod[3][ROLL[b][0]] = (uint8_t)(mod[3][ROLL[b][0]] & ~7u) | ROLL[b][1];
    }
}

static const char *const ETIME_NAME[6] = {"1/8", "3/16", "1/4", "1/4.", "1/2", "1 bar"};
static const float ETIME_BEATS[6] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};
/* the LFOs' rates (0.15.13: 1/8, 1/8T, 1/16, 1/16T appended for the dubstep wobble; the stored values never renumbered) */
#define NLFO_RATE 16
static const char *const LFO_RATE_NAME[NLFO_RATE] = {"1/4", "1/2", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars", "1/8", "1/8T", "1/16", "1/16T",
                                                     "1/4T", "1/8.", "1/4.", "1/2."};   /* (12-15: 0.17.10, after the others so a stored rate keeps its meaning) */
static const float LFO_RATE_BEATS[NLFO_RATE] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f,
                                                2.0f / 3.0f, 0.75f, 1.5f, 3.0f};
static const uint16_t LFO_P3[NLFO_RATE] = {12, 24, 48, 96, 192, 384, 768, 1536, 6, 4, 3, 2, 8, 9, 18, 36};   /* a cycle in thirds of a sixteenth (beats x 12) */
static const char *const LFO_SHAPE_NAME[6] = {"Sine", "Triangle", "Saw", "Square", "S&H", "Drift"};
static const char *const PAD_DEST_NAME[8] = {"Filter", "Level", "Pan", "Pitch", "Acid cut", "Acid reso", "Echo time", "Punch cut"};   /* (4-7: 0.16.4, the LFO on other voices and effects) */
static const char *const DRONE_DEST_NAME[8] = {"Cutoff", "Pitch", "Level", "Drive", "Reso", "Acid cut", "Acid reso", "Echo time"};
static const char *const NOISE_NAME[8] = {"Mint", "Clean", "Light", "Used", "Worn", "Scratched", "Dusty", "Trashed"};   /* the vinyl's wear */
static const char *const TRIG_NAME[6] = {"Chord", "1 bar", "2 bars", "4 bars", "8 bars", "Off"};
static const char *const THEME_NAME[EN_NTHEME] = {"Console", "Imp", "Synth", "Acid", "Ice", "Ember", "Phosphor"};
/* the voice bank: what each voice can play (the sounds themselves: PAD_SOUND, DRONE_SOUND, ACID_SOUND, and
 * the kits: the old styles' drums) */
const char *const EN_PAD_SOUND[EN_NSOUND] = {"Rhodes", "Voices", "Strings", "Glass", "Organ", "Warm", "Stabs", "Dark",
                                             "Dub", "Trance", "Poly", "Hollow", "Pings", "Hits", "Rave", "Sine",
                                             "Ooh", "Wurli", "Mallet", "Brass", "Fat", "Pluck", "Chapel", "Air"};
const char *const EN_DRONE_SOUND[EN_NSOUND] = {"Saws", "Warm", "Grit", "Dirty", "Reese", "Pulse", "FM", "Organ",
                                               "Rumble", "Sub", "Square", "Hoover", "Fifths", "Metal", "Sync", "Bright",
                                               "Wub", "Bite", "Solid", "Growl", "Gnarl", "Wide", "Power", "Air"};
const char *const EN_ACID_SOUND[EN_NACID] = {"Saw", "Square", "Driven", "Sync", "Pluck", "PWM", "Chip", "Sub",
                                              "Trance", "Bleep", "Fifth", "Bell", "Organ", "Laser", "Fold", "Soft",
                                              "Marimba", "E.Piano", "Kalimba", "Brass", "Gong", "Clav", "Vibes", "Digital",
                                              "Super", "CZ", "Mini", "Hyper", "CZ Pulse", "CZ Bell", "Lead"};
static char ukit_name[EN_NUKIT][13] = {"Empty 1", "Empty 2", "Empty 3"};   /* (0.17.38) the loaded kits' names */
const char *const EN_KIT_NAME[EN_NKITS] = {"Hypno", "House", "Techno", "Acid", "Break", "Minimal", "Hard", "808", "Dusty", "Heavy",
                                                   "909", "Electro", "DnB", "Trap", "Boom", "Ride", "Snap", "606", "Gabber", "Linn", "Shaker",
                                                   "Machine", "Boombap", ukit_name[0], ukit_name[1], ukit_name[2]};   /* (0.17.36: the sampled ones last; 0.17.38:
                                                 * the loaded ones by the names they came with) */

/* the envelopes' times, from their knobs (0..100), in seconds */
static float pe_atk_s(int v) { return 0.05f * fm_exp2f((float)v * 0.0764f); }        /* 0.05 .. 10 s */
static float pe_hold_s(int v) { return 8.0f * (float)(v * v) * 0.0001f; }              /* 0 .. 8 s */
static float pe_rel_s(int v) { return 0.1f * fm_exp2f((float)v * 0.0691f); }          /* 0.1 .. 12 s */
static float pe_rest_s(int v) { return 6.0f * (float)(v * v) * 0.0001f; }             /* 0 .. 6 s */
static float de_atk_s(int v) { return 0.01f * fm_exp2f((float)v * 0.0864f); }         /* 0.01 .. 4 s */
static float de_dec_s(int v) { return 0.05f * fm_exp2f((float)v * 0.0832f); }         /* 0.05 .. 16 s */

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static void secs_text(float s, char *b)          /* "0.25s", "2.4s", "12s" */
{
    if (s < 1.0f) {
        int ms = (int)(s * 100.0f + 0.5f);
        *b++ = '0';
        *b++ = '.';
        *b++ = (char)('0' + ms / 10 % 10);
        *b++ = (char)('0' + ms % 10);
    } else if (s < 10.0f) {
        int d = (int)(s * 10.0f + 0.5f);
        *b++ = (char)('0' + d / 10);
        *b++ = '.';
        *b++ = (char)('0' + d % 10);
    } else {
        b = itoa_s((int)(s + 0.5f), b);
    }
    *b++ = 's';
    *b = 0;
}

static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */
static int hr_bars_v(int v);                    /* the chord's bars in a genre with a harmonic rhythm (0: by Drift's clock) */

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    switch (i) {
    case P_ETIME: s = ETIME_NAME[(unsigned)v < 6u ? v : 0]; break;
    case P_MIDI: s = ONOFF[v ? 1 : 0]; break;
    case P_PADTONE: s = EN_PAD_SOUND[(unsigned)v < EN_NSOUND ? v : 0]; break;
    case P_DR_INST: s = EN_DRONE_SOUND[(unsigned)v < EN_NSOUND ? v : 0]; break;
    case P_KIT_KICK: case P_KIT_SNARE: case P_KIT_HAT: case P_KIT_PERC: s = EN_KIT_NAME[(unsigned)v < EN_NKITS ? v : 0]; break;
    case P_MODE: s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0]; break;
    case P_ROOT: s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0]; break;
    case P_PL_RATE: case P_DL_RATE: s = LFO_RATE_NAME[(unsigned)v < NLFO_RATE ? v : 0]; break;
    case P_PL_SHAPE: case P_DL_SHAPE: s = LFO_SHAPE_NAME[(unsigned)v < 6u ? v : 0]; break;
    case P_PL_DEST: s = PAD_DEST_NAME[v & 7]; break;
    case P_DL_DEST: s = DRONE_DEST_NAME[v & 7]; break;
    case P_NZ_COLOR: s = NOISE_NAME[v & 7]; break;
    case P_DE_TRIG: s = TRIG_NAME[(unsigned)v < 6u ? v : 0]; break;
    case P_AC_WAVE: s = EN_ACID_SOUND[(unsigned)v < EN_NACID ? v : 0]; break;
    case P_CM_SND: s = EN_CM_SOUND[(unsigned)v < EN_NCMSND ? v : 0]; break;
    case P_CM_STYLE: s = EN_CM_STYLE[(unsigned)v < EN_NCMSTYLE ? v : 0]; break;
    case P_CM_RATE: s = EN_CM_RATE[v & 3]; break;
    case P_LSRC: case P_LSRC1: case P_LSRC2: case P_LSRC3: case P_LSRC4: case P_LSRC5: { static const char *const S[7] = {"Follow", "Pat 1", "Pat 2", "Pat 3", "Pat 4", "Pat 5", "Pat 6"}; s = S[(unsigned)v < 7u ? v : 0]; break; }
    case P_STYLE: s = v ? "Loop" : "Bar"; break;
    case P_AC_EVERY: { static const char *const E[8] = {"2nd", "3rd", "4th", "5th", "6th", "7th", "8th", "Drift"}; s = E[(unsigned)v < 8u ? v : 0]; break; }
    case P_AC_RESP: {                            /* type and direction: "Oct +", "Mirr =", "Rev -2" */
        static const char *const T[9] = {"Off", "Oct", "5th", "3rd", "Mirr", "Rev", "Rot", "Spar", "Acc"}, *const D[5] = {"-2", "-", "=", "+", "+2"};
        const char *q = T[(unsigned)v < 45u ? v / 5 : 0];
        char *o = b;
        while (*q)
            *o++ = *q++;
        if (v >= 5) {
            *o++ = ' ';
            for (q = D[v % 5]; *q; q++)
                *o++ = *q;
        }
        *o = 0;
        return;
    }
    case P_CM_LOOP: { static const char *const L[4] = {"4", "8", "16", "32"}; s = L[(unsigned)v < 4u ? v : 0]; break; }
    case P_AC_CHG: { static const char *const C[4] = {"All", "Notes", "Rhythm", "Accents"}; s = C[(unsigned)v < 4u ? v : 0]; break; }
    case P_PAD_COMP: { static const char *const C[9] = {"Auto", "Hold", "Off-beat", "On-beat", "3-3-2", "Charleston", "Syncop", "Sparse", "Eighths"}; s = C[(unsigned)v < 9u ? v : 0]; break; }
    case P_CHAIN: { static const char *const C[10] = {"Off", "12", "1112", "1123", "1134", "1213", "1234", "1232", "1115", "123456"}; s = C[(unsigned)v < 10u ? v : 0]; break; }
    case P_SLICE: { static const char *const S[10] = {"Off", "Halves", "Stutter", "Reverse", "Delay", "Beats", "Mirror", "Double", "Swap", "Auto"}; s = S[(unsigned)v < 10u ? v : 0]; break; }
    case P_TIME: { static const char *const T[3] = {"Normal", "Half", "Double"}; s = T[(unsigned)v < 3u ? v : 0]; break; }
    case P_COND: case P_COND1: case P_COND2: case P_COND3: case P_COND4: case P_COND5: { static const char *const C[8] = {"Always", "1:2", "2:2", "1:4", "4:4", "1:8", "Fill", "No fill"}; s = C[(unsigned)v < 8u ? v : 0]; break; }
    case P_AC_LOCK: s = v ? "Frozen" : "Free"; break;
    case P_CM_LOCK: s = v ? "Frozen" : "Free"; break;
    case P_BL_MODE: { static const char *const M[7] = {"Hold", "Root", "Kick", "Eighths", "Off-beat", "Walk", "Riff"}; s = (unsigned)v < 7u ? M[v] : v == EN_BL_STEPS ? "Steps" : (unsigned)BL_FIG_OF(v) < (unsigned)EN_NBLFIG ? BL_FIG_NAME[BL_FIG_OF(v)] : M[0]; break; }
    case P_CH_LOOP: { static const char *const C[7] = {"Off", "2", "4", "8", "2 ~", "4 ~", "8 ~"}; s = C[(unsigned)v < 7u ? v : 0]; break; }
    case P_THEME: s = THEME_NAME[(unsigned)v < EN_NTHEME ? v : 0]; break;
    case P_LINKS: s = ONOFF[v ? 1 : 0]; break;
    case P_LK_PERC: s = v ? "Clock" : "Off"; break;
    case P_CHORD: s = EN_CHORD_NAME[(unsigned)v < EN_NCHORD ? v : 0]; break;
    case P_VOICING: s = v ? "Open" : "Close"; break;
    case P_DD_CHANGE: { static const char *const C[3] = {"Off", "Next", "Random"}; s = C[(unsigned)v < 3u ? v : 0]; break; }
    case P_DD_EVERY: { static const char *const E[6] = {"Chord", "4 bars", "8 bars", "16 bars", "32 bars", "64 bars"}; s = E[(unsigned)v < 6u ? v : 0]; break; }
    case P_RV_TYPE: { static const char *const T[3] = {"Plate", "Shimmer", "Gated"}; s = T[(unsigned)v < 3u ? v : 0]; break; }
    case P_RV_PRE: {                              /* 6 .. 30 ms */
        char *p = itoa_s(6 + 24 * v / 100, b);
        *p++ = 'm';
        *p++ = 's';
        *p = 0;
        return;
    }
    case P_PAN: case P_PAN1: case P_PAN2: case P_PAN3: case P_PAN4: {    /* L100 .. C .. R100 */
        char *p = b;
        if (v == 50) {
            b[0] = 'C';
            b[1] = 0;
            return;
        }
        *p++ = v < 50 ? 'L' : 'R';
        p = itoa_s((v < 50 ? 50 - v : v - 50) * 2, p);
        *p = 0;
        return;
    }
    case P_MC_RATIO: { static const char *const R[7] = {"Off", "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1"}; s = R[(unsigned)v < 7u ? v : 0]; break; }
    case P_MC_THR: case P_MC_MAKE: case P_ML_CEIL: {   /* in dB: -30 .. 0, +0 .. 12 (a decimal), -12 .. 0 */
        const int t10 = i == P_MC_THR ? -300 + 3 * v : i == P_MC_MAKE ? 12 * v / 10 : -120 + 12 * v / 10;
        const int neg = t10 < 0, a = neg ? -t10 : t10;
        char *p = b;
        *p++ = neg ? '-' : i == P_MC_MAKE ? '+' : ' ';
        p = itoa_s(a / 10, p);
        if (i != P_MC_THR) {
            *p++ = '.';
            *p++ = (char)('0' + a % 10);
        }
        *p++ = 'd';
        *p++ = 'B';
        *p = 0;
        return;
    }
    case P_DD_MUTATE: { static const char *const M[3] = {"Off", "Shift", "Morph"}; s = M[(unsigned)v < 3u ? v : 0]; break; }
    default: break;
    }
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_DRIFT) {
        int sec = (int)(drift_secs(v) + 0.5f);
        const int hb = hr_bars_v(v);
        if (hb) {                               /* a genre with a harmonic rhythm: bars a chord */
            const char *q = hb == 1 ? " bar" : " bars";
            b = itoa_s(hb, b);
            while (*q)
                *b++ = *q++;
            *b = 0;
            return;
        }
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    switch (i) {
    case P_PE_ATK: secs_text(pe_atk_s(v), b); return;
    case P_PE_HOLD: secs_text(pe_hold_s(v), b); return;
    case P_PE_REL: secs_text(pe_rel_s(v), b); return;
    case P_PE_REST: secs_text(pe_rest_s(v), b); return;
    case P_DE_ATK: secs_text(de_atk_s(v), b); return;
    case P_DE_DEC: secs_text(de_dec_s(v), b); return;
    case P_PATTERN: itoa_s(v + 1, b); return;
    case P_DD_POOL: *b++ = '1'; *b++ = '-'; itoa_s(v, b); return;
    case P_AC_RANGE: b = itoa_s(v, b); *b++ = ' '; *b++ = 'o'; *b++ = 'c'; *b++ = 't'; *b = 0; return;
    case P_SWING: b = itoa_s(50 + v / 4, b); *b++ = '%'; *b = 0; return;
    default: break;
    }
    if ((i == P_TUNE || i == P_AC_OCT || i == P_BASSOCT || i == P_OCT || i == P_PAD_OCT || (i >= P_LANE && i < P_ECHO && (i - P_LANE) % 3 == 1)) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

const char *const EN_CHORD_NAME[EN_NCHORD] = {"Auto", "Triads", "7ths", "9ths", "Sus", "Dub", "Fourths", "11ths"};

int en_chord_kind(uint32_t pool, int root, int deg, int kind, int *pcs)
{
    /* the scale steps above the degree to take, in order (then what's left, should they repeat) */
    static const uint8_t OFF[5][7] = {
        {0, 2, 4, 6, 1, 3, 5},                  /* thirds: 1 3 5 7 */
        {0, 2, 6, 1, 4, 3, 5},                  /* ninths: 1 3 7 9 (the fifth left out) */
        {0, 3, 4, 6, 1, 2, 5},                  /* sus: 1 4 5 b7 */
        {0, 6, 1, 3, 2, 4, 5},                  /* elevenths: 1 b7 9 11 (the third and fifth left out, as it is usually voiced) */
        {0, 1, 4, 6, 2, 3, 5},                  /* sus2: 1 2 5 7 (Sus where the fourth is sharp, as on Ionian's IV: 0.16.9) */
    };
    static const uint8_t SHAPE[4] = {0, 3, 10, 14};   /* dub: m9 (no fifth) */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    const uint8_t *off = OFF[0];
    deg = ((deg % m) + m) % m;
    if (kind == EN_CH_DUB) {                    /* a fixed shape on the degree's note: dub techno's parallel stabs, whatever the scale says */
        for (i = 0; i < 4; i++)
            pcs[i] = (root + st[deg] + SHAPE[i]) % 12;
        return 4;
    }
    if (kind == EN_CH_4THS) {                   /* the scale's own fourths stacked (0.17.16; a fixed 0 5 10 15 before, a quarter of it out of the scale):
                                                   three steps of a 7-note scale, two of a pentatonic; where that is a tritone, a step less (the So What voicing) */
        const int k = (m * 5 + 6) / 12;
        int d = deg;
        pcs[0] = (root + st[deg]) % 12;
        for (i = 1; i < 4; i++) {
            int e = d + k;
            if (k > 1 && (st[e % m] - st[d % m] + 12) % 12 == 6)
                e--;
            pcs[i] = (root + st[e % m]) % 12;
            d = e;
        }
        return 4;
    }
    want = kind == EN_CH_TRIAD ? 3 : kind == EN_CH_AUTO ? (m >= 6 ? 4 : 3) : 4;
    if (kind == EN_CH_9TH)
        off = OFF[1];
    else if (kind == EN_CH_SUS)
        off = m == 7 && (st[(deg + 3) % m] - st[deg] + 12) % 12 == 6 ? OFF[4] : OFF[2];
    else if (kind == EN_CH_11TH)
        off = OFF[3];
    for (i = 0; i < 7 && n < want; i++) {
        int pc = (root + st[(deg + off[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs) { return en_chord_kind(pool, root, deg, EN_CH_AUTO, pcs); }

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    if (n == 4 && iv == (1u << 5 | 1u << 10 | 1u << 3)) {   /* stacked fourths (the Fourths type): named as such, not "m11" (0.16.9) */
        q = "4ths";
        goto out;
    }
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? (iv >> 2 & 1u ? "M9" : "M7") : iv >> 10 & 1u ? (iv >> 2 & 1u ? "9" : "7")
            : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else if (iv >> 10 & 1u)
            q = iv >> 5 & 1u ? "m11" : iv >> 2 & 1u ? "m9" : "m7";
        else
            q = iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        /* no third: sus4 or sus2, with its seventh (0.16.9: the major seventh too; 1 b7 9 11 is the 11ths' "A11", 1 7 9 11 their "CM11") */
        const int sev = iv >> 10 & 1u ? 1 : iv >> 11 & 1u ? 2 : 0;
        if (iv >> 5 & 1u)
            q = iv >> 2 & 1u && sev ? (sev == 1 ? "11" : "M11") : sev == 1 ? "7sus4" : sev == 2 ? "M7sus4" : "sus4";
        else if (iv >> 2 & 1u)
            q = sev == 1 ? "7sus2" : sev == 2 ? "M7sus2" : "sus2";
        else
            q = "5";
    }
out:
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_SECTION, C_FX, C_MUTE, C_NOTE, C_HUSH, C_FXK, C_TRK, C_GATEPAT, C_FXS, C_FILLQ, C_DJSYNC, C_FLXH, C_SHED, C_USERVOICE, C_USERCLEAR, C_ACSTEP, C_ACCAP, C_CMCAP, C_INTRO, C_CMSTEP, C_PROG, C_PROG2, C_PROGK, C_MAP, C_DTAP, C_DEG, C_BEND, C_PEDAL, C_BSSTEP, C_NEWPART, C_BSCAP, C_ACKEEP, C_EXTCH, C_KITS, C_PHRASE, C_LINK };
static uint8_t muted;                            /* remote: no sound, no MIDI (C_MUTE, after what's queued) */
static uint8_t hush;                             /* a store while playing: fade out, hold, fade back in (C_HUSH) */
static float hush_g = 1.0f;                      /* its gain, 0..1 */
volatile uint8_t en_hushed;                      /* faded right out: the flash can be written */
#define QN 1024u                                 /* about seven times every parameter at once (project_apply, a piece, a genre): a quick flick of PRESETS filled 256 and dropped commands */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
volatile uint32_t en_q_drops;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN) {
        en_q_drops++;                             /* full: dropped (never seen at the UI's rate; counted since 0.17.48, the audit) */
        return;
    }
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_seed_exact(uint32_t s) { post(C_SEED, 1, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_mute(int on) { post(C_MUTE, on, 0); }
void en_hush(int on) { post(C_HUSH, on, 0); }
void en_note_v(int voice, int note, int vel) { post(C_NOTE, voice, (note & 0x7F) | (vel < 0 ? 0 : vel > 127 ? 127 : vel) << 8); }   /* (the velocity in b's bits 8-14) */
void en_note(int voice, int note, int on) { en_note_v(voice, note, on ? 100 : 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_new_part(int p) { post(C_NEWPART, p, 0); }
void en_section(int cmd) { post(C_SECTION, cmd, 0); }
void en_build_bars(int bars) { post(C_SECTION, EN_CMD_BUILD, bars); }
void en_drop(void) { post(C_SECTION, EN_CMD_DROP, 1); }
void en_fx(uint64_t mask)                       /* in four quarters: the queue carries 16 bits */
{
    int q;
    for (q = 0; q < 4; q++)
        post(C_FX, q, (int)(mask >> (16 * q) & 0xFFFFu));
}
void en_fxk(int k, int v) { post(C_FXK, k, v); }
void en_trk(uint32_t mute, uint32_t solo) { post(C_TRK, 0, (int)((mute & 0xFFu) | (solo & 0xFFu) << 8)); }
void en_pedal(int bits) { post(C_PEDAL, bits & 3, 0); }
void en_gate_pat(uint32_t steps) { post(C_GATEPAT, 0, (int)(steps & 0xFFFFu)); }
void en_fxs(int i, int v) { post(C_FXS, i, v); }
void en_fill_next(void) { post(C_FILLQ, 0, 0); }
void en_flx_hold(int on) { post(C_FLXH, on ? 1 : 0, 0); }
volatile uint8_t en_shed_level;
static uint8_t shed3;                          /* shed level 3 (the chunk's copy): the first core gives up the Echoplex, the vinyl noise and the exact output clip */
void en_shed(int level) { post(C_SHED, level < 0 ? 0 : level > 3 ? 3 : level, 0); }
void en_dj_sync(int on) { post(C_DJSYNC, on ? 1 : 0, 0); }
void en_drum_tap(int d, int vel) { post(C_DTAP, d, vel < 1 ? 1 : vel > 127 ? 127 : vel); }
void en_chord_deg(int d) { post(C_DEG, d < 0 ? 0 : d, 0); }
void en_bend(int voice, int bend) { post(C_BEND, voice & 0x7F, bend < -8192 ? -8192 : bend > 8191 ? 8191 : bend); }
void en_modw(int voice, int depth) { post(C_BEND, (voice & 0x7F) | 0x80, depth < 0 ? 0 : depth > 127 ? 127 : depth); }
void en_midi_map(int m) { post(C_MAP, (unsigned)m < (unsigned)EN_NMAP ? m : 0, 0); }
void en_intro(int secs) { post(C_INTRO, secs < 0 ? 0 : secs > 1800 ? 1800 : secs, 0); }
volatile float en_intro_k = 1.0f;
/* THE PIECES' LEVELS (0.16.6; 0.17.21 both ways): each piece's whole mix measured on the host (tests/host/gsound.c, 32 bars as the
 * unit starts, integrated loudness, the perf-sound-check skill), then brought to the median of all (-15.3 LUFS) at the output, in half
 * dB: a boost at most 4 dB and never past a -1 dBFS peak, a cut at most 6. The genres' medians had spread from -12.6 (PSY, FOREST) to
 * -17.8 (DUB, TRIPHOP), so the auto-DJ's genre changes jumped 5 dB; now every genre's median is within 0.2 dB, every piece within
 * -17.6 .. -15.0 */
static const int8_t EN_TRIM[EN_NGENRE][10] = {   /* half dB, + louder */
    {  4,   5,  -1,   5,   2,   6,   3,   2,  -2,  -2},   /* HYPNO */
    {  1,   6,   3,   4,   3,   3,   2,   6,   0,   2},   /* LOFI */
    { -2,  -3,  -1,  -3,  -3,  -3,  -3,  -1,  -2,  -2},   /* HOUSE */
    {  0,  -1,  -8,  -4,  -1,  -7,  -7,  -2,  -5,  -6},   /* DUBSTEP */
    {  4,   5,   5,   4,   5,   6,   4,   3,   3,   2},   /* DUB */
    {  5,   3,   2,   3,   5,   4,   3,   0,   2,   3},   /* DNB */
    { -3,  -2,  -2,  -3,  -5,   4,  -4,  -6,   0,  -3},   /* BREAKS */
    {  7,   3,   0,   5,   2,   1,   4,   3,  -1,   5},   /* GARAGE */
    { -3,  -1,   0,   0,  -4,   2,   0,  -2,  -1,  -3},   /* ELECTRO */
    { -4,   1,  -2,  -5,   0,  -1,  -2,  -2,   1,  -4},   /* TRAP */
    { -1,   0,  -2,  -4,  -3,  -1,  -2,  -4,  -2,  -3},   /* DISCO */
    {  2,  -1,   2,   4,  -1,   1,   2,   1,   0,   3},   /* FUNK */
    {  3,   2,   4,   4,   0,   3,   6,   2,   0,   0},   /* REGGAE */
    { -2,   3,  -1,  -1,   1,   0,  -5,   4,  -2,  -3},   /* SYNTHWV */
    { -1,   3,   5,  -2,   4,   3,   5,  -3,   1,   0},   /* AMAPI */
    {  4,   6,   4,   6,   4,   8,   4,   8,   5,   2},   /* TRIPHOP */
    {  1,  -6,  -6,  -2,  -6,  -4,  -2,  -7,  -6,   0},   /* INDUS */
    {  1,  -4,  -7,  -3, -10,  -3,  -5,  -4,  -8,  -5},   /* PSY (0.17.43: re-levelled after its short kick and bass) */
    { -1,  -2,  -3,  -6,  -4,  -2,  -4,  -2,  -1,  -3},   /* FOREST (0.17.43: the same) */
    {  1,   2,  -2,   0,   0,   1,   1,  -3,  -1,  -1},   /* HIPHOP */
    { -1,  -2,  -5,  -3,  -5,  -1,  -4,  -5,  -5,   0},   /* TRANCE */
    {  1,   8,   0,   4,   4,   2,   2,   8,   8,   0},   /* AMBIENT */
};
static float trim_to = 1.0f, trim_k = 1.0f;
void en_trim(int g, int piece)
{
    const int h = (unsigned)g < (unsigned)EN_NGENRE && (unsigned)piece < 10u ? EN_TRIM[g][piece] : 0;
    trim_to = fm_exp2f(0.0830482f * (float)h);    /* (half dB: 2^(h / 2 / 20 * log2 10)) */
}
static int32_t intro_len, intro_t;              /* the intro's length and how far it is, samples (0 length: none) */
volatile float en_drift_bars = 99.0f;

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level;
volatile float en_acid_level;
volatile uint8_t en_acid_len = 16, en_acid_pos;
volatile uint16_t en_acid_gate, en_acid_acc;
volatile float en_acid_glow;
volatile uint8_t en_held;
volatile uint8_t en_drum_pos;
volatile uint8_t en_lane_pos[EN_NLANE];         /* and each lane's own (it may be shorter than the bar) */
volatile float en_drum_glow[EN_NDRUM];
volatile uint8_t en_pat_now, en_pat_next;
volatile uint32_t en_bars;
volatile float en_step_frac;
volatile uint8_t en_deg;                         /* (0.17.31) the chord's scale degree now (Capture keeps it with each note played) */
static uint8_t ac_barsync;                       /* (0.17.31) a line just captured: on the next step the line's step is the bar's */
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static int16_t par[P_NPARAMS];
int en_par_now(int p) { return (unsigned)p < (unsigned)P_NPARAMS ? par[p] : 0; }
static int links_on(void) { return par[P_LINKS] != 0; }
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale (as it sounds: the dominant's raised seventh, a secondary dominant's or a borrowed chord's notes) */
static int steps_nat[12];                       /* the scale as the piece has it (0.16.27) */
static int8_t walk_next = -1;                   /* the walk's next degree, chosen a chord ahead (0.16.27; -1 none yet) */
static uint8_t walk_bor;                        /* and whether it is borrowed */
volatile int8_t en_walk_next = -1;
volatile uint8_t en_walk_next_root, en_chord_alt;
static int deg;                                 /* the chord's scale degree */
static uint8_t dom_on;
static uint8_t dh[8], dh_n, dh_i, dh_ln;        /* the progression loop: its chords, how many, the next, its length */
static uint8_t pg_kind[EN_PRG_N];               /* each slot's chord type (en_prog_kinds: 0 the piece's) */
static uint8_t pg_deg[EN_PRG_N], pg_bars[EN_PRG_N], pg_len, pg_i, pg_on, drift_unit;   /* the progression set by hand (en_prog): each slot's chord (0 empty, 1..7 a degree) and
                                                * bars (1 << it), the slots in use, the one playing, Play; Drift's unit (0 the genre's, 1 bars, 2 seconds) */
static uint8_t hist[8], hist_n;                 /* the last chords, the newest last (for the progression's capture) */
static uint32_t pg_lo;                          /* the slots' chords, until their bars come (C_PROG, C_PROG2) */
static uint8_t pg_start;                        /* the next chord is the first slot's (Play just turned to Loop) */
volatile int8_t en_prog_pos = -1;                          /* a dominant in minor: the scale's seventh degree raised a semitone while the chord is V (dom_apply) */
static int chord_pcs[4], chord_nt;
/* THE CHORD FROM OUTSIDE (0.17.31, MIDI in's CHORDS, an arranger's left hand): its tones (root first, up to four), its bass (12: the root), on
 * (1). The walk and the progression hold while it is on; the scale is bent to it (each tone outside it takes the place of the nearest one that
 * is not a tone), so the acid, the bass and the pluck stay in key with it; the pad plays its tones as they are. */
static uint8_t ext_on, ext_n, ext_bass = 12;
static int8_t ext_pc[4];
volatile uint8_t en_extch;
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
/* the pad's own stream: its render may run on the other core, and the shared one would then be taken in whatever order the cores got there */
static uint32_t prng = 0x2545F491u;
static float pfrand(void)
{
    prng ^= prng << 13;
    prng ^= prng >> 17;
    prng ^= prng << 5;
    return (float)(prng >> 8) * (1.0f / 16777216.0f);
}
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }
#ifdef EN_HOST
void en_test_reseed(uint32_t s)                 /* the host tests: a block's random walk its own, not what the blocks before it used up */
{
    rng = s ? s : 0x9E3779B9u;
    prng = 0x2545F491u ^ s;
}
#endif

/* the MIDI out maps (en_midi_map): each part's channel (pad, pluck, bass, acid; MAP_OFF not sent) and a transposition for a sampler's root key,
 * each drum lane's channel and note (kick, snare, clap, closed hat, open hat, perc). Channels count from 0. */
#define MAP_OFF 0xFFu
typedef struct { uint8_t ch[4]; int8_t tr[4]; uint8_t dch[EN_NDRUM], dn[EN_NDRUM]; } midimap_t;
const char *const EN_MAP_NAME[EN_NMAP] = {"GM", "M:Samples", "MPC Sample", "SP-404"};
static const midimap_t MAPS[EN_NMAP] = {
    {{0, 1, 2, 3}, {0, 0, 0, 0}, {9, 9, 9, 9, 9, 9}, {36, 38, 39, 42, 46, 37}},
    /* Model:Samples (manual OS 1.13, 7.5): its six tracks on channels 1-6, the drums one a track at note 36 (the sample's own pitch: 12-60 is -24..+24);
     * the melodic parts would land on its drum tracks, so they are not sent */
    {{MAP_OFF, MAP_OFF, MAP_OFF, MAP_OFF}, {0, 0, 0, 0}, {0, 1, 2, 3, 4, 5}, {36, 36, 36, 36, 36, 36}},
    /* MPC Sample: its default pad map is pad index + 36 (Melodee's transcription of blank.xpj, banks E-H; A is 36-51), so A01-A06 on channel 1;
     * with MIDI In on All any channel plays the pads, so the melodic parts are not sent */
    {{MAP_OFF, MAP_OFF, MAP_OFF, MAP_OFF}, {0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {36, 37, 38, 39, 40, 41}},
    /* SP-404MKII, MIDI mode A (Roland's note map v5.50): channel 1 is bank A, notes 36-39 pads 13-16, 40-43 pads 9-12; channels 2-10 are banks B-J,
     * so the melodic parts are not sent */
    {{MAP_OFF, MAP_OFF, MAP_OFF, MAP_OFF}, {0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {36, 37, 38, 39, 40, 41}},
};
static volatile uint8_t midi_map;
static uint8_t arr_m;                            /* (the arrangement's resting tracks: THE ARRANGEMENT, below) */
static uint8_t lk_q, lk_mute;                    /* (0.17.47) THE LINK (lk_send): EN_LK_* bits, from the UI; the tracks this unit leaves to its partner */
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (muted)
        return;
    if (st >= 0xF0u ? (lk_q & EN_LK_FOLLOW) || (!par[P_MIDI] && !(lk_q & EN_LK_LEAD)) : !par[P_MIDI] || (lk_q & EN_LK_ON))
        return;                                  /* (linked: no notes out, the follower no clock; the lead's clock goes out even with MIDI out off) */
    if (st < 0xF0u && (st & 15u) < 4u) {         /* a part's note: its channel on the map, transposed */
        static const uint8_t TR[4] = {EN_TR_PAD, EN_TR_PLUCK, EN_TR_DRONE, EN_TR_ACID};
        const midimap_t *m = &MAPS[midi_map];
        if ((st & 0xF0u) == 0x90u && d2 && arr_m >> TR[st & 15u] & 1u)
            return;                             /* (a part the arrangement rests: not sent) */
        const uint32_t p = st & 15u;
        const int n = (int)d1 + m->tr[p];
        if (m->ch[p] == MAP_OFF)
            return;
        st = (st & 0xF0u) | m->ch[p];
        d1 = (uint32_t)(n < 0 ? 0 : n > 127 ? 127 : n);
    }
    en_midi_out(st, d1, d2);
}
static void midi_drum(int d, uint32_t vel)      /* a drum hit: its lane's channel and note (a note-off at once: one-shots) */
{
    const midimap_t *m = &MAPS[midi_map];
    if (!par[P_MIDI] || muted || (lk_q & EN_LK_ON) || (unsigned)d >= (unsigned)EN_NDRUM)
        return;
    en_midi_out(0x90u | m->dch[d], m->dn[d], vel < 1u ? 1u : vel > 127u ? 127u : vel);
    en_midi_out(0x80u | m->dch[d], m->dn[d], 0);
}
static uint32_t map_channels(void)              /* the channels the map uses, a bit each */
{
    const midimap_t *m = &MAPS[midi_map];
    uint32_t used = 0;
    int k;
    for (k = 0; k < 4; k++)
        if (m->ch[k] != MAP_OFF)
            used |= 1u << m->ch[k];
    for (k = 0; k < EN_NDRUM; k++)
        used |= 1u << m->dch[k];
    return used;
}
static void midi_all_off(void)                  /* all notes off on every channel the map uses */
{
    const uint32_t used = map_channels();
    uint32_t c;
    for (c = 0; c < 16u; c++)
        if (used >> c & 1u)
            en_midi_out(0xB0u | c, 123, 0);
}
void en_midi_cc(uint32_t cc, uint32_t v)        /* a control change on every channel the map uses (0.15.22: the FX knobs out; MIDI out on) */
{
    const uint32_t used = map_channels();
    uint32_t c;
    if (!par[P_MIDI] || (lk_q & EN_LK_ON))
        return;
    for (c = 0; c < 16u; c++)
        if (used >> c & 1u)
            en_midi_out(0xB0u | c, cc & 0x7Fu, v > 127u ? 127u : v);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
#include "sint_tab.h"                   /* SINT: the table, in flash (it was filled at init, in 4 KB of RAM) */
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return SINT[i] + (SINT[i + 1] - SINT[i]) * f;
}

static inline float sine_any(float ph)          /* ph in turns, any value (FM's phases go below 0) */
{
    float fl = (float)(int)ph;
    if (ph < fl)
        fl -= 1.0f;
    return sine(ph - fl);
}

/* ------------------------------------------------- slow random motion --- */
#define NMOD 10                                 /* 0..5 the pads', 6..8 the echo's, 9 the drone's */
#define NMOD_PAD 6
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion;                            /* 0..1: Motion */

/* -------------------------------------------------- the voice bank --- */
/* What each voice plays. The drums: a kit for each of the kick, the snare (with the clap), the hats (closed
 * and open) and the perc (KITS: the first five were the old styles'). The pad, drone and acid: sixteen
 * sounds each (PAD_SOUND, DRONE_SOUND, ACID_SOUND below). Before 1.0.27 a piece had one style for all of
 * it; en_style_sounds turns a stored style into these. */
typedef struct {
    float k_start, k_drop, k_dec0, k_dec1, k_click, k_drive, k_rumble;   /* kick: its sweep from k_start times
                                                 * its note, the drop's speed, its decay, click, drive, a rumble */
    float sn_f1, sn_f2, sn_noise, sn_body, sn_dec;   /* snare: its two tones (Hz), noise, body, decay */
    float cl_gap, cl_dec;                       /* clap: its bursts apart (samples), its tail */
    float h_metal, h_noise, h_dec, oh_dec;      /* hats: the metal's pitch, the noise in it, the decays */
    float p_freq, p_ratio, p_idx;               /* perc: its note (Hz), the modulator's ratio, its index */
    float duck, chop, crush;                    /* the kick's: its duck; chops and crush (the drums all) */
    float p_noise;                              /* (0.17.34) the perc's noise against its FM blip: 1, a shaker */
} kit_t;
static const kit_t KITS[EN_NKIT] = {
    /* Hypno: as it always was */
    {5.0f, 0.0016f, 0.15f, 0.75f, 0.25f, 1.0f, 0.0f,
     185.0f, 330.0f, 1.1f, 0.7f, 1.0f, 353.0f, 1.0f,
     1.0f, 0.6f, 1.0f, 1.0f, 380.0f, 1.48f, 1.4f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* House: a round kick, a wide clap, open hats, congas */
    {3.2f, 0.0011f, 0.2f, 0.8f, 0.12f, 0.6f, 0.0f,
     210.0f, 360.0f, 1.3f, 0.5f, 0.9f, 440.0f, 1.4f,
     1.25f, 0.9f, 1.1f, 1.4f, 300.0f, 1.0f, 0.6f, 1.25f, 0.0f, 0.0f, 0.0f},
    /* Techno: a hard kick with a rumble, metal hats, a tight clap, a bell perc */
    {7.0f, 0.0024f, 0.12f, 0.5f, 0.4f, 1.6f, 0.6f,
     170.0f, 290.0f, 1.4f, 0.4f, 0.7f, 260.0f, 0.7f,
     1.55f, 0.35f, 0.7f, 1.3f, 560.0f, 3.5f, 2.2f, 1.1f, 0.0f, 0.0f, 0.0f},
    /* Acid: a punchy 909-ish kit with a cowbell */
    {4.0f, 0.0014f, 0.13f, 0.6f, 0.35f, 1.25f, 0.0f,
     180.0f, 330.0f, 1.5f, 0.6f, 1.2f, 440.0f, 1.1f,
     1.0f, 0.6f, 0.9f, 1.6f, 560.0f, 1.48f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* Break: a distorted kick, a cracking snare, noisy hats, a tom; chopped and crushed */
    {6.0f, 0.0022f, 0.1f, 0.45f, 0.45f, 2.5f, 0.2f,
     240.0f, 420.0f, 1.6f, 0.8f, 0.6f, 300.0f, 0.8f,
     1.3f, 1.0f, 0.6f, 0.8f, 200.0f, 1.0f, 0.9f, 0.8f, 1.0f, 0.6f, 0.0f},
    /* Minimal: a short clicky kick, a rim, tight hats, a woodblock */
    {4.0f, 0.0018f, 0.08f, 0.35f, 0.5f, 1.0f, 0.0f,
     400.0f, 820.0f, 0.5f, 1.0f, 0.5f, 300.0f, 0.6f,
     1.4f, 0.3f, 0.5f, 0.7f, 720.0f, 2.0f, 0.8f, 0.9f, 0.0f, 0.0f, 0.0f},
    /* Hard: an industrial kick, distorted with a long rumble, a noisy snare, a metal clank; a little crushed */
    {8.0f, 0.0028f, 0.14f, 0.6f, 0.6f, 3.2f, 0.9f,
     200.0f, 340.0f, 1.8f, 0.5f, 0.8f, 220.0f, 0.8f,
     1.7f, 0.8f, 0.8f, 1.2f, 140.0f, 2.76f, 2.8f, 1.3f, 0.0f, 0.25f, 0.0f},
    /* 808: a long boom, a snappy snare, a wide clap, a cowbell */
    {2.2f, 0.0007f, 0.3f, 1.6f, 0.08f, 0.5f, 0.0f,
     238.0f, 476.0f, 1.0f, 0.9f, 1.1f, 380.0f, 1.6f,
     1.15f, 0.5f, 0.8f, 1.8f, 540.0f, 1.48f, 1.2f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* Dusty (lofi): a soft round kick, a loose low snare, a muted clap, dull short hats, a soft rim; the kick ducks the rest a little */
    {2.8f, 0.0010f, 0.2f, 0.95f, 0.08f, 0.45f, 0.0f,
     172.0f, 290.0f, 0.9f, 0.85f, 0.95f, 380.0f, 0.8f,
     0.85f, 0.55f, 0.85f, 1.1f, 520.0f, 1.5f, 0.9f, 0.35f, 0.0f, 0.0f, 0.0f},
    /* Heavy (dubstep): a deep sub kick with a long tail, a big cracking snare, a wide clap, tight hats, a metal perc; it ducks the bass hard */
    {3.0f, 0.0013f, 0.28f, 1.35f, 0.25f, 1.1f, 0.35f,
     190.0f, 330.0f, 1.5f, 0.75f, 1.05f, 330.0f, 1.1f,
     1.2f, 0.5f, 0.7f, 1.0f, 300.0f, 2.2f, 1.6f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* 909: a punchy kick with a click, a snappy snare, a clap, ticking hats and long open ones, a tom */
    {5.0f, 0.0015f, 0.16f, 0.7f, 0.3f, 1.1f, 0.0f,
     188.0f, 332.0f, 1.25f, 0.75f, 0.85f, 410.0f, 0.95f,
     1.3f, 0.7f, 0.7f, 1.2f, 150.0f, 1.0f, 0.5f, 1.1f, 0.0f, 0.0f, 0.0f},
    /* Electro: a short high kick, a tight bright snare, a thin clap, glassy hats, a zappy tom */
    {6.0f, 0.0020f, 0.09f, 0.4f, 0.5f, 1.3f, 0.0f,
     220.0f, 400.0f, 1.5f, 0.5f, 0.6f, 300.0f, 0.4f,
     1.6f, 0.5f, 0.5f, 1.0f, 160.0f, 1.0f, 1.2f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* DnB: a tight kick, a loud cracking snare, a clap, fast tiny hats, a clicky perc */
    {5.5f, 0.0018f, 0.1f, 0.4f, 0.35f, 1.0f, 0.0f,
     240.0f, 460.0f, 1.3f, 0.7f, 0.55f, 380.0f, 0.5f,
     1.5f, 0.5f, 0.45f, 0.8f, 320.0f, 1.5f, 0.8f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* Trap: a long 808 sub with almost no click, a snare and a wide clap together, tiny hats, a bell */
    {2.0f, 0.0006f, 0.35f, 2.0f, 0.05f, 0.6f, 0.0f,
     200.0f, 390.0f, 1.2f, 0.6f, 0.9f, 460.0f, 1.0f,
     1.7f, 0.4f, 0.4f, 0.9f, 460.0f, 1.48f, 1.0f, 1.1f, 0.0f, 0.0f, 0.0f},

    /* (0.17.34) Boom: boom-bap, an SP-1200 / MPC flavour: a round kick with body, a fat cracking snare, crisp short hats, a rim; a little crushed */
    {3.5f, 0.0012f, 0.2f, 0.9f, 0.2f, 1.4f, 0.0f,
     190.0f, 340.0f, 1.5f, 0.85f, 1.1f, 400.0f, 1.0f,
     1.2f, 0.8f, 0.5f, 0.9f, 600.0f, 2.0f, 0.9f, 0.9f, 0.0f, 0.35f, 0.0f},
    /* Ride: the hats as a ride (low metal, little noise, a long tick; open: the wash), a light kick and snare, a woodblock */
    {4.0f, 0.0014f, 0.15f, 0.65f, 0.25f, 1.0f, 0.0f,
     200.0f, 350.0f, 1.1f, 0.6f, 0.9f, 380.0f, 0.9f,
     0.62f, 0.25f, 1.6f, 3.0f, 800.0f, 2.0f, 0.7f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* Snap: the snare a snap (high, little body, short), the clap one burst; a short sub kick, tight hats, a high rim */
    {3.0f, 0.0010f, 0.2f, 1.0f, 0.15f, 0.8f, 0.0f,
     330.0f, 640.0f, 1.3f, 0.35f, 0.55f, 120.0f, 0.6f,
     1.5f, 0.5f, 0.45f, 0.9f, 900.0f, 1.0f, 0.6f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* 606: a short tight kick, a small snappy snare, bright short hats, a tom */
    {4.5f, 0.0018f, 0.1f, 0.35f, 0.3f, 0.9f, 0.0f,
     260.0f, 480.0f, 1.4f, 0.4f, 0.6f, 300.0f, 0.7f,
     1.65f, 0.6f, 0.4f, 0.8f, 220.0f, 1.0f, 0.4f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* Gabber: the kick driven hard with a long tail, a hard noisy snare, harsh hats, a metal perc */
    {7.0f, 0.0026f, 0.15f, 1.0f, 0.6f, 4.5f, 0.3f,
     210.0f, 380.0f, 1.9f, 0.6f, 0.8f, 250.0f, 0.8f,
     1.6f, 0.9f, 0.6f, 1.0f, 180.0f, 2.76f, 3.0f, 1.2f, 0.0f, 0.1f, 0.0f},
    /* Linn: the 80s, a round kick, a fat long snare (for the gated reverb), a wide clap, a tom */
    {3.0f, 0.0011f, 0.18f, 0.6f, 0.15f, 0.7f, 0.0f,
     180.0f, 300.0f, 1.6f, 0.9f, 1.4f, 450.0f, 1.3f,
     1.1f, 0.7f, 0.6f, 1.3f, 260.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f},
    /* Shaker: the perc a shaker (noise, no blip), the hats noisy, a soft round kick, a rim snare */
    {3.2f, 0.0011f, 0.2f, 0.75f, 0.12f, 0.6f, 0.0f,
     380.0f, 760.0f, 0.6f, 0.9f, 0.5f, 380.0f, 0.9f,
     1.4f, 1.0f, 0.5f, 1.0f, 450.0f, 1.0f, 0.3f, 1.2f, 0.0f, 0.0f, 1.0f},};
/* (0.17.34, the maker: "sometimes snare or closed hat are too quiet") each kit's drums levelled to one loudness per drum: every drum of
 * every kit measured alone (build/audit/drumlev.c: K-weighted, its loudest 50 ms), then brought to the kicks' median less 7 dB (snare), 9
 * (clap), 17.5 (closed hat), 11 (open hat), 9 (perc); at most 8 dB up, 4 down. The quiet hats of Techno, Minimal, DnB, Trap and Electro
 * were 8-11 dB under the others. */
static const int8_t KIT_LV[EN_NKIT][5] = {   /* half dB: snare, clap, hat, open hat, perc */
    {  4,   5,   5,   5,   2},   /* Hypno */
    {  4,   4,  -3,  -3,   6},   /* House */
    {  3,   6,  16,  12,   2},   /* Techno */
    { -2,   4,   6,   3,   2},   /* Acid */
    {  6,   7,   9,   3,   8},   /* Break */
    { 11,   7,  16,  16,   1},   /* Minimal */
    {  1,   8,   5,   3,   3},   /* Hard */
    {  4,   3,   8,   6,   2},   /* 808 */
    {  6,   6,   9,   6,   3},   /* Dusty */
    {  0,   5,  12,   8,   2},   /* Heavy */
    {  3,   5,   4,   2,   9},   /* 909 */
    {  4,  11,  15,   8,   7},   /* Electro */
    {  6,  10,  16,  10,   4},   /* DnB */
    {  4,   5,  16,  13,   3},   /* Trap */
    {  0,   4,   8,   5,   3},   /* Boom */
    {  5,   5,  16,  15,   1},   /* Ride */
    {  9,   8,  14,   9,   2},   /* Snap */
    {  5,   8,  13,   6,  11},   /* 606 */
    { -3,   6,   5,  -1,   2},   /* Gabber */
    { -4,   2,   7,   1,   8},   /* Linn */
    { 12,   5,   1,  -3,   1},   /* Shaker */
};
static float kit_g[EN_NDRUM] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
/* THE SAMPLED KITS (0.17.36, the maker: "a few loadable and a few built in"): six sounds a kit (kick, snare, clap, hat, open hat, perc), each 4-bit
 * IMA ADPCM at 22.05 kHz read straight from the flash, played at the lane's tune (a semitone a step) and cut short by its decay below the middle. The
 * built-in ones are rendered offline by tools/smpkits.py (our own models: GPL-clean); they come after the synth kits in each drum's list. */
typedef struct { const uint8_t *d; uint32_t n; uint8_t i0; } smp_t;
typedef struct { const char *name; smp_t s[6]; } skit_t;
#include "smpkits.h"
#include "djsmp.h"                              /* (0.17.44) the sampled DJ snips: the air horn, the rave horn, the crash (tools/djsamples.py) */
_Static_assert(EN_NSKIT_BI == EN_NSKIT_BI_H, "the sampled kits: EN_NSKIT_BI as smpkits.h has them");
static skit_t skit_user[EN_NUKIT];               /* (0.17.38) the loaded kits (their sounds in the flash), */
static int8_t ukit_lv[EN_NUKIT][6];              /* their sounds' levels (half dB, the installer measured them), */
static uint8_t ukit_ok[EN_NUKIT];                /* and which hold one */
static const int8_t SKIT_LV[EN_NSKIT_BI][6] = {{-2, 0, 10, 19, 19, 12}, {1, 4, 3, 12, 9, 3}};   /* half dB: each sound against the synth kits' levels (measured) */
static const smp_t *drum_smp[EN_NDRUM];           /* a drum's sample when its kit is a sampled one (else 0: the synth) */
static const int16_t ADPCM_STEP[89] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
    20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t ADPCM_IDX[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static void smp_kit(int d, int v, int s)         /* drum d plays sound s of kit v (a sampled kit), or the synth */
{
    const int k = v - EN_NKIT, u = k - EN_NSKIT_BI;
    drum_smp[d] = (unsigned)k < (unsigned)EN_NSKIT_BI ? &SKIT_BI[k].s[s] : (unsigned)u < (unsigned)EN_NUKIT && ukit_ok[u] ? &skit_user[u].s[s] : 0;
    if (drum_smp[d])
        kit_g[d] = fm_exp2f((float)(k < EN_NSKIT_BI ? SKIT_LV[k][s] : ukit_lv[u][s]) * (0.5f / 6.0206f));
}
static uint32_t rd32le(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
int en_user_kit(int slot, const uint8_t *img, uint32_t len)   /* (0.17.38) a loaded kit: its header checked (ui.c checked its CRC), its sounds in place */
{
    int s, ok = img != 0 && (unsigned)slot < (unsigned)EN_NUKIT && len > 128u && img[4] == 1 && img[5] == 6;
    for (s = 0; ok && s < 6; s++) {
        const uint8_t *e = img + 28 + 12 * s;
        const uint32_t off = rd32le(e), n = rd32le(e + 4);
        ok = off >= 128u && n >= 2u && off + (n + 1u) / 2u <= len && e[8] <= 88u;
    }
    if ((unsigned)slot >= (unsigned)EN_NUKIT)
        return 0;
    ukit_ok[slot] = 0;                           /* (taken out first: the sounds are swapped while none plays them) */
    if (ok) {
        int k;
        for (s = 0; s < 6; s++) {
            const uint8_t *e = img + 28 + 12 * s;
            skit_user[slot].s[s].d = img + rd32le(e);
            skit_user[slot].s[s].n = rd32le(e + 4);
            skit_user[slot].s[s].i0 = e[8];
            ukit_lv[slot][s] = (int8_t)e[9];
        }
        for (k = 0; k < 12 && img[8 + k] >= 32 && img[8 + k] < 127; k++)
            ukit_name[slot][k] = (char)img[8 + k];
        ukit_name[slot][k] = 0;
        if (!k)
            ukit_name[slot][0] = 'U', ukit_name[slot][1] = 's', ukit_name[slot][2] = 'e', ukit_name[slot][3] = 'r', ukit_name[slot][4] = (char)('1' + slot),
            ukit_name[slot][5] = 0;
        skit_user[slot].name = ukit_name[slot];
        ukit_ok[slot] = 1;
    } else {
        static const char EMPTY[8] = "Empty 1";
        for (s = 0; s < 8; s++)
            ukit_name[slot][s] = EMPTY[s];
        ukit_name[slot][6] = (char)('1' + slot);
    }
    post(C_KITS, 0, 0);                          /* the drums take their kits again (a drum on this slot: its new sound, or the synth) */
    return ok;
}   /* the levels above for the kits playing (the kick's is 1) */
static float kit_db(int k, int d) { return (unsigned)k < EN_NKIT ? fm_exp2f((float)KIT_LV[k][d] * (0.5f / 6.0206f)) : 1.0f; }
static const kit_t *KK = &KITS[0], *KS = &KITS[0], *KH = &KITS[0], *KP = &KITS[0];   /* kick, snare and
                                                 * clap, hats, perc: each its kit */

/* the pad's sounds: the oscillator, stabs (chords on the off-beats) or breaths, its bus filter and level */
enum { PO_RHODES, PO_VOICES, PO_STRINGS, PO_GLASS, PO_ORGAN, PO_WARM, PO_SUPER, PO_PWM, PO_HOLLOW, PO_SINE };
static const struct { uint8_t osc, stabs; float fc, g; } PAD_SOUND[EN_NSOUND] = {
    {PO_RHODES, 0, 1.0f, 1.0f}, {PO_VOICES, 0, 1.0f, 1.0f}, {PO_STRINGS, 0, 1.25f, 0.75f}, {PO_GLASS, 0, 1.7f, 0.85f},
    {PO_ORGAN, 0, 1.1f, 0.8f}, {PO_WARM, 0, 0.85f, 1.0f}, {PO_RHODES, 1, 1.3f, 0.9f}, {PO_VOICES, 0, 0.6f, 0.8f},
    {PO_STRINGS, 1, 0.55f, 1.4f},               /* Dub: saw chords stabbed, dark, into the echo */
    {PO_SUPER, 0, 1.3f, 0.75f},                 /* Trance: a supersaw */
    {PO_PWM, 0, 1.1f, 0.9f},                   /* Poly: two detuned saws less each other, a pulse's width drifting */
    {PO_HOLLOW, 0, 1.0f, 0.75f},                /* Hollow: odd harmonics, a clarinet's */
    {PO_GLASS, 1, 1.6f, 1.2f},                  /* Pings: the bells, stabbed */
    {PO_ORGAN, 1, 1.3f, 1.05f},                 /* Hits: a house organ, stabbed */
    {PO_SUPER, 1, 1.5f, 1.1f},                  /* Rave: supersaw stabs */
    {PO_SINE, 0, 1.0f, 0.65f},                   /* Sine: pure, for minimal */
    {PO_VOICES, 0, 0.7f, 0.85f},                /* Ooh: the voices, darker and rounder */
    {PO_RHODES, 1, 1.8f, 0.9f},                 /* Wurli: an electric piano, bright, played as stabs */
    {PO_GLASS, 1, 1.0f, 1.1f},                  /* Mallet: FM at 3.5, softer, stabbed: marimba and vibes chords */
    {PO_STRINGS, 1, 1.9f, 0.9f},                /* Brass: saw chords stabbed, the filter wide open */
    {PO_SUPER, 0, 0.8f, 0.85f},                 /* Fat: a supersaw with the top closed down */
    {PO_PWM, 1, 1.5f, 1.0f},                    /* Pluck: the PWM pad as short stabs */
    {PO_ORGAN, 0, 0.7f, 0.8f},                  /* Chapel: drawbars with the top closed: a church organ's */
    {PO_SINE, 0, 1.6f, 0.7f},                   /* Air: a sine, open: breath */
};
/* the drone's: the oscillator; the saws and sub's levels, drive, cutoff (octaves) and level */
enum { DO_SAWS, DO_REESE, DO_PULSE, DO_FM, DO_ORGAN, DO_SINE, DO_SQUARE, DO_HOOVER, DO_FIFTHS, DO_METAL, DO_SYNC, DO_FMR };
typedef struct { uint8_t osc; float saw, sub, drive, cut, g; } drone_snd_t;
static const float DRONE_FR[EN_NSOUND] = {[17] = 2.0f, [18] = 1.0f, [19] = 0.5f};   /* DO_FMR's modulator ratio, by sound */
static const drone_snd_t DRONE_SOUND[EN_NSOUND] = {
    {DO_SAWS, 0.4f, 0.55f, 1.0f, 0.0f, 1.0f},   {DO_SAWS, 0.3f, 0.75f, 0.8f, -0.4f, 1.0f},
    {DO_SAWS, 0.45f, 0.5f, 2.2f, -0.3f, 1.0f},  {DO_SAWS, 0.3f, 0.8f, 3.0f, -0.6f, 0.9f},
    {DO_REESE, 0.5f, 0.4f, 1.4f, 0.0f, 1.0f},   {DO_PULSE, 0.45f, 0.45f, 1.1f, 0.0f, 1.0f},
    {DO_FM, 0.55f, 0.35f, 1.2f, 0.4f, 1.0f},    {DO_ORGAN, 0.45f, 0.5f, 1.0f, 0.6f, 0.9f},
    {DO_SAWS, 0.12f, 1.0f, 2.6f, -1.0f, 0.95f}, /* Rumble: the sub driven, the saws far back: a techno rumble */
    {DO_SINE, 0.6f, 0.5f, 1.3f, 0.0f, 1.05f},   /* Sub: a sine and its sub, a little drive */
    {DO_SQUARE, 0.45f, 0.45f, 1.0f, 0.2f, 0.95f},   /* Square: a hollow deep-house bass */
    {DO_HOOVER, 0.4f, 0.5f, 1.6f, 0.5f, 0.85f}, /* Hoover: detuned pulses, far apart, and an octave down */
    {DO_FIFTHS, 0.4f, 0.5f, 1.2f, 0.2f, 0.9f},  /* Fifths: the root and its fifth, a dub techno chord */
    {DO_METAL, 0.5f, 0.45f, 1.3f, 0.5f, 0.95f}, /* Metal: FM at a ratio off the harmonics: clangy */
    {DO_SYNC, 0.45f, 0.5f, 1.2f, 0.3f, 0.85f},  /* Sync: a saw synced, the envelope sweeping it */
    {DO_SAWS, 0.45f, 0.4f, 1.2f, 1.2f, 0.85f},  /* Bright: the saws, the filter open */
    {DO_SAWS, 0.35f, 0.9f, 3.5f, -0.3f, 0.9f},  /* Wub: the sub on top, hard driven, the filter low: a dubstep wobble's body */
    {DO_FMR, 0.5f, 0.45f, 1.3f, 0.4f, 0.95f},   /* Bite: FM at 2, the envelope opening it */
    {DO_FMR, 0.5f, 0.5f, 1.2f, 0.2f, 1.0f},     /* Solid: FM at 1, the DX7's solid bass: round, the envelope a punch (0.17.69, was Tine: FM at 3.5) */
    {DO_FMR, 0.5f, 0.45f, 1.6f, 0.0f, 0.95f},   /* Growl: FM at a half, an octave's grind under the note (0.17.69, was Clang: FM at 5.19) */
    {DO_REESE, 0.55f, 0.35f, 2.6f, 0.2f, 0.9f},  /* Gnarl: the reese driven hard */
    {DO_HOOVER, 0.3f, 0.6f, 1.0f, 0.3f, 0.9f},  /* Wide: the hoover, softer, the sub up */
    {DO_FIFTHS, 0.45f, 0.6f, 1.8f, -0.2f, 0.9f},    /* Power: the root and its fifth, driven */
    {DO_SINE, 0.7f, 0.2f, 1.0f, 1.0f, 1.0f},    /* Air: a soft open sine */
};
/* the acid's: the oscillator, its drive and level */
enum { AO_SAW, AO_SQUARE, AO_SYNC, AO_PLUCK, AO_PWM, AO_CHIP, AO_SUB, AO_TRANCE, AO_BLEEP, AO_FIFTH, AO_BELL, AO_ORGAN,
       AO_LASER, AO_FOLD, AO_SOFT, AO_FMR, AO_SUPER, AO_CZ, AO_MINI, AO_HYPER, AO_CZP, AO_CZB, AO_LEAD };
typedef struct { uint8_t osc; float drive, g; } acid_snd_t;
static const struct { float fr, i0, i1; uint8_t rise; } ACID_FM[EN_NACID] = {   /* AO_FMR: the modulator's ratio, the index (i0 + i1 * the envelope, or its opposite if rise) */
    [16] = {4.0f, 0.05f, 1.3f, 0},   [17] = {1.0f, 0.25f, 0.9f, 0},  [18] = {5.4f, 0.04f, 0.8f, 0},  [19] = {1.0f, 0.4f, 2.0f, 1},
    [20] = {2.76f, 0.2f, 1.8f, 0},   [21] = {3.0f, 0.5f, 1.2f, 0},   [22] = {4.01f, 0.05f, 0.5f, 0}, [23] = {1.5f, 0.35f, 1.6f, 0}};
static const acid_snd_t ACID_SOUND[EN_NACID] = {
    {AO_SAW, 1.0f, 1.0f}, {AO_SQUARE, 1.0f, 1.0f}, {AO_SAW, 1.5f, 1.45f}, {AO_SYNC, 1.1f, 0.85f},
    {AO_PLUCK, 1.0f, 1.15f}, {AO_PWM, 1.0f, 1.0f}, {AO_CHIP, 1.2f, 0.7f}, {AO_SUB, 1.1f, 0.95f},
    {AO_TRANCE, 1.0f, 0.9f},                   /* Trance: three saws detuned, a trance arp's */
    {AO_BLEEP, 0.9f, 0.85f},                     /* Bleep: a sine, the bleeps of early techno */
    {AO_FIFTH, 1.1f, 0.85f},                    /* Fifth: a saw and its fifth, a rave stab */
    {AO_BELL, 1.0f, 1.0f},                      /* Bell: FM at 3.5, the index falling */
    {AO_ORGAN, 1.0f, 0.95f},                    /* Organ: drawbars, a house organ's riff */
    {AO_LASER, 1.0f, 1.0f},                     /* Laser: a sine, its pitch falling with the envelope */
    {AO_FOLD, 1.1f, 1.0f},                      /* Fold: a sine folded over, more as the envelope opens */
    {AO_SOFT, 0.9f, 0.95f},                      /* Soft: a triangle, round, for quiet arps */
    {AO_FMR, 1.0f, 1.0f},      /* Marimba: FM at 4, the index falling fast */
    {AO_FMR, 1.0f, 1.05f},     /* E.Piano: FM at 1: a Rhodes' bark and bell */
    {AO_FMR, 1.0f, 1.0f},      /* Kalimba: FM at 5.4: a thumb piano */
    {AO_FMR, 1.3f, 0.95f},      /* Brass: FM at 1, the index rising as the note settles */
    {AO_FMR, 1.0f, 0.95f},     /* Gong: FM at 2.76: a bar's partials */
    {AO_FMR, 1.3f, 1.0f},       /* Clav: FM at 3, bitten */
    {AO_FMR, 1.0f, 1.0f},     /* Vibes: FM at 4, soft */
    {AO_FMR, 1.0f, 1.0f},      /* Digital: FM at 1.5: a DX-style bell */
    {AO_SUPER, 1.1f, 0.7f},    /* Super: five saws spread, a trance lead's wall */
    {AO_CZ, 1.0f, 1.0f},       /* CZ: phase distortion, the saw bending open with the envelope, a resonant sweep on top */
    {AO_MINI, 1.2f, 0.85f},    /* Mini: a virtual analog: two saws a few cents apart and a pulse an octave down, a monosynth's bass */
    {AO_HYPER, 1.1f, 0.62f},   /* Hyper: five saws spread twice as wide (to +-60 cents), the wall of a hard trance lead */
    {AO_CZP, 1.0f, 1.0f},      /* CZ Pulse: phase distortion toward a square: the knee squeezes as the envelope falls, a hollow to a buzz */
    {AO_CZB, 0.9f, 0.95f},     /* CZ Bell: the cosine and a resonance at 3.5 times its pitch, ringing out with the envelope */
    {AO_LEAD, 1.2f, 0.8f},     /* Lead: two saws seven cents apart and a pulse whose width wanders: a monosynth lead's thickness */
};

static int pad_snd(void) { return (unsigned)par[P_PADTONE] < EN_NSOUND ? par[P_PADTONE] : 0; }
static int pad_stabs(void)                      /* Comp: Hold never stabs, a rhythm always does, Auto as the sound is */
{
    const int c = par[P_PAD_COMP];
    return c == 1 ? 0 : c >= 2 ? 1 : PAD_SOUND[pad_snd()].stabs;
}
static int pad_osc(void) { return PAD_SOUND[pad_snd()].osc; }
int en_pad_filtered(void) { const int o = pad_osc(); return o == PO_STRINGS || o == PO_SUPER || o == PO_PWM; }   /* (0.17.18) the pad's sound has the filter (its saw sounds) */
static uint8_t stabs_were;                      /* the pad was stabbing (to start the breaths again after) */

void en_style_sounds(int16_t *p)                /* a stored style (1..4): its kit and sounds, as it sounded */
{
    static const int8_t PADS[EN_NSTYLE] = {-1, 6, 7, -1, -1}, DRONES[EN_NSTYLE] = {0, 1, 2, 0, 3},
                        ACIDS[EN_NSTYLE] = {-1, -1, -1, 2, -1};
    int st = p[P_STYLE];
    if (st <= 0 || st >= EN_NSTYLE)
        return;
    p[P_KIT_KICK] = p[P_KIT_SNARE] = p[P_KIT_HAT] = p[P_KIT_PERC] = (int16_t)st;
    if (PADS[st] >= 0)
        p[P_PADTONE] = PADS[st];
    p[P_DR_INST] = DRONES[st];
    if (ACIDS[st] >= 0)
        p[P_AC_WAVE] = ACIDS[st];
    p[P_STYLE] = 0;
}
static float stab_env, stab_k = 0.99f;          /* the pad's stabs: the envelope, its fall each 32 samples */

/* ---------------------------------------------------------------- links --- */
/* The drums patched into the voices (P_LINKS on, SEL + SEQ): each kick opens the drone's filter for a
 * moment (a rolling bass from the kick's pattern), each snare or clap flashes the pad's filter open, a
 * hat on an acid note accents it. Each by its amount (0: none). (The perc lane as the acid's clock: retired in 0.17.50.) */
static float lk_kick, lk_snare;                 /* their envelopes, 1 at a hit, falling */
static uint8_t lk_hat;                          /* a hat on this sixteenth */
static int links_on(void);

/* ---------------------------------------------------------------- pad --- */
/* Breathing voices, as the sung tape loops of Music for Airports' "2/1": each voice swells in, hangs
 * and fades on its own cycle (the pad's envelope: attack, hold, release, rest; each voice's a little
 * longer than the last, so they drift apart), taking its note of the chord as it starts, so the chord
 * forms and dissolves rather than sitting there. The timbre: two saws a few
 * cents apart through two formant band-passes, a vowel that Bright moves from "oo" through "ah" to
 * "eh", with a little of the first formant's low-pass for body. */
typedef struct {
    float ph[4], inc[4], sub;                   /* the two saws Detune apart, a second pair inside them (strings, supersaw), the sub's phase */
    float note, target;                         /* MIDI: the note of this breath, the next one's */
    float t, rate, peak, amp;                   /* t 0..1 through the breath, per 32 samples */
    float fa, fh, fr;                           /* the breath's attack, hold and release, as parts of it */
    float ic[4];                                /* the two filters' states */
    float c1[4], c2[4];                         /* their coefficients: a1, a2, a3, k */
    float vib, pan;
    float hf;                                   /* a handoff's fade: 1 normal */
    uint8_t midi_note, hand;                    /* hand: 1 fading out of the old key, 2 into the new */
    uint8_t voices;                             /* this breath's oscillator (PO_*) */
    uint8_t play;                               /* played from the keys: 1 held, 2 let go (ringing out), 0 not */
    uint8_t pnote;                              /* the key that plays it */
    uint32_t pord;                              /* when it was played (the oldest is taken first) */
    float pamp;                                 /* a played note's envelope, 0..1 */
} pad_t;
static pad_t pad[EN_NPAD];
static volatile int16_t fxs_v[EN_NFXS];         /* (0.17.30: defined with the effects' settings, further on) */
static float vt_d[7], vt_pf[2] = {1.0f, 1.0f};  /* (0.17.30) Vintage (vt_tick): 0 .. 3 the pad's voices, 4 the bass, 5 the acid, 6 the pluck, semitones; the acid's and the pluck's as factors */
static float pad_gate, pad_gain, pad_vol, bright, detune = 0.00144f;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */
static float pad_filt, pad_lvl = 1.0f, pad_pan, pad_pitch;   /* the pad's LFO, where it goes this chunk */
static float lfo_acut, lfo_ares, lfo_echo, lfo_tcut;   /* (0.16.4) the LFOs on others: the acid's cutoff (octaves) and resonance (0..1), the echo's time, the punch-in filter's cutoff */
static float acut_was, dlv_was;                 /* (0.17.44) the acid's and the bass's cutoff LFO as the last chunk left it: each glides from there across the next */

static float breath(const pad_t *v, float t)    /* rise, hang, fall, rest: the envelope */
{
    if (t < v->fa)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t / v->fa);
    t -= v->fa;
    if (t < v->fh)
        return 1.0f;
    t -= v->fh;
    if (t < v->fr)
        return 0.5f + 0.5f * fm_cosf(FM_PI * t / v->fr);
    return 0.0f;
}

/* the LFOs (the pad's, the drone's): synced to the tempo, 1/16T .. 32 bars a cycle; sine, triangle,
 * saw, square, sample and hold, or a drift (a smooth wander to a new random place each cycle). Playing, the phase is the song's place
 * (0.17.10): a cycle starts on the bar (a 1-bar sine is at its middle on the downbeat, every bar), whatever the rate was before, a tempo change
 * or the dubstep wobble's rates; stopped, it runs on by itself */
typedef struct {
    float ph, v, a, b;                          /* the phase (turns); the value -1..1; S&H / drift: from, to */
} lfo_t;
static lfo_t pad_lfo, drone_lfo;
static uint32_t lrng = 0x6C078965u;              /* the LFOs' own random stream (0.17.10): their S&H and drift steer no other choice, whenever their cycles turn */
static float lfrand(void)
{
    lrng ^= lrng << 13;
    lrng ^= lrng >> 17;
    lrng ^= lrng << 5;
    return (float)(lrng >> 8) * (1.0f / 16777216.0f);
}
static uint32_t sixteenth;                      /* (the song's place: below) */
static int32_t step_t;
static void lfo_step(lfo_t *l, int rate, int shape, float n, float step_samples)
{
    const unsigned r = (unsigned)rate < NLFO_RATE ? (unsigned)rate : 0u;
    float p;
    if (en_playing && sixteenth > 0u && step_samples > 1.0f) {   /* locked to the song: the sixteenths since the start, and how far into this one */
        float x = ((float)(((sixteenth - 1u) * 3u) % LFO_P3[r]) + 3.0f * fm_minf((float)step_t / step_samples, 1.0f)) / (float)LFO_P3[r];
        x -= (float)(int)x;
        if (x < l->ph) {                         /* a new cycle */
            l->a = l->b;
            l->b = lfrand() * 2.0f - 1.0f;
        }
        l->ph = x;
    } else {
        l->ph += n / (step_samples * 4.0f * LFO_RATE_BEATS[r]);
        if (l->ph >= 1.0f) {
            l->ph -= (float)(int)l->ph;
            l->a = l->b;
            l->b = lfrand() * 2.0f - 1.0f;
        }
    }
    p = l->ph;
    switch (shape) {
    case 0: l->v = fm_sinf(FM_TWO_PI * p); break;
    case 1: l->v = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
    case 2: l->v = 2.0f * p - 1.0f; break;
    case 3: l->v = p < 0.5f ? 1.0f : -1.0f; break;
    case 4: l->v = l->b; break;
    default: l->v = l->a + (l->b - l->a) * (0.5f - 0.5f * fm_cosf(FM_PI * p)); break;
    }
}

#ifdef EN_HOST
int en_test_lfo16(void) { return (int)(pad_lfo.ph * 16.0f); }                        /* the host tests (0.17.10): the pad LFO's place, in sixteenths of its cycle */
int en_test_step16(void) { return sixteenth ? (int)((sixteenth - 1u) & 15u) : -1; }  /* ... and the bar's step */
#endif

static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

static inline float svf_lp(float x, const float *c, float *s)   /* TPT SVF (Zavalishin): the low-pass */
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    return v2;
}
static inline float svf_pick(float x, float v1, float v2, float k, int t)   /* one of the SVF's outputs: 0 low, 1 high, 2 band (unity at the centre), 3 notch, 4 peak */
{
    if (!t)                                      /* (0.15.20: the low-pass, nearly always, by one test rather than the switch's four) */
        return v2;
    switch (t) {
    case 1: return x - k * v1 - v2;
    case 2: return k * v1;
    case 3: return x - k * v1;
    case 4: return 2.0f * v2 - x + k * v1;
    default: return v2;
    }
}
static inline float svf_mm(float x, const float *c, float *s, int t)   /* the multimode TPT SVF (0.15.15, the FILTERS page) */
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    return svf_pick(x, v1, v2, c[3], t);
}
static inline float svf_morph(float x, const float *c, float *s, float wl, float wb, float wh)   /* (0.17.72) the SVF's low, band and high, mixed */
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    const float bp = c[3] * v1;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    return wl * v2 + wb * bp + wh * (x - bp - v2);
}
/* a voice's two filter stages by its type: low and high are 24 dB (the second stage the same), band, notch and peak the first stage alone; the acid's
 * sixth type is the 303 ladder (FT_303, 0.17.17) */
#define FT_TWO(t) ((t) <= 1)
#define FT_303 5
/* (0.17.18, the filter check) each type's place and level against the low-pass's, a voice (pad, bass, acid): High's corner moved down (the voices' cutoffs
 * sit low for a low-pass, a high-pass there left a few dB of the bass: -19 dB), its second stage below it rather than above (a 24 dB high-pass mirrors
 * the low); Band given back the level it loses (unity only at its centre). Measured on the host (tests/host/gacid.c, V=bass|pad, five genres): the bass's
 * High 2-5 dB under Low (was 19), its Band 5-10 (was 17; its saturation holds it); the pad's within 3 and 2-8 (was 18). */
static const float FT_FC[3][5] = {{1.0f, 0.4f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.25f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f, 1.0f, 1.0f}};
static const float FT_G[3][5] = {{1.0f, 2.3f, 4.5f, 1.0f, 1.0f}, {1.0f, 3.0f, 5.5f, 1.0f, 1.0f}, {1.0f, 1.2f, 1.6f, 1.0f, 1.0f}};
#define FT_IX(t) ((unsigned)(t) < 5u ? (t) : 0)

static void midi(uint32_t st, uint32_t d1, uint32_t d2);
/* the pad's MIDI notes are queued, and sent by the first core once the pad's render is done (MIDI out is not for the other core to call) */
static uint8_t pq_st[16], pq_d1[16], pq_d2[16], pq_n;
static void pad_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (pq_n < 16u) {
        pq_st[pq_n] = (uint8_t)st;
        pq_d1[pq_n] = (uint8_t)d1;
        pq_d2[pq_n] = (uint8_t)d2;
        pq_n++;
    }
}
static void pad_midi_flush(void)
{
    uint32_t k;
    for (k = 0; k < pq_n; k++)
        midi(pq_st[k], pq_d1[k], pq_d2[k]);
    pq_n = 0;
}
static void pad_breathe(pad_t *v, int i)        /* a new breath: its length, its strength, its note */
{
    float a = pe_atk_s(par[P_PE_ATK]), h = pe_hold_s(par[P_PE_HOLD]), r = pe_rel_s(par[P_PE_REL]),
          secs = a + h + r + pe_rest_s(par[P_PE_REST]);
    secs = fm_maxf(secs, 0.3f);
    v->fa = fm_maxf(a / secs, 0.002f);
    v->fh = h / secs;
    v->fr = fm_maxf(r / secs, 0.002f);
    secs *= BREATH[i] * (0.85f + 0.3f * pfrand());
    v->rate = 32.0f / (secs * EN_SR);
    v->peak = 0.6f + 0.4f * pfrand();
    if (en_playing && v->midi_note)
        pad_midi(0x80, v->midi_note, 0);
    v->note = v->target;
    v->voices = (uint8_t)pad_osc();
    v->midi_note = (uint8_t)v->target;
    if (en_playing && !par[P_PAD_MAN])
        pad_midi(0x90, v->midi_note, (uint32_t)(50.0f + 40.0f * v->peak));
}

/* a new key (piece, root, scale): the voices still singing the old one fade out in about a second and
 * breathe again on the new chord; a voice at rest just takes its new note. Chord drift doesn't do
 * this: there the old notes are meant to hang over */
#define HAND_STEP (32.0f / (1.0f * EN_SR))
static void pad_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->play)                            /* played from the keys: left alone */
            continue;
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != pad_osc()))
            v->hand = 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

/* a chord change: a voice singing a note the new chord doesn't have fades out in about a second and
 * breathes in on its new note; a voice on a shared note holds it to the end of its breath */
static void pad_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        int pc = v->midi_note % 12, shared = 0;
        if (v->play)
            continue;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

static void pad_coefs(pad_t *v, int i)          /* every 32 samples: the breath, pitch, the vowel */
{
    float f, vw, f1, f2;
    int a;
    if (v->hand == 1) {                         /* out of the old key */
        v->hf -= HAND_STEP;
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->t = 0.15f;                       /* a breath already rising, on the new chord */
            pad_breathe(v, i);
        }
    } else if (v->hand == 2) {                  /* into the new */
        v->hf += HAND_STEP * 0.6f;
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    if (v->play) {                              /* played: its own envelope (Pad env's attack and release, quicker) */
        float rel = fm_clampf(pe_rel_s(par[P_PE_REL]) * 0.35f, 0.08f, 3.0f),
              atk = fm_clampf(pe_atk_s(par[P_PE_ATK]) * 0.12f, 0.006f, 2.0f);
        if (v->play == 1)
            v->pamp = fm_minf(v->pamp + 32.0f / (atk * EN_SR), 1.0f);
        else if ((v->pamp -= 32.0f / (rel * EN_SR)) <= 0.0f) {
            v->pamp = 0.0f;
            v->play = 0;
            v->note = v->target;
            v->hand = 0;
            v->hf = 1.0f;
            v->t = 0.0f;                        /* a breath from the start: rising again (when it's back) */
            if (!par[P_PAD_MAN])
                pad_breathe(v, i);
        }
    } else if (!pad_stabs() && !par[P_PAD_MAN]) {   /* stabs: the tick plays them; off the algorithm: none */
        v->t += v->rate;
        if (v->t >= 1.0f) {
            v->t -= 1.0f;
            pad_breathe(v, i);
        }
    }
    v->vib += (4.6f + 0.4f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    f = mtof(v->note + 0.08f * motion * mod[i].v + pad_pitch + vt_d[i & 3]);              /* a slow drift, as tape; the LFO */
    v->inc[0] = f * (1.0f - detune) * (1.0f / EN_SR);   /* Detune: 2.5 .. 25 cents either way */
    v->inc[1] = f * (1.0f + detune) * (1.0f / EN_SR);
    v->inc[2] = f * (1.0f - 0.45f * detune) * (1.0f / EN_SR);   /* the inner pair, a little less apart: four saws, no two beating alike */
    v->inc[3] = f * (1.0f + 0.45f * detune) * (1.0f / EN_SR);
    vw = fm_clampf((float)bright * 0.01f + 0.25f * arcm + 0.2f * motion * mod[(i + 2) % NMOD_PAD].v + 0.4f * pad_filt,
                   0.0f, 1.0f) * 2.999f;
    a = (int)vw;
    vw -= (float)a;
    f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
    f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
    svf_coefs(v->c1, f1 * (1.0f + 0.03f * (float)i), 0.45f);
    svf_coefs(v->c2, f2 * (1.0f - 0.02f * (float)i), 0.3f);
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* ----------------------------------------------------------------- drone --- */
/* (bass in the code) Three saws a few cents apart and a sub-octave sine, through two low-passes in a
 * row (24 dB, the first resonant), a little driven. The note hardly moves (the chord's root, gliding
 * when the chord changes); what moves is the filter: CUTOFF and RESO, its envelope (retriggered on
 * each chord or every 1 .. 8 bars), its LFO (to the cutoff, the pitch, the level or the drive), a slow
 * random drift, the arc, and the sections (it closes in a breakdown, opens through the build). */
static struct {
    float ph[3], sph, mph, note, target, amp;   /* mph: Metal's modulator, Sync's slave */
    float s[4], c1[4], c2[4];                   /* the filters' states, their coefficients */
    float env;                                  /* its envelope, 0..1 */
    float gtarget;                              /* the chord's root, while the keys play it */
    uint8_t midi_note, rest, env_up;            /* env_up: attacking */
    uint8_t play;                               /* played from the keys */
    float vd;                                   /* a key's velocity, as a gain less 1 (0: as the algorithm plays it; 0.15.23) */
} bass;
static float bl_env, bl_gate;                   /* the bass line's envelope, 0..1, and the samples its note still holds */
#define WUB_OUT 0.8f                            /* the wub's level into the bass's last soft clip */
static float wub_lv, wub_hp;                    /* (0.17.27) the wub's sweep a chunk ago (0..1: drawn from it to now across the chunk), its mid's high-pass */
static uint8_t bl_up, bl_kick, bl_beat;         /* attacking; the kick hit this step; the notes since the bar (the riff's place) */
static float bass_vol, reso_k = 1.0f;
static const float GLIDE_K = 1.0f / (0.6f * EN_SR);
static float brk;                               /* the sections, smoothed: +1 breakdown, -0.5 build, 0 groove */
static float duck_env, duck_dec = 0.9997f;      /* the kick's sidechain: 1 just hit, falling */
static float rd_env;                             /* (0.17.72) the returns' own (RETURNS: Custom), set with it */

/* ------------------------------------------------------------------ acid --- */
/* A 303-ish line, made up as it goes: a loop of LENGTH sixteenths (1 .. 16, so it can turn against the
 * bar), each step a note or a rest (DENSITY), the note a chord tone (now and then another of the
 * scale) within RANGE octaves, some steps accented, some sliding into the next. MUTATE re-rolls a
 * few steps each time round; the notes follow the chord. The voice: a saw or a square through two
 * low-passes in a row, the first resonant, driven between them; an envelope sweeps the cutoff (ENV MOD,
 * DECAY), an accent sweeps it further and plays louder (ACCENT); a slide glides the pitch without a
 * new attack. */
static struct {
    float ph, inc, inc_t, amp, amp_t, env, edec, acc, s[4], c1[4], c2[4];
    int32_t gate_left;                          /* samples until the note's gate closes; -1 held (a slide) */
    uint8_t note, sliding;
    uint8_t play;                               /* played from the keys: the line steps on, silent */
    float ps, p3, p4, p5, lph;                  /* more phases (sync, sub, the trance saws, the supersaw's...), the PWM's LFO */
    float lfb;                                  /* the 303 filter: its output's slow part (150 Hz), kept out of the feedback */
} acid;
static int8_t ac_off[EN_NSTEP], ac_oct[EN_NSTEP];   /* each step: scale steps above the chord's degree, octave */
static uint16_t ac_gate, ac_accent, ac_slide;       /* bit s: step s plays; is accented; slides into the next */
static int ac_pos;
static float acid_vol;

/* ----------------------------------------------------------------- noise --- */
/* A bed of noise, in eight colours: white; pink (-3 dB an octave, Paul Kellet's filter); brown (-6 dB,
 * a leaky sum); blue (+3 dB: pink, differenced); violet (+6 dB: white, differenced); grey (loud at both
 * ends, as the ear hears flat); black (mostly silence: crackle, as dust on vinyl); yellow (pink through
 * a band-pass at ~1.8 kHz: a radio's hiss). TONE closes a low-pass on it; PUMP ducks it with the kick,
 * harder than the rest. Two generators, left and right. */
typedef struct {
    float b0, b1, b2, br, pw, pp, y1, y2, crack, lp, r1, r2;
    uint32_t rng, cnt;
} nz_t;
static nz_t nz[2] = {{.rng = 0x1234567u}, {.rng = 0x89ABCDEu}};
static float noise_vol, nz_lp_k = 0.5f, nz_pump;
static int nz_color;

/* ----------------------------------------------------------------- drums --- */
/* six voices, one a lane: a kick (a sine falling fast from five times its note, a click, driven), a
 * snare (two tuned sines and band-passed noise), a clap (three bursts of noise, then its tail), a
 * closed and an open hat (the 808's six squares' metal and noise, high-passed; the closed chokes the
 * open), a perc (an FM blip: a rim or a tom, by its tune). Each lane its level, tune and decay. */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend, t, hp, ph2, inc2;
    int32_t wait, wait2;                        /* swing, rolls: samples until a hit that waits (pend, pend2: its velocity) */
    float pend2;
    int32_t rgap;                               /* a ratchet: the samples between its hits, the hits still to come (rn), and what each is of the one before (rmul) */
    float rmul;
    float xl, fo;                               /* the last sample out; a hit on a sounding voice: the old hit's last sample, fading (no step, no click) */
    uint8_t rn;
    uint8_t on, note;
    const smp_t *sp;                            /* (0.17.36) the sample playing (0: the synth), its next sample's place, the decoder's predictor and index */
    uint32_t si;
    int32_t pr;
    uint8_t ai;
} drum_t;
static drum_t drum[EN_NDRUM];
#define DR_FADE (1.0f - 1.0f / (0.0008f * EN_SR))   /* the old hit's fade: ~0.8 ms, gone in ~4 */
static float pulse_vol, swing, drive_k = 1.0f, lane_g[EN_NDRUM], lane_tune[EN_NDRUM], lane_dec[EN_NDRUM];
static float kick_age = 1e9f;                  /* samples since the last kick (the pump can follow it) */
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 38, 39, 42, 46, 37};   /* kick, snare, clap, hats, side stick */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR); the style's pitch */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};
static float hat_inc[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};


/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f, echo_fbx = 0.55f;   /* echo_fbx: with a throw */
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    int k;
    nsteps = en_scale(pool, steps);
    for (k = 0; k < 12; k++)
        steps_nat[k] = steps[k];
    dom_on = 0;                                 /* the natural scale (dom_apply raises the seventh again if the chord is V) */
    en_chord_alt = 0;
    walk_next = en_walk_next = -1;              /* (another scale: the next chord is chosen again) */
}

/* THE ANSWER. The acid line asks (the call) and now and then answers: a value type * 5 + dir (0 off; types 1 octave, 2 fifth, 3 third in the scale,
 * 4 mirror, 5 reverse, 6 rotate, 7 sparse, 8 accents; dir 0 .. 4 is two steps down, one, the same, one up, two up: a step is an octave, a fifth, two
 * degrees of the scale, and for the others an octave). It is counted in BARS, so the line's length does not matter, and it can follow the drum
 * drift (Every: Drift): the bar before the drums change. Mirror turns the contour upside down round the line's own middle, so it stays in the
 * line's register. Where a move would leave the register (28 .. 96) it goes the other way. */
static uint8_t ac_ans_bar, ac_ans_loop, ac_fill_bar;   /* this bar (Sync: Bar) or this loop (Sync: Loop) is an answer's; the bar before the drums change */
static uint32_t ac_loop;                        /* the times round the line */
static uint32_t ac_bar0, ac_loop0;               /* the bar and the loop the answer's counting starts from (the beginning of the next bar, of the next loop, after its settings change) */
static uint8_t ac_resync_bar, ac_resync_loop;
static volatile uint8_t ac_now;                 /* (0.17.22, SEL + ARP) an answer asked for: the next bar (Sync Bar) or the next time round (Loop) answers */
void en_acid_ans_now(void) { ac_now = 1; }
volatile uint8_t en_acid_ans;                   /* the answer sounding now, for the screen */
static int acid_raw(int s, int dd)              /* step s's note, the chord's degree moved dd degrees, in the register (no answer) */
{
    int d = deg + ac_off[s] + dd, o, n;
    d += nsteps * 8;
    o = d / nsteps - 8;
    n = 36 + par[P_ROOT] + steps[d % nsteps] + 12 * (o + ac_oct[s] + par[P_AC_OCT] + par[P_OCT]);   /* the root: C2 .. B2 */
    while (n > 96)
        n -= 12;
    while (n < 28)
        n += 12;
    return n;
}
static int ac_centre(void)                      /* the middle of the line's notes (for the mirror) */
{
    int s, lo = 127, hi = 0, len = par[P_AC_LEN] < 1 ? 1 : par[P_AC_LEN] > EN_NSTEP ? EN_NSTEP : par[P_AC_LEN];
    for (s = 0; s < len; s++)
        if (ac_gate >> s & 1u) {
            const int n = acid_raw(s, 0);
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    return hi >= lo ? (lo + hi) / 2 : 60;
}
/* a major chord's avoid note (0.16.26, as SAMPERF's lines): the fourth, a semitone above the chord's third, clashes when it is held or stressed; the
 * next note of the scale up, the fifth, takes its place. Minor chords keep their sixths and seconds (the acid's dark colours). */
static int avoid_fix(int n)
{
    int k, maj = 0, mnr = 0, has4th = 0;
    if (chord_nt < 2)
        return n;
    for (k = 1; k < chord_nt; k++) {
        const int d = (chord_pcs[k] - chord_pcs[0] + 12) % 12;
        maj |= d == 4;
        mnr |= d == 3;
        has4th |= d == 5;                       /* (0.17.29) an 11th chord's own fourth is a chord tone, not an avoid note */
    }
    return maj && !mnr && !has4th && ((n - chord_pcs[0]) % 12 + 12) % 12 == 5 && n + 2 <= 96 ? n + 2 : n;
}
static int in_scale(int n)                      /* n is a note of the scale as it sounds now (an altered chord's too) */
{
    int k;
    for (k = 0; k < nsteps; k++)
        if ((n - par[P_ROOT] - steps[k] + 120) % 12 == 0)
            return 1;
    return 0;
}
static int scale_snap(int n)                    /* n, or the nearest note of the scale (a tie: the lower) (0.17.16) */
{
    int d;
    for (d = 0; d < 6 && nsteps; d++) {
        if (in_scale(n - d))
            return n - d;
        if (in_scale(n + d))
            return n + d;
    }
    return n;
}
static int acid_note(int s, int v)              /* acid step s's note, as the call (v 0) or as answer v plays it */
{
    const int type = v / 5, ds = v % 5 - 2;
    int n, sh = 0;
    if (!type) {
        return acid_raw(s, 0);
    } else if (type == 3) {
        n = acid_raw(s, 2 * ds);
    } else {
        n = acid_raw(s, 0);
        if (type == 4) {                        /* mirror: round the line's middle, then folded into the register */
            n = 2 * ac_centre() - n;
            while (n > 96)
                n -= 12;
            while (n < 28)
                n += 12;
        }
        sh = type == 1 ? 12 * ds : type == 2 ? 7 * ds : type >= 4 ? 12 * ds : 0;
    }
    if (sh) {
        int to = n + sh;
        if (to < 28 || to > 96)
            to = n - sh;                        /* the other way (folded back, it would sound as the call did) */
        while (to > 96)
            to -= 12;
        while (to < 28)
            to += 12;
        n = to;
    }
    return type == 2 || type == 4 ? scale_snap(n) : n;   /* (the fifth and the mirror move by semitones: back onto the scale, 0.17.16) */
}
/* the line's step st when answer v plays: which step of the line it reads (reverse and rotate move them) and whether it sounds */
static int acid_src(int st, int len, int type)
{
    return type == 5 ? len - 1 - st : type == 6 ? ((st - 4) % len + len) % len : st;
}
static int acid_gate(int st, int len, int v)
{
    const int type = v / 5, ss = acid_src(st, len, type);
    return (ac_gate >> ss & 1u) && !(type == 7 && (st & 1)) && !(type == 8 && !(ac_accent >> ss & 1u));
}
int en_acid_preview(int st, int v)
{
    const int len = par[P_AC_LEN] < 1 ? 1 : par[P_AC_LEN] > EN_NSTEP ? EN_NSTEP : par[P_AC_LEN], n = (unsigned)st < EN_NSTEP ? st : 0;
    if (n >= len || nsteps < 1 || !acid_gate(n, len, v))
        return -1;
    return acid_note(acid_src(n, len, v / 5), v);
}

/* PEDAL (0.16.21, Steven's "harmonic drone followers", issue #3): a voice either follows the chords (as always) or holds the key's root
 * under them: bit 0 the bass (its held note and its line's root; a walk walks the key's own triad), bit 1 the pad (its voices hold the
 * key's chord, I, while the harmony moves on; en_chord still shows the harmony). A player's switch, not the piece's (the UI keeps it in
 * the global record). */
static uint8_t pedal;
static int bass_pc(void) { return pedal & 1u ? par[P_ROOT] % 12 : ext_on && ext_bass < 12 ? ext_bass : chord_pcs[0]; }
static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], i;
    chord_nt = en_chord_kind(pool, par[P_ROOT], deg, pg_on && pg_kind[pg_i] ? pg_kind[pg_i] : par[P_CHORD], chord_pcs);   /* (the slot's own type, when it has one) */
    {   /* the chord's tones as the scale sounds now: the dominant's raised seventh (its third), a secondary dominant's third and seventh, a borrowed
         * chord's minor scale (0.16.27: was the dominant's alone) */
        int j;
        for (i = 0; i < chord_nt; i++)
            for (j = 0; j < nsteps; j++)
                if (chord_pcs[i] == (par[P_ROOT] + steps_nat[j]) % 12) {
                    chord_pcs[i] = (par[P_ROOT] + steps[j]) % 12;
                    break;
                }
    }
    if (ext_on && ext_n >= 3) {                 /* (0.17.31) the chord from outside, as it is */
        chord_nt = ext_n;
        for (i = 0; i < ext_n; i++)
            chord_pcs[i] = ext_pc[i];
    }
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    {   /* the pad's register (C3 .. F5), moved by its octaves and the song's, within C1 .. C8 */
        int sh = 12 * (par[P_PAD_OCT] + par[P_OCT]), lo = 48 + sh, hi = 77 + sh;
        while (lo < 24)
            lo += 12, hi += 12;
        while (hi > 108)
            lo -= 12, hi -= 12;
        en_voice_lead(from, chord_pcs, chord_nt, to, lo, hi);
        if (par[P_VOICING]) {                   /* open: drop 2, the second voice from the top an octave down */
            int a = -1, b = -1;
            for (i = 0; i < EN_NPAD; i++)
                if (a < 0 || to[i] > to[a])
                    a = i;
            for (i = 0; i < EN_NPAD; i++)
                if (i != a && (b < 0 || to[i] > to[b]))
                    b = i;
            if (to[b] - 12 >= lo - 7)
                to[b] -= 12;
        }
    }
    for (i = 0; i < EN_NPAD; i++)               /* the harmony as it is, for the screens and the keys' lights */
        en_chord[i] = (uint8_t)to[i];
    if (pedal & 2u) {                            /* the pad on pedal: the key's chord (I, of the piece's or the slot's type) held under the harmony */
        int pcs[4], n, sh = 12 * (par[P_PAD_OCT] + par[P_OCT]), lo = 48 + sh, hi = 77 + sh, from2[EN_NPAD];
        while (lo < 24)
            lo += 12, hi += 12;
        while (hi > 108)
            lo -= 12, hi -= 12;
        n = en_chord_kind(pool, par[P_ROOT], 0, pg_on && pg_kind[pg_i] ? pg_kind[pg_i] : par[P_CHORD], pcs);
        for (i = 0; i < EN_NPAD; i++)
            from2[i] = (int)(pad[i].target + 0.5f);
        en_voice_lead(from2, pcs, n, to, lo, hi);
    }
    for (i = 0; i < EN_NPAD; i++)               /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static void bass_to(int pc)
{
    int n = 36 + pc % 12 + 12 * (par[P_BASSOCT] + par[P_OCT]);   /* C2..B2, its octaves and the song's */
    while (n < 24)                              /* kept where it can be heard: C1 .. B5 */
        n += 12;
    while (n > 83)
        n -= 12;
    bass.gtarget = (float)n;
    if (bass.play || par[P_BL_MODE])            /* the keys have it: the root waits (a bass line plays its own notes) */
        return;
    bass.target = (float)n;
    if (en_playing && bass.midi_note != n) {
        midi(0x82, bass.midi_note, 0);
        midi(0x92, (uint32_t)n, 80);
    }
    bass.midi_note = (uint8_t)n;
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static uint8_t change_pending, fifth_done;
static int hr_left;                             /* a harmonic rhythm: the chord's bars left after this one (the bar line counts; 0 otherwise) */
static int hr_bars(void) { return pg_on ? 1 << pg_bars[pg_i] : hr_bars_v(par[P_DRIFT]); }   /* (a progression set by hand: its slot's bars) */

static void drift_reset(void)
{
    hr_left = hr_bars();
    drift_total = hr_left ? (float)hr_left * 16.0f * step_len : drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

/* each genre's chord habits on a 7-note scale (degrees 0..6: I, ii, III, IV, V, VI, VII; the chord is a number of the scale, minor or major): every move to
 * another degree weighs 1; the genre's own moves add to that (the edges: from, to, weight: ii-V-I for lofi, i-VII-VI-VII for house, i-VI-III-VII for synthwave),
 * home adds a pull (tonic); the chord before a phrase's end (every cad_every chords) heads for cad_to and the phrase's end for home, so the tonic is
 * heard on the phrase boundary; back: the move straight back to the chord before (A B A) is allowed (a house loop wants it, a lofi turnaround does not).
 * hr: the genre's harmonic rhythm, bars a chord (0: Drift's clock in seconds, hypno); Drift then halves or doubles it (hr_bars_v). dom: a dominant
 * in minor: on V the scale's seventh degree is raised a semitone (the chord's third, the acid's and the bass's note) so the cadence is a real one.
 * A diminished degree (its fifth a tritone: vii in a major mode, ii in a minor one, vi in Dorian) is never walked to (0.16.26: it was rare, x0.15, and a
 * genre's own move could still land on it, house's i-VI in Dorian), and a leading chord that is one is replaced by V (or IV). Home is home even when it is
 * diminished (Locrian). A progression set by hand plays what it says.
 * An edge's weight with EB2 set counts only when the scale's second is a semitone above the root (0.16.7, the audit's "bII": a Phrygian piece's flat II,
 * the chord that makes Phrygian Phrygian; in Aeolian or Dorian the same degree is ii and the edge does nothing). */
#define EB2 0x80u
typedef struct { uint8_t from, to, w; } ch_edge_t;
static const struct { uint8_t tonic, cad_every, cad_to, back, hr, dom; ch_edge_t e[8]; } CHT[EN_NGENRE] = {
    {0, 0, 0, 0, 0, 0, {{0}}},                                                                    /* HYPNO: the walk by degree (W7) */
    {2, 8, 4, 0, 2, 1, {{1, 4, 12}, {4, 0, 12}, {0, 1, 4}, {0, 3, 4}, {3, 1, 5}, {5, 1, 6}, {2, 5, 6}, {6, 2, 5}}},   /* lofi: ii-V-I, turnarounds */
    {2, 4, 6, 1, 2, 0, {{0, 6, 8}, {6, 5, 6}, {5, 6, 6}, {6, 0, 5}, {5, 0, 3}, {0, 5, 5}, {3, 6, 3}, {0, 0, 0}}},    /* house: i-VII-VI-VII loops */
    {4, 8, 6, 0, 8, 0, {{0, 5, 6}, {5, 0, 8}, {0, 6, 4}, {6, 0, 6}, {0, 4, 3}, {0, 1, EB2 | 5}, {1, 0, EB2 | 8}, {0, 0, 0}}},   /* dubstep: sparse, VI and home (Phrygian: bII) */
    {3, 4, 3, 1, 4, 0, {{0, 3, 8}, {3, 0, 8}, {0, 6, 5}, {6, 0, 5}, {0, 5, 3}, {5, 6, 3}, {0, 0, 0}, {0, 0, 0}}},    /* dub: a minor vamp, one and four */
    {2, 4, 4, 0, 4, 1, {{1, 4, 10}, {4, 0, 8}, {0, 5, 5}, {5, 1, 6}, {0, 3, 4}, {3, 1, 4}, {0, 0, 0}, {0, 0, 0}}},   /* dnb: liquid ii-V */
    {3, 4, 4, 0, 4, 1, {{0, 3, 4}, {3, 4, 4}, {4, 0, 6}, {0, 5, 4}, {5, 3, 4}, {6, 0, 4}, {0, 0, 0}, {0, 0, 0}}},    /* breaks: even, V home */
    {2, 4, 4, 0, 2, 1, {{1, 4, 8}, {4, 0, 8}, {0, 5, 5}, {5, 3, 5}, {3, 1, 5}, {2, 5, 4}, {0, 0, 0}, {0, 0, 0}}},    /* garage: soulful */
    {4, 4, 4, 0, 4, 0, {{0, 4, 5}, {4, 0, 7}, {0, 3, 4}, {3, 0, 6}, {0, 6, 3}, {6, 0, 4}, {0, 0, 0}, {0, 0, 0}}},    /* electro: a fifth, home */
    {5, 4, 6, 0, 4, 0, {{0, 5, 6}, {5, 6, 5}, {6, 0, 8}, {0, 3, 3}, {3, 0, 7}, {5, 0, 5}, {0, 1, EB2 | 5}, {1, 0, EB2 | 8}}},   /* trap: dark, home (Phrygian: bII) */
    {2, 4, 4, 0, 2, 1, {{1, 4, 10}, {4, 0, 9}, {0, 3, 5}, {3, 6, 6}, {6, 2, 6}, {2, 5, 6}, {5, 1, 6}, {0, 0, 0}}},   /* disco: the circle of fifths */
    {5, 4, 3, 1, 4, 0, {{0, 3, 8}, {3, 0, 8}, {0, 6, 3}, {6, 0, 5}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}},    /* funk: the dorian vamp (i-IV) */
    {3, 4, 4, 1, 2, 1, {{0, 3, 7}, {3, 0, 7}, {0, 4, 5}, {4, 0, 7}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}},    /* reggae: one, four, five */
    {3, 4, 6, 0, 2, 0, {{0, 5, 8}, {5, 2, 8}, {2, 6, 8}, {6, 0, 7}, {6, 5, 3}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}},    /* synthwave: i-VI-III-VII */
    {2, 8, 4, 0, 4, 1, {{1, 4, 10}, {4, 0, 9}, {0, 1, 5}, {0, 3, 4}, {3, 1, 5}, {5, 1, 5}, {0, 0, 0}, {0, 0, 0}}},   /* amapiano: jazzy turnarounds */
    {5, 8, 6, 0, 4, 0, {{0, 5, 5}, {5, 0, 7}, {0, 3, 4}, {3, 0, 6}, {6, 0, 5}, {0, 1, EB2 | 4}, {1, 0, EB2 | 7}, {0, 0, 0}}},   /* trip-hop: minor, sparse, home (Phrygian: bII) */
    {5, 4, 1, 1, 2, 0, {{0, 1, 9}, {1, 0, 9}, {0, 6, 4}, {6, 0, 4}, {0, 4, 3}, {4, 1, 3}, {0, 0, 0}, {0, 0, 0}}},    /* industrial: the grind between i and the flat II */
    {5, 8, 1, 1, 8, 0, {{0, 1, 8}, {1, 0, 10}, {0, 6, 4}, {6, 0, 5}, {0, 5, 3}, {5, 0, 4}, {0, 0, 0}, {0, 0, 0}}},    /* psy: home held 8 bars, the flat II and back */
    {6, 8, 1, 1, 8, 0, {{0, 1, 10}, {1, 0, 10}, {0, 3, 3}, {3, 0, 4}, {0, 6, 2}, {6, 0, 3}, {0, 0, 0}, {0, 0, 0}}},   /* dark forest: i and the flat II, barely moving */
    {5, 4, 3, 1, 2, 1, {{0, 3, 8}, {3, 0, 8}, {0, 5, 6}, {5, 0, 6}, {0, 6, 4}, {6, 0, 4}, {5, 4, 3}, {4, 0, 5}}},    /* hiphop: one and four, VI, VII, a real V */
    {3, 4, 6, 0, 4, 0, {{0, 5, 8}, {5, 6, 8}, {6, 0, 6}, {5, 2, 6}, {2, 6, 6}, {6, 3, 4}, {3, 0, 5}, {0, 6, 4}}},    /* trance: i-VI-VII, i-VI-III-VII, 4 bars a chord */
    {4, 8, 3, 1, 8, 0, {{0, 3, 6}, {3, 0, 6}, {0, 5, 5}, {5, 0, 5}, {0, 4, 4}, {4, 0, 4}, {3, 5, 3}, {5, 3, 3}}},    /* ambient: slow, I and IV, vi, V */
};
/* synthwave in a major mode (0.16.7, the audit's 2b): I-vi-IV-V, the decade's own loop, in place of its minor i-VI-III-VII edges */
static const ch_edge_t SYN_MAJ[8] = {{0, 5, 8}, {5, 3, 8}, {3, 4, 8}, {4, 0, 8}, {0, 3, 4}, {4, 5, 3}, {0, 0, 0}, {0, 0, 0}};
static volatile uint8_t chord_genre;
void en_genre(int g) { chord_genre = (uint8_t)((unsigned)g < EN_NGENRE ? g : 0); }
static int hr_bars_v(int v)                     /* the genre's bars a chord, by Drift: a quarter below 20, half below 45, double above 80, four times above 95 */
{
    int b = CHT[chord_genre].hr;
    if (drift_unit == 2 || (!b && drift_unit != 1))
        return 0;                               /* seconds: hypno's way, or the player's choice */
    if (!b)
        b = 4;                                  /* bars where the genre counts seconds: four a chord, then Drift as everywhere */
    b = v < 20 ? b >> 2 : v < 45 ? b >> 1 : v > 95 ? b << 2 : v > 80 ? b << 1 : b;
    return b < 1 ? 1 : b;
}
static int deg_nat(int k) { return steps[k % 7] - (k % 7 == 6 ? dom_on : 0); }   /* the natural scale's step (without the dominant's raise) */
static int deg_dim(int i) { return nsteps == 7 && (deg_nat(i + 4) - deg_nat(i) + 12) % 12 == 6; }   /* the degree's triad is diminished */
static int deg_aug(int i) { return nsteps == 7 && (deg_nat(i + 4) - deg_nat(i) + 12) % 12 == 8; }   /* (0.17.33) or augmented (harmonic and melodic minor's III) */
static int diatonic(void)                       /* (0.17.33) the scale is one of the major scale's modes (the borrowing and the secondary dominants are
                                                 * for those: the other seven-note scales are their own colour already) */
{
    int r;
    for (r = 0; r < 12; r++)
        if ((((uint32_t)pool >> r | (uint32_t)pool << (12 - r)) & 0xFFFu) == 0xAB5u)
            return 1;
    return 0;
}
static void dom_apply(void)                     /* the dominant in minor: the seventh degree raised while the chord is V, in a genre that cadences (CHT dom) */
{
    if (nsteps != 7)
        return;
    steps[6] -= dom_on;
    dom_on = (uint8_t)(!ext_on && CHT[chord_genre].dom && deg == 4 && steps[6] - steps[4] == 3 && steps[6] < 11);
    steps[6] += dom_on;
}
int en_prog_chord(int d, int kind, int *pcs)     /* a degree's chord as chord_change would play it, of a slot's type (0 the piece's: the Progression page) */
{
    const int n = en_chord_kind(pool, par[P_ROOT], d, kind ? kind : par[P_CHORD], pcs);
    if (nsteps == 7 && CHT[chord_genre].dom && d % 7 == 4 && deg_nat(6) - deg_nat(4) == 3 && deg_nat(6) < 11) {   /* the cadence's V in minor: its raised seventh */
        const int lt = (par[P_ROOT] + deg_nat(6)) % 12;
        int k;
        for (k = 0; k < n; k++)
            if (pcs[k] == lt)
                pcs[k] = (lt + 1) % 12;
    }
    return n;
}
int en_genre_bars(void) { return CHT[chord_genre].hr != 0; }
void en_prog(const uint8_t *b)                  /* (a command carries 24 bits: the slots' chords, then their bars, Play and the unit) */
{
    post(C_PROG, b[0], b[1] | b[2] << 8);
    post(C_PROG2, b[3], b[4] | b[5] << 8);
}
void en_prog_kinds(const uint8_t *b) { post(C_PROGK, b[0], b[1] | b[2] << 8); }
void en_prog_capture(uint8_t *b)
{
    int s, n, bc, hb = hr_bars_v(par[P_DRIFT]);
    uint8_t d[8];
    if (dh_ln && dh_n >= dh_ln) {               /* the chord loop, as it plays */
        n = dh_ln;
        for (s = 0; s < n; s++)
            d[s] = dh[s];
    } else {                                    /* else the last four chords heard (the one playing last) */
        n = hist_n < 4 ? hist_n : 4;
        for (s = 0; s < n; s++)
            d[s] = hist[hist_n - n + s];
        if (!n) {
            d[0] = (uint8_t)deg;
            n = 1;
        }
    }
    for (bc = 0; bc < 3 && (1 << bc) < (hb ? hb : 4); bc++)
        ;
    for (s = 0; s < EN_PRG_N; s++) {
        en_prg_put(b, 3 * s, 3, s < n ? d[s] + 1 : 0);
        en_prg_put(b, 24 + 2 * s, 2, bc);
    }
}
static void chord_change(void);
static void prog_take(uint32_t lo, uint32_t hi) /* the progression from the page or the piece (en_prog) */
{
    const int was = pg_on, unit = (int)(hi >> 17 & 3u);
    int s;
    pg_len = 0;
    for (s = 0; s < EN_PRG_N; s++) {
        pg_deg[s] = (uint8_t)(lo >> 3 * s & 7u);
        pg_bars[s] = (uint8_t)(hi >> 2 * s & 3u);
        if (pg_deg[s])
            pg_len = (uint8_t)(s + 1);
    }
    pg_on = (uint8_t)((hi >> 16 & 1u) && pg_len);
    if (pg_i >= pg_len)
        pg_i = 0;
    if (pg_on && !was) {                        /* on: the first slot from the next bar (stopped: now) */
        pg_start = 1;
        if (en_playing)
            change_pending = 1;
        else
            chord_change();
    } else if (pg_on) {
        if (hr_left >= hr_bars())               /* the slot playing made shorter: it ends sooner */
            hr_left = hr_bars() - 1;
    }
    en_prog_pos = (int8_t)(pg_on ? pg_i : -1);
    if ((!pg_on && was) || unit != drift_unit) {   /* back to the walk, or Drift's unit changed: the chord's time anew */
        drift_unit = (uint8_t)unit;
        drift_reset();
    }
}
#ifdef EN_HOST
int en_host_dimwalk;                            /* moves to a diminished chord other than home (the walk never: 0.16.26) */
int en_host_alt[3];                             /* the walk's chords: as they are, secondary dominants, borrowed (0.16.27) */
int en_host_fillq;                              /* fills asked for (en_fill_next) */
int en_host_arrm(void) { return arr_m; }
#endif
static uint8_t deg_prev, cad_n;                 /* the chord before this one; the chords since the phrase began */
static uint8_t drop_tonic;                       /* the drop takes the next chord home */
static void chord_to(int i);
/* THE CIRCLE MOVES (0.16.27; the maker: "if following circle of fifths and that sounds good then we're good"). By genre, a chord a fifth above
 * the next one (the walk knows it, a chord ahead) becomes that chord's dominant: its third major, its seventh minor (D7 to G, E7 A7 D7 G7 round the
 * circle); in a major key IV, VI and VII can be borrowed from the parallel minor (iv, bVI, bVII: the scale is Aeolian on the root while the chord
 * plays, so the pad, the bass, the acid and the pluck all follow). VII in a major key is then always bVII (its own chord, vii°, is never walked
 * to). Percent of the chords that can: CH_SEC, CH_BOR. A progression set by hand plays its chords as they are. */
static const uint8_t CH_SEC[EN_NGENRE] = {0, 45, 15, 0, 0, 30, 0, 35, 0, 0, 50, 25, 15, 10, 40, 15, 0, 0, 0, 20, 0, 0};
static const uint8_t CH_BOR[EN_NGENRE] = {0, 20, 20, 0, 0, 15, 0, 15, 0, 0, 15, 10, 0, 40, 15, 25, 0, 0, 0, 10, 0, 10};
static const uint8_t AEOLIAN[7] = {0, 2, 3, 5, 7, 8, 10};
static int fixed_shape(void) { return par[P_CHORD] == EN_CH_DUB || par[P_CHORD] == EN_CH_4THS; }
static int bor_key(void) { return nsteps == 7 && chord_genre && CH_BOR[chord_genre] && steps_nat[2] == 4 && !fixed_shape() && diatonic(); }   /* a major key, a genre that borrows */
static int borrow_vii(int i) { return i == 6 && bor_key(); }
static int bor_decide(int i) { return bor_key() && (i == 6 || ((i == 3 || i == 5) && frand() * 100.0f < (float)CH_BOR[chord_genre])); }
static void alt_clear(void)                     /* the natural scale again (dom_apply and alt_apply set it as the next chord needs) */
{
    int k;
    for (k = 0; k < nsteps; k++)
        steps[k] = steps_nat[k];
    dom_on = 0;
    en_chord_alt = 0;
}
static void alt_apply(int i, int bor, int n)    /* the chord on degree i (just moved to): borrowed, or the dominant of the next (n) */
{
    int k, changed = 0;
    if (nsteps != 7 || !chord_genre || fixed_shape() || !diatonic())
        return;
    if (bor) {
        for (k = 0; k < 7; k++)
            steps[k] = AEOLIAN[k];
        dom_on = 0;
        en_chord_alt = 2;
        changed = 1;
    } else if (n >= 1 && n < 7 && n != i && CH_SEC[chord_genre] && (steps_nat[i] - steps_nat[n] + 12) % 12 == 7 &&
               (steps_nat[(i + 4) % 7] - steps_nat[i] + 12) % 12 == 7 && frand() * 100.0f < (float)CH_SEC[chord_genre]) {
        const int t = (i + 2) % 7, s = (i + 6) % 7;
        if ((steps[t] - steps_nat[i] + 12) % 12 == 3)
            steps[t]++, changed = 1;            /* its third, major */
        if ((steps[s] - steps_nat[i] + 12) % 12 == 11)
            steps[s]--, changed = 1;            /* its seventh, minor */
        if (changed)
            en_chord_alt = 1;
    }
    if (changed) {
        pad_retarget();
        pad_leave();                            /* (0.17.16) a voice on the scale's own note fades out for the altered one, as at any chord change; it held
                                                   to the end of its breath before, a semitone from the new chord (the audit: twice the clashes) */
        bass_to(bass_pc());
    }
}
static int walk_ok(int i) { return !(i && (deg_dim(i) || deg_aug(i))) || borrow_vii(i); }
static int walk_pick(void)                      /* the walk's next degree from where it is (the natural scale): the genre's own moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i, cad_to = 0;
    const int cg = chord_genre;
    const int rich = nsteps == 7 && cg;
    if (rich) {                                 /* the leading chord: the genre's, unless the mode makes it diminished (then V, or IV) */
        cad_to = CHT[cg].cad_to;
        if (deg_dim(cad_to) || deg_aug(cad_to))   /* (0.17.33: or augmented; and V or IV may be either in the newer scales: then home) */
            cad_to = !deg_dim(4) && !deg_aug(4) ? 4 : !deg_dim(3) && !deg_aug(3) ? 3 : 0;
    }
    for (i = 0; i < nsteps; i++) {
        if (i == deg) {
            w[i] = 0.0f;
        } else if (rich) {
            const ch_edge_t *const e = cg == 13 && deg_nat(2) == 4 ? SYN_MAJ : CHT[cg].e;   /* (synthwave, a major third: its major loop) */
            int k;
            w[i] = 1.0f + (i == 0 ? (float)CHT[cg].tonic : 0.0f);
            for (k = 0; k < 8; k++)
                if (e[k].w && e[k].from == deg && e[k].to == i && (!(e[k].w & EB2) || deg_nat(1) == 1))
                    w[i] += (float)(e[k].w & ~EB2);
            if (!CHT[cg].back && i == deg_prev && deg_prev != deg && cad_n)   /* no A B A (a loop's own chord is the pull back) */
                w[i] = 0.0f;
            if (CH_SEC[cg] && i != 0 && (steps_nat[deg] - steps_nat[i] + 12) % 12 == 7)
                w[i] += (float)CH_SEC[cg] * 0.1f;   /* a fifth down: the circle (0.16.27, as much as the genre likes its dominants) */
            if (cad_n + 1 >= CHT[cg].cad_every && i == cad_to && deg != 0)
                w[i] += 10.0f;                  /* the chord before the phrase's end: its leading chord */
            if (cad_n >= CHT[cg].cad_every && i == 0)
                w[i] += 30.0f;                  /* the phrase's end: home */
        } else {
            w[i] = nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        }
        if (i != 0 && (deg_dim(i) || deg_aug(i)) && !borrow_vii(i))
            w[i] = 0.0f;                        /* a diminished chord: not walked to (hypno's walk too); VII borrowed as bVII is (0.16.27) */
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    {   /* the progression loop: the last 2, 4 or 8 chords repeat (4..6: now and then one is drawn anew); it starts from the chord playing, so a loop of two is
         * the home chord and one other, and a new piece empties it */
        const int lp = par[P_CH_LOOP], ln = lp ? 2 << ((lp - 1) % 3) : 0, dv = lp > 3 ? 25 : 0;
        if (ln != dh_ln) {
            dh_ln = (uint8_t)ln;
            dh_n = dh_i = 0;
        }
        if (ln && dh_n == 0)
            dh[dh_n++] = (uint8_t)deg;
        if (ln && dh_n >= ln) {                  /* the loop is full: its next chord (or, now and then, a new one in its place) */
            const int k = dh_i % ln;
            dh_i = (uint8_t)(k + 1);
            if (dv && frand() * 100.0f < (float)dv)
                dh[k] = (uint8_t)i;
            else if (dh[k] < nsteps && walk_ok(dh[k]))   /* (a mode changed under the loop: its diminished chord drawn anew) */
                i = dh[k];
        } else if (ln) {                         /* filling it */
            dh[dh_n++] = (uint8_t)i;
            dh_i = 0;
        }
    }
    return i;
}
static void chord_change(void)                  /* to another degree: the progression set by hand, or the walk (its next chosen a chord ahead) */
{
    int i, bor;
    if (ext_on) {                               /* (0.17.31) the chord from outside holds until another comes */
        drift_reset();
        return;
    }
    alt_clear();
    if (pg_on) {                                /* the progression set by hand: its next slot (the drop: its first); an empty slot keeps the chord for its bars */
        pg_i = (uint8_t)(drop_tonic || pg_start ? 0 : (pg_i + 1) % pg_len);
        drop_tonic = pg_start = 0;
        en_prog_pos = (int8_t)pg_i;
        walk_next = en_walk_next = -1;
        if (pg_deg[pg_i]) {
            i = (pg_deg[pg_i] - 1) % nsteps;
#ifdef EN_HOST
            if (i && deg_dim(i))
                en_host_dimwalk++;              /* (tests/host/gharm.c: only a progression set by hand may) */
#endif
            chord_to(i);
        } else
            drift_reset();
        return;
    }
    if (walk_next >= 0 && walk_next < nsteps && walk_ok(walk_next)) {
        i = walk_next;
        bor = walk_bor;
    } else {
        i = walk_pick();
        bor = bor_decide(i);
    }
    if (drop_tonic) {                           /* the drop: home */
        drop_tonic = 0;
        i = 0;
        bor = 0;
    }
    chord_to(i);
    walk_next = (int8_t)walk_pick();            /* the next, from here (the screens show it) */
    walk_bor = (uint8_t)bor_decide(walk_next);
    en_walk_next_root = (uint8_t)((par[P_ROOT] + (walk_bor ? AEOLIAN[walk_next % 7] : steps_nat[walk_next])) % 12);
    en_walk_next = walk_next;
    alt_apply(i, bor, walk_next);
#ifdef EN_HOST
    if (i && (deg_dim(i) || deg_aug(i)))   /* (0.17.33: or augmented) */
        en_host_dimwalk++;                      /* (with its alteration: bVII is not diminished) */
    en_host_alt[en_chord_alt]++;
#endif
}
static void chord_to(int i)                     /* the chord moves to degree i */
{
    int k;
    deg_prev = (uint8_t)deg;
    cad_n =(uint8_t)(i == 0 && nsteps == 7 && chord_genre && cad_n >= CHT[chord_genre].cad_every ? 0 : cad_n + 1);
    deg = i;
    if (hist_n >= 8)
        for (k = 0; k < 7; k++)
            hist[k] = hist[k + 1];
    hist[hist_n < 8 ? hist_n++ : 7] = (uint8_t)i;
    dom_apply();
    pad_retarget();
    pad_leave();
    bass_to(bass_pc());
    if (par[P_DE_TRIG] == 0)
        bass.env_up = 1;                        /* the drone's envelope: on each chord */
    drift_reset();
}
static void ext_apply(void)                     /* (0.17.31) the chord from outside into the scale, its degree, the voices */
{
    int j, k, d = 0, best = 99;
    const int r = par[P_ROOT] % 12;
    alt_clear();
    if (nsteps == 7) {
        for (j = 0; j < ext_n; j++) {
            const int off = (ext_pc[j] - r + 24) % 12;
            int in = 0, bk = -1, bd = 99;
            for (k = 0; k < 7; k++)
                in |= steps[k] == off;
            if (in)
                continue;
            for (k = 0; k < 7; k++) {
                int t, ct = 0, dd = steps[k] > off ? steps[k] - off : off - steps[k];
                for (t = 0; t < ext_n; t++)
                    ct |= (ext_pc[t] - r + 24) % 12 == steps[k];
                if (!ct && dd < bd) {
                    bd = dd;
                    bk = k;
                }
            }
            if (bk >= 0 && bd <= 2)
                steps[bk] = off;
        }
        en_chord_alt = 3;
    }
    for (k = 0; k < nsteps; k++) {              /* its degree: the scale's tone nearest its root */
        int dd = ((steps[k] - (ext_pc[0] - r)) % 12 + 24) % 12;
        if (dd > 6)
            dd = 12 - dd;
        if (dd < best) {
            best = dd;
            d = k;
        }
    }
    walk_next = en_walk_next = -1;
    chord_to(d);
}

/* ----------------------------------------------------------- the acid line --- */
int en_acid_pack(uint8_t *b)
{
    uint32_t acc = 0, bits = 0, o = 0;
    int s;
    for (s = 0; s < EN_NSTEP; s++) {
        const uint32_t v = (en_acid_off[s] & 15u) | (uint32_t)(en_acid_oct[s] & 3u) << 4 | (uint32_t)(en_acid_gate >> s & 1u) << 6 |
                           (uint32_t)(en_acid_acc >> s & 1u) << 7 | (uint32_t)(en_acid_slide >> s & 1u) << 8;
        acc |= v << bits;
        bits += 9;
        while (bits >= 8) {
            b[o++] = (uint8_t)acc;
            acc >>= 8;
            bits -= 8;
        }
    }
    return (int)o;
}
void en_acid_unpack(const uint8_t *b)
{
    uint32_t acc = 0, bits = 0, i = 0;
    int s;
    for (s = 0; s < EN_NSTEP; s++) {
        uint32_t v;
        while (bits < 9) {
            acc |= (uint32_t)b[i++] << bits;
            bits += 8;
        }
        v = acc & 0x1FFu;
        acc >>= 9;
        bits -= 9;
        post(C_ACSTEP, s, (int)((v & 15u) | (v >> 4 & 3u) << 4 | (v >> 6 & 1u) << 6 | (v >> 7 & 1u) << 7 | (v >> 8 & 1u) << 8));
    }
}
void en_acid_step(int s, int off, int oct, int gate, int accent, int slide)
{
    post(C_ACSTEP, s & 15, (off & 15) | (oct & 3) << 4 | (gate ? 1 : 0) << 6 | (accent ? 1 : 0) << 7 | (slide ? 1 : 0) << 8);
}
void en_acid_capture(int s, int note, int accent, int slide) { post(C_ACCAP, (s & 15) | (accent ? 16 : 0) | (slide ? 32 : 0), note & 127); }
void en_acid_capture_d(int s, int note, int d, int accent, int slide)   /* (0.17.31) as played over degree d */
{
    post(C_ACCAP, (s & 15) | (accent ? 16 : 0) | (slide ? 32 : 0), (note & 127) | (d & 15) << 7 | 1 << 11);
}
void en_bass_capture_d(int s, int note, int d, int accent, int glide)   /* (0.17.31) a bass step as played over degree d */
{
    post(C_BSCAP, (s & 15) | (accent ? 16 : 0) | (glide ? 32 : 0), (note & 127) | (d & 15) << 7);
}
void en_acid_keep_answer(void) { post(C_ACKEEP, 0, 0); }
void en_ext_chord(int root, int bass, uint32_t mask) { post(C_EXTCH, (root & 15) | ((bass < 0 || bass > 11 ? 12 : bass) & 15) << 4, (int)(mask & 0xFFFu)); }
void en_pluck_capture(int s, int note) { post(C_CMCAP, s & 15, note < 0 ? 128 : note & 127); }
/* THE PLUCK'S LINE (the Pluck steps page, 0.14.21): deja vu's loop, Loop 16 and Rate 1/16 and Pluck vu 100, read as sixteen steps of the bar, each a rung of
 * the chord's ladder and a level; frozen (P_CM_LOCK) it survives a new piece and is stored with it, six bits a step. */
volatile uint8_t en_cm_rung[16], en_cm_lvl[16], en_cm_pos;
void en_pluck_step(int s, int rung, int level) { post(C_CMSTEP, s & 15, (rung & 15) | (level & 3) << 4); }
int en_pluck_pack(uint8_t *b)
{
    uint32_t acc = 0, bits = 0, i = 0;
    int s, any = 0;
    for (s = 0; s < 16; s++) {
        const uint32_t v = ((uint32_t)en_cm_rung[s] & 15u) | ((uint32_t)en_cm_lvl[s] & 3u) << 4;
        any |= v != 0;
        acc |= v << bits;
        bits += 6;
        while (bits >= 8) {
            b[i++] = (uint8_t)acc;
            acc >>= 8;
            bits -= 8;
        }
    }
    return any;
}
void en_pluck_unpack(const uint8_t *b)
{
    uint32_t acc = 0, bits = 0, i = 0;
    int s;
    for (s = 0; s < 16; s++) {
        uint32_t v;
        while (bits < 6) {
            acc |= (uint32_t)b[i++] << bits;
            bits += 8;
        }
        v = acc & 63u;
        acc >>= 6;
        bits -= 6;
        post(C_CMSTEP, s, (int)v);
    }
}
static void ac_roll_w(int st, int what)         /* a step made anew: what 0 all of it, 1 the note, 2 the rhythm (whether it sounds), 3 the accent and the slide */
{
    float dens = (float)par[P_AC_DENS] * 0.01f;
    uint16_t m = (uint16_t)(1u << st);
    if (what == 0 || what == 2) {
        if (frand() < (st == 0 ? fm_minf(dens + 0.3f, 1.0f) : dens))   /* the first step: more often a note */
            ac_gate |= m;
        else
            ac_gate &= (uint16_t)~m;
    }
    if (what <= 1) {
        ac_off[st] = (int8_t)(frand() < 0.8f ? 2 * irand(chord_nt) : irand(nsteps));   /* a chord tone, or now and then not */
        ac_oct[st] = (int8_t)(frand() < 0.55f ? 0 : irand(par[P_AC_RANGE]));
    }
    if (what == 0 || what == 3) {
        if (frand() < 0.3f)
            ac_accent |= m;
        else
            ac_accent &= (uint16_t)~m;
        if (frand() < (float)par[P_AC_SLIDE] * 0.007f)
            ac_slide |= m;
        else
            ac_slide &= (uint16_t)~m;
    }
}
static void ac_roll(int st) { ac_roll_w(st, 0); }

volatile uint8_t en_acid_off[EN_NSTEP], en_acid_oct[EN_NSTEP];
volatile uint16_t en_acid_slide;
static void ac_publish(void)
{
    int s;
    en_acid_gate = ac_gate;
    en_acid_acc = ac_accent;
    en_acid_slide = ac_slide;
    for (s = 0; s < EN_NSTEP; s++) {
        en_acid_off[s] = (uint8_t)ac_off[s];
        en_acid_oct[s] = (uint8_t)ac_oct[s];
    }
    en_acid_len = (uint8_t)par[P_AC_LEN];
}
static void ac_step_set(int s, int off, int oct, int gate, int accent, int slide)   /* the player's edit of a step */
{
    const uint16_t m = (uint16_t)(1u << s);
    ac_off[s] = (int8_t)off;
    ac_oct[s] = (int8_t)oct;
    ac_gate = (uint16_t)(gate ? ac_gate | m : ac_gate & ~m);
    ac_accent = (uint16_t)(accent ? ac_accent | m : ac_accent & ~m);
    ac_slide = (uint16_t)(slide ? ac_slide | m : ac_slide & ~m);
    ac_publish();
}

static void ac_near(int note, int d0, int *po, int *pc)   /* the offset and octave of the line nearest a note, over degree d0 */
{
    int off, oct, bo = 0, bc = 0, best = 999;
    for (oct = 0; oct < 4; oct++)
        for (off = 0; off < 12 && off < 2 * nsteps; off++) {
            int d = d0 + off, o, n, diff;
            d += nsteps * 8;
            o = d / nsteps - 8;
            n = 36 + par[P_ROOT] + steps[d % nsteps] + 12 * (o + oct + par[P_AC_OCT] + par[P_OCT]);   /* (acid_raw, before its register wrap) */
            diff = n > note ? n - note : note - n;
            if (diff < best) {
                best = diff;
                bo = off;
                bc = oct;
            }
        }
    *po = bo;
    *pc = bc;
}
static void ac_capture(int s, int note, int d0, int accent, int slide)   /* Capture: a note played by hand into step s, as the offset and octave nearest it
                                                 * under the chord it was played over (d0; -1: the chord now) and the scale (the line then follows the chords) */
{
    int off, oct;
    ac_near(note, d0 < 0 ? deg : d0, &off, &oct);
    ac_step_set(s, off, oct, 1, accent, slide);
    ac_barsync = 1;
}
static void ac_keep_answer(void)                /* (0.17.31, a Reddit player's "learn what was answered into the track") the answer as the line, frozen */
{
    const int len = par[P_AC_LEN] < 1 ? 1 : par[P_AC_LEN] > EN_NSTEP ? EN_NSTEP : par[P_AC_LEN], v = par[P_AC_RESP], type = v / 5;
    int8_t off[EN_NSTEP], oct[EN_NSTEP];
    uint16_t g = 0, a = 0, sl = 0;
    int st;
    if (type < 1 || nsteps < 1)
        return;
    for (st = 0; st < len; st++) {
        const int ss = acid_src(st, len, type);
        int o, c;
        ac_near(acid_note(ss, v), deg, &o, &c);
        off[st] = (int8_t)o;
        oct[st] = (int8_t)c;
        g |= (uint16_t)(acid_gate(st, len, v) ? 1u << st : 0u);
        a |= (uint16_t)((ac_accent >> ss & 1u) << st);
        sl |= (uint16_t)((ac_slide >> ss & 1u) << st);
    }
    for (st = 0; st < len; st++) {
        ac_off[st] = off[st];
        ac_oct[st] = oct[st];
    }
    ac_gate = g;
    ac_accent = a;
    ac_slide = sl;
    ac_publish();
}

static void ac_regrow(void)
{
    int st;
    for (st = 0; st < EN_NSTEP; st++)
        ac_roll(st);
    ac_publish();
}

static void ac_mutate(void)                     /* the loop's end: Mutate (a deja-vu knob) re-rolls steps: 0 holds the line, 100 renews it every loop */
{
    if (par[P_AC_LOCK]) {                       /* frozen: the line as it is */
        ac_publish();
        return;
    }
    const float k = (float)par[P_AC_MUT] * 0.01f;
    float m = k * k + (par[P_AC_MUT] ? 0.05f * evolve : 0.0f);     /* (squared: 25 is as it was, the top reaches every step) */
    int st;
    for (st = 0; st < par[P_AC_LEN]; st++)
        if (frand() < m)
            ac_roll_w(st, par[P_AC_CHG] > 0 && par[P_AC_CHG] < 4 ? par[P_AC_CHG] : 0);
    ac_publish();
}

/* the acid's note, as the sixteenth chose it, played when its time comes: on the tick, or after the swing and the looseness (ac_wait: samples; the
 * drums' and the pluck's waits are served with it, drums_wait_step) */
static int32_t ac_wait;
static int32_t bl_wait, stab_wait;               /* the bass line's note and the pad's stab, waiting the same way */
static int bl_pn;
static uint8_t bl_pfl, bl_slide;                 /* (0.16.29) the waiting note's accent and glide; a glide sounding */
static float stab_pe;
static void bl_note(int n, int fl);
static void stab_fire(float envv);
static struct { int n; uint8_t slid_ok, acc, hat, slide, beat; float dec; } ac_pend;
static void acid_fire(void)
{
    const int n = ac_pend.acc || ac_pend.slide || ac_pend.beat || (ac_pend.hat && par[P_LK_HAT] > 0) ? avoid_fix(ac_pend.n) : ac_pend.n;
    /* accented (its own accent, or a hat's through the links, 0.17.29), held into the next or on the
     * beat: no clashing fourth over the chord as it is when the note sounds (0.16.26) */
    const int slid = ac_pend.slid_ok && acid.amp_t > 0.0f;
    acid.inc_t = mtof((float)n) * (1.0f / EN_SR);
    if (!slid) {                                /* a new note: the envelope again */
        acid.inc = acid.inc_t;
        acid.env = 1.0f;
        acid.acc = ac_pend.acc ? 1.0f : 0.0f;
        if (ac_pend.hat)                         /* the links: a hat here accents it */
            acid.acc = fm_maxf(acid.acc, (float)par[P_LK_HAT] * 0.012f);
        acid.amp_t = 1.0f;
        acid.edec = fm_expf(-6.9f * 32.0f / (ac_pend.dec * (acid.acc > 0.0f ? 0.6f : 1.0f) * EN_SR));
        if (acid.note)
            midi(0x83, acid.note, 0);
        midi(0x93, (uint32_t)n, acid.acc > 0.0f ? 120u : 90u);
        acid.note = (uint8_t)n;
    }
    acid.sliding = (uint8_t)slid;
    acid.gate_left = ac_pend.slide ? -1 : (int32_t)(0.55f * step_len);
    en_acid_glow = 1.0f;
}
static void acid_step(int32_t wait)             /* a sixteenth of the acid line (wait: the swing and the looseness, samples) */
{
    const int len = par[P_AC_LEN], st = ac_pos;
    int v, type;
    if (st == 0 && ac_resync_loop) {            /* the answer's settings changed: its count starts at this loop */
        ac_loop0 = ac_loop;
        ac_resync_loop = 0;
    }
    if (st == 0 && par[P_AC_CHANCE]) {          /* Sync: Loop: the whole loop is the answer, every 2nd .. 8th time round, or the loop that starts in the bar before the drums change */
        const uint32_t ev = (uint32_t)par[P_AC_EVERY];
        int due = 0;
        if (par[P_AC_RESP] >= 5)
            due = ev >= 7 ? (ac_fill_bar ? 1 : (!par[P_DD_CHANGE] && (ac_loop - ac_loop0) % 8u == 7u)) : (ac_loop - ac_loop0) % (ev + 2u) == ev + 1u;
        if (ac_now && par[P_AC_RESP] >= 5) {    /* asked for: this time round */
            due = 1;
            ac_now = 0;
        }
        ac_ans_loop = en_acid_ans = (uint8_t)due;
    }
    v = (par[P_AC_CHANCE] ? ac_ans_loop : ac_ans_bar) ? par[P_AC_RESP] : 0;
    type = v / 5;
    const int ss = acid_src(st, len, type), prev = type == 5 ? (ss + 1) % len : (ss + len - 1) % len;   /* (the step it reads, and the one played before it) */
    const uint16_t m = (uint16_t)(1u << ss);
    en_acid_pos = (uint8_t)st;
    if (acid.play || par[P_AC_MAN]) {           /* the keys have it: the line steps on, unheard */
    } else if (acid_gate(st, len, v)) {
        if (ac_wait > 0) {                      /* the last note is still waiting (a long push): it sounds now */
            ac_wait = 0;
            acid_fire();
        }
        ac_pend.n = acid_note(ss, v);
        ac_pend.beat = (uint8_t)!(st & 3);
        ac_pend.slid_ok = (uint8_t)((ac_slide >> prev & 1u) && (ac_gate >> prev & 1u));
        ac_pend.dec = 0.06f * fm_exp2f((float)par[P_AC_DEC] * 0.05f);   /* 0.06 .. 1.9 s */
        ac_pend.acc = (uint8_t)((ac_accent & m) != 0u);
        ac_pend.hat = (uint8_t)(lk_hat && links_on());
        ac_pend.slide = (uint8_t)((ac_slide & m) != 0u);
        if (wait > 0)
            ac_wait = wait;
        else
            acid_fire();
    } else if (acid.gate_left < 0) {            /* a slide into a rest: let go */
        acid.gate_left = 0;
        acid.amp_t = 0.0f;
    }
    if (++ac_pos >= len) {
        ac_pos = 0;
        ac_loop++;
        ac_mutate();
    }
}

/* ---------------------------------------------------------------- drums --- */
/* The sequencer: six lanes of sixteen steps, six patterns (en_pats, the UI's to edit). A new pattern
 * (Pattern) waits for the end of the bar, so turning through several on the way plays none of them.
 * SWING delays the off sixteenths.
 *
 * The sections are the player's (en_section, SAVE on the FM-1): a breakdown on the next bar, the drums
 * gone, the drone's filter closing and the pad rising, for as long as it is left; then a build of four
 * bars, the hats back and a snare roll quickening (quarters, eighths, sixteenths), the filter opening;
 * or the drop at once. The drop often brings a new chord. */
enum { L_KICK, L_SNARE, L_CLAP, L_HAT, L_OHAT, L_PERC };
enum { SEC_GROOVE = EN_SEC_GROOVE, SEC_BREAK = EN_SEC_BREAK, SEC_BUILD = EN_SEC_BUILD };
static int sec, sec_bars, sec_req, pat_now, pat_next;   /* sec_bars: a build's bars left; sec_req: cmd + 1 */
static uint32_t bars;                           /* bars played (the drone's envelope counts them) */
static uint8_t fill_hold;
static uint8_t riser_cut;                         /* the drop: the build's riser is cut at once (its high-pass was thinning the first kick) */
static uint8_t fill_q, fill_bar, dj_sync, drop_imp;   /* a fill asked for the next bar; this bar's; the DJ follows the chords; the drop's impact is due */
static uint8_t brk_imp;                         /* (0.17.54) the breakdown's release is due: a last kick, a crash and the bass's root on its one, the echo thrown */
static float brk_throw;                         /* (0.17.54) the echo's throw at a breakdown's start (fx_control takes it once) */
/* SECTIONS by genre (audit M-13): what a breakdown keeps (the kick: house, techno, dub; the hats: drum and bass), how far the pad rises in it (percent),
 * how many bars a build takes (a trap's two, a house's eight), and whether the kick comes back for the build's last bar (the four on the floor) */
/* each genre's sections: what the breakdown keeps (kick, hats, snare, the bass line: a dub-out keeps drums and bass), the build's bars, how far the pad
 * rises in the break, the kick back for the build's last bar; ft: the time plan (0.15.13, the DJ's dubstep): after a drop ft bars as written, ft bars at
 * double time, then as written again, and the build's last two bars double; fl: SF_ROLL the build's last bar ends in 32nds and triplets, SF_WOB the
 * drone's LFO steps through fast rates for 8 bars after the drop, SF_HROLL a fill rolls the hats too */
enum { SF_ROLL = 1, SF_WOB = 2, SF_HROLL = 4, SF_GAP = 8, SF_PBUILD = 16, SF_PFILL = 32, SF_KDROP = 64 };   /* SF_GAP: the whole mix cut for the build's last beat;
                                                 * SF_PBUILD: the build rolls on the perc, not the snare (genres that don't snare-roll); SF_PFILL: the triplet fill on the perc (garage's rim);
                                                 * SF_KDROP: the kick drops out for the last beat of every 8th bar (techno) */
static const struct { uint8_t kick, hat, build, pad, kret, snare, bass, ft, fl; } SECT[EN_NGENRE] = {
    {1, 0, 4, 90, 1, 0, 0, 0, SF_KDROP}, {0, 0, 2, 60, 0, 0, 0, 0, SF_PBUILD}, {1, 0, 8, 90, 1, 0, 0, 0, SF_GAP}, {0, 0, 8, 80, 0, 0, 0, 8, SF_ROLL | SF_WOB | SF_GAP},
    {1, 1, 4, 40, 0, 1, 1, 0, SF_PBUILD}, {0, 1, 8, 90, 0, 0, 0, 0, SF_ROLL | SF_GAP}, {1, 1, 4, 0, 0, 1, 0, 0, 0}, {0, 0, 4, 80, 0, 0, 0, 0, SF_PFILL},
    {0, 0, 4, 90, 1, 0, 0, 0, 0}, {0, 0, 4, 40, 0, 0, 0, 0, SF_ROLL | SF_HROLL | SF_GAP}, {0, 0, 4, 90, 1, 0, 0, 0, SF_GAP}, {1, 1, 4, 0, 0, 1, 0, 0, 0},
    {0, 0, 4, 60, 0, 0, 0, 0, SF_PBUILD}, {0, 0, 4, 90, 1, 0, 0, 0, SF_GAP}, {0, 0, 4, 80, 0, 0, 0, 0, SF_PBUILD}, {0, 0, 2, 60, 0, 0, 0, 0, SF_PBUILD},
    {0, 1, 4, 50, 1, 0, 0, 0, SF_ROLL | SF_HROLL | SF_GAP},   /* industrial: the hats grind on through the break, the kick back for the build's last bar */
    {0, 0, 8, 90, 0, 0, 0, 0, SF_ROLL | SF_GAP},               /* psy: the break all pad and atmosphere, 8 bars of build, the gap before the drop */
    {0, 1, 4, 70, 1, 0, 0, 0, SF_ROLL | SF_HROLL | SF_GAP},    /* dark forest: the hats glitch on through the break */
    {0, 1, 2, 70, 0, 0, 1, 0, SF_GAP},                         /* hiphop: the beat drops out but the hats and the bass, a short build, the gap */
    {0, 0, 8, 90, 0, 0, 0, 0, SF_ROLL | SF_GAP},               /* trance: the long breakdown, all pad, 8 bars of build and its snare roll */
    {0, 0, 2, 90, 0, 0, 0, 0, 0}};                            /* ambient: barely a section: the drums out, the pad on */
/* (0.15.13, from the genre research: breaks and funk break to the drums alone; house, dubstep, DnB, trap, disco and synthwave cut the mix for the beat
 * before the drop; lofi, dub, reggae, amapiano and trip-hop build on the perc, not with a snare roll; garage's triplet fill is on the rim) */
/* the fills each genre may use (0.15.13): bit k is fill kind k (0 the ramp, 1-5 FILLS, 6 a triplet roll on the last beat): house and disco ramp and rush,
 * dub and reggae drag and breathe, trap and dubstep roll */
static const uint8_t FILL_OK[EN_NGENRE] = {0x3F, 0x16, 0x07, 0x49, 0x14, 0x7F, 0x7E, 0x5A, 0x3F, 0x64, 0x87, 0x9E, 0x14, 0x87, 0x12, 0x16, 0x70, 0x47, 0x7E, 0x16, 0x47, 0x12};   /* (bit 7: a tom fill, the perc
                                                 * falling: disco, funk, synthwave) */
static uint8_t gap_on;                          /* the drop gap: the mix cut for the build's last beat (SF_GAP) */
static float gap_g = 1.0f, perc_fall;           /* its gain; a tom fill's pitch, semitones under the perc's */
static uint8_t grv_bars;                        /* bars of groove since the drop (the time plan) */
/* (0.17.27) THE WUB: dubstep's bass (and the Wub sound in any genre), its LFO on the cutoff. On the drop the LFO's rate follows a rhythm a beat at a
 * time over two bars, as a dubstep producer draws it (a piece's own, and every 8 bars its other one); through a build it speeds up, 1/4 to 1/16; else
 * the piece's rate. The beat-locked LFO starts each beat at the top of its cycle at every one of these rates (1/2 only on even beats), so a change is
 * seamless. render_bass draws the sound (wub_on). Was (0.15.13): a different rate each bar for the drop's first 8 bars. */
#define DR_WUB 16                               /* the drone's Wub sound */
static const int8_t WUB_PAT[6][8] = {           /* LFO rates (LFO_RATE_NAME): 0 1/4, 1 1/2, 8 1/8, 9 1/8T, 10 1/16, 11 1/16T */
    {0, 0, 8, 8, 0, 0, 9, 10}, {8, 8, 8, 8, 9, 9, 10, 10}, {1, 1, 8, 8, 1, 1, 10, 10},
    {9, 9, 9, 9, 0, 0, 10, 11}, {0, 8, 0, 8, 0, 8, 10, 10}, {10, 10, 8, 8, 10, 10, 0, 0}};
static int wub_on(void) { return par[P_DL_DEST] == 0 && ((SECT[chord_genre].fl & SF_WOB) || par[P_DR_INST] == DR_WUB); }
static int wub_rate(void)
{
    unsigned base, alt;
    if (!en_playing || !sixteenth || !((SECT[chord_genre].fl & SF_WOB) || par[P_DR_INST] == DR_WUB))   /* (0.17.31: the Wub sound in any genre wobbles
                                                 * to the drop's rhythms too: DnB 2 and 6, industrial 4 and 8 have it) */
        return par[P_DL_RATE];
    if (sec == SEC_BUILD)
        return sec_bars >= 3 ? 0 : sec_bars == 2 ? 8 : 10;
    if (sec != SEC_GROOVE)
        return par[P_DL_RATE];
    base = (unsigned)(par[P_ROOT] * 5 + par[P_TEMPO] + par[P_DR_INST]) % 6u;
    alt = grv_bars ? ((unsigned)grv_bars - 1u) / 8u & 1u : 0u;
    return WUB_PAT[(base + 3u * alt) % 6u][((sixteenth - 1u) >> 2) & 7u];
}
int en_build_len(void) { return SECT[chord_genre].build; }
static int sec_n = 4;                           /* the build's length in bars (from SECT when it starts) */
static uint8_t sec_blen;                        /* (0.17.61) a build asked for with its length (song mode, en_build_bars): from the groove too; a drop asked
                                                 * for so (en_drop): from the groove too (the next place's new piece restarted the section) */
static float snr_rise;                          /* the snare's pitch, semitones: it climbs through a build */
static uint8_t crash_next, fill_kind;           /* the next open hat is the drop's crash (long); this bar's fill */
/* the fills (audit M-13): the snare's velocity on each of the bar's sixteen steps (0: none), nine is full; the second half is what a drift fill plays,
 * the whole bar what a DJ fill or the FX does. Kind 0 is the old ramp (the snare and the hats on every step: in tick), 1..5 are these */
static const uint8_t FILLS[5][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 5, 0, 6, 7, 8, 9},   /* a rush at the end */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 4, 6, 9},   /* a drag: the last beat only */
    {0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 5, 6, 7, 8, 0, 0},   /* a roll and a breath before the bar */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 6, 0, 7, 0, 9},   /* the off-beats */
    {0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 5, 6, 8, 9}};  /* a hit, a gap, then the last beat in sixteenths */
volatile uint8_t en_sec, en_sec_req;
volatile uint8_t en_sec_bars;                   /* a build's bars left (the header's countdown) */
volatile uint8_t en_cm_note, en_cm_count;       /* the pluck's last note and a count of its notes (the sigil's runes) */

/* DRUM DRIFT, as GRAV-1's Drift and LOF-1's: at each bar, a count to the next change; the bar before it a
 * fill (the snare and hats rolling in its second half); then the next pattern, or another from 1..Pool.
 * Mutate, at the same moments: Shift moves a hit or two a step (never the kick's: the floor stays), Morph
 * also adds or takes one. What it changes stays in the pattern (the UI saves en_pats). With Every on Chord,
 * a chord change starts the count: the fill next bar, the change the bar after. */
static int8_t dd_in = -1;                        /* bar lines to the change, -1 none */
static uint8_t dd_fill;                          /* this bar ends in a fill */
static uint32_t dd_bars;
volatile int8_t en_dd_left = -1;
volatile uint32_t en_dd_changes, en_dd_mutations;
static int bits16(uint32_t b) { int n = 0; while (b) { n += (int)(b & 1u); b >>= 1; } return n; }
/* FORM (0.16.26, the genre audit: every piece looped its first pattern for ever). Song (the default): while the player's Drum Drift is off, the piece
 * moves through its own patterns, the next one every SONG_BARS of its genre with a fill in the bar before, as a track would; the patterns it has
 * that differ, up to four. Loop: the first pattern for ever, as before. The player's Drum Drift, when set, is the one that plays. */
volatile uint8_t en_song;
static const uint8_t SONG_BARS[EN_NGENRE] = {16, 8, 8, 16, 16, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 16, 8, 16, 16, 8, 16, 16};   /* hypno, dubstep, dub, trip-hop, psy, forest: 16 */
static int song_pool(void)                      /* the piece's patterns that differ from the ones before them, from the first (2 .. 4; 1: nothing to move to) */
{
    int p, q, l, n = 1;
    for (p = 1; p < 4 && p < EN_NPAT; p++) {
        int same = 0;
        for (q = 0; q < p && !same; q++) {
            for (l = 0; l < EN_NLANE && en_pats[p][l] == en_pats[q][l]; l++)
                ;
            same = l == EN_NLANE;
        }
        if (same)
            break;
        n++;
    }
    return n;
}
static int song_on(void) { return en_song && !par[P_DD_CHANGE] && !par[P_DD_MUTATE] && song_pool() > 1; }
/* THE SONG KEEPS ITS FEEL (0.17.24, the maker's jam on W02: the song walked from four on the floor into the genre's half-time pattern; 88 of the 190
 * pieces had a pattern of another feel among the four). A pattern's feel: the kick on every beat or not, and the backbeat (the snare or the clap on
 * 2 and 4, or on 3). When the song (not the player) has moved to a pattern whose feel is not pattern 0's, its kick, snare and clap lanes play
 * pattern 0's, and only the hats, the open hat and the perc move on. */
static uint8_t song_auto;                        /* the pattern is the song's move, not the player's choice */
static int song_feel_ok(int p)
{
    const int f0 = bits16(en_pats[0][L_KICK] & 0x1111u) == 4, f = bits16(en_pats[p][L_KICK] & 0x1111u) == 4;
    const uint32_t b0 = (en_pats[0][L_SNARE] | en_pats[0][L_CLAP]) & 0x1110u, b = (en_pats[p][L_SNARE] | en_pats[p][L_CLAP]) & 0x1110u;
    return f == f0 && (b == b0 || !b || !b0);
}
static uint8_t lane_src_of(int l)               /* the pattern a lane plays from the bar: its own (P_LSRC), the song's anchor, or the one playing */
{
    if (par[P_LSRC + l])
        return (uint8_t)(par[P_LSRC + l] - 1);
    if (song_auto && l <= L_CLAP && pat_now != 0 && song_on() && !song_feel_ok(pat_now))
        return 0;
    return (uint8_t)pat_now;
}
volatile uint16_t en_played[EN_NLANE];          /* each lane's steps as they sounded, the last sixteen (fills, ghosts, breaks, chances: what the UI shows) */
/* THE ARRANGEMENT (0.17.16, the song-length audit: pluck, bass, acid and drums all played in 97% of bars, nothing ever left and came back). With Song,
 * each phrase (the genre's SONG_BARS, from one pattern change to the next; never the first) may rest the pluck or the acid, or both, for its first or
 * second half (ARR_AMT: percent of phrases), and the bass may sit out the fill bar before a change and land on its one (ARR_BOUT). The pad always plays.
 * A resting part's track is muted as a mute is (5 ms) and its notes are not sent to MIDI. Not in a breakdown or a build, the intro, or a set-list
 * place with its own bars (en_arrange 0). Drawn from its own seed (taken from the piece's at regrow), so the rest of the music is as it was. */
static const uint8_t ARR_AMT[EN_NGENRE] = {30, 45, 60, 50, 50, 55, 55, 55, 55, 50, 55, 45, 40, 50, 55, 40, 50, 45, 40, 45, 55, 30};
static const uint8_t ARR_BOUT[EN_NGENRE] = {0, 25, 40, 50, 35, 30, 30, 40, 30, 40, 30, 25, 30, 25, 30, 25, 30, 25, 25, 30, 40, 20};
volatile uint8_t en_arrange = 1;
volatile uint8_t en_rests = 2;                   /* (0.17.70, the maker: "the bass line cuts out 1 bar every so often", "the acid line cuts out a bit too much in song
                                                 * mode") FORM's Few rests (1: half as often) and No rests (0); the seed is drawn as before, so the music is the same */
static uint8_t arr_m, arr_k, arr_plan, arr_bout, arr_ph;   /* the tracks resting; the bar in the phrase; its plan; the bass out at its end; phrases */
static uint32_t arr_s, arr_chg;                  /* the seed; drum drift's change count at the phrase's start */
static uint32_t arr_rand(uint32_t n) { arr_s = arr_s * 1664525u + 1013904223u; return (arr_s >> 8) % n; }
static void arr_bar(int intro)                   /* at each bar line, after dd_bar */
{
    static const uint8_t PLAN[5] = {1u << EN_TR_PLUCK, 1u << EN_TR_ACID, 1u << EN_TR_PLUCK, 1u << EN_TR_ACID, 1u << EN_TR_PLUCK | 1u << EN_TR_ACID};
    const int L = SONG_BARS[chord_genre];
    if (!song_on() || !en_arrange || sec != SEC_GROOVE || intro) {
        arr_m = 0;
        arr_plan = 0;
        arr_bout = 0;
        arr_chg = en_dd_changes;
        return;
    }
    if (en_dd_changes != arr_chg) {             /* a new phrase: its plan */
        static const uint8_t W[5] = {25, 25, 15, 15, 20};
        arr_chg = en_dd_changes;
        arr_k = 0;
        arr_plan = 0;
        if (arr_ph < 255u)
            arr_ph++;
        if (arr_rand(100) < (uint32_t)ARR_AMT[chord_genre] * en_rests / 2u) {
            uint32_t r = arr_rand(100), i = 0;
            while (i < 4u && r >= W[i])
                r -= W[i++];
            arr_plan = (uint8_t)(i + 1u);
        }
        arr_bout = (uint8_t)(arr_rand(100) < (uint32_t)ARR_BOUT[chord_genre] * en_rests / 2u);
    } else if (arr_k < 255u) {
        arr_k++;
    }
    arr_m = 0;
    if (arr_plan) {
        const int first = arr_plan == 1 || arr_plan == 2 || arr_plan == 5, in_first = arr_k < L / 2;
        if (first == in_first)
            arr_m = PLAN[arr_plan - 1];
    }
    if (arr_bout && dd_fill)
        arr_m |= 1u << EN_TR_DRONE;
    if (!en_rests)                               /* (0.17.70) No rests, from the next bar (a phrase planned before it was chosen rests no more) */
        arr_m = 0;
}
static int dd_len(void)
{
    static const uint8_t L[6] = {0, 4, 8, 16, 32, 64};
    return song_on() ? SONG_BARS[chord_genre] : L[(unsigned)par[P_DD_EVERY] < 6u ? par[P_DD_EVERY] : 0];
}
volatile uint8_t en_lane_src[EN_NLANE];
static uint8_t lane_src[EN_NLANE];              /* the pattern each lane plays (P_LSRC, taken at the bar) */
/* a lane of a pattern as it plays at a density: 50 as written; below, the hits of least importance go first (the off sixteenths, then the off
 * eighths, the quarters last); above, hits arrive on the most important empty steps (up to half of them, at 100). The pattern is never edited. */
uint16_t en_lane_mask(int l, int pat, int dens)
{
    static const uint8_t IMP[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 3, 5, 7, 9, 11, 13, 15};   /* the steps, most important first */
    uint32_t b = en_pats[(unsigned)pat < EN_NPAT ? pat : 0][(unsigned)l < EN_NLANE ? l : 0];
    int pop = bits16(b), n, k;
    if (dens == 50)
        return (uint16_t)b;
    if (dens < 50) {
        n = (pop * (50 - dens) + 25) / 50;
        for (k = 15; k >= 0 && n > 0; k--)
            if (b >> IMP[k] & 1u) {
                b &= ~(1u << IMP[k]);
                n--;
            }
    } else {
        n = ((16 - pop) * (dens - 50) + 50) / 100;
        for (k = 0; k < 16 && n > 0; k++)
            if (!(b >> IMP[k] & 1u)) {
                b |= 1u << IMP[k];
                n--;
            }
    }
    return (uint16_t)b;
}

static void dd_mutate(int p)
{
    static const uint8_t LN[4] = {L_SNARE, L_HAT, L_OHAT, L_PERC};   /* the kick and the clap stay as written */
    int c;
    for (c = 0; c < 2; c++) {                   /* a hit or two, a step either way */
        int l = LN[irand(4)], on[16], m = 0, st;
        uint32_t b = en_pats[p][l];
        const uint32_t keep = l == L_SNARE ? 0x1111u : 0u;   /* the snare on a beat (the backbeat, or 3 in half time) stays where it is */
        for (st = 0; st < 16; st++)
            if ((b & ~keep) >> st & 1u)
                on[m++] = st;
        if (m) {
            int k = on[irand(m)], to = (k + (frand() < 0.5f ? 15 : 1)) & 15;
            if (!(b >> to & 1u))
                en_pats[p][l] = (uint16_t)((b & ~(1u << k)) | 1u << to);
        }
    }
    if (par[P_DD_MUTATE] == 2) {                /* Morph: one more, or one fewer */
        int l = LN[irand(4)], st;
        uint32_t b = en_pats[p][l];
        int h = bits16(b);
        const uint32_t keep = l == L_SNARE ? 0x1111u : 0u, free_ = b & ~keep;   /* (a snare on a beat is never the one taken) */
        if (((frand() < 0.5f && h > 1) || h >= 10) && free_) {
            for (st = irand(16); !(free_ >> st & 1u); st = (st + 1) & 15)
                ;
            b &= ~(1u << st);
        } else if (h < 10) {
            for (st = irand(16); b >> st & 1u; st = (st + 1) & 15)
                ;
            b |= 1u << st;
        }
        en_pats[p][l] = (uint16_t)b;
    }
    en_dd_mutations++;
}

static void dd_bar(void)                        /* at each bar line, before the new pattern is taken */
{
    int len = dd_len();
    const int song = song_on(), chg = song ? 1 : par[P_DD_CHANGE];
    if (!chg && !par[P_DD_MUTATE]) {
        dd_in = en_dd_left = -1;
        dd_fill = 0;
        dd_bars = 0;
        return;
    }
    if (dd_in > 0)
        dd_in--;
    if (dd_in == 0) {
        int pool = song ? song_pool() : par[P_DD_POOL] < 2 ? 2 : par[P_DD_POOL] > EN_NPAT ? EN_NPAT : par[P_DD_POOL], nb = pat_now;
        if (chg == 1)
            nb = pat_now + 1 >= pool ? 0 : pat_now + 1;
        else if (chg == 2)
            do
                nb = irand(pool);
            while (nb == pat_now);
        if (chg) {
            pat_next = nb;
            par[P_PATTERN] = (int16_t)nb;
            song_auto = (uint8_t)song;           /* (the song's move: its feel is kept) */
            en_dd_changes++;
        }
        if (par[P_DD_MUTATE])
            dd_mutate(pat_next);
        dd_in = -1;
        dd_bars = 0;
    } else if (len && dd_in < 0 && ++dd_bars >= (uint32_t)len - 1u) {
        dd_in = 1;
    }
    dd_fill = dd_in == 1 && chg;
    en_dd_left = (int8_t)(dd_in >= 0 ? dd_in : len ? len - (int)dd_bars : -1);
}

static void drum_publish(void)
{
    en_pulse_rest = (uint8_t)(sec == SEC_BREAK);
    en_sec = (uint8_t)sec;
    en_sec_bars = (uint8_t)(sec == SEC_BUILD ? sec_bars : 0);
    en_sec_req = (uint8_t)sec_req;
    en_pat_now = (uint8_t)pat_now;
    en_pat_next = (uint8_t)pat_next;
}

static void section_cmd(int c)                  /* the player's: on the bar */
{
    if (c == EN_CMD_BREAK) {
        if (sec == SEC_GROOVE && en_playing) {  /* (0.17.54, the maker: "the transition into the breakdown is too abrupt"): the groove lets go on the one, not cut */
            brk_imp = 1;
            brk_throw = 0.6f;
        }
        sec = SEC_BREAK;
    } else if (c == EN_CMD_BUILD) {
        if (sec == SEC_BREAK || (sec == SEC_GROOVE && sec_blen)) {   /* (0.17.61: song mode's builds come from the groove too, as long as the place has room) */
            sec = SEC_BUILD;
            sec_n = sec_bars = sec_blen ? sec_blen : SECT[chord_genre].build;
        }
    } else if (sec != SEC_GROOVE || (sec_blen && !drop_imp)) {   /* the drop: at once the full groove (the breakdown's swell and the build's dip gone, not eased out), the kick, clap and open hat on the one */
        sec = SEC_GROOVE;
        brk = 0.0f;
        drop_imp = 1;
        riser_cut = 1;
        drop_tonic = 0;
        grv_bars = 0;
        gap_on = 0;
        if (deg != 0 && nsteps > 1 && frand() < 0.7f) {   /* the drop lands at home, mostly */
            drop_tonic = 1;
            change_pending = 1;
        } else if (frand() < 0.4f + 0.5f * evolve) {
            change_pending = 1;
        }
    }
    sec_blen = 0;
}

static int kick_note(void) { return 28 + (par[P_ROOT] + 8) % 12; }   /* E1 .. Eb2, on the piece's root */

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    const float dec = d == L_OHAT && crash_next ? 1.0f : lane_dec[d], tf = fm_exp2f((lane_tune[d] + (d == L_SNARE ? snr_rise : d == L_PERC ? perc_fall : 0.0f)) * (1.0f / 12.0f));
    if (d == L_OHAT)
        crash_next = 0;
    v->fo = v->on ? v->xl : 0.0f;               /* the new hit starts from 0 (att): the old one fades out from where it was */
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    v->t = 0.0f;
    v->note = DRUM_GM[d];
    v->sp = drum_smp[d];
    if (v->sp) {                                 /* (0.17.36) a sampled kit's: the sample from its start, at the lane's tune; its decay below the middle cuts it */
        v->si = 0;
        v->pr = 0;
        v->ai = v->sp->i0;
        v->ph = 0.0f;
        v->x1 = v->x2 = 0.0f;
        v->inc = (22050.0f / EN_SR) * fm_exp2f((lane_tune[d] + (d == L_SNARE ? snr_rise : d == L_PERC ? perc_fall : 0.0f)) * (1.0f / 12.0f));
        v->adec = dec >= 0.5f ? 1.0f : fm_expf(-6.9f / ((0.03f + 0.9f * dec) * EN_SR));
        if (d == L_KICK && lane_g[L_KICK] > 0.0f)
            duck_env = rd_env = 1.0f;
        if (d == L_HAT)
            drum[L_OHAT].adec = fm_expf(-6.9f / (0.02f * EN_SR));   /* the closed chokes the open, as the synth's */
        return;
    }
    switch (d) {
    case L_KICK: {                              /* a sine dropping fast from (five) times its note */
        float f = mtof((float)kick_note() + lane_tune[0]);
        v->inc = f * KK->k_start * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / ((KK->k_dec0 + KK->k_dec1 * dec) * EN_SR));
        v->ph = 0.0f;
        v->idx = 1.0f;                          /* the click */
        v->x1 = KK->k_rumble;                    /* the rumble: a slow envelope less a fast one */
        v->x2 = KK->k_rumble;
        if (lane_g[L_KICK] > 0.0f)
            duck_env = rd_env = 1.0f;
        break;
    }
    case L_SNARE:                               /* two tuned sines and noise */
        v->inc = KS->sn_f1 * tf * (1.0f / EN_SR);
        v->inc2 = KS->sn_f2 * tf * (1.0f / EN_SR);
        v->ph = v->ph2 = 0.0f;
        v->adec = fm_expf(-6.9f / ((0.08f + 0.3f * dec) * KS->sn_dec * EN_SR));
        v->hp = fm_clampf(0.42f * tf, 0.15f, 0.9f);   /* its noise's band */
        break;
    case L_CLAP:                                /* three bursts of noise, then its tail */
        v->inc = 185.0f * tf * (1.0f / EN_SR);
        v->ph = 0.0f;
        v->adec = fm_expf(-6.9f / ((0.1f + 0.4f * dec) * KS->cl_dec * EN_SR));
        v->inc_end = fm_expf(-6.9f / (0.012f * EN_SR));   /* the bursts' decay */
        v->pend = vel;
        v->hp = fm_clampf(0.18f * tf, 0.08f, 0.5f);
        break;
    case L_HAT:                                 /* closed: it chokes the open one */
        v->adec = fm_expf(-6.9f / ((0.015f + 0.1f * dec) * KH->h_dec * EN_SR));
        v->hp = fm_clampf(0.72f * tf, 0.4f, 1.1f);
        drum[L_OHAT].adec = fm_expf(-6.9f / (0.02f * EN_SR));
        break;
    case L_OHAT:
        v->adec = fm_expf(-6.9f / ((0.08f + 0.6f * dec) * KH->oh_dec * EN_SR));
        v->hp = fm_clampf(0.72f * tf, 0.4f, 1.1f);
        break;
    default: {                                  /* perc: an FM blip, falling into its note */
        float f = KP->p_freq * tf;
        v->inc = f * 2.0f * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->inc2 = KP->p_ratio;                   /* the modulator's ratio */
        v->idx = KP->p_idx;
        v->ph = v->ph2 = 0.0f;
        v->adec = fm_expf(-6.9f / ((0.04f + 0.35f * dec) * EN_SR));
        break;
    }
    }
    if (links_on()) {                           /* the links: the hits into the voices */
        if (d == L_KICK)
            lk_kick = fm_maxf(lk_kick, vel);
        else if (d == L_SNARE || d == L_CLAP)
            lk_snare = fm_maxf(lk_snare, vel);
    }
    en_drum_glow[d] = 1.0f;
    midi_drum(d, (uint32_t)(vel * 100.0f));
}

static void drum_hit(int d, float vel, int32_t wait)   /* wait: a swung sixteenth's delay */
{
    if (wait > 0) {
        drum[d].wait = wait;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

/* A step's ratchet (en_mod): the hit and the hits that follow it inside the step. Each row: the hits to come after the first, the step's
 * division between them (0: a flam's 20 ms), what the first is of the velocity (rf) and each hit of the one before (rm). */
static const struct { uint8_t n, div; float rf, rm; } RATCHET[8] = {
    {0, 0, 1.0f, 1.0f},   {1, 2, 1.0f, 0.8f},  {2, 3, 1.0f, 0.8f},  {3, 4, 1.0f, 0.8f},
    {5, 6, 1.0f, 0.88f},  {1, 0, 0.4f, 2.5f},  {2, 3, 0.5f, 1.4f},  {3, 4, 0.4f, 1.35f}};
static void drum_chain(int d, float vel)        /* a hit of a ratchet has sounded: the next one waits */
{
    drum_t *v = &drum[d];
    if (v->rn) {
        v->rn--;
        v->wait = v->rgap;
        v->pend = vel * v->rmul;
    }
}
static void drum_ratchet(int d, float vel, int32_t wait, int r)   /* wait: the swing and the step's push */
{
    drum_t *v = &drum[d];
    const float v1 = vel * RATCHET[r].rf;
    v->rn = RATCHET[r].n;
    v->rgap = RATCHET[r].div ? (int32_t)(step_len / (float)RATCHET[r].div) : (int32_t)(0.02f * EN_SR);
    v->rmul = RATCHET[r].rm;
    if (wait > 0) {
        v->wait = wait;
        v->pend = v1;
    } else {
        drum_fire(d, v1);
        drum_chain(d, v1);
    }
}

static int32_t cm_wait, cm_pnote;                /* a pluck note that waits for the swing */
static float cm_pvel;
static void cm_trigger(int note, float vel);
/* The pluck on MIDI out, channel 2: every note-on has its note-off booked for when the note has sounded (a plain sound: its decay to silence;
 * a DX voice: the key's time), eight notes at once; the same note again, or the oldest of eight, is let go first; Stop and Panic let go of all. */
#define CM_MN 8
static uint8_t cm_mn[CM_MN];
static int32_t cm_moff[CM_MN];
static void cm_midi_off(int k)
{
    if (cm_mn[k])
        midi(0x81, cm_mn[k], 0);
    cm_mn[k] = 0;
    cm_moff[k] = 0;
}
static void cm_midi_on(int note, uint32_t vel, int32_t len)
{
    int k, slot = -1, oldest = -1;
    for (k = 0; k < CM_MN; k++) {
        if (cm_mn[k] == note)
            cm_midi_off(k);
        if (!cm_mn[k]) {
            if (slot < 0)
                slot = k;
        } else if (oldest < 0 || cm_moff[k] < cm_moff[oldest]) {
            oldest = k;
        }
    }
    if (slot < 0) {
        cm_midi_off(oldest);
        slot = oldest;
    }
    cm_mn[slot] = (uint8_t)note;
    cm_moff[slot] = len;
    midi(0x91, (uint32_t)note, vel);
}
static void cm_midi_step(int32_t k)             /* k samples went by */
{
    int i;
    for (i = 0; i < CM_MN; i++)
        if (cm_mn[i] && (cm_moff[i] -= k) <= 0)
            cm_midi_off(i);
}
static void cm_midi_flush(void)
{
    int i;
    for (i = 0; i < CM_MN; i++)
        cm_midi_off(i);
}
static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = cm_wait > 0 ? cm_wait : 0;
    int d;
    if (ac_wait > 0 && (!m || ac_wait < m))
        m = ac_wait;
    if (bl_wait > 0 && (!m || bl_wait < m))
        m = bl_wait;
    if (stab_wait > 0 && (!m || stab_wait < m))
        m = stab_wait;
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
        if (drum[d].wait2 > 0 && (!m || drum[d].wait2 < m))
            m = drum[d].wait2;
    }
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    if (cm_wait > 0 && (cm_wait -= k) <= 0) {   /* the swung pluck note */
        cm_wait = 0;
        cm_trigger(cm_pnote, cm_pvel);
    }
    if (ac_wait > 0 && (ac_wait -= k) <= 0) {   /* the acid, the bass line and the pad's stab that waited */
        ac_wait = 0;
        acid_fire();
    }
    if (bl_wait > 0 && (bl_wait -= k) <= 0) {
        bl_wait = 0;
        bl_note(bl_pn, bl_pfl);
    }
    if (stab_wait > 0 && (stab_wait -= k) <= 0) {
        stab_wait = 0;
        stab_fire(stab_pe);
    }
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            const float pv = drum[d].pend;
            drum[d].wait = 0;
            drum_fire(d, pv);
            drum_chain(d, pv);
        }
        if (drum[d].wait2 > 0 && (drum[d].wait2 -= k) <= 0) {
            drum[d].wait2 = 0;
            drum_fire(d, drum[d].pend2);
        }
    }
}


/* ------------------------------------------------------ the counter-melody --- */
/* PLUCK: a counter-melody: two plucked FM voices (taking turns, so a tail rings on under the next note) playing an arpeggio of the chord's
 * tones over RANGE octaves, in a STYLE (up, down, up and down, random, skipping, thirds), at a RATE (1/8, 1/16, 1/4, dotted 1/8), a note
 * for each step with the chance DENSITY gives it (fewer in a breakdown). Its sounds are FM at their own ratios: each decays over DECAY. */
typedef struct { float fr, i0, i1; uint8_t rise; float g; } cm_snd_t;
#define CM_NPLAIN 12                            /* the plain FM sounds; from here the DX voices (fm6.inc) */
static const cm_snd_t CM_SOUND[CM_NPLAIN] = {
    {2.0f, 0.2f, 2.2f, 0, 1.0f},     /* Pluck */
    {4.0f, 0.05f, 1.3f, 0, 1.0f},    /* Marimba */
    {1.0f, 0.25f, 0.9f, 0, 1.05f},   /* E.Piano */
    {5.4f, 0.04f, 0.8f, 0, 1.0f},    /* Kalimba */
    {3.5f, 0.15f, 1.8f, 0, 1.0f},    /* Bell */
    {4.01f, 0.05f, 0.5f, 0, 1.0f},   /* Vibes */
    {2.76f, 0.2f, 1.8f, 0, 0.95f},   /* Gong */
    {3.0f, 0.5f, 1.2f, 0, 1.0f},     /* Clav */
    {1.5f, 0.35f, 1.6f, 0, 1.0f},    /* Digital */
    {1.0f, 0.4f, 2.0f, 1, 0.9f},     /* Brass */
    {1.0f, 0.0f, 0.0f, 0, 1.15f},    /* Bleep: a plain sine */
    {7.01f, 0.1f, 1.0f, 0, 0.9f},    /* Glass */
};
#include "fm6_dy.h"                             /* (the DY set's names: the thirty more are on the list as they are) */
const char *EN_CM_SOUND[EN_NCMSND] = {"Pluck", "Marimba", "E.Piano", "Kalimba", "Bell", "Vibes", "Gong", "Clav", "Digital", "Brass", "Bleep", "Glass",
    "DX Tine EP", "DX Glass Bell", "DX Round Bass", "DX Brass Sect", "DX Soft Pad", "DX Wood Bars", "DX Drawbars", "DX Nylon Pick", "DX Brass 2", "DX Solid Bass", "DX Bells", "DX Marimba", "DX Clavinet", "DX Strings", "DX Glass Pad", "DX Sync Lead", "DX Harp", "DX Kalimba", "DX Flute", "DX Steel Drum", "DX Saw Bass", "DX Tubular", "DX Steel Gtr", "DX Clean Gtr", "DX Muted Gtr", "DX Drive Gtr", "DX Slap Bass", "DX Synth Brass", "DX Sax", "DX Mute Trumpet",
    "(empty)", "(empty)", "(empty)", "(empty)", "(empty)", "(empty)", "(empty)", "(empty)",
    "(empty)", "(empty)", "(empty)", "(empty)", "(empty)", "(empty)", "(empty)", "(empty)",
    FM6_DYX_NAME_LIST};
typedef char fm_dyx_names_ok[FM6_NDY == EN_NFMFACT + EN_NFMX ? 1 : -1];
static const char *const FM_DX_NAME[EN_NFMFACT] = {   /* (the same thirty, to put back when the DY set is switched off: en_fm_set_dy) */
    "DX Tine EP", "DX Glass Bell", "DX Round Bass", "DX Brass Sect", "DX Soft Pad", "DX Wood Bars", "DX Drawbars", "DX Nylon Pick", "DX Brass 2", "DX Solid Bass", "DX Bells", "DX Marimba", "DX Clavinet", "DX Strings", "DX Glass Pad", "DX Sync Lead", "DX Harp", "DX Kalimba", "DX Flute", "DX Steel Drum", "DX Saw Bass", "DX Tubular", "DX Steel Gtr", "DX Clean Gtr", "DX Muted Gtr", "DX Drive Gtr", "DX Slap Bass", "DX Synth Brass", "DX Sax", "DX Mute Trumpet"};
const char *const EN_CM_STYLE[EN_NCMSTYLE] = {"Up", "Down", "Up-down", "Random", "Skip", "Thirds"};
const char *const EN_CM_RATE[4] = {"1/4", "3/16", "1/8", "1/16"};   /* slowest to fastest */
static int32_t fm_bend;                          /* a keyboard's bend on the DX pluck, as a Q24 log (fm6's pitch; hb_tick sets it) */
#include "fm6.inc"
/* a keyboard's pitch bend and mod wheel (0.15.26: an MPK mini's) on the voice its keys play: the pluck (plain: its phase; DX: fm_bend), the drone, the acid.
 * Taken once a chunk (hb_tick): hb_semi semitones now (the bend, and the vibrato at 5.5 Hz), hb_pf the frequency's factor; the pad isn't bent. */
static uint8_t hb_voice = 0xFF;
static float hb_bend, hb_mod, hb_ph, hb_semi, hb_pf = 1.0f;

/* VINTAGE (0.17.30; the outside research: the fuller firmwares let each voice wander a little, as an analog one does): a slow random walk of pitch
 * for each of the pad's voices, the bass, the acid and the pluck, each its own (its own random numbers: the music's are untouched). A new target a
 * second or two apart, glided to over about 0.7 s; Vintage (0 .. 100) is the walk's reach, 0 .. 12 cents (30: about 4). */
static float vt_t[7];
static uint32_t vt_rng = 0x2545F491u;
static void vt_tick(uint32_t n)
{
    const float dep = (float)fxs_v[EN_FXS_VINTAGE] * 0.0012f, k = fm_minf((float)n * (1.0f / (0.7f * EN_SR)), 1.0f);
    const uint32_t pc = (uint32_t)((float)n * (65536.0f / (1.4f * EN_SR)));
    int i;
    for (i = 0; i < 7; i++) {
        vt_rng = vt_rng * 1664525u + 1013904223u;
        if ((vt_rng >> 16) < pc) {
            vt_rng = vt_rng * 1664525u + 1013904223u;
            vt_t[i] = (float)(vt_rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
        }
        vt_d[i] = fm_flush(vt_d[i] + (vt_t[i] * dep - vt_d[i]) * k);
    }
    vt_pf[0] = fm_exp2f(vt_d[5] * (1.0f / 12.0f));
    vt_pf[1] = fm_exp2f(vt_d[6] * (1.0f / 12.0f));
}
static void hb_tick(uint32_t n)
{
    vt_tick(n);
    hb_ph += 5.5f * (float)n * (1.0f / EN_SR);
    if (hb_ph >= 1.0f)
        hb_ph -= 1.0f;
    hb_semi = hb_bend + (hb_mod > 0.0f ? 0.5f * hb_mod * sine(hb_ph) : 0.0f);
    hb_pf = hb_semi != 0.0f ? fm_exp2f(hb_semi * (1.0f / 12.0f)) : 1.0f;
    fm_bend = (hb_voice == EN_V_PLUCK ? (int32_t)(hb_semi * (16777216.0f / 12.0f)) : 0) +
              (int32_t)(vt_d[6] * (1200.0f * 4.0f / 12.0f)) * 3495;   /* (the pluck's Vintage in quarter cents: the DX voices' frequencies are figured anew only when it moves) */
}
#define HB(v) (hb_voice == (v))
#define BLK_FM 256                              /* = BLK (checked below) */
#define FM_OUT 0.22f                            /* the DX voices' level against the plain sounds; FM_GAIN: each patch's, to level them */
/* fmcal levels each voice's peak to the plain sounds'; the ear hears loudness, and a voice that sustains sits 30-40 dB above a plain pluck 0.3 s into
 * a note (audit M-65), so the sustaining ones are taken down 8 dB (x0.40; x0.42 nylon and harp, x0.55 steel drum) and the percussive ones stay */
static const float FM_GAIN[EN_NCMFM] = {0.24f, 0.38f, 0.29f, 0.38f, 1.02f, 1.87f, 0.30f, 0.53f, 0.38f, 0.53f, 0.30f, 2.55f, 0.35f, 0.49f, 1.54f, 0.34f, 0.55f, 2.13f, 0.27f, 0.87f, 0.65f, 0.29f,
                                         0.49f, 0.20f, 1.80f, 0.26f, 0.93f, 0.23f, 0.38f, 0.45f,   /* (the eight newer DX voices; the muted guitar's is its peak's, its rms is only a click) */
                                         0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,   /* (the loaded voices: not measured, a sustaining voice's -6 dB) */
                                         /* (0.17.25) the thirty more DY voices: each one's rms in the arpeggio (dycal, at the loaded voices' 0.5) set to the median of the
                                          * first thirty DY voices' levelled rms */
                                         0.26f, 0.25f, 0.35f, 0.20f, 1.10f, 0.24f, 0.55f, 0.32f, 0.26f, 0.50f, 0.55f, 0.50f, 0.44f, 0.33f, 0.26f, 0.22f, 0.33f, 0.57f, 0.23f, 0.48f,
                                         0.16f, 0.47f, 0.28f, 0.28f, 0.82f, 0.16f, 0.36f, 0.36f, 0.21f, 0.36f};   /* (2026-11-13 audit: harpsichord, accordion, oboe,
                                          * pan pipe, koto, cimbalom and banjo in place of Soft Keys, Rock Organ, Poly Brass, Vintage Str, Glide Bass, Synth Bass, Celeste) */
/* the DY voices' gains: each set so the voice's rms in the arpeggio (tests/host/dycal on build/fm6_dy.bin, at the user slots' 0.5) equals the DX
 * voice's in the same slot (fmcal's rms column), then all x1.8 (+5 dB): the DX set sits 6 dB under the plain sounds (audit M-65), and on the unit
 * that read as too quiet against the loaded voices (0.14.13), which play at a flat 0.5, about 1.5 dB under the plain mean */
static const float FM_GAIN_DY[EN_NFMFACT] = {0.45f, 0.29f, 0.22f, 0.59f, 3.73f, 0.67f, 0.40f, 1.15f, 0.70f, 0.52f, 0.52f, 1.64f, 0.29f, 0.49f, 0.77f, 0.29f, 1.17f, 3.35f, 0.94f, 0.59f,
                                              0.23f, 0.20f, 1.37f, 0.35f, 0.92f, 0.34f, 0.76f, 0.23f, 0.40f, 0.43f};   /* (0.17.25: Wood Bars is FLUTEMRMBA, levelled to
                                              * LOG DRUM 1, which played one pitch on every key: at its level, -27.6 dB; 2026-11-13: Clean Gtr is CHOR.STRAT, at GITARA ELE's rms) */
/* the DX voices are rendered by the other core, a chunk ahead: its job renders this chunk's samples into a ring, and the first core plays the ones from
 * FM_D samples before (a chunk), so it never waits for them; the cost is that they sound 5.8 ms late */
#define FM_R 512
#define FM_D 256
static float fm_fifo[FM_R] EN_POOL;
static uint32_t fm_ctr;                         /* samples gone by (the first core's) */
static float fm_ol[BLK_FM] EN_POOL, fm_or[BLK_FM] EN_POOL, fm_rs[BLK_FM] EN_POOL, fm_es[BLK_FM] EN_POOL;   /* the DX voices' part of the mix and the sends */
static struct { float ph, ps, inc, env, dm, vel; uint32_t age; } cm[2];
static float cm_vol;
static int cm_i, cm_alt;
static uint8_t cm_f[32], cm_g[32], cm_ix[32], cm_vl[32];   /* the pluck's loop (deja vu): a slot heard (f), its gate, its place in the chord's tones (octave leap in bit 7 and 6: up, down), its velocity */
static uint32_t cm_k;                           /* the slot the next note is in */
static uint8_t cm_sync;                         /* a captured loop: its slot 0 is the next bar's first step */
/* THE MOTIF (after Tuna's motif engine, GPL-3.0): the first bar of an 8-bar phrase is kept as an idea, which sixteenths sounded and each note's step
 * up or down the chord's ladder from the one before; the bars after develop it as a composer would: the same figure from where the line is
 * (a sequence), upside down (an inversion), with its last interval drawn afresh (a new ending), and, after the half, at its own pitch again (the
 * restatement). Deja vu lies on top of it. P_CM_MOTIF is how often a bar develops rather than invents; a kept idea opens the next phrase too. */
static uint16_t mo_play;                        /* the idea's rhythm: bit s, sixteenth s sounded */
static int8_t mo_k[16];                         /* its intervals: ladder steps from the note before (the first: 0) */
static int8_t mo_last;                          /* its last sounding step, -1: none */
static uint8_t mo_valid, mo_rec, mo_on, mo_inv, mo_alt, mo_restate, mo_first, mo_idx0, mo_prev;   /* kept; being recorded; this bar develops it: inverted, re-ended, restated; the bar's first note is to come; the idea's first rung; the last rung played */
volatile float en_cm_level;

static void cm_trigger(int note, float vel)     /* a note: the next voice, its decay as the Decay knob has it */
{
    const int voice = cm_alt ^= 1;
    en_cm_note = (uint8_t)note;                 /* (for the screen: the sigil's rune at this note) */
    en_cm_count++;
    const float dec = 0.1f * fm_exp2f((float)par[P_CM_DEC] * 0.045f);   /* the time to silence: 0.1 .. 2.2 s */
    const float dk = (float)par[P_CM_DEC] * 0.01f, key_s = 0.12f + 1.6f * dk * dk;   /* a DX voice's key time */
    cm_midi_on(note, (uint32_t)fm_clampf(40.0f + 87.0f * vel, 1.0f, 127.0f), (int32_t)((par[P_CM_SND] >= CM_NPLAIN ? key_s : dec) * EN_SR));
    if (par[P_CM_SND] >= CM_NPLAIN) {            /* a DX voice: the key is down for as long as the Decay knob says (the patch's own envelopes do the rest) */
        fm_note_on(note, vel, (int32_t)(key_s * EN_SR));
        return;
    }
    cm[voice].inc = mtof((float)note) * (1.0f / EN_SR);
    cm[voice].ps = 0.0f;
    cm[voice].env = 1.0f;
    cm[voice].age = 0;
    cm[voice].vel = vel;
    cm[voice].dm = fm_expf(-6.9f / (dec * EN_SR));
}
static void cm_play(int key, float vg)          /* a note played by hand (the JAM page, pads): the key is the pitch, within the song's and the pluck's octaves; vg the velocity / 100 */
{
    int note = key + 12 * (par[P_OCT] + par[P_CM_OCT]);
    while (note > 100)
        note -= 12;
    while (note < 36)
        note += 12;
    cm_trigger(note, fm_minf(0.95f * vg, 1.15f));
}

static int cm_ladder(int *arp)                  /* the chord's tones over the pluck's octaves, low to high: the ladder a slot's index points into */
{
    const int range = par[P_CM_RANGE] < 1 ? 1 : par[P_CM_RANGE] > 3 ? 3 : par[P_CM_RANGE], base = 60 + 12 * (par[P_OCT] + par[P_CM_OCT]);
    int n = 0, k, o, t;
    for (o = 0; o < range; o++)
        for (k = 0; k < chord_nt && n < 12; k++)
            arp[n++] = base + chord_pcs[k] + 12 * o;
    for (k = 1; k < n; k++) {
        t = arp[k];
        for (o = k - 1; o >= 0 && arp[o] > t; o--)
            arp[o + 1] = arp[o];
        arp[o + 1] = t;
    }
    return n;
}

static void cm_capture(int slot, int note)      /* Capture: a note played by hand (or a rest, note < 0) into a slot of the loop, as the nearest rung of the ladder of now, an octave's leap allowed */
{
    int arp[12], n, k, best = 0, leap = 0, bd = 999;
    cm_f[slot] = 1;
    cm_g[slot] = (uint8_t)(note >= 0);
    cm_sync = 1;
    if (note < 0 || chord_nt < 1)
        return;
    n = cm_ladder(arp);
    for (k = 0; k < n; k++) {
        int l;
        for (l = -1; l <= 1; l++) {
            const int c = arp[k] + 12 * l, d = c > note ? c - note : note - c;
            if (d < bd) {
                bd = d;
                best = k;
                leap = l;
            }
        }
    }
    cm_ix[slot] = (uint8_t)(best | (leap > 0 ? 32 : leap < 0 ? 64 : 0) | 128);
    cm_vl[slot] = 230;
}

static void cm_publish(void)                    /* the loop's first sixteen slots, for the screen and the record */
{
    int s;
    for (s = 0; s < 16; s++) {
        const int on = cm_f[s] && cm_g[s];
        en_cm_rung[s] = (uint8_t)(cm_ix[s] & 15);
        en_cm_lvl[s] = (uint8_t)(!on ? 0 : cm_vl[s] < 140 ? 1 : cm_vl[s] < 220 ? 2 : 3);
    }
}
static void cm_step_set(int s, int rung, int level)   /* the player's edit of a step */
{
    cm_f[s] = 1;
    cm_g[s] = (uint8_t)(level > 0);
    cm_ix[s] = (uint8_t)((rung > 11 ? 11 : rung) | 128);
    cm_vl[s] = (uint8_t)(level <= 1 ? 110 : level == 2 ? 190 : 255);
    cm_publish();
}
static void cm_step(int st, int32_t sw)         /* a sixteenth of the counter-melody: st its place in the bar, sw the swing's delay (samples) */
{
    static const uint8_t RATE[4] = {4, 3, 2, 1};   /* sixteenths between notes, by the Rate knob */
    const int ri = par[P_CM_RATE] < 0 ? 0 : par[P_CM_RATE] > 3 ? 3 : par[P_CM_RATE];
    int arp[12], n = 0, note, idx, slot, replay, leap = 0;
    float dens = (float)par[P_CM_DENS] * 0.01f, vel;
    if (st == 0 && cm_sync) {                   /* a captured loop starts with the bar */
        cm_k = 0;
        cm_i = 0;
        cm_sync = 0;
    }
    if (st == 0)
        cm_publish();
    if (st == 0) {                              /* the motif, bar by bar through the phrase of eight */
        const uint32_t ph = bars & 7u;
        const int use = par[P_CM_MOTIF];
        if (mo_rec) {
            mo_valid = (uint8_t)(mo_play != 0);
            mo_rec = 0;
        }
        if (!use) {
            mo_on = 0;
        } else if (ph == 0) {                   /* the phrase's first bar: a new idea (most times), or the kept one opens it again */
            /* (0.17.16, the song-length audit: the pluck's notes echoed least in these genres; a hook comes back) the new idea's chance, a genre */
            static const uint8_t MO_NEW[EN_NGENRE] = {70, 50, 70, 70, 70, 70, 50, 50, 70, 70, 50, 70, 70, 70, 70, 70, 70, 35, 35, 50, 70, 35};
            if (!mo_valid || frand() * 100.0f < (en_song ? (float)MO_NEW[chord_genre] : 70.0f)) {   /* (with Song: a song has hooks) */
                mo_play = 0;
                mo_last = -1;
                mo_rec = 1;
                mo_on = 0;
            } else {
                mo_on = 1;
                mo_inv = mo_alt = 0;
                mo_restate = 1;
            }
        } else {
            mo_restate = (uint8_t)(ph == 4);     /* after the half: the answer restates it at its pitch */
            mo_on = (uint8_t)(mo_valid && (mo_restate || frand() * 100.0f < (float)use));
            mo_inv = (uint8_t)(mo_on && !mo_restate && frand() < 0.2f);
            mo_alt = (uint8_t)(mo_on && !mo_restate && frand() < 0.3f);
        }
        mo_first = 1;
    }
    /* on the bar's grid, whatever has gone before (a stop, a new piece): every 2nd, 1st, 4th sixteenth; the dotted eighth 0 3 6 9 12 each bar */
    if (RATE[ri] == 3 ? (st % 3 != 0 || st > 12) : st % RATE[ri])
        return;
    if (par[P_CM_LVL] == 0 || par[P_CM_MAN] || chord_nt < 1)
        return;
    /* DEJA VU (after Marbles): the notes of the last 4, 8, 16 or 32 are kept, and each comes round again with the chance Deja vu gives (the chord
     * it is played over is the chord of now, so the phrase follows the harmony); 0 makes every note new */
    slot = (int)(cm_k++ % (4u << (par[P_CM_LOOP] < 0 ? 0 : par[P_CM_LOOP] > 3 ? 3 : par[P_CM_LOOP])));
    en_cm_pos = (uint8_t)(slot & 15);
    replay = par[P_CM_VU] > 0 && cm_f[slot] && frand() * 100.0f < (float)par[P_CM_VU];
    if (replay) {
        if (!cm_g[slot] || (sec == SEC_BREAK && frand() >= 0.4f))
            return;
    } else {
        int sound;
        if (sec == SEC_BREAK)
            dens *= 0.4f;
        if (mo_on && !mo_rec)                   /* the motif's rhythm */
            sound = mo_play >> st & 1u;
        else
            sound = frand() < (cm_i == 0 ? fm_minf(dens + 0.3f, 1.0f) : dens);
        cm_f[slot] = 1;
        cm_g[slot] = (uint8_t)sound;
        if (!sound)
            return;
    }
    n = cm_ladder(arp);                         /* the chord's tones over the octaves, low to high */
    if (replay) {
        idx = (cm_ix[slot] & 15) % n;
        leap = cm_ix[slot] & 32 ? 1 : cm_ix[slot] & 64 ? -1 : 0;
        vel = (float)cm_vl[slot] * (1.0f / 255.0f);
    } else if (mo_on && !mo_rec) {              /* the motif's intervals, from where the line is (restated: from the idea's own first rung); inverted, they go the other way; re-ended, the last is drawn afresh */
        int k = mo_k[st];
        if (mo_inv)
            k = -k;
        if (mo_alt && st == mo_last)
            k = irand(5) - 2;
        idx = mo_first ? (mo_restate ? mo_idx0 : mo_prev) : mo_prev + k;
        idx = ((idx % n) + n) % n;
        mo_first = 0;
        vel = 0.75f + 0.25f * frand();
        cm_ix[slot] = (uint8_t)(idx | 128);
        cm_vl[slot] = (uint8_t)(vel * 255.0f);
    } else {
        switch (par[P_CM_STYLE]) {
        case 1: idx = n - 1 - cm_i % n; cm_i++; break;
        case 2: {                                /* up and down, not repeating the ends */
            const int per = n > 1 ? 2 * n - 2 : 1, q = cm_i % per;
            idx = q < n ? q : per - q;
            cm_i++;
            break;
        }
        case 3: idx = irand(n); cm_i++; break;
        case 4: idx = cm_i % n; cm_i += 2; break;
        case 5: idx = cm_i % n; cm_i += 3; break;
        default: idx = cm_i % n; cm_i++; break;
        }
        if (par[P_CM_VU] > 0 && (cm_ix[slot] & 128) && frand() < 0.5f) {   /* a morph: not the note heard here, nor a new one, but its neighbour in the chord's tones */
            idx = ((cm_ix[slot] & 15) % n + (frand() < 0.5f ? 1 : n - 1)) % n;
        }
        if (par[P_CM_LEAP] && frand() * 100.0f < (float)par[P_CM_LEAP])   /* a leap: this note an octave away */
            leap = frand() < 0.7f ? 1 : -1;
        vel = 0.7f + 0.3f * frand();
        cm_ix[slot] = (uint8_t)(idx | (leap > 0 ? 32 : leap < 0 ? 64 : 0) | 128);
        cm_vl[slot] = (uint8_t)(vel * 255.0f);
    }
    if (mo_rec) {                               /* the idea, as it sounds (deja vu's notes too) */
        mo_k[st] = (int8_t)(mo_play ? idx - (int)mo_prev : 0);
        if (!mo_play)
            mo_idx0 = (uint8_t)idx;
        mo_play |= (uint16_t)(1u << st);
        mo_last = (int8_t)st;
    }
    if ((bars & 7u) == 7u && st >= 12 && !par[P_CM_LOCK]) {   /* the phrase's last beat (bar 8 of 8): the chord's root, the rung nearest (0.16.26, the audit) */
        int k, best = idx, bd = 99;
        for (k = 0; k < n; k++)
            if (((arp[k] - chord_pcs[0]) % 12 + 12) % 12 == 0) {
                const int d = k > idx ? k - idx : idx - k;
                if (d < bd) {
                    bd = d;
                    best = k;
                }
            }
        idx = best;
        cm_ix[slot] = (uint8_t)((cm_ix[slot] & 0xF0u) | (uint8_t)idx);
    }
    mo_prev = (uint8_t)idx;
    note = arp[idx] + 12 * leap;
    while (note > 100)
        note -= 12;
    while (note < 36)
        note += 12;
    if (sw > 0) {                               /* the off sixteenths are late, as the drums' are */
        cm_wait = sw;
        cm_pnote = note;
        cm_pvel = vel;
    } else {
        cm_trigger(note, vel);
    }
}


/* ------------------------------------------------------------ the bass line --- */
/* The drone as a bass line (P_BL_MODE): a note on the steps its mode chooses, on the chord's root (or a chord tone, an octave, an approach note), held
 * for Length and let go; each note opens the filter again (the Drone env page's attack and decay). 1 Root: the genre's rhythm. 2 Kick: where the kick
 * hits. 3 Eighths. 4 Off-beat. 5 Walk: a note a beat, root, third or fifth, fifth, then an approach to the next. 6 Riff: the genre's figure.
 * 7 .. 15: a figure by name, whatever the genre (BL_FIG). A figure is a mask of the bar's sixteenths and four intervals, one a hit (the fifth hit
 * takes the first again), so a figure of four hits plays the same each bar. Move is how often a note goes up an octave, to the fifth or to the
 * scale note below the root instead. The fifth is the perfect fifth unless the chord has an altered one (bl_fifth; a 9th, Dub or Fourths chord
 * leaves the fifth out of its voicing, and the line used to take the chord's third tone, the seventh, for it). A breakdown plays none. */
static const uint16_t BL_ROOT[EN_NGENRE] = {0x1111, 0x0421, 0x1111, 0x0401, 0x0501, 0x0441, 0x1121, 0x0449, 0x5555, 0x2401, 0x5555, 0x4529, 0x0149, 0x5555, 0x2449, 0x0401, 0x0405, 0xEEEE, 0xEEEE, 0x0481, 0x4444, 0x0001};
typedef struct { uint16_t mask; int8_t iv[4]; } bl_fig_t;
static const bl_fig_t BL_RIFF[EN_NGENRE] = {
    {0x5555, {0, 0, 12, 0}},  {0x0421, {0, 7, 5, 0}},   {0x4444, {0, 0, 12, 0}},  {0x0401, {0, 0, -2, 0}},  {0x0501, {0, 0, 7, 5}},
    {0x0441, {0, 0, 7, 5}},   {0x1121, {0, 0, 12, 7}},  {0x0449, {0, 7, 0, 12}},  {0x5555, {0, 0, 12, 0}},  {0x2401, {0, 0, 12, 7}},
    {0x5555, {0, 12, 0, 12}}, {0x0509, {0, 12, 7, 0}},  {0x0149, {0, 0, 7, 5}},   {0x5555, {0, 0, 12, 0}},  {0x2449, {0, 0, 7, 0}},   {0x0401, {0, 0, -2, 0}},
    {0x1249, {0, 1, 0, -2}},                     /* industrial: the dotted 3-3-3-3-4 grind on root, flat II, root, flat VII */
    {0xEEEE, {0, 0, 0, 12}}, {0xEEEE, {0, 0, 1, 0}},    /* psy: K-B-B-B, an octave drifting across the beats; dark forest: the flat II through it */
    {0x0481, {0, 0, 7, 0}}, {0x4444, {0, 0, 0, 12}}, {0x0101, {0, 0, 7, 0}}};   /* hiphop: on the kick, the fifth; trance: off the beat; ambient: long */
static const bl_fig_t BL_FIG[EN_NBLFIG] = {      /* the figures by name (the bass line's modes 7 ..): see BL_FIG_NAME */
    {0x5555, {0, 12, 0, 12}},                   /* Octaves: eighths, root and octave (disco) */
    {0x0441, {0, 0, 12, 0}},                    /* Two-step: 1, and-of-2, and-of-3 (garage, drum and bass) */
    {0x0149, {0, 0, 7, 5}},                     /* One-drop: 1, a-of-1, and-of-2, 3 (reggae) */
    {0x2449, {0, 0, 7, 0}},                     /* Log drum: 1, a-of-1, and-of-2, and-of-3, a-of-4 (amapiano) */
    {0x0449, {0, 7, 0, 12}},                    /* Skippy: 1, a-of-1, and-of-2, and-of-3, up to the fifth and the octave */
    {0x0509, {0, 12, 7, 0}},                    /* Funk: 1, a-of-1, 3, and-of-3 */
    {0x1121, {0, 0, 12, 7}},                    /* Big beat: 1, e-of-2, 3, 4 */
    {0x0501, {0, 0, 7, 5}},                     /* Dub: 1, 3, and-of-3 */
    {0x7777, {0, 0, 0, 12}},                    /* Gallop: three sixteenths a beat, an octave moving through them */
    {0xEEEE, {0, 0, 0, 12}},                    /* (0.17.69, modes 17 ..) Rolling: every sixteenth but the beat's, the kick's gap (rolling techno, psy) */
    {0x4949, {0, 0, 7, 0}},                     /* Tresillo: 3 + 3 + 2 sixteenths, twice (dancehall, reggaeton, moombah) */
    {0x8441, {0, 0, 7, 12}},                    /* Push: 1, and-of-2, and-of-3, the last sixteenth up an octave into the next bar */
    {0x4545, {0, 12, 0, 7}},                    /* Bounce: 1, and-of-1, and-of-2, 3, and-of-3, and-of-4 (house, garage) */
};
/* THE BASS STEPS (0.16.29, GitHub issue #6): the bass line's mode 16 plays sixteen steps of the player's (the Bass steps page), a byte a step (amb.h
 * EN_BS_*): a rest, a note, an accent (louder, the filter opened by it as a new note), a glide (into it from the note before, legato: no new
 * attack), or both; its note a number of the scale's steps above the chord's root (so the line follows the chords as the acid's steps do) and
 * an octave. The line the other modes play is kept as it is heard (bs_hist), and the first time Steps plays with no step set it starts from that
 * bar. The steps are the piece's (project.c, OBJ_BSTEP). */
static uint8_t bs[16], bs_set;
volatile uint8_t en_bs_hist[16];                 /* (the screen reads it: the Console look's bass strip) */
#define bs_hist en_bs_hist
volatile uint8_t en_bs[16];
volatile int8_t en_bs_pos = -1;
static void bs_publish(void)
{
    int s;
    for (s = 0; s < 16; s++)
        en_bs[s] = bs[s];
}
static int bs_semis(int k)                      /* k steps of the scale above the chord's root (the key's, on pedal), in semitones */
{
    const int m = nsteps > 0 ? nsteps : 7, d0 = pedal & 1u ? 0 : deg, d = d0 + k;
    return steps[d % m] + 12 * (d / m) - (steps[d0 % m] + 12 * (d0 / m));
}
void en_bass_step(int s, int v) { post(C_BSSTEP, s & 15, v & 0xFF); }
static void bs_publish(void);
static int bl_fit(int n);
static void bs_cap(int s, int note, int d0, int accent, int glide)   /* (0.17.31) Capture: a bass note played by hand into step s, as the scale step and
                                                 * octave nearest it above the root of the chord it was played over (degree d0) */
{
    const int m = nsteps > 0 ? nsteps : 7, d = pedal & 1u ? 0 : d0, rp = (par[P_ROOT] + steps[d % m]) % 12,
              root = bl_fit(36 + rp + 12 * (par[P_BASSOCT] + par[P_OCT])), r0 = steps[d % m] + 12 * (d / m);
    int k, o, bk = 0, bo = 1, best = 999;
    for (o = 0; o < 3; o++)
        for (k = 0; k < 8; k++) {
            const int dk = d + k, n = root + steps[dk % m] + 12 * (dk / m) - r0 + 12 * (o - 1), diff = n > note ? n - note : note - n;
            if (diff < best) {
                best = diff;
                bk = k;
                bo = o;
            }
        }
    bs[s & 15] = EN_BS_MAKE(accent && glide ? 4 : glide ? 3 : accent ? 2 : 1, bk, bo);
    bs_set = 1;
    bs_publish();
}
void en_bass_capture(void) { post(C_BSSTEP, 0x20, 0); }   /* none set: the bar the line played last (the page opened) */

/* THE PHRASE (0.17.45, the maker: "I want to be able to play and record a line that is typically longer than the 16 steps, with held notes and
 * spaces"). The JAM page records up to four bars of what is played on a voice (the pluck, the acid or the bass), each sixteenth a note as
 * pressed (its key, as the page sends it) and how long it was held (bit 7: an accent); here it plays them back, from the bar's first step,
 * through the hand's own path (note_cmd: what the page's keys do), each note let go when its length is up. The UI takes the voice's own line
 * off while it plays (its hand switch). Not stored with the piece. The UI writes en_ph_* only while no phrase plays (en_phrase(-1, 0) first). */
uint8_t en_ph_note[EN_PH_N], en_ph_len[EN_PH_N];
uint16_t en_ph_iv[EN_PH_N];                     /* (0.17.53) a chord on the pluck: its other keys, as played (the acid and the bass play one) */
static uint8_t ph_v = 0xFF, ph_n, ph_pos, ph_arm, ph_cur, ph_left;
static void note_cmd(int voice, int key, int vel);
void en_phrase(int voice, int len) { post(C_PHRASE, voice, len); }
static void ph_release(void)                    /* the note it holds, let go */
{
    if (ph_v != 0xFF && ph_cur)
        note_cmd(ph_v, ph_cur, 0);
    ph_cur = 0;
    ph_left = 0;
}
static void ph_set(int v, int len)
{
    const int late = len >> 8 & 1;              /* (0.17.46) bit 8: ended a little after the bar: from the step it is on now, in its first bar */
    ph_release();
    len &= 0xFF;
    if (v < 0 || len <= 0 || len > EN_PH_N) {
        ph_v = 0xFF;
        return;
    }
    ph_v = (uint8_t)v;
    ph_n = (uint8_t)len;
    ph_pos = 0;
    ph_arm = (uint8_t)(late ? 2 : 1);           /* (from the next bar's first step, or 2: the next step, where it is in the bar) */
}
#ifdef EN_HOST
int en_test_ph(int w) { return w == 0 ? ph_v : w == 1 ? ph_n : ph_cur; }
int en_test_lk(int w);   /* the host tests: the phrase's voice (255 none), length, note sounding */
#endif
static void ph_step(int st)                     /* on every sixteenth */
{
    uint8_t nt, ln;
    if (ph_v == 0xFF || !en_playing)
        return;
    if (ph_arm) {
        if (ph_arm == 1 && st != 0)
            return;
        ph_pos = (uint8_t)(ph_arm == 2 ? st % ph_n : 0);
        ph_arm = 0;
    }
    if (ph_left && !--ph_left)
        ph_release();
    nt = en_ph_note[ph_pos];
    ln = en_ph_len[ph_pos];
    if (nt) {
        ph_release();
        note_cmd(ph_v, nt, ln & 0x80u ? 127 : 100);
        if (ph_v == EN_V_PLUCK) {               /* (0.17.53) the chord as it was played: each key as the page sent it, the same path */
            uint32_t iv = en_ph_iv[ph_pos];
            for (; iv; iv >>= 5)
                if (iv & 31u)
                    note_cmd(ph_v, nt + (int)(iv & 31u) > 127 ? 127 : nt + (int)(iv & 31u), ln & 0x80u ? 127 : 100);
        }
        ph_cur = nt;
        ph_left = (uint8_t)((ln & 0x7Fu) ? (ln & 0x7Fu) : 1u);
    }
    ph_pos = (uint8_t)((ph_pos + 1u) % ph_n);
}
static void bs_capture(void)
{
    int k, v = 0;
    for (k = 0; k < 16; k++)
        v |= bs[k] = bs_hist[k];
    if (!v)
        bs[0] = 1 | 1 << 6;                     /* (it played none: a root on the one) */
    bs_set = 1;
    bs_publish();
}
void en_bass_steps_set(const uint8_t *b)       /* a piece's sixteen (all rests: none set, the next Steps starts from the line heard) */
{
    int s;
    for (s = 0; s < 16; s++)
        post(C_BSSTEP, s | (b[s] ? 0 : 0x10), b[s] | (s == 15 ? 0x100 : 0));   /* (bit 4 of a: a rest that is not the player's; bit 8 of b: the last) */
}
void en_bass_steps_get(uint8_t *b)
{
    int s;
    for (s = 0; s < 16; s++)
        b[s] = en_bs[s];
}
static int bl_fit(int n)
{
    while (n < 24)
        n += 12;
    while (n > 83)
        n -= 12;
    return n;
}
static int bl_below(int root)                   /* the scale note just below the root (a semitone below if the scale has none within two) */
{
    int d;
    for (d = 1; d <= 2; d++)
        if (pool >> (((root - d - par[P_ROOT]) % 12 + 12) % 12) & 1u)
            return root - d;
    return root - 1;
}
static void bl_note(int n, int fl)              /* fl (the steps, 0.16.29): bit 0 an accent, bit 1 a glide into it */
{
    const int glide = (fl & 2) && bass.note >= 1.0f && bl_env > 0.05f;
    n = bl_fit(n);
    bass.target = (float)n;
    if (!glide && (!par[P_BL_GLIDE] || bass.note < 1.0f))
        bass.note = (float)n;
    bl_slide = (uint8_t)glide;
    midi(0x82, bass.midi_note, 0);
    midi(0x92, (uint32_t)n, fl & 1 ? 120u : 90u);
    bass.midi_note = (uint8_t)n;
    bass.vd = fl & 1 ? 0.25f : 0.0f;
    if (!glide) {                               /* (a glide is legato: the note before goes on into it) */
        bass.env_up = 1;
        bl_up = 1;
    }
    bl_gate = (0.25f + 0.0475f * (float)par[P_BL_LEN]) * step_len * (fl & 4 ? 3.0f : 1.0f);   /* (bit 2: the answer's Sparse, held) */
}
static void bl_go_f(int n, int32_t w, int fl)
{
    if (bl_wait > 0) {                          /* the last one is still waiting: it sounds now */
        bl_wait = 0;
        bl_note(bl_pn, bl_pfl);
    }
    if (w > 0) {
        bl_pn = n;
        bl_pfl = (uint8_t)fl;
        bl_wait = w;
    } else {
        bl_note(n, fl);
    }
}
static void bl_rec(int st, int n, int root)     /* the line as heard, as steps: the note's scale steps above the root and its octave (the nearest) */
{
    int iv = n - root, o = 1, k, best = 0, bd = 99;
    while (iv < 0 && o > 0)
        iv += 12, o--;
    while (iv >= 12 && o < 2)
        iv -= 12, o++;
    for (k = 0; k < 8; k++) {
        const int d = bs_semis(k) - iv, ad = d < 0 ? -d : d;
        if (ad < bd)
            bd = ad, best = k;
    }
    bs_hist[st & 15] = (uint8_t)(1 | best << 3 | o << 6);
}
static int bl_fifth(void)                       /* the fifth above the root, semitones: the chord's altered fifth if it has one (diminished, augmented), else the perfect fifth (a 9th chord leaves it out of the voicing, not the harmony), or the scale's if that is not in the scale */
{
    int k;
    for (k = 1; k < chord_nt; k++) {
        const int d = (chord_pcs[k] - chord_pcs[0] + 12) % 12;
        if (d == 7)
            return 7;
        if (d == 6 || d == 8)
            return d;
    }
    return pool >> (((chord_pcs[0] + 7 - par[P_ROOT]) % 12 + 12) % 12) & 1u ? 7 : 6;
}
/* THE BASS'S ANSWER (0.17.30, a Reddit player: "an answer for every voice", the bass first). In its bar the line plays changed: Octave (every second
 * note up an octave), Fifth (every second root to the fifth), Run (the last beat climbs the scale from the root, into the next bar), Busy (a note on
 * every eighth the line leaves empty, root and octave), Sparse (two notes a bar, held), Slide (every note glides from the one before, every second
 * an octave up: legato, as the 303). When: every 2nd .. 8th bar (the last of each), the bar before the drums change, with the acid's answer, or the
 * bar after it (a call and response between the two). A line of any mode but Hold answers; its steps (Steps) too. The settings are the piece's
 * (EN_FXS_BA_*, kept with it as FILTERS are). */
static uint8_t ba_bar, ba_acid_was, ba_half;   /* this bar answers; the acid answered the bar before; Sparse: this half bar has its note */
static int8_t ba_seen_t = -1, ba_seen_e = -1;
static uint32_t ba_bar0;                        /* the bar the count starts from (a change of the settings starts it again) */
volatile uint8_t en_bass_ans;
static int ba_note(int st, int n, int root, int *fl)   /* the note the answer plays for step st (n: the line's note, -1 a rest; -1 back: none) */
{
    const int t = fxs_v[EN_FXS_BA_TYPE];
    if (!ba_bar || t <= 0)
        return n;
    switch (t) {
    case 1: return n >= 0 && (bl_beat & 1) ? n + 12 : n;
    case 2: return n >= 0 && (bl_beat & 1) && ((n - root) % 12 + 12) % 12 == 0 ? root + bl_fifth() : n;
    case 3:
        if (st < 12)
            return n;
        if (st == 15)
            *fl |= 1;                           /* (the top of the run: an accent) */
        return root + bs_semis(st - 11);
    case 4: return n < 0 && !(st & 1) ? root + (st & 2 ? 12 : 0) : n;
    case 5:
        if (st == 0 || st == 8)
            ba_half = 0;
        if (n < 0 || ba_half)
            return -1;
        ba_half = 1;
        *fl |= 4;                               /* (held three times as long) */
        return n;
    case 6:
        if (n < 0)
            return n;
        *fl |= 2;
        return bl_beat & 1 ? n + 12 : n;
    }
    return n;
}
static void bl_out(int st, int n, int root, int32_t w, int fl)   /* the line's step as played: the answer's change, then the note */
{
    n = ba_note(st, n, root, &fl);
    if (n >= 0)
        bl_go_f(n, w, fl);
}
static void bl_step(int st, int32_t w)
{
    const int mode = par[P_BL_MODE], g = chord_genre;
    int hit = 0, iv = 0, root;
    if (!mode || bass.play || par[P_DR_MAN] || !en_playing || !nsteps)
        return;
    if (st == 0)
        bl_beat = 0;
    if (sec == SEC_BREAK && !SECT[g].bass && !(brk_imp && st == 0))
        return;                                 /* (its one: the release's root, 0.17.54) */
    if (mode == EN_BL_STEPS) {                  /* the player's steps */
        int v, k;
        if (!bs_set)                            /* none set: from the bar the line played last (or a root on the one, if it played none) */
            bs_capture();
        en_bs_pos = (int8_t)st;
        v = bs[st & 15];
        k = v & 7;
        if (k == EN_BS_HOLD) {                  /* (0.17.42) held: the note sounding goes on to the next step (no new note, no answer) */
            if (bl_env > 0.05f || bl_wait > 0)
                bl_gate = fm_maxf(bl_gate, 1.05f * step_len + (float)(w > 0 ? w : 0));
            return;
        }
        root = bl_fit(36 + bass_pc() % 12 + 12 * (par[P_BASSOCT] + par[P_OCT]));
        if (!k || k > 4)
            bl_out(st, -1, root, w, 0);         /* (a rest: the answer may play there) */
        else
            bl_out(st, root + bs_semis(v >> 3 & 7) + 12 * ((v >> 6 & 3) - 1), root, w, (k == 2 || k == 4 ? 1 : 0) | (k >= 3 ? 2 : 0));
        if (k && k <= 4)
            bl_beat++;
        return;
    }
    en_bs_pos = -1;
    switch (mode) {
    case 1: hit = BL_ROOT[g] >> st & 1; break;
    case 2: hit = bl_kick && sec != SEC_BUILD; break;
    case 3: hit = !(st & 1); break;
    case 4: hit = (st & 3) == 2; break;
    case 5: hit = !(st & 3); break;
    case 6: hit = BL_RIFF[g].mask >> st & 1; break;
    default: hit = BL_FIG[BL_FIG_OF(mode) % EN_NBLFIG].mask >> st & 1; break;
    }
    if ((drop_imp || brk_imp) && st == 0)
        hit = 1;
    root = bl_fit(36 + bass_pc() % 12 + 12 * (par[P_BASSOCT] + par[P_OCT]));
    if (!hit) {
        bs_hist[st & 15] = 0;
        if (ba_bar)
            bl_out(st, -1, root, w, 0);
        return;
    }
    if (mode == 5) {                            /* the walk (on pedal: the key's own triad, the scale's third and fifth) */
        const int beat = st >> 2,
                  three = pedal & 1u ? root + deg_nat(2) : chord_nt > 1 ? root + (chord_pcs[1] - chord_pcs[0] + 12) % 12 : root + 3,
                  five = pedal & 1u ? root + deg_nat(4) : root + bl_fifth();
        int n = root;
        if (beat == 1)
            n = frand() < 0.5f ? three : five;
        else if (beat == 2)
            n = five;
        else if (beat == 3 && frand() * 100.0f < (float)par[P_BL_MOVE] + 20.0f)
            n = frand() < 0.5f ? root - 1 : bl_below(root);   /* the lead-in to the bar's one: half the time the semitone below, a chromatic approach (0.16.7,
                                                 * the audit's; a passing note, the only one the engine plays outside the scale), else the scale's note below */
        bl_rec(st, n, root);
        bl_out(st, n, root, w, 0);
        bl_beat++;
        return;
    }
    if (mode == 6)
        iv = BL_RIFF[g].iv[bl_beat & 3];
    else if (mode > 6)
        iv = BL_FIG[BL_FIG_OF(mode) % EN_NBLFIG].iv[bl_beat & 3];
    if (frand() * 100.0f < (float)par[P_BL_MOVE]) {   /* a move: the octave, the fifth, or the note below the root */
        const float r = frand();
        iv = r < 0.5f ? 12 : r < 0.8f ? bl_fifth() : bl_below(root) - root;
    }
    bl_rec(st, root + iv, root);
    bl_out(st, root + iv, root, w, 0);
    bl_beat++;
}

static void stab_fire(float envv)
{
    int i;
    stab_env = envv;
    stab_k = fm_expf(-6.9f * 32.0f / (0.08f * fm_exp2f((float)par[P_PE_REL] * 0.04f) * EN_SR));
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->play)                            /* played from the keys: not stabbed */
            continue;
        if (v->midi_note)
            midi(0x80, v->midi_note, 0);
        v->note = v->target;
        v->hand = 0;
        v->hf = 1.0f;
        v->voices = (uint8_t)pad_osc();
        v->midi_note = (uint8_t)v->target;
        midi(0x90, v->midi_note, (uint32_t)(60.0f + 40.0f * stab_env));
    }
}
static int32_t loose(int32_t sw, float ms)      /* the swing's delay and a little more (Humanize, r*r: mostly small; ms at 100) for a voice that plays on the tick */
{
    if (par[P_HUMAN]) {
        const float r = frand();
        sw += (int32_t)(r * r * (float)par[P_HUMAN] * 0.01f * ms * 0.001f * EN_SR);
    }
    return sw;
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static int32_t step_t;                          /* samples into this sixteenth (the gate's clock) */
static uint8_t rpt_arm;                         /* a beat repeat waiting for the next sixteenth: its rate + 1 */
static uint8_t scat_arm, var_exit, rvs_arm;              /* a scatter waiting for the sixteenth; a brake or spin-back let go, leaving on the next */
static void rpt_engage(int r);
static void rpt_refresh(int st);
static void scat_engage(void);
static void var_leave(void);
static void rvs_engage(void);
static void flx_tick(int st);
static uint64_t fx_on;
/* The pad's comping: the steps of the bar its stabs fall on. Comp 2 .. 8 chooses one; Auto takes the genre's (hypnotic: the off-beats, as ever). */
static const uint16_t COMP_MASK[7] = {0x4444, 0x1111, 0x4949, 0x0041, 0x2448, 0x0401, 0x5555};   /* off-beat, on-beat, 3-3-2, Charleston, syncopated, sparse, eighths */
static const uint16_t COMP_GENRE[EN_NGENRE] = {0x4444, 0x0041, 0x4949, 0x0401, 0x4444, 0x0408, 0x0A48, 0x2448, 0x2222, 0x0401, 0x4545, 0x2448, 0x4444, 0x0401, 0x2448, 0x0401, 0x1249, 0x0101, 0x0001, 0x0408, 0x4444, 0x0001};
static uint16_t comp_mask(void)
{
    const int c = par[P_PAD_COMP];
    return c >= 2 ? COMP_MASK[c - 2] : COMP_GENRE[chord_genre];
}
static int comp_hit(int st) { return comp_mask() >> st & 1; }

/* The genre's ghost notes: steps where a soft snare (or kick) may sound that the pattern does not have, with the chance Ghosts gives (at 100: six in ten). */
static const uint16_t GHOST_SN[EN_NGENRE] = {0, 0x8480, 0x0200, 0, 0, 0x8200, 0xC288, 0x0408, 0, 0, 0, 0x8A48, 0, 0, 0, 0x8280, 0x8A20, 0, 0x0420, 0x8480, 0, 0};
static const uint16_t GHOST_KK[EN_NGENRE] = {0, 0x4400, 0, 0, 0, 0, 0x2200, 0, 0, 0, 0, 0x2200, 0, 0, 0, 0x4400, 0x0880, 0, 0, 0x4400, 0, 0};

/* THE PHRASE. Chain: from the bar line the pattern is the chain's next (the bars of 1 2 / 1 1 1 2 / 1 1 2 3 / the Amen's 1 1 3 4 / 1 2 1 3 / 1 2 3 4 / 1 2 3 2 / 1 1 1 5 /
 * all six), so a piece has a phrase of its own and drum drift's changes give way to it. Slice: the drum steps read in another order (halves swapped, a beat stuttered,
 * the bar backwards ...), the drums only. Time: half reads the pattern over two bars, double twice in a bar (an odd step joins the one before). A lane with its own
 * length (polymeter) keeps its steps. Cond: a lane's hits sound only in some bars (odd, even, the first or last of four, the first of eight) or only in fills. */
static const uint8_t CHAIN[9][7] = {{2, 1, 2}, {4, 1, 1, 1, 2}, {4, 1, 1, 2, 3}, {4, 1, 1, 3, 4}, {4, 1, 2, 1, 3}, {4, 1, 2, 3, 4}, {4, 1, 2, 3, 2}, {4, 1, 1, 1, 5}, {6, 1, 2, 3, 4, 5, 6}};
static const uint8_t SLICE[8][16] = {
    {8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7},   {0, 1, 2, 3, 0, 1, 2, 3, 8, 9, 10, 11, 8, 9, 10, 11}, {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 6, 7, 8, 9, 10, 11, 12, 13},    {0, 1, 2, 3, 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 14, 15}, {0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7},        {4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11}};
static uint8_t chain_i, slice_perm;              /* the chain's place; the slice in force this bar (0 none) */
static int pat_bit(uint16_t m, int ls, int poly) /* the lane's hit on step ls of its bar */
{
    const uint32_t ft = SECT[chord_genre].ft;
    const int tm = par[P_TIME] ? par[P_TIME] :   /* the player's Time first; else the genre's time plan */
                   ft && ((sec == SEC_GROOVE && grv_bars > ft && grv_bars <= 2u * ft) || (sec == SEC_BUILD && sec_bars <= 2)) ? 2 : 0;
    if (poly)
        return m >> ls & 1;
    if (tm == 1) {
        if (sixteenth & 1u)                     /* (each step is two sixteenths long: it sounds on the first) */
            return 0;
        ls = (int)(sixteenth >> 1 & 15u);
    } else if (tm == 2) {
        const int s2 = (ls & 7) * 2;
        return ((m >> s2) | (m >> (s2 + 1))) & 1;
    }
    if (slice_perm)
        ls = SLICE[slice_perm - 1][ls & 15];
    return m >> ls & 1;
}
static int cond_ok(int l, int fill)              /* the lane's condition (bars counted from the start; `bars` has moved on at the bar line) */
{
    const uint32_t n = bars - 1u;
    switch (par[P_COND + l]) {
    case 1: return !(n & 1u);
    case 2: return n & 1u;
    case 3: return n % 4u == 0u;
    case 4: return n % 4u == 3u;
    case 5: return n % 8u == 0u;
    case 6: return fill;
    case 7: return !fill;
    default: return 1;
    }
}

static void tick(void)
{
    static const uint8_t TRIG_BARS[6] = {0, 1, 2, 4, 8, 0};
    int l, st = (int)(sixteenth & 15u);
    const float pa = fm_clampf(1.0f + 0.4f * arcm, 0.6f, 1.1f);   /* the arc: the hats a little softer */
    const int32_t sw = (st & 1) ? (int32_t)(fm_minf(swing, 0.5f) * step_len) : 0;
    const float chop = KK->chop * (0.4f + 0.6f * evolve);   /* breakcore: how much the drums are chopped */
    int perc_hit = 0;
    step_t = 0;
    if (scat_arm) {                             /* the scatter too */
        scat_engage();
        scat_arm = 0;
    }
    if (rvs_arm) {                              /* the reverse swell, from the sixteenth */
        rvs_engage();
        rvs_arm = 0;
    }
    flx_tick(st);                               /* Flex Beat: its step, or its start */
    if (var_exit) {                             /* a brake let go, spun up: back to the live mix on the grid */
        var_leave();
        var_exit = 0;
    }
    if (rpt_arm) {                              /* the beat repeat starts on the sixteenth */
        rpt_engage(rpt_arm - 1);
        rpt_arm = 0;
    } else {
        rpt_refresh(st);                        /* the slip refresh: a repeat's slice is the newest again on the beat (or the bar) */
    }
    if (st == 0) {                              /* a bar */
        int tb = TRIG_BARS[(unsigned)par[P_DE_TRIG] < 6u ? par[P_DE_TRIG] : 0];
        fill_bar = fill_q;                      /* the fill the DJ asked for a bar ahead */
        fill_q = 0;
        {   /* this bar's fill, if there is one: one the genre uses (FILL_OK), turning with the bars */
            const uint32_t ok = FILL_OK[chord_genre];
            uint32_t k = (bars + (uint32_t)chord_genre * 2u) % 8u, n;
            for (n = 0; n < 8u && !(ok >> k & 1u); n++)
                k = (k + 1u) % 8u;
            fill_kind = (uint8_t)k;
        }
        if (dj_sync && sec == SEC_GROOVE && en_playing && !en_frozen) {   /* the DJ follows the chords: the bar before a change is a fill, the change's bar a breakdown */
            if (change_pending)
                section_cmd(EN_CMD_BREAK);
            else if (hr_bars() ? hr_left == 1 : drift_total > 0.0f && drift_left <= step_len * 16.0f)
                fill_bar = 1;
        }
        if (sec == SEC_BUILD && --sec_bars <= 0)
            section_cmd(EN_CMD_DROP);           /* the build's end: the drop */
        if (sec_req) {
            section_cmd(sec_req - 1);
            sec_req = 0;
        }
        if (sec == SEC_GROOVE && grv_bars < 255u)
            grv_bars++;                         /* (the drop set it to 0: its first bar is 1) */
        gap_on = 0;
        dd_bar();                               /* drum drift: a fill coming, or a change now */
        arr_bar(intro_len > 0 && intro_t < intro_len);   /* the arrangement: which parts rest this bar */
        {   /* the acid's answer, Sync: Bar: this bar? (every 2nd .. 8th bar, or the bar before the drums change); Sync: Loop is decided where the line starts (acid_step) */
            const int rv = par[P_AC_RESP], ev = par[P_AC_EVERY];
            if (ac_resync_bar) {                /* the answer's settings changed: its count starts at this bar */
                ac_bar0 = bars;
                ac_resync_bar = 0;
            }
            ac_fill_bar = dd_fill;
            if (!par[P_AC_CHANCE]) {
                int due = 0;
                if (rv >= 5) {
                    if (ev >= 7)
                        due = dd_fill ? 1 : (!par[P_DD_CHANGE] && (bars - ac_bar0) % 8u == 7u);   /* (with drum drift not changing patterns: every 8th bar) */
                    else
                        due = (bars - ac_bar0) % ((uint32_t)ev + 2u) == (uint32_t)ev + 1u;
                }
                if (ac_now && rv >= 5) {        /* asked for (SEL + ARP): this bar */
                    due = 1;
                    ac_now = 0;
                }
                ac_ans_bar = en_acid_ans = (uint8_t)due;
            }
        }
        {   /* (0.17.30) the bass's answer: this bar? */
            const int bt = fxs_v[EN_FXS_BA_TYPE], be = fxs_v[EN_FXS_BA_EVERY], an = par[P_AC_CHANCE] ? ac_ans_loop : ac_ans_bar;
            int due = 0;
            if (bt != ba_seen_t || be != ba_seen_e) {   /* the settings changed: the count starts at this bar */
                ba_seen_t = (int8_t)bt;
                ba_seen_e = (int8_t)be;
                ba_bar0 = bars;
            }
            if (bt > 0) {
                if (be >= 0 && be <= 6)
                    due = (bars - ba_bar0) % ((uint32_t)be + 2u) == (uint32_t)be + 1u;
                else if (be == 7)
                    due = dd_fill ? 1 : (!par[P_DD_CHANGE] && (bars - ba_bar0) % 8u == 7u);
                else if (be == 8)
                    due = an;
                else
                    due = ba_acid_was && !an;
            }
            ba_acid_was = (uint8_t)an;
            ba_bar = en_bass_ans = (uint8_t)due;
        }
        if (par[P_CHAIN] > 0 && par[P_CHAIN] <= 9) {   /* the chain: this bar's pattern, whatever was asked */
            const uint8_t *ch = CHAIN[par[P_CHAIN] - 1];
            pat_next = (int)ch[1 + chain_i % ch[0]] - 1;
            chain_i = (uint8_t)((chain_i + 1u) % ch[0]);
        }
        slice_perm = (uint8_t)(par[P_SLICE] == 9 ? (bars % 4u == 3u ? 1 + irand(8) : 0) : par[P_SLICE]);
        pat_now = pat_next;                     /* a new pattern: on the bar */
        for (l = 0; l < EN_NLANE; l++)          /* each lane's own source, from this bar */
            en_lane_src[l] = lane_src[l] = lane_src_of(l);
        if (tb && bars % (uint32_t)tb == 0u)
            bass.env_up = 1;                    /* the drone's envelope: every 1 .. 8 bars */
        bars++;
    }
    drum_publish();
    lk_hat = 0;
    snr_rise = sec == SEC_BUILD ? 7.0f * (float)((sec_n - sec_bars) * 16 + st) / (float)(sec_n * 16) : 0.0f;   /* the snare climbs a fifth through a build */
    if (sec == SEC_BUILD && sec_bars == 1 && st >= 12 && (SECT[chord_genre].fl & SF_GAP))
        gap_on = 1;                             /* the drop gap: the beat before the drop, silence */
    perc_fall = (fill_hold || fill_bar || (dd_fill && sec == SEC_GROOVE)) && fill_kind == 7 && st >= 8 ? -0.9f * (float)(st - 8) : 0.0f;
    for (l = 0; l < EN_NDRUM; l++) {
        const int llen = par[P_LLEN + l] < 16 && par[P_LLEN + l] >= 1 ? par[P_LLEN + l] : 16;
        const int ls = llen < 16 ? (int)(sixteenth % (uint32_t)llen) : st;   /* a shorter lane runs against the bar: polymeter */
        int hit = pat_bit(en_lane_mask(l, lane_src[l], par[P_DENS + l]), ls, llen < 16) && (!par[P_COND + l] || cond_ok(l, fill_hold || fill_bar || (dd_fill && st >= 8))), dbl = 0;   /* dbl: a roll's extra hits (1: one at half a step; 2: two, in triplets; 3: a flam) */
        en_lane_pos[l] = (uint8_t)ls;
        const uint64_t fxm = fx_on;
        float vel = 0.9f;
        int32_t push = 0;
        int rat = 0;
        if (l == L_KICK && sec == SEC_GROOVE && st >= 12 && (SECT[chord_genre].fl & SF_KDROP) && grv_bars % 8u == 0u)
            hit = 0;                            /* techno: the kick out for the last beat of every 8th bar */
        if (sec == SEC_BREAK) {                 /* the breakdown: the drums stop, but for what the genre keeps (the kick, the hats) */
            hit = hit && ((l == L_KICK && SECT[chord_genre].kick) || (l == L_HAT && SECT[chord_genre].hat) || (l == L_SNARE && SECT[chord_genre].snare));
        } else if (sec == SEC_BUILD) {
            if (l == L_PERC && (SECT[chord_genre].fl & SF_PBUILD)) {   /* the build on the perc: eighths, then sixteenths, rising */
                hit = sec_bars > 1 ? (st & 1) == 0 : st < 14;
                vel = 0.3f + 0.6f * ((float)((sec_n - sec_bars) * 16 + st)) / (float)(sec_n * 16);
            } else if (l == L_KICK || l == L_CLAP || l == L_PERC) {
                hit = 0;
                if (l == L_KICK && sec_bars == 1 && SECT[chord_genre].kret && (st == 0 || st == 8))   /* the kick comes back for the last bar: before the drop */
                    hit = 1;
            } else if (l == L_SNARE && (SECT[chord_genre].fl & SF_PBUILD)) {
                hit = 0;
            } else if (l == L_SNARE) {          /* the roll: quarters, eighths, sixteenths, and a breath before the drop */
                hit = sec_bars > 2 ? (st & 3) == 0 : sec_bars == 2 ? (st & 1) == 0 : st < 14;
                vel = 0.3f + 0.7f * ((float)((sec_n - sec_bars) * 16 + st)) / (float)(sec_n * 16);
                if (sec_bars == 1 && (SECT[chord_genre].fl & SF_ROLL) && st >= 8 && st < 14)
                    dbl = st >= 12 ? 2 : 1;     /* the last beats in 32nds, then triplets (dubstep, DnB, trap) */
            } else if (l == L_OHAT && sec_bars == 1) {   /* the last bar: the open hat on every eighth */
                hit = (st & 1) == 0;
                vel = 0.55f;
            }
        } else if (sec == SEC_GROOVE && sec_req == EN_CMD_BREAK + 1 && (l == L_PERC || l == L_OHAT || (l == L_HAT && (st & 1)))) {
            hit = 0;                            /* the break is asked for: the rest of the bar thins out, so it comes as a release */
        }
        if ((fill_hold || fill_bar || (dd_fill && st >= 8 && sec == SEC_GROOVE)) && l == L_PERC && st >= 8 &&
            ((fill_kind == 6 && (SECT[chord_genre].fl & SF_PFILL)) || fill_kind == 7)) {   /* the fill on the perc: the rim in triplets (garage), or a tom falling */
            hit = fill_kind == 7 ? st != 9 && st != 11 : st >= 12 || !(st & 1);
            vel = 0.5f + 0.05f * (float)(st - 8);
            dbl = fill_kind == 6 && st >= 12 ? 2 : 0;
        }
        if ((fill_hold || fill_bar || (dd_fill && st >= 8 && sec == SEC_GROOVE)) && (l == L_SNARE || l == L_HAT)) {   /* FILL; the DJ's; drift's */
            if (!fill_kind) {                   /* the ramp: every step */
                hit = 1;
                vel = l == L_SNARE ? 0.35f + 0.04f * (float)st : 0.5f;
            } else if (l == L_SNARE && st >= 8 && (fill_kind == 7 || (fill_kind == 6 && (SECT[chord_genre].fl & SF_PFILL)))) {
                hit = 0;                        /* (the perc has the fill) */
            } else if (l == L_SNARE && st >= 8 && fill_kind == 6) {   /* a triplet roll: the third beat in eighths, the last in triplets */
                hit = st >= 12 || !(st & 1);
                vel = 0.45f + 0.06f * (float)(st - 8);
                dbl = st >= 12 ? 2 : 0;
            } else if (l == L_SNARE && st >= 8) {   /* one of the others, in the bar's second half; the hats play on */
                const uint8_t fv = FILLS[fill_kind - 1][st];
                hit = fv != 0;
                vel = 0.1f * (float)fv + 0.1f;
            } else if (l == L_HAT && st >= 8 && (SECT[chord_genre].fl & SF_HROLL)) {   /* trap: the hats roll, 32nds then triplets */
                hit = 1;
                vel = (st & 1) ? 0.45f : 0.6f;
                dbl = st >= 12 ? 2 : 1;
            }
        }
        if (chop > 0.0f && sec == SEC_GROOVE && !fill_hold) {   /* breakcore: ghosts, and some hits rolled */
            if (!hit && l == L_SNARE && frand() < 0.16f * chop) {
                hit = 1;
                vel = 0.25f + 0.25f * frand();
            } else if (!hit && l == L_KICK && (st & 1) && frand() < 0.1f * chop) {
                hit = 1;
                vel = 0.7f;
            } else if (l == L_HAT && frand() < 0.25f * chop) {
                hit = !hit;
                vel = 0.5f;
            }
        }
        if (!hit && par[P_GHOST] && sec == SEC_GROOVE && !fill_hold && !fill_bar && !(dd_fill && st >= 8) && (l == L_SNARE || l == L_KICK) &&
            ((l == L_SNARE ? GHOST_SN[chord_genre] : GHOST_KK[chord_genre]) >> ls & 1u) && frand() * 100.0f < 0.6f * (float)par[P_GHOST]) {   /* a ghost note */
            hit = 1;
            vel = 0.22f + 0.2f * frand();
        }
        if (hit && par[P_PROB + l] < 100 && frand() * 100.0f >= (float)par[P_PROB + l])   /* the lane's probability: some hits do not sound */
            hit = 0;
        if (drop_imp && st == 0 && (l == L_KICK || l == L_CLAP || l == L_OHAT)) {   /* the drop lands */
            hit = 1;
            vel = l == L_KICK ? 1.0f : 0.95f;
            if (l == L_OHAT)
                crash_next = 1;                 /* the open hat is a crash: it rings */
        }
        if (brk_imp && st == 0 && (l == L_KICK || l == L_OHAT)) {   /* (0.17.54) the breakdown's release: a last kick and a crash ringing out, then the drums rest */
            hit = 1;
            vel = l == L_KICK ? 0.9f : 0.8f;
            if (l == L_OHAT)
                crash_next = 1;
        }
        if (l == L_KICK) {
            bl_kick = (uint8_t)(hit != 0);
            if (hit)
                kick_age = 0.0f;
        }
        if (l == L_HAT && (fxm & (FXB(FX_HATROLL) | FXB(FX_HATTRIP)))) {     /* HAT ROLL / HAT TRIP: every step, in 32nds or triplets */
            hit = 1;
            vel = (st & 1) ? 0.5f : 0.65f;
            dbl = (fxm & FXB(FX_HATTRIP)) ? 2 : 1;
        }
        if (l == L_SNARE && (fxm & FXB(FX_SNROLL))) {                      /* SN ROLL: every step, louder through the bar, doubled on the odd ones */
            hit = 1;
            vel = 0.3f + 0.045f * (float)st;
            dbl = (st & 1) ? 1 : 0;
        }
        if (hit && (l == L_SNARE || l == L_CLAP) && (fxm & FXB(FX_FLAM)))   /* FLAM: a grace note just before it */
            dbl = 3;
        if (hit && l == L_KICK && (fxm & FXB(FX_DBLKICK)))                  /* DBL KICK: and again half a step on */
            dbl = 1;
        if (hit && sec == SEC_GROOVE && !fill_hold && !fill_bar && !(dd_fill && st >= 8) && !drop_imp) {   /* the step's tricks: its level, push and ratchet (not in a fill, a build or the drop's own hits) */
            static const float LV[4] = {1.0f, 0.4f, 0.7f, 1.12f};
            const uint32_t md = en_mod[l][ls & 15];
            vel = fm_clampf(vel * (1.0f + (LV[md >> 6 & 3u] - 1.0f) * (float)par[P_ACCENT] * 0.02f), 0.05f, 1.0f);   /* (Accents 50: as marked) */
            push = (int32_t)((float)(md >> 3 & 7u) * step_len * (1.0f / 12.0f));
            rat = (int)(md & 7u);
        }
        if (hit && par[P_HUMAN]) {              /* looseness: a little softer, a little late, never on the same terms twice */
            const float hm = (float)par[P_HUMAN] * 0.01f;
            vel *= 1.0f - 0.2f * hm * frand();
            push += (int32_t)(frand() * hm * 0.12f * step_len);
        }
        en_played[l] = (uint16_t)(hit ? en_played[l] | 1u << (ls & 15) : en_played[l] & ~(1u << (ls & 15)));
        if (hit && (l == L_HAT || l == L_OHAT))
            lk_hat = 1;
        if (hit && l == L_PERC)
            perc_hit = 1;
        if (hit) {
            drum[l].rn = 0;                     /* a ratchet still coming ends where a new hit lands */
            if (drum[l].wait > 0) {             /* a pushed hit still waiting sounds now, not lost (one slot a lane) */
                drum[l].wait = 0;
                drum_fire(l, drum[l].pend);
            }
            if (drum[l].wait2 > 0) {
                drum[l].wait2 = 0;
                drum_fire(l, drum[l].pend2);
            }
            vel *= l == L_HAT || l == L_OHAT ? pa : 1.0f;
            if (dbl == 3) {                     /* a flam: the grace note now, the hit 20 ms on */
                drum_fire(l, vel * 0.4f);
                drum[l].wait = (int32_t)(0.02f * EN_SR);
                drum[l].pend = vel;
            } else if (dbl) {                   /* a roll: the hit, and its echoes within the step */
                drum_fire(l, vel);
                drum[l].wait = (int32_t)(step_len * (dbl == 2 ? 0.3333f : 0.5f));
                drum[l].pend = vel * 0.7f;
                if (dbl == 2) {
                    drum[l].wait2 = (int32_t)(step_len * 0.6667f);
                    drum[l].pend2 = vel * 0.6f;
                }
            } else if (rat && !dbl) {           /* the step's ratchet */
                drum_ratchet(l, vel, sw + push, rat);
            } else if (chop > 0.0f && !sw && !push && (l == L_SNARE || l == L_KICK || l == L_HAT) && frand() < 0.2f * chop) {
                drum_fire(l, vel);              /* a 32nd roll: the hit, and again half a step on */
                drum[l].wait = (int32_t)(0.5f * step_len);
                drum[l].pend = vel * 0.7f;
            } else {
                drum_hit(l, vel, sw + push);
            }
        }
    }
    bl_step(st, loose(sw, 4.0f));
    ph_step(st);                                /* (0.17.45) a phrase played back */
    if (pad_stabs() && !par[P_PAD_MAN] && en_playing &&   /* the pad's stabs: a rhythm (Comp; Auto: the genre's), the off-beats with more with Evolve; in a breakdown
                                                           * the rhythm alone, softer (0.15.4: none fired there, so an all-stab genre had the pluck alone) */
        (comp_hit(st) || (sec != SEC_BREAK && comp_mask() == 0x4444u && par[P_PAD_COMP] == 0 && (st == 7 || st == 15) && frand() < 0.35f * evolve))) {
        const int32_t w = loose(sw, 8.0f);
        const float pe = (st & 1 ? 0.7f : 1.0f) * (sec == SEC_BREAK ? 0.8f : 1.0f);
        if (stab_wait > 0) {                    /* the last stab is still waiting: it sounds now */
            stab_wait = 0;
            stab_fire(stab_pe);
        }
        if (w > 0) {
            stab_wait = w;
            stab_pe = pe;
        } else {
            stab_fire(pe);
        }
    }
    en_drum_pos = (uint8_t)st;
    en_deg = (uint8_t)deg;
    if (ac_barsync) {                           /* (0.17.31) a captured line plays where it was played: its step is the bar's */
        ac_barsync = 0;
        ac_pos = st % (par[P_AC_LEN] < 1 ? 1 : par[P_AC_LEN]);
    }
    drop_imp = 0;
    brk_imp = 0;
    (void)perc_hit;                             /* (0.17.50: P>Clock retired, the maker: "it just removes sound"; the perc lane clocked the acid, which
                                                 * in most pieces left it nearly still. P_LK_PERC stays in the pieces, unread.) */
    acid_step(loose(sw, 6.0f));
    cm_step(st, sw);
    if (st == 0 && change_pending && !en_frozen) {   /* chords move on the bar */
        change_pending = 0;
        chord_change();
        if (!par[P_DD_EVERY] && (par[P_DD_CHANGE] || par[P_DD_MUTATE]) && dd_in < 0)
            dd_in = 2;                          /* drum drift with the chord: a fill next bar, the change after */
    }
    if (st == 0 && en_playing && !en_frozen && hr_bars()) {   /* the genre's harmonic rhythm: this bar counted; the chord's last bar asks for the change on the next bar line */
        if (hr_left > 0)
            hr_left--;
        if (hr_left == 0)
            change_pending = 1;
        drift_total = (float)hr_bars() * 16.0f * step_len;   /* the way to the next chord, for the screen and the DJ */
        drift_left = (float)(hr_left + 1) * 16.0f * step_len;
    }
    sixteenth++;
    en_step_frac = 0.0f;                         /* (0.17.48: first, so the UI never sees the new sixteenth with the old one's place) */
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define PRE_D (PRE_N - 1)                            /* the predelay in samples: the tank reads the input this old */
#define PRE_LP (PRE_N + 256)                         /* the line is a chunk longer: a chunk is written while the last one's tank reads */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_LP, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_LP + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
/* The lines are stored as 16-bit integers (the maths stays float): half the memory, 70 KB for other things. Scale 8192: a range of +-4, steps of
 * -78 dBFS, rounded to nearest (a tail below half a step goes quiet rather than hissing). */
#define RV_Q 16384.0f
static int16_t pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    int16_t *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float rv_damp = 0.45f, rv_shim, rv_shim_fb, rv_gate_hold, rg_env, rg_g = 1.0f, rg_hold;   /* the type's settings and state (control sets, the tank uses) */
static int rv_type, rv_pre = 1322;
#define SH_N 2048
#define SH_W 1024.0f
static float sh_buf[SH_N] EN_POOL, sh_ph;                        /* the shimmer's pitch shifter: a delay line, two taps, an octave up */
static uint32_t sh_w;
static float sh_dc;                                                   /* the shimmer's feed, its DC and lowest lows taken out (left in, they built up round the loop until it howled, even after a stop) */
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return (float)d->b[j < 0 ? j + d->n : j] * (1.0f / RV_Q);
}
static inline void push(dl_t *d, float x)
{
    x = fm_clampf(x * RV_Q, -32767.0f, 32767.0f);
    d->b[d->i] = (int16_t)(int32_t)x;           /* (0.17.44: truncated, toward 0: rounded, a dying tail never reached 0 and hissed at -80 dBFS for good) */
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

/* The tank, one sample: x is the input after the predelay. */
static inline float sh_tap(float d)               /* the shimmer line, d samples back, interpolated */
{
    float rp = (float)sh_w - d, f;
    int i0;
    if (rp < 0.0f)
        rp += (float)SH_N;
    i0 = (int)rp;
    f = rp - (float)i0;
    return sh_buf[i0] + (sh_buf[i0 + 1 >= SH_N ? 0 : i0 + 1] - sh_buf[i0]) * f;
}

static inline void reverb_tank(float x, float *ol, float *or_)
{
    float a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    if (rv_type == 2) {                           /* gated: the input opens it, it stays open for the hold and then closes in ~10 ms */
        rg_env = fm_flush(fm_maxf(fm_fabsf(x), rg_env * 0.9995f));
        if (rg_env > 0.004f)
            rg_hold = rv_gate_hold;
        else if (rg_hold > 0.0f)
            rg_hold -= 1.0f;
        rg_g = fm_flush(rg_g + ((rg_hold > 0.0f ? 1.0f : 0.0f) - rg_g) * 0.002f);
    } else if (rv_type == 1) {
        x += rv_shim_fb;
    }
    pl_bw += 0.6f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += rv_damp * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += rv_damp * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
    if (rv_type == 2) {
        *ol *= rg_g;
        *or_ *= rg_g;
    } else if (rv_type == 1) {                    /* shimmer: the tail, an octave up, goes back in (a little) */
        const float m = 0.5f * (*ol + *or_), p2 = sh_ph + 0.5f >= 1.0f ? sh_ph - 0.5f : sh_ph + 0.5f;
        const float w1 = 1.0f - fm_fabsf(2.0f * sh_ph - 1.0f), w2 = 1.0f - fm_fabsf(2.0f * p2 - 1.0f);
        sh_dc += 0.04f * (m - sh_dc);             /* a one-pole high-pass, ~290 Hz (0.17.44: ~100 Hz let Size 100 and X 85+ build back up to a 50 Hz howl after minutes), before the shifter */
        sh_buf[sh_w] = m - sh_dc;
        rv_shim_fb = fm_flush((sh_tap((1.0f - sh_ph) * SH_W + 2.0f) * w1 + sh_tap((1.0f - p2) * SH_W + 2.0f) * w2) * rv_shim);
        sh_ph += 1.0f / SH_W;
        if (sh_ph >= 1.0f)
            sh_ph -= 1.0f;
        if (++sh_w >= SH_N)
            sh_w = 0;
        *ol += rv_shim_fb * 0.7f;                 /* and heard, the octave up, in the tail */
        *or_ += rv_shim_fb * 0.7f;
    }
}

/* The reverb as a job. The input is written to the predelay line a chunk at a time (render_chunk); the tank
 * reads it PRE_D samples old, so a chunk's tank needs none of that chunk's input: it can start with the chunk and run
 * on the other core (en_core2_attach) while this one renders the voices, the drums and the echoes. Same job, same
 * result, whichever core runs it. The line's slots a job reads and the slots being written are apart (PRE_LP). */
#define RV_MAX 256
typedef struct { int n, r0; int fm_on; uint32_t fm_w, pad_w; int bass; } rv_job_t;   /* bass: the job renders the drone and the acid too; pad_w: where the pad's ring is written */
static rv_job_t rv_job;
/* The pad reaches the mix a chunk late (PAD_D frames), as the DX voices do: the job writes this ring and the first core reads it a chunk on, so it
 * never waits for the pad. (Until 0.14.4 it waited at its third stage, 1.2 ms into the chunk, a time the other core could not meet with the chorus
 * to do; the balancer then ran the chorus on the first core, 250-330 us a chunk in most genres, while the other core sat at 30 %.) */
#define PAD_R 512
#define PAD_D 256
static float pad_ring[3][PAD_R] EN_POOL;         /* left, right (after the chorus), the dry sum (the sends' source) */
static uint32_t pad_ctr;                         /* frames the first core has taken from the ring */
static void pad_ring_clear(uint32_t w, uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        const uint32_t q = (w + j) & (PAD_R - 1);
        pad_ring[0][q] = pad_ring[1][q] = pad_ring[2][q] = 0.0f;
    }
}
static float rv_wl[RV_MAX] EN_POOL, rv_wr[RV_MAX] EN_POOL;           /* the tank's output for the chunk */
static int (*c2_submit)(void (*)(void *), void *);
static int (*c2_join)(void);
static void (*c2_fence)(void);
static int (*c2_alive)(void);
uint32_t en_c2_jobs, en_c2_lost;
void en_core2_attach(int (*submit)(void (*)(void *), void *), int (*join)(void), void (*fence)(void), int (*alive)(void))
{
    c2_submit = submit;
    c2_join = join;
    c2_fence = fence;
    c2_alive = alive;
}
volatile uint8_t en_chorus_c0;                   /* the pad's chorus is run by the first core (set when it has time to spare and the other has not) */
static int8_t bal_force = EN_DIAG ? 1 : -1;      /* tests: -1 as it decides, else the level (0.17.64: the diagnostic builds hold level 1, so their runs compare) */
static uint8_t bal_lvl = 1;                      /* where the work runs: 1 the usual (the pad, its chorus, the reverb and the DX voices on the other core); 2 the drone and the acid on the other core as well (0, the chorus on the first core, is gone since 0.14.4) */
static uint32_t bal_wait, bal_tot, bal_dwell, bal_block, bal_up_t, bal_back = 40u;
static uint32_t bal_fr, bal_sum, bal_k, bal_hold, bal_ph, bal_c[2], bal_hmax = 4u;   /* (0.17.57) the A/B: frames summed, the costs of a phase, its windows, the hold, trying, each level's cost */               /* the first core's waiting for the other, and its chunks' time, in ticks since the last decision */
volatile uint8_t en_bass_c1;
static void bal_apply(int l)
{
    if (l < 1)                                  /* level 0 (the chorus on the first core) is gone since 0.14.4: the pad's ring means the first core never waits for it */
        l = 1;
    bal_lvl = (uint8_t)l;
    en_chorus_c0 = 0;
    en_bass_c1 = (uint8_t)(l == 2);
}
void en_balance_force(int v) { bal_force = (int8_t)v; }
static float bass_o[RV_MAX] EN_POOL, acid_o[RV_MAX] EN_POOL, gg_v[RV_MAX] EN_POOL;   /* the drone's and the acid's samples for the chunk, and the gate's gain per sample */
static float bass_duck;                          /* the sidechain's envelope as the chunk began (what the drone's filter follows: the kick moves it later in the chunk) */
static volatile uint32_t stage_done, stage2_done, stage3_done;   /* the job's first stage (the pad) is done: its outputs may be read; the second (the drone, the acid); the third (the reverb's tank) */
static void render_pad(uint32_t n);
static void pad_publish(uint32_t w, uint32_t n);
static void render_bass(uint32_t n);
static void render_acid(uint32_t n);
#ifndef EN_ACID_RAM
#define EN_ACID_RAM 1                            /* (0.17.67) the acid's render runs from RAM (.ram_hot, copied at boot; EN_ACID_RAM=0 at build: from the flash).
                                                 * The maker's A/B (diag 0.17.65 / 0.17.66, TRANCE 01 hands off): late renders 1.48 a second from the flash,
                                                 * 0.16 from RAM, the slowest half 12.8 to 12.2 ms; the acid 343 to 266 us a chunk, its fixed cost a chunk
                                                 * (its code fetched from the flash every chunk) gone. Flattened (no calls) and called through a pointer:
                                                 * a pi32v2 call cannot reach between the flash and RAM. RAMTEXT has about 512 B left after it. */
#endif
#ifndef EN_BASS_RAM
#define EN_BASS_RAM 1                            /* (0.17.68) the bass's render in RAM too, as the acid's (its fixed cost a chunk on the unit: about 230-250 us
                                                 * of its 650-690); EN_BASS_RAM=0 at build: from the flash */
#endif
#if EN_BASS_RAM && !defined(EN_HOST) && !defined(EN_WEB)
static void (*volatile bass_fn)(uint32_t) = render_bass;   /* (called through a pointer, as the acid) */
#define RENDER_BASS(n) bass_fn(n)
#else
#define RENDER_BASS(n) render_bass(n)
#endif
#if EN_ACID_RAM && !defined(EN_HOST) && !defined(EN_WEB)
static void (*volatile acid_fn)(uint32_t) = render_acid;   /* (0.17.64) the acid in RAM: called through a pointer (a call cannot reach RAM from the flash) */
#define RENDER_ACID(n) acid_fn(n)
#else
#define RENDER_ACID(n) render_acid(n)
#endif
static void reverb_job(void *p)
{
    const rv_job_t *c = (const rv_job_t *)p;
    const int16_t *pre = dl[L_PRE].b;
    const int np = dl[L_PRE].n;
    int r = c->r0, j;
    for (j = 0; j < c->n; j++) {
        reverb_tank((float)pre[r] * (1.0f / RV_Q), &rv_wl[j], &rv_wr[j]);
        if (++r >= np)
            r = 0;
    }
}
static void fm_job(const rv_job_t *c)           /* the DX voices' samples for this chunk, into the ring (they are played a chunk later) */
{
    int j;
    if (!c->fm_on) {                             /* not the pluck's sound: the voices let go, nothing sounds */
        for (j = 0; j < FM_NV; j++)
            fmx[j].used = 0;
        for (j = 0; j < c->n; j++)
            fm_fifo[(c->fm_w + (uint32_t)j) & (FM_R - 1)] = 0.0f;
        return;
    }
    for (j = 0; j < c->n; j++)
        fm_fifo[(c->fm_w + (uint32_t)j) & (FM_R - 1)] = fm_next();
}
/* what the other core does in a chunk: the pad's voices (the stage the first core waits for, before it mixes), then the reverb's tank (the first core needs its
 * output at the end of the chunk), then the DX voices (needed a chunk on, so nothing waits for them) */
static void chunk_job(void *p)
{
    /* in the order the first core needs them (0.14.5): the drone and the acid first if they are this core's (wanted after the drums, about 1.5 ms into
     * the chunk), then the tank (wanted before the master, about 3 ms in), then the pad and the DX voices, which are both for the next chunk. With the
     * pad first, the first core waited 80-390 us a chunk for the tank (0.14.4's log). */
    if (((const rv_job_t *)p)->bass) {
        RENDER_BASS((uint32_t)((const rv_job_t *)p)->n);
        RENDER_ACID((uint32_t)((const rv_job_t *)p)->n);
        if (c2_fence)
            c2_fence();
        stage2_done = 1;
    }
    reverb_job(p);
    if (c2_fence)
        c2_fence();
    stage3_done = 1;                            /* the tank's output may be read: the first core goes on to its master while the pad and the DX voices render */
    render_pad((uint32_t)((const rv_job_t *)p)->n);
    pad_publish(((const rv_job_t *)p)->pad_w, (uint32_t)((const rv_job_t *)p)->n);   /* into the ring: the first core takes it a chunk on */
    if (c2_fence)
        c2_fence();
    stage_done = 1;
    fm_job((const rv_job_t *)p);
}

static void plate_init(void)
{
    int16_t *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0;
    pl_bw = pl_damp_l = pl_damp_r = sh_dc = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (up to a bar, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 21800                             /* 2.0 s at 11.025 kHz: a bar at 122 BPM, the longest echo */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2, tp_l1, tp_l2;   /* (tp_l1, tp_l2: its feed's low-pass, 0.17.44) */
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    tp_l1 = fm_flush(tp_l1);                    /* (the tape feed's low-pass, 0.17.44) */
    tp_l2 = fm_flush(tp_l2);
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion)
            head2_t = HEAD2[(int)(frand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y;
    int t = plex_w - PLEX_TAP;
    if (shed3) {                                 /* shed level 3: no Echoplex (the tape alone) */
        *xl = *xr = 0.0f;
        tp_l1 += 0.51f * (in - tp_l1);          /* (0.17.44) the tape's feed low-passed at ~5 kHz twice before it is boxed down to 11 kHz: above ~6 kHz the alias came */
        tp_l2 += 0.51f * (tp_l1 - tp_l2);       /* back louder than the tone (HYPNO 1, PSY 1: -27 dB of alias in the echo; now -38) */
        tape_acc += tp_l2;
        goto tape_part;
    }
    y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tp_l1 += 0.51f * (in + 0.3f * plex_lp - tp_l1);   /* (the tape's feed low-passed, as above) */
    tp_l2 += 0.51f * (tp_l1 - tp_l2);
    tape_acc += tp_l2;
tape_part:
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v + lfo_echo);   /* wow (and an LFO on the echo's time) */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += 0.55f * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + echo_fbx * fb);
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static void tempo_set(void)
{
    step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f);
    duck_dec = fm_expf(-4.0f / (step_len * 3.0f));   /* the duck: back up in about three sixteenths */
}

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    const int was = par[p];
    par[p] = (int16_t)v;
    if (p >= P_LANE && p < P_ECHO) {             /* a drum lane's level, tune or decay */
        int l = (p - P_LANE) / 3, k = (p - P_LANE) % 3;
        if (k == 0)
            lane_g[l] = (float)(v * v) * 0.0001f;
        else if (k == 1)
            lane_tune[l] = (float)v * 0.24f;     /* semitones: an octave either way */
        else
            lane_dec[l] = (float)v * 0.01f;
        return;
    }
    switch (p) {
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_ACID: acid_vol = (float)(v * v) * 0.0001f; break;
    case P_CM_LVL: cm_vol = (float)(v * v) * 0.0001f; break;
    case P_BASS: bass_vol = (float)(v * v) * 0.0001f; break;
    case P_PULSE: pulse_vol = (float)(v * v) * 0.0001f; break;
    case P_NOISE: noise_vol = (float)(v * v) * 0.0001f; break;
    case P_DETUNE: detune = 0.00144f * fm_exp2f((float)v * 0.0332f); break;   /* 2.5 .. 25 cents */
    case P_CM_LOCK: if (v) cm_sync = 1; break;  /* frozen: the line starts with the bar */
    case P_NZ_COLOR: nz_color = v; break;
    case P_NZ_PUMP: nz_pump = (float)v * 0.01f; break;
    case P_BASSOCT:
        if (nsteps)
            bass_to(bass.midi_note % 12);
        break;
    case P_OCT: case P_PAD_OCT:                  /* the pad's voices cross over as for a new key */
        if (nsteps) {
            int i;
            for (i = 0; i < EN_NPAD; i++)       /* a voice's next note: from its register's middle */
                pad[i].target = (float)(55 + 12 * (par[P_PAD_OCT] + par[P_OCT]));
            pad_retarget();
            pad_handoff();
            if (p == P_OCT)
                bass_to(bass.midi_note % 12);
        }
        break;
    case P_AC_RESP:                              /* (0.17.22) a new kind or direction keeps the count, so the answer still comes when it was due (a Reddit
                                                  * player: changing it mid-way pushed the answer back); switched on, the count starts */
        if (was == 0 && v > 0)
            ac_resync_bar = ac_resync_loop = 1;
        break;
    case P_AC_EVERY: case P_STYLE:               /* the answer's count starts again at the next bar and the next loop (it was out of step with them when the settings changed mid-way) */
        if (v != was)
            ac_resync_bar = ac_resync_loop = 1;
        break;
    case P_CHAIN:                                /* the chain starts over at the next bar */
        chain_i = 0;
        break;
    case P_CM_LOOP:                              /* a new length (or a new piece): the loop is forgotten, the next notes are heard afresh */
        if (par[P_CM_LOCK]) {                   /* a frozen pluck line is the piece's own: kept, from the bar */
            cm_sync = 1;
        } else {
            int q;
            for (q = 0; q < 32; q++)
                cm_f[q] = 0;
            cm_k = 0;
            mo_valid = mo_on = mo_rec = 0;      /* (and the motif: a new piece, a new idea) */
        }
        cm_publish();
        break;
    case P_BL_MODE:                              /* back to the drone: the root again; a line starts from silence */
        bl_up = 0;
        bl_env = 0.0f;
        bl_gate = 0.0f;
        if (nsteps)
            bass_to(bass_pc());
        break;
    case P_DR_RES: reso_k = 1.5f - 0.0138f * (float)v; break;   /* none .. near singing (k 1.5 .. 0.12) */
    case P_AC_LEN:
        if (ac_pos >= v)
            ac_pos = 0;
        en_acid_len = (uint8_t)v;
        break;
    case P_AC_DENS: case P_AC_RANGE:             /* heard at once: the line made again (0.17.31: not a frozen one, which a
                                                 * piece load, then its mood, sent here and lost) */
        if (nsteps && !par[P_AC_LOCK])
            ac_regrow();
        break;
    case P_DRIVE: drive_k = 1.0f + 0.04f * (float)v; break;
    case P_SWING: swing = (float)v * 0.005f; break;   /* the off sixteenths up to half a step late */
    case P_PATTERN:                              /* waits for the bar (now, while stopped) */
        pat_next = v;
        song_auto = 0;                           /* (the player's choice: played as it is) */
        if (!en_playing)
            pat_now = v;
        drum_publish();
        break;
    case P_DRIFT:
        if (hr_bars()) {                         /* a harmonic rhythm: a shorter chord takes effect now, a longer one from the next chord */
            const int hb = hr_bars();
            if (hr_left >= hb)
                hr_left = hb - 1;
        } else if (drift_total > 0.0f) {         /* keep the progress, at the new pace */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_SIZE:
        pl_decay_t = 0.6f + 0.375f * (float)v * 0.01f;     /* to 0.975: a long hall */
        echo_fb = 0.5f + 0.32f * (float)v * 0.01f;
        break;
    case P_BRIGHT: bright = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_PADTONE:                              /* the voices cross over as they did for a new key */
        if (stabs_were && !pad_stabs()) {        /* out of the stabs: the breaths start over, overlapping */
            int k;
            for (k = 0; k < EN_NPAD; k++) {
                pad[k].t = START_T[k];
                pad_breathe(&pad[k], k);
            }
        }
        stabs_were = (uint8_t)pad_stabs();
        pad_handoff();
        break;
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_AC_MAN:                               /* the acid off the algorithm: the line's note let go */
        if (v && !acid.play) {
            acid.amp_t = 0.0f;
            acid.gate_left = 0;
        }
        break;
    case P_DR_MAN:                               /* the drone back: to the chord's root */
        if (!v && !bass.play)
            bass_to(bass_pc());
        break;
    case P_KIT_KICK:                             /* the kit: at each drum's next hit */
        KK = &KITS[(unsigned)v < EN_NKIT ? v : 0];
        smp_kit(L_KICK, v, 0);
        break;
    case P_KIT_SNARE:
        KS = &KITS[(unsigned)v < EN_NKIT ? v : 0];
        kit_g[L_SNARE] = kit_db(v, 0);
        kit_g[L_CLAP] = kit_db(v, 1);
        smp_kit(L_SNARE, v, 1);
        smp_kit(L_CLAP, v, 2);
        break;
    case P_KIT_PERC:
        KP = &KITS[(unsigned)v < EN_NKIT ? v : 0];
        kit_g[L_PERC] = kit_db(v, 4);
        smp_kit(L_PERC, v, 5);
        break;
    case P_KIT_HAT: {
        kit_g[L_HAT] = kit_db(v, 2);
        kit_g[L_OHAT] = kit_db(v, 3);
        smp_kit(L_HAT, v, 3);
        smp_kit(L_OHAT, v, 4);
        int k;
        KH = &KITS[(unsigned)v < EN_NKIT ? v : 0];
        for (k = 0; k < 4; k++)
            hat_inc[k] = HAT_INC[k] * KH->h_metal;
        break;
    }    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_ROOT: case P_CHORD: case P_VOICING:   /* the chord again: each voice moves at its next breath */
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            bass_to(bass_pc());
        }
        break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    if (ext_on) {                               /* (0.17.31) the chord from outside, bent into the new scale */
        ext_apply();
        return;
    }
    dom_apply();
    pad_retarget();
    pad_handoff();
    bass_to(bass_pc());
}

static void arc_restart(void);
static void regrow(void)                        /* a new piece: a fresh acid line, the home chord (or the progression's first), an empty progression loop */
{
    pg_i = pg_start = 0;
    deg = pg_on && pg_deg[0] ? (pg_deg[0] - 1) % (nsteps ? nsteps : 7) : 0;
    en_prog_pos = (int8_t)(pg_on ? 0 : -1);
    dh_n = dh_i = hist_n = 0;
    dom_apply();
    pad_retarget();
    pad_handoff();
    bass_to(bass_pc());
    bass.rest = 0;
    if (!par[P_AC_LOCK])                        /* a frozen line is the piece's own (project_apply loaded it) */
        ac_regrow();
    else
        ac_publish();
    ac_pos = 0;
    if (!par[P_CM_LOCK]) {                      /* (0.17.11, track codes) the pluck's loop and motif start empty: what it heard before counts for nothing */
        int k;
        for (k = 0; k < 32; k++)
            cm_f[k] = cm_g[k] = cm_ix[k] = cm_vl[k] = 0;
        cm_k = 0;
        cm_sync = 0;
    }
    cm_i = 0;
    mo_valid = mo_rec = mo_on = mo_inv = mo_alt = mo_restate = mo_first = mo_prev = mo_idx0 = 0;
    mo_play = 0;
    mo_last = -1;
    arc_restart();                              /* the slow arc from the middle, its first tide drawn afresh */
    dd_in = -1;                                 /* drum drift counts its bars from here */
    dd_bars = 0;
    dd_fill = 0;
    arr_m = arr_k = arr_plan = arr_bout = arr_ph = 0;   /* the arrangement from its first phrase, its own seed from the piece's */
    arr_s = rng ^ 0x5BD1E995u;
    arr_chg = en_dd_changes;
    grv_bars = 0;                               /* the time plan, the fills and the drop's tricks too */
    fill_hold = fill_q = fill_bar = drop_imp = 0;
    gap_on = 0;
    gap_g = 1.0f;
    crash_next = fill_kind = 0;
    snr_rise = 0.0f;
    sec = SEC_GROOVE;
    sec_req = 0;
    drum_publish();
    drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    cm_midi_flush();
    midi_all_off();
}

/* KEYS PLAYED ON A VOICE'S PAGE (en_note, from the UI): the pad takes up to four at once, each on one of its
 * voices, which breathes again when let go; the drone and the acid take the last key held (the drone
 * two octaves down, the acid one; a second key while one is held slides to it), the drone's root and the
 * acid's line waiting meanwhile. The voice's octaves and the song's move them, as the generative notes. */
static uint8_t held[3][8], nheld[3];
static uint32_t play_seq;
static int fold_note(int n, int lo, int hi)
{
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}
static void pad_play(int key, int on)
{
    int i, best = -1;
    float bv = 1e9f;
    if (!on) {
        for (i = 0; i < EN_NPAD; i++)
            if (pad[i].play == 1 && pad[i].pnote == key) {
                pad[i].play = 2;
                midi(0x80, pad[i].midi_note, 0);
            }
        return;
    }
    for (i = 0; i < EN_NPAD; i++)               /* a free voice, the quietest; else one ringing out; else the oldest */
        if (pad[i].play == 0 && pad[i].amp < bv)
            bv = pad[i].amp, best = i;
    if (best < 0)
        for (i = 0, bv = 1e9f; i < EN_NPAD; i++)
            if (pad[i].play == 2 && pad[i].pamp < bv)
                bv = pad[i].pamp, best = i;
    if (best < 0)
        for (i = 0, bv = 4e9f; i < EN_NPAD; i++)
            if ((float)pad[i].pord < bv)
                bv = (float)pad[i].pord, best = i;
    {
        pad_t *v = &pad[best];
        if (v->midi_note)
            midi(0x80, v->midi_note, 0);
        v->play = 1;
        v->pnote = (uint8_t)key;
        v->pord = ++play_seq;
        v->note = (float)fold_note(key + 12 * (par[P_PAD_OCT] + par[P_OCT]), 24, 108);
        v->pamp = fm_minf(v->amp / 0.8f, 1.0f);
        v->hand = 0;
        v->hf = 1.0f;
        v->voices = (uint8_t)pad_osc();
        v->midi_note = (uint8_t)v->note;
        midi(0x90, v->midi_note, 100);
    }
}
static void smp_note(int key, int on);
static void note_cmd(int voice, int key, int vel)   /* vel 0: the key let go; 100 a plain press (en_note), 127 the loudest (0.15.23: a pad's or SEL's accent) */
{
    const int on = vel > 0;
    const float vg = (float)vel * 0.01f;
    int k, t, legato;
    if (voice == EN_V_PLUCK) {                  /* the counter-melody played by hand: the note's pitch (the off does nothing: it rings out) */
        if (on)
            cm_play(key, vg);
        return;
    }
    if (voice == EN_V_SAMPLE) {                 /* the sampler: its key (the note less 53) */
        smp_note(key - 53, on);
        return;
    }
    if (voice < 0 || voice > 2)
        return;
    for (k = 0; k < nheld[voice]; k++)          /* the held keys: this one out of the list, and in at the top */
        if (held[voice][k] == key) {
            for (; k + 1 < nheld[voice]; k++)
                held[voice][k] = held[voice][k + 1];
            nheld[voice]--;
            break;
        }
    legato = nheld[voice] > 0;
    if (on) {
        if (nheld[voice] == 8) {
            for (k = 0; k < 7; k++)
                held[voice][k] = held[voice][k + 1];
            nheld[voice] = 7;
        }
        held[voice][nheld[voice]++] = (uint8_t)key;
    }
    t = nheld[voice] ? held[voice][nheld[voice] - 1] : -1;
    en_held = (uint8_t)(nheld[0] + nheld[1] + nheld[2]);
    if (voice == EN_V_PAD) {
        pad_play(key, on);
    } else if (voice == EN_V_DRONE) {
        if (t >= 0) {
            int m = fold_note(t - 24 + 12 * (par[P_BASSOCT] + par[P_OCT]), 24, 83);
            if (on) {                           /* (0.17.69: a key pressed before the last let go too: its envelope had decayed, the note came out
                                                 * nearly silent; the attack climbs from where it is, so no click) */
                bass.env_up = 1;                /* a new press: the envelope again, at its velocity */
                bass.vd = fm_clampf(vg, 0.3f, 1.2f) - 1.0f;
            }
            if (bass.amp < 0.02f)
                bass.note = (float)m;           /* from silence: straight there, no glide */
            if (bass.midi_note != m) {
                midi(0x82, bass.midi_note, 0);
                midi(0x92, (uint32_t)m, (uint32_t)(vel ? vel : 100));
            }
            bass.play = 1;
            bass.target = (float)m;
            bass.midi_note = (uint8_t)m;
        } else if (bass.play) {                 /* let go: back to the chord's root */
            bass.play = 0;
            bass.vd = 0.0f;
            if (par[P_DR_MAN])                  /* (0.17.45) played by hand (JAM, a phrase): the note's MIDI off now, as it fades (it waited for the next note) */
                midi(0x82, bass.midi_note, 0);
            bass_to((int)bass.gtarget % 12);
        }
    } else if (t >= 0) {                        /* the acid */
        int m = fold_note(t - 12 + 12 * (par[P_AC_OCT] + par[P_OCT]), 28, 96);
        acid.inc_t = mtof((float)m) * (1.0f / EN_SR);
        if (legato && acid.play && acid.amp_t > 0.0f) {
            acid.sliding = 1;                   /* legato: a slide, no new attack */
            if (acid.env < 0.6f)                /* (0.17.69) the filter opened again part way: the envelope had closed it and a held-over key's note was */
                acid.env = 0.6f;                /* dull and quiet (the sequencer's own slides go through acid_step, as the 303's) */
        } else {
            float dec = 0.06f * fm_exp2f((float)par[P_AC_DEC] * 0.05f);
            acid.inc = acid.inc_t;
            acid.env = 1.0f;
            acid.acc = vel >= 115 ? 1.0f : 0.6f * vg;   /* (a plain press 0.6, as before; 115 up a full accent) */
            acid.edec = fm_expf(-6.9f * 32.0f / (dec * EN_SR));
            acid.sliding = 0;
        }
        if (acid.note)
            midi(0x83, acid.note, 0);
        midi(0x93, (uint32_t)m, (uint32_t)(vel >= 115 ? 120 : vel));
        acid.note = (uint8_t)m;
        acid.amp_t = fm_minf(1.0f, 0.3f + 0.7f * vg);
        acid.gate_left = -1;                    /* held while the key is */
        acid.play = 1;
        en_acid_glow = 1.0f;
    } else if (acid.play) {
        acid.play = 0;
        acid.amp_t = 0.0f;
        acid.gate_left = 0;
        acid.sliding = 0;
        if (acid.note)
            midi(0x83, acid.note, 0);
    }
}

static void quiet(void)
{
    int i;
    nheld[0] = nheld[1] = nheld[2] = 0;
    en_held = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].play = 0, pad[i].pamp = 0.0f;
    bass.play = 0;
    acid.play = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0, drum[i].rn = 0;
    cm_wait = ac_wait = bl_wait = stab_wait = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].amp = 0.0f;
    bass.amp = 0.0f;
    acid.amp = acid.amp_t = 0.0f;
    notes_off();
}

static void master_dyn_reset(void);
void en_init(void)
{
    int i, l;
    plate_init();
    fm_init();
    fm_reset();
    master_dyn_reset();
    pad_ring_clear(0, PAD_R);
    pad_ctr = 0;
    for (i = 0; i < CM_MN; i++)
        cm_mn[i] = 0, cm_moff[i] = 0;          /* (no pluck notes out) */
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    bass.ph[0] = 0.0f;                           /* (0.17.21) the bass's phases from the same place each start, as the pad's: its low notes' sub now sits on */
    bass.ph[1] = 0.37f;                          /* the note itself, so the saws beat against it, and a render must not depend on where the last one stopped */
    bass.ph[2] = 0.61f;
    bass.sph = bass.mph = 0.0f;
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
        pad[i].ph[2] = 0.61f;
        pad[i].ph[3] = 0.19f;
        pad[i].vib = 0.3f * (float)i;
        pad[i].midi_note = 0;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;
        pad[i].fa = 0.4f;
        pad[i].fh = 0.15f;
        pad[i].fr = 0.35f;
    }
    for (i = 0; i < EN_NPAT; i++)               /* the factory patterns (the app loads the player's after) */
        for (l = 0; l < EN_NLANE; l++)
            en_pats[i][l] = EN_PIECE_PATS[0][i][l];
    arcm = 0.0f;
    pad_gain = 0.0f;
    ext_on = en_extch = 0;                      /* (0.17.31) no chord from outside */
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].note = pad[i].target;
    bass.note = bass.target;
    ac_regrow();
    ac_pos = 0;
    sec = SEC_GROOVE;
    sec_req = 0;
    pat_now = pat_next = 0;
    drum_publish();
    drift_reset();
    pl_decay = pl_decay_t;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        bars = 0;
        ac_pos = 0;
        chain_i = 0;
        slice_perm = 0;
        pat_now = pat_next;
        song_auto = 0;
        for (i = 0; i < EN_NLANE; i++) {
            en_lane_src[i] = lane_src[i] = lane_src_of(i);
            en_played[i] = 0;
        }
        pad_lfo.ph = drone_lfo.ph = 0.0f;
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++)               /* from silence: the breaths start over, overlapping */
        if (pad[i].amp < 1e-4f) {
            pad[i].t = START_T[i];
            pad[i].midi_note = 0;
            pad_breathe(&pad[i], i);
        }
    if (bass.amp < 1e-4f)
        bass.note = bass.target;
    bass.env_up = 1;
    midi(0x92, bass.midi_note, 80);
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = drum[d].wait2 = 0, drum[d].rn = 0;        /* a swung hit still waiting stays unplayed */
    cm_wait = ac_wait = bl_wait = stab_wait = 0;   /* and the swung pluck, acid, bass and stab notes */
    ph_release();                               /* (0.17.45) the phrase's note let go; it starts again from its top with the next bar */
    ph_arm = ph_v != 0xFF;
    en_playing = 0;
    pad_gate = 0.0f;
    acid.amp_t = 0.0f;
    acid.gate_left = 0;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

/* --------------------------------------------------------- performance FX --- */
/* Held effects (en_fx). The master is recorded, always, into a ring of 0.95 s (22.05 kHz, stereo:
 * a half note at 126 BPM; the pool holds no more beside the reverb, the tape and the screen's canvas), which three of them play back instead of the live mix while the engine
 * runs on underneath, in time:
 *   beat repeat   from the next sixteenth, the last 1/2 .. 1/32 note looped (the shortest held wins;
 *                 the ring stops recording while it loops);
 *   reverse       the ring played backwards from now (stopped recording);
 *   tape stop     the live mix slowing to a stop over a beat (still recording, read behind).
 * The rest shape the live mix: mute and solo for six tracks (5 ms ramps), a low-pass and a high-pass
 * closing in over half a second, a trance gate on the synths (1/16, 1/8), an echo throw (everything
 * into the tape echo, its feedback up), a crusher (5.5 kHz, 5 bits), and a drum fill (the pulse's). */
#define RB_N 21000
static int16_t rb[RB_N * 2] EN_POOL;
static uint32_t rb_w;                            /* frames written (they wrap: an index is taken mod RB_N) */
static uint32_t rb_wi;                           /* rb_w mod RB_N, kept by counting (no division in the audio path) */
static uint32_t bm_sm;                           /* bm_start mod RB_N, set with it (SETSTART) */
static uint8_t rb_odd;
static float rb_hl, rb_hr;                       /* the first sample of each pair, waiting for the second */
static uint64_t fx_req;
enum { BM_LIVE, BM_RPT, BM_REV, BM_STOP, BM_SCAT, BM_VAR, BM_SCR, BM_GRN, BM_RVS, BM_FLX };
static int bmode, brate = -1;                    /* the ring's mode; the repeat's rate (FX_RPT2 ..) */
/* the repeat's settings (the Repeat page, 0.14.10): the read's speed (the pitch, climbing to its target over four beats held), the gain (a decay
 * each time round), the gate's share of the slice, the slip refresh (the slice is the newest again each beat or bar, the ring writing on under it) */
static float rp_rate = 1.0f, rp_gain = 1.0f, rp_step = 1.0f, rp_gatef = 1.0f, rp_t, rp_pos, rp_pit;   /* rp_t: beats held; rp_pos: the place in the repeat's interval (output samples); rp_pit: the pitch asked, semitones */
static uint8_t rp_ref, rp_ok;                    /* the refresh asked for; the ring long enough for it with this slice */
static uint32_t bm_start;                        /* the frame the reading counts from */
static float bm_off, bm_len, bm_speed, bm_mix, bm_tgt;   /* samples from bm_start; the slice; 0 live .. 1 ring */
static float trk_g[EN_NTRACK] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
             trk_g1[EN_NTRACK] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static float tg_inv = 1.0f / 256.0f;             /* 1 / the chunk: the tracks' gains ramp across it */
#define TG(t, j) (trk_g[t] + (trk_g1[t] - trk_g[t]) * (float)(j) * tg_inv)
#define TR_KICK EN_TR_KICK
#define TR_HATS EN_TR_HATS
#define TR_SNARE EN_TR_SNARE                     /* snare and clap */
#define TR_PERC EN_TR_PERC
#define TR_DRONE EN_TR_DRONE
#define TR_ACID EN_TR_ACID
#define TR_PAD EN_TR_PAD                         /* pad and vinyl */
#define TR_CM EN_TR_PLUCK                        /* the counter-melody */
static uint8_t trk_mute, trk_solo;               /* latched (en_trk), a bit a track */
uint32_t en_trk_heard(void) { return trk_mute; }
int en_bass_held(void) { return (int)(bass.gtarget + 0.5f); }         /* the bass's held note, and a pad voice's: for the tests (0.16.21) */
int en_pad_held(int i) { return (int)(pad[i % EN_NPAD].target + 0.5f); }        /* the mutes the engine has (a test reads it: 0.16.12) */
static uint16_t gx_pat = 0x5249;                 /* the pattern gate: steps 0 3 6 9 12 14 open (3+3+3+3+2+2) */
const int16_t EN_FXS_DEF[EN_NFXS] = EN_FXS_DEFAULTS;
typedef char fxs_defaults_all[sizeof((int16_t[])EN_FXS_DEFAULTS) == sizeof(int16_t) * EN_NFXS ? 1 : -1];   /* (every setting has its default) */
int en_piece_fx_def(int g, int i, int k)
{
    if (k == EN_FXS_BA_TYPE || k == EN_FXS_BA_EVERY) {   /* (0.17.30) the roles' bass answers: Bass-led runs into every fourth bar, Peak gets busy every eighth */
        const int r = en_piece_role(g, i);
        if (r == RL_BASS)
            return k == EN_FXS_BA_TYPE ? 3 : 2;
        if (r == RL_PEAK)
            return k == EN_FXS_BA_TYPE ? 4 : 6;
    }
    return (unsigned)k < EN_NFXS ? EN_FXS_DEF[k] : 0;
}
static volatile int16_t fxs_v[EN_NFXS] = EN_FXS_DEFAULTS;   /* the effects' settings (en_fxs) */
static float gate_f[EN_NTRACK] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};   /* the pattern gate's gain a track */
static float lp_amt, hp_amt, lp_s[4], hp_s[4], lp_c[4], hp_c[4], throw_amt, throw_key, crush_l, crush_r, gate_g = 1.0f;
static int crush_n;
/* tape, flanger and phaser, on the master after the filters: a tape's wow and flutter (a short delay, its
 * time wobbling) with a little saturation and a duller top; a flanger (a short delay, its time swept, with
 * feedback); a phaser (four all-passes, their corner swept, left and right a quarter turn apart). Each
 * fades in and out (~80 ms) and does nothing, and costs nothing, while it is off. */
#define MOD_N 512
static float tp_buf[2][MOD_N], fl_buf[2][MOD_N];
static uint32_t mod_w;
static float vin_m, roll_t, riser_amt, wash_e;   /* vinyl in; the roll's time held (beats); the build; the wash's total */
static float sw_m, sw_s[2][2], sw_c[4], sw_s2[2][2], sw_c2[4], sw_lt, sw_lf, sw_k1, sw_k2;   /* (0.17.18: the corner's target and its gliding place, log2 Hz; the two ks) */
static uint32_t sw_cnt;
static float tk_m, tk_c[2][4], tk_s[2][2][2], wh_m, wh_c[4], wh_s[2][2], wh_env;   /* talk (two formants a channel), wah */
static float tk_lo[2], wh_lo[2];                 /* (0.17.19) their low ends, kept dry (a one-pole at ~180 Hz) */
#define XO_A 0.0253f                             /* that one-pole's step: 1 - e^(-2 pi 180 / 44100) */
#define TK_G 2.2f                                /* TALK's makeup on what it filters (the host: -6..-11 dB before) */
#define WH_G 1.6f                                /* WAH's (-4..-14 dB before) */
static uint32_t wh_cnt;   /* (the second: the resonant spike, for the band-pass) */
static float rm_m, rm_ph, rm_inc, rm_wet;   /* the ring mod: its fade, phase, step and share */   /* the sweep: its fade, its two filters' states, its coefficients */
static float rl_gain = 1.0f, tp_wd, tp_fd, tp_drive = 1.5f, tp_idrive = 1.0f / 1.5f, fl_inv = 1.0f, tp_k = 0.55f, rs_inv, rs_filt, rs_wash, rs_echo;   /* the roll's swell; the tape's; the riser's */
static float vn_amp = 1.0f;
static float ph_k0, ph_k1;                      /* the phaser's mix, as constants for the chunk: dry and wet shares (0.88 of them) */
static float ph_fbk, ph_wet, fl_inc, fl_rng, fl_fbk, fl_wet, vn_g, vn_pop, vn_hiss;   /* the settings, as the render uses them */
static inline float soft_sat(float x)           /* ~tanh: x (27 + x^2) / (27 + 9 x^2), held at +-3 (where it reaches 1) */
{
    x = x > 3.0f ? 3.0f : x < -3.0f ? -3.0f : x;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
static float tp_d = 150.0f, tp_dd, fl_d = 10.0f, fl_dd;   /* the tape's and the flanger's delay now, and its step a sample */
static float tp_m, fl_m, ph_m, tp_ph, tp_ph2, fl_ph, ph_ph, tp_lp[2], ph_z[2][4], ph_fb[2], ph_a[2];
/* the FX knobs (en_fxk): filter -50..50 (a DJ filter: low-pass to the left, high-pass to the right), crush,
 * throw (the whole mix into the echo), wash (the whole mix into the reverb), each 0..100. They are the
 * performer's, not a piece's: they start neutral and are not stored. The smoothed values follow them. */
static volatile int16_t fxk_v[EN_NFXK];
static float lpk_s, hpk_s, crk_s, thk_s, wash_s;
/* THE PUNCH-INS ON TRACKS (0.15.19, after Caustic's per-machine effects): the tracks a held FILTER, DELAY or PHASER targets are mixed into a bus of
 * their own (busl/busr) instead of the mix, at the same points and with the same gains, the pumps and the gate; the bus goes through the filter and the
 * phaser, into the echo for the delay, and back into the mix before the echoes and the reverb. Two held together share the bus: both act on every
 * track either one targets. The drums go as one (any drum in the targets takes all four), after their crush and mixer. */
const char *const EN_PT_NAME[EN_NPT] = {"ALL", "DRUMS", "SYNTHS", "PAD", "BASS", "ACID", "PLUCK", "ACID+PLK", "PAD+PLK", "BASS+ACID"};
static const uint8_t PT_MASK[EN_NPT] = {0xFF, 0x0F, 0xF0, 0x40, 0x10, 0x20, 0x80, 0xA0, 0xC0, 0x30};
static uint32_t bus_m;                          /* the tracks in the bus this chunk (a bit a track, EN_TR_*): 0, no bus */
static float tfx_m, tdx_m, tpx_m;               /* the three faded in and out (0..1) */
static float tf_c[4], tf_s[4], tp_a[2], tp_z[2][4], tp_fb[2], tp_ph;   /* the filter's coefficients and states; the phaser's */
static uint8_t tf_type;
#define TF_MORPH 5                               /* (0.17.72) the punch-in FILTER's Type 5 .. 25: Morph, low (5) to band (15) to high (25), its place smoothed */
static float tf_m = -1.0f, tf_mt, tf_w[3];      /* the place (0 low .. 1 high; -1: not yet), its target, the three outputs' weights at the last chunk's end */
static float lp_amt_e, hp_amt_e;                 /* the filters' amounts: the key's or the knob's, the higher */
static float cr_lv = 2048.0f, cr_wet;
static int cr_hold = 1;
static const float RPT_BEATS[9] = {2.0f, 1.0f, 2.0f / 3.0f, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f, 1.0f / 6.0f, 0.0625f};   /* (the 7th and 8th: triplets; the last, 1/64, only the roll reaches) */
const char *const EN_FX_NAME[EN_NFX] = {
    [FX_RPT2] = "Rpt 1/2", "Rpt 1/4", "Rpt 1/6", "Rpt 1/8", "Rpt 1/16", "Rpt 1/32", "Reverse", "Tape stop",
    "Mute kick", "Mute hats", "Mute snare", "Mute perc", "Low-pass", "High-pass",
    "Gate 1/16", "Gate 1/8", "Gate pattern", "Echo throw", "Fill", "Phaser", "Flanger", "Tape", "Roll", "Pump", "Riser", "Vinyl", "Scatter", "Brake", "Spin back", "Half time", "Sweep", "Scratch", "Freeze", "Rev swell", "Ring mod", "Rpt 1/8T", "Rpt 1/16T",
    "Flex Beat", "Filter", "Delay", "Phaser", "Mute drums", "", "", "", "", "", "", "", "", "", "", "Talk", "Wah", "Hat roll", "Hat trip", "Snr roll", "Flam", "Dbl kick"};

static void rpt_engage(int r)                   /* the slice that just ended, looped from its start */
{
    float len = RPT_BEATS[r] * 4.0f * step_len;
    while (len > (float)(RB_N - 8) * 2.0f && r < 5)   /* too long for the ring: the next shorter, in time */
        len = RPT_BEATS[++r] * 4.0f * step_len;
    if (len > (float)(RB_N - 8) * 2.0f)
        len = (float)(RB_N - 8) * 2.0f;
    if (bmode != BM_RPT)
        rp_t = 0.0f;                            /* a fresh repeat: the pitch's climb starts over (a roll's next stage, or a refresh, carries it on) */
    bmode = BM_RPT;
    brate = r;
    bm_len = len;
    {   /* (0.17.19) a slice shorter than a sixteenth starts where the last sixteenth did, on its hit: the half that just ended was its tail (1/32 sat
         * 4-16 dB under the music) */
        const float back = len < step_len && step_len * 0.5f < (float)(RB_N - 8) ? step_len : len;
        bm_start = rb_w - (uint32_t)(back * 0.5f + 0.5f), bm_sm = bm_start % RB_N;
    }
    bm_off = 0.0f;
    bm_tgt = 1.0f;
    rp_gain = 1.0f;
    rp_pos = 0.0f;
    rp_rate = rp_pit != 0.0f ? fm_exp2f(rp_pit * fm_minf(rp_t * 0.25f, 1.0f) * (1.0f / 12.0f)) : 1.0f;   /* at the pitch the climb has reached */
}

static void rpt_refresh(int st)                 /* on a sixteenth: a repeat with Refresh on takes the newest slice again each beat (or bar); the pitch's climb goes on */
{
    if (bmode == BM_RPT && rp_ref && rp_ok && (rp_ref == 2 ? st == 0 : (st & 3) == 0))
        rpt_engage(brate);                      /* (the climb goes on: rpt_engage keeps rp_t while a repeat is running) */
}

static inline void rb_read_from(uint32_t sm, float off, float *l, float *r)   /* off samples (44.1 kHz) after the ring index sm (before it, if negative) */
{
    float f = off * 0.5f, t;
    int32_t k = (int32_t)f, i;
    uint32_t i0, i1;
    if (f < 0.0f && (float)k != f)
        k--;
    t = f - (float)k;
    i = (int32_t)sm + k;                        /* (k is within one ring either way: one step brings it home) */
    i += i < 0 ? (int32_t)RB_N : i >= (int32_t)RB_N ? -(int32_t)RB_N : 0;
    i0 = (uint32_t)i;
    i1 = i0 + 1u == RB_N ? 0u : i0 + 1u;
    *l = ((float)rb[2 * i0] + ((float)rb[2 * i1] - (float)rb[2 * i0]) * t) * (1.0f / 12000.0f);
    *r = ((float)rb[2 * i0 + 1] + ((float)rb[2 * i1 + 1] - (float)rb[2 * i0 + 1]) * t) * (1.0f / 12000.0f);
}
static inline void rb_read(float off, float *l, float *r) { rb_read_from(bm_sm, off, l, r); }   /* off samples (44.1 kHz) after bm_start */

/* the master's spectrum, for the screen (the UI asks, fifteen times a second): the last 512 frames of the ring (22.05 kHz; about 23 ms),
 * windowed, then nb bands from 70 Hz to 10 kHz, spaced like notes, each a Goertzel filter: ~12k multiply-adds a call. out: 0 .. 1 (a dB
 * scale, tilted up towards the top as music falls away there). It reads the ring the repeat and scratch effects hold still, and zero while stopped. */
void en_spectrum(float *out, int nb)
{
    enum { N = 512 };
    static float x[N];
    int i, k;
    if (!en_playing) {
        for (k = 0; k < nb; k++)
            out[k] = 0.0f;
        return;
    }
    {
        uint32_t idx = rb_wi;
        for (i = N - 1; i >= 0; i--) {                      /* oldest first */
            const uint32_t j = idx == 0u ? RB_N - 1u : idx - 1u;
            idx = j;
            x[i] = ((float)rb[2 * j] + (float)rb[2 * j + 1]) * (0.5f / 12000.0f) * (0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i * (1.0f / (float)N)));
        }
    }
    for (k = 0; k < nb; k++) {
        const float f = 70.0f * fm_exp2f((float)k * (7.1f / (float)(nb > 1 ? nb - 1 : 1))), cf = 2.0f * fm_cosf(FM_TWO_PI * f * (1.0f / 22050.0f));
        float s1 = 0.0f, s2 = 0.0f, p, db;
        for (i = 0; i < N; i++) {
            const float s0 = x[i] + cf * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        p = s1 * s1 + s2 * s2 - cf * s1 * s2;
        db = 10.0f * 0.30103f * fm_log2f(p * (4.0f / ((float)N * (float)N)) + 1e-9f);   /* amplitude in dB (a full-scale sine ~ 0) */
        out[k] = fm_clampf((db + 56.0f + (float)k * (18.0f / (float)(nb > 1 ? nb - 1 : 1))) * (1.0f / 50.0f), 0.0f, 1.0f);
    }
}

static inline void rb_write(float l, float r)
{
    if (!rb_odd) {
        rb_hl = l;
        rb_hr = r;
        rb_odd = 1;
        return;
    }
    rb_odd = 0;
    {
        uint32_t i = rb_wi;
        rb[2 * i] = s16(0.5f * (rb_hl + l));
        rb[2 * i + 1] = s16(0.5f * (rb_hr + r));
        rb_w++;
        rb_wi = i + 1u == RB_N ? 0u : i + 1u;
    }
}

/* the scatter: the last few slices (a sixteenth each, or a half or a double) of the ring, played again slice by
 * slice: the first as it was, the rest by chance another, some backwards. It starts on the sixteenth. */
static float sc_pos, sc_len, sc_off;
static int sc_n, sc_rev;
static uint32_t sc_k;
static void scat_pick(void)
{
    const float chaos = (float)fxs_v[EN_FXS_SC_CHAOS] * 0.01f, rev = (float)fxs_v[EN_FXS_SC_REV] * 0.01f;
    int src = (int)(sc_k % (uint32_t)sc_n);
    if (src && frand() < chaos)
        src = (int)(frand() * (float)sc_n) % sc_n;
    sc_rev = src && frand() < rev;
    sc_off = (float)src * sc_len;
    sc_pos = 0.0f;
    sc_k++;
}
static void scat_engage(void)
{
    static const float SZ[3] = {0.5f, 1.0f, 2.0f};
    const float sl = fm_maxf(step_len * SZ[(int)fm_clampf((float)fxs_v[EN_FXS_SC_SIZE], 0.0f, 2.0f)], 64.0f), avail = (float)(RB_N - 8) * 2.0f;
    int n = (int)fm_clampf((float)fxs_v[EN_FXS_SC_LEN], 2.0f, 8.0f);
    while (n > 2 && (float)n * sl > avail)
        n--;
    sc_n = n;
    sc_len = sl;
    sc_k = 0;
    bmode = BM_SCAT;
    bm_start = rb_w - (uint32_t)((float)n * sl * 0.5f + 0.5f), bm_sm = bm_start % RB_N;
    bm_tgt = 1.0f;
    scat_pick();
}

/* FLEX BEAT (after the MPC's): one-bar patterns that rework the last stretch of the music (the ring: up to eight sixteenths) on the grid.
 * Each of the sixteen steps plays a slice (e: 0 the sixteenth just gone, 1 the one before, .. 7), or the live mix (15), muted, backwards,
 * or cut in two / four / eight and repeated (a stutter). Quantize: off (the next sixteenth), beat, bar. One shot: the bar plays out and
 * it is over (en_flx_done counts them, for the UI to let a latched key go); loop: it goes on while it is held or latched. The ring is
 * frozen while it plays (as the scatter), the live mix goes on under the live steps. Mix: how much of the effect against the music. */
#define FL(e, r, m, d) ((uint8_t)((e) | (r) << 4 | (m) << 5 | (d) << 6))
#define FLP(e) FL(e, 0, 0, 0)                   /* slice e */
#define FLR(e) FL(e, 1, 0, 0)                   /* slice e backwards */
#define FLM FL(0, 0, 1, 0)                      /* muted */
#define FLV FL(15, 0, 0, 0)                     /* the live mix */
#define FLD(d) FL(0, 0, 0, d)                   /* the last sixteenth cut in 2 (1), 4 (2), 8 (3) and repeated */
static const uint8_t FLEX[EN_NFLEX][16] = {
    /* 1 BEAT RPT */ {FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 2 HALF RPT */ {FL(7, 0, 0, 0), FL(6, 0, 0, 0), FL(5, 0, 0, 0), FL(4, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(7, 0, 0, 0), FL(6, 0, 0, 0), FL(5, 0, 0, 0), FL(4, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 3 BEAT REV */ {FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0)},
    /* 4 SKIP */ {FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0), FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0), FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0), FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0)},
    /* 5 TRANCE */ {FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0)},
    /* 6 GATE 8 */ {FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0)},
    /* 7 DOUBLE */ {FL(3, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 8 SHUFFLE */ {FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0)},
    /* 9 BACK-FWD */ {FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0)},
    /* 10 ROLL OUT */ {FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1)},
    /* 11 BUILD */ {FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 2), FL(0, 0, 0, 2)},
    /* 12 SCRATCH */ {FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0)},
    /* 13 DROPOUT */ {FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0)},
    /* 14 JUMP BK */ {FLP(3), FLP(2), FLP(1), FLP(0), FLV, FLV, FLV, FLV, FLV, FLV, FLV, FLV, FLV, FLV, FLV, FLV},
    /* 15 HALVES */ {FLP(3), FLP(2), FLP(1), FLP(0), FLP(7), FLP(6), FLP(5), FLP(4), FLP(3), FLP(2), FLP(1), FLP(0), FLP(7), FLP(6), FLP(5), FLP(4)},
    /* 16 SWAP */ {FLP(6), FLP(7), FLP(4), FLP(5), FLP(2), FLP(3), FLP(0), FLP(1), FLP(6), FLP(7), FLP(4), FLP(5), FLP(2), FLP(3), FLP(0), FLP(1)},
    /* 17 MIRROR */ {FLP(7), FLP(6), FLP(5), FLP(4), FLR(0), FLR(1), FLR(2), FLR(3), FLP(7), FLP(6), FLP(5), FLP(4), FLR(0), FLR(1), FLR(2), FLR(3)},
    /* 18 LAG */ {FLP(7), FLP(6), FLP(5), FLP(4), FLM, FLP(3), FLP(2), FLP(1), FLP(0), FLP(7), FLP(6), FLP(5), FLP(4), FLM, FLP(3), FLP(2)},
    /* 19 SLOWMO */ {FLP(7), FLP(7), FLP(6), FLP(6), FLP(5), FLP(5), FLP(4), FLP(4), FLP(3), FLP(3), FLP(2), FLP(2), FLP(1), FLP(1), FLP(0), FLP(0)},
    /* 20 SKIP 3 */ {FLP(2), FLP(1), FLP(0), FLP(2), FLP(1), FLP(0), FLP(2), FLP(1), FLP(0), FLP(2), FLP(1), FLP(0), FLP(2), FLP(1), FLP(0), FLP(2)},
    /* 21 STUT UP */ {FLP(0), FLP(0), FLP(0), FLP(0), FLD(1), FLD(1), FLD(1), FLD(1), FLD(2), FLD(2), FLD(2), FLD(2), FLD(3), FLD(3), FLD(3), FLD(3)},
    /* 22 STUT DN */ {FLD(3), FLD(3), FLD(3), FLD(3), FLD(2), FLD(2), FLD(2), FLD(2), FLD(1), FLD(1), FLD(1), FLD(1), FLP(0), FLP(0), FLP(0), FLP(0)},
    /* 23 TRESILLO */ {FLP(0), FLM, FLM, FLP(0), FLM, FLM, FLP(0), FLM, FLP(0), FLM, FLM, FLP(0), FLM, FLM, FLP(0), FLM},
    /* 24 OFFBEAT */ {FLM, FLV, FLM, FLV, FLM, FLV, FLM, FLV, FLM, FLV, FLM, FLV, FLM, FLV, FLM, FLV},
    /* 25 CUT 4 */ {FLV, FLV, FLV, FLM, FLV, FLV, FLV, FLM, FLV, FLV, FLV, FLM, FLV, FLV, FLV, FLM},
    /* 26 REV 8 */ {FLR(0), FLR(1), FLR(2), FLR(3), FLR(4), FLR(5), FLR(6), FLR(7), FLR(0), FLR(1), FLR(2), FLR(3), FLR(4), FLR(5), FLR(6), FLR(7)},
    /* 27 HALF GAP */ {FLP(7), FLP(6), FLP(5), FLP(4), FLP(3), FLP(2), FLP(1), FLP(0), FLM, FLM, FLM, FLM, FLM, FLM, FLM, FLM},
};
#define FLEXMASK FXB(FX_FLEX)
static volatile uint8_t flx_sel;                /* the pattern the Flex bit plays (en_flex_select) */
void en_flex_select(int pat) { flx_sel = (uint8_t)((unsigned)pat < (unsigned)EN_NFLEX ? pat : 0); }
uint8_t en_flex_step(int pat, int step) { return FLEX[(unsigned)pat < (unsigned)EN_NFLEX ? pat : 0][step & 15]; }
volatile uint8_t en_flx_state;                  /* 0 idle, 1 armed (waiting for its time), 2 playing */
volatile uint32_t en_flx_done;                  /* one shots played out */
static uint8_t flx_arm, flx_active, flx_pat, flx_pos, flx_held, flx_wait_rel, flx_rev, flx_live, flx_mute, flx_phys;
static int flx_n;
static float flx_slice, flx_off, flx_sub, flx_p;
static void flx_load(void)                      /* the step's slice */
{
    const uint8_t b = FLEX[flx_pat][flx_pos & 15];
    const int e = b & 15, div = b >> 6 & 3;
    flx_mute = (b >> 5) & 1u;
    flx_rev = (b >> 4) & 1u;
    flx_live = e == 15;
    if (!flx_live) {
        flx_off = (float)(flx_n - 1 - (e * flx_n >> 3)) * flx_slice;   /* below 126 BPM fewer slices: the pattern's shape kept, coarser */
        flx_sub = fm_maxf(flx_slice / (float)(1 << div), 32.0f);
    }
    flx_p = 0.0f;
}
static void flx_engage(void)
{
    if (flx_active && bmode == BM_FLX) {        /* already playing: the next pattern, from its start */
        flx_pos = 0;
        flx_load();
        return;
    }
    flx_slice = fm_maxf(step_len, 64.0f);
    flx_n = (int)(((float)(RB_N - 8) * 2.0f) / flx_slice);
    flx_n = flx_n < 1 ? 1 : flx_n > 8 ? 8 : flx_n;
    bmode = BM_FLX;
    bm_start = rb_w - (uint32_t)((float)flx_n * flx_slice * 0.5f + 0.5f), bm_sm = bm_start % RB_N;
    bm_tgt = 0.01f * (float)fm_clampf((float)fxs_v[EN_FXS_FX_MIX], 0.0f, 100.0f);
    flx_pos = 0;
    flx_active = 1;
    en_flx_state = 2;
    flx_load();
}
static void flx_tick(int st)                    /* on every sixteenth */
{
    if (flx_active && bmode != BM_FLX) {        /* another effect took over */
        flx_active = 0;
        en_flx_state = flx_arm ? 1 : 0;
    }
    if (flx_active) {
        if (++flx_pos >= 16) {
            if ((fxs_v[EN_FXS_FX_MODE] == 1 && flx_held) || flx_phys) {   /* a loop while held or latched; a one shot too while the pad is held down by hand */
                flx_pos = 0;
            } else {                            /* the bar is over */
                flx_active = 0;
                bm_tgt = 0.0f;
                flx_wait_rel = flx_held;
                en_flx_done++;
                en_flx_state = flx_arm ? 1 : 0;
            }
        }
        if (flx_active)
            flx_load();
    }
    if (flx_arm) {
        const int q = (int)fm_clampf((float)fxs_v[EN_FXS_FX_QUANT], 0.0f, 2.0f);
        if (q == 0 || (q == 1 && (st & 3) == 0) || (q == 2 && st == 0)) {
            flx_pat = (uint8_t)(flx_arm - 1u);
            flx_arm = 0;
            flx_engage();
        }
    }
}

/* THE DJ SNIPS (the FX menu's fourth bank; they replaced the sampler): the 27 keys are sounds a DJ throws in, made on the spot: air horns, sirens, lasers,
 * zaps, beeps, a whoop, a dub siren, a drop, a riser, a rewind, a crash, a rave stab, a bell, a snare roll. Up to four at once. Held mode (the page's Play):
 * the sounds that go on (horns, sirens, the dub siren, the riser, the roll) sound as long as the key is down; the others play out. One shot: every one plays
 * out (the long ones for at most a second or so). Pitch: the page's octave, up or down. Echo: how much of them goes to the echo and the reverb. Level. */
enum { DJ_HORN, DJ_SIREN, DJ_LASER, DJ_ZAP, DJ_BEEP, DJ_DBL, DJ_WHOOP, DJ_DUB, DJ_DROP, DJ_RISER, DJ_REWIND, DJ_CRASH, DJ_STAB, DJ_BELL, DJ_ROLL, DJ_BLAST, DJ_RAVE, DJ_RSTAB,
       DJ_KICK, DJ_SNARE };
/* (0.17.44, the maker: "the horns are still trash, maybe a sampled sound would be easier and sound better. A good crash, kick, and snare too") the air
 * horn and blast, the rave horn and stab, and the crash are samples now (djsmp.h: rendered offline by tools/djsamples.py, our own models), the horns
 * looped while held and played on the chord's root; the kick and the snare (on SIREN 3's and LASER LO's keys) are the sampled kits' (MACHINE's kick,
 * BOOMBAP's snare). */
/* (0.17.24, the snips pass: each one measured alone; their levels were 25 dB apart, every pitched one a fixed B flat, all mono, the crash, the roll and the
 * riser mostly hiss.) Now: the pitched ones take the chord playing (dj_note), each has its own trim (DJ_TRIM, from the measure: within 2 dB of each other),
 * the horns, the sirens, the rave horn, the stabs, the crash, the riser and the roll have a side (left and right differ; in mono it cancels), and the air
 * horns, the rave horn, the crash, the roll, the riser and the bell are remade. HORN MIN's key is a chord stab now (the chord playing, in saws). */
static const struct { uint8_t type; float hz, aux; const char *name; } DJS[27] = {
    {DJ_HORN, 466.0f, 0.0f, "AIR HORN"}, {DJ_BLAST, 466.0f, 0.0f, "AIR BLAST"}, {DJ_STAB, 233.0f, 0.0f, "CHORD STAB"}, {DJ_RAVE, 233.0f, 0.0f, "RAVE HORN"},
    {DJ_SIREN, 900.0f, 1.2f, "SIREN"}, {DJ_SIREN, 1100.0f, 3.0f, "SIREN 2"}, {DJ_KICK, 0.0f, 0.0f, "KICK"},
    {DJ_LASER, 2400.0f, 0.06f, "LASER"}, {DJ_SNARE, 0.0f, 0.0f, "SNARE"},
    {DJ_ZAP, 3000.0f, 0.15f, "ZAP"}, {DJ_ZAP, 6000.0f, 0.08f, "ZAP HI"},
    {DJ_BEEP, 1000.0f, 0.1f, "BEEP"}, {DJ_BEEP, 600.0f, 0.12f, "BEEP LO"}, {DJ_BEEP, 1700.0f, 0.08f, "BEEP HI"}, {DJ_DBL, 1200.0f, 0.07f, "2 BEEP"},
    {DJ_WHOOP, 300.0f, 0.35f, "WHOOP"}, {DJ_WHOOP, 800.0f, 0.6f, "WHOOP 2"},
    {DJ_DUB, 1400.0f, 0.9f, "DUB SIREN"}, {DJ_DUB, 900.0f, 1.6f, "DUB LOW"},
    {DJ_DROP, 140.0f, 1.2f, "DROP"}, {DJ_DROP, 100.0f, 2.4f, "DROP LONG"},
    {DJ_RISER, 110.0f, 3.0f, "RISER"}, {DJ_REWIND, 2200.0f, 0.7f, "REWIND"}, {DJ_CRASH, 0.0f, 1.2f, "CRASH"},
    {DJ_RSTAB, 233.0f, 0.0f, "RAVE STAB"}, {DJ_BELL, 880.0f, 0.0f, "BELL"}, {DJ_ROLL, 0.0f, 2.5f, "SNR ROLL"}};
/* each snip's trim, half dB (+ louder), so that each sits at the same loudness (its loudest 400 ms, K-weighted): measured alone (0.17.24) */
static const int8_t DJ_TRIM[27] = {-14, -12, 1, -10, -11, -11, -10, 3, -4, 5, 8, 3, 3, 1, 0, -6, -12, -4, -4, -5, -6, 17, 4, -2, 10, -7, 6};   /* (0.17.44: the sampled horns, crash, kick and snare measured the same way) */
const char *en_dj_name(int key) { return DJS[(unsigned)key < 27u ? key : 0].name; }
typedef struct {
    uint8_t on, held, key, type, hold_mode, land;   /* (land, 0.17.72: its time is the way to the line it lands on) */
    float hz, aux, aux2, t, ph, ph2, ph3, ph4, env, z, z2, zs, zs2, g;
    uint32_t age;
    const uint8_t *sd;                          /* (0.17.44) a sampled snip: its ADPCM, length and loop (0: none), where it is, the decoder and its state at the loop's start, */
    uint32_t sn, sls, sle, si;
    int32_t spr, slpr;
    uint8_t sai, slai, slsv;
    float sx1, sx2, sph, srate;                 /* the two samples about the place, the place between them, the step (the sample's rate and the note) */
    float ee[3], em[3];                         /* (0.17.60) its decays, stepped: each e^(-K t) now, and its step a sample (0: not started) */
} dj_t;
static dj_t djv[4];
static uint32_t dj_age;
volatile uint8_t en_smp_active;                 /* the snips sounding */

/* a pitched snip in key: the nearest note (within a tritone) that is a tone of the chord playing (its root only, for the horns and the stabs), in the
 * piece's tuning; and the chord's third and fifth above it as ratios (the horns' and the stab's other notes; a diminished fifth is played perfect) */
static float dj_note(float hz, int root_only, float *third, float *fifth)
{
    const int n0 = 69 + (int)fm_floorf(12.0f * fm_log2f(hz * (1.0f / 440.0f)) + 0.5f);
    const int nt = chord_nt > 0 ? chord_nt : 0, r = nt ? chord_pcs[0] % 12 : par[P_ROOT] % 12;
    int d, k, best = 0, iv3 = 4, iv5 = 7;
    for (d = 0; d <= 6; d++) {
        int ok = 0, s;
        for (s = -1; s <= 1 && !ok; s += 2) {
            const int pc = ((n0 + s * d) % 12 + 12) % 12;
            if (pc == r)
                ok = 1;
            for (k = 1; k < nt && !root_only && !ok; k++)
                ok = pc == chord_pcs[k] % 12;
            if (ok)
                best = s * d;
            if (!d)
                break;
        }
        if (ok)
            break;
    }
    if (nt >= 3) {
        iv3 = (chord_pcs[1] - chord_pcs[0] + 24) % 12;
        iv5 = (chord_pcs[2] - chord_pcs[0] + 24) % 12;
        iv5 = iv5 == 6 || iv5 < 5 ? 7 : iv5;
    }
    if (third)
        *third = fm_exp2f((float)iv3 * (1.0f / 12.0f));
    if (fifth)
        *fifth = fm_exp2f((float)iv5 * (1.0f / 12.0f));
    return 440.0f * fm_exp2f(((float)(n0 + best - 69) + 0.01f * (float)par[P_TUNE]) * (1.0f / 12.0f));
}

static void smp_note(int key, int on)
{
    int i, v = -1;
    if (key < 0 || key > 26)
        return;
    if (!on) {
        for (i = 0; i < 4; i++)
            if (djv[i].on && djv[i].key == key)
                djv[i].held = 0;
        return;
    }
    for (i = 0; i < 4; i++)
        if (!djv[i].on) {
            v = i;
            break;
        }
    if (v < 0) {                                /* all four going: the oldest is taken */
        v = 0;
        for (i = 1; i < 4; i++)
            if (djv[i].age < djv[v].age)
                v = i;
    }
    {
        dj_t *d = &djv[v];
        const int oct = fxs_v[EN_FXS_SM_MODE], ty = DJS[key].type;
        float hz = DJS[key].hz * (oct == 1 ? 2.0f : oct == 2 ? 0.5f : 1.0f), r3 = 1.26f, r5 = 1.498f;
        d->on = 1;
        d->held = 1;
        d->em[0] = d->em[1] = d->em[2] = 0.0f;   /* (0.17.60) its decays from the start */
        d->key = (uint8_t)key;
        d->type = (uint8_t)ty;
        d->hold_mode = fxs_v[EN_FXS_SM_LOOP] != 0;
        d->aux = DJS[key].aux;
        if (ty == DJ_HORN || ty == DJ_BLAST || ty == DJ_STAB || ty == DJ_RAVE || ty == DJ_RSTAB || ty == DJ_RISER) {
            hz = dj_note(hz, 1, &r3, &r5);
            if (ty == DJ_HORN || ty == DJ_BLAST || ty == DJ_STAB)
                d->aux = r3;
        } else if (ty == DJ_SIREN || ty == DJ_BEEP || ty == DJ_DBL || ty == DJ_WHOOP || ty == DJ_DUB || ty == DJ_BELL) {
            hz = dj_note(hz, 0, 0, 0);
        }
        d->hz = hz;
        d->aux2 = r5;
        d->g = fm_exp2f(0.0830482f * (float)DJ_TRIM[key]);   /* (half dB: 2^(h/12.04)) */
        d->t = d->ph = d->ph2 = d->ph3 = d->ph4 = d->env = d->z = d->z2 = d->zs = d->zs2 = 0.0f;
        d->land = 0;
        if ((ty == DJ_RISER || ty == DJ_REWIND || ty == DJ_ROLL) && fxs_v[EN_FXS_SM_LAND] > 0 && en_playing && sixteenth > 0u && step_len > 1.0f) {
            /* (0.17.72, ChatGPT's: "a rewind pulls into the next downbeat, a riser peaks there, and a roll ends with the drop") Land: the next line of
             * L sixteenths (a beat, two, a bar) at least L away; it starts now and its time is the way there, so its peak is on the line */
            static const uint8_t LS[4] = {0, 4, 8, 16};
            const int L = LS[fxs_v[EN_FXS_SM_LAND] > 3 ? 3 : fxs_v[EN_FXS_SM_LAND]];
            const float now = (float)(sixteenth - 1u) + fm_minf((float)step_t / step_len, 0.999f), x = (now + (float)L) / (float)L;
            float q = fm_floorf(x);
            if (q < x)                           /* (the ceiling: floor(x + 0.999) picked a line a hair short of L away just after a line) */
                q += 1.0f;
            d->aux = ((float)L * q - now) * step_len * (1.0f / EN_SR);
            d->land = 1;
            d->aux2 = 0.0f;                     /* (the roll: its hits a beat, from the first beat: eighths) */
            d->ph4 = now * 0.25f;                /* (the roll: the beat it is at, for its hits on the grid) */
            d->ph2 = 1.0f / (4.0f * step_len);  /* (and beats a sample: the roll leaves ph2 unused) */
        }
        d->age = ++dj_age;
        d->sd = 0;
        {   /* (0.17.44) the sampled ones: the horns at the chord's root (as their note against the sample's), the crash, the kick and the snare as they are */
            const djsmp_t *s = ty == DJ_HORN || ty == DJ_BLAST ? &DJ_SMP[DJS_HORN_I] : ty == DJ_RAVE || ty == DJ_RSTAB ? &DJ_SMP[DJS_RAVE_I] :
                               ty == DJ_CRASH ? &DJ_SMP[DJS_CRASH_I] : 0;
            const float po = oct == 1 ? 2.0f : oct == 2 ? 0.5f : 1.0f;   /* (the page's octave, for the unpitched ones too) */
            if (s) {
                d->sd = s->d, d->sn = s->n, d->sls = s->ls, d->sle = s->le, d->sai = s->i0;
                d->srate = s->sr * (1.0f / EN_SR) * (s->hz > 0.0f ? hz / s->hz : po);
            } else if (ty == DJ_KICK || ty == DJ_SNARE) {
                const smp_t *k = ty == DJ_KICK ? &SKIT_BI[0].s[0] : &SKIT_BI[1].s[1];
                d->sd = k->d, d->sn = k->n, d->sls = d->sle = 0, d->sai = k->i0;
                d->srate = 22050.0f * (1.0f / EN_SR) * po;
            }
            d->si = 0;
            d->spr = d->slpr = 0;
            d->slai = d->sai;
            d->slsv = 0;
            d->sx1 = d->sx2 = 0.0f;
            d->sph = 0.0f;
        }
    }
}

static float dj_next(dj_t *v)                    /* (0.17.44) the next sample of a sampled snip, decoded (0 past its end); a held horn loops (the decoder's state
                                                 * kept at the loop's start, so each time round decodes the same) */
{
    int c, st, df;
    if (v->sle && v->si >= v->sle) {
        v->si = v->sls;
        v->spr = v->slpr;
        v->sai = v->slai;
    }
    if (v->si >= v->sn)
        return 0.0f;
    if (v->sle && v->si == v->sls && !v->slsv) {
        v->slsv = 1;
        v->slpr = v->spr;
        v->slai = v->sai;
    }
    c = v->sd[v->si >> 1] >> ((v->si & 1u) * 4u) & 15;
    v->si++;
    st = ADPCM_STEP[v->sai];
    df = st >> 3;
    if (c & 4)
        df += st;
    if (c & 2)
        df += st >> 1;
    if (c & 1)
        df += st >> 2;
    v->spr += c & 8 ? -df : df;
    v->spr = v->spr < -32768 ? -32768 : v->spr > 32767 ? 32767 : v->spr;
    st = v->sai + ADPCM_IDX[c];
    v->sai = (uint8_t)(st < 0 ? 0 : st > 88 ? 88 : st);
    return (float)v->spr * (1.0f / 32768.0f);
}
static float dj_smp(dj_t *v)                    /* one output sample of it, between two of its own (its rate and note) */
{
    while (v->sph >= 0.0f) {
        v->sx1 = v->sx2;
        v->sx2 = dj_next(v);
        v->sph -= 1.0f;
    }
    {
        const float y = v->sx2 + (v->sx1 - v->sx2) * -v->sph;
        v->sph += v->srate;
        return y * 1.6f;                        /* (about the synthesized ones' level: DJ_TRIM evens them) */
    }
}
static inline float dj_wrap(float p) { return p - (float)(int)p; }
/* (0.17.60) e^(-K t) for a voice's decay k, stepped: the first call takes the exp, each one after (once a sample, every sample) a multiply. The snips
 * took up to three exps a sample a voice, four voices at once: on the host, TRANCE 01 rendered 49 % slower with the bell on every beat, 32 % with the
 * long drop, 25 % with the roll, and the maker's log had every dropout where the snips were playing. */
static inline float dj_dec(dj_t *v, int k, float K, float t)
{
    if (v->em[k] == 0.0f) {
        v->em[k] = fm_expf(-K * (1.0f / EN_SR));
        v->ee[k] = fm_expf(-K * t);
    } else {
        v->ee[k] *= v->em[k];
    }
    return v->ee[k];
}
static inline float dj_pulse(float ph, float w, float inc) { return blep_saw(ph, inc) - blep_saw(dj_wrap(ph + w), inc); }
/* one sample of a voice: its middle (returned), its side (*side: left gets + it, right - it), its own envelope (*amp); it ends itself (on = 0) */
static float dj_voice(dj_t *v, float *amp, float *side)
{
    const float t = v->t * (1.0f / EN_SR), inc = v->hz * (1.0f / EN_SR), n = frand() * 2.0f - 1.0f;
    float y = 0.0f, a = 1.0f, s = 0.0f;
    switch (v->type) {
    case DJ_HORN: case DJ_BLAST: case DJ_RAVE: case DJ_RSTAB: case DJ_CRASH: case DJ_KICK: case DJ_SNARE: {   /* (0.17.44) the sampled ones (djsmp.h) */
        y = dj_smp(v);
        if (v->type == DJ_BLAST) {              /* a short blast of the horn: the air dying after a third of a second */
            a = t < 0.32f ? 1.0f : dj_dec(v, 0, 12.0f, t - 0.32f);
            if (t > 0.9f)
                v->on = 0;
        } else if (v->type == DJ_RSTAB) {       /* a stab of the rave horn */
            a = dj_dec(v, 0, 7.0f, t);
            if (t > 0.55f)
                v->on = 0;
        } else if (v->type == DJ_CRASH || v->type == DJ_KICK || v->type == DJ_SNARE) {
            if (v->si >= v->sn && fm_fabsf(v->sx1) < 1e-4f)
                v->on = 0;                       /* (played out) */
        }
        break;
    }
    case DJ_STAB: {                             /* THE CHORD STAB (0.17.24, back on a key): three saws on the chord playing, a filter closing, the third left, the fifth right */
        const float k = 0.5f * dj_dec(v, 0, 6.0f, t) + 0.04f, r2 = v->aux > 1.0f ? v->aux : 1.189f, r3 = v->aux2 > 1.0f ? v->aux2 : 1.498f;
        const float s1 = blep_saw(v->ph, inc), s2 = blep_saw(v->ph2, inc * r2), s3 = blep_saw(v->ph3, inc * r3);
        y = (s1 + s2 + s3 + 0.5f * blep_saw(v->ph4, inc * 0.5f)) * 0.4f;
        v->ph = dj_wrap(v->ph + inc);
        v->ph2 = dj_wrap(v->ph2 + inc * r2);
        v->ph3 = dj_wrap(v->ph3 + inc * r3);
        v->ph4 = dj_wrap(v->ph4 + inc * 0.5f);
        v->z += k * (y - v->z);
        y = v->z;
        v->zs += k * ((s2 - s3) * 0.25f - v->zs);
        s = v->zs;
        a = dj_dec(v, 1, 7.0f, t);
        if (t > 0.7f)
            v->on = 0;
        break;
    }
    case DJ_SIREN: {                            /* a sine swept up and down; the side a quarter turn on (left and right apart, nothing lost in mono) */
        const float lf = dj_wrap(t * v->aux), tri = lf < 0.5f ? lf * 4.0f - 1.0f : 3.0f - lf * 4.0f;
        v->ph = dj_wrap(v->ph + inc * (1.0f + 0.5f * tri));
        y = 0.6f * sine(v->ph) + 0.25f * sine(dj_wrap(v->ph * 2.0f));
        s = 0.3f * sine(dj_wrap(v->ph + 0.25f));
        break;
    }
    case DJ_LASER:                              /* a fast fall in pitch, hard-edged */
        v->ph = dj_wrap(v->ph + (v->hz * dj_dec(v, 0, 1.0f / v->aux, t) + 150.0f) * (1.0f / EN_SR));
        y = 0.5f * fm_tanhf(3.0f * sine(v->ph));
        a = dj_dec(v, 1, 4.0f, t);
        if (t > 0.5f)
            v->on = 0;
        break;
    case DJ_ZAP: {                              /* noise through a band that falls */
        const float k = 0.04f + 0.6f * dj_dec(v, 0, 1.0f / v->aux, t);
        v->z += k * (n - v->z);
        v->z2 += k * (v->z - v->z2);
        y = (v->z - v->z2) * 3.0f;
        a = dj_dec(v, 1, 7.0f / v->aux * 0.5f, t);
        if (t > 0.5f)
            v->on = 0;
        break;
    }
    case DJ_BEEP: case DJ_DBL: {                /* (0.17.24: the gate eased over half a millisecond, not a step) */
        const float ln = v->aux, g1 = t < ln ? 1.0f : 0.0f, g2 = v->type == DJ_DBL && t > ln * 1.9f && t < ln * 2.9f ? 1.0f : 0.0f;
        v->ph = dj_wrap(v->ph + inc);
        y = sine(v->ph) * 0.7f;
        v->z += 0.05f * (g1 + g2 - v->z);
        a = v->z;
        if (t > ln * 3.2f)
            v->on = 0;
        break;
    }
    case DJ_WHOOP: {                            /* up two and a half octaves */
        const float u = fm_minf(t / v->aux, 1.0f);
        v->ph = dj_wrap(v->ph + inc * fm_exp2f(2.585f * u));
        y = 0.55f * sine(v->ph) + 0.2f * sine(dj_wrap(v->ph * 2.0f));
        a = u < 1.0f ? 1.0f : fm_maxf(0.0f, 1.0f - (t - v->aux) / 0.15f);
        if (t > v->aux + 0.16f)
            v->on = 0;
        break;
    }
    case DJ_DUB: {                              /* a square falling from high to low, again and again */
        const float cyc = t / v->aux, fr = cyc - (float)(int)cyc;
        v->ph = dj_wrap(v->ph + inc * fm_exp2f(-2.5f * fr));
        y = (v->ph < 0.5f ? 0.4f : -0.4f);
        v->z += 0.35f * (y - v->z);
        y = v->z;
        break;
    }
    case DJ_DROP:                               /* a sine falling to the sub */
        v->ph = dj_wrap(v->ph + (v->hz * dj_dec(v, 0, 3.0f, t) + 28.0f) * (1.0f / EN_SR));
        y = 0.9f * sine(v->ph);
        a = dj_dec(v, 1, 1.0f / (v->aux * 0.5f), t);
        if (t > v->aux * 2.0f)
            v->on = 0;
        break;
    case DJ_RISER: {                            /* THE RISER (0.17.24: louder, and a tone under the noise): noise through a low-pass that opens over its time, and two
                                                 * saws a hair apart (one each side) rising two octaves from the chord's root, the whole growing */
        const float u = fm_minf(t / v->aux, 1.0f), fi = inc * fm_exp2f(2.0f * u), k = 0.03f + 0.9f * u * u;
        const float n2 = frand() * 2.0f - 1.0f, s1 = blep_saw(v->ph, fi), s2 = blep_saw(v->ph2, fi * 1.006f);
        v->ph = dj_wrap(v->ph + fi);
        v->ph2 = dj_wrap(v->ph2 + fi * 1.006f);
        v->z += k * (n - v->z);
        v->zs += k * (n2 - v->zs);
        v->z2 += (0.05f + 0.5f * u) * ((s1 + s2) - v->z2);   /* the tone's own low-pass, opening */
        y = v->z * 2.4f + 0.45f * v->z2;
        s = v->zs * 1.4f + 0.25f * (s1 - s2);
        a = 0.15f + 0.85f * u * u;
        break;
    }
    case DJ_REWIND: {                           /* a saw falling away, as a record pulled back */
        const float u = fm_minf(t / v->aux, 1.0f), f = v->hz * fm_exp2f(-5.0f * u) + 40.0f;
        v->ph = dj_wrap(v->ph + f * (1.0f / EN_SR));
        y = blep_saw(v->ph, f * (1.0f / EN_SR)) * 0.5f;
        v->z += 0.3f * (y - v->z);
        y = v->z;
        a = 1.0f - u * u;
        if (t > v->aux)
            v->on = 0;
        break;
    }
    case DJ_BELL: {                             /* THE BELL (0.17.24: in tune; its overtones were off the note, a clang): two operators 1:3, the index falling, and a glass partial two octaves up */
        v->ph = dj_wrap(v->ph + inc);
        v->ph2 = dj_wrap(v->ph2 + inc * 3.0f);
        v->ph3 = dj_wrap(v->ph3 + inc * 4.0f);
        y = 0.65f * sine_any(v->ph + 1.1f * dj_dec(v, 0, 4.0f, t) * sine(v->ph2)) + 0.12f * dj_dec(v, 1, 6.0f, t) * sine(v->ph3);
        a = dj_dec(v, 2, 2.2f, t);
        if (t > 3.0f)
            v->on = 0;
        break;
    }
    default: {                                  /* THE SNARE ROLL (0.17.24, remade: it was hiss and a hum): hits that speed up, each a body (a sine falling from about 330 to
                                                 * 180 Hz) and a band of noise near 3.5 kHz, the roll getting louder; the side is the noise's own copy */
        const float u = fm_minf(t / v->aux, 1.0f);
        float rate = 7.0f + 18.0f * u, bt, ht, ne, hp, n2 = frand() * 2.0f - 1.0f;
        if (v->land) {                          /* (0.17.72) Land: on the grid, eighths, then sixteenths, then thirty-seconds, each from a beat's start */
            const float hb = v->aux2 > 0.0f ? v->aux2 : 2.0f, b0 = v->ph4 * hb;
            float b1;
            v->ph4 += v->ph2;
            b1 = v->ph4 * hb;
            if ((int)b1 != (int)b0) {           /* a hit; at a beat's start the rate for the next one */
                v->em[0] = v->em[1] = v->em[2] = 0.0f;
                if ((int)v->ph4 != (int)(v->ph4 - v->ph2))
                    v->aux2 = u < 0.45f ? 2.0f : u < 0.8f ? 4.0f : 8.0f;
            }
            v->ph3 = b1 - (float)(int)b1;
            rate = hb * v->ph2 * EN_SR;
        } else {
            const float pb = v->ph3;
            v->ph3 = dj_wrap(v->ph3 + rate * (1.0f / EN_SR));
            if (v->ph3 < pb)                    /* (0.17.60) a new hit: its decays from it, exact */
                v->em[0] = v->em[1] = v->em[2] = 0.0f;
        }
        bt = v->ph3;
        ht = bt / rate;                         /* seconds into this hit */
        v->ph = dj_wrap(v->ph + (180.0f + 150.0f * dj_dec(v, 0, 40.0f, ht)) * (1.0f / EN_SR));
        v->z += 0.49f * v->z2;                  /* the noise's band: a state-variable filter at ~3.5 kHz */
        hp = n - v->z - v->z2;
        v->z2 += 0.49f * hp;
        v->zs += 0.49f * v->zs2;
        hp = n2 - v->zs - v->zs2;
        v->zs2 += 0.49f * hp;
        ne = dj_dec(v, 1, 22.0f, ht);
        y = 0.6f * sine(v->ph) * dj_dec(v, 2, 30.0f, ht) + 1.1f * v->z2 * ne;
        s = 0.45f * v->zs2 * ne;
        a = 0.35f + 0.65f * u;
        break;
    }
    }
    v->t += 1.0f;
    *amp = a * v->g;
    *side = s;
    return y;
}

/* a brake, a spin-back, half time: the ring read at a rate that slides to its target (0; backwards; a half or less)
 * while the live mix goes on being written; a low-pass follows the rate down. Let go: the rate slides back to 1 and
 * the live mix returns on the next sixteenth. It lets go by itself when the ring is used up. */
static float vr_rate, vr_tgt, vr_c, vr_lp[2], vr_dull;
static void var_leave(void) { bm_tgt = 0.0f; }
static int vr_which;
static float vr_time(float v, float base)       /* base * 2 ^ (v * 0.06) seconds, as a slide's coefficient */
{
    return 1.0f - fm_expf(-3.0f / (base * fm_exp2f(v * 0.06f) * EN_SR));
}
static void var_engage(int wh)
{
    bmode = BM_VAR;
    bm_start = rb_w - 2u, bm_sm = bm_start % RB_N;
    bm_off = 0.0f;
    bm_tgt = 1.0f;
    vr_rate = 1.0f;
    vr_which = wh;
    vr_lp[0] = vr_lp[1] = 0.0f;
}
static void var_params(int wh)
{
    static const float HS[3] = {0.667f, 0.5f, 0.25f};
    if (wh == FX_HALF) {
        vr_tgt = HS[(int)fm_clampf((float)fxs_v[EN_FXS_HF_SPEED], 0.0f, 2.0f)];
        vr_c = vr_time((float)fxs_v[EN_FXS_HF_SLIDE], 0.02f);
        vr_dull = 0.0f;
    } else {
        vr_tgt = wh == FX_SPIN ? -(float)fxs_v[EN_FXS_BR_BACK] * 0.04f : 0.0f;
        vr_c = vr_time((float)fxs_v[EN_FXS_BR_TIME], 0.03f);
        vr_dull = (float)fxs_v[EN_FXS_BR_DULL] * 0.009f;
    }
}

/* the scratch: the ring's last stretch (a sixteenth up to half a note) read back and forth, the read point following a sine
 * (baby) or running forward and snapping back (pull); the speed it moves gives the pitch. */
static float scr_t, scr_w, scr_per;
static void scr_engage(void)
{
    static const float WIN[4] = {1.0f, 2.0f, 4.0f, 8.0f}, PER[4] = {4.0f, 2.0f, 1.0f, 0.5f};
    const float avail = (float)(RB_N - 8) * 2.0f;
    scr_w = fm_minf(fm_maxf(step_len * WIN[(int)fm_clampf((float)fxs_v[EN_FXS_SR_REACH], 0.0f, 3.0f)], 64.0f), avail);
    scr_per = fm_maxf(step_len * PER[(int)fm_clampf((float)fxs_v[EN_FXS_SR_EVERY], 0.0f, 3.0f)], 32.0f);
    scr_w = fm_minf(scr_w, scr_per * (fxs_v[EN_FXS_SR_STYLE] == 0 ? 0.8f : 0.6f));   /* the record never moves faster than ~2.5x: a shorter stroke, not a squeak */
    scr_t = 0.0f;
    bmode = BM_SCR;
    bm_start = rb_w - (uint32_t)(scr_w * 0.5f + 0.5f), bm_sm = bm_start % RB_N;
    bm_tgt = 1.0f;
}
/* the freeze: a stretch of the past (0.1 .. 0.85 s) held, and grains (20 .. 200 ms, one to four) drawn from it at random
 * places, each in a parabolic window, overlapping: a shimmering texture of the moment you froze. */
#define GN_MAX 4
static float gn_pos[GN_MAX], gn_st[GN_MAX], gn_len, gn_region, gn_scale;
static int gn_n;
static void gn_respawn(int i)
{
    gn_st[i] = frand() * fm_maxf(gn_region - gn_len, 1.0f);
    gn_pos[i] = 0.0f;
}
static void grn_engage(void)
{
    const float frames = fm_minf((0.1f + 0.0075f * (float)fxs_v[EN_FXS_FZ_SPREAD]) * 22050.0f, (float)(RB_N - 8));
    int i;
    gn_region = frames * 2.0f;
    gn_len = fm_minf(20.0f * fm_exp2f((float)fxs_v[EN_FXS_FZ_SIZE] * 0.033f) * 44.1f, gn_region);
    gn_n = 1 + (int)fm_clampf((float)fxs_v[EN_FXS_FZ_GRAINS], 0.0f, 3.0f);
    gn_scale = 1.5f / fm_sqrtf((float)gn_n);
    bmode = BM_GRN;
    bm_start = rb_w - (uint32_t)frames, bm_sm = bm_start % RB_N;
    bm_tgt = 1.0f;
    for (i = 0; i < gn_n; i++) {
        gn_respawn(i);
        gn_pos[i] = gn_len * (float)i / (float)gn_n;    /* staggered */
    }
}
/* the reverse swell: the last stretch (a half beat, a beat or two) played backwards once, rising in level, ending where it
 * began, from the sixteenth; under the live mix by its Mix. One swell for each press. */
static float rv_pos, rv_len;
static uint8_t rv_held;
static void rvs_engage(void)
{
    static const float LEN[3] = {2.0f, 4.0f, 8.0f};    /* sixteenths */
    const float avail = (float)(RB_N - 8) * 2.0f;
    rv_len = fm_minf(step_len * LEN[(int)fm_clampf((float)fxs_v[EN_FXS_RV_LEN], 0.0f, 2.0f)], avail);
    rv_pos = rv_len - 1.0f;
    bmode = BM_RVS;
    bm_start = rb_w - (uint32_t)(rv_len * 0.5f + 0.5f), bm_sm = bm_start % RB_N;
    bm_tgt = 0.3f + 0.007f * (float)fxs_v[EN_FXS_RV_MIX];
}

/* THE LINK (0.17.47, the maker: "like they detect same firmware and can double their efforts"). Two units running PERF (two FM-1s, an FM-1 and the
 * SMK), one the other's USB host, find each other and play as one band: the lead keeps the tempo (MIDI clock) and sends its key, scale, chord and
 * sections; the follower takes them (ui.c, lk_in) and plays its own lines over them; each can leave tracks to the other (Plays). All of it is control
 * changes on channel 16 (CC 20 .. 31: undefined in MIDI; the remote has 102 .. 117), sent from here, on the block a change happens (the UI's frame would add up to a frame of lateness):
 *   20 hello, about every second: the unit (1 FM-1, 2 SMK) + 64 if it leads     23 the root (0 .. 11)     24 the mode
 *   25, 26 the scale's notes (6 bits each, the low first; taken on 26)         27, 28 the chord's notes (pitch classes, 6 bits each), 29 its root (taken on 29)
 *   30 a section asked for on the next bar (EN_CMD_* + 1)                         31 the section now (EN_SEC_*)
 * Everything is sent again every four bars, so a follower that joins late (or missed one) catches up; one that joins while the lead plays gets
 * a Start (0xFA) on the lead's next bar line, so it plays from there, its bars the lead's. */
#ifndef EN_LINK_KIND
#define EN_LINK_KIND 1                           /* (the SMK's build: 2) */
#endif
void en_link(int q, int mute) { post(C_LINK, q, mute); }
#ifdef EN_HOST
int en_test_lk(int w) { return w ? lk_mute : lk_q; }
#endif
static void lk_cc(uint32_t cc, uint32_t v) { en_midi_out(0xBFu, cc, v & 0x7Fu); }
static void lk_send(uint32_t n)
{
    static uint32_t hello, cm_s, pool_s, all_s, b16;
    static uint8_t sync;
    static uint8_t root_s, mode_s, cr_s, req_s, sec_s, fresh = 1, said = 0;
    uint32_t cm = 0, i;
    if (muted) {                                 /* (the remote's, which speaks on channel 16 itself: nothing, the hello on its second after) */
        fresh = 1;
        hello = 0;
        return;
    }
    if (!(lk_q & EN_LK_HELLO)) {
        fresh = 1;
        said = 0;
        return;
    }
    hello += n;
    if (hello >= (uint32_t)EN_SR || !said || (lk_q & EN_LK_WANT ? 64u : 0u) != (said & 64u)) {   /* (at once when switched on, or the lead's bit changed) */
        hello = 0;
        said = (uint8_t)(0x80u | (lk_q & EN_LK_WANT ? 64u : 0u));
        lk_cc(20, EN_LINK_KIND | (said & 64u));
    }
    if (!(lk_q & EN_LK_LEAD)) {
        fresh = 1;
        sync = 1;
        return;
    }
    if (sync && en_playing && en_bars != b16 && !(en_bars & 15u)) {   /* a follower just linked: a Start on this bar line, so its bars are the lead's */
        en_midi_out(0xFAu, 0, 0);
        sync = 0;
    }
    b16 = en_bars;
    if ((en_bars >> 6) != all_s)                 /* every four bars: all of it again */
        all_s = en_bars >> 6, fresh = 1;
    for (i = 0; i < (uint32_t)chord_nt && i < 4u; i++)   /* (the chord's pitch classes, root first) */
        cm |= 1u << ((unsigned)chord_pcs[i] % 12u);
    if (fresh || par[P_ROOT] != root_s)
        root_s = (uint8_t)par[P_ROOT], lk_cc(23, root_s);
    if (fresh || par[P_MODE] != mode_s)
        mode_s = (uint8_t)par[P_MODE], lk_cc(24, mode_s);
    if (fresh || pool != pool_s)
        pool_s = pool, lk_cc(25, pool & 63u), lk_cc(26, pool >> 6 & 63u);
    if (fresh || cm != cm_s || (uint8_t)chord_pcs[0] != cr_s)
        cm_s = cm, cr_s = (uint8_t)chord_pcs[0], lk_cc(27, cm & 63u), lk_cc(28, cm >> 6 & 63u), lk_cc(29, cr_s);
    if (en_sec_req != req_s && (req_s = en_sec_req) != 0)
        lk_cc(30, req_s);
    if (fresh || en_sec != sec_s)
        sec_s = en_sec, lk_cc(31, sec_s);
    fresh = 0;
}
static void fx_control(uint32_t n)              /* once a chunk: what the held effects ask for */
{
    const uint64_t m = fx_req;
    uint32_t solo = trk_solo,
             mute = trk_mute | (uint32_t)(m >> FX_MUTE_KICK & 1u) << EN_TR_KICK | (uint32_t)(m >> FX_MUTE_HATS & 1u) << EN_TR_HATS |
                    (uint32_t)(m >> FX_MUTE_SNARE & 1u) << EN_TR_SNARE | (uint32_t)(m >> FX_MUTE_PERC & 1u) << EN_TR_PERC |
                    (uint32_t)(m >> FX_MDRUMS & 1u) * 0x0Fu | arr_m | lk_mute;   /* (M:DRUMS: all four; arr_m: the arrangement's rests; lk_mute: the link's partner's) */
    float dt = (float)n * (1.0f / EN_SR);
    int t, want = -1;
    lk_send(n);
    fx_on = m;
    {   /* the pattern gate: its loop's step now, open or closed, for each track it is on (gx_pat, the settings) */
        const uint32_t len = (uint32_t)fm_clampf((float)fxs_v[EN_FXS_GX_LEN], 2.0f, 16.0f), tg = (uint32_t)fxs_v[EN_FXS_GX_TGT];
        const int open = (int)(gx_pat >> ((sixteenth - 1u) % len) & 1u);
        const float closed = 1.0f - fm_clampf((float)fxs_v[EN_FXS_GX_DEPTH] * 0.01f, 0.0f, 1.0f);
        for (t = 0; t < EN_NTRACK; t++)
            gate_f[t] = (m >> FX_GATEX & 1u) && (tg >> t & 1u) && !open ? closed : 1.0f;
        kick_age += (float)n;
        if (m >> FX_PUMP & 1u) {                /* the pump: down on each period (or on each kick), back up over its release, on its tracks */
            static const uint8_t PER[4] = {8, 4, 2, 1};   /* sixteenths: 1/2 note, 1/4, 1/8, 1/16 */
            const int kk = fxs_v[EN_FXS_PM_RATE] >= 4;   /* Kick: it follows the kick, wherever the kicks are */
            const uint32_t per = PER[(int)fm_clampf((float)fxs_v[EN_FXS_PM_RATE], 0.0f, 3.0f)], pt = (uint32_t)fxs_v[EN_FXS_PM_TGT];
            const float ph = kk ? kick_age / (4.0f * step_len) : ((float)((sixteenth - 1u) % per) * step_len + (float)step_t) / ((float)per * step_len),
                        rel = 0.15f + 0.0085f * (float)fxs_v[EN_FXS_PM_REL], dp = (float)fxs_v[EN_FXS_PM_DEPTH] * 0.01f,
                        f = (1.0f - dp) + dp * fm_minf(ph / rel, 1.0f);
            for (t = 0; t < EN_NTRACK; t++)
                if (pt >> t & 1u)
                    gate_f[t] *= f;
        }
    }
    {   /* the intro: from Play the tracks come in one at a time over its length, the pad from the first bar, then the hats, perc, kick, snare, bass, pluck, acid
         * (each over a seventh of it), so a set can be mixed in under a pad and a hat rather than everything at once */
        static const uint8_t IN_AT[EN_NTRACK] = {40, 6, 52, 20, 62, 82, 0, 72};   /* kick, hats, snare, perc, bass, acid, pad, pluck: hundredths of the intro where each starts */
        float k = 1.0f;
        if (intro_len > 0 && en_playing) {
            if (intro_t < intro_len)
                intro_t += (int32_t)n;
            k = intro_t >= intro_len ? 1.0f : (float)intro_t / (float)intro_len;
        }
        en_intro_k = k;
        for (t = 0; t < EN_NTRACK; t++) {       /* mute and solo: 5 ms ramps */
            float tgt = (!(mute >> t & 1u) && (!solo || (solo >> t & 1u))) ? gate_f[t] : 0.0f, d = (float)n * (1.0f / 220.0f);
            if (k < 1.0f && IN_AT[t])
                tgt *= fm_clampf((k * 100.0f - (float)IN_AT[t]) * (1.0f / 14.0f), 0.0f, 1.0f);
            trk_g[t] = trk_g1[t];
            trk_g1[t] = trk_g[t] + fm_clampf(tgt - trk_g[t], -d, d);
        }
    }
    tg_inv = 1.0f / (float)n;
    lp_amt = fm_clampf(lp_amt + (m >> FX_LP & 1u ? dt * 2.0f : -dt * 6.0f), 0.0f, 1.0f);   /* in 0.5 s, out in 0.17 */
    hp_amt = fm_clampf(hp_amt + (m >> FX_HP & 1u ? dt * 2.0f : -dt * 6.0f), 0.0f, 1.0f);
    throw_key = fm_clampf(throw_key + (m >> FX_THROW & 1u ? dt * 60.0f : -dt * 60.0f), 0.0f, 1.0f);
    vin_m += ((float)(uint32_t)(m >> FX_VINYL & 1u) - vin_m) * fm_minf(dt * 8.0f, 1.0f);   /* the vinyl: ~0.15 s */
    rs_inv = 1.0f / (fm_exp2f((float)fxs_v[EN_FXS_RS_TIME] * 0.036f));          /* 1 .. 12 s to the top (rises a second at a time) */
    if (riser_cut) {                            /* the drop: gone before the first hit */
        riser_amt = 0.0f;
        riser_cut = 0;
    }
    riser_amt = fm_clampf(riser_amt + (m >> FX_RISER & 1u ? dt * rs_inv : -dt * 2.0f), 0.0f, 1.0f);   /* (and 0.5 s down) */
    rs_filt = (float)fxs_v[EN_FXS_RS_FILT] * 0.009f;                             /* the high-pass: up to 0.9 */
    rs_wash = (float)fxs_v[EN_FXS_RS_WASH] * 0.005f;                             /* the reverb: up to 0.5 */
    rs_echo = (float)fxs_v[EN_FXS_RS_ECHO] * 0.007f;                             /* the echo: up to 0.7 */
    roll_t = m >> FX_ROLL & 1u ? roll_t + dt * EN_SR / (4.0f * step_len) : 0.0f;   /* beats held */
    {   /* the repeat's settings: the pitch climbs to its target over four beats held, a decay each time round, the gate's share, the slip refresh (only where the
         * ring outlasts a beat or a bar with this slice: 21000 frames at 22.05 kHz less the slice, against the time to the next refresh) */
        rp_pit = fm_clampf((float)fxs_v[EN_FXS_RP_PITCH], 0.0f, 24.0f) - 12.0f;   /* (the rate itself is set as each repeat begins: fx_master) */
        if (bmode == BM_RPT)
            rp_t += dt * EN_SR / (4.0f * step_len);
        rp_step = fm_exp2f(-(float)fm_clampf((float)fxs_v[EN_FXS_RP_DECAY], 0.0f, 100.0f) * 0.06f * (1.0f / 6.0206f));
        rp_gatef = 1.0f - 0.25f * fm_clampf((float)fxs_v[EN_FXS_RP_GATE], 0.0f, 3.0f);
        rp_ref = (uint8_t)fm_clampf((float)fxs_v[EN_FXS_RP_REFRESH], 0.0f, 2.0f);
        rp_ok = (uint8_t)(((float)RB_N - bm_len * 0.5f) * (1.0f / 22050.0f) > (rp_ref == 2 ? 16.0f : 4.0f) * step_len * (1.0f / EN_SR));
    }
    {   /* tape, flanger, phaser: in and out over ~80 ms; the phaser's all-pass corners (left, right), swept */
        const float sm = fm_minf(dt * 12.0f, 1.0f);
        int c;
        rm_m += ((float)(uint32_t)(m >> FX_RING & 1u) - rm_m) * sm;
        rm_m = rm_m < 0.002f && !(m >> FX_RING & 1u) ? 0.0f : rm_m;
        rm_inc = 40.0f * fm_exp2f((float)fxs_v[EN_FXS_RM_FREQ] * 0.0433f) * (1.0f / EN_SR);
        rm_wet = (float)fxs_v[EN_FXS_RM_MIX] * 0.01f;
        sw_m += ((float)(uint32_t)(m >> FX_SWEEP & 1u) - sw_m) * sm;
        sw_m = sw_m < 0.002f && !(m >> FX_SWEEP & 1u) ? 0.0f : sw_m;
        if (sw_m > 0.0f) {                      /* the cutoff: a triangle over its period (2 bars .. 1/8 note), 120 Hz up by its depth */
            static const uint8_t SWP[7] = {128, 64, 32, 16, 8, 4, 2};   /* sixteenths: 8, 4, 2, 1 bars, 1/2, 1/4, 1/8 */
            const uint32_t per = SWP[(int)fm_clampf((float)fxs_v[EN_FXS_SW_RATE], 0.0f, 6.0f)];
            const float ph = ((float)((sixteenth - 1u) % per) * step_len + (float)step_t) / ((float)per * step_len),
                        tri = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
            const float kr = 1.4f - 0.0128f * (float)fxs_v[EN_FXS_SW_RESO];
            sw_lt = 6.9069f + (float)fxs_v[EN_FXS_SW_DEPTH] * 0.07f * tri;   /* log2 of the corner: 120 Hz up by the depth (the sample loop glides to it) */
            sw_k1 = fxs_v[EN_FXS_SW_TYPE] == 2 ? 1.0f : kr;   /* band-pass: a wide base (always), and the resonance a narrow spike on top of it */
            sw_k2 = kr;
            if (sw_m <= 0.03f || sw_lf <= 0.0f)  /* (coming in: from where it is asked, no glide up from nothing) */
                sw_lf = sw_lt;
        }
        tk_m += ((float)(uint32_t)(m >> FX_TALK & 1u) - tk_m) * sm;           /* TALK: a vowel filter across everything, sweeping A E I O U with the tempo */
        tk_m = tk_m < 0.002f && !(m >> FX_TALK & 1u) ? 0.0f : tk_m;
        if (tk_m > 0.0f) {
            static const uint8_t TKP[5] = {32, 16, 8, 4, 2};   /* sixteenths: 2 bars, 1 bar, 1/2, 1/4, 1/8 */
            static const float VF1[5] = {700.0f, 530.0f, 270.0f, 570.0f, 300.0f}, VF2[5] = {1220.0f, 1840.0f, 2290.0f, 840.0f, 870.0f};   /* A E I O U */
            const uint32_t per = TKP[(int)fm_clampf((float)fxs_v[EN_FXS_TK_RATE], 0.0f, 4.0f)];
            const float ph = ((float)((sixteenth - 1u) % per) * step_len + (float)step_t) / ((float)per * step_len), tri = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f,
                        pos = fm_clampf(tri * 4.0f * (0.15f + 0.85f * (float)fxs_v[EN_FXS_TK_DEPTH] * 0.01f), 0.0f, 4.0f), k = 0.5f - 0.0042f * (float)fxs_v[EN_FXS_TK_RESO];
            const int i = pos >= 4.0f ? 3 : (int)pos;
            const float fr = pos - (float)i;
            svf_coefs(tk_c[0], VF1[i] + (VF1[i + 1] - VF1[i]) * fr, k);
            svf_coefs(tk_c[1], VF2[i] + (VF2[i + 1] - VF2[i]) * fr, k);
        }
        wh_m += ((float)(uint32_t)(m >> FX_WAH & 1u) - wh_m) * sm;            /* WAH: a band-pass that follows how loud it is */
        wh_m = wh_m < 0.002f && !(m >> FX_WAH & 1u) ? 0.0f : wh_m;
        tp_m += ((float)(uint32_t)(m >> FX_TAPE & 1u) - tp_m) * sm;
        fl_m += ((float)(uint32_t)(m >> FX_FLANGER & 1u) - fl_m) * sm;
        ph_m += ((float)(uint32_t)(m >> FX_PHASER & 1u) - ph_m) * sm;
        tp_m = tp_m < 0.002f && !(m >> FX_TAPE & 1u) ? 0.0f : tp_m;
        fl_m = fl_m < 0.002f && !(m >> FX_FLANGER & 1u) ? 0.0f : fl_m;
        ph_m = ph_m < 0.002f && !(m >> FX_PHASER & 1u) ? 0.0f : ph_m;
        tfx_m += ((float)(uint32_t)(m >> FX_TFILT & 1u) - tfx_m) * sm;     /* the punch-ins on tracks: faded as the others */
        tdx_m += ((float)(uint32_t)(m >> FX_TDELAY & 1u) - tdx_m) * sm;
        tpx_m += ((float)(uint32_t)(m >> FX_TPHASE & 1u) - tpx_m) * sm;
        tfx_m = tfx_m < 0.002f && !(m >> FX_TFILT & 1u) ? 0.0f : tfx_m;
        tdx_m = tdx_m < 0.002f && !(m >> FX_TDELAY & 1u) ? 0.0f : tdx_m;
        tpx_m = tpx_m < 0.002f && !(m >> FX_TPHASE & 1u) ? 0.0f : tpx_m;
        {
#define PT_OF(i) PT_MASK[(unsigned)fxs_v[i] < EN_NPT ? fxs_v[i] : 0]
            const uint32_t bm = (tfx_m > 0.0f ? PT_OF(EN_FXS_TF_TGT) : 0u) | (tdx_m > 0.0f ? PT_OF(EN_FXS_TD_TGT) : 0u) | (tpx_m > 0.0f ? PT_OF(EN_FXS_TP_TGT) : 0u);
            if (bm && !bus_m) {                 /* the bus starts: its effects' states from silence */
                int c, k;
                for (c = 0; c < 4; c++)
                    tf_s[c] = 0.0f;
                for (c = 0; c < 2; c++) {
                    tp_fb[c] = 0.0f;
                    for (k = 0; k < 4; k++)
                        tp_z[c][k] = 0.0f;
                }
            }
            bus_m = bm;
        }
        if (tfx_m > 0.0f) {                     /* 40 Hz .. 16 kHz; resonance from none to near ringing */
            svf_coefs(tf_c, 40.0f * fm_exp2f((float)fxs_v[EN_FXS_TF_CUT] * 0.0864f + lfo_tcut), 2.0f - 0.0195f * (float)fxs_v[EN_FXS_TF_RESO]);
            tf_type = (uint8_t)((unsigned)fxs_v[EN_FXS_TF_TYPE] < 5u ? fxs_v[EN_FXS_TF_TYPE] : (unsigned)fxs_v[EN_FXS_TF_TYPE] <= 25u ? TF_MORPH : 0);
            if (tf_type == TF_MORPH)
                tf_mt = (float)(fxs_v[EN_FXS_TF_TYPE] - 5) * 0.05f;
            else
                tf_m = -1.0f;
        }
        if (tpx_m > 0.0f) {                     /* the phaser's sweep, as the master's: 0.08 .. 2.6 Hz, up to 4.5 octaves from 250 Hz */
            int c;
            const float oct = (float)fxs_v[EN_FXS_TP_DEPTH] * 0.045f;
            tp_ph += dt * 0.08f * fm_exp2f((float)fxs_v[EN_FXS_TP_RATE] * 0.05f);
            tp_ph -= (float)(int)tp_ph;
            for (c = 0; c < 2; c++) {
                const float x = fm_sin_turns(tp_ph + 0.25f * (float)c) * 0.5f + 0.5f;
                const float w = fm_minf(3.14159265f * 250.0f * fm_exp2f(oct * x) * (1.0f / EN_SR), 0.9f);
                tp_a[c] = (1.0f - w) / (1.0f + w);
            }
        }
        ph_fbk = (float)fxs_v[EN_FXS_PH_FB] * 0.009f;     /* 0 .. 0.9 */
        ph_wet = (float)fxs_v[EN_FXS_PH_MIX] * 0.005f;    /* the wet's share: 0 .. 0.5 (half and half: the deepest notches) */
        ph_k0 = (1.0f - ph_wet) * 0.88f;
        ph_k1 = ph_wet * 0.88f / (1.0f + 0.6f * ph_fbk);    /* (the feedback adds level) */
        for (c = 0; c < 2; c++) {                         /* the denormal flush, once a chunk */
            ph_fb[c] = fm_flush(ph_fb[c]);
            ph_z[c][0] = fm_flush(ph_z[c][0]);
            ph_z[c][1] = fm_flush(ph_z[c][1]);
            ph_z[c][2] = fm_flush(ph_z[c][2]);
            ph_z[c][3] = fm_flush(ph_z[c][3]);
        }
        if (ph_m > 0.0f) {                      /* rate 0.08 .. 2.6 Hz; the sweep 0 .. 4.5 octaves up from 250 Hz */
            const float oct = (float)fxs_v[EN_FXS_PH_DEPTH] * 0.045f;
            ph_ph += dt * 0.08f * fm_exp2f((float)fxs_v[EN_FXS_PH_RATE] * 0.05f);
            ph_ph -= (float)(int)ph_ph;
            for (c = 0; c < 2; c++) {
                float x = fm_sin_turns(ph_ph + 0.25f * (float)c) * 0.5f + 0.5f;        /* 0..1 */
                float w = 3.14159265f * 250.0f * fm_exp2f(oct * x) * (1.0f / EN_SR);
                w = fm_minf(w, 0.9f);
                ph_a[c] = (1.0f - w) / (1.0f + w);
            }
        }
        fl_inc = 0.05f * fm_exp2f((float)fxs_v[EN_FXS_FL_RATE] * 0.06f) * (1.0f / EN_SR);   /* 0.05 .. 3.2 Hz */
        fl_rng = (float)fxs_v[EN_FXS_FL_DEPTH] * 1.1f;                                  /* the sweep: 0 .. 110 samples */
        fl_fbk = (float)fxs_v[EN_FXS_FL_FB] * 0.0095f;                                  /* 0 .. 0.95 */
        fl_wet = (float)fxs_v[EN_FXS_FL_MIX] * 0.009f;                                  /* 0 .. 0.9 */
        tp_wd = (float)fxs_v[EN_FXS_TP_WOW];                                           /* wow: 0 .. 100 samples either way (0.7 Hz) */
        tp_fd = (float)fxs_v[EN_FXS_TP_FLUT] * 0.2f;                                    /* flutter: 0 .. 20 (8 Hz) */
        tp_drive = 1.0f + (float)fxs_v[EN_FXS_TP_DRIVE] * 0.025f;                       /* saturation: 1 .. 3.5 */
        tp_idrive = 1.0f / tp_drive;                                                    /* (a division once a chunk, not per sample) */
        fl_inv = 1.0f / (1.0f + 0.85f * fl_wet * fl_m);
        tp_k = 1.0f - (float)fxs_v[EN_FXS_TP_DULL] * 0.009f;                            /* the top: 1 (open) .. 0.1 (dull) */
        vn_g = (float)fxs_v[EN_FXS_VN_LEVEL] * 0.0144f;   /* (the loudest ticks at 50 are near -15 dBFS, the whole near -45; at 100, 6 dB more) */
        {   /* 1 .. 128 pops a second; above 10 a second each pop is quieter, by the square root, so the level stays: Crackle is how many */
            const float pps = fm_exp2f((float)fxs_v[EN_FXS_VN_CRACK] * 0.07f);
            vn_pop = pps * (1.0f / EN_SR);
            vn_amp = pps > 10.0f ? fm_sqrtf(10.0f / pps) : 1.0f;
        }
        vn_hiss = (float)fxs_v[EN_FXS_VN_HISS] * 0.0012f;
        nz_lp_k = 1.0f - fm_expf(-FM_TWO_PI * 300.0f * fm_exp2f((float)fxs_v[EN_FXS_VN_TONE] * 0.06f) * (1.0f / EN_SR));
    }
    {   /* the FX knobs, smoothed (~50 ms); each takes over from its key when it is the higher */
        const float sm = fm_minf(dt * 20.0f, 1.0f);
        float f = (float)fxk_v[EN_FXK_FILTER] * 0.02f, ca;   /* -1 .. 1 */
        f = f > -0.06f && f < 0.06f ? 0.0f : f;              /* a dead zone at the centre: off */
        lpk_s += ((f < 0.0f ? -f : 0.0f) - lpk_s) * sm;
        hpk_s += ((f > 0.0f ? f : 0.0f) - hpk_s) * sm;
        crk_s += ((float)fxk_v[EN_FXK_CRUSH] * 0.01f - crk_s) * sm;
        thk_s += ((float)fxk_v[EN_FXK_THROW] * 0.009f - thk_s) * sm;
        wash_s += ((float)fxk_v[EN_FXK_WASH] * 0.01f - wash_s) * sm;
        lpk_s = fm_flush(lpk_s);
        hpk_s = fm_flush(hpk_s);
        crk_s = fm_flush(crk_s);
        thk_s = fm_flush(thk_s);
        wash_s = fm_flush(wash_s);
        throw_amt = fm_maxf(fm_maxf(throw_key, thk_s), rs_echo * riser_amt);
        wash_e = fm_maxf(wash_s, rs_wash * riser_amt);
        /* the crusher: its key is a gentle 0.6; the knob goes on to 1 (5.5 kHz, 5 bits, as the key was) */
        ca = crk_s;
        cr_hold = 1 + (int)(7.0f * ca);
        cr_lv = fm_exp2f(11.0f - 7.0f * ca);
        cr_wet = fm_minf(ca * 3.0f, 1.0f);
        lp_amt_e = fm_maxf(fm_maxf(lp_amt, lpk_s), 0.35f * fm_maxf(brk, 0.0f));   /* (a breakdown darkens the whole mix) */
        hp_amt_e = fm_maxf(fm_maxf(hp_amt, hpk_s), rs_filt * riser_amt);   /* the build: a high-pass rising */
    }
    {   /* (0.17.49) THE DUB THROW: a throw by a key (THROW, or the DELAY punch-in on its tracks) lets its send go at once when the key is let go, as
         * before, but its repeats stay up for a bar after (over the next downbeat), then die away over half a bar: the echo carries the throw.
         * Before, the repeats fell back with the send. (The THROW knob and the swell are steered by hand: not held.) */
        static float th_tail, th_hold;
        const float th_src = fm_maxf(fm_maxf(throw_key, tdx_m * (float)fxs_v[EN_FXS_TD_FB] * 0.0095f), brk_throw);   /* (0.17.54: a breakdown's start throws the echo: the last bar rings on into it) */
        brk_throw = 0.0f;
        if (th_src >= th_tail) {
            th_tail = th_src;
            th_hold = 16.0f * step_len;
        } else if (th_hold > 0.0f) {
            th_hold -= (float)n;
        } else {
            th_tail = fm_flush(th_tail * fm_expf(-(float)n / (8.0f * step_len)));
        }
        echo_fbx = echo_fb + (0.86f - echo_fb) * fm_maxf(fm_maxf(throw_amt, tdx_m * (float)fxs_v[EN_FXS_TD_FB] * 0.0095f), th_tail);   /* (the punch-in DELAY's repeats too) */
    }
    if (fill_hold != (uint8_t)(m >> FX_FILL & 1u)) {
        fill_hold = (uint8_t)(m >> FX_FILL & 1u);
        drum_publish();
    }
    if (lp_amt_e > 0.0f)                        /* 16 kHz down to ~250 Hz */
        svf_coefs(lp_c, 16000.0f * fm_exp2f(-6.0f * lp_amt_e), 0.8f);
    if (hp_amt_e > 0.0f)                        /* 20 Hz up to ~1.6 kHz */
        svf_coefs(hp_c, 20.0f * fm_exp2f(6.3f * hp_amt_e), 0.8f);
    for (t = 0; t < 4; t++) {
        lp_s[t] = fm_flush(lp_s[t]);
        hp_s[t] = fm_flush(hp_s[t]);
    }
    {   /* the ring: the shortest repeat held (its index into RPT_BEATS) */
        static const uint8_t RB[8] = {FX_RPT2, FX_RPT4, FX_RPT6, FX_RPT8, FX_RPT16, FX_RPT32, FX_RPT8T, FX_RPT16T};
        for (t = 0; t < 8; t++)
            if ((m >> RB[t] & 1u) && (want < 0 || RPT_BEATS[t] < RPT_BEATS[want]))
                want = t;
    }
    if (!(m >> FX_RVSWELL & 1u)) {
        rv_held = 0;
        rvs_arm = 0;
    }
    if (roll_t > 0.0f) {                        /* a roll: from its first rate toward its last, a stage of its time at each (a repeat held beats it) */
        static const float RL_BEATS[4] = {0.5f, 1.0f, 2.0f, 4.0f};
        const int s = (int)fm_clampf((float)fxs_v[EN_FXS_RL_START], 0.0f, 6.0f), e = (int)fm_clampf((float)fxs_v[EN_FXS_RL_END], 0.0f, 6.0f),
                  tm = (int)fm_clampf((float)fxs_v[EN_FXS_RL_TIME], 0.0f, 3.0f), stage = (int)(roll_t / RL_BEATS[tm]);
        const int rr = s < e ? (s + stage < e ? s + stage : e) : (s - stage > e ? s - stage : e);   /* the rate: 0..5 are 1/2 .. 1/32, 6 is 1/64 */
        const int r = FX_RPT2 + (rr == 6 ? 8 : rr);
        if (want < 0 || RPT_BEATS[r] < RPT_BEATS[want])
            want = r;
        rl_gain = 1.0f + (float)fxs_v[EN_FXS_RL_SWELL] * 0.008f * fm_minf(roll_t * 0.25f, 1.0f);   /* a swell, to +80% over four beats */
    } else {
        rl_gain = 1.0f;
    }
    if (!(m & FLEXMASK)) {                      /* no Flex Beat pad down: let go (a one shot plays out its bar) */
        flx_held = 0;
        flx_wait_rel = 0;
        if (flx_arm) {
            flx_arm = 0;
            en_flx_state = flx_active ? 2 : 0;
        }
    }
    if (m >> FX_STOP & 1u) {
        rpt_arm = 0;
        if (bmode != BM_STOP || bm_tgt < 1.0f) {
            bmode = BM_STOP;
            bm_start = rb_w - 2u, bm_sm = bm_start % RB_N;
            bm_off = 0.0f;
            bm_speed = 1.0f;
            bm_tgt = 1.0f;
        }
    } else if (m >> FX_REV & 1u) {
        rpt_arm = 0;
        if (bmode != BM_REV || bm_tgt < 1.0f) {
            bmode = BM_REV;
            bm_len = (float)(RB_N - 8) * 2.0f;
            bm_start = rb_w - (RB_N - 8), bm_sm = bm_start % RB_N;
            bm_off = bm_len - 1.0f;
            bm_tgt = 1.0f;
        }
    } else if (m & FLEXMASK) {                  /* Flex Beat: the pattern en_flex_select chose */
        const int p = flx_sel;
        rpt_arm = 0;
        scat_arm = 0;
        var_exit = 0;
        flx_held = 1;
        if (flx_active)
            bm_tgt = 0.01f * (float)fm_clampf((float)fxs_v[EN_FXS_FX_MIX], 0.0f, 100.0f);   /* (Mix turned while it plays) */
        if (!flx_wait_rel && (flx_active ? (flx_pat != p && flx_arm != p + 1) : flx_arm != p + 1)) {
            flx_arm = (uint8_t)(p + 1);
            en_flx_state = 1;
            if (!en_playing || en_ext) {        /* no grid to wait for */
                flx_pat = (uint8_t)p;
                flx_arm = 0;
                flx_engage();
            }
        }
    } else if (m >> FX_SCATTER & 1u) {
        rpt_arm = 0;
        var_exit = 0;
        if (bmode != BM_SCAT || bm_tgt < 1.0f) {
            if (!en_playing || en_ext)
                scat_engage();
            else
                scat_arm = 1;
        }
    } else if (m & (FXB(FX_BRAKE) | FXB(FX_SPIN) | FXB(FX_HALF))) {
        const int wh = m >> FX_BRAKE & 1u ? FX_BRAKE : m >> FX_SPIN & 1u ? FX_SPIN : FX_HALF;
        rpt_arm = 0;
        scat_arm = 0;
        var_exit = 0;
        if (bmode != BM_VAR || bm_tgt < 1.0f || vr_which != wh)
            var_engage(wh);
        var_params(wh);
    } else if (m >> FX_SCRATCH & 1u) {
        rpt_arm = 0;
        scat_arm = 0;
        var_exit = 0;
        if (bmode != BM_SCR || bm_tgt < 1.0f)
            scr_engage();
    } else if (m >> FX_FREEZE & 1u) {
        rpt_arm = 0;
        scat_arm = 0;
        var_exit = 0;
        if (bmode != BM_GRN || bm_tgt < 1.0f)
            grn_engage();
    } else if (m >> FX_RVSWELL & 1u) {
        rpt_arm = 0;
        scat_arm = 0;
        var_exit = 0;
        if (!rv_held) {                         /* one swell for each press */
            rv_held = 1;
            if (!en_playing || en_ext)
                rvs_engage();
            else
                rvs_arm = 1;
        }
    } else if (bmode == BM_VAR && bm_tgt >= 1.0f) {   /* let go: spin back up, and out on the grid */
        vr_tgt = 1.0f;
        vr_c = vr_time((float)fxs_v[EN_FXS_BR_RET], 0.02f);
        if (!en_playing || en_ext)
            bm_tgt = 0.0f;
        else
            var_exit = 1;
    } else if (want >= 0) {
        if (bmode != BM_RPT || brate != want || bm_tgt < 1.0f) {
            if (!en_playing || en_ext)
                rpt_engage(want);               /* no grid to wait for */
            else
                rpt_arm = (uint8_t)(want + 1);
        }
    } else if (flx_active) {
        /* a Flex Beat pattern plays out its bar (a loop let go ends at the bar line too) */
    } else {
        rpt_arm = 0;
        bm_tgt = 0.0f;                          /* back to the live mix (the ring fades out) */
    }
}

/* ----------------------------------------------------- the master bus: a compressor and a limiter --- */
/* After the performance effects, before the master gain. The compressor: a stereo-linked peak detector in dB, a soft knee (6 dB), the
 * ratio, the threshold, a fast attack (4 ms) and a release that follows the programme (it is slower the deeper the reduction), the
 * make-up. The limiter: a look-ahead brick wall: the signal is delayed 64 samples (1.5 ms), the gain it needs for the peak that is
 * coming is the minimum over the window, smoothed over the window (so it is in place when the peak arrives and never steps), and
 * released over about 60 ms; the ceiling is its level, 0 dB (the mix's full scale before the master gain) down to -12 dB. */
#define LK 64
#define LKS 8                                   /* the window's sub-blocks of 8 samples */
static float lk_l[LK], lk_r[LK], lk_gm[LK], lk_sub[LKS];
static float lk_cur = 1.0f, lk_sum = (float)LK, lk_rs = 1.0f, cp_env;
static float po_pk;                             /* (0.17.59) the mix's peak going into the limiter (instant up, ~0.3 s down): Polish rounds at it scaled to the Ceiling */
static uint32_t lk_chk;
static float cp_g = 1.0f, lk_subm = 1.0f;       /* the compressor's gain at the end of the last block; the minimum of lk_sub (it changes every 8 samples) */
static uint32_t lk_i, lk_n, lk_si;
static float cp_thr = 0.0f, cp_slope, cp_make = 1.0f, cp_ka = 0.05f, cp_kr = 0.0005f, lk_ceil = 1.0f;
volatile float en_comp_gr, en_lim_gr, en_out_peak;
static float gr_hold, pk_hold, cp_hl, cp_hr, cp_avg;    /* (cp_hl, cp_hr: the compressor's ear, the lows it leaves out) */

static void reverb_set(void)                    /* once a chunk, before the tank's job: the type, damping, predelay */
{
    rv_type = par[P_RV_TYPE] < 0 ? 0 : par[P_RV_TYPE] > 2 ? 2 : par[P_RV_TYPE];
    rv_damp = fm_maxf(0.85f - 0.0085f * (float)par[P_RV_DAMP], 0.03f);   /* (0.17.26) floored: at 0 the tank's damping froze, the tail died and a DC step stayed */
    rv_pre = 256 + (PRE_D - 256) * par[P_RV_PRE] / 100;                    /* 6 .. 30 ms (never under a chunk: the line is written as it is read) */
    rv_shim = 0.05f + 0.30f * (float)par[P_RV_X] * 0.01f;                  /* the shimmer's feedback */
    rv_gate_hold = (0.08f + 0.32f * (float)par[P_RV_X] * 0.01f) * EN_SR;   /* the gate's hold: 80 .. 400 ms */
    pl_damp_l = fm_flush(pl_damp_l);            /* (0.17.44: with the lines truncated a dying tail reaches 0, and these would park on the smallest subnormal) */
    pl_damp_r = fm_flush(pl_damp_r);
}

/* THE POLISH (0.17.30, the SOUND page; the outside research: the fuller firmwares' master chains). After the performance effects: the side below
 * about 140 Hz taken out (the lows mono: a club's, a phone's), the side above 2.5 kHz up by Width (air), and a tilt: a low shelf at 120 Hz and a
 * high one at 4 kHz, the genre's own (TILT, half dB) or warmer or brighter. Before the limiter, with Polish on: the peaks above -4.4 dB of the
 * ceiling rounded (tanh), so the limiter has less to catch. One-pole filters: about 30 sums a sample, on the first core. */
static const int8_t TILT[EN_NGENRE][2] = {   /* half dB: the low shelf, the high shelf. From the sound check of all 180 pieces (0.17.30): every genre's slope
                                              * is inside the masters' 3-5 dB an octave; the tops nearly gone in reggae, dub, lofi, trip-hop and funk
                                              * (0.03-0.18 % of the energy above 4 kHz) get a little back, the thinnest lows (DnB, psy: 85 % below 250 Hz) a
                                              * little more; the rest as they were */
    {0, 0}, {0, 1}, {0, 0}, {0, 0}, {-1, 2}, {2, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},   /* HYPNO LOFI HOUSE DUBSTEP DUB DNB BREAKS GARAGE ELECTRO TRAP */
    {0, 0}, {0, 1}, {0, 3}, {0, 0}, {0, 0}, {0, 1}, {0, 0}, {2, 0}, {0, 0},           /* DISCO FUNK REGGAE SYNTHWV AMAPI TRIPHOP INDUS PSY FOREST */
    {1, 0}, {0, 0}, {0, 0}};                                                         /* HIPHOP TRANCE AMBIENT */
static float po_s1, po_s2, po_l1, po_l2, po_r1, po_r2, po_glo, po_ghi, po_w, pc_k = 0.6f, pc_r = 0.45f, pc_ir = 2.2222f;
static uint8_t po_on, po_clip;
static inline void polish(float *pl, float *pr)
{
    float m = 0.5f * (*pl + *pr), s = 0.5f * (*pl - *pr), l, r;
    po_s1 += (s - po_s1) * 0.019749f;            /* the side below 140 Hz */
    po_s2 += (s - po_s2) * 0.29967f;             /* and below 2.5 kHz */
    s += po_w * (s - po_s2) - (po_clip ? po_s1 : 0.0f);
    l = m + s;
    r = m - s;
    if (po_glo != 0.0f || po_ghi != 0.0f) {
        po_l1 += (l - po_l1) * 0.016952f;        /* 120 Hz */
        po_l2 += (l - po_l2) * 0.43444f;         /* 4 kHz */
        po_r1 += (r - po_r1) * 0.016952f;
        po_r2 += (r - po_r2) * 0.43444f;
        l += po_glo * po_l1 + po_ghi * (l - po_l2);
        r += po_glo * po_r1 + po_ghi * (r - po_r2);
    }
    *pl = l;
    *pr = r;
}
static inline float pclip(float x)              /* the peaks rounded: straight up to the knee, then a tanh towards a little over the ceiling */
{
    const float a = fm_fabsf(x);
    float y;
    if (a <= pc_k)
        return x;
    y = pc_k + pc_r * fm_tanhf((a - pc_k) * pc_ir);
    return x < 0.0f ? -y : y;
}
static void polish_set(void)                    /* once a chunk */
{
    const int t = fxs_v[EN_FXS_TILT], g = (unsigned)chord_genre < EN_NGENRE ? chord_genre : 0;
    float lo = 0.0f, hi = 0.0f;
    if (t == 1) {
        lo = 0.5f * (float)TILT[g][0];
        hi = 0.5f * (float)TILT[g][1];
    } else if (t == 2) {
        lo = 1.5f;
        hi = -1.5f;
    } else if (t == 3) {
        lo = -0.5f;
        hi = 2.0f;
    }
    po_on = en_shed_level < 3;                  /* (the heaviest shedding: none of it) */
    po_clip = fxs_v[EN_FXS_POLISH] > 0 && po_on;
    po_glo = lo != 0.0f ? fm_db2lin(lo) - 1.0f : 0.0f;
    po_ghi = hi != 0.0f ? fm_db2lin(hi) - 1.0f : 0.0f;
    po_w = fxs_v[EN_FXS_WIDTH] > 0 ? fm_db2lin(0.06f * (float)fxs_v[EN_FXS_WIDTH]) - 1.0f : 0.0f;
    pc_k = 0.6f * lk_ceil;
    pc_r = 0.45f * lk_ceil;
    pc_ir = 1.0f / pc_r;
    po_s1 = fm_flush(po_s1);
    po_s2 = fm_flush(po_s2);
    po_l1 = fm_flush(po_l1);
    po_l2 = fm_flush(po_l2);
    po_r1 = fm_flush(po_r1);
    po_r2 = fm_flush(po_r2);
}
static void jc_set(void);
static void master_dyn_set(void)                /* once a chunk: the settings, from the parameters */
{
    static const float RATIO[7] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f};
    const int r = par[P_MC_RATIO] < 0 ? 0 : par[P_MC_RATIO] > 6 ? 6 : par[P_MC_RATIO];
    cp_slope = r ? 1.0f - 1.0f / RATIO[r] : 0.0f;
    cp_thr = -30.0f + 0.3f * (float)par[P_MC_THR];                 /* -30 .. 0 dB */
    cp_make = fm_db2lin(0.12f * (float)par[P_MC_MAKE] + 0.8f * cp_avg);   /* 0 .. +12 dB, and (0.17.42) most of what it takes on average given back: the level kept */
    cp_ka = 1.0f - fm_expf(-1.0f / (0.010f * EN_SR));              /* (0.17.42: 10 ms, was 4: a drum's hit gets through before the bus comes down, the punch kept) */
    cp_kr = 1.0f - fm_expf(-1.0f / ((0.08f + 0.05f * cp_env) * EN_SR));     /* the release: ~80 ms, up to 530 ms at 9 dB of reduction (set once a chunk) */
    lk_ceil = fm_db2lin(-12.0f + 0.12f * (float)par[P_ML_CEIL]);   /* -12 .. 0 dB */
    jc_set();                                   /* (0.17.44) the CHORUS page */
    polish_set();
}

static void master_dyn_reset(void)              /* a new engine: nothing delayed, no reduction */
{
    uint32_t k;
    for (k = 0; k < LK; k++)
        lk_l[k] = lk_r[k] = 0.0f, lk_gm[k] = 1.0f;
    for (k = 0; k < LKS; k++)
        lk_sub[k] = 1.0f;
    lk_cur = lk_rs = lk_subm = cp_g = 1.0f;
    lk_sum = (float)LK;
    lk_i = lk_n = lk_si = 0;
    cp_env = gr_hold = pk_hold = cp_hl = cp_hr = cp_avg = 0.0f;
    po_pk = 0.0f;                               /* (0.17.62) Polish's peak too (an outside review: the last engine's carried into the new one) */
}


/* a block of the master bus. The compressor decides once a 4 samples (the loudest of them, one log and one exp for four) and its gain is
 * ramped across them; the limiter's window minimum is kept, not rescanned. */
static void master_dyn_block(float *pl, float *pr, uint32_t n)
{
    static const float INV[5] = {0.0f, 1.0f, 0.5f, 0.33333334f, 0.25f};
    uint32_t j = 0;
    while (j < n) {
        const uint32_t m = n - j > 4u ? 4u : n - j;
        float g1 = 1.0f, gs, g0 = cp_g;
        uint32_t i;
        if (cp_slope > 0.0f) {                  /* the compressor */
            float d = 0.0f, gr = 0.0f, lv;
            for (i = 0; i < m; i++) {           /* (0.17.42) it listens through a high-pass at ~100 Hz: the kick and the sub no longer pull the whole mix down */
                cp_hl += (pl[j + i] - cp_hl) * 0.0142f;
                cp_hr += (pr[j + i] - cp_hr) * 0.0142f;
                d = fm_maxf(d, fm_maxf(fm_fabsf(pl[j + i] - cp_hl), fm_fabsf(pr[j + i] - cp_hr)));
            }
            if (!(d < 64.0f)) {                 /* (0.17.48, the audit) a sample that is not a number: the detector's filters cleared, or they would keep it
                                                 * and the compressor would stop for good */
                cp_hl = cp_hr = 0.0f;
                d = 0.0f;
            }
            lv = 6.0206f * fm_log2f(d + 1e-9f) - cp_thr;
            if (lv > 3.0f)
                gr = cp_slope * lv;
            else if (lv > -3.0f)
                gr = cp_slope * (lv + 3.0f) * (lv + 3.0f) * (1.0f / 12.0f);
            cp_env = fm_flush(cp_env + (gr - cp_env) * ((gr > cp_env ? cp_ka : cp_kr) * (float)m));
            g1 = fm_db2lin(-cp_env);
            en_comp_gr = cp_env;
            cp_avg = fm_flush(cp_avg + (cp_env - cp_avg) * (0.00003f * (float)m));   /* its reduction on average, over ~0.75 s */
        } else {
            cp_env = cp_avg = 0.0f;
            en_comp_gr = 0.0f;
        }
        gs = (g1 - g0) * INV[m];
        float pg = 1.0f, ipg = 1.0f;
        if (po_clip) {                          /* (0.17.59) the sub-block's peak seen first, so a hit after a quiet spell is scaled too */
            const float gm = fm_maxf(g0, g1) * cp_make;
            float pk = po_pk;
            for (i = 0; i < m; i++)
                pk = fm_maxf(pk, fm_maxf(fm_fabsf(pl[j + i]), fm_fabsf(pr[j + i])) * gm);
            po_pk = fm_flush(pk * (1.0f - (float)m * (1.0f / (0.3f * EN_SR))));
            if (pk > lk_ceil) {
                pg = fm_maxf(lk_ceil / pk, 0.0625f);
                ipg = 1.0f / pg;
            }
        }
        for (i = 0; i < m; i++) {
            float l, r, p, gq, avg;
            g0 += gs;
            l = pl[j + i] * g0 * cp_make;
            r = pr[j + i] * g0 * cp_make;
            if (!(fm_fabsf(l) < 64.0f))         /* a sample that is not a number (or absurd) never reaches the limiter: its sums would keep it for good, and the output would stay silent */
                l = 0.0f;
            if (!(fm_fabsf(r) < 64.0f))
                r = 0.0f;
            if (po_clip) {                      /* (0.17.30) the polish: the peaks rounded before the limiter. (0.17.59) On the mix scaled so its recent peak
                                                 * sits at the Ceiling (pg), then scaled back: before, at the level after Make-up, so a piece with more
                                                 * Make-up than its Ceiling ran most of its music through the tanh (the maker: "major distortion" on saved
                                                 * pieces). What Polish adds to the mid, under the mix, piece 1 of TRANCE, HOUSE, DNB: Make-up 10 / Ceiling
                                                 * 100 -51 .. -60 dB (unchanged); 80 / 85 -21 .. -25 dB before, -32 .. -34 now; 100 / 70 -16 .. -19 before,
                                                 * -31 .. -32 now. (The limiter's own gain as the level did nothing: the rounding left it nothing to catch.) */
                l = pclip(l * pg) * ipg;
                r = pclip(r * pg) * ipg;
            }
            /* the limiter: the audio goes into the delay, the gain is figured for what is coming */
            p = fm_maxf(fm_fabsf(l), fm_fabsf(r));
            gq = p > lk_ceil ? lk_ceil / p : 1.0f;
            lk_l[lk_i] = l;
            lk_r[lk_i] = r;
            lk_cur = fm_minf(lk_cur, gq);       /* the window's minimum: the sub-block under way, and the eight before */
            avg = fm_minf(lk_cur, lk_subm);
            lk_sum += avg - lk_gm[lk_i];        /* smoothed over the window */
            lk_gm[lk_i] = avg;
            if (++lk_n >= 8u) {
                uint32_t k;
                float mn;
                lk_n = 0;
                lk_sub[lk_si] = lk_cur;
                lk_si = (lk_si + 1u) & (LKS - 1u);
                lk_cur = 1.0f;
                mn = lk_sub[0];
                for (k = 1; k < LKS; k++)
                    mn = fm_minf(mn, lk_sub[k]);
                lk_subm = mn;
            }
            lk_i = (lk_i + 1u) & (LK - 1u);
            if (!(++lk_chk & 1023u)) {          /* the running sum is made exact now and then (it drifts), and a sum or a smoothed gain gone wrong is put right */
                float sm = 0.0f;
                uint32_t k;
                for (k = 0; k < LK; k++)
                    sm += lk_gm[k];
                lk_sum = sm;
            }
            if (!(lk_sum > 0.0f && lk_sum < (float)LK * 1.001f) || !(lk_rs > 0.0f && lk_rs < 1.001f)) {
                uint32_t k;
                for (k = 0; k < LK; k++)
                    lk_gm[k] = 1.0f;
                lk_sum = (float)LK;
                lk_rs = lk_cur = lk_subm = 1.0f;
            }
            avg = lk_sum * (1.0f / (float)LK);
            if (avg < lk_rs)                    /* quick down, ~60 ms back up */
                lk_rs = avg;
            else
                lk_rs += (avg - lk_rs) * (1.0f / (0.06f * EN_SR));
            l = lk_l[lk_i] * lk_rs;             /* (lk_i is now the oldest: 64 samples ago) */
            r = lk_r[lk_i] * lk_rs;
            pl[j + i] = l;
            pr[j + i] = r;
            gr_hold = fm_flush(fm_maxf(gr_hold * 0.9998f, 1.0f - lk_rs));
            pk_hold = fm_flush(fm_maxf(pk_hold * 0.9997f, fm_maxf(fm_fabsf(l), fm_fabsf(r))));
        }
        cp_g = g1;
        j += m;
    }
}

static inline void fx_master(float *pl, float *pr)   /* one sample of the master, before the gain */
{
    float l = *pl, r = *pr;
    if (bmode == BM_LIVE || bmode == BM_STOP || bmode == BM_VAR || (bmode == BM_RPT && rp_ref && rp_ok))   /* (a refreshing repeat: the ring goes on recording under the loop) */
        rb_write(l, r);
    if (bmode != BM_LIVE) {
        float bl = 0.0f, br = 0.0f, w = 1.0f;
        if (bmode == BM_RPT) {
            /* the repeat keeps its interval whatever the pitch does to the slice: each time round starts on the grid (rp_pos counts the output), the slice is
             * read at the repeat's speed (bm_off), cut at the interval when it is slower, over early (then a rest) when it is faster; the pitch steps
             * at each start, climbing to what the page asks over four beats held; the decay too */
            if (bm_off < bm_len) {
                rb_read(bm_off, &bl, &br);
                w = fm_minf(fm_minf(fm_minf(bm_off, bm_len - bm_off), bm_len - rp_pos) * (1.0f / 64.0f), 1.0f) * rp_gain;   /* 1.5 ms edges, at the slice's ends and the interval's */
                if (rp_gatef < 1.0f)            /* the gate: the rest of the interval silent (a 1.5 ms edge) */
                    w = fm_minf(w, fm_maxf((bm_len * rp_gatef - rp_pos) * (1.0f / 64.0f), 0.0f));
                bm_off += rp_rate;
            } else {
                w = 0.0f;
            }
            if ((rp_pos += 1.0f) >= bm_len) {
                rp_pos -= bm_len;
                bm_off = 0.0f;
                rp_gain *= rp_step;
                rp_rate = rp_pit != 0.0f ? fm_exp2f(rp_pit * fm_minf(rp_t * 0.25f, 1.0f) * (1.0f / 12.0f)) : 1.0f;
            }
        } else if (bmode == BM_REV) {
            rb_read(bm_off, &bl, &br);
            w = fm_minf(fm_minf(bm_off, bm_len - bm_off) * (1.0f / 64.0f), 1.0f);
            if ((bm_off -= 1.0f) < 0.0f)
                bm_off += bm_len;
        } else if (bmode == BM_SCAT) {
            rb_read(sc_off + (sc_rev ? sc_len - 1.0f - sc_pos : sc_pos), &bl, &br);
            w = fm_minf(fm_minf(sc_pos, sc_len - 1.0f - sc_pos) * (1.0f / 48.0f), 1.0f);
            if ((sc_pos += 1.0f) >= sc_len)
                scat_pick();
        } else if (bmode == BM_SCR) {
            const float ph = scr_t / scr_per;
            float off;
            if (fxs_v[EN_FXS_SR_STYLE] == 0) {
                off = scr_w * (0.5f - 0.5f * fm_sin_turns(ph + 0.25f));    /* baby: out and back */
                w = 1.0f;
            } else {                                                       /* pull: forward at ease, back quickly with the fader cut */
                const float u = ph < 0.6f ? ph * (1.0f / 0.6f) : 1.0f - (ph - 0.6f) * 2.5f, e = u * u * (3.0f - 2.0f * u);
                off = scr_w * e;
                w = ph < 0.6f ? 1.0f : 0.2f;
            }
            rb_read(off, &bl, &br);
            if ((scr_t += 1.0f) >= scr_per)
                scr_t -= scr_per;
        } else if (bmode == BM_FLX) {
            if (flx_live) {                     /* the live mix goes through */
                bl = l;
                br = r;
            } else if (flx_mute) {
                w = 0.0f;
            } else {
                rb_read(flx_off + (flx_rev ? flx_sub - 1.0f - flx_p : flx_p), &bl, &br);
                w = fm_maxf(fm_minf(fm_minf(flx_p, flx_sub - 1.0f - flx_p) * (1.0f / 48.0f), 1.0f), 0.0f);
                if ((flx_p += 1.0f) >= flx_sub)
                    flx_p = 0.0f;               /* a stutter: the same piece again */
            }
        } else if (bmode == BM_GRN) {
            int i;
            for (i = 0; i < gn_n; i++) {
                float gl, gr, p = gn_pos[i] / gn_len, win = 4.0f * p * (1.0f - p);
                rb_read(gn_st[i] + gn_pos[i], &gl, &gr);
                bl += gl * win;
                br += gr * win;
                if ((gn_pos[i] += 1.0f) >= gn_len)
                    gn_respawn(i);
            }
            w = gn_scale;
        } else if (bmode == BM_RVS) {
            if (rv_pos >= 0.0f) {
                const float g = 1.0f - rv_pos / rv_len;
                rb_read(rv_pos, &bl, &br);
                w = g * g * fm_minf(rv_pos * (1.0f / 48.0f) + 0.0f, 1.0f);
                rv_pos -= 1.0f;
            } else {
                w = 0.0f;
                bm_tgt = 0.0f;                  /* the swell is over: the live mix alone */
            }
        } else if (bmode == BM_VAR) {
            float k, ar, lag;
            vr_rate += (vr_tgt - vr_rate) * vr_c;
            rb_read(bm_off, &bl, &br);
            bm_off += vr_rate;
            if (bm_off >= 16384.0f) {           /* (rb_read wraps once: keep the offset within a ring of its start) */
                bm_off -= 8192.0f;
                bm_start += 4096u;
                bm_sm += 4096u;
                if (bm_sm >= RB_N)
                    bm_sm -= RB_N;
            } else if (bm_off <= -16384.0f) {
                bm_off += 8192.0f;
                bm_start -= 4096u;
                bm_sm = bm_sm >= 4096u ? bm_sm - 4096u : bm_sm + RB_N - 4096u;
            }
            ar = vr_rate < 0.0f ? -vr_rate : vr_rate;
            k = fm_clampf(1.0f - vr_dull * (1.0f - fm_minf(ar, 1.0f)), 0.04f, 1.0f);
            vr_lp[0] += k * (bl - vr_lp[0]);
            vr_lp[1] += k * (br - vr_lp[1]);
            bl = vr_lp[0];
            br = vr_lp[1];
            w = fm_minf(ar * 8.0f, 1.0f);
            lag = 2.0f * (float)(rb_w - bm_start) - bm_off;      /* how far behind the live mix it is */
            if (lag > (float)(RB_N - 16) * 2.0f)
                bm_tgt = 0.0f;                  /* the ring is used up: back to the live mix */
        } else if (bm_speed > 0.0f) {          /* the tape slowing: a beat to a stop */
            rb_read(bm_off, &bl, &br);
            w = bm_speed;
            bm_off += bm_speed;
            bm_speed -= 1.0f / (4.0f * step_len);
        }
        bm_mix += (bm_tgt - bm_mix) * 0.02f;    /* ~1 ms */
        l += (bl * w * rl_gain - l) * bm_mix;
        r += (br * w * rl_gain - r) * bm_mix;
        if (bm_tgt <= 0.0f && bm_mix < 0.0005f) {
            bm_mix = 0.0f;
            bmode = BM_LIVE;
            brate = -1;
        }
    }
    if (lp_amt_e > 0.0f) {
        l = svf_lp(l, lp_c, &lp_s[0]);
        r = svf_lp(r, lp_c, &lp_s[2]);
    }
    if (hp_amt_e > 0.0f) {                      /* the high-pass: what the low-pass leaves out */
        l -= svf_lp(l, hp_c, &hp_s[0]);
        r -= svf_lp(r, hp_c, &hp_s[2]);
    }
    if (sw_m > 0.0f) {                          /* the sweep: low-, high- or band-pass, its corner moving with the tempo */
        const int type = (int)fm_clampf((float)fxs_v[EN_FXS_SW_TYPE], 0.0f, 2.0f);
        if (!(sw_cnt++ & 15u)) {                /* (0.17.18) every 16 samples the corner a step nearer the control's (about 3 ms behind it) */
            const float fc = fm_exp2f(sw_lf += (sw_lt - sw_lf) * 0.125f);
            svf_coefs(sw_c, fc, sw_k1);
            if (type == 2)
                svf_coefs(sw_c2, fc, sw_k2);
        }
        const float k = sw_c[3];
        float in[2] = {l, r}, o[2];
        int c;
        for (c = 0; c < 2; c++) {
            float *s = sw_s[c], x = in[c];
            const float v3 = x - s[1], v1 = sw_c[0] * s[0] + sw_c[1] * v3, v2 = s[1] + sw_c[1] * s[0] + sw_c[2] * v3;
            s[0] = fm_flush(2.0f * v1 - s[0]);
            s[1] = fm_flush(2.0f * v2 - s[1]);
            if (type == 2) {                    /* band: the wide one, and the resonant one's spike (as much as Reso) */
                float *s2 = sw_s2[c];
                const float u3 = x - s2[1], u1 = sw_c2[0] * s2[0] + sw_c2[1] * u3, u2 = s2[1] + sw_c2[1] * s2[0] + sw_c2[2] * u3;
                s2[0] = fm_flush(2.0f * u1 - s2[0]);
                s2[1] = fm_flush(2.0f * u2 - s2[1]);
                o[c] = k * v1 * 1.3f + sw_c2[3] * u1 * (float)fxs_v[EN_FXS_SW_RESO] * 0.012f;
            } else {
                o[c] = type == 0 ? v2 : x - k * v1 - v2;   /* low, high */
            }
        }
        l += (o[0] - l) * sw_m;
        r += (o[1] - r) * sw_m;
    }
    if (tk_m > 0.0f) {                          /* TALK: two formants (band-passes), summed */
        const float w = tk_m * 0.01f * (float)fm_clampf((float)fxs_v[EN_FXS_TK_MIX], 0.0f, 100.0f);
        float in[2] = {l, r}, o[2];
        int c, f;
        for (c = 0; c < 2; c++) {
            float y = 0.0f, hi;
            tk_lo[c] = fm_flush(tk_lo[c] + XO_A * (in[c] - tk_lo[c]));   /* (0.17.19) the kick and the bass stay as they are */
            hi = in[c] - tk_lo[c];
            for (f = 0; f < 2; f++) {
                float *s = tk_s[c][f];
                const float *cc = tk_c[f], v3 = hi - s[1], v1 = cc[0] * s[0] + cc[1] * v3, v2 = s[1] + cc[1] * s[0] + cc[2] * v3;
                s[0] = fm_flush(2.0f * v1 - s[0]);
                s[1] = fm_flush(2.0f * v2 - s[1]);
                y += cc[3] * v1 * (f ? 0.7f : 1.0f);
            }
            o[c] = y * 1.8f * TK_G + tk_lo[c];
        }
        l += (o[0] - l) * w;
        r += (o[1] - r) * w;
    }
    if (wh_m > 0.0f) {                          /* WAH: the cutoff rides the envelope (every 32 samples) */
        const float w = wh_m * 0.01f * (float)fm_clampf((float)fxs_v[EN_FXS_WH_MIX], 0.0f, 100.0f), x = 0.5f * (fm_fabsf(l) + fm_fabsf(r));
        float in[2] = {l, r}, o[2];
        int c;
        wh_env += (x - wh_env) * (x > wh_env ? 0.02f : 0.0015f);
        if (!(wh_cnt++ & 31u)) {
            const float e = fm_clampf(wh_env * (2.0f + 14.0f * (float)fxs_v[EN_FXS_WH_SENS] * 0.01f), 0.0f, 1.0f);
            svf_coefs(wh_c, 280.0f * fm_exp2f(4.0f * (float)fxs_v[EN_FXS_WH_RANGE] * 0.01f * e), 0.7f - 0.0058f * (float)fxs_v[EN_FXS_WH_RESO]);
        }
        for (c = 0; c < 2; c++) {
            float *s = wh_s[c], hi;
            wh_lo[c] = fm_flush(wh_lo[c] + XO_A * (in[c] - wh_lo[c]));   /* (0.17.19) the low end dry, the wah on the rest */
            hi = in[c] - wh_lo[c];
            {
                const float v3 = hi - s[1], v1 = wh_c[0] * s[0] + wh_c[1] * v3, v2 = s[1] + wh_c[1] * s[0] + wh_c[2] * v3;
                s[0] = fm_flush(2.0f * v1 - s[0]);
                s[1] = fm_flush(2.0f * v2 - s[1]);
                o[c] = wh_c[3] * v1 * 2.0f * WH_G + wh_lo[c];
            }
        }
        l += (o[0] - l) * w;
        r += (o[1] - r) * w;
    }
    if (tp_m > 0.0f || fl_m > 0.0f || ph_m > 0.0f) {
        const uint32_t w = mod_w & (MOD_N - 1u);
        float in[2] = {l, r}, out[2];
        int c;
        if (tp_m > 0.0f) {                      /* tape: wow 0.7 Hz (+-70 samples, about 12 cents), flutter 8 Hz */
            float d;
            if (!(mod_w & 15u)) {               /* the delay time is worked out every 16 samples (two sines) and slid between */
                float dn;
                tp_ph += 16.0f * 0.7f * (1.0f / EN_SR);
                tp_ph2 += 16.0f * 8.0f * (1.0f / EN_SR);
                tp_ph -= (float)(int)tp_ph;
                tp_ph2 -= (float)(int)tp_ph2;
                dn = 150.0f + tp_wd * fm_sin_turns(tp_ph) + tp_fd * fm_sin_turns(tp_ph2);
                tp_dd = (dn - tp_d) * (1.0f / 16.0f);
            }
            tp_d += tp_dd;
            d = tp_d;
            for (c = 0; c < 2; c++) {
                float rd = (float)w - d, y;
                uint32_t i0, i1;
                float fr;
                rd += rd < 0.0f ? (float)MOD_N : 0.0f;
                i0 = (uint32_t)rd & (MOD_N - 1u);
                i1 = (i0 + 1u) & (MOD_N - 1u);
                fr = rd - (float)(int)rd;
                tp_buf[c][w] = in[c];
                y = tp_buf[c][i0] + (tp_buf[c][i1] - tp_buf[c][i0]) * fr;
                tp_lp[c] += tp_k * (y - tp_lp[c]);      /* a duller top */
                y = soft_sat(tp_lp[c] * tp_drive) * tp_idrive;   /* and saturation (a rational tanh: one division) */
                in[c] += (y - in[c]) * tp_m;
            }
        }
        if (fl_m > 0.0f) {                      /* flanger: a short delay (0.2 ms and up), swept, with feedback */
            float d;
            if (!(mod_w & 15u)) {               /* the delay time: a sine every 16 samples, slid between */
                float dn;
                fl_ph += 16.0f * fl_inc;
                fl_ph -= (float)(int)fl_ph;
                dn = 10.0f + fl_rng * (0.5f + 0.5f * fm_sin_turns(fl_ph));
                fl_dd = (dn - fl_d) * (1.0f / 16.0f);
            }
            fl_d += fl_dd;
            d = fl_d;
            for (c = 0; c < 2; c++) {
                float rd = (float)w - d, y;
                uint32_t i0, i1;
                float fr;
                rd += rd < 0.0f ? (float)MOD_N : 0.0f;
                i0 = (uint32_t)rd & (MOD_N - 1u);
                i1 = (i0 + 1u) & (MOD_N - 1u);
                fr = rd - (float)(int)rd;
                y = fl_buf[c][i0] + (fl_buf[c][i1] - fl_buf[c][i0]) * fr;
                fl_buf[c][w] = fm_flush(in[c] + fl_fbk * (c ? -y : y));   /* the right side's feedback inverted: wider */
                in[c] = (in[c] + fl_wet * y * fl_m) * fl_inv;
            }
        }
        out[0] = in[0];
        out[1] = in[1];
        if (ph_m > 0.0f) {                      /* phaser: four all-passes, with feedback, mixed with the dry */
            for (c = 0; c < 2; c++) {
                float x = in[c] + ph_fbk * ph_fb[c], y = x;
                const float a = ph_a[c];
                float v;
                v = a * y + ph_z[c][0];  ph_z[c][0] = y - a * v;  y = v;
                v = a * y + ph_z[c][1];  ph_z[c][1] = y - a * v;  y = v;
                v = a * y + ph_z[c][2];  ph_z[c][2] = y - a * v;  y = v;
                v = a * y + ph_z[c][3];  ph_z[c][3] = y - a * v;  y = v;
                ph_fb[c] = y;
                out[c] = in[c] + (in[c] * ph_k0 + y * ph_k1 - in[c]) * ph_m;
            }
        }
        mod_w++;
        l = out[0];
        r = out[1];
    }
    if (rm_m > 0.0f) {                          /* the ring mod: the mix times a sine, 40 .. 800 Hz */
        float s;
        rm_ph += rm_inc;
        rm_ph -= (float)(int)rm_ph;
        s = fm_sin_turns(rm_ph);
        s *= 1.414f;                            /* (0.17.19) a sine's RMS is 0.71: the ring mod sat 3 dB under the music (the host, eight genres) */
        l += (l * s - l) * rm_m * rm_wet;
        r += (r * s - r) * rm_m * rm_wet;
    }
    if (cr_wet > 0.0f) {                        /* sample-rate and bit-depth, together, to 5.5 kHz and 5 bits */
        if (++crush_n >= cr_hold) {
            const float iv = 1.0f / cr_lv;
            crush_n = 0;
            crush_l = (float)(int32_t)(l * cr_lv + (l < 0.0f ? -0.5f : 0.5f)) * iv;
            crush_r = (float)(int32_t)(r * cr_lv + (r < 0.0f ? -0.5f : 0.5f)) * iv;
        }
        l += (crush_l - l) * cr_wet;
        r += (crush_r - r) * cr_wet;
    }
    *pl = l;
    *pr = r;
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing) {
                intro_t = 0;                     /* the intro from the start */
                play_start(1);
            }
            else if (!a && en_playing)
                play_stop();
            if (!a)
                en_frozen = 0;                   /* (0.17.36) stopped: the freeze thawed (its reverb, held for ever, rang on after STOP: the maker, 2026-11-14) */
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            if (a) {                             /* exact (a track code): every stream from the seed alone */
                const uint32_t s = (uint32_t)(b & 0xFFFF);
                rng = s * 2654435761u ^ 0x9E3779B9u;
                prng = 0x2545F491u ^ s * 40503u;
                lrng = 0x6C078965u ^ s * 69069u;
                if (!rng)
                    rng = 1u;
                break;
            }
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_MUTE: muted = (uint8_t)(a != 0); break;
        case C_HUSH: hush = (uint8_t)(a != 0); break;
        case C_NOTE: note_cmd(a, b & 0x7F, b >> 8); break;
        case C_REGROW: regrow(); break;
        case C_NEWPART:                          /* (0.17.23) one part's line new, the rest as they are: 0 the acid, 1 the pluck (a frozen one stays) */
            if (a == 0 && !par[P_AC_LOCK]) {
                ac_regrow();
            } else if (a == 1 && !par[P_CM_LOCK]) {
                int k;
                for (k = 0; k < 32; k++)
                    cm_f[k] = cm_g[k] = cm_ix[k] = cm_vl[k] = 0;
                cm_k = 0;
                cm_sync = 0;
                mo_valid = mo_rec = mo_on = 0;
            }
            break;
        case C_INTRO:                            /* set while playing: no intro now, the next Play's (unless play has only just begun: the boot's) */
            intro_len = a * (int32_t)EN_SR;
            intro_t = en_playing && en_bars >= 16u ? intro_len : 0;
            break;
        case C_SECTION:
            if (a == EN_CMD_BREAK && sec != SEC_GROOVE && sec != SEC_BUILD)
                break;                          /* already down */
            sec_blen = (uint8_t)((a == EN_CMD_BUILD || a == EN_CMD_DROP) && b > 0 && b < 64 ? b : 0);
            sec_req = a + 1;
            en_sec_req = (uint8_t)sec_req;
            if (!en_playing) {                  /* resting: now */
                section_cmd(a);
                sec_req = 0;
                drum_publish();
            }
            break;
        case C_FXK:
            if (a < EN_NFXK)
                fxk_v[a] = (int16_t)b;
            break;
        case C_PEDAL:                            /* follow or hold: at once (the pad's voices and the bass's note retargeted) */
            if (pedal != (uint8_t)(a & 3)) {
                pedal = (uint8_t)(a & 3);
                pad_retarget();
                bass_to(bass_pc());
            }
            break;
        case C_TRK:
            trk_mute = (uint8_t)(b & 0xFF);
            trk_solo = (uint8_t)(b >> 8 & 0xFF);
            break;
        case C_GATEPAT: gx_pat = (uint16_t)b; break;
        case C_FILLQ:
            fill_q = 1;
#ifdef EN_HOST
            en_host_fillq++;
#endif
            break;
        case C_FLXH: flx_phys = (uint8_t)a; break;
        case C_SHED: en_shed_level = (uint8_t)a; break;
        case C_USERVOICE: fm_user_take(a); break;
        case C_USERCLEAR: fm_user_wipe(); break;
        case C_ACSTEP: ac_step_set(a & 15, b & 15, b >> 4 & 3, b >> 6 & 1, b >> 7 & 1, b >> 8 & 1); break;
        case C_ACCAP: ac_capture(a & 15, b & 127, b & 0x800 ? b >> 7 & 15 : -1, a >> 4 & 1, a >> 5 & 1); break;
        case C_BSCAP: bs_cap(a & 15, b & 127, b >> 7 & 15, a >> 4 & 1, a >> 5 & 1); break;
        case C_ACKEEP: ac_keep_answer(); break;
        case C_PHRASE:                           /* (0.17.45) a phrase to play (a voice, its length in sixteenths), or none */
            ph_set(a, b);
            break;
        case C_LINK:                             /* (0.17.47) the link's state (EN_LK_*), the tracks left to the partner */
            lk_q = (uint8_t)a;
            lk_mute = (uint8_t)b;
            break;
        case C_KITS:                             /* (0.17.38) a loaded kit came or went: each drum's kit again */
            apply(P_KIT_KICK, par[P_KIT_KICK]);
            apply(P_KIT_SNARE, par[P_KIT_SNARE]);
            apply(P_KIT_HAT, par[P_KIT_HAT]);
            apply(P_KIT_PERC, par[P_KIT_PERC]);
            break;
        case C_EXTCH:                            /* (0.17.31) the chord from outside (a 12-bit mask; 0: off, back to the walk from this chord) */
            if (!(b & 0xFFF)) {
                if (ext_on) {
                    ext_on = en_extch = 0;
                    alt_clear();
                    chord_to(deg);
                }
                break;
            }
            {
                const int root = a & 15, m = b & 0xFFF;
                int iv[4], n = 0, k2;
                static const uint8_t ORD[11] = {4, 3, 10, 11, 7, 2, 5, 9, 6, 8, 1};   /* after the root: the third, the seventh, the fifth, then the colours */
                ext_pc[0] = (int8_t)root;
                n = 1;
                for (k2 = 0; k2 < 11 && n < 4; k2++)
                    if (m >> ((root + ORD[k2]) % 12) & 1)
                        iv[n] = ORD[k2], ext_pc[n++] = (int8_t)((root + ORD[k2]) % 12);
                (void)iv;
                ext_n = (uint8_t)n;
                ext_bass = (uint8_t)(a >> 4 & 15);
                ext_on = en_extch = 1;
                ext_apply();
            }
            break;
        case C_CMCAP: cm_capture(a & 15, b & 128 ? -1 : b & 127); cm_publish(); break;
        case C_CMSTEP: cm_step_set(a & 15, b & 15, b >> 4 & 3); break;
        case C_BSSTEP:                          /* a bass step (the page), or a piece's (bit 4 of a: a rest of a piece's that set none); 0x20: capture */
            if (a & 0x20) {
                if (!bs_set)
                    bs_capture();
                break;
            }
            bs[a & 15] = (uint8_t)(b & 0xFF);
            if (!(a & 0x10))
                bs_set = 1;
            else if ((b & 0x100) && !(bs[0] | bs[1] | bs[2] | bs[3] | bs[4] | bs[5] | bs[6] | bs[7] | bs[8] | bs[9] | bs[10] | bs[11] | bs[12] | bs[13] | bs[14] | bs[15]))
                bs_set = 0;
            bs_publish();
            break;
        case C_PROG: pg_lo = (uint32_t)a | ((uint32_t)b & 0xFFFFu) << 8; break;
        case C_DTAP:
            if ((unsigned)a < (unsigned)EN_NDRUM)
                drum_hit(a, (float)b * (1.0f / 127.0f), 0);
            break;
        case C_BEND:                             /* a = the voice (bit 7: the mod wheel), b = the bend or the depth */
            if ((a & 0x7F) != hb_voice) {
                hb_bend = hb_mod = 0.0f;         /* (another voice: the old one straightens) */
                hb_voice = (uint8_t)(a & 0x7F);
            }
            if (a & 0x80)
                hb_mod = (float)b * (1.0f / 127.0f);
            else
                hb_bend = (float)b * (2.0f / 8192.0f);
            break;
        case C_DEG:
            if (a < nsteps && a != deg) {
                alt_clear();
                chord_to(a);
                walk_next = en_walk_next = -1;   /* (a chord from the pads: the walk's next is chosen at its change) */
            }
            break;
        case C_MAP:                              /* another map: the old one's notes let go first */
            if (a != midi_map) {
                notes_off();
                midi_map = (uint8_t)a;
            }
            break;
        case C_PROG2: prog_take(pg_lo, (uint32_t)a | ((uint32_t)b & 0xFFFFu) << 8); break;
        case C_PROGK: {                          /* the slots' types: the chord playing takes its slot's at once */
            const uint32_t k = (uint32_t)a | ((uint32_t)b & 0xFFFFu) << 8;
            int s, was = pg_on ? pg_kind[pg_i] : -1;
            for (s = 0; s < EN_PRG_N; s++)
                pg_kind[s] = (uint8_t)(k >> 3 * s & 7u);
            if (pg_on && pg_kind[pg_i] != was)
                pad_retarget();
            break;
        }
        case C_DJSYNC: dj_sync = (uint8_t)a; break;
        case C_FXS:
            if (a < EN_NFXS)
                fxs_v[a] = (int16_t)b;
            break;
        case C_FX:
            fx_req = (fx_req & ~(0xFFFFull << (16 * (a & 3)))) | (uint64_t)(b & 0xFFFF) << (16 * (a & 3));
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
#define RZ_MAKE 13.0f                          /* (0.17.51) the resonator's level at Amount 100, Q 8 (made up by sqrt Q above it): measured, THE RESONATOR */
#define RZ_PLK 0.06f                             /* the pluck's level, 24 dB under a hit's: its notes are the chord's, on the bands' own pitches, where a hit's
                                                 * make-up rang them 20 dB louder than the drums (HOUSE 3: the mix +4.5 dB) */
#ifndef EN_MIN_CHUNK
#define EN_MIN_CHUNK 128                        /* frames: the shortest chunk a swung or humanized hit may cut (en_render; the clock pulse cuts anywhere) */
#endif
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];
static float busl[BLK], busr[BLK];              /* the punch-ins' bus (0.15.19) */
static float dsn_l[BLK], dsn_r[BLK];            /* the mix before the drums (their crush, and the drums into the bus) */

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static uint32_t pad_ctl[EN_NPAD], dr_ctl, ac_ctl;      /* the control steps' counts (every 32 samples), kept across chunks */
static float padl[BLK] EN_POOL, padr[BLK] EN_POOL;
static float plp[4];
static float pad_ol[BLK] EN_POOL, pad_or[BLK] EN_POOL;   /* the job's working buffers: the pad's contribution to the mix, through the chorus, then into pad_ring (its dry sum goes straight there from pad_bus) */
/* the pad's ensemble: three delays a side, each swept by a slow triangle (the three a third of a turn apart, the right side's the other way), around
 * 7 ms, 3 ms deep; its sum is added to the pad (Chorus is how much). Widens every pad sound without the pulsing a single swept delay makes against a
 * pure tone: the beats are never all in step. On the pad's stage, so on the other core with it. */
#define CH_N 512
static float ch_buf[2][CH_N] EN_POOL;
static float ch_ph[3] = {0.0f, 0.3333f, 0.6667f};
static uint32_t ch_i;
static float pad_lk;                             /* the snare link, as it was when the chunk began (the pad may be rendered on the other core) */

static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(700.0f * PAD_SOUND[pad_snd()].fc * fm_exp2f(bright * 0.03f + 0.6f * arcm + 1.5f * pad_filt +
                                                         3.0f * (float)par[P_LK_SNARE] * 0.01f * pad_lk), 200.0f, 12000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1] * TG(TR_PAD, j);
        r = plp[3] * TG(TR_PAD, j);
        pad_ol[j] = l * 0.8f;
        pad_or[j] = r * 0.8f;
        pad_ring[2][(rv_job.pad_w + j) & (PAD_R - 1)] = l + r;   /* the dry sum, the sends' source: 0.6 of it to the reverb, 0.2 to the echo (render_chunk, a chunk on) */
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

static void pad_publish(uint32_t w, uint32_t n)   /* the job's pad (after the chorus) into the ring, for the first core a chunk on (the dry sum went in from pad_bus) */
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        const uint32_t q = (w + j) & (PAD_R - 1);
        pad_ring[0][q] = pad_ol[j];
        pad_ring[1][q] = pad_or[j];
    }
}

static void pad_chorus(uint32_t n)
{
    const float a = en_shed_level >= 1 ? 0.0f : (float)par[P_PL_CHO] * 0.01f * 0.55f;   /* (0.17.61: the first to go, before the DX voices) */   /* (no chorus when the load is far too high: it runs on the pad's core, and the first level answers the first core's time) */
    const float rate[3] = {0.47f / EN_SR, 0.61f / EN_SR, 0.79f / EN_SR};   /* Hz, the sweeps are slow and unequal */
    uint32_t j;
    int k;
    for (j = 0; j < n; j++) {
        const float dl = pad_ol[j], dr = pad_or[j];
        float wl = 0.0f, wr = 0.0f;
        ch_buf[0][ch_i] = dl;
        ch_buf[1][ch_i] = dr;
        if (a > 0.0f)
            for (k = 0; k < 3; k++) {
                const float tri = ch_ph[k] < 0.5f ? 4.0f * ch_ph[k] - 1.0f : 3.0f - 4.0f * ch_ph[k];
                const float d1 = (0.007f + 0.003f * tri) * EN_SR, d2 = (0.007f - 0.003f * tri) * EN_SR;
                float rp = (float)ch_i - d1, f;
                int i0;
                if (rp < 0.0f)
                    rp += (float)CH_N;
                i0 = (int)rp;
                if (i0 >= (int)CH_N)            /* (a position just under 0 rounds up to the end of the ring) */
                    i0 -= (int)CH_N, rp -= (float)CH_N;
                f = rp - (float)i0;
                wl +=ch_buf[0][i0] + (ch_buf[0][i0 + 1 >= CH_N ? 0 : i0 + 1] - ch_buf[0][i0]) * f;
                rp = (float)ch_i - d2;
                if (rp < 0.0f)
                    rp += (float)CH_N;
                i0 = (int)rp;
                if (i0 >= (int)CH_N)
                    i0 -= (int)CH_N, rp -= (float)CH_N;
                f = rp - (float)i0;
                wr +=ch_buf[1][i0] + (ch_buf[1][i0 + 1 >= CH_N ? 0 : i0 + 1] - ch_buf[1][i0]) * f;
            }
        for (k = 0; k < 3; k++) {
            ch_ph[k] += rate[k];
            if (ch_ph[k] >= 1.0f)
                ch_ph[k] -= 1.0f;
        }
        ch_i = ch_i + 1u >= CH_N ? 0u : ch_i + 1u;
        pad_ol[j] = dl + wl * a * 0.45f;
        pad_or[j] = dr + wr * a * 0.45f;
    }
}

static void render_pad(uint32_t n)
{
    const int ft_pad = fxs_v[EN_FXS_FT_PAD];   /* the FILTERS page's type for the saw sounds */
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    const float g = pad_vol * 0.5f * pad_lvl * (1.0f + 0.01f * (float)SECT[chord_genre].pad * fm_maxf(brk, 0.0f)) * PAD_SOUND[pad_snd()].g *   /* the pad rises in a breakdown */
                    (1.0f + 0.35f * (float)par[P_LK_SNARE] * 0.01f * pad_lk);                    /* and swells with the links */
    uint32_t nb = (n + 31u) >> 5;
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 5.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], p3 = v->ph[3], ps = v->sub, amp = v->amp,
              s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2], s3 = v->ic[3], gl, gr, pn, tgt = 0.0f, idx = 0.0f, se = stab_env, ar = 0.01f;
        const int saw = v->voices == PO_STRINGS || v->voices == PO_SUPER || v->voices == PO_PWM;   /* the saw sounds: a sub and the sweep filter */
        uint32_t j;
        if (amp < 1e-5f && pad_gain < 1e-4f && !v->play) {
            v->amp = 0.0f;
            continue;
        }
        pn = fm_clampf(v->pan + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v + pad_pan * (i & 1 ? 1.0f : -1.0f), -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x, v1, v2, v3, b1, l1, b2;
            if (!(pad_ctl[i]++ & 31u)) {         /* (counted across chunks: a chunk is cut at every swung hit and clock pulse, and a count from each chunk's start ran the envelopes fast) */
                float br;
                pad_coefs(v, i);
                ar = 0.01f;
                if (v->play) {                   /* played from the keys */
                    br = v->pamp;
                    tgt = br * 0.8f;
                } else if (par[P_PAD_MAN]) {     /* off the algorithm: quiet (fading, not cut) */
                    br = 0.0f;
                    tgt = 0.0f;
                    ar = 0.0004f;
                } else if (pad_stabs()) {        /* a stab: the shared envelope */
                    br = se;
                    se *= stab_k;
                    tgt = br * 0.85f * pad_gain;
                } else {
                    br = breath(v, v->t);
                    tgt = br * v->peak * pad_gain * v->hf;
                }
                /* the Rhodes blooms: more overtones as the breath swells (index in turns) */
                idx = (0.04f + 0.1f * bright * 0.01f + 0.1f * br) * (1.0f + 0.4f * arcm);
                if (saw) {                       /* the saw sounds' filter: a resonant low-pass that opens with the breath or the stab and closes after it, a Prophet's
                                                  * sweep (two poles resonant, two flat: 24 dB); its states are the formants' (a saw sound has none) */
                    const float fcv = 300.0f * PAD_SOUND[pad_snd()].fc * fm_exp2f(3.3f * br + 0.02f * bright + 1.5f * pad_filt + 0.5f * arcm) * FT_FC[0][FT_IX(ft_pad)];
                    svf_coefs(v->c1, fcv, 0.5f);
                    svf_coefs(v->c2, ft_pad == 1 ? fcv * (1.0f / 1.3f) : fcv * 1.3f, 1.0f);
                }
            }
            amp += (tgt - amp) * ar;
            switch (v->voices) {
            case PO_RHODES: {                    /* FM, the modulator at the carrier's pitch: a Rhodes */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f;   /* one carrier: no beating */
                break;
            }
            case PO_STRINGS:                     /* four saws, detuned (Detune, and a pair inside it) */
                x = 0.36f * (blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[1]) + blep_saw(p2, v->inc[2]) + blep_saw(p3, v->inc[3]));
                break;
            case PO_GLASS: {                     /* FM at 3.5: a bell, glassy, brighter as it swells */
                float m = sine(p1) * (0.25f + 4.0f * idx);
                x = (sine_any(p0 + m) + 0.25f * sine(2.0f * p0)) * 1.1f;
                break;
            }
            case PO_ORGAN:                       /* drawbars: 8', 4', 2 2/3', 2' */
                x = (0.62f * sine(p0) + 0.38f * sine(2.0f * p0) + 0.26f * sine(3.0f * p0) + 0.16f * sine(4.0f * p0) +
                     0.2f * sine(p1)) * 1.05f;
                break;
            case PO_WARM:                        /* two triangles, detuned, and a sine: soft and round */
                x = ((p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) + (p1 < 0.5f ? 4.0f * p1 - 1.0f : 3.0f - 4.0f * p1)) *
                    0.7f + 0.5f * sine(p0);
                break;
            case PO_SUPER: {                     /* four saws detuned and one an octave up: a supersaw */
                float po = 2.0f * p0;
                po -= (float)(int)po;
                x = 0.3f * (blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[1]) + blep_saw(p2, v->inc[2]) + blep_saw(p3, v->inc[3])) +
                    0.3f * blep_saw(po, 2.0f * v->inc[0]);
                break;
            }
            case PO_PWM: {                       /* a saw less another, detuned: a pulse whose width drifts */
                float q = p1 + 0.5f;
                if (q >= 1.0f)
                    q -= 1.0f;
                x = 0.55f * (blep_saw(p0, v->inc[0]) - blep_saw(q, v->inc[1])) + 0.3f * sine(p0);
                break;
            }
            case PO_HOLLOW:                      /* odd harmonics: hollow, as a clarinet */
                x = (sine(p0) + 0.33f * sine(3.0f * p0) + 0.2f * sine(5.0f * p0) + 0.14f * sine(7.0f * p0)) * 1.15f;
                break;
            case PO_SINE:                        /* a sine and a little of its octave */
                x = (sine(p0) + 0.18f * sine(2.0f * p0)) * 1.3f;
                break;
            default:                             /* Voices: as Discreet Music's Synthi, through two formants */
                x = (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) * 1.4f + 0.45f * blep_saw(p0, v->inc[0]) + 0.6f * sine(p0);
                v3 = x - s1;                     /* the first formant: TPT SVF (Zavalishin), bp and lp */
                v1 = v->c1[0] * s0 + v->c1[1] * v3;
                v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
                s0 = 2.0f * v1 - s0;
                s1 = 2.0f * v2 - s1;
                b1 = v1 * v->c1[3];
                l1 = v2;
                v3 = x - s3;                     /* the second */
                v1 = v->c2[0] * s2 + v->c2[1] * v3;
                v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
                s2 = 2.0f * v1 - s2;
                s3 = 2.0f * v2 - s3;
                b2 = v1 * v->c2[3];
                x = b1 + 0.5f * b2 + 0.45f * l1;
                break;
            }
            if (saw) {                           /* the sub, an octave down; then the sweep: resonant pair, flat pair (the FILTERS page's type) */
                float y1;
                x += 0.15f * sine(ps);
                v3 = x - s1;
                v1 = v->c1[0] * s0 + v->c1[1] * v3;
                v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
                s0 = 2.0f * v1 - s0;
                s1 = 2.0f * v2 - s1;
                y1 = svf_pick(x, v1, v2, v->c1[3], ft_pad);
                v3 = y1 - s3;
                v1 = v->c2[0] * s2 + v->c2[1] * v3;
                v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
                s2 = 2.0f * v1 - s2;
                s3 = 2.0f * v2 - s3;
                x = (FT_TWO(ft_pad) ? svf_pick(y1, v1, v2, v->c2[3], ft_pad) : y1) * 1.1f * FT_G[0][FT_IX(ft_pad)];
                p2 += v->inc[2];
                if (p2 >= 1.0f)
                    p2 -= 1.0f;
                p3 += v->inc[3];
                if (p3 >= 1.0f)
                    p3 -= 1.0f;
                ps += 0.5f * v->inc[0];
                if (ps >= 1.0f)
                    ps -= 1.0f;
            }
            p0 += v->inc[0];
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            p1 += v->voices == PO_GLASS ? v->inc[0] * 3.5f : v->inc[1];
            if (p1 >= 1.0f)
                p1 -= (float)(int)p1;
            x *= amp;
            padl[j] += x * gl;                   /* to the pad's bus: smoothed below */
            padr[j] += x * gr;
        }
        v->ic[0] = fm_flush(s0);
        v->ic[1] = fm_flush(s1);
        v->ic[2] = fm_flush(s2);
        v->ic[3] = fm_flush(s3);
        v->amp = amp;
        v->ph[0] = p0;
        v->ph[1] = p1;
        v->ph[2] = p2;
        v->ph[3] = p3;
        v->sub = ps;
        en_pad_note[i] = (uint8_t)v->note;
        en_pad_amp[i] = amp;
        lvl = fm_maxf(lvl, amp);
    }
    en_pad_level = lvl;
    while (nb--)
        stab_env *= stab_k;
    stab_env = fm_flush(stab_env);
    pad_bus(n);
    pad_chorus(n);
}

/* (0.14.4 tried this in RAM, .ram_hot: on the device the bass costs about 200 us a chunk whatever the chunk's length, the most of any stage, which
 * looks like its code fetched cold from the flash each chunk. It makes 43 calls out to helpers the compiler keeps in the flash, and a pi32v2 call
 * cannot reach the flash from RAM (R_PI32V2_LONG_JUMP_23M2 truncated), so it needs those helpers inlined or in RAM with it first.) */
#if EN_BASS_RAM && !defined(EN_HOST) && !defined(EN_WEB)
__attribute__((section(".ram_hot.bass"), noinline, flatten))   /* (0.17.68: in RAM, its helpers inlined) */
#endif
static void render_bass(uint32_t n)
{
    const int ft_bass = fxs_v[EN_FXS_FT_BASS];
    uint32_t j;
    const drone_snd_t *DS = &DRONE_SOUND[(unsigned)par[P_DR_INST] < EN_NSOUND ? par[P_DR_INST] : 0];
    const int dest = par[P_DL_DEST], osc = DS->osc, sq = fxs_v[EN_FXS_SQ_TGT] >= 1;   /* SCREAM on the bass (its Target: 1 the bass, 2 both) */
    const float sqd = sq ? (float)fxs_v[EN_FXS_SQ_DRIVE] * 0.01f : 0.0f, sqa = sq ? (float)fxs_v[EN_FXS_SQ_AMT] * 0.01f : 0.0f;
    const float depth = (float)par[P_DL_DEPTH] * 0.01f, lv = dest == 2 ? 1.0f - 0.8f * depth * (0.5f - 0.5f * drone_lfo.v) : 1.0f,
                drv = 1.4f * DS->drive * (dest == 3 ? 1.0f + 2.5f * depth * (0.5f + 0.5f * drone_lfo.v) : 1.0f) * (1.0f + 3.0f * sqd),
                k1 = reso_k * (1.0f - 0.93f * sqa) * (dest == 4 ? 1.0f - 0.85f * depth * (0.5f + 0.5f * drone_lfo.v) : 1.0f),   /* Scream; the LFO on Reso */
                k2 = 1.4f * (1.0f - 0.75f * sqa), sqin = 1.0f + 2.0f * sqd,
                g = bass_vol * 0.36f * lv * DS->g / fm_minf(1.0f + 0.15f * (DS->drive - 1.0f), 1.3f) * (1.0f + bass.vd) / (1.0f + 0.5f * sqd + 0.4f * sqa),
                dsaw = DS->saw,
                tgt = (en_playing && !par[P_DR_MAN]) || bass.play ? 1.0f : 0.0f,
                rate = tgt > 0.0f ? 1.0f / ((bass.play ? 0.03f : 2.0f) * EN_SR) : 1.0f / ((par[P_DR_MAN] ? 0.3f : 4.0f) * EN_SR),
                fmi = 0.5f + 2.2f * bass.env + 0.3f * arcm, pw = 0.5f + 0.3f * mod[9].v;
    const int bl_on = par[P_BL_MODE] && !bass.play;    /* the drone as a bass line: its notes' envelope in the loop */
    /* (0.17.27) THE WUB (wub_on): a clean sine sub under a mid layer (the sound's oscillator), the mid through a resonant low-pass the LFO sweeps over five
     * octaves (about 70 Hz to 3-4 kHz) and a vowel band above it (the "yoi"), then driven after the filter, harder as it opens (the growl), then a
     * high-pass at 110 Hz so the sub stays clean. The sweep: the LFO's place as a level (sine, triangle, drift: a raised cosine, closed at the beat;
     * saw: open then closing; square: open/shut; S&H: a level a cycle), drawn across the chunk from the last one's (no steps on the cutoff). */
    const int wub = wub_on();
    const float wl0 = wub_lv, wdrv = (3.0f + 1.0f * DS->drive) * (osc == DO_METAL ? 0.65f : 1.0f) * (1.0f + 2.0f * sqd);   /* (0.17.28: Metal's tamed, "a bit too much") */
    float wl1 = 0.0f, wlv = wl0, wsa = 0.0f;
    if (wub) {
        const float lp = drone_lfo.ph;
        const int sh = par[P_DL_SHAPE];
        wl1 = sh == 2 ? 1.0f - lp : sh == 3 ? (lp < 0.5f ? 1.0f : 0.0f) : sh == 4 ? 0.5f + 0.5f * drone_lfo.b : 0.5f - 0.5f * fm_cosf(FM_TWO_PI * lp);
    }
    wub_lv = wl1;
    float p0 = bass.ph[0], p1 = bass.ph[1], p2 = bass.ph[2], sp = bass.sph, mp = bass.mph, amp = bass.amp, f, i0, i1, i2, mi, dsub = DS->sub, dsubu;
    if (amp < 1e-5f && !en_playing && !bass.play) {
        bass.amp = 0.0f;
        en_bass_level = 0.0f;
        for (j = 0; j < n; j++)
            bass_o[j] = 0.0f;
        return;
    }
    {
        float gk = bass.play ? GLIDE_K * 12.0f : GLIDE_K;
        if (bl_on)                              /* a bass line: no glide is the note itself; else its time, 10 .. 410 ms */
            gk = par[P_BL_GLIDE] ? 1.0f / ((0.01f + 0.004f * (float)par[P_BL_GLIDE]) * EN_SR) : bl_slide ? 1.0f / (0.06f * EN_SR) : 1.0f;   /* (a step's glide: 60 ms) */
        bass.note += (bass.target - bass.note) * fm_minf((float)n * gk, 1.0f);
    }
    f = mtof(bass.note + (dest == 1 ? depth * drone_lfo.v : 0.0f) + (HB(EN_V_DRONE) ? hb_semi : 0.0f) + vt_d[4]);   /* the LFO on the pitch: a semitone at most; a keyboard's bend */
    i0 = f * (osc == DO_FIFTHS ? 1.49831f : osc == DO_REESE || osc == DO_HOOVER ? 0.9862f : 0.99481f) *
         (1.0f / EN_SR);                        /* -9, 0, +9 cents (Reese and Hoover: 24; Fifths: a fifth) */
    i1 = f * (1.0f / EN_SR);
    {   /* the sub an octave down only where it can be heard: from 30 Hz, fading to nothing at 18 (D1's sub is 18 Hz: the sound check
         * found a fifth of HYPNO's and DUB's energy under 20 Hz); what it leaves goes to the note itself, as a sine, at 0.7 */
        const float sg = fm_clampf((0.5f * f - 18.0f) * (1.0f / 12.0f), 0.0f, 1.0f);
        dsubu = dsub * (1.0f - sg) * 0.7f;
        dsub *= sg;
    }
    wsa = fm_clampf((f - 60.0f) * 0.05f, 0.0f, 1.0f);   /* (the wub's sub: on the note below 60 Hz, an octave down above 80, crossfaded between: no jump) */
    i2 = f * (osc == DO_REESE || osc == DO_HOOVER ? 1.0140f : 1.00521f) * (1.0f / EN_SR);
    mi = osc == DO_METAL ? i1 * 1.4142f : osc == DO_FMR ? i1 * DRONE_FR[(unsigned)par[P_DR_INST] < EN_NSOUND ? par[P_DR_INST] : 0] : i1 * (1.5f + 2.5f * bass.env);   /* Metal's and FMR's modulator; Sync's slave */
    for (j = 0; j < n; j++) {
        float x;
        if (wub && !(dr_ctl & 31u)) {           /* the wub's: the sweep, its vowel band (Q about 3), the cutoff's base the knob's (40: none), the breakdown closing it */
            float fc;
            wlv = wl0 + (wl1 - wl0) * ((float)j / (float)n);
            fc = fm_clampf(fm_maxf(f * 1.5f, 70.0f) * fm_exp2f((1.5f + 4.5f * depth) * wlv + (float)(par[P_DR_CUT] - 40) * 0.04f + 0.3f * motion * mod[9].v -
                                                                2.0f * fm_maxf(brk, 0.0f) - 1.6f * fm_minf(brk, 0.0f) + 2.0f * (float)par[P_LK_KICK] * 0.01f * lk_kick), 50.0f, 7000.0f);
            svf_coefs(bass.c1, fc * FT_FC[1][FT_IX(ft_bass)], k1 * 0.6f);
            svf_coefs(bass.c2, fm_minf(fc * 1.8f, 9000.0f), 0.3f);
        }
        if (!(dr_ctl++ & 31u) && !wub) {        /* the filter, every 32 samples (counted across chunks) */
            float oct, fc;
            oct = (float)par[P_DR_CUT] * 0.075f - 1.5f + DS->cut + (dest == 0 ? 2.5f * depth * (dlv_was + (drone_lfo.v - dlv_was) * (float)j / (float)n) : 0.0f) +
                  4.0f * (float)par[P_DE_AMT] * 0.01f * bass.env + 0.8f * motion * mod[9].v + 0.8f * arcm -
                  2.0f * fm_maxf(brk, 0.0f) - 1.6f * fm_minf(brk, 0.0f) - 0.35f * bass_duck * pulse_vol +
                  4.0f * (float)par[P_LK_KICK] * 0.01f * lk_kick;   /* the links: the kick opens it */
            fc = fm_clampf(f * 2.0f * fm_exp2f(oct), 45.0f, 9000.0f);
            fc *= FT_FC[1][FT_IX(ft_bass)];
            svf_coefs(bass.c1, fc, k1);
            svf_coefs(bass.c2, ft_bass == 1 ? fc * (1.0f / 1.3f) : fc * 1.3f, k2);
        }
        switch (osc) {
        case DO_REESE:                          /* two saws far apart: a slow phasing growl */
            x = (blep_saw(p0, i0) + blep_saw(p2, i2)) * dsaw * 1.35f;
            break;
        case DO_PULSE: {                        /* a pulse, its width wandering */
            float q = p1 + pw;
            if (q >= 1.0f)
                q -= 1.0f;
            x = (blep_saw(p1, i1) - blep_saw(q, i1)) * dsaw * 1.4f;
            break;
        }
        case DO_FM:                             /* two-operator FM, the envelope opening the index */
            x = sine_any(p1 + fmi * sine(p0)) * dsaw * 2.2f;
            break;
        case DO_ORGAN:
            x = (sine(p1) + 0.5f * sine(2.0f * p1) + 0.33f * sine(3.0f * p1) + 0.2f * sine(4.0f * p1)) * dsaw * 1.5f;
            break;
        case DO_SINE:
            x = (sine(p1) + 0.15f * sine(2.0f * p1)) * dsaw * 1.8f;
            break;
        case DO_SQUARE: {                       /* a square: the saw less itself half a turn on */
            float q = p1 + 0.5f;
            if (q >= 1.0f)
                q -= 1.0f;
            x = (blep_saw(p1, i1) - blep_saw(q, i1)) * dsaw * 1.2f;
            break;
        }
        case DO_HOOVER: {                       /* two pulses (a third wide), far apart, and the saw's octave down */
            float q0 = p0 + 0.3f, q2 = p2 + 0.3f;
            if (q0 >= 1.0f)
                q0 -= 1.0f;
            if (q2 >= 1.0f)
                q2 -= 1.0f;
            x = (blep_saw(p0, i0) - blep_saw(q0, i0) + blep_saw(p2, i2) - blep_saw(q2, i2)) * dsaw * 0.9f +
                0.5f * dsaw * (2.0f * sp - 1.0f);
            break;
        }
        case DO_FIFTHS:                         /* the root and the fifth (p0 runs at 3/2) */
            x = (blep_saw(p1, i1) + 0.75f * blep_saw(p0, i0)) * dsaw * 1.1f;
            break;
        case DO_FMR:                            /* FM at the sound's own ratio, the envelope opening the index */
            x = sine_any(p1 + (0.25f + 1.5f * bass.env) * sine(mp)) * dsaw * 2.0f;
            break;
        case DO_METAL:                          /* FM, the modulator at 1.41: off the harmonics */
            x = sine_any(p1 + (0.3f + 1.6f * bass.env) * (wub ? 0.55f : 1.0f) * sine(mp)) * dsaw * 2.0f;   /* (the wub: a lower index, less clang) */
            break;
        case DO_SYNC:                           /* a saw synced to the note, the envelope sweeping it */
            x = blep_saw(mp, mi) * dsaw * 1.3f;
            break;
        default:                                /* three saws a few cents apart */
            x = (blep_saw(p0, i0) + blep_saw(p1, i1) + blep_saw(p2, i2)) * dsaw;
            break;
        }
        if (wub) {                              /* the mid: filtered, its vowel, driven, high-passed; the sub (on the note, an octave down from 80 Hz) left clean */
            const float m = x * sqin;
            float y = svf_mm(m, bass.c1, &bass.s[0], ft_bass) * FT_G[1][FT_IX(ft_bass)] + (0.4f + 0.8f * wlv) * svf_mm(m, bass.c2, &bass.s[2], 2);
            y = fm_tanhf(y * wdrv * (1.0f + 1.5f * wlv) + 0.3f) - 0.2913f;   /* (a bias: the even harmonics too; tanh 0.3 off again) */
            wub_hp += 0.0157f * (y - wub_hp);
            x = 1.3f * (y - wub_hp) + 0.65f * (wsa <= 0.0f ? sine(p1) : wsa >= 1.0f ? sine(sp) : sine(p1) + wsa * (sine(sp) - sine(p1)));
        } else {
            x += dsub * sine(sp);
            if (dsubu > 1e-4f)                   /* (the sub's share below 30 Hz: on the note) */
                x += dsubu * sine(p1);
        }
        p0 += i0;
        p1 += i1;
        p2 += i2;
        sp += i1 * 0.5f;                        /* the sub: an octave down */
        mp += mi;
        if (mp >= 1.0f)
            mp -= (float)(int)mp;
        if (osc == DO_SYNC && p1 >= 1.0f)
            mp = (p1 - 1.0f) * (mi / i1);       /* the sync: the slave starts again with the note */
        if (p0 >= 1.0f)
            p0 -= 1.0f;
        if (p1 >= 1.0f)
            p1 -= 1.0f;
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        if (sp >= 1.0f)
            sp -= 1.0f;
        if (!wub) {
            x = svf_mm(x * sqin, bass.c1, &bass.s[0], ft_bass);
            if (FT_TWO(ft_bass))
                x = svf_mm(x, bass.c2, &bass.s[2], ft_bass);
            x *= FT_G[1][FT_IX(ft_bass)];
        }
        amp += (tgt - amp) * rate;
        if (bl_on) {                            /* the line's note: 4 ms attack, a slow fall while it holds, 30 ms release after */
            if (bl_up) {
                bl_env += 1.0f / (0.004f * EN_SR);
                if (bl_env >= 1.0f) {
                    bl_env = 1.0f;
                    bl_up = 0;
                }
            } else if (bl_gate > 0.0f) {
                bl_gate -= 1.0f;
                bl_env *= 0.99994785f;
            } else {
                bl_env *= 0.9948f;
            }
            x *= bl_env;
        }
        bass_o[j] = fm_tanhf(wub ? x * WUB_OUT : x * drv) * amp * g * TG(TR_DRONE, j);   /* (into the mix by render_chunk, with its sends: 0.06 and 0.04 of it) */
    }
    dlv_was = drone_lfo.v;                      /* (0.17.44: where the cutoff LFO glides from next chunk) */
    for (j = 0; j < 4; j++)
        bass.s[j] = fm_flush(bass.s[j]);
    wub_hp = fm_flush(wub_hp);
    bass.ph[0] = p0;
    bass.ph[1] = p1;
    bass.ph[2] = p2;
    bass.sph = sp;
    bass.mph = mp;
    bass.amp = amp;
    en_bass_level = bl_on ? amp * bl_env : amp;
}

static void render_dj(uint32_t n)               /* the snips, into the mix and its sends, before the sidechain */
{
    static const float ES[3][2] = {{0.0f, 0.0f}, {0.25f, 0.35f}, {0.6f, 0.9f}};          /* the reverb's and the echo's share */
    const int ec = fxs_v[EN_FXS_SM_LEN] < 0 ? 0 : fxs_v[EN_FXS_SM_LEN] > 2 ? 2 : fxs_v[EN_FXS_SM_LEN];
    const float gain = 0.01f * (float)fm_clampf((float)fxs_v[EN_FXS_SM_MIX], 0.0f, 100.0f) * 0.5f;
    int v, live = 0;
    uint32_t j;
    for (v = 0; v < 4; v++) {
        dj_t *d = &djv[v];
        if (!d->on)
            continue;
        for (j = 0; j < n && d->on; j++) {
            float a, sd, y = dj_voice(d, &a, &sd);
            const int sustained = d->type == DJ_HORN || d->type == DJ_RAVE || d->type == DJ_SIREN || d->type == DJ_DUB || d->type == DJ_RISER || d->type == DJ_ROLL;
            /* a sustained sound: while its key is down (held mode), else for a second or so; the others play out */
            const int gate = d->land ? d->t < d->aux * EN_SR : !sustained || (d->held && d->hold_mode) || (!d->hold_mode && d->t < 1.2f * EN_SR);
            d->env += ((gate ? 1.0f : 0.0f) - d->env) * (gate ? 0.01f : d->land ? 0.004f : 0.00025f);   /* (landed: out in about 6 ms, on the line) */
            const float k = a * d->env * gain;   /* (0.17.60: not the acid's track gain: in song mode the arrangement's rests of the acid silenced them) */
            y *= k;
            sd *= k;
            mixl[j] += y + sd;                  /* (the side: left and right apart, nothing of it in mono) */
            mixr[j] += y - sd;
            rsend[j] += y * ES[ec][0];
            esend[j] += y * ES[ec][1];
            if (sustained && !gate && d->env < 0.003f)
                d->on = 0;
            d->z = fm_flush(d->z);
            d->z2 = fm_flush(d->z2);
            d->zs = fm_flush(d->zs);
            d->zs2 = fm_flush(d->zs2);
        }
        live += d->on;
    }
    en_smp_active = (uint8_t)live;
}

/* (0.17.25) the DX pluck's ensemble, after SAMPERF's (0.29.0): a DX7 voice is one voice in the middle, which reads as thin beside the pad's chorus
 * and the plain pluck's two pans. What is above 200 Hz goes through a delay with a tap a side, around 8 ms, swept 2.5 ms by a slow sine (0.37 Hz)
 * the sides a quarter turn apart, with a small faster shimmer (5.3 Hz); the lows stay in the middle, and so does a bass voice (and the sends).
 * On the DX voices' stage, so on the other core with them; off when the load is far too high (shed level 2). */
#define FE_N 512                                /* (10.7 ms at the most: 471 samples) */
#define FE_AMT 0.4f                             /* the taps' share (SAMPERF's pluck and keys: 0.4 .. 0.6 of its 0.8) */
#define FE_Q 8192.0f                            /* the line holds 16-bit samples, 1/8192 a step (+-4): 1 KB, in RAM (the pool keeps its 4 KB spare) */
static int16_t fe_buf[FE_N];
static uint32_t fe_w;
static float fe_hp, fe_ph, fe_ph2, fe_amt;
static int fm_is_bass(int c)                   /* the bass voices (the same slots in the DX and the DY set; and the thirty more's three) */
{
    return c == 2 || c == 9 || c == 20 || c == 26 || (c >= EN_NFMFACT + EN_NUSER + 20 && c <= EN_NFMFACT + EN_NUSER + 22);
}
static float fe_tap(float d)                    /* d samples back, between two */
{
    float at = (float)fe_w - d, f;
    int i;
    if (at < 0.0f)
        at += (float)FE_N;
    i = (int)at;
    f = at - (float)i;
    {
        const float a = (float)fe_buf[i & (FE_N - 1)], b = (float)fe_buf[(i + 1) & (FE_N - 1)];
        return (a + (b - a) * f) * (1.0f / FE_Q);
    }
}

/* THE CHORUS (0.17.44, the CHORUS page; the maker: "a Juno-106 with different types of chorus" for wider sounds). (0.17.49) And a rotary speaker's,
 * slow and fast: the same delay swept by a sine (the horn coming and going, a little pitch), each side's level swung a quarter turn later (the
 * horn pointing at it, then away), 0.8 or 6.7 turns a second, the speed gliding from one to the other over about a second. As the Juno's: one short
 * delay swept by a triangle, the other side swept the opposite way, mixed with the dry: I slow and deep (0.51 Hz), II faster (0.86 Hz), I+II
 * a fast shallow shimmer (9.75 Hz), 1.7 .. 5.4 ms. On the pluck (its line is the DX voices' ensemble's: the plain pluck and a DX voice never
 * play at once, so they share it; the ensemble steps aside), the acid (a line of its own, 512 B), or both; the lows (under 200 Hz) stay
 * dry in the middle. Off when the load is far too high (shed level 2). */
#define JA_N 256
#define JC_WET 0.8f                             /* the swept copies' share; the dry and the copies scaled so the level stays where it was */
#define JC_NORM 0.78f
static int16_t ja_buf[JA_N];
static uint32_t ja_w;
static float jc_ph, jc_rate, jc_c, jc_d, ja_hp, jp_hp, jc_am;
static uint8_t jc_pl, jc_ac, jc_sin;
static void jc_set(void)                        /* once a chunk: the type and the targets */
{
    static const float RATE[5] = {0.513f, 0.863f, 9.75f, 0.8f, 6.7f};
    const int t = fxs_v[EN_FXS_JC_TYPE], g = fxs_v[EN_FXS_JC_TGT];
    const int on = t >= 1 && t <= 5 && en_shed_level < 1;   /* (0.17.61: shed level 1 now: the enhancers before the DX voices) */
    float want;
    jc_pl = (uint8_t)(on && g != 1);
    jc_ac = (uint8_t)(on && g != 0);
    if (!on)
        return;
    want = RATE[t - 1] * (1.0f / EN_SR);
    jc_rate = t >= 4 && jc_sin ? jc_rate + (want - jc_rate) * 0.012f : want;   /* (the rotor speeds up and slows down; the Juno's at once) */
    jc_sin = (uint8_t)(t >= 4);
    jc_c = 0.0035f * EN_SR;
    jc_d = (t == 3 ? 0.00025f : t >= 4 ? 0.0004f : 0.00185f) * EN_SR;
    jc_am = t >= 4 ? 0.3f : 0.0f;
    ja_hp = fm_flush(ja_hp);
    jp_hp = fm_flush(jp_hp);
}
static inline float jc_tri(float p)             /* the triangle, -1 .. 1 */
{
    p -= (float)(int)p;
    return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
}
static inline float jc_lfo(float p) { return jc_sin ? sine(p) : jc_tri(p); }   /* (0.17.49: the rotor's a sine) */
static inline float ja_tap(float d)
{
    float at = (float)ja_w - d, f;
    int i;
    if (at < 0.0f)
        at += (float)JA_N;
    i = (int)at;
    f = at - (float)i;
    {
        const float a = (float)ja_buf[i & (JA_N - 1)], b = (float)ja_buf[(i + 1) & (JA_N - 1)];
        return (a + (b - a) * f) * (1.0f / FE_Q);
    }
}
static inline int16_t jc_q(float h)
{
    h *= FE_Q;
    return (int16_t)(h > 32767.0f ? 32767.0f : h < -32767.0f ? -32767.0f : h);
}

/* the DX voices (the pluck's sounds from CM_NPLAIN): a stage of the chunk's job, with the pad's, into their own buffers */
static void render_fm(uint32_t n)
{
    const int on = par[P_CM_SND] >= CM_NPLAIN && cm_vol > 0.0f;
    const int cur = on && fm_cur >= 0 ? fm_cur : 0;   /* (the voice that is loaded: an empty user slot plays the first built-in one, at its own gain) */
    const float g = cm_vol * 0.5f * FM_OUT * (fm_dy_cur && cur < EN_NFMFACT ? FM_GAIN_DY[cur] : FM_GAIN[cur]);
    uint32_t j;
    float lvl = 0.0f;
    const float fe_to = en_shed_level >= 1 || fm_is_bass(cur) ? 0.0f : FE_AMT, fe_k = 1.0f - fm_expf(-FM_TWO_PI * 200.0f / EN_SR), ms = EN_SR * 0.001f;
    if (!on) {
        for (j = 0; j < n; j++)
            fm_ol[j] = fm_or[j] = fm_rs[j] = fm_es[j] = 0.0f;
        fm_ctr += n;
        fe_amt = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x = fm_fifo[(fm_ctr - FM_D + j) & (FM_R - 1)];   /* (rendered a chunk ago, by the other core) */
        const float y = (x < -16.0f ? -16.0f : x > 16.0f ? 16.0f : x) * g * TG(TR_CM, j);
        fe_amt += (fe_to - fe_amt) * 0.0005f;   /* (gliding: a new voice, a bass, the load) */
        fe_hp += fe_k * (y - fe_hp);
        {
            const float h = (y - fe_hp) * FE_Q;
            fe_buf[fe_w] = (int16_t)(h > 32767.0f ? 32767.0f : h < -32767.0f ? -32767.0f : h);
        }
        fe_ph += 0.37f / EN_SR;
        fe_ph2 += 5.3f / EN_SR;
        if (fe_ph >= 1.0f)
            fe_ph -= 1.0f;
        if (fe_ph2 >= 1.0f)
            fe_ph2 -= 1.0f;
        if (jc_pl && !fm_is_bass(cur)) {        /* (0.17.44) the CHORUS on the pluck: the Juno's sweep on the same line, instead of the ensemble's */
            const float p = jc_ph + (float)j * jc_rate, t = jc_lfo(p), a = jc_am * jc_tri(p + 0.25f);   /* (a: the rotor's level swing, 0 for the Juno) */
            fm_ol[j] = (y + JC_WET * fe_tap(jc_c + jc_d * t)) * JC_NORM * (1.0f + a);
            fm_or[j] = (y + JC_WET * fe_tap(jc_c - jc_d * t)) * JC_NORM * (1.0f - a);
        } else if (fe_amt > 1e-4f) {
            const float wl = fe_tap((8.0f + 2.5f * sine(fe_ph) + 0.12f * sine(fe_ph2)) * ms),
                        wr = fe_tap((8.0f + 2.5f * sine(fe_ph + 0.25f) + 0.12f * sine(fe_ph2 + 0.5f)) * ms),
                        nm = 1.0f - 0.5f * fe_amt * fe_amt;   /* (about 1 / sqrt(1 + amt^2): the level stays where it was) */
            fm_ol[j] = (y + fe_amt * wl) * nm;
            fm_or[j] = (y + fe_amt * wr) * nm;
        } else {
            fm_ol[j] = y;
            fm_or[j] = y;
        }
        fe_w = (fe_w + 1u) & (FE_N - 1u);
        fm_rs[j] = y * 0.35f;
        fm_es[j] = y * 0.5f;
        lvl = fm_maxf(lvl, y < 0.0f ? -y : y);
    }
    en_cm_level = fm_minf(lvl * 4.0f, 1.0f);
    fm_ctr += n;
    fe_hp = fm_flush(fe_hp);
}

static void render_cm(uint32_t n)
{
    const int sn = par[P_CM_SND] < 0 ? 0 : par[P_CM_SND] >= CM_NPLAIN ? CM_NPLAIN - 1 : par[P_CM_SND];
    const cm_snd_t *S = &CM_SOUND[sn];
    const float g = cm_vol * 0.5f * S->g, pfc = (HB(EN_V_PLUCK) ? hb_pf : 1.0f) * vt_pf[1];   /* (a keyboard's bend) */
    float lvl = 0.0f;
    int v;
    uint32_t j;
    cm_midi_step((int32_t)n);                   /* the notes out on MIDI: their note-offs when due */
    if (par[P_CM_SND] >= CM_NPLAIN) {              /* a DX voice: render_fm */
        cm[0].env = cm[1].env = 0.0f;
        return;
    }
    if (g <= 0.0f) {
        cm[0].env = cm[1].env = 0.0f;
        en_cm_level = 0.0f;
        return;
    }
    for (v = 0; v < 2; v++) {
        const float pl = v ? 0.65f : 0.35f, pr = 1.0f - pl;
        if (cm[v].env < 0.0008f)
            continue;
        for (j = 0; j < n; j++) {
            const float e = cm[v].env, idx = S->i0 + S->i1 * (S->rise ? 1.0f - e : e),
                        y = sine_any(cm[v].ph + idx * sine(cm[v].ps)) * e * fm_minf((float)cm[v].age * (1.0f / 96.0f), 1.0f) * cm[v].vel * g *
                            TG(TR_CM, j);
            mixl[j] += y * pl * 2.0f;
            mixr[j] += y * pr * 2.0f;
            rsend[j] += y * 0.35f;
            esend[j] += y * 0.5f;
            cm[v].ph += cm[v].inc * pfc;
            if (cm[v].ph >= 1.0f)
                cm[v].ph -= 1.0f;
            cm[v].ps += cm[v].inc * pfc * S->fr;
            if (cm[v].ps >= 1.0f)
                cm[v].ps -= (float)(int)cm[v].ps;
            cm[v].env *= cm[v].dm;
            cm[v].age++;
            lvl = fm_maxf(lvl, e * cm[v].vel);
        }
        cm[v].env = fm_flush(cm[v].env);
    }
    en_cm_level = lvl;
}

#if EN_ACID_RAM && !defined(EN_HOST) && !defined(EN_WEB)
__attribute__((section(".ram_hot.acid"), noinline, flatten))   /* (flatten: its helpers inlined, a call from RAM cannot reach the flash) */
#endif
static void render_acid(uint32_t n)
{
    const int ft_acid = fxs_v[EN_FXS_FT_ACID];
    uint32_t j;
    const acid_snd_t *AS = &ACID_SOUND[(unsigned)par[P_AC_WAVE] < EN_NACID ? par[P_AC_WAVE] : 0];
    const int osc = AS->osc;
    const int sq = fxs_v[EN_FXS_SQ_TGT] != 1;    /* SCREAM on the acid (its Target: 0 the acid, 2 both); at 0 each factor below is exactly 1 */
    const float sqd = sq ? (float)fxs_v[EN_FXS_SQ_DRIVE] * 0.01f : 0.0f, sqa = sq ? (float)fxs_v[EN_FXS_SQ_AMT] * 0.01f : 0.0f,
                sqw = sq ? (float)fxs_v[EN_FXS_SQ_WOW] * 0.01f : 0.0f;
    const float g = acid_vol * 0.75f * AS->g / (1.0f + 0.5f * sqd + 0.4f * sqa), adrv = AS->drive, res = (float)par[P_AC_RES] * 0.01f,
                envmod = (float)par[P_AC_ENV] * 0.06f, accamt = (float)par[P_AC_ACC] * 0.01f, base = (float)par[P_AC_CUT] * 0.066f,
                k1 = (1.45f - 1.38f * res) * (1.0f - 0.93f * sqa) * (1.0f - 0.85f * lfo_ares),   /* Scream: the resonance to about Q 200; an LFO on it */
                k2 = 1.2f * (1.0f - 0.75f * sqa), sqin = 1.0f + 3.0f * sqd,   /* the second stage follows (it held the peak down); Drive: harder into the filter */
                vel = 0.75f + 0.5f * accamt * acid.acc, glide = acid.sliding ? 0.0012f : 1.0f;
    const float inv_n = 1.0f / (float)(n ? n : 1u);   /* (0.17.44: the LFO's glide across the chunk) */
    static float ac_wow, ac_sratio = 1.0f, ac_pw = 0.5f, ac_lz = 1.0f;   /* (0.17.62) the 32-sample values, kept across chunks as the count is: each chunk
                                                 * began at the defaults (Sync's ratio 1, the PWM's width 0.5, Laser's pitch, no Wow) until the next update,
                                                 * up to 31 samples a chunk (found by an outside review) */
    float sqg = sqin + (sqw > 0.0f ? 2.0f * ac_wow : 0.0f);   /* this block's drive: Wow, an accent opening the resonance and the drive further (the held Wow
                                                 * only while Wow is on: off takes effect at once) */
    static float ans_k = 1.0f;                   /* the answer's level (EN_FXS_AC_ANS), gliding over about 20 ms as the answer comes and goes */
    {
        const int al = fxs_v[EN_FXS_AC_ANS];
        const float to = en_acid_ans && al > 0 ? fm_exp2f(0.1107309f * (float)(al > 3 ? 3 : al)) : 1.0f;   /* (2 dB a step: 2^(2 / 20 * log2 10)) */
        ans_k += (to - ans_k) * fm_minf((float)n * (1.0f / (0.02f * EN_SR)), 1.0f);
    }
    float ph = acid.ph, inc = acid.inc, amp = acid.amp, lvl = 0.0f, ps = acid.ps, p3 = acid.p3, p4 = acid.p4, p5 = acid.p5, sratio = ac_sratio, pw = ac_pw, lz = ac_lz;
    if (amp < 1e-5f && acid.amp_t <= 0.0f) {
        acid.amp = 0.0f;
        en_acid_level = 0.0f;
        for (j = 0; j < n; j++)
            acid_o[j] = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x, y;
        if (!(ac_ctl++ & 31u) || acid.env >= 1.0f) {   /* the cutoff: its knob, the envelope, an accent's push (every 32 samples, counted across chunks;
                                                 * 0.17.64: and at once on a new note, whose envelope is 1 until this first update: it waited up to 31
                                                 * samples with the last note's values) */
            float oct = base + acut_was + (lfo_acut - acut_was) * (float)j * inv_n + envmod * acid.env * (1.0f + accamt * acid.acc) + 1.2f * accamt * acid.acc * acid.env;   /* (0.17.44: the LFO gliding across the chunk from its last value: a fast one stepped, a zipper) */
            float fc = fm_clampf(60.0f * fm_exp2f(oct), 50.0f, 14000.0f);
            const float wow = sqw * acid.acc * acid.env;
            ac_wow = wow;
            sqg = sqin + 2.0f * wow;
            if (ft_acid == FT_303) {            /* the 303: the stages' gain G, the feedback k, G^4 and 1 / (1 + k G^4) (acid.c1 holds them) */
                const float gg = fm_tanf(FM_PI * fm_clampf(fc, 40.0f, 16000.0f) * (1.0f / EN_SR)), G = gg / (1.0f + gg), G2 = G * G,
                            k = fm_minf((0.15f + 3.85f * res) * (1.0f + 0.5f * sqa) * (1.0f - 0.85f * lfo_ares) * (1.0f + 0.3f * wow), 4.4f);
                acid.c1[0] = G;
                acid.c1[1] = k;
                acid.c1[2] = G2 * G2;
                acid.c1[3] = 1.0f / (1.0f + k * G2 * G2);
            } else {
                svf_coefs(acid.c1, fc, k1 * (1.0f - 0.8f * wow));
                svf_coefs(acid.c2, ft_acid == 1 ? fc * (1.0f / 1.15f) : fc * 1.15f, k2);
            }
            sratio = 1.6f + 2.6f * acid.env;    /* Sync: the slave's pitch, swept by the envelope */
            acid.lph += 32.0f * 0.35f / EN_SR;  /* PWM: its width, 0.35 Hz */
            if (acid.lph >= 1.0f)
                acid.lph -= 1.0f;
            pw = 0.5f + 0.38f * sine(acid.lph);
            lz = 1.0f + 7.0f * acid.env * acid.env;   /* Laser: its pitch, falling as the envelope does */
            ac_sratio = sratio;
            ac_pw = pw;
            ac_lz = lz;
            acid.env *= acid.edec;
        }
        if (acid.gate_left > 0 && --acid.gate_left == 0)
            acid.amp_t = 0.0f;                  /* the gate closes */
        inc += (acid.inc_t * (HB(EN_V_ACID) ? hb_pf : 1.0f) * vt_pf[0] - inc) * glide;      /* a slide: ~50 ms (a keyboard's bend on the target) */
        switch (osc) {
        case AO_SQUARE: case AO_PWM: {          /* a square (or a pulse): a saw less itself, part a turn on */
            float p2 = ph + (osc == AO_PWM ? pw : 0.5f);
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            x = 0.7f * (blep_saw(ph, inc) - blep_saw(p2, inc));
            break;
        }
        case AO_SYNC:                           /* a saw hard-synced to the note, swept up by the envelope */
            x = blep_saw(ps, inc * sratio);
            ps += inc * sratio;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            break;
        case AO_PLUCK:                          /* FM, the index falling with the envelope */
            x = sine_any(ph + (0.2f + 2.2f * acid.env) * sine(2.0f * ph)) * 1.2f;
            break;
        case AO_CHIP:                           /* a raw square, as an 8-bit game's */
            x = ph < 0.5f ? 0.75f : -0.75f;
            break;
        case AO_SUB:                            /* the saw and a square an octave down */
        {   /* (0.17.44: band-limited, two saws half a turn apart: the raw square aliased at -30 dB on a high note) */
            const float q = ps + 0.5f >= 1.0f ? ps - 0.5f : ps + 0.5f;
            x = blep_saw(ph, inc) + 0.7f * (blep_saw(q, inc * 0.5f) - blep_saw(ps, inc * 0.5f));
        }
            ps += inc * 0.5f;
            if (ps >= 1.0f)
                ps -= 1.0f;
            break;
        case AO_TRANCE:                         /* three saws, -15, 0, +15 cents */
            x = 0.5f * (blep_saw(ph, inc) + blep_saw(ps, inc * 1.0087f) + blep_saw(p3, inc * 0.9913f));
            ps += inc * 1.0087f;
            p3 += inc * 0.9913f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            if (p3 >= 1.0f)
                p3 -= (float)(int)p3;
            break;
        case AO_BLEEP:
            x = sine(ph) * 1.3f;
            break;
        case AO_FIFTH:                          /* the saw and its fifth */
            x = 0.75f * (blep_saw(ph, inc) + 0.75f * blep_saw(ps, inc * 1.49831f));
            ps += inc * 1.49831f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            break;
        case AO_BELL:                           /* FM, the modulator at 3.5 */
            x = sine_any(ph + (0.15f + 1.8f * acid.env) * sine(ps)) * 1.2f;
            ps += inc * 3.5f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            break;
        case AO_ORGAN:
            x = (sine(ph) + 0.5f * sine(2.0f * ph) + 0.3f * sine(3.0f * ph) + 0.2f * sine(4.0f * ph)) * 0.95f;
            break;
        case AO_LASER:                          /* a sine, up to three octaves above the note as it starts, falling */
            x = sine(ps) * 1.25f;
            ps += inc * lz;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            break;
        case AO_FOLD:                           /* a sine through a sine: it folds over as the envelope opens */
            x = sine_any((0.3f + 1.2f * acid.env) * sine(ph)) * 1.1f;
            break;
        case AO_FMR: {                          /* FM at the sound's ratio: marimba, piano, kalimba, brass, gong, clav, vibes */
            const int sn = (unsigned)par[P_AC_WAVE] < EN_NACID ? par[P_AC_WAVE] : 0;
            const float idx = ACID_FM[sn].i0 + ACID_FM[sn].i1 * (ACID_FM[sn].rise ? 1.0f - acid.env : acid.env);
            x = sine_any(ph + idx * sine(ps)) * 1.2f;
            ps += inc * ACID_FM[sn].fr;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            break;
        }
        case AO_SUPER: {                        /* five saws, -24 .. +24 cents: the middle one the note */
            x = 0.38f * (blep_saw(ph, inc) + blep_saw(ps, inc * 1.0139f) + blep_saw(p3, inc * 0.9863f) +
                         blep_saw(p4, inc * 1.0278f) + blep_saw(p5, inc * 0.9728f));
            ps += inc * 1.0139f;
            p3 += inc * 0.9863f;
            p4 += inc * 1.0278f;
            p5 += inc * 0.9728f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            if (p3 >= 1.0f)
                p3 -= (float)(int)p3;
            if (p4 >= 1.0f)
                p4 -= (float)(int)p4;
            if (p5 >= 1.0f)
                p5 -= (float)(int)p5;
            break;
        }
        case AO_CZ: {                           /* phase distortion: a cosine's phase bent into a saw's shape (the knee falls as the
                                                 * envelope rises: a sine to a saw), a resonant peak above it, as the CZ's */
            const float d = 0.5f - 0.46f * acid.env;
            const float pp = ph < d ? ph * (0.5f / d) : 0.5f + (ph - d) * (0.5f / (1.0f - d));
            const float rs = sine_any(ph * (1.0f + 5.0f * acid.env)) * (1.0f - ph);
            x = (sine_any(pp + 0.25f) * 0.9f + 0.5f * rs) * 0.95f;
            break;
        }
        case AO_MINI:                           /* two saws 7 cents apart and a pulse an octave down */
        {   /* (0.17.44: the pulse band-limited, two saws 0.6 of a turn apart: no DC, no alias) */
            const float q = p3 + 0.6f >= 1.0f ? p3 - 0.4f : p3 + 0.6f;
            x = 0.55f * (blep_saw(ph, inc) + blep_saw(ps, inc * 1.0041f)) + 0.6f * (blep_saw(q, inc * 0.5f) - blep_saw(p3, inc * 0.5f));
        }
            ps += inc * 1.0041f;
            p3 += inc * 0.5f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            if (p3 >= 1.0f)
                p3 -= 1.0f;
            break;
        case AO_HYPER: {                        /* five saws, twice as wide as Super's */
            x = 0.38f * (blep_saw(ph, inc) + blep_saw(ps, inc * 1.0285f) + blep_saw(p3, inc * 0.9720f) + blep_saw(p4, inc * 1.0581f) + blep_saw(p5, inc * 0.9460f));
            ps += inc * 1.0285f;
            p3 += inc * 0.9720f;
            p4 += inc * 1.0581f;
            p5 += inc * 0.9460f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            if (p3 >= 1.0f)
                p3 -= (float)(int)p3;
            if (p4 >= 1.0f)
                p4 -= (float)(int)p4;
            if (p5 >= 1.0f)
                p5 -= (float)(int)p5;
            break;
        }
        case AO_CZP: {                          /* the phase held at half a turn for part of each half: a cosine that flattens into a square as the knee tightens */
            const float f = 1.0f + 5.0f * (1.0f - acid.env);
            const float pp = ph < 0.5f ? fm_minf(ph * f, 0.5f) : 1.0f - fm_minf((1.0f - ph) * f, 0.5f);
            x = sine_any(pp - 0.25f) * 1.05f;
            break;
        }
        case AO_CZB:                            /* the fundamental and a partial at 3.5, windowed by the cycle, its level following the envelope */
            x = (0.8f * sine_any(ph) + 0.55f * acid.env * sine_any(ph * 3.5f + 0.1f) * (1.0f - ph)) * 1.05f;
            break;
        case AO_LEAD: {                         /* two saws 7 cents apart and a pulse of wandering width */
            float q = ph + pw;
            if (q >= 1.0f)
                q -= 1.0f;
            x = 0.42f * (blep_saw(ph, inc) + blep_saw(ps, inc * 1.0041f)) + 0.45f * (blep_saw(ph, inc) - blep_saw(q, inc));
            ps += inc * 1.0041f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            break;
        }
        case AO_SOFT:                           /* a triangle */
            x = (ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph) * 1.2f;
            break;
        default:
            x = blep_saw(ph, inc);
            break;
        }
        ph += inc;
        if (ph >= 1.0f) {
            ph -= 1.0f;
            if (osc == AO_SYNC)
                ps = ph * sratio;               /* the sync: the slave starts again with the note */
        }
        if (ft_acid == FT_303) {
            /* THE 303 (0.17.17): four one-pole stages in a ladder with the output fed back (zero-delay: the loop solved for the output, then a soft
             * clip on the ladder's input, as the transistors do), the feedback's low end taken out (a 150 Hz high-pass in the loop, as the TB-303's
             * coupling does), so the bass stays full while the resonance sings and squeals near the top; then the squelch, a touch gentler. */
            const float G = acid.c1[0], k = acid.c1[1], b = 1.0f - G, xin = x * sqin * (1.0f + 0.45f * k),   /* (the passband held up as the resonance rises) */
                        sum = ((b * acid.s[0] * G + b * acid.s[1]) * G + b * acid.s[2]) * G + b * acid.s[3],
                        y4 = (acid.c1[2] * (xin + k * acid.lfb) + sum) * acid.c1[3];
            float u = fm_tanhf(1.2f * (xin - k * (y4 - acid.lfb))) * (1.0f / 1.2f), v;
            int st;
            for (st = 0; st < 4; st++) {        /* each stage, zero-delay: v = (in - s) G, out = v + s, s = out + v */
                v = (u - acid.s[st]) * G;
                u = v + acid.s[st];
                acid.s[st] = u + v;
            }
            acid.lfb += 0.0211f * (u - acid.lfb);
            y = fm_tanhf(u * (3.4f + 2.2f * res) * adrv * sqg) / fm_minf(adrv, 1.3f);   /* (its makeup: a 24 dB ladder passes less than the two SVFs, about 6 dB) */
        } else {
            y = svf_mm(x * sqin, acid.c1, &acid.s[0], ft_acid);
            y = fm_tanhf(y * (1.6f + 1.5f * res) * adrv * sqg) / fm_minf(adrv, 1.3f);   /* the squelch: driven between the two (Drive and Wow: harder) */
            if (FT_TWO(ft_acid))
                y = svf_mm(y, acid.c2, &acid.s[2], ft_acid);
            y *= FT_G[2][FT_IX(ft_acid)];
        }
        amp += (acid.amp_t - amp) * 0.006f;     /* ~4 ms edges */
        acid_o[j] = y * amp * vel * g * ans_k * TG(TR_ACID, j);   /* (the sends: 0.15 and 0.35 of it) */
        lvl = fm_maxf(lvl, amp);
    }
    for (j = 0; j < 4; j++)
        acid.s[j] = fm_flush(acid.s[j]);
    acid.lfb = fm_flush(acid.lfb);
    acut_was = lfo_acut;
    acid.ph = ph;
    acid.ps = ps;
    acid.p3 = p3;
    acid.p4 = p4;
    acid.p5 = p5;
    acid.inc = inc;
    acid.amp = amp;
    en_acid_level = lvl * (0.6f + 0.4f * acid.env);
}

/* the noise: one sample of a colour from a generator */
static inline float nz_white(nz_t *z)
{
    z->rng = z->rng * 1664525u + 1013904223u;
    return (float)(int32_t)z->rng * (1.0f / 2147483648.0f);
}
/* the vinyl (the VINYL key; its settings en_fxs VN_*): a few pops a second to forty, each a click that rings
 * a moment (a 2.8 kHz resonator, about a millisecond), most small and now and then a loud one, the loud ones
 * with a duller thump (a 700 Hz one); under them the faintest surface hiss (pink). Left and right pop apart. */
static inline float nz_sample(nz_t *z, int unused)
{
    float w, pk = 0.0f, o, d;
    uint32_t h;
    (void)unused;
    z->rng = z->rng * 1664525u + 1013904223u;    /* one step a sample: the noise from it, the chances from a scramble of it */
    w = (float)(int32_t)z->rng * (1.0f / 2147483648.0f);
    if (vn_hiss > 0.0f) {                       /* the hiss: pink (Paul Kellet's economy filter), only if it is wanted */
        z->b0 = 0.99765f * z->b0 + w * 0.0990460f;
        z->b1 = 0.96300f * z->b1 + w * 0.2965164f;
        z->b2 = 0.57000f * z->b2 + w * 1.0526913f;
        pk = (z->b0 + z->b1 + z->b2 + w * 0.1848f) * 0.22f * vn_hiss;
    }
    if (!(++z->cnt & 3u)) {                     /* the chances, every fourth sample at four times the odds (the same ticks and dust a second; a tick lasts
                                                 * 11 to 66 samples, so nothing is lost at that grain): two scrambles and their conversions a sample less */
        h = (z->rng ^ z->rng >> 15) * 2246822519u;
        if ((float)(h >> 8) * (1.0f / 16777216.0f) < 4.0f * vn_pop) {   /* a tick: a burst of noise that dies in a fraction of a millisecond or two */
            float a;
            h = (h ^ h >> 13) * 3266489917u;    /* another scramble: its size, not the number that chose it */
            a = (float)(h >> 8) * (1.0f / 16777216.0f);          /* 0 .. 1 */
            z->crack = (0.1f + 1.2f * a * a) * vn_amp;           /* most small, now and then a loud one */
            z->r1 = 0.90f + 0.075f * a;         /* its decay: 0.25 ms (small) to 1.5 ms (big) */
            if (a > 0.85f)
                z->y2 += 0.45f * vn_amp * (a - 0.85f) * 6.0f;    /* a loud one brings a dull thud with it */
        } else {
            h = (h ^ h >> 13) * 3266489917u;
            if ((float)(h >> 8) * (1.0f / 16777216.0f) < 20.0f * vn_pop)   /* and the fine dust between: five times as many, very small */
                z->r2 = 0.09f * vn_amp;
        }
    }
    z->pp += 0.55f * (w - z->pp);               /* a tick's body: noise with its top rolled off (about 5 kHz): full-band white clicks were piercing */
    d = z->pp * 0.9f;
    o = d * (z->crack + z->r2) + z->y2 + pk;
    z->crack *= z->r1;
    z->r2 *= 0.80f;
    z->y2 *= 0.9965f;
    z->pw = w;
    return o;
}

static float nzl[BLK], nzr[BLK];
static float nz_hl, nz_hr;                       /* the pair held across two samples */
static uint32_t nz_ph;
static void render_noise(uint32_t n)            /* into its own buffer: the pump ducks it on its own */
{
    uint32_t j;
    const float g = vin_m * vn_g, k = nz_lp_k;   /* the vinyl key fades it in; its settings are en_fxs's */
    if (g <= 0.0f || !en_playing || shed3) {     /* (shed level 3: the vinyl's noise is off) */
        for (j = 0; j < n; j++)
            nzl[j] = nzr[j] = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        const float tg = TG(TR_PAD, j) * g;
        if (!(nz_ph++ & 1u)) {                  /* a new pair every other sample, held for the next (0.14.4: the noise was 10 % of the first core in every
                                                 * genre with the vinyl latched): the hiss is pink and the ticks' bodies roll off at 5 kHz, and the hold's
                                                 * own roll-off (-4 dB at 11 kHz) sits under the Tone low-pass */
            nz_hl = nz_sample(&nz[0], nz_color);
            nz_hr = nz_sample(&nz[1], nz_color);
        }
        nz[0].lp += k * (nz_hl - nz[0].lp);     /* Tone: a low-pass, 300 Hz .. 19 kHz */
        nz[1].lp += k * (nz_hr - nz[1].lp);
        nzl[j] = nz[0].lp * tg;
        nzr[j] = nz[1].lp * tg;
    }
    nz[0].lp = fm_flush(nz[0].lp);
    nz[1].lp = fm_flush(nz[1].lp);
    nz[0].br = fm_flush(nz[0].br);
    nz[1].br = fm_flush(nz[1].br);
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, its corner the lane's tune); little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att, fo, x = 0.0f;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att, fo = v->fo;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp;
        for (k = 0; k < 4; k++) {
            v->mph[k] += hat_inc[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + KH->h_noise * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += v->hp * hp;
        x2 += v->hp * x1;
        x = hp * amp * att * g * TG(TR_HATS, j) + fo;
        fo *= DR_FADE;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->fo = fm_flush(fo);
    v->xl = x;
    v->on = amp > 0.0005f;
}

/* the clap: noise through a band-pass, three bursts 8 ms apart, the last with a tail */
static void render_clap(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float s1 = v->x1, s2 = v->x2, amp = v->amp, t = v->t, ph = v->ph, fo = v->fo, x = 0.0f;
    const float gap = KS->cl_gap;
    if (!v->on)
        return;
    for (j = 0; j < n; j++) {
        float in, bp;
        if (t == gap || t == 2.0f * gap)
            amp = v->pend;                      /* the next burst */
        noise = noise * 1664525u + 1013904223u;
        in = (float)(int32_t)noise * (1.0f / 2147483648.0f);
        bp = in - s2 - 0.7f * s1;               /* Chamberlin SVF: the band-pass is s1 */
        s1 += v->hp * bp;
        s2 += v->hp * s1;
        x = (s1 * 1.6f + 0.35f * sine(ph) * fm_maxf(1.0f - t * (1.0f / 2500.0f), 0.0f)) * amp * g * TG(TR_SNARE, j) + fo;
        fo *= DR_FADE;
        ph += v->inc;
        if (ph >= 1.0f)
            ph -= 1.0f;
        mixl[j] += x * 0.95f;
        mixr[j] += x;
        rsend[j] += x * 0.45f;
        esend[j] += x * 0.12f;
        amp *= t < 2.0f * gap ? v->inc_end : v->adec;
        t += 1.0f;
    }
    v->x1 = fm_flush(s1);
    v->x2 = fm_flush(s2);
    v->amp = amp;
    v->t = t;
    v->ph = ph;
    v->fo = fm_flush(fo);
    v->xl = x;
    v->on = t < 2.0f * gap || amp > 0.0005f;
}

/* the snare: two tuned sines falling quickly, and noise through a band-pass decaying more slowly */
static void render_snare(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float s1 = v->x1, s2 = v->x2, amp = v->amp, ph = v->ph, ph2 = v->ph2, att = v->att, body = v->idx, fo = v->fo, x = 0.0f;
    if (!v->on)
        return;
    if (v->t == 0.0f)
        body = 1.0f;
    for (j = 0; j < n; j++) {
        float in, bp;
        noise = noise * 1664525u + 1013904223u;
        in = (float)(int32_t)noise * (1.0f / 2147483648.0f);
        bp = in - s2 - 0.5f * s1;
        s1 += v->hp * bp;
        s2 += v->hp * s1;
        x = ((sine(ph) + 0.6f * sine(ph2)) * body * KS->sn_body + s1 * KS->sn_noise) * amp * att * g * TG(TR_SNARE, j) + fo;
        fo *= DR_FADE;
        ph += v->inc;
        ph2 += v->inc2;
        if (ph >= 1.0f)
            ph -= 1.0f;
        if (ph2 >= 1.0f)
            ph2 -= 1.0f;
        mixl[j] += x;
        mixr[j] += x * 0.95f;
        rsend[j] += x * 0.35f;
        esend[j] += x * 0.1f;
        amp *= v->adec;
        body *= 0.9992f;                        /* the body: ~20 ms */
        att += (1.0f - att) * 0.4f;
    }
    v->x1 = fm_flush(s1);
    v->x2 = fm_flush(s2);
    v->amp = amp;
    v->ph = ph;
    v->ph2 = ph2;
    v->att = att;
    v->idx = body;
    v->t = 1.0f;
    v->fo = fm_flush(fo);
    v->xl = x;
    v->on = amp > 0.0005f;
}

/* the perc: an FM blip (the modulator at 1.48 times, its index falling), the pitch falling into place */
static void render_perc(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float ph = v->ph, ph2 = v->ph2, inc = v->inc, amp = v->amp, idx = v->idx, att = v->att, fo = v->fo, x = 0.0f;
    if (!v->on)
        return;
    const float pn = KP->p_noise;
    float lp = v->x1;
    for (j = 0; j < n; j++) {
        float m = sine(ph2) * idx, y = sine(ph + m + 4.0f);
        if (pn > 0.0f) {                         /* (0.17.34) the shaker: the noise above about 2.5 kHz */
            float w;
            noise = noise * 1664525u + 1013904223u;
            w = (float)(int32_t)noise * (1.0f / 2147483648.0f);
            lp += 0.3f * (w - lp);
            y += pn * (1.6f * (w - lp) - y);
        }
        x = y * amp * att * g * TG(TR_PERC, j) + fo;
        fo *= DR_FADE;
        ph += inc;
        ph2 += inc * v->inc2;
        if (ph >= 1.0f)
            ph -= 1.0f;
        if (ph2 >= 1.0f)
            ph2 -= 1.0f;
        inc += (v->inc_end - inc) * 0.004f;
        mixl[j] += x * 0.8f;
        mixr[j] += x;
        rsend[j] += x * 0.3f;
        esend[j] += x * 0.25f;
        amp *= v->adec;
        idx *= 0.9995f;
        att += (1.0f - att) * 0.3f;
    }
    v->ph = ph;
    v->ph2 = ph2;
    v->inc = inc;
    v->amp = amp;
    v->idx = idx;
    v->att = att;
    v->x1 = fm_flush(lp);
    v->fo = fm_flush(fo);
    v->xl = x;
    v->on = amp > 0.0005f;
}

static float smp_next(drum_t *v)                /* the next sample of v's, decoded (0 past its end) */
{
    const smp_t *s = v->sp;
    int c, st, df;
    if (v->si >= s->n)
        return 0.0f;
    c = s->d[v->si >> 1] >> ((v->si & 1u) * 4u) & 15;
    v->si++;
    st = ADPCM_STEP[v->ai];
    df = st >> 3;
    if (c & 4)
        df += st;
    if (c & 2)
        df += st >> 1;
    if (c & 1)
        df += st >> 2;
    v->pr += c & 8 ? -df : df;
    v->pr = v->pr < -32768 ? -32768 : v->pr > 32767 ? 32767 : v->pr;
    st = v->ai + ADPCM_IDX[c];
    v->ai = (uint8_t)(st < 0 ? 0 : st > 88 ? 88 : st);
    return (float)v->pr * (1.0f / 32768.0f);
}
static void render_smp(drum_t *v, uint32_t n, float g, int tr, float pl, float pr, float rs, float es)   /* a drum playing a sample */
{
    uint32_t j;
    float ph = v->ph, amp = v->amp, fo = v->fo, x = 0.0f;
    if (!v->on)
        return;
    for (j = 0; j < n; j++) {
        while (ph >= 0.0f) {                     /* (x1, x2: the two samples about the place; ph from -1 to 0 between them) */
            v->x1 = v->x2;
            v->x2 = smp_next(v);
            ph -= 1.0f;
        }
        x = (v->x2 + (v->x1 - v->x2) * -ph) * amp * g * TG(tr, j) + fo;
        fo *= DR_FADE;
        ph += v->inc;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * rs;
        esend[j] += x * es;
        amp *= v->adec;
    }
    v->ph = ph;
    v->amp = amp;
    v->fo = fm_flush(fo);
    v->xl = x;
    v->on = (v->si < v->sp->n || fm_fabsf(v->x2) > 0.0001f) && amp > 0.0005f;
}

/* THE RESONATOR (0.17.51; after the DUB-1 resonator idea, the modal resonators of Mutable Instruments' Rings (MIT) as the model, three bands here, written
 * for PERF): the hits of the tracks chosen (the perc, the hats, the snare and clap, all of them, the pluck) ring three band-pass filters tuned to the
 * chord's notes (its root, third and fifth, in the register chosen, rising), so a rim or a tick sounds pitched, in the key, retuned with each chord. Decay:
 * how long they ring (Q 8 .. 256: wood to glass); Amount: how loud (0 off). The first band in the middle, the second left, the third right; a little
 * into the reverb. A send: what goes in is each chosen track's share of the mix (the mix after it less before it: no copies). The CHORUS page's
 * neighbour. Off when the load is far too high (shed level 2). */
static float *rz_in;                             /* (render_chunk's own, on the stack: the pool has no 1 KB to spare) */
#define RZ_FN __attribute__((noinline))          /* (one copy each: inlined at the five drums they cost 2 KB of flash) */
static float rz_c[3][4], rz_s[3][2], rz_g, rz_lv;   /* (rz_lv: the input's level, followed a chunk at a time) */
static uint8_t rz_m;                             /* the lanes in (bit L_*), 0x80 the pluck; 0 off */
static RZ_FN void rz_set(void)                   /* once a control: on, the bands' tuning and level */
{
    static const uint8_t TM[6] = {1u << L_PERC, 1u << L_HAT | 1u << L_OHAT, 1u << L_SNARE | 1u << L_CLAP, 1u << L_PERC | 1u << L_HAT | 1u << L_OHAT,
                                  1u << L_SNARE | 1u << L_CLAP | 1u << L_HAT | 1u << L_OHAT | 1u << L_PERC, 0x80u};
    static uint32_t key = 0xFFFFFFFFu;
    const int amt = fxs_v[EN_FXS_RZ_AMT], dec = fxs_v[EN_FXS_RZ_DEC], reg = fxs_v[EN_FXS_RZ_REG], tg = fxs_v[EN_FXS_RZ_TGT];
    const uint8_t m = amt > 0 && en_shed_level < 1 ? TM[(unsigned)tg < 6u ? tg : 0] : 0u;   /* (0.17.61: level 1) */
    uint32_t k2;
    int b;
    if (!m) {
        if (rz_m)
            for (b = 0; b < 3; b++)
                rz_s[b][0] = rz_s[b][1] = 0.0f;
        rz_m = 0;
        return;
    }
    rz_m = m;
    k2 = (uint32_t)(chord_pcs[0] & 15) | (uint32_t)(chord_pcs[chord_nt > 1 ? 1 : 0] & 15) << 4 | (uint32_t)(chord_pcs[chord_nt > 2 ? 2 : 0] & 15) << 8 |
         (uint32_t)(reg & 3) << 12 | (uint32_t)(dec & 127) << 14 | (uint32_t)(par[P_ROOT] & 15) << 21;
    if (k2 != key) {                             /* (the chord, the register or the decay changed: the bands retuned) */
        const float kq = 0.125f * fm_exp2f(-0.05f * (float)dec);   /* 1/Q: 1/8 .. 1/256 */
        int prev = 0;
        key = k2;
        for (b = 0; b < 3; b++) {
            int nt = 36 + 12 * (reg % 3) + (chord_pcs[b < chord_nt ? b : 0] % 12);
            while (nt <= prev)
                nt += 12;
            prev = nt;
            svf_coefs(rz_c[b], 440.0f * fm_exp2f((float)(nt - 69) * (1.0f / 12.0f)), kq);
        }
    }
    rz_g = (float)amt * 0.01f * RZ_MAKE * fm_exp2f(0.025f * (float)dec) * (m & 0x80u ? RZ_PLK : 1.0f);   /* (the narrower, the less a hit's energy rings it: made
                                                                                                    * up by about sqrt Q; the pluck's notes need none of it) */
}
static RZ_FN void rz_sub(uint32_t n, float k)          /* before a chosen track: the mix as it was (k: its share) */
{
    uint32_t j;
    for (j = 0; j < n; j++)
        rz_in[j] -= (mixl[j] + mixr[j]) * k;
}
static RZ_FN void rz_add(uint32_t n, float k)          /* after it: what it added */
{
    uint32_t j;
    for (j = 0; j < n; j++)
        rz_in[j] += (mixl[j] + mixr[j]) * k;
}
static RZ_FN void rz_run(uint32_t n)             /* the bands rung, into the mix and the reverb */
{
    uint32_t j;
    int b;
    float pk = 0.0f, nm;
    for (j = 0; j < n; j++)                      /* the input brought to one level (its peaks over about a second): a quiet pluck or a soft perc rings them as a
                                                 * loud snare does, so Amount means the same in every piece (it was 30 dB apart) */
        pk = fm_maxf(pk, fm_fabsf(rz_in[j]));
    rz_lv = pk > rz_lv ? pk : fm_flush(rz_lv * fm_expf(-(float)n * (1.0f / EN_SR)));
    nm = 0.5f * fm_minf(0.25f / (rz_lv + 1e-4f), 32.0f);
    for (j = 0; j < n; j++) {
        float y[3];
        const float x = rz_in[j] * nm;
        for (b = 0; b < 3; b++) {
            float *const s = rz_s[b];
            const float *const c = rz_c[b], v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
            s[0] = 2.0f * v1 - s[0];
            s[1] = 2.0f * v2 - s[1];
            y[b] = c[3] * v1 * rz_g;
        }
        mixl[j] += y[0] + y[1] + 0.5f * y[2];
        mixr[j] += y[0] + 0.5f * y[1] + y[2];
        rsend[j] += 0.3f * (y[0] + y[1] + y[2]);
    }
    for (b = 0; b < 3; b++) {
        rz_s[b][0] = fm_flush(rz_s[b][0]);
        rz_s[b][1] = fm_flush(rz_s[b][1]);
    }
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.1f;
    drum_t *v;
    v = &drum[L_KICK];                           /* kick: a sine falling fast to its note, a click, driven */
    if (v->sp)
        render_smp(v, n, g * lane_g[L_KICK] * kit_g[L_KICK] * 0.9f, TR_KICK, 1.0f, 1.0f, 0.06f, 0.04f);
    else if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att, ck = v->idx, gk = g * lane_g[L_KICK] * 1.15f, fo = v->fo, x = 0.0f;
        float r1 = v->x1, r2 = v->x2, rl1 = v->mph[0], rl2 = v->mph[1], rp = v->ph2;
        const float dk = drive_k * KK->k_drive, drop = KK->k_drop, cka = KK->k_click, rum = KK->k_rumble;
        const float norm = 0.85f / fm_minf(1.0f + 0.25f * (dk - 1.0f), 1.6f);
        for (j = 0; j < n; j++) {
            float nz;
            noise = noise * 1664525u + 1013904223u;
            nz = (float)(int32_t)noise * (1.0f / 2147483648.0f);
            x = sine(ph) * amp * att + ck * cka * nz;
            if (rum > 0.0f) {                         /* the rumble: low noise and the note, swelling after */
                rl1 += 0.012f * (nz - rl1);           /* the hit and dying slowly, as a reverb's low end */
                rl2 += 0.012f * (rl1 - rl2);
                x += (r1 - r2) * (3.0f * rl2 + 0.5f * sine(rp));
                rp += v->inc_end;
                if (rp >= 1.0f)
                    rp -= 1.0f;
                r1 *= 0.99985f;                       /* ~0.35 s */
                r2 *= 0.9985f;                        /* ~35 ms */
            }
            x = fm_tanhf(x * 1.3f * dk) * norm * gk * TG(TR_KICK, j) + fo;
            fo *= DR_FADE;
            mixl[j] += x;
            mixr[j] += x;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * drop;        /* the drop: ~14 ms */
            amp *= v->adec;
            att += (1.0f - att) * 0.25f;
            ck *= 0.985f;                            /* the click: ~2 ms */
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->idx = ck;
        v->x1 = r1;
        v->x2 = r2;
        v->mph[0] = fm_flush(rl1);
        v->mph[1] = fm_flush(rl2);
        v->ph2 = rp;
        v->fo = fm_flush(fo);
        v->xl = x;
        v->on = amp > 0.0005f || r1 - r2 > 0.002f;
    }
#define SMP_OR(d, call, gs, tr, pl, pr, rs, es) do { const int rz_ = rz_m >> (d) & 1u; if (rz_) rz_sub(n, 1.0f); \
        if (drum[d].sp) render_smp(&drum[d], n, g * (gs) * lane_g[d] * kit_g[d], tr, pl, pr, rs, es); else call; if (rz_) rz_add(n, 1.0f); } while (0)
    SMP_OR(L_SNARE, render_snare(&drum[L_SNARE], n, g * 0.38f * lane_g[L_SNARE] * kit_g[L_SNARE]), 0.5f, TR_SNARE, 1.0f, 0.95f, 0.35f, 0.1f);
    SMP_OR(L_CLAP, render_clap(&drum[L_CLAP], n, g * 0.42f * lane_g[L_CLAP] * kit_g[L_CLAP]), 0.5f, TR_SNARE, 0.95f, 1.0f, 0.4f, 0.1f);
    SMP_OR(L_HAT, render_hat(&drum[L_HAT], n, g * 0.22f * lane_g[L_HAT] * kit_g[L_HAT], 0.75f, 1.0f), 0.3f, TR_HATS, 0.75f, 1.0f, 0.1f, 0.1f);    /* a little right */
    SMP_OR(L_OHAT, render_hat(&drum[L_OHAT], n, g * 0.18f * lane_g[L_OHAT] * kit_g[L_OHAT], 1.0f, 0.75f), 0.3f, TR_HATS, 1.0f, 0.75f, 0.15f, 0.1f);  /* a little left */
    SMP_OR(L_PERC, render_perc(&drum[L_PERC], n, g * 0.3f * lane_g[L_PERC] * kit_g[L_PERC]), 0.4f, TR_PERC, 0.8f, 1.0f, 0.3f, 0.25f);
#undef SMP_OR
    if (rz_m)
        rz_run(n);                               /* (0.17.51) the resonator */
}

_Static_assert(BLK == BLK_FM, "the DX voices' buffers are a chunk long");
_Static_assert(BLK <= RV_MAX && RV_MAX <= 256, "a chunk is at most the predelay line's extra length");
/* the mixer: a layer's place (pan: a balance, the middle leaves it as it is) and its sends. The layers render straight into the mix, so a layer that is off its
 * defaults is taken as the difference it makes: the mix as it was before it, its change scaled after. The drone 1, the acid 2, the pluck 3 and the drums 4 (the pad 0
 * and the DX voices are in their own buffers: scaled where they are added). */
static float mx_sv[4][BLK] EN_POOL;
static void mx_gains(int t, float *gl, float *gr, float *sg)
{
    const float p = (float)par[P_PAN + t] * 0.01f;
    *gl = fm_minf(1.0f, 2.0f * (1.0f - p));
    *gr = fm_minf(1.0f, 2.0f * p);
    *sg = (float)par[P_SEND + t] * 0.02f;
}
static int mx_off(int t) { return par[P_PAN + t] != 50 || par[P_SEND + t] != 50; }
static int jc_plain(void) { return jc_pl && par[P_CM_SND] < CM_NPLAIN; }   /* (0.17.44) the CHORUS on the plain pluck: it needs the mix as it was before it */
static void mx_begin(int t, uint32_t n)
{
    uint32_t j;
    if (!mx_off(t) && !(t == 3 && jc_plain()))
        return;
    for (j = 0; j < n; j++) {
        mx_sv[0][j] = mixl[j];
        mx_sv[1][j] = mixr[j];
        mx_sv[2][j] = rsend[j];
        mx_sv[3][j] = esend[j];
    }
}
static void mx_end(int t, uint32_t n)
{
    float gl, gr, sg;
    uint32_t j;
    if (!mx_off(t))
        return;
    mx_gains(t, &gl, &gr, &sg);
    for (j = 0; j < n; j++) {
        mixl[j] = mx_sv[0][j] + (mixl[j] - mx_sv[0][j]) * gl;
        mixr[j] = mx_sv[1][j] + (mixr[j] - mx_sv[1][j]) * gr;
        rsend[j] = mx_sv[2][j] + (rsend[j] - mx_sv[2][j]) * sg;
        esend[j] = mx_sv[3][j] + (esend[j] - mx_sv[3][j]) * sg;
    }
}
static void jc_cm(uint32_t n)                   /* (0.17.44) the plain pluck through the CHORUS: its part of the mix (the mix less what it was before it, mx_begin's) */
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        const float dl = mixl[j] - mx_sv[0][j], dr = mixr[j] - mx_sv[1][j], x = 0.5f * (dl + dr), p = jc_ph + (float)j * jc_rate, t = jc_lfo(p),
                    a = jc_am * jc_tri(p + 0.25f);
        jp_hp += 0.0281f * (x - jp_hp);         /* (200 Hz: the lows stay dry) */
        fe_buf[fe_w] = jc_q(x - jp_hp);
        mixl[j] = mx_sv[0][j] + (dl + JC_WET * fe_tap(jc_c + jc_d * t)) * JC_NORM * (1.0f + a);
        mixr[j] = mx_sv[1][j] + (dr + JC_WET * fe_tap(jc_c - jc_d * t)) * JC_NORM * (1.0f - a);
        fe_w = (fe_w + 1u) & (FE_N - 1u);
    }
}

/* the first core's time by stage, for the telemetry (the firmware sets the clock; the host leaves it off): ticks summed since last read */
uint32_t (*en_prof_clock)(void);
volatile uint8_t en_usb_fixed;                  /* the USB audio at a fixed level (the Settings' USB page) */
int16_t *en_usb_out;                            /* where this render writes it (16-bit, as the USB stream is), or NULL */
static int16_t *usb_cur;                        /* this chunk's part of en_usb_out, or NULL */
static float usb_gain;
volatile uint32_t en_prof[EN_NPROF];
volatile uint32_t en_prof_sm[11];
/* the diagnostic frame (0.14.3): inside the stages, fourteen pieces (ticks, all chunks: 0-13, the chunks shorter than 256 frames again: 14-27),
 * the chunk sizes (28-31: < 64, < 128, < 256, 256 frames) and the work between chunks (32-35: drain, control, the clock pulse, the hits that waited) */
#if EN_DIAG
volatile uint32_t en_diag[40];                   /* (0.17.70: frame 0x5C; 36 fx_control and 37 the master's and the reverb's settings, ticks; 39 control()'s calls) */
#define DT0() do { if (en_prof_clock) dt0_ = en_prof_clock(); } while (0)
#define DT(i) do { if (en_prof_clock) { const uint32_t t1_ = en_prof_clock(); ds_[i] += t1_ - dt0_; dt0_ = t1_; } } while (0)
#define RT(i, stmt) do { if (en_prof_clock) { const uint32_t t0_ = en_prof_clock(); stmt; en_diag[i] += en_prof_clock() - t0_; } else { stmt; } } while (0)
#define DIAG_LOCALS uint32_t dt0_ = pt0_, ds_[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#else
#define DT0() do { } while (0)
#define DT(i) do { } while (0)
#define RT(i, stmt) do { stmt; } while (0)
#define DIAG_LOCALS
#endif
volatile uint32_t en_peak_stage[9], en_peak_total, en_peak_info;   /* the longest chunk since last read: its stages (ticks), its total, and when it was */
#define PT(i) do { if (en_prof_clock) { const uint32_t t1_ = en_prof_clock(); en_prof[i] += t1_ - pt0_; cs_[i] = t1_ - pt0_; pt0_ = t1_; } } while (0)
static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j, pt0_ = en_prof_clock ? en_prof_clock() : 0u, bal_t0 = pt0_, bal_a = 0u, bal_w = 0u, cs_[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    DIAG_LOCALS
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm) * (1.0f + 0.7f * fm_maxf(brk, 0.0f)), eg = echo_send;
    float dk[BLK], de = duck_env, gg = gate_g, rzb[BLK];   /* (rzb: the resonator's input, 0.17.51) */
    const float pre = 2.0f * trim_k;            /* (0.17.42) the master bus's input: +6 dB and the piece's level (en_render takes the 6 dB off after it) */
    const float depth = fm_minf(0.55f * KK->duck, 0.85f) * fm_minf(pulse_vol * 1.5f, 1.0f) * trk_g1[TR_KICK];   /* a muted kick ducks nothing */
    const int gate = fx_on >> FX_GATE16 & 1u ? 1 : fx_on >> FX_GATE8 & 1u ? 2 : 0;
    const int odd = (int)((sixteenth - 1u) & 1u);         /* the sixteenth playing now: the second of an eighth? */
    int rv_c2;
    shed3 = en_shed_level >= 3;
    hb_tick(n);
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = busl[j] = busr[j] = 0.0f;
    {   /* the tank for this chunk starts now (it reads input older than this chunk's), here or on the other core */
        const dl_t *pd = &dl[L_PRE];
        rv_job.n = (int)n;
        rv_job.r0 = pd->i >= rv_pre ? pd->i - rv_pre : pd->i - rv_pre + pd->n;
        rv_job.fm_on = par[P_CM_SND] >= CM_NPLAIN && cm_vol > 0.0f;
        rv_job.fm_w = fm_ctr & (FM_R - 1);
        rv_job.pad_w = pad_ctr & (PAD_R - 1);
        pad_lk = lk_snare;
        rv_job.bass = en_bass_c1;
        bass_duck = duck_env;
        stage_done = 0;
        stage2_done = 0;
        stage3_done = 0;
        rv_c2 = c2_submit && c2_submit(chunk_job, &rv_job);
        if (rv_c2)
            en_c2_jobs++;
        else
            chunk_job(&rv_job);
    }
    PT(0);                                      /* the clear, the job's start */
    DT0();
    if (!rv_job.bass) {                         /* the drone and the acid here (else the other core's: merged after the drums) */
        RENDER_BASS(n);
        DT(0);
        RENDER_ACID(n);
        DT(1);
    }
    PT(1);                                      /* bass, acid */
    DT0();
    mx_begin(3, n);
    rz_in = rzb;
    if (rz_m) {                                  /* (0.17.51) the resonator's input from silence; the plain pluck's share of the mix, if chosen */
        for (j = 0; j < n; j++)
            rz_in[j] = 0.0f;
        if (rz_m & 0x80u)
            rz_sub(n, 1.0f);
    }
    render_cm(n);
    if (rz_m & 0x80u)
        rz_add(n, 1.0f);
    if (jc_plain())
        jc_cm(n);
    DT(2);
    render_fm(n);                               /* the DX voices' samples from a chunk ago (the other core's), into the fm_ buffers added below */
    mx_end(3, n);
    if (bus_m >> TR_CM & 1u)                    /* the plain pluck to the punch-ins' bus: it is all the mix holds so far */
        for (j = 0; j < n; j++) {
            busl[j] = mixl[j];
            busr[j] = mixr[j];
            mixl[j] = mixr[j] = 0.0f;
        }
    DT(3);
    render_dj(n);
    DT(4);
    render_noise(n);
    DT(5);
    PT(2);                                      /* pluck, the DX voices' samples, snips, noise */
    bal_a = pt0_;
    PT(3);                                      /* (the wait for the pad: none since 0.14.4, the pad comes from its ring a chunk late; the column stays) */
    bal_w = pt0_ - bal_a;
    DT0();
    {   /* the pad from a chunk ago (the ring, written by the job), with the DX voices' samples */
        float pl, pr, ps, fl, fr, fs;
        const uint32_t rp = pad_ctr - PAD_D;
        mx_gains(0, &pl, &pr, &ps);
        mx_gains(3, &fl, &fr, &fs);
        {   /* (each into the punch-ins' bus when targeted: chosen once a chunk, 0.15.20, not tested a sample) */
            float *const pal = bus_m >> TR_PAD & 1u ? busl : mixl, *const par_ = bus_m >> TR_PAD & 1u ? busr : mixr;
            float *const fal = bus_m >> TR_CM & 1u ? busl : mixl, *const far_ = bus_m >> TR_CM & 1u ? busr : mixr;
        for (j = 0; j < n; j++) {
            const uint32_t q = (rp + j) & (PAD_R - 1);
            const float ds = pad_ring[2][q] * ps;
            pal[j] += pad_ring[0][q] * pl;
            par_[j] += pad_ring[1][q] * pr;
            fal[j] += fm_ol[j] * fl;
            far_[j] += fm_or[j] * fr;
            if (rz_m & 0x80u)                    /* (0.17.51) the DX pluck into the resonator */
                rz_in[j] += fm_ol[j] * fl + fm_or[j] * fr;
            rsend[j] += ds * 0.6f + fm_rs[j] * fs;
            esend[j] += ds * 0.2f + fm_es[j] * fs;
        }
        }
        pad_ctr += n;
    }
    pad_midi_flush();
    DT(6);
    for (j = 0; j < n; j++) {                   /* the sidechain: the kick pushes the rest down, briefly */
        float gt = 1.0f;
        if (gate == 1)                          /* the gate: the first half of each sixteenth */
            gt = (float)(step_t + (int32_t)j) < 0.5f * step_len ? 1.0f : 0.0f;
        else if (gate == 2)                     /* or of each eighth */
            gt = odd ? 0.0f : 1.0f;

        gg += (gt - gg) * 0.04f;
        gg_v[j] = gg;
        dk[j] = 1.0f - depth * de;
        de *= duck_dec;
        mixl[j] *= dk[j] * gg;
        mixr[j] *= dk[j] * gg;
        rsend[j] *= gg;
        esend[j] *= gg;
        {   /* the noise: its own pump, from none to deep */
            float pk = 1.0f - nz_pump * 0.95f * de * trk_g1[TR_KICK];
            mixl[j] += nzl[j] * pk;
            mixr[j] += nzr[j] * pk;
            rsend[j] += (nzl[j] + nzr[j]) * pk * 0.15f;
        }
    }
    if (bus_m)                                  /* the bus pumps and gates as the mix does (only while it is in use, 0.15.20) */
        for (j = 0; j < n; j++) {
            busl[j] *= dk[j] * gg_v[j];
            busr[j] *= dk[j] * gg_v[j];
        }
    gate_g = gg;
    DT(7);
    PT(4);                                      /* the merge, the sidechain */
    DT0();
    mx_begin(4, n);
    if (KK->crush > 0.0f) {                      /* breakcore: the drums crushed (what they add, held and stepped) */
        static float hl, hr;
        static uint32_t hc;
        const uint32_t hold = 2u + (uint32_t)(KK->crush * 4.0f);
        const float c = KK->crush, q = 24.0f;
        float *const bl = dsn_l, *const br = dsn_r;   /* (the mix before the drums: what they add is crushed) */
        for (j = 0; j < n; j++) {
            bl[j] = mixl[j];
            br[j] = mixr[j];
        }
        render_drums(n);
        for (j = 0; j < n; j++) {
            float dl = mixl[j] - bl[j], dr = mixr[j] - br[j];
            if (++hc >= hold) {
                hc = 0;
                hl = (float)(int32_t)(dl * q + (dl < 0.0f ? -0.5f : 0.5f)) * (1.0f / q);
                hr = (float)(int32_t)(dr * q + (dr < 0.0f ? -0.5f : 0.5f)) * (1.0f / q);
            }
            mixl[j] = bl[j] + dl + (hl - dl) * c;
            mixr[j] = br[j] + dr + (hr - dr) * c;
        }
    } else {
        if (bus_m & 0x0Fu)                      /* (the drums to the bus: the mix before them, to take out what they add) */
            for (j = 0; j < n; j++) {
                dsn_l[j] = mixl[j];
                dsn_r[j] = mixr[j];
            }
        render_drums(n);                        /* (the kick sets duck_env for the next chunk) */
    }
    mx_end(4, n);
    if (bus_m & 0x0Fu)                          /* the drums to the punch-ins' bus: what they added (crushed, through the mixer), out of the mix */
        for (j = 0; j < n; j++) {
            busl[j] += mixl[j] - dsn_l[j];
            busr[j] += mixr[j] - dsn_r[j];
            mixl[j] = dsn_l[j];
            mixr[j] = dsn_r[j];
        }
    DT(8);
    PT(5);                                      /* the drums */
    if (rv_c2 && rv_job.bass) {                 /* the drone and the acid are the other core's: wait for them (the reverb goes on behind) */
        while (!stage2_done)
            if (c2_alive && !c2_alive()) {
                if (c2_join())                  /* it finished just as the time ran out: not lost */
                    continue;
                rv_c2 = 0;                      /* (the join retired it: before the drone and the acid it was writing are read) */
                en_c2_lost++;
                pad_ring_clear(rv_job.pad_w, n);   /* (the pad it was writing for the next chunk) */
                for (j = 0; j < n; j++)
                    bass_o[j] = acid_o[j] = rv_wl[j] = rv_wr[j] = 0.0f;
                plate_init();
                fm_reset();
                for (j = 0; j < FM_R; j++)
                    fm_fifo[j] = 0.0f;
                break;
            }
        if (c2_fence)
            c2_fence();
    }
    DT0();
    {   /* the drone and the acid into the mix as they were before the sidechain: ducked by the kick and gated, their sends gated, the mixer's pan and sends on each */
        float bl, br, bs, al, ar, as;
        mx_gains(1, &bl, &br, &bs);
        mx_gains(2, &al, &ar, &as);
        for (j = 0; j < n; j++) {
            const float b = bass_o[j], a = acid_o[j], dg = dk[j] * gg_v[j];
            float *const tbl = bus_m >> TR_DRONE & 1u ? busl : mixl, *const tbr = bus_m >> TR_DRONE & 1u ? busr : mixr;   /* (the punch-ins' bus when targeted) */
            float *const tal = bus_m >> TR_ACID & 1u ? busl : mixl, *const tar = bus_m >> TR_ACID & 1u ? busr : mixr;
            tbl[j] += b * bl * dg;
            tbr[j] += b * br * dg;
            if (jc_ac) {                        /* (0.17.44) the CHORUS on the acid: its own line, the sides swept apart */
                const float p = jc_ph + (float)j * jc_rate, t = jc_lfo(p), sw = jc_am * jc_tri(p + 0.25f);
                ja_hp += 0.0281f * (a - ja_hp);
                ja_buf[ja_w] = jc_q(a - ja_hp);
                tal[j] += (a + JC_WET * ja_tap(jc_c + jc_d * t)) * JC_NORM * (1.0f + sw) * al * dg;
                tar[j] += (a + JC_WET * ja_tap(jc_c - jc_d * t)) * JC_NORM * (1.0f - sw) * ar * dg;
                ja_w = (ja_w + 1u) & (JA_N - 1u);
            } else {
                tal[j] += a * al * dg;
                tar[j] += a * ar * dg;
            }
            rsend[j] += (b * 0.06f * bs + a * 0.15f * as) * gg_v[j];
            esend[j] += (b * 0.04f * bs + a * 0.35f * as) * gg_v[j];
        }
    }
    if (jc_pl || jc_ac) {                       /* the chorus's sweep on, a chunk on */
        jc_ph += (float)n * jc_rate;
        jc_ph -= (float)(int)jc_ph;
    }
    duck_env = fm_flush(de);
    DT(9);
    if (bus_m) {                                /* the punch-ins' bus: the filter, the phaser, then back into the mix */
        const float pk1 = (float)fxs_v[EN_FXS_TP_MIX] * 0.005f;   /* the phaser's wet share, 0 .. 0.5 */
        float mw0[3] = {0.0f, 0.0f, 0.0f}, mwd[3] = {0.0f, 0.0f, 0.0f};
        const int morph = tfx_m > 0.0f && tf_type == TF_MORPH;
        if (morph) {                            /* (0.17.72) MORPH: equal-power between low and band, band and high; the band lifted by 1 + 0.85/k, its
                                                 * level against the others' at the same resonance (pink noise, the host: within 4 dB end to end, no dip) */
            const float gb = 1.0f + 0.85f / tf_c[3], inv = 1.0f / (float)n;   /* (ChatGPT's check, loud acid alone and TRANCE 01: held at BP it peaks
                                                 * 3.5-4 dB under the plain low-pass and never moves the limiter, at reso 60 and 90; half the lift only weaker) */
            float w1[3], a;
            int q;
            const int first = tf_m < 0.0f;
            tf_m = first ? tf_mt : tf_m + (tf_mt - tf_m) * 0.25f;   /* (about 20 ms to its place) */
            a = tf_m < 0.5f ? 2.0f * tf_m : 2.0f * tf_m - 1.0f;
            {
                const float cw = fm_sin_turns(0.25f - 0.25f * a), sw = fm_sin_turns(0.25f * a);
                w1[0] = tf_m < 0.5f ? cw : 0.0f;
                w1[1] = gb * (tf_m < 0.5f ? sw : cw);
                w1[2] = tf_m < 0.5f ? 0.0f : sw;
            }
            for (q = 0; q < 3; q++) {
                mw0[q] = first ? w1[q] : tf_w[q];
                mwd[q] = (w1[q] - mw0[q]) * inv;
                tf_w[q] = w1[q];
            }
        }
        for (j = 0; j < n; j++) {
            float x[2];
            int c;
            x[0] = busl[j];
            x[1] = busr[j];
            for (c = 0; c < 2; c++) {
                if (morph)
                    x[c] += (svf_morph(x[c], tf_c, &tf_s[2 * c], mw0[0] + mwd[0] * (float)j, mw0[1] + mwd[1] * (float)j, mw0[2] + mwd[2] * (float)j) - x[c]) * tfx_m;
                else if (tfx_m > 0.0f)
                    x[c] += (svf_mm(x[c], tf_c, &tf_s[2 * c], tf_type) - x[c]) * tfx_m;
                if (tpx_m > 0.0f) {             /* four all-passes, a little feedback, mixed with the dry: as the master's phaser */
                    float y = x[c] + 0.5f * tp_fb[c], v;
                    const float a = tp_a[c];
                    v = a * y + tp_z[c][0];  tp_z[c][0] = y - a * v;  y = v;
                    v = a * y + tp_z[c][1];  tp_z[c][1] = y - a * v;  y = v;
                    v = a * y + tp_z[c][2];  tp_z[c][2] = y - a * v;  y = v;
                    v = a * y + tp_z[c][3];  tp_z[c][3] = y - a * v;  y = v;
                    tp_fb[c] = fm_flush(y);
                    x[c] += pk1 * (y - x[c]) * tpx_m;   /* (Mix 100: half and half, the deepest notches) */
                }
            }
            busl[j] = x[0];
            busr[j] = x[1];
            mixl[j] += x[0];
            mixr[j] += x[1];
        }
        for (j = 0; j < 4; j++)
            tf_s[j] = fm_flush(tf_s[j]), tp_z[0][j] = fm_flush(tp_z[0][j]), tp_z[1][j] = fm_flush(tp_z[1][j]);   /* (0.17.44: the phaser's too: they held a subnormal for good once the bus went quiet) */
    }
    {
    const float thr = throw_amt * 0.5f, tdg = tdx_m * (float)fxs_v[EN_FXS_TD_SEND] * 0.006f;   /* (the bus is cleared each chunk, so 0 when unused) */
    static const uint8_t RD_S[6] = {1, 2, 3, 4, 8, 16};
    const int rdm = fxs_v[EN_FXS_RD_MODE];      /* (0.17.72) the returns' duck: the kit's (dk, as the mix), none, or its own depth and release */
    const float rdep = rdm == 2 ? fm_minf(0.0085f * (float)fxs_v[EN_FXS_RD_DEPTH], 0.85f) * fm_minf(pulse_vol * 1.5f, 1.0f) * trk_g1[TR_KICK] : 0.0f,
                rdd = rdm == 2 ? fm_expf(-4.0f / (step_len * (float)RD_S[(unsigned)fxs_v[EN_FXS_RD_REL] < 6u ? fxs_v[EN_FXS_RD_REL] : 2])) : 1.0f;
    float re = rd_env;
    for (j = 0; j < n; j++) {                   /* the echoes; the reverb's input into its line; the dry part of the mix */
        float el, er, xl, xr, rk;
        echoes(esend[j] * eg + thr * (mixl[j] + mixr[j]) + tdg * (busl[j] + busr[j]), &el, &er, &xl, &xr);
        push(&dl[L_PRE], (rsend[j] * rg + (el + er + xl + xr) * 0.35f * rev_send + wash_e * 0.2f * (mixl[j] + mixr[j])) * 0.8f);
        if (rdm == 2) {
            rk = 1.0f - rdep * re;
            re *= rdd;
        } else {
            rk = rdm == 1 ? 1.0f : dk[j];
        }
        mixl[j] = (mixl[j] + (0.75f * el + 0.15f * xl) * rk) * 0.36f;
        mixr[j] = (mixr[j] + (0.75f * er + 0.15f * xr) * rk) * 0.36f;
        dk[j] = rk * (1.3f * 0.36f);            /* what the tank's output is scaled by */
    }
    rd_env = fm_flush(rdm == 2 ? re : duck_env);
    }
    DT(10);
    PT(6);                                      /* the echoes, the reverb's input */
    bal_a = pt0_;
    if (rv_c2) {                                /* the tank's output is the other core's: wait for that only (its DX voices go on behind, joined after the master) */
        while (!stage3_done)
            if (c2_alive && !c2_alive()) {
                if (c2_join())                  /* it finished just as the time ran out: not lost */
                    continue;
                rv_c2 = 0;                      /* (the join retired it: this chunk has no reverb, and the tank starts over) */
                en_c2_lost++;
                pad_ring_clear(rv_job.pad_w, n);
                for (j = 0; j < n; j++)
                    rv_wl[j] = rv_wr[j] = 0.0f;
                plate_init();
                fm_reset();
                for (j = 0; j < FM_R; j++)
                    fm_fifo[j] = 0.0f;
                break;
            }
        if (c2_fence)
            c2_fence();
    }
    PT(7);                                      /* the wait for the reverb */
    bal_w += pt0_ - bal_a;
    DT0();
    for (j = 0; j < n; j++) {                   /* the tank's output in, the performance effects */
        float l = mixl[j] + dk[j] * rv_wl[j], r = mixr[j] + dk[j] * rv_wr[j];
        fx_master(&l, &r);
        if (po_on)
            polish(&l, &r);                     /* (0.17.30) */
        mixl[j] = l * pre;
        mixr[j] = r * pre;
    }
    DT(11);
    master_dyn_block(mixl, mixr, n);            /* the compressor and the limiter */
    DT(12);
    for (j = 0; j < n; j++) {                   /* the master gain and the soft clip */
        if (shed3) {                            /* the cheap clip (a rational tanh) */
            out[2 * j] = (int32_t)(soft_sat(mixl[j] * gain) * 8300000.0f);
            out[2 * j + 1] = (int32_t)(soft_sat(mixr[j] * gain) * 8300000.0f);
        } else {
            out[2 * j] = (int32_t)(fm_tanhf(mixl[j] * gain) * 8300000.0f);
            out[2 * j + 1] = (int32_t)(fm_tanhf(mixr[j] * gain) * 8300000.0f);
        }
    }
    if (usb_cur)                                /* the USB audio at its fixed level (the Settings' USB page), only while a computer records: the same clip, unity gain,
                                                 * at the scale the USB tap makes of the 24-bit output (8300000 / 256) */
        for (j = 0; j < n; j++) {
            usb_cur[2 * j] = (int16_t)(fm_tanhf(mixl[j] * usb_gain) * 32421.0f);
            usb_cur[2 * j + 1] = (int16_t)(fm_tanhf(mixr[j] * usb_gain) * 32421.0f);
        }
    DT(13);
    PT(8);                                      /* the master: effects, dynamics, the soft clip */
    if (rv_c2) {                                /* the other core's DX voices (they run behind the master): done before anything touches their state */
        bal_a = pt0_;
        if (!c2_join()) {                       /* lost: its voices were its own */
            en_c2_lost++;
            pad_ring_clear(rv_job.pad_w, n);
            fm_reset();
            for (j = 0; j < FM_R; j++)
                fm_fifo[j] = 0.0f;
            plate_init();
        }
        if (en_prof_clock) {
            const uint32_t t1_ = en_prof_clock();
            en_prof[7] += t1_ - pt0_;
            cs_[7] += t1_ - pt0_;
            pt0_ = t1_;
        }
        bal_w += pt0_ - bal_a;
    }
    en_prof[9]++;
    if (en_prof_clock && n < (uint32_t)BLK) {   /* the short chunks' stages apart: against the full chunks' they give each stage's fixed cost a chunk (tools/fm1sum.py) */
        uint32_t k;
        for (k = 0; k < 9u; k++)
            en_prof_sm[k] += cs_[k];
        en_prof_sm[9]++;
        en_prof_sm[10] += n;
    }
#if EN_DIAG
    if (en_prof_clock) {                        /* the diagnostic frame's pieces */
        uint32_t k;
        for (k = 0; k < 14u; k++) {
            en_diag[k] += ds_[k];
            if (n < (uint32_t)BLK)
                en_diag[14 + k] += ds_[k];
        }
        en_diag[28 + (n < 64u ? 0u : n < 128u ? 1u : n < (uint32_t)BLK ? 2u : 3u)]++;
    }
#endif
    if (en_prof_clock && pt0_ - bal_t0 > en_peak_total) {   /* the slowest chunk: which stage, which step, how long it was, what was moved */
        uint32_t k;
        for (k = 0; k < 9u; k++)
            en_peak_stage[k] = cs_[k];
        en_peak_total = pt0_ - bal_t0;
        en_peak_info = (uint32_t)((sixteenth - 1u) & 15u) | (n & 0xFFu) << 4 | (step_t < 64 ? 1u << 12 : 0u) | (uint32_t)(rv_job.bass != 0) << 13 |
                       (uint32_t)(en_chorus_c0 != 0) << 14 | (uint32_t)(en_shed_level & 3) << 15 | (uint32_t)(sec & 3) << 17;
    }
    if (bal_force >= 0) {
        bal_apply(bal_force);
    } else if (en_prof_clock && rv_c2) {        /* where the work runs, from how long the first core waited for the other (a quarter second's, summed) */
        if (bal_lvl == 2 && bal_w > (pt0_ - bal_t0) / 4u) {   /* (0.17.58) the other core cannot keep up (the first waited over a quarter of this chunk): back at once.
                                                                * The maker's 03:16 log (0.17.56): every one of its 36 xruns came in the quarter second
                                                                * the drone and the acid were moved (TRANCE 01: the first core waiting 2.3-2.5 ms a chunk) */
            bal_apply(1);
            bal_ph = bal_hold = bal_sum = bal_k = 0;
            bal_tot = bal_wait = bal_fr = 0;
            bal_hmax = bal_hmax < 24u ? bal_hmax * 2u : 48u;   /* (the next try twice as far off: 10 s, 20, 40 .. 2 minutes) */
        }
        bal_tot += pt0_ - bal_t0;
        bal_wait += bal_w;
        bal_fr += n;
        if (bal_tot >= 6000000u) {
            /* (0.17.57) A/B, measured: the first core's time a frame, waiting for the other included, with the drone and the acid here (level 1) and
             * there (level 2); the cheaper is kept and tried again every ten seconds. Before, they moved only when the first core never waited (under
             * 1 %) and, back soon, were blocked for 10 s, then 20, up to a minute, for good: the maker's log through 42 pieces had them moved in 5 of
             * them, the first core at 67-90 % and the other at 24-55 %; FOREST 10, moved all along: 69 % / 41 %, no late render. Nothing is shed. */
            const uint32_t cost = bal_tot / (bal_fr ? bal_fr : 1u);
            bal_sum += cost;
            if (++bal_k >= 8u) {                /* two seconds measured at this level */
                const uint32_t c = bal_sum / 8u;
                bal_sum = bal_k = 0;
                if (bal_ph == 0) {              /* holding: after ten seconds, try the other */
                    if (++bal_hold >= bal_hmax) {
                        bal_hold = 0;
                        bal_c[bal_lvl - 1u] = c;
                        bal_apply(bal_lvl == 1 ? 2 : 1);
                        bal_ph = 1;
                    }
                } else {                        /* tried: keep it if it is cheaper by 2 %, else back */
                    const uint32_t was = bal_c[bal_lvl == 1 ? 1 : 0];
                    bal_ph = 0;
                    if (!(c * 50u < was * 49u))
                        bal_apply(bal_lvl == 1 ? 2 : 1);
                    else if (bal_lvl == 2)
                        bal_hmax = 4u;              /* (kept there: the next try soon again) */
                }
            }
            bal_tot = bal_wait = bal_fr = 0;
            (void)bal_dwell, (void)bal_block, (void)bal_up_t, (void)bal_back;
        }
    }
    step_t += (int32_t)n;
    en_step_frac = step_len > 1.0f ? fm_minf((float)step_t / step_len, 0.999f) : 0.0f;   /* (0.17.48) for the UI's keys */
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;
static void arc_restart(void)                   /* (0.17.11, track codes) the slow things' clocks from now: the arc from the middle, the modulators' and the tape
                                                 * head's next turns at once (they draw on the shared stream: a track's draws in the same order every start) */
{
    int i;
    arc = arc_tgt = arcm = 0.0f;
    arc_left = 0;
    head_left = 0;
    for (i = 0; i < NMOD; i++)
        mod[i].left = 0;
}

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    heads_step(n);
    RT(36, fx_control(n));
#if EN_DIAG
    en_diag[39]++;
#endif
    if (par[P_CM_SND] >= CM_NPLAIN)             /* the DX voices: the patch, the tune, the keys' time */
        fm_set_patch(par[P_CM_SND] - CM_NPLAIN);
    fm_tune = par[P_TUNE] * 13981;
    {   /* how long a DX note may ring on after its key is up: 0.35 s .. 2.5 s by the pluck's Decay (a bell's tail would go on for many) */
        const float d = (float)par[P_CM_DEC] * 0.01f;
        fm_gates(n, (int32_t)((0.35f + 2.2f * d * d) * EN_SR));
    }
    RT(37, { master_dyn_set(); rz_set(); reverb_set(); });
    en_lim_gr = 8.6859f * gr_hold;               /* 20 log10 of 1/(1 - x), near enough for the small ones: the limiter's reduction in dB */
    en_out_peak = pk_hold;
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((50.0f + 100.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (35.0f * EN_SR));
    arcm = arc * motion;
    en_arc = arcm;
    {   /* the sections, smoothed: a breakdown falls in over ~2 s, a build rises through its four bars */
        float bt = sec == SEC_BREAK ? 1.0f : sec == SEC_BUILD ? -0.6f : 0.0f;
        brk += (bt - brk) * fm_minf((float)n / ((sec == SEC_BUILD ? 4.0f : 2.0f) * EN_SR), 1.0f);
    }
    pl_decay += ((en_frozen ? 0.9995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    if (en_playing && !en_frozen) {
        const int hr = hr_bars();
        if (!change_pending || hr) {
            drift_left -= (float)n;
            if (drift_left <= 0.0f) {
                if (hr)
                    drift_left = 0.0f;          /* a harmonic rhythm: the bar line changes the chord */
                else
                    change_pending = 1;
            }
        }
    }
    en_drift_bars = drift_total > 0.0f && step_len > 1.0f ? fm_minf(drift_left / (step_len * 16.0f), 99.0f) : 99.0f;
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    en_acid_glow *= 0.93f;
    {   /* the links' envelopes: the amount sets how far and how long (kick 40 .. 160 ms, snare 80 .. 280 ms) */
        float ak = (float)par[P_LK_KICK] * 0.01f, as = (float)par[P_LK_SNARE] * 0.01f;
        lk_kick *= fm_expf(-(float)n / ((0.04f + 0.12f * ak) * EN_SR));
        lk_snare *= fm_expf(-(float)n / ((0.08f + 0.2f * as) * EN_SR));
    }
    if (!links_on())
        lk_kick = lk_snare = 0.0f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
    {   /* the LFOs, and where the pad's goes */
        float d = (float)par[P_PL_DEPTH] * 0.01f;
        lfo_step(&pad_lfo, par[P_PL_RATE], par[P_PL_SHAPE], (float)n, step_len);
        lfo_step(&drone_lfo, wub_on() ? wub_rate() : par[P_DL_RATE], par[P_DL_SHAPE], (float)n, step_len);
        pad_filt = par[P_PL_DEST] == 0 ? d * pad_lfo.v : 0.0f;
        pad_lvl = par[P_PL_DEST] == 1 ? 1.0f - 0.85f * d * (0.5f - 0.5f * pad_lfo.v) : 1.0f;
        pad_pan = par[P_PL_DEST] == 2 ? 0.9f * d * pad_lfo.v : 0.0f;
        pad_pitch = par[P_PL_DEST] == 3 ? 0.5f * d * pad_lfo.v : 0.0f;
        {   /* (0.16.4) either LFO on another voice or an effect: the acid's cutoff (2.5 octaves either way) and resonance, the tape echo's time (a wobble,
             * 6 % at full: a dub siren), the punch-in filter's cutoff (3 octaves either way: a wah on the tracks it takes) */
            const float e = (float)par[P_DL_DEPTH] * 0.01f, pv = pad_lfo.v, dv = drone_lfo.v;
            const int pd = par[P_PL_DEST], dd = par[P_DL_DEST];
            lfo_acut = 2.5f * ((pd == 4 ? d * pv : 0.0f) + (dd == 5 ? e * dv : 0.0f));
            lfo_ares = fm_minf((pd == 5 ? d * (0.5f + 0.5f * pv) : 0.0f) + (dd == 6 ? e * (0.5f + 0.5f * dv) : 0.0f), 1.0f);
            lfo_echo = 0.06f * ((pd == 6 ? d * pv : 0.0f) + (dd == 7 ? e * dv : 0.0f));
            lfo_tcut = pd == 7 ? 3.0f * d * pv : 0.0f;
        }
    }
    {   /* the drone's envelope: attack (a line), then decay */
        if (bass.env_up) {
            bass.env += (float)n / (de_atk_s(par[P_DE_ATK]) * EN_SR);
            if (bass.env >= 1.0f) {
                bass.env = 1.0f;
                bass.env_up = 0;
            }
        } else {
            bass.env *= fm_expf(-6.9f * (float)n / (de_dec_s(par[P_DE_DEC]) * EN_SR));
        }
    }
}

static uint32_t ctl_acc;                        /* frames since control() last ran */
void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    float gain;
    int32_t *const out0 = out;
    const uint32_t n0 = n;
    RT(32, drain());
    gain = muted ? 0.0f : (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    usb_gain = muted ? 0.0f : 1.0f;             /* the USB audio at a fixed level (en_usb_out): unity, the knob at about three quarters */
    trim_k += (trim_to - trim_k) * 0.15f;       /* the piece's level (en_trim), over about 40 ms: now before the compressor (render_chunk's pre) */
    gain *= 0.5f;                               /* (0.17.42) the bus's +6 dB taken off again after it: the same level out, the dynamics working at the mix's real level */
    usb_gain *= 0.5f;
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {                             /* (0.17.62) only the step's pulse cuts the chunk (its notes land on their sample). The five between are
                                                 * the MIDI clock out alone, so one goes at a chunk edge: at a chunk's start if due within EN_MIN_CHUNK of it, else at the next chunk's start, at most EN_MIN_CHUNK early or late
                                                 * (2.9 ms; the grid does not drift, and a follower steps on the step's pulse, which stays exact).
                                                 * The maker's TRANCE 01 logs: half the chunks were cut short, mostly by these pulses, and every chunk
                                                 * costs the first core 0.8-1.3 ms whatever its length. */
            if (pulse_left <= 0.0f || (pulse && pulse_left < (float)EN_MIN_CHUNK)) {
                RT(34, clock_pulse());
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if (!pulse && (float)k > pulse_left) {
                k = (uint32_t)pulse_left + 1u;   /* (0.17.64-0.17.69 diag builds counted these in en_diag[36]) */
            }
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample, unless that would cut the chunk shorter than EN_MIN_CHUNK: then it
                                                 * sounds at that mark, up to 2.9 ms late. Every chunk costs the device a fixed share whatever its length
                                                 * (0.14.1's log: a swung step whose humanized hats, perc, pluck, acid and bass each cut their own chunk
                                                 * made halves of 7-9 chunks and 12-13.7 ms, their stages 10-11 ms against 7 for the usual two or three),
                                                 * so a 256-frame render is now at most three chunks for the hits. The clock pulse still cuts where it
                                                 * falls: the grid and the MIDI clock out keep their timing; only hits that swing or humanize (6-60 ms
                                                 * off the grid by design) may come the little later. */
            if (w > 0 && (uint32_t)w < k) {
                k = (uint32_t)w >= EN_MIN_CHUNK ? (uint32_t)w : k < EN_MIN_CHUNK ? k : EN_MIN_CHUNK;
            }
        }
        if (timed)
            pulse_left -= (float)k;
        ctl_acc += k;
        if (ctl_acc >= 128u) {                  /* the slow things once per 128 frames at most (0.14.4): a short chunk, cut for a swung hit, skips them. On the
                                                 * device control() cost 210-260 us a call whatever the chunk's length (a dozen functions fetched cold); the
                                                 * things it steps take the frames since it last ran, so nothing drifts */
            RT(33, control(ctl_acc));
            ctl_acc = 0;
        } else {
            int t;
            tg_inv = 1.0f / (float)k;           /* the tracks' gains hold at their targets through the skipped chunk; the ramp goes on at the next control */
            for (t = 0; t < EN_NTRACK; t++)
                trk_g[t] = trk_g1[t];
        }
        usb_cur = en_usb_out ? en_usb_out + (out - out0) : 0;
        render_chunk(out, k, gain);
        RT(35, drums_wait_step((int32_t)k));
        out += 2 * k;
        n -= k;
    }
#if EN_DIAG
    en_diag[38]++;                              /* (0.17.64) renders (0.17.70: 39 is control()'s calls, no longer those of three chunks or more) */
#endif
    if (gap_on || gap_g < 1.0f) {               /* the drop gap: out in ~6 ms, back in ~1 ms (the drop's first hit whole) */
        uint32_t j;
        for (j = 0; j < n0; j++) {
            gap_g = gap_on ? fm_maxf(gap_g - 1.0f / (0.006f * EN_SR), 0.0f) : fm_minf(gap_g + 1.0f / (0.001f * EN_SR), 1.0f);
            out0[2 * j] = (int32_t)((float)out0[2 * j] * gap_g);
            out0[2 * j + 1] = (int32_t)((float)out0[2 * j + 1] * gap_g);
            if (en_usb_out) {
                en_usb_out[2 * j] = (int16_t)((float)en_usb_out[2 * j] * gap_g);
                en_usb_out[2 * j + 1] = (int16_t)((float)en_usb_out[2 * j + 1] * gap_g);
            }
        }
    }
    if (hush || hush_g < 1.0f) {                /* the hush: ~6 ms each way */
        const float step = 1.0f / (0.006f * EN_SR), tgt = hush ? 0.0f : 1.0f;
        uint32_t j;
        for (j = 0; j < n0; j++) {
            hush_g = tgt > hush_g ? fm_minf(hush_g + step, 1.0f) : fm_maxf(hush_g - step, 0.0f);
            out0[2 * j] = (int32_t)((float)out0[2 * j] * hush_g);
            out0[2 * j + 1] = (int32_t)((float)out0[2 * j + 1] * hush_g);
            if (en_usb_out) {
                en_usb_out[2 * j] = (int16_t)((float)en_usb_out[2 * j] * hush_g);
                en_usb_out[2 * j + 1] = (int16_t)((float)en_usb_out[2 * j + 1] * hush_g);
            }
        }
    }
    en_hushed = (uint8_t)(hush && hush_g <= 0.0f);
}
