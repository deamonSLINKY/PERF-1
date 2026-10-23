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
                                             "Pent maj", "Pent min"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10)};
#undef B
const char *const EN_LANE_NAME[EN_NLANE] = {"Kick", "Snare", "Clap", "Hat", "Open hat", "Perc"};

static const en_param_t PARAMS[P_NPARAMS] = {
    [P_PAD] = {"Pad", 0, 100, 45},          [P_ACID] = {"Acid", 0, 100, 55},       [P_BASS] = {"Drone", 0, 100, 65},
    [P_PULSE] = {"Drums", 0, 100, 75},      [P_NOISE] = {"Vinyl", 0, 100, 20},
    [P_TEMPO] = {"Tempo", 70, 190, 128},   [P_EVOLVE] = {"Evolve", 0, 100, 40},   [P_MOTION] = {"Motion", 0, 100, 60},
    [P_DRIFT] = {"Drift", 0, 100, 60},
    [P_BRIGHT] = {"Bright", 0, 100, 35},    [P_PADTONE] = {"Sound", 0, EN_NSOUND - 1, 1},     [P_DETUNE] = {"Detune", 0, 100, 30},
    [P_PE_ATK] = {"Attack", 0, 100, 45},    [P_PE_HOLD] = {"Hold", 0, 100, 35},    [P_PE_REL] = {"Release", 0, 100, 55},
    [P_PE_REST] = {"Rest", 0, 100, 20},
    [P_PL_RATE] = {"Rate", 0, 7, 4},        [P_PL_DEPTH] = {"Depth", 0, 100, 25},  [P_PL_SHAPE] = {"Shape", 0, 5, 0},
    [P_PL_DEST] = {"Target", 0, 3, 0},
    [P_NZ_COLOR] = {"Wear", 0, 7, 1},     [P_NZ_TONE] = {"Tone", 0, 100, 50},    [P_NZ_PUMP] = {"Pump", 0, 100, 60},
    [P_BASSOCT] = {"Octave", -3, 3, 0},     [P_DR_CUT] = {"Cutoff", 0, 100, 45},   [P_DR_RES] = {"Reso", 0, 100, 45},
    [P_DE_TRIG] = {"Trigger", 0, 5, 0},     [P_DE_ATK] = {"Attack", 0, 100, 25},   [P_DE_DEC] = {"Decay", 0, 100, 55},
    [P_DE_AMT] = {"Amount", 0, 100, 35},
    [P_DL_RATE] = {"Rate", 0, 7, 6},        [P_DL_DEPTH] = {"Depth", 0, 100, 55},  [P_DL_SHAPE] = {"Shape", 0, 5, 0},
    [P_DL_DEST] = {"Target", 0, 3, 0},
    [P_AC_CUT] = {"Cutoff", 0, 100, 35},    [P_AC_RES] = {"Reso", 0, 100, 70},     [P_AC_ENV] = {"Env mod", 0, 100, 55},
    [P_AC_DEC] = {"Decay", 0, 100, 40},     [P_AC_ACC] = {"Accent", 0, 100, 50},   [P_AC_SLIDE] = {"Slide", 0, 100, 30},
    [P_AC_WAVE] = {"Sound", 0, EN_NACID - 1, 0},
    [P_AC_DENS] = {"Density", 0, 100, 60},  [P_AC_LEN] = {"Length", 1, 16, 16},    [P_AC_RANGE] = {"Range", 1, 3, 1},
    [P_AC_MUT] = {"Mutate", 0, 100, 25},    [P_AC_OCT] = {"Acid oct", -3, 3, 0},
    [P_DRIVE] = {"Drive", 0, 100, 40},      [P_SWING] = {"Swing", 0, 100, 0},      [P_PATTERN] = {"Pattern", 0, EN_NPAT - 1, 0},
    [P_LANE_LVL(0)] = {"Level", 0, 100, 85}, [P_LANE_TUNE(0)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(0)] = {"Decay", 0, 100, 50},
    [P_LANE_LVL(1)] = {"Level", 0, 100, 60}, [P_LANE_TUNE(1)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(1)] = {"Decay", 0, 100, 45},
    [P_LANE_LVL(2)] = {"Level", 0, 100, 65}, [P_LANE_TUNE(2)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(2)] = {"Decay", 0, 100, 45},
    [P_LANE_LVL(3)] = {"Level", 0, 100, 55}, [P_LANE_TUNE(3)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(3)] = {"Decay", 0, 100, 35},
    [P_LANE_LVL(4)] = {"Level", 0, 100, 50}, [P_LANE_TUNE(4)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(4)] = {"Decay", 0, 100, 50},
    [P_LANE_LVL(5)] = {"Level", 0, 100, 50}, [P_LANE_TUNE(5)] = {"Tune", -50, 50, 0}, [P_LANE_DEC(5)] = {"Decay", 0, 100, 40},
    [P_ECHO] = {"Echo", 0, 100, 45},        [P_ETIME] = {"Echo time", 0, 5, 1},    [P_REVERB] = {"Reverb", 0, 100, 40},
    [P_SIZE] = {"Size", 0, 100, 70},
    [P_ROOT] = {"Root", 0, 11, 9},          [P_MODE] = {"Mode", 0, EN_NMODES - 1, 5}, [P_TUNE] = {"Tune", -50, 50, 0},
    [P_MIDI] = {"MIDI out", 0, 1, 1},       [P_THEME] = {"Theme", 0, EN_NTHEME - 1, 0},
    [P_STYLE] = {"Style", 0, EN_NSTYLE - 1, 0},
    [P_LINKS] = {"Links", 0, 1, 0},         [P_LK_KICK] = {"Kick", 0, 100, 50},    [P_LK_SNARE] = {"Snare", 0, 100, 40},
    [P_LK_HAT] = {"Hats", 0, 100, 50},      [P_LK_PERC] = {"Perc", 0, 1, 0},
    [P_OCT] = {"Song oct", -3, 3, 0},       [P_PAD_OCT] = {"Pad oct", -3, 3, 0},
    [P_DR_INST] = {"Sound", 0, EN_NSOUND - 1, 0},
    [P_KIT_KICK] = {"Kick", 0, EN_NKIT - 1, 0}, [P_KIT_SNARE] = {"Snare", 0, EN_NKIT - 1, 0},
    [P_KIT_HAT] = {"Hats", 0, EN_NKIT - 1, 0},  [P_KIT_PERC] = {"Perc", 0, EN_NKIT - 1, 0},
    [P_PAD_MAN] = {"Pad hand", 0, 1, 0},     [P_DR_MAN] = {"Drone hand", 0, 1, 0}, [P_AC_MAN] = {"Acid hand", 0, 1, 0},
    [P_CHORD] = {"Chord", 0, EN_NCHORD - 1, 0}, [P_VOICING] = {"Voicing", 0, 1, 0},
    [P_DD_CHANGE] = {"Change", 0, 2, 0}, [P_DD_EVERY] = {"Every", 0, 5, 2}, [P_DD_POOL] = {"Pool", 2, EN_NPAT, 4},
    [P_DD_MUTATE] = {"Mutate", 0, 2, 0},
    [P_CM_LVL] = {"Pluck", 0, 100, 30},     [P_CM_SND] = {"Sound", 0, EN_NCMSND - 1, 1}, [P_CM_DENS] = {"Density", 0, 100, 45},
    [P_CM_OCT] = {"Pluck oct", -2, 2, 0},   [P_CM_DEC] = {"Decay", 0, 100, 45},          [P_CM_STYLE] = {"Style", 0, EN_NCMSTYLE - 1, 0},
    [P_CM_RATE] = {"Rate", 0, 3, 0},        [P_CM_RANGE] = {"Range", 1, 3, 2},
    [P_PROB] = {"Prob", 0, 100, 100},     [P_PROB1] = {"Prob", 0, 100, 100},   [P_PROB2] = {"Prob", 0, 100, 100},
    [P_PROB3] = {"Prob", 0, 100, 100},    [P_PROB4] = {"Prob", 0, 100, 100},   [P_PROB5] = {"Prob", 0, 100, 100},
    [P_PAN] = {"Pan", 0, 100, 50},          [P_PAN1] = {"Pan", 0, 100, 50},        [P_PAN2] = {"Pan", 0, 100, 50},       [P_PAN3] = {"Pan", 0, 100, 50},
    [P_PAN4] = {"Pan", 0, 100, 50},         [P_SEND] = {"Send", 0, 100, 50},       [P_SEND1] = {"Send", 0, 100, 50},     [P_SEND2] = {"Send", 0, 100, 50},
    [P_SEND3] = {"Send", 0, 100, 50},       [P_SEND4] = {"Send", 0, 100, 50},
    [P_CM_MAN] = {"Pluck hand", 0, 1, 0},
    [P_RV_TYPE] = {"Type", 0, 2, 0},       [P_RV_DAMP] = {"Damping", 0, 100, 47}, [P_RV_PRE] = {"Predelay", 0, 100, 100}, [P_RV_X] = {"Amount", 0, 100, 30},
    [P_PL_CHO] = {"Chorus", 0, 100, 0},
    [P_MC_RATIO] = {"Ratio", 0, 6, 1},     [P_MC_THR] = {"Thresh", 0, 100, 60}, [P_MC_MAKE] = {"Make-up", 0, 100, 10}, [P_ML_CEIL] = {"Ceiling", 0, 100, 100},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }
int en_par_global(int p) { return p == P_TUNE || p == P_MIDI || p == P_THEME; }
int en_par_sticky(int p) { return en_par_global(p) || (p >= P_DD_CHANGE && p <= P_DD_MUTATE); }

/* the pieces' factory drum patterns: six for each piece (tools/gen_pats.py writes them) */
#include "pats.h"
volatile uint16_t en_pats[EN_NPAT][EN_NLANE];

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
static const int16_t PC_MONOLITH[] = {P_TEMPO, 132, P_ROOT, 4, P_MODE, 2, P_PAD, 20, P_ACID, 60, P_BASS, 75,
    P_AC_RES, 80, P_AC_ENV, 70, P_AC_DENS, 70, P_DRIVE, 60, P_NOISE, 15, P_REVERB, 25, P_SIZE, 60, P_PATTERN, 1,
    LV(0), 90, LV(2), 70, DE(3), 25, -1};
static const int16_t PC_ASCENSION[] = {P_TEMPO, 138, P_ROOT, 7, P_PAD, 60, P_BRIGHT, 60, P_ACID, 45, P_BASS, 60,
    P_AC_LEN, 12, P_PATTERN, 1, P_REVERB, 55, P_SIZE, 85, P_MOTION, 80, P_NZ_COLOR, 0, P_NOISE, 15, P_PL_DEPTH, 40,
    LV(4), 60, LV(2), 70, -1};
static const int16_t PC_NIGHT_DRIVE[] = {P_TEMPO, 130, P_ROOT, 9, P_MODE, 1, P_PAD, 35, P_ACID, 65, P_AC_CUT, 45,
    P_AC_RES, 75, P_AC_WAVE, 1, P_AC_SLIDE, 50, P_AC_DENS, 55, P_AC_LEN, 14, P_PATTERN, 1, P_NZ_COLOR, 4, P_NOISE, 12,
    P_SWING, 15, TU(5), 20, LV(5), 55, -1};
static const int16_t PC_DUB_CHAMBER[] = {P_TEMPO, 120, P_ROOT, 5, P_MODE, 7, P_PAD, 70, P_CHORD, EN_CH_DUB, P_PADTONE, 8, P_ACID, 40, P_AC_CUT, 20,
    P_AC_DEC, 60, P_ECHO, 80, P_SIZE, 90, P_AC_LEN, 7, P_AC_DENS, 35, P_PATTERN, 1, P_NZ_COLOR, 6, P_NOISE, 35,
    P_DE_TRIG, 1, P_SWING, 20, TU(1), 25, DE(1), 25, DE(0), 70, -1};
static const int16_t PC_GLASS_TUNNEL[] = {P_TEMPO, 134, P_ROOT, 11, P_MODE, 6, P_PAD, 30, P_BRIGHT, 55, P_ACID, 55,
    P_AC_LEN, 10, P_AC_DENS, 65, P_AC_MUT, 40, P_PATTERN, 1, P_NZ_COLOR, 7, P_NOISE, 20, P_PL_RATE, 3, P_PL_DEPTH, 50,
    P_PL_DEST, 2, TU(5), 30, DE(5), 25, DE(3), 25, -1};
static const int16_t PC_TIDAL[] = {P_TEMPO, 126, P_ROOT, 10, P_MODE, 4, P_PAD, 50, P_ACID, 50, P_BASS, 75,
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
#define LOFI_BASE P_PADTONE, 0, P_PAD, 62, P_BRIGHT, 30, P_DETUNE, 35, P_CHORD, EN_CH_7TH, P_VOICING, 1, P_PE_ATK, 25, P_PE_REL, 50, \
    P_ACID, 28, P_AC_WAVE, 15, P_AC_CUT, 30, P_AC_RES, 18, P_AC_ENV, 25, P_AC_DEC, 55, P_AC_SLIDE, 35, P_AC_DENS, 28, \
    P_AC_LEN, 12, P_AC_MUT, 15, P_BASS, 58, P_DR_INST, 1, P_DR_CUT, 30, P_DR_RES, 10, P_PULSE, 68, KIT(8), P_DRIVE, 20, \
    P_SWING, 38, P_REVERB, 34, P_SIZE, 55, P_ECHO, 32, P_ETIME, 3, P_EVOLVE, 30, P_MOTION, 30, P_PATTERN, 0, LV(1), 55, \
    LV(3), 42, LV(4), 38, LV(5), 40, P_PL_CHO, 50, P_RV_DAMP, 70, P_MC_RATIO, 2, P_MC_THR, 55, P_MC_MAKE, 25
static const int16_t PL_01[] = {LOFI_BASE, P_TEMPO, 72, P_ROOT, 2, P_MODE, 1, P_CHORD, EN_CH_9TH, -1};                       /* D dorian, ninths */
static const int16_t PL_02[] = {LOFI_BASE, P_TEMPO, 78, P_ROOT, 9, P_MODE, 5, P_DR_INST, 9, P_AC_WAVE, 4, -1};                /* A aeolian, sub bass, plucks */
static const int16_t PL_03[] = {LOFI_BASE, P_TEMPO, 84, P_ROOT, 7, P_MODE, 4, P_CHORD, EN_CH_9TH, P_PADTONE, 5, P_AC_WAVE, 11, -1};   /* G mixolydian, warm pad, bells */
static const int16_t PL_04[] = {LOFI_BASE, P_TEMPO, 90, P_ROOT, 0, P_MODE, 0, P_SWING, 42, P_AC_DENS, 35, -1};                /* C major, sevenths */
static const int16_t PL_05[] = {LOFI_BASE, P_TEMPO, 70, P_ROOT, 5, P_MODE, 1, P_CHORD, EN_CH_9TH, P_PADTONE, 2, P_AC_WAVE, 4, P_REVERB, 42, -1};   /* F dorian, strings, slow */
static const int16_t PL_06[] = {LOFI_BASE, P_TEMPO, 80, P_ROOT, 10, P_MODE, 5, P_PADTONE, 4, P_AC_WAVE, 11, P_PAD, 52, -1};  /* Bb aeolian, organ */
static const int16_t PL_07[] = {LOFI_BASE, P_TEMPO, 86, P_ROOT, 3, P_MODE, 7, P_AC_DENS, 22, P_SWING, 34, -1};                /* Eb minor pentatonic, sparse */
static const int16_t PL_08[] = {LOFI_BASE, P_TEMPO, 76, P_ROOT, 4, P_MODE, 1, P_CHORD, EN_CH_9TH, P_PADTONE, 3, P_BRIGHT, 24, P_ECHO, 40, -1};   /* E dorian, glass */
static const int16_t PL_09[] = {LOFI_BASE, P_TEMPO, 74, P_ROOT, 1, P_MODE, 3, P_DR_INST, 9, P_REVERB, 46, P_SIZE, 70, -1};   /* C# lydian, dreamy */
static const int16_t PL_10[] = {LOFI_BASE, P_TEMPO, 94, P_ROOT, 8, P_MODE, 4, P_CHORD, EN_CH_9TH, P_SWING, 45, P_AC_DENS, 38, P_LANE_LVL(0), 90, -1};   /* G# mixolydian, busier */
/* HOUSE: ten pieces, 118 .. 126 beats a minute, four on the floor, a clap on 2 and 4, open hats on the off-beats, stabs and
 * organ chords, a sub or square bass, a plucked line rolling in sixteenths; the pump (latched with the genre) ducks the chords. */
#define HOUSE_BASE P_PADTONE, 6, P_PAD, 52, P_BRIGHT, 40, P_CHORD, EN_CH_7TH, P_VOICING, 0, P_ACID, 38, P_AC_WAVE, 4, P_AC_CUT, 45, \
    P_AC_RES, 40, P_AC_ENV, 40, P_AC_DENS, 45, P_AC_LEN, 16, P_AC_MUT, 25, P_BASS, 70, P_DR_INST, 9, P_DR_CUT, 40, P_PULSE, 80, \
    KIT(1), P_DRIVE, 35, P_SWING, 12, P_ECHO, 30, P_ETIME, 3, P_REVERB, 30, P_SIZE, 50, P_EVOLVE, 45, P_MOTION, 50, P_PATTERN, 0, \
    LV(0), 90, LV(2), 72, LV(3), 50, LV(4), 55, LV(5), 45, P_PL_CHO, 35, P_RV_DAMP, 30, P_RV_PRE, 60, P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 35
static const int16_t PH_01[] = {HOUSE_BASE, P_TEMPO, 122, P_ROOT, 9, P_MODE, 1, -1};
static const int16_t PH_02[] = {HOUSE_BASE, P_TEMPO, 124, P_ROOT, 0, P_MODE, 5, P_PADTONE, 4, P_DR_INST, 10, P_AC_WAVE, 9, -1};
static const int16_t PH_03[] = {HOUSE_BASE, P_TEMPO, 120, P_ROOT, 5, P_MODE, 1, P_PADTONE, 0, P_CHORD, EN_CH_9TH, -1};
static const int16_t PH_04[] = {HOUSE_BASE, P_TEMPO, 126, P_ROOT, 2, P_MODE, 5, P_PADTONE, 10, P_DR_INST, 5, -1};
static const int16_t PH_05[] = {HOUSE_BASE, P_TEMPO, 118, P_ROOT, 7, P_MODE, 4, P_PADTONE, 8, P_CHORD, EN_CH_9TH, P_DR_INST, 1, P_AC_WAVE, 15, -1};
static const int16_t PH_06[] = {HOUSE_BASE, P_TEMPO, 124, P_ROOT, 11, P_MODE, 5, P_AC_WAVE, 9, -1};
static const int16_t PH_07[] = {HOUSE_BASE, P_TEMPO, 122, P_ROOT, 3, P_MODE, 7, P_PADTONE, 4, P_DR_INST, 10, -1};
static const int16_t PH_08[] = {HOUSE_BASE, P_TEMPO, 125, P_ROOT, 10, P_MODE, 1, P_PADTONE, 1, -1};
static const int16_t PH_09[] = {HOUSE_BASE, P_TEMPO, 121, P_ROOT, 4, P_MODE, 5, P_PADTONE, 14, P_DR_INST, 5, P_AC_WAVE, 8, -1};
static const int16_t PH_10[] = {HOUSE_BASE, P_TEMPO, 126, P_ROOT, 8, P_MODE, 1, P_SWING, 20, -1};
static const int16_t *const HOUSE_PIECES[EN_NPIECES] = {PH_01, PH_02, PH_03, PH_04, PH_05, PH_06, PH_07, PH_08, PH_09, PH_10};
static const char *const HOUSE_NAME[EN_NPIECES] = {"H01", "H02", "H03", "H04", "H05", "H06", "H07", "H08", "H09", "H10"};
/* DUBSTEP: ten pieces at 136 .. 150, the beat in half time (the snare on the third beat), a deep sub kick, the drone as the wobble
 * bass (its LFO, a quarter or half note, on its cutoff), a harsh lead, dark pads in the minor modes. */
#define DUB_BASE P_PADTONE, 7, P_PAD, 32, P_BRIGHT, 25, P_CHORD, EN_CH_4THS, P_VOICING, 1, P_ACID, 42, P_AC_WAVE, 14, P_AC_CUT, 55, \
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
static const int16_t PD_08[] = {DUB_BASE, P_TEMPO, 140, P_ROOT, 10, P_MODE, 2, P_DR_INST, 8, P_DL_RATE, 1, P_DL_SHAPE, 0, -1};
static const int16_t PD_09[] = {DUB_BASE, P_TEMPO, 136, P_ROOT, 6, P_MODE, 5, P_PADTONE, 11, -1};
static const int16_t PD_10[] = {DUB_BASE, P_TEMPO, 150, P_ROOT, 11, P_MODE, 2, P_DR_INST, 11, P_DL_SHAPE, 3, -1};
static const int16_t *const DUB_PIECES[EN_NPIECES] = {PD_01, PD_02, PD_03, PD_04, PD_05, PD_06, PD_07, PD_08, PD_09, PD_10};
static const char *const DUB_NAME[EN_NPIECES] = {"D01", "D02", "D03", "D04", "D05", "D06", "D07", "D08", "D09", "D10"};
/* the other six genres (4 .. 9: JUNGLE, DNB, BREAKS, GARAGE, ELECTRO, TRAP): a base recipe each, ten rows of tempo, root, mode, chord, pad, drone and acid, six
 * drum patterns from their classic masks (kick, snare, clap as the ghosts, hat, open, perc), the piece choosing which comes first */
static const int16_t X_JUNGLE[] = {P_PULSE, 80, P_DRIVE, 55, P_SWING, 6, P_BASSOCT, -1, P_DE_TRIG, 2, P_PAD, 38, P_ACID, 40, P_BASS, 80, P_AC_DENS, 30, P_AC_LEN, 8, P_ECHO, 40, P_REVERB, 35,
    KIT(4), DE(0), 25, LV(2), 30, P_PATTERN, 0, P_EVOLVE, 40, P_PL_CHO, 25, P_RV_DAMP, 50, P_RV_PRE, 40, P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 30, -1};
static const int16_t X_DNB[] = {P_PULSE, 78, P_DRIVE, 40, P_SWING, 5, P_BASSOCT, -1, P_DE_TRIG, 2, P_PAD, 36, P_ACID, 42, P_BASS, 82, P_AC_DENS, 40, P_AC_LEN, 12, P_ECHO, 35, P_REVERB, 40,
    KIT(12), DE(0), 20, P_PATTERN, 0, P_EVOLVE, 40, P_PL_CHO, 45, P_RV_TYPE, 1, P_RV_X, 25, P_RV_DAMP, 40, P_MC_RATIO, 3, P_MC_THR, 45, P_MC_MAKE, 30, -1};
static const int16_t X_BREAKS[] = {P_PULSE, 80, P_DRIVE, 70, P_SWING, 14, P_PAD, 40, P_ACID, 55, P_BASS, 70, P_AC_RES, 70, P_AC_ENV, 65, P_AC_DENS, 55, P_ECHO, 40, P_REVERB, 30,
    KIT(4), P_PATTERN, 0, P_EVOLVE, 45, P_PL_CHO, 20, P_RV_TYPE, 2, P_RV_X, 35, P_MC_RATIO, 4, P_MC_THR, 42, P_MC_MAKE, 35, -1};
static const int16_t X_GARAGE[] = {P_PULSE, 70, P_DRIVE, 25, P_SWING, 52, P_PAD, 42, P_ACID, 42, P_BASS, 70, P_AC_DENS, 45, P_AC_LEN, 12, P_ECHO, 40, P_REVERB, 40,
    P_KIT_KICK, 1, P_KIT_SNARE, 5, P_KIT_HAT, 1, P_KIT_PERC, 5, P_PATTERN, 0, P_EVOLVE, 35, P_PL_CHO, 40, P_RV_DAMP, 45, P_MC_RATIO, 2, P_MC_THR, 50, P_MC_MAKE, 25, -1};
static const int16_t X_ELECTRO[] = {P_PULSE, 78, P_DRIVE, 45, P_SWING, 3, P_PAD, 40, P_ACID, 55, P_BASS, 70, P_AC_DENS, 60, P_AC_LEN, 12, P_ECHO, 45, P_REVERB, 30,
    KIT(11), P_PATTERN, 0, P_EVOLVE, 40, P_PL_CHO, 40, P_RV_TYPE, 2, P_RV_X, 30, P_MC_RATIO, 3, P_MC_THR, 48, P_MC_MAKE, 30, -1};
static const int16_t X_TRAP[] = {P_PULSE, 85, P_DRIVE, 35, P_SWING, 0, P_BASSOCT, -1, P_DE_TRIG, 2, P_PAD, 40, P_ACID, 35, P_BASS, 88, P_AC_DENS, 20, P_AC_LEN, 8, P_ECHO, 40, P_REVERB, 35,
    KIT(13), LV(0), 95, P_PATTERN, 0, P_EVOLVE, 35, P_PL_CHO, 30, P_RV_TYPE, 1, P_RV_X, 30, P_RV_DAMP, 40, P_MC_RATIO, 4, P_MC_THR, 40, P_MC_MAKE, 40, -1};
static const int16_t *const XBASE[6] = {X_JUNGLE, X_DNB, X_BREAKS, X_GARAGE, X_ELECTRO, X_TRAP};
static const uint8_t XROW[6][10][7] = {
    {{165, 5, 5, 5, 8, 4, 9}, {168, 7, 2, 4, 7, 9, 13}, {162, 2, 1, 2, 0, 1, 4}, {170, 9, 5, 1, 14, 11, 3}, {160, 0, 7, 5, 8, 9, 11}, {166, 4, 2, 6, 11, 4, 14}, {164, 10, 5, 1, 2, 1, 9}, {168, 6, 1, 3, 3, 9, 4}, {170, 1, 5, 4, 7, 2, 13}, {160, 8, 2, 5, 8, 4, 9}},
    {{174, 9, 1, 3, 0, 9, 4}, {172, 2, 5, 2, 1, 1, 15}, {174, 5, 3, 3, 3, 9, 11}, {175, 4, 2, 6, 7, 4, 14}, {174, 0, 5, 4, 11, 3, 2}, {176, 7, 1, 2, 5, 9, 4}, {170, 10, 5, 3, 2, 1, 15}, {175, 6, 2, 4, 7, 13, 13}, {172, 11, 1, 2, 0, 9, 11}, {178, 3, 5, 6, 11, 2, 14}},
    {{100, 2, 1, 1, 4, 3, 2}, {110, 4, 7, 4, 14, 2, 0}, {118, 9, 5, 1, 6, 11, 3}, {125, 0, 4, 1, 4, 3, 1}, {130, 7, 2, 6, 7, 4, 14}, {104, 5, 1, 2, 10, 2, 2}, {135, 11, 5, 4, 14, 11, 13}, {120, 3, 7, 1, 6, 3, 0}, {112, 10, 4, 1, 4, 2, 6}, {128, 6, 5, 4, 13, 13, 2}},
    {{130, 5, 1, 3, 0, 9, 4}, {132, 9, 5, 2, 4, 5, 11}, {128, 0, 4, 3, 1, 9, 12}, {134, 2, 1, 2, 5, 10, 4}, {130, 7, 5, 3, 0, 4, 15}, {136, 10, 1, 2, 1, 9, 11}, {126, 4, 5, 3, 4, 5, 12}, {132, 1, 4, 2, 5, 9, 4}, {130, 6, 1, 3, 3, 1, 15}, {138, 10, 5, 2, 0, 10, 11}},
    {{118, 9, 5, 1, 2, 10, 13}, {120, 2, 2, 6, 3, 6, 3}, {122, 0, 1, 4, 14, 11, 9}, {124, 4, 5, 1, 2, 10, 13}, {125, 7, 2, 6, 3, 6, 3}, {126, 5, 1, 4, 14, 11, 9}, {127, 11, 5, 1, 2, 10, 13}, {128, 9, 2, 6, 3, 6, 3}, {129, 6, 1, 4, 14, 11, 9}, {130, 2, 5, 1, 2, 10, 13}},
    {{130, 1, 5, 6, 7, 9, 11}, {134, 5, 5, 4, 12, 9, 4}, {136, 9, 2, 1, 2, 9, 15}, {138, 3, 5, 6, 13, 3, 11}, {140, 7, 7, 4, 7, 9, 4}, {140, 11, 5, 1, 12, 9, 15}, {142, 6, 2, 6, 2, 9, 11}, {144, 0, 5, 4, 13, 3, 4}, {146, 4, 7, 1, 7, 9, 15}, {150, 10, 5, 6, 12, 9, 11}},
};
static const uint16_t XPAT[6][6][6] = {
    {{0x0C05, 0x1010, 0x8280, 0x5555, 0x0000, 0x0000}, {0x4C45, 0x1010, 0x8280, 0x5555, 0x0000, 0x0000}, {0x0405, 0x4010, 0x0280, 0x5555, 0x0000, 0x0000}, {0x040C, 0x4010, 0x0282, 0x4044, 0x0400, 0x0000}, {0x0401, 0x1010, 0x8288, 0x5555, 0x0000, 0x4040}, {0x0005, 0xF550, 0x0000, 0x0155, 0x0000, 0x0000}},
    {{0x0401, 0x1010, 0x0000, 0x5555, 0x0000, 0x0000}, {0x0401, 0x1010, 0x8080, 0xFF7F, 0x0000, 0x4040}, {0x0405, 0x1010, 0x8080, 0xFFFF, 0x0000, 0x0000}, {0x0401, 0x1010, 0x0000, 0x4444, 0x0000, 0x8008}, {0x0409, 0x1010, 0x0000, 0x5555, 0x0000, 0x4080}, {0x4041, 0x0100, 0x0100, 0x5555, 0x1000, 0x0000}},
    {{0x2445, 0x1010, 0x8A80, 0xDF7F, 0x2080, 0x0000}, {0x4181, 0x1010, 0x0000, 0x51D5, 0x0400, 0x0000}, {0x0C83, 0x1010, 0x0000, 0x0000, 0x5555, 0x0000}, {0x0501, 0x9090, 0x0000, 0x5555, 0x0000, 0x0000}, {0x4405, 0x1010, 0x1010, 0x5555, 0x8000, 0x0080}, {0x0005, 0xF510, 0x1000, 0x0155, 0x0000, 0x0000}},
    {{0x0401, 0x1010, 0x0000, 0x1111, 0x4044, 0x0000}, {0x0421, 0x1010, 0x0000, 0x1313, 0x4044, 0x8880}, {0x1111, 0x8080, 0x1010, 0x4444, 0x0000, 0x0000}, {0x0449, 0x1010, 0x0000, 0x1111, 0x4044, 0x0000}, {0x0401, 0x9244, 0x1010, 0x0111, 0x4000, 0x0000}, {0x0401, 0xF010, 0x0000, 0x1111, 0x0044, 0x0000}},
    {{0x2449, 0x0000, 0x1010, 0x4444, 0x0000, 0x0200}, {0x0909, 0x0000, 0x1010, 0x5555, 0x0000, 0x4040}, {0x0505, 0x1010, 0x1010, 0x5555, 0x0000, 0x8040}, {0x2449, 0x1000, 0x0010, 0x4444, 0x0000, 0x1449}, {0x050B, 0x0000, 0x1010, 0xFFFF, 0x0000, 0x0000}, {0x0109, 0xF000, 0x1110, 0x0055, 0x0000, 0x0000}},
    {{0x1001, 0x0000, 0x0100, 0x5555, 0x0000, 0x0000}, {0x0481, 0x0100, 0x0100, 0x5555, 0x0000, 0x0000}, {0x0009, 0x0100, 0x0100, 0xF555, 0x0000, 0x0000}, {0x2041, 0x0100, 0x0100, 0x5575, 0x0000, 0x0000}, {0x0401, 0x0100, 0x0100, 0x1111, 0x0000, 0x0000}, {0x0801, 0xD100, 0x0100, 0xFF55, 0x0000, 0x0000}},
};
static const char *const XNAME[6][10] = {
    {"J01", "J02", "J03", "J04", "J05", "J06", "J07", "J08", "J09", "J10"},
    {"N01", "N02", "N03", "N04", "N05", "N06", "N07", "N08", "N09", "N10"},
    {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10"},
    {"G01", "G02", "G03", "G04", "G05", "G06", "G07", "G08", "G09", "G10"},
    {"E01", "E02", "E03", "E04", "E05", "E06", "E07", "E08", "E09", "E10"},
    {"T01", "T02", "T03", "T04", "T05", "T06", "T07", "T08", "T09", "T10"},
};
static void x_pats(int g, int i, uint16_t pats[EN_NPAT][EN_NLANE])
{
    int p, l;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            pats[p][l] = XPAT[g][(i + p) % 6][l];
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
    static const uint16_t SNARE[3] = {0x0100, 0x4100, 0x0110};
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
static const char *const GENRE_NAME[EN_NGENRE] = {"HYPNO", "LOFI", "HOUSE", "DUBSTEP", "JUNGLE", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP"};
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
static const struct { uint8_t pad[5], drone[5], acid[5], pluck[5], kick[3], snare[3], hat[3], perc[3]; } PALETTE[EN_NGENRE] = {
    {{0}, {0}, {0}, {1, 4, 13, 28, 16}, {0}, {0}, {0}, {0}},
    {{0, 17, 5, 2, 3}, {1, 9, 7, 18, 23}, {15, 17, 18, 22, 4}, {12, 2, 17, 23, 28}, {8, 5, 7}, {8, 5, 1}, {8, 5, 1}, {8, 5, 11}},
    {{6, 4, 0, 10, 14}, {9, 10, 5, 1, 12}, {4, 9, 25, 8, 5}, {24, 7, 27, 20, 9}, {1, 10, 2}, {1, 10, 11}, {1, 10, 5}, {5, 1, 11}},
    {{7, 8, 11, 22, 1}, {4, 16, 20, 3, 8}, {14, 2, 24, 3, 20}, {31, 6, 22, 33, 8}, {9, 7, 6}, {9, 6, 4}, {9, 5, 2}, {9, 4, 6}},
    {{7, 23, 16, 8, 1}, {4, 8, 20, 9, 16}, {4, 21, 9, 26, 1}, {24, 10, 29, 12, 7}, {4, 12, 9}, {4, 12, 6}, {4, 12, 10}, {4, 12, 8}},
    {{3, 23, 5, 22, 10}, {4, 9, 11, 21, 20}, {8, 13, 23, 15, 9}, {26, 13, 3, 30, 11}, {12, 10, 2}, {12, 4, 10}, {12, 2, 10}, {12, 11, 5}},
    {{6, 14, 19, 10, 0}, {11, 2, 22, 5, 3}, {2, 1, 24, 3, 19}, {15, 24, 27, 9, 18}, {4, 6, 3}, {4, 6, 10}, {4, 10, 2}, {4, 11, 6}},
    {{5, 17, 0, 20, 16}, {9, 5, 21, 1, 10}, {4, 15, 25, 16, 9}, {12, 19, 28, 2, 29}, {1, 5, 8}, {5, 1, 8}, {1, 5, 10}, {5, 11, 1}},
    {{10, 14, 9, 3, 12}, {5, 10, 14, 6, 17}, {9, 6, 26, 23, 5}, {27, 10, 8, 22, 31}, {11, 10, 3}, {11, 2, 10}, {11, 2, 3}, {11, 10, 2}},
    {{7, 15, 22, 3, 18}, {9, 8, 22, 4, 10}, {11, 18, 16, 24, 22}, {13, 22, 33, 3, 29}, {13, 7, 9}, {13, 7, 10}, {13, 7, 11}, {13, 11, 7}},
};
static const uint8_t PLUCK_RATE[EN_NGENRE] = {0, 0, 0, 2, 1, 1, 0, 0, 1, 2};   /* the pluck's rate by genre: 1/8, 1/16, 1/4 */
static void vary(int g, int i, int16_t *par)
{
    const int pw = g * 3 + i;
    par[P_CM_SND] = PALETTE[g].pluck[(i * 4 + g) % 5];
    par[P_CM_STYLE] = (int16_t)((i * 2 + g) % EN_NCMSTYLE);
    par[P_CM_RATE] = PLUCK_RATE[g];
    if (g == 0)
        return;                                 /* the original pieces keep their pad, drone, acid and kit */
    par[P_PADTONE] = PALETTE[g].pad[i % 5];
    par[P_DR_INST] = PALETTE[g].drone[(i * 2 + g) % 5];
    par[P_AC_WAVE] = PALETTE[g].acid[(i * 3 + pw) % 5];
    par[P_KIT_KICK] = PALETTE[g].kick[(i + g) % 3];
    par[P_KIT_SNARE] = PALETTE[g].snare[(i * 2 + g) % 3];
    par[P_KIT_HAT] = PALETTE[g].hat[(i + 2 * g) % 3];
    par[P_KIT_PERC] = PALETTE[g].perc[(i * 2 + 2 * g) % 3];
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

static const char *const ETIME_NAME[6] = {"1/8", "3/16", "1/4", "1/4.", "1/2", "1 bar"};
static const float ETIME_BEATS[6] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};
static const char *const LFO_RATE_NAME[8] = {"1/4", "1/2", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars"};
static const float LFO_RATE_BEATS[8] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f};
static const char *const LFO_SHAPE_NAME[6] = {"Sine", "Triangle", "Saw", "Square", "S&H", "Drift"};
static const char *const PAD_DEST_NAME[4] = {"Filter", "Level", "Pan", "Pitch"};
static const char *const DRONE_DEST_NAME[4] = {"Cutoff", "Pitch", "Level", "Drive"};
static const char *const NOISE_NAME[8] = {"Mint", "Clean", "Light", "Used", "Worn", "Scratched", "Dusty", "Trashed"};   /* the vinyl's wear */
static const char *const TRIG_NAME[6] = {"Chord", "1 bar", "2 bars", "4 bars", "8 bars", "Off"};
static const char *const THEME_NAME[EN_NTHEME] = {"Synth", "Acid", "Ice", "Ember", "Phosphor"};
/* the voice bank: what each voice can play (the sounds themselves: PAD_SOUND, DRONE_SOUND, ACID_SOUND, and
 * the kits: the old styles' drums) */
const char *const EN_PAD_SOUND[EN_NSOUND] = {"Rhodes", "Voices", "Strings", "Glass", "Organ", "Warm", "Stabs", "Dark",
                                             "Dub", "Trance", "Poly", "Hollow", "Pings", "Hits", "Rave", "Sine",
                                             "Ooh", "Wurli", "Mallet", "Brass", "Fat", "Pluck", "Chapel", "Air"};
const char *const EN_DRONE_SOUND[EN_NSOUND] = {"Saws", "Warm", "Grit", "Dirty", "Reese", "Pulse", "FM", "Organ",
                                               "Rumble", "Sub", "Square", "Hoover", "Fifths", "Metal", "Sync", "Bright",
                                               "Wub", "Bite", "Tine", "Clang", "Gnarl", "Wide", "Power", "Air"};
const char *const EN_ACID_SOUND[EN_NACID] = {"Saw", "Square", "Driven", "Sync", "Pluck", "PWM", "Chip", "Sub",
                                              "Trance", "Bleep", "Fifth", "Bell", "Organ", "Laser", "Fold", "Soft",
                                              "Marimba", "E.Piano", "Kalimba", "Brass", "Gong", "Clav", "Vibes", "Digital",
                                              "Super", "CZ", "Mini"};
const char *const EN_KIT_NAME[EN_NKIT] = {"Hypno", "House", "Techno", "Acid", "Break", "Minimal", "Hard", "808", "Dusty", "Heavy",
                                                   "909", "Electro", "DnB", "Trap"};

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

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    switch (i) {
    case P_ETIME: s = ETIME_NAME[(unsigned)v < 6u ? v : 0]; break;
    case P_MIDI: s = ONOFF[v ? 1 : 0]; break;
    case P_PADTONE: s = EN_PAD_SOUND[(unsigned)v < EN_NSOUND ? v : 0]; break;
    case P_DR_INST: s = EN_DRONE_SOUND[(unsigned)v < EN_NSOUND ? v : 0]; break;
    case P_KIT_KICK: case P_KIT_SNARE: case P_KIT_HAT: case P_KIT_PERC: s = EN_KIT_NAME[(unsigned)v < EN_NKIT ? v : 0]; break;
    case P_MODE: s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0]; break;
    case P_ROOT: s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0]; break;
    case P_PL_RATE: case P_DL_RATE: s = LFO_RATE_NAME[v & 7]; break;
    case P_PL_SHAPE: case P_DL_SHAPE: s = LFO_SHAPE_NAME[(unsigned)v < 6u ? v : 0]; break;
    case P_PL_DEST: s = PAD_DEST_NAME[v & 3]; break;
    case P_DL_DEST: s = DRONE_DEST_NAME[v & 3]; break;
    case P_NZ_COLOR: s = NOISE_NAME[v & 7]; break;
    case P_DE_TRIG: s = TRIG_NAME[(unsigned)v < 6u ? v : 0]; break;
    case P_AC_WAVE: s = EN_ACID_SOUND[(unsigned)v < EN_NACID ? v : 0]; break;
    case P_CM_SND: s = EN_CM_SOUND[(unsigned)v < EN_NCMSND ? v : 0]; break;
    case P_CM_STYLE: s = EN_CM_STYLE[(unsigned)v < EN_NCMSTYLE ? v : 0]; break;
    case P_CM_RATE: s = EN_CM_RATE[v & 3]; break;
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

const char *const EN_CHORD_NAME[EN_NCHORD] = {"Auto", "Triads", "7ths", "9ths", "Sus", "Dub", "Fourths"};

int en_chord_kind(uint32_t pool, int root, int deg, int kind, int *pcs)
{
    /* the scale steps above the degree to take, in order (then what's left, should they repeat) */
    static const uint8_t OFF[5][7] = {
        {0, 2, 4, 6, 1, 3, 5},                  /* thirds: 1 3 5 7 */
        {0, 2, 6, 1, 4, 3, 5},                  /* ninths: 1 3 7 9 (the fifth left out) */
        {0, 3, 4, 6, 1, 2, 5},                  /* sus: 1 4 5 b7 */
    };
    static const uint8_t SHAPE[2][4] = {{0, 3, 10, 14}, {0, 5, 10, 15}};   /* dub: m9 (no fifth); fourths */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    const uint8_t *off = OFF[0];
    deg = ((deg % m) + m) % m;
    if (kind == EN_CH_DUB || kind == EN_CH_4THS) {   /* a fixed shape on the degree's note */
        for (i = 0; i < 4; i++)
            pcs[i] = (root + st[deg] + SHAPE[kind == EN_CH_4THS][i]) % 12;
        return 4;
    }
    want = kind == EN_CH_TRIAD ? 3 : kind == EN_CH_AUTO ? (m >= 6 ? 4 : 3) : 4;
    if (kind == EN_CH_9TH)
        off = OFF[1];
    else if (kind == EN_CH_SUS)
        off = OFF[2];
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
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_SECTION, C_FX, C_MUTE, C_NOTE, C_HUSH, C_FXK, C_TRK, C_GATEPAT, C_FXS, C_FILLQ, C_DJSYNC, C_FLXH, C_SHED };
static uint8_t muted;                            /* remote: no sound, no MIDI (C_MUTE, after what's queued) */
static uint8_t hush;                             /* a store while playing: fade out, hold, fade back in (C_HUSH) */
static float hush_g = 1.0f;                      /* its gain, 0..1 */
volatile uint8_t en_hushed;                      /* faded right out: the flash can be written */
#define QN 256u                                  /* more than every parameter at once (project_apply), with room */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
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
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_mute(int on) { post(C_MUTE, on, 0); }
void en_hush(int on) { post(C_HUSH, on, 0); }
void en_note(int voice, int note, int on) { post(C_NOTE, voice, (note & 0x7F) | (on ? 0x100 : 0)); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_section(int cmd) { post(C_SECTION, cmd, 0); }
void en_fx(uint64_t mask)                       /* in four quarters: the queue carries 16 bits */
{
    int q;
    for (q = 0; q < 4; q++)
        post(C_FX, q, (int)(mask >> (16 * q) & 0xFFFFu));
}
void en_fxk(int k, int v) { post(C_FXK, k, v); }
void en_trk(uint32_t mute, uint32_t solo) { post(C_TRK, 0, (int)((mute & 0xFFu) | (solo & 0xFFu) << 8)); }
void en_gate_pat(uint32_t steps) { post(C_GATEPAT, 0, (int)(steps & 0xFFFFu)); }
void en_fxs(int i, int v) { post(C_FXS, i, v); }
void en_fill_next(void) { post(C_FILLQ, 0, 0); }
void en_flx_hold(int on) { post(C_FLXH, on ? 1 : 0, 0); }
volatile uint8_t en_shed_level;
void en_shed(int level) { post(C_SHED, level < 0 ? 0 : level > 2 ? 2 : level, 0); }
void en_dj_sync(int on) { post(C_DJSYNC, on ? 1 : 0, 0); }
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
volatile float en_drum_glow[EN_NDRUM];
volatile uint8_t en_pat_now, en_pat_next;
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static int16_t par[P_NPARAMS];
int en_par_now(int p) { return (unsigned)p < (unsigned)P_NPARAMS ? par[p] : 0; }
static int links_on(void) { return par[P_LINKS] != 0; }
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int deg;                                 /* the chord's scale degree */
static int chord_pcs[4], chord_nt;
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

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI] && !muted)
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
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
} kit_t;
static const kit_t KITS[EN_NKIT] = {
    /* Hypno: as it always was */
    {5.0f, 0.0016f, 0.15f, 0.75f, 0.25f, 1.0f, 0.0f,
     185.0f, 330.0f, 1.1f, 0.7f, 1.0f, 353.0f, 1.0f,
     1.0f, 0.6f, 1.0f, 1.0f, 380.0f, 1.48f, 1.4f, 1.0f, 0.0f, 0.0f},
    /* House: a round kick, a wide clap, open hats, congas */
    {3.2f, 0.0011f, 0.2f, 0.8f, 0.12f, 0.6f, 0.0f,
     210.0f, 360.0f, 1.3f, 0.5f, 0.9f, 440.0f, 1.4f,
     1.25f, 0.9f, 1.1f, 1.4f, 300.0f, 1.0f, 0.6f, 1.25f, 0.0f, 0.0f},
    /* Techno: a hard kick with a rumble, metal hats, a tight clap, a bell perc */
    {7.0f, 0.0024f, 0.12f, 0.5f, 0.4f, 1.6f, 0.6f,
     170.0f, 290.0f, 1.4f, 0.4f, 0.7f, 260.0f, 0.7f,
     1.55f, 0.35f, 0.7f, 1.3f, 560.0f, 3.5f, 2.2f, 1.1f, 0.0f, 0.0f},
    /* Acid: a punchy 909-ish kit with a cowbell */
    {4.0f, 0.0014f, 0.13f, 0.6f, 0.35f, 1.25f, 0.0f,
     180.0f, 330.0f, 1.5f, 0.6f, 1.2f, 440.0f, 1.1f,
     1.0f, 0.6f, 0.9f, 1.6f, 560.0f, 1.48f, 2.0f, 1.0f, 0.0f, 0.0f},
    /* Break: a distorted kick, a cracking snare, noisy hats, a tom; chopped and crushed */
    {6.0f, 0.0022f, 0.1f, 0.45f, 0.45f, 2.5f, 0.2f,
     240.0f, 420.0f, 1.6f, 0.8f, 0.6f, 300.0f, 0.8f,
     1.3f, 1.0f, 0.6f, 0.8f, 200.0f, 1.0f, 0.9f, 0.8f, 1.0f, 0.6f},
    /* Minimal: a short clicky kick, a rim, tight hats, a woodblock */
    {4.0f, 0.0018f, 0.08f, 0.35f, 0.5f, 1.0f, 0.0f,
     400.0f, 820.0f, 0.5f, 1.0f, 0.5f, 300.0f, 0.6f,
     1.4f, 0.3f, 0.5f, 0.7f, 720.0f, 2.0f, 0.8f, 0.9f, 0.0f, 0.0f},
    /* Hard: an industrial kick, distorted with a long rumble, a noisy snare, a metal clank; a little crushed */
    {8.0f, 0.0028f, 0.14f, 0.6f, 0.6f, 3.2f, 0.9f,
     200.0f, 340.0f, 1.8f, 0.5f, 0.8f, 220.0f, 0.8f,
     1.7f, 0.8f, 0.8f, 1.2f, 140.0f, 2.76f, 2.8f, 1.3f, 0.0f, 0.25f},
    /* 808: a long boom, a snappy snare, a wide clap, a cowbell */
    {2.2f, 0.0007f, 0.3f, 1.6f, 0.08f, 0.5f, 0.0f,
     238.0f, 476.0f, 1.0f, 0.9f, 1.1f, 380.0f, 1.6f,
     1.15f, 0.5f, 0.8f, 1.8f, 540.0f, 1.48f, 1.2f, 1.0f, 0.0f, 0.0f},
    /* Dusty (lofi): a soft round kick, a loose low snare, a muted clap, dull short hats, a soft rim; the kick ducks the rest a little */
    {2.8f, 0.0010f, 0.2f, 0.95f, 0.08f, 0.45f, 0.0f,
     172.0f, 290.0f, 0.9f, 0.85f, 0.95f, 380.0f, 0.8f,
     0.85f, 0.55f, 0.85f, 1.1f, 520.0f, 1.5f, 0.9f, 0.35f, 0.0f, 0.0f},
    /* Heavy (dubstep): a deep sub kick with a long tail, a big cracking snare, a wide clap, tight hats, a metal perc; it ducks the bass hard */
    {3.0f, 0.0013f, 0.28f, 1.35f, 0.25f, 1.1f, 0.35f,
     190.0f, 330.0f, 1.5f, 0.75f, 1.05f, 330.0f, 1.1f,
     1.2f, 0.5f, 0.7f, 1.0f, 300.0f, 2.2f, 1.6f, 1.0f, 0.0f, 0.0f},
    /* 909: a punchy kick with a click, a snappy snare, a clap, ticking hats and long open ones, a tom */
    {5.0f, 0.0015f, 0.16f, 0.7f, 0.3f, 1.1f, 0.0f,
     188.0f, 332.0f, 1.25f, 0.75f, 0.85f, 410.0f, 0.95f,
     1.3f, 0.7f, 0.7f, 1.2f, 150.0f, 1.0f, 0.5f, 1.1f, 0.0f, 0.0f},
    /* Electro: a short high kick, a tight bright snare, a thin clap, glassy hats, a zappy tom */
    {6.0f, 0.0020f, 0.09f, 0.4f, 0.5f, 1.3f, 0.0f,
     220.0f, 400.0f, 1.5f, 0.5f, 0.6f, 300.0f, 0.4f,
     1.6f, 0.5f, 0.5f, 1.0f, 160.0f, 1.0f, 1.2f, 1.0f, 0.0f, 0.0f},
    /* DnB: a tight kick, a loud cracking snare, a clap, fast tiny hats, a clicky perc */
    {5.5f, 0.0018f, 0.1f, 0.4f, 0.35f, 1.0f, 0.0f,
     240.0f, 460.0f, 1.3f, 0.7f, 0.55f, 380.0f, 0.5f,
     1.5f, 0.5f, 0.45f, 0.8f, 320.0f, 1.5f, 0.8f, 1.0f, 0.0f, 0.0f},
    /* Trap: a long 808 sub with almost no click, a snare and a wide clap together, tiny hats, a bell */
    {2.0f, 0.0006f, 0.35f, 2.0f, 0.05f, 0.6f, 0.0f,
     200.0f, 390.0f, 1.2f, 0.6f, 0.9f, 460.0f, 1.0f,
     1.7f, 0.4f, 0.4f, 0.9f, 460.0f, 1.48f, 1.0f, 1.1f, 0.0f, 0.0f},
};
static const kit_t *KK = &KITS[0], *KS = &KITS[0], *KH = &KITS[0], *KP = &KITS[0];   /* kick, snare and
                                                 * clap, hats, perc: each its kit */

/* the pad's sounds: the oscillator, stabs (chords on the off-beats) or breaths, its bus filter and level */
enum { PO_RHODES, PO_VOICES, PO_STRINGS, PO_GLASS, PO_ORGAN, PO_WARM, PO_SUPER, PO_PWM, PO_HOLLOW, PO_SINE };
static const struct { uint8_t osc, stabs; float fc, g; } PAD_SOUND[EN_NSOUND] = {
    {PO_RHODES, 0, 1.0f, 1.0f}, {PO_VOICES, 0, 1.0f, 1.0f}, {PO_STRINGS, 0, 1.25f, 0.75f}, {PO_GLASS, 0, 1.7f, 0.85f},
    {PO_ORGAN, 0, 1.1f, 0.8f}, {PO_WARM, 0, 0.85f, 1.0f}, {PO_RHODES, 1, 1.3f, 0.9f}, {PO_VOICES, 0, 0.6f, 0.8f},
    {PO_STRINGS, 1, 0.55f, 1.4f},               /* Dub: saw chords stabbed, dark, into the echo */
    {PO_SUPER, 0, 1.3f, 0.75f},                 /* Trance: a supersaw */
    {PO_PWM, 0, 1.1f, 1.1f},                   /* Poly: two detuned saws less each other, a pulse's width drifting */
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
static const float DRONE_FR[EN_NSOUND] = {[17] = 2.0f, [18] = 3.5f, [19] = 5.19f};   /* DO_FMR's modulator ratio, by sound */
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
    {DO_FMR, 0.45f, 0.4f, 1.1f, 0.6f, 0.9f},    /* Tine: FM at 3.5: an electric piano's bass */
    {DO_FMR, 0.5f, 0.4f, 1.4f, 0.5f, 0.9f},    /* Clang: FM far off the harmonics */
    {DO_REESE, 0.55f, 0.35f, 2.6f, 0.2f, 0.9f},  /* Gnarl: the reese driven hard */
    {DO_HOOVER, 0.3f, 0.6f, 1.0f, 0.3f, 0.9f},  /* Wide: the hoover, softer, the sub up */
    {DO_FIFTHS, 0.45f, 0.6f, 1.8f, -0.2f, 0.9f},    /* Power: the root and its fifth, driven */
    {DO_SINE, 0.7f, 0.2f, 1.0f, 1.0f, 1.0f},    /* Air: a soft open sine */
};
/* the acid's: the oscillator, its drive and level */
enum { AO_SAW, AO_SQUARE, AO_SYNC, AO_PLUCK, AO_PWM, AO_CHIP, AO_SUB, AO_TRANCE, AO_BLEEP, AO_FIFTH, AO_BELL, AO_ORGAN,
       AO_LASER, AO_FOLD, AO_SOFT, AO_FMR, AO_SUPER, AO_CZ, AO_MINI };
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
};

static int pad_snd(void) { return (unsigned)par[P_PADTONE] < EN_NSOUND ? par[P_PADTONE] : 0; }
static int pad_stabs(void) { return PAD_SOUND[pad_snd()].stabs; }
static int pad_osc(void) { return PAD_SOUND[pad_snd()].osc; }
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
 * hat on an acid note accents it, and the perc lane can be the acid's clock (it steps only on a perc
 * hit; in a breakdown, with the drums out, it runs free again). Each by its amount (0: none). */
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
    float ph[2], inc[2];
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
static float pad_gate, pad_gain, pad_vol, bright, detune = 0.00144f;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */
static float pad_filt, pad_lvl = 1.0f, pad_pan, pad_pitch;   /* the pad's LFO, where it goes this chunk */

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

/* the LFOs (the pad's, the drone's): synced to the tempo, 1/4 note .. 32 bars a cycle; sine, triangle,
 * saw, square, sample and hold, or a drift (a smooth wander to a new random place each cycle) */
typedef struct {
    float ph, v, a, b;                          /* the phase (turns); the value -1..1; S&H / drift: from, to */
} lfo_t;
static lfo_t pad_lfo, drone_lfo;
static float frand(void);
static void lfo_step(lfo_t *l, int rate, int shape, float n, float step_samples)
{
    float p;
    l->ph += n / (step_samples * 4.0f * LFO_RATE_BEATS[rate & 7]);
    if (l->ph >= 1.0f) {
        l->ph -= (float)(int)l->ph;
        l->a = l->b;
        l->b = frand() * 2.0f - 1.0f;
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
    f = mtof(v->note + 0.08f * motion * mod[i].v + pad_pitch);              /* a slow drift, as tape; the LFO */
    v->inc[0] = f * (1.0f - detune) * (1.0f / EN_SR);   /* Detune: 2.5 .. 25 cents either way */
    v->inc[1] = f * (1.0f + detune) * (1.0f / EN_SR);
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
} bass;
static float bass_vol, reso_k = 1.0f;
static const float GLIDE_K = 1.0f / (0.6f * EN_SR);
static float brk;                               /* the sections, smoothed: +1 breakdown, -0.5 build, 0 groove */
static float duck_env, duck_dec = 0.9997f;      /* the kick's sidechain: 1 just hit, falling */

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
    uint32_t rng;
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
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static float pulse_vol, swing, drive_k = 1.0f, lane_g[EN_NDRUM], lane_tune[EN_NDRUM], lane_dec[EN_NDRUM];
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
    nsteps = en_scale(pool, steps);
}

static int acid_note(int s)                     /* acid step s's note: from the chord's degree, in its register */
{
    int d = deg + ac_off[s], o = d / nsteps, n;
    n = 36 + par[P_ROOT] + steps[d % nsteps] + 12 * (o + ac_oct[s] + par[P_AC_OCT] + par[P_OCT]);   /* the root: C2 .. B2 */
    while (n > 96)
        n -= 12;
    while (n < 28)
        n += 12;
    return n;
}

static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], i;
    chord_nt = en_chord_kind(pool, par[P_ROOT], deg, par[P_CHORD], chord_pcs);
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
    for (i = 0; i < EN_NPAD; i++) {             /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)to[i];
    }
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
    if (bass.play)                              /* the keys have it: the root waits */
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

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i;
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
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
    deg = i;
    pad_retarget();
    pad_leave();
    bass_to(chord_pcs[0]);
    if (par[P_DE_TRIG] == 0)
        bass.env_up = 1;                        /* the drone's envelope: on each chord */
    drift_reset();
}

/* ----------------------------------------------------------- the acid line --- */
static void ac_roll(int st)                     /* a step made anew */
{
    float dens = (float)par[P_AC_DENS] * 0.01f;
    uint16_t m = (uint16_t)(1u << st);
    if (frand() < (st == 0 ? fm_minf(dens + 0.3f, 1.0f) : dens))   /* the first step: more often a note */
        ac_gate |= m;
    else
        ac_gate &= (uint16_t)~m;
    ac_off[st] = (int8_t)(frand() < 0.8f ? 2 * irand(chord_nt) : irand(nsteps));   /* a chord tone, or now and then not */
    ac_oct[st] = (int8_t)(frand() < 0.55f ? 0 : irand(par[P_AC_RANGE]));
    if (frand() < 0.3f)
        ac_accent |= m;
    else
        ac_accent &= (uint16_t)~m;
    if (frand() < (float)par[P_AC_SLIDE] * 0.007f)
        ac_slide |= m;
    else
        ac_slide &= (uint16_t)~m;
}

static void ac_publish(void)
{
    en_acid_gate = ac_gate;
    en_acid_acc = ac_accent;
    en_acid_len = (uint8_t)par[P_AC_LEN];
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
    const float k = (float)par[P_AC_MUT] * 0.01f;
    float m = k * k + (par[P_AC_MUT] ? 0.05f * evolve : 0.0f);     /* (squared: 25 is as it was, the top reaches every step) */
    int st;
    for (st = 0; st < par[P_AC_LEN]; st++)
        if (frand() < m)
            ac_roll(st);
    ac_publish();
}

static void acid_step(void)                     /* a sixteenth of the acid line */
{
    int st = ac_pos, len = par[P_AC_LEN], prev = (st + len - 1) % len;
    uint16_t m = (uint16_t)(1u << st);
    en_acid_pos = (uint8_t)st;
    if (acid.play || par[P_AC_MAN]) {           /* the keys have it: the line steps on, unheard */
    } else if (ac_gate & m) {
        int n = acid_note(st), slid = (ac_slide >> prev & 1u) && (ac_gate >> prev & 1u) && acid.amp_t > 0.0f;
        float dec = 0.06f * fm_exp2f((float)par[P_AC_DEC] * 0.05f);   /* 0.06 .. 1.9 s */
        acid.inc_t = mtof((float)n) * (1.0f / EN_SR);
        if (!slid) {                            /* a new note: the envelope again */
            acid.inc = acid.inc_t;
            acid.env = 1.0f;
            acid.acc = (ac_accent & m) ? 1.0f : 0.0f;
            if (lk_hat && links_on())            /* the links: a hat here accents it */
                acid.acc = fm_maxf(acid.acc, (float)par[P_LK_HAT] * 0.012f);
            acid.amp_t = 1.0f;
            acid.edec = fm_expf(-6.9f * 32.0f / (dec * (acid.acc > 0.0f ? 0.6f : 1.0f) * EN_SR));
            if (acid.note)
                midi(0x83, acid.note, 0);
            midi(0x93, (uint32_t)n, acid.acc > 0.0f ? 120u : 90u);
            acid.note = (uint8_t)n;
        }
        acid.sliding = (uint8_t)slid;
        acid.gate_left = (ac_slide & m) ? -1 : (int32_t)(0.55f * step_len);
        en_acid_glow = 1.0f;
    } else if (acid.gate_left < 0) {            /* a slide into a rest: let go */
        acid.gate_left = 0;
        acid.amp_t = 0.0f;
    }
    if (++ac_pos >= len) {
        ac_pos = 0;
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
static uint8_t fill_q, fill_bar, dj_sync, drop_imp;   /* a fill asked for the next bar; this bar's; the DJ follows the chords; the drop's impact is due */
volatile uint8_t en_sec, en_sec_req;

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
static int dd_len(void) { static const uint8_t L[6] = {0, 4, 8, 16, 32, 64}; return L[(unsigned)par[P_DD_EVERY] < 6u ? par[P_DD_EVERY] : 0]; }
static int bits16(uint32_t b) { int n = 0; while (b) { n += (int)(b & 1u); b >>= 1; } return n; }

static void dd_mutate(int p)
{
    static const uint8_t LN[4] = {L_SNARE, L_HAT, L_OHAT, L_PERC};   /* the kick and the clap stay as written */
    int c;
    for (c = 0; c < 2; c++) {                   /* a hit or two, a step either way */
        int l = LN[irand(4)], on[16], m = 0, st;
        uint32_t b = en_pats[p][l];
        for (st = 0; st < 16; st++)
            if (b >> st & 1u)
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
        if ((frand() < 0.5f && h > 1) || h >= 10) {
            for (st = irand(16); !(b >> st & 1u); st = (st + 1) & 15)
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
    if (!par[P_DD_CHANGE] && !par[P_DD_MUTATE]) {
        dd_in = en_dd_left = -1;
        dd_fill = 0;
        dd_bars = 0;
        return;
    }
    if (dd_in > 0)
        dd_in--;
    if (dd_in == 0) {
        int pool = par[P_DD_POOL] < 2 ? 2 : par[P_DD_POOL] > EN_NPAT ? EN_NPAT : par[P_DD_POOL], nb = pat_now;
        if (par[P_DD_CHANGE] == 1)
            nb = pat_now + 1 >= pool ? 0 : pat_now + 1;
        else if (par[P_DD_CHANGE] == 2)
            do
                nb = irand(pool);
            while (nb == pat_now);
        if (par[P_DD_CHANGE]) {
            pat_next = nb;
            par[P_PATTERN] = (int16_t)nb;
            en_dd_changes++;
        }
        if (par[P_DD_MUTATE])
            dd_mutate(pat_next);
        dd_in = -1;
        dd_bars = 0;
    } else if (len && dd_in < 0 && ++dd_bars >= (uint32_t)len - 1u) {
        dd_in = 1;
    }
    dd_fill = dd_in == 1 && par[P_DD_CHANGE];
    en_dd_left = (int8_t)(dd_in >= 0 ? dd_in : len ? len - (int)dd_bars : -1);
}

static void drum_publish(void)
{
    en_pulse_rest = (uint8_t)(sec == SEC_BREAK);
    en_sec = (uint8_t)sec;
    en_sec_req = (uint8_t)sec_req;
    en_pat_now = (uint8_t)pat_now;
    en_pat_next = (uint8_t)pat_next;
}

static void section_cmd(int c)                  /* the player's: on the bar */
{
    if (c == EN_CMD_BREAK) {
        sec = SEC_BREAK;
    } else if (c == EN_CMD_BUILD) {
        if (sec == SEC_BREAK) {
            sec = SEC_BUILD;
            sec_bars = 4;
        }
    } else if (sec != SEC_GROOVE) {             /* the drop: at once the full groove (the breakdown's swell and the build's dip gone, not eased out), the kick, clap and open hat on the one */
        sec = SEC_GROOVE;
        brk = 0.0f;
        drop_imp = 1;
        if (frand() < 0.4f + 0.5f * evolve)
            change_pending = 1;
    }
}

static int kick_note(void) { return 28 + (par[P_ROOT] + 8) % 12; }   /* E1 .. Eb2, on the piece's root */

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    const float dec = lane_dec[d], tf = fm_exp2f(lane_tune[d] * (1.0f / 12.0f));
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    v->t = 0.0f;
    v->note = DRUM_GM[d];
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
            duck_env = 1.0f;
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
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
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

static int32_t cm_wait, cm_pnote;                /* a pluck note that waits for the swing */
static float cm_pvel;
static void cm_trigger(int note, float vel);
static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = cm_wait > 0 ? cm_wait : 0;
    int d;
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
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
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
const char *const EN_CM_SOUND[EN_NCMSND] = {"Pluck", "Marimba", "E.Piano", "Kalimba", "Bell", "Vibes", "Gong", "Clav", "Digital", "Brass", "Bleep", "Glass",
    "DX Tine EP", "DX Glass Bell", "DX Round Bass", "DX Brass Sect", "DX Soft Pad", "DX Wood Bars", "DX Drawbars", "DX Nylon Pick", "DX Brass 2", "DX Solid Bass", "DX Bells", "DX Marimba", "DX Clavinet", "DX Strings", "DX Glass Pad", "DX Sync Lead", "DX Harp", "DX Kalimba", "DX Flute", "DX Steel Drum", "DX Saw Bass", "DX Tubular"};
const char *const EN_CM_STYLE[EN_NCMSTYLE] = {"Up", "Down", "Up-down", "Random", "Skip", "Thirds"};
const char *const EN_CM_RATE[4] = {"1/8", "1/16", "1/4", "3/16"};
#include "fm6.inc"
#define BLK_FM 256                              /* = BLK (checked below) */
#define FM_OUT 0.22f                            /* the DX voices' level against the plain sounds; FM_GAIN: each patch's, to level them */
static const float FM_GAIN[EN_NCMFM] = {0.60f, 0.96f, 0.73f, 0.95f, 2.56f, 1.87f, 0.76f, 1.26f, 0.94f, 1.33f, 0.75f, 2.55f, 0.88f, 1.22f, 3.84f, 0.85f, 1.32f, 2.13f, 0.67f, 1.58f, 1.62f, 0.73f};   /* tests/host/fmcal.c */
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
volatile float en_cm_level;

static void cm_trigger(int note, float vel)     /* a note: the next voice, its decay as the Decay knob has it */
{
    const int voice = cm_alt ^= 1;
    const float dec = 0.1f * fm_exp2f((float)par[P_CM_DEC] * 0.045f);   /* the time to silence: 0.1 .. 2.2 s */
    if (par[P_CM_SND] >= CM_NPLAIN) {            /* a DX voice: the key is down for as long as the Decay knob says (the patch's own envelopes do the rest) */
        const float d = (float)par[P_CM_DEC] * 0.01f;
        fm_note_on(note, vel, (int32_t)((0.12f + 1.6f * d * d) * EN_SR));
        return;
    }
    cm[voice].inc = mtof((float)note) * (1.0f / EN_SR);
    cm[voice].ps = 0.0f;
    cm[voice].env = 1.0f;
    cm[voice].age = 0;
    cm[voice].vel = vel;
    cm[voice].dm = fm_expf(-6.9f / (dec * EN_SR));
}
static void cm_play(int key)                    /* a note played by hand (the JAM page): the key is the pitch, within the song's and the pluck's octaves */
{
    int note = key + 12 * (par[P_OCT] + par[P_CM_OCT]);
    while (note > 100)
        note -= 12;
    while (note < 36)
        note += 12;
    cm_trigger(note, 0.95f);
}

static void cm_step(int st, int32_t sw)         /* a sixteenth of the counter-melody: st its place in the bar, sw the swing's delay (samples) */
{
    static const uint8_t RATE[4] = {2, 1, 4, 3};
    const int ri = par[P_CM_RATE] < 0 ? 0 : par[P_CM_RATE] > 3 ? 3 : par[P_CM_RATE];
    int arp[12], n = 0, k, o, t, note, idx, range = par[P_CM_RANGE] < 1 ? 1 : par[P_CM_RANGE] > 3 ? 3 : par[P_CM_RANGE];
    float dens = (float)par[P_CM_DENS] * 0.01f;
    const int base = 60 + 12 * (par[P_OCT] + par[P_CM_OCT]);
    /* on the bar's grid, whatever has gone before (a stop, a new piece): every 2nd, 1st, 4th sixteenth; the dotted eighth 0 3 6 9 12 each bar */
    if (RATE[ri] == 3 ? (st % 3 != 0 || st > 12) : st % RATE[ri])
        return;
    if (par[P_CM_LVL] == 0 || par[P_CM_MAN] || chord_nt < 1)
        return;
    if (sec == SEC_BREAK)
        dens *= 0.4f;
    if (frand() >= (cm_i == 0 ? fm_minf(dens + 0.3f, 1.0f) : dens))
        return;
    for (o = 0; o < range; o++)                 /* the chord's tones over the octaves, low to high */
        for (k = 0; k < chord_nt && n < 12; k++)
            arp[n++] = base + chord_pcs[k] + 12 * o;
    for (k = 1; k < n; k++) {
        t = arp[k];
        for (o = k - 1; o >= 0 && arp[o] > t; o--)
            arp[o + 1] = arp[o];
        arp[o + 1] = t;
    }
    switch (par[P_CM_STYLE]) {
    case 1: idx = n - 1 - cm_i % n; cm_i++; break;
    case 2: {                                    /* up and down, not repeating the ends */
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
    note = arp[idx];
    while (note > 100)
        note -= 12;
    while (note < 36)
        note += 12;
    if (sw > 0) {                               /* the off sixteenths are late, as the drums' are */
        cm_wait = sw;
        cm_pnote = note;
        cm_pvel = 0.7f + 0.3f * frand();
    } else {
        cm_trigger(note, 0.7f + 0.3f * frand());
    }
}


/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static int32_t step_t;                          /* samples into this sixteenth (the gate's clock) */
static uint8_t rpt_arm;                         /* a beat repeat waiting for the next sixteenth: its rate + 1 */
static uint8_t scat_arm, var_exit, rvs_arm;              /* a scatter waiting for the sixteenth; a brake or spin-back let go, leaving on the next */
static void rpt_engage(int r);
static void scat_engage(void);
static void var_leave(void);
static void rvs_engage(void);
static void flx_tick(int st);
static uint64_t fx_on;
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
    }
    if (st == 0) {                              /* a bar */
        int tb = TRIG_BARS[(unsigned)par[P_DE_TRIG] < 6u ? par[P_DE_TRIG] : 0];
        fill_bar = fill_q;                      /* the fill the DJ asked for a bar ahead */
        fill_q = 0;
        if (dj_sync && sec == SEC_GROOVE && en_playing && !en_frozen) {   /* the DJ follows the chords: the bar before a change is a fill, the change's bar a breakdown */
            if (change_pending)
                section_cmd(EN_CMD_BREAK);
            else if (drift_total > 0.0f && drift_left <= step_len * 16.0f)
                fill_bar = 1;
        }
        if (sec == SEC_BUILD && --sec_bars <= 0)
            section_cmd(EN_CMD_DROP);           /* the build's end: the drop */
        if (sec_req) {
            section_cmd(sec_req - 1);
            sec_req = 0;
        }
        dd_bar();                               /* drum drift: a fill coming, or a change now */
        pat_now = pat_next;                     /* a new pattern: on the bar */
        if (tb && bars % (uint32_t)tb == 0u)
            bass.env_up = 1;                    /* the drone's envelope: every 1 .. 8 bars */
        bars++;
    }
    drum_publish();
    lk_hat = 0;
    for (l = 0; l < EN_NDRUM; l++) {
        int hit = (int)(en_pats[pat_now][l] >> st & 1u), dbl = 0;   /* dbl: a roll's extra hits (1: one at half a step; 2: two, in triplets; 3: a flam) */
        const uint64_t fxm = fx_on;
        float vel = 0.9f;
        if (sec == SEC_BREAK) {
            hit = 0;
        } else if (sec == SEC_BUILD) {
            if (l == L_KICK || l == L_CLAP || l == L_PERC)
                hit = 0;
            else if (l == L_SNARE) {            /* the roll: quarters, eighths, sixteenths, and a breath before the drop */
                hit = sec_bars > 2 ? (st & 3) == 0 : sec_bars == 2 ? (st & 1) == 0 : st < 14;
                vel = 0.3f + 0.7f * ((float)(4 - sec_bars) * 16.0f + (float)st) / 64.0f;
            }
        }
        if ((fill_hold || fill_bar || (dd_fill && st >= 8 && sec == SEC_GROOVE)) && (l == L_SNARE || l == L_HAT)) {   /* FILL; the DJ's; drift's */
            hit = 1;
            vel = l == L_SNARE ? 0.35f + 0.04f * (float)st : 0.5f;
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
        if (hit && par[P_PROB + l] < 100 && frand() * 100.0f >= (float)par[P_PROB + l])   /* the lane's probability: some hits do not sound */
            hit = 0;
        if (drop_imp && st == 0 && (l == L_KICK || l == L_CLAP || l == L_OHAT)) {   /* the drop lands */
            hit = 1;
            vel = l == L_KICK ? 1.0f : 0.95f;
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
        if (hit && (l == L_HAT || l == L_OHAT))
            lk_hat = 1;
        if (hit && l == L_PERC)
            perc_hit = 1;
        if (hit) {
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
            } else if (chop > 0.0f && !sw && (l == L_SNARE || l == L_KICK || l == L_HAT) && frand() < 0.2f * chop) {
                drum_fire(l, vel);              /* a 32nd roll: the hit, and again half a step on */
                drum[l].wait = (int32_t)(0.5f * step_len);
                drum[l].pend = vel * 0.7f;
            } else {
                drum_hit(l, vel, sw);
            }
        }
    }
    if (pad_stabs() && !par[P_PAD_MAN] && en_playing && sec != SEC_BREAK &&   /* the pad's stabs: the off-beats, more with Evolve */
        ((st & 3) == 2 || ((st == 7 || st == 15) && frand() < 0.35f * evolve))) {
        int i;
        stab_env = st & 1 ? 0.7f : 1.0f;
        stab_k = fm_expf(-6.9f * 32.0f / (0.08f * fm_exp2f((float)par[P_PE_REL] * 0.04f) * EN_SR));
        for (i = 0; i < EN_NPAD; i++) {
            pad_t *v = &pad[i];
            if (v->play)                        /* played from the keys: not stabbed */
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
    en_drum_pos = (uint8_t)st;
    drop_imp = 0;
    if (links_on() && par[P_LK_PERC] && sec == SEC_GROOVE) {
        if (perc_hit)
            acid_step();                        /* the links: the perc lane is the acid's clock */
    } else {
        acid_step();
    }
    cm_step(st, sw);
    if (st == 0 && change_pending && !en_frozen) {   /* chords move on the bar */
        change_pending = 0;
        chord_change();
        if (!par[P_DD_EVERY] && (par[P_DD_CHANGE] || par[P_DD_MUTATE]) && dd_in < 0)
            dd_in = 2;                          /* drum drift with the chord: a fill next bar, the change after */
    }
    sixteenth++;
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
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return (float)d->b[j < 0 ? j + d->n : j] * (1.0f / RV_Q);
}
static inline void push(dl_t *d, float x)
{
    x = fm_clampf(x * RV_Q, -32767.0f, 32767.0f);
    d->b[d->i] = (int16_t)(int32_t)(x + (x < 0.0f ? -0.5f : 0.5f));
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
        sh_buf[sh_w] = m;
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
typedef struct { int n, r0; int fm_on; uint32_t fm_w; } rv_job_t;
static rv_job_t rv_job;
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
static volatile uint32_t stage_done;             /* the job's first stage (the pad) is done: its outputs may be read */
static void render_pad(uint32_t n);
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
    render_pad((uint32_t)((const rv_job_t *)p)->n);
    if (c2_fence)
        c2_fence();
    stage_done = 1;
    reverb_job(p);
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
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
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
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
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
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v);   /* wow */
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
    case P_NZ_COLOR: nz_color = v; break;
    case P_NZ_TONE: break;                      /* (the old noise's: unused, kept for the bank's layout) */
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
    case P_DR_RES: reso_k = 1.5f - 0.0138f * (float)v; break;   /* none .. near singing (k 1.5 .. 0.12) */
    case P_AC_LEN:
        if (ac_pos >= v)
            ac_pos = 0;
        en_acid_len = (uint8_t)v;
        break;
    case P_AC_DENS: case P_AC_RANGE:             /* heard at once: the line made again */
        if (nsteps)
            ac_regrow();
        break;
    case P_DRIVE: drive_k = 1.0f + 0.04f * (float)v; break;
    case P_SWING: swing = (float)v * 0.005f; break;   /* the off sixteenths up to half a step late */
    case P_PATTERN:                              /* waits for the bar (now, while stopped) */
        pat_next = v;
        if (!en_playing)
            pat_now = v;
        drum_publish();
        break;
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep the progress, at the new pace */
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
            bass_to(chord_pcs[0]);
        break;
    case P_KIT_KICK: KK = &KITS[(unsigned)v < EN_NKIT ? v : 0]; break;   /* the kit: at each drum's next hit */
    case P_KIT_SNARE: KS = &KITS[(unsigned)v < EN_NKIT ? v : 0]; break;
    case P_KIT_PERC: KP = &KITS[(unsigned)v < EN_NKIT ? v : 0]; break;
    case P_KIT_HAT: {
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
            bass_to(chord_pcs[0]);
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
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
}

static void regrow(void)                        /* a new piece: a fresh acid line, the home chord */
{
    deg = 0;
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
    bass.rest = 0;
    ac_regrow();
    ac_pos = 0;
    sec = SEC_GROOVE;
    sec_req = 0;
    drum_publish();
    drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
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
static void note_cmd(int voice, int key, int on)
{
    int k, t, legato;
    if (voice == EN_V_PLUCK) {                  /* the counter-melody played by hand: the note's pitch (the off does nothing: it rings out) */
        if (on)
            cm_play(key);
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
            if (on && !legato)
                bass.env_up = 1;                /* a new press: the envelope again */
            if (bass.amp < 0.02f)
                bass.note = (float)m;           /* from silence: straight there, no glide */
            if (bass.midi_note != m) {
                midi(0x82, bass.midi_note, 0);
                midi(0x92, (uint32_t)m, 100);
            }
            bass.play = 1;
            bass.target = (float)m;
            bass.midi_note = (uint8_t)m;
        } else if (bass.play) {                 /* let go: back to the chord's root */
            bass.play = 0;
            bass_to((int)bass.gtarget % 12);
        }
    } else if (t >= 0) {                        /* the acid */
        int m = fold_note(t - 12 + 12 * (par[P_AC_OCT] + par[P_OCT]), 28, 96);
        acid.inc_t = mtof((float)m) * (1.0f / EN_SR);
        if (legato && acid.play && acid.amp_t > 0.0f) {
            acid.sliding = 1;                   /* legato: a slide, no new attack */
        } else {
            float dec = 0.06f * fm_exp2f((float)par[P_AC_DEC] * 0.05f);
            acid.inc = acid.inc_t;
            acid.env = 1.0f;
            acid.acc = 0.6f;
            acid.edec = fm_expf(-6.9f * 32.0f / (dec * EN_SR));
            acid.sliding = 0;
        }
        if (acid.note)
            midi(0x83, acid.note, 0);
        midi(0x93, (uint32_t)m, 110);
        acid.note = (uint8_t)m;
        acid.amp_t = 1.0f;
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
        drum[i].on = 0, drum[i].wait = 0;
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
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    fm_init();
    fm_reset();
    master_dyn_reset();
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
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
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
        pat_now = pat_next;
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
        drum[d].wait = drum[d].wait2 = 0;        /* a swung hit still waiting stays unplayed */
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
static uint16_t gx_pat = 0x5249;                 /* the pattern gate: steps 0 3 6 9 12 14 open (3+3+3+3+2+2) */
const int16_t EN_FXS_DEF[EN_NFXS] = EN_FXS_DEFAULTS;
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
static float sw_m, sw_s[2][2], sw_c[4], sw_s2[2][2], sw_c2[4];
static float tk_m, tk_c[2][4], tk_s[2][2][2], wh_m, wh_c[4], wh_s[2][2], wh_env;   /* talk (two formants a channel), wah */
static uint32_t wh_cnt;   /* (the second: the resonant spike, for the band-pass) */
static float rm_m, rm_ph, rm_inc, rm_wet;   /* the ring mod: its fade, phase, step and share */   /* the sweep: its fade, its two filters' states, its coefficients */
static float rl_gain = 1.0f, tp_wd, tp_fd, tp_drive = 1.5f, tp_k = 0.55f, rs_inv, rs_filt, rs_wash, rs_echo;   /* the roll's swell; the tape's; the riser's */
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
static float lp_amt_e, hp_amt_e;                 /* the filters' amounts: the key's or the knob's, the higher */
static float cr_lv = 2048.0f, cr_wet;
static int cr_hold = 1;
static const float RPT_BEATS[9] = {2.0f, 1.0f, 2.0f / 3.0f, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f, 1.0f / 6.0f, 0.0625f};   /* (the 7th and 8th: triplets; the last, 1/64, only the roll reaches) */
const char *const EN_FX_NAME[EN_NFX] = {
    [FX_RPT2] = "Rpt 1/2", "Rpt 1/4", "Rpt 1/6", "Rpt 1/8", "Rpt 1/16", "Rpt 1/32", "Reverse", "Tape stop",
    "Mute kick", "Mute hats", "Mute snare", "Mute perc", "Low-pass", "High-pass",
    "Gate 1/16", "Gate 1/8", "Gate pattern", "Echo throw", "Fill", "Phaser", "Flanger", "Tape", "Roll", "Pump", "Riser", "Vinyl", "Scatter", "Brake", "Spin back", "Half time", "Sweep", "Scratch", "Freeze", "Rev swell", "Ring mod", "Rpt 1/8T", "Rpt 1/16T",
    "Stut 16", "Stut 32", "Beat repeat", "Half Rpt", "Beat Rev", "Skip", "Trance", "Gate 8", "Double", "Shuffle", "Back-Fwd", "Roll Out", "Build", "Scratch", "Dropout", "Talk", "Wah", "Hat roll", "Hat trip", "Snr roll", "Flam", "Dbl kick"};

static void rpt_engage(int r)                   /* the slice that just ended, looped from its start */
{
    float len = RPT_BEATS[r] * 4.0f * step_len;
    while (len > (float)(RB_N - 8) * 2.0f && r < 5)   /* too long for the ring: the next shorter, in time */
        len = RPT_BEATS[++r] * 4.0f * step_len;
    if (len > (float)(RB_N - 8) * 2.0f)
        len = (float)(RB_N - 8) * 2.0f;
    bmode = BM_RPT;
    brate = r;
    bm_len = len;
    bm_start = rb_w - (uint32_t)(len * 0.5f + 0.5f), bm_sm = bm_start % RB_N;
    bm_off = 0.0f;
    bm_tgt = 1.0f;
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
static const uint8_t FLEX[15][16] = {
    /* 1 STUT 16 */ {FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 2 STUT 32 */ {FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1)},
    /* 3 BEAT RPT */ {FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 4 HALF RPT */ {FL(7, 0, 0, 0), FL(6, 0, 0, 0), FL(5, 0, 0, 0), FL(4, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(7, 0, 0, 0), FL(6, 0, 0, 0), FL(5, 0, 0, 0), FL(4, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 5 BEAT REV */ {FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0)},
    /* 6 SKIP */ {FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0), FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0), FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0), FL(3, 0, 0, 0), FL(0, 0, 1, 0), FL(1, 0, 0, 0), FL(0, 0, 1, 0)},
    /* 7 TRANCE */ {FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0)},
    /* 8 GATE 8 */ {FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0)},
    /* 9 DOUBLE */ {FL(3, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(3, 0, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0)},
    /* 10 SHUFFLE */ {FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(3, 0, 0, 0), FL(0, 0, 0, 0), FL(2, 0, 0, 0)},
    /* 11 BACK-FWD */ {FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0), FL(3, 0, 0, 0), FL(2, 0, 0, 0), FL(1, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(1, 1, 0, 0), FL(2, 1, 0, 0), FL(3, 1, 0, 0)},
    /* 12 ROLL OUT */ {FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 1)},
    /* 13 BUILD */ {FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 0), FL(0, 0, 0, 1), FL(0, 0, 0, 1), FL(0, 0, 0, 2), FL(0, 0, 0, 2)},
    /* 14 SCRATCH */ {FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(1, 0, 0, 0), FL(1, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0), FL(0, 0, 0, 0), FL(0, 1, 0, 0)},
    /* 15 DROPOUT */ {FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(0, 0, 1, 0), FL(0, 0, 1, 0), FL(15, 0, 0, 0), FL(15, 0, 0, 0)},
};
#define FLEXMASK (0x7FFFull << FX_FLEX1)
uint8_t en_flex_step(int pat, int step) { return FLEX[(unsigned)pat < 15u ? pat : 0][step & 15]; }
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
        flx_off = (float)(flx_n - 1 - (e % flx_n)) * flx_slice;
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
enum { DJ_HORN, DJ_SIREN, DJ_LASER, DJ_ZAP, DJ_BEEP, DJ_DBL, DJ_WHOOP, DJ_DUB, DJ_DROP, DJ_RISER, DJ_REWIND, DJ_CRASH, DJ_STAB, DJ_BELL, DJ_ROLL };
static const struct { uint8_t type; float hz, aux; const char *name; } DJS[27] = {
    {DJ_HORN, 466.0f, 0.0f, "AIR HORN"}, {DJ_HORN, 349.0f, 0.0f, "HORN LO"}, {DJ_HORN, 622.0f, 0.0f, "HORN HI"}, {DJ_HORN, 466.0f, 1.0f, "HORN MIN"},
    {DJ_SIREN, 900.0f, 1.2f, "SIREN"}, {DJ_SIREN, 1100.0f, 3.0f, "SIREN 2"}, {DJ_SIREN, 700.0f, 0.4f, "SIREN 3"},
    {DJ_LASER, 2400.0f, 0.06f, "LASER"}, {DJ_LASER, 1400.0f, 0.1f, "LASER LO"},
    {DJ_ZAP, 3000.0f, 0.15f, "ZAP"}, {DJ_ZAP, 6000.0f, 0.08f, "ZAP HI"},
    {DJ_BEEP, 1000.0f, 0.1f, "BEEP"}, {DJ_BEEP, 600.0f, 0.12f, "BEEP LO"}, {DJ_BEEP, 1700.0f, 0.08f, "BEEP HI"}, {DJ_DBL, 1200.0f, 0.07f, "2 BEEP"},
    {DJ_WHOOP, 300.0f, 0.35f, "WHOOP"}, {DJ_WHOOP, 800.0f, 0.6f, "WHOOP 2"},
    {DJ_DUB, 1400.0f, 0.9f, "DUB SIREN"}, {DJ_DUB, 900.0f, 1.6f, "DUB LOW"},
    {DJ_DROP, 140.0f, 1.2f, "DROP"}, {DJ_DROP, 100.0f, 2.4f, "DROP LONG"},
    {DJ_RISER, 0.0f, 3.0f, "RISER"}, {DJ_REWIND, 2200.0f, 0.7f, "REWIND"}, {DJ_CRASH, 0.0f, 0.9f, "CRASH"},
    {DJ_STAB, 220.0f, 0.0f, "STAB"}, {DJ_BELL, 880.0f, 0.0f, "BELL"}, {DJ_ROLL, 0.0f, 2.5f, "SNR ROLL"}};
const char *en_dj_name(int key) { return DJS[(unsigned)key < 27u ? key : 0].name; }
typedef struct { uint8_t on, held, key, type, hold_mode; float hz, aux, t, ph, ph2, ph3, env, z, z2; uint32_t age; } dj_t;
static dj_t djv[4];
static uint32_t dj_age;
volatile uint8_t en_smp_active;                 /* the snips sounding */

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
        const int oct = fxs_v[EN_FXS_SM_MODE];
        d->on = 1;
        d->held = 1;
        d->key = (uint8_t)key;
        d->type = DJS[key].type;
        d->hold_mode = fxs_v[EN_FXS_SM_LOOP] != 0;
        d->hz = DJS[key].hz * (oct == 1 ? 2.0f : oct == 2 ? 0.5f : 1.0f);
        d->aux = DJS[key].aux;
        d->t = d->ph = d->ph2 = d->ph3 = d->env = d->z = d->z2 = 0.0f;
        d->age = ++dj_age;
    }
}

static inline float dj_wrap(float p) { return p - (float)(int)p; }
/* one sample of a voice: its signal and its own envelope; it ends itself (on = 0) when its sound is over */
static float dj_voice(dj_t *v, float *amp)
{
    const float t = v->t * (1.0f / EN_SR), inc = v->hz * (1.0f / EN_SR), n = frand() * 2.0f - 1.0f;
    float y = 0.0f, a = 1.0f;
    switch (v->type) {
    case DJ_HORN: {                             /* three saws (a chord), low-passed */
        const float r2 = v->aux > 0.5f ? 1.189f : 1.26f;
        y = (blep_saw(v->ph, inc) + blep_saw(v->ph2, inc * r2) + blep_saw(v->ph3, inc * 1.5f)) * 0.4f;
        v->ph = dj_wrap(v->ph + inc);
        v->ph2 = dj_wrap(v->ph2 + inc * r2);
        v->ph3 = dj_wrap(v->ph3 + inc * 1.5f);
        v->z += 0.3f * (y - v->z);
        y = v->z * 1.4f;
        break;
    }
    case DJ_SIREN: {                            /* a sine swept up and down */
        const float lf = dj_wrap(t * v->aux), tri = lf < 0.5f ? lf * 4.0f - 1.0f : 3.0f - lf * 4.0f;
        v->ph = dj_wrap(v->ph + inc * (1.0f + 0.5f * tri));
        y = 0.6f * sine(v->ph) + 0.25f * sine(dj_wrap(v->ph * 2.0f));
        break;
    }
    case DJ_LASER:                              /* a fast fall in pitch, hard-edged */
        v->ph = dj_wrap(v->ph + (v->hz * fm_expf(-t / v->aux) + 150.0f) * (1.0f / EN_SR));
        y = 0.5f * fm_tanhf(3.0f * sine(v->ph));
        a = fm_expf(-t * 4.0f);
        if (t > 0.5f)
            v->on = 0;
        break;
    case DJ_ZAP: {                              /* noise through a band that falls */
        const float k = 0.04f + 0.6f * fm_expf(-t / v->aux);
        v->z += k * (n - v->z);
        v->z2 += k * (v->z - v->z2);
        y = (v->z - v->z2) * 3.0f;
        a = fm_expf(-t * 7.0f / v->aux * 0.5f);
        if (t > 0.5f)
            v->on = 0;
        break;
    }
    case DJ_BEEP: case DJ_DBL: {
        const float ln = v->aux, g1 = t < ln ? 1.0f : 0.0f, g2 = v->type == DJ_DBL && t > ln * 1.9f && t < ln * 2.9f ? 1.0f : 0.0f;
        v->ph = dj_wrap(v->ph + inc);
        y = sine(v->ph) * 0.7f;
        a = g1 + g2;
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
        v->ph = dj_wrap(v->ph + (v->hz * fm_expf(-t * 3.0f) + 28.0f) * (1.0f / EN_SR));
        y = 0.9f * sine(v->ph);
        a = fm_expf(-t / (v->aux * 0.5f));
        if (t > v->aux * 2.0f)
            v->on = 0;
        break;
    case DJ_RISER: {                            /* noise, brighter and louder over its time */
        const float u = fm_minf(t / v->aux, 1.0f);
        v->z += (0.03f + 0.9f * u * u) * (n - v->z);
        y = v->z * 3.0f;
        a = 0.08f + 0.9f * u * u;
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
    case DJ_CRASH:
        v->z += 0.7f * (n - v->z);
        y = (n - v->z) * 0.9f;                  /* the highs of noise */
        a = fm_expf(-t / (v->aux * 0.35f));
        if (t > v->aux * 2.0f)
            v->on = 0;
        break;
    case DJ_STAB: {                             /* three saws a minor chord, a filter closing */
        const float k = 0.5f * fm_expf(-t * 6.0f) + 0.04f;
        y = (blep_saw(v->ph, inc) + blep_saw(v->ph2, inc * 1.189f) + blep_saw(v->ph3, inc * 1.498f)) * 0.4f;
        v->ph = dj_wrap(v->ph + inc);
        v->ph2 = dj_wrap(v->ph2 + inc * 1.189f);
        v->ph3 = dj_wrap(v->ph3 + inc * 1.498f);
        v->z += k * (y - v->z);
        y = v->z;
        a = fm_expf(-t * 7.0f);
        if (t > 0.7f)
            v->on = 0;
        break;
    }
    case DJ_BELL: {
        v->ph = dj_wrap(v->ph + inc);
        v->ph2 = dj_wrap(v->ph2 + inc * 1.4f);
        y = 0.7f * sine_any(v->ph + 2.0f * fm_expf(-t * 3.0f) * sine(v->ph2));
        a = fm_expf(-t * 2.2f);
        if (t > 3.0f)
            v->on = 0;
        break;
    }
    default: {                                  /* a snare roll that speeds up */
        const float rate = 7.0f + 18.0f * fm_minf(t / v->aux, 1.0f);
        float bt;
        v->ph3 = dj_wrap(v->ph3 + rate * (1.0f / EN_SR));
        bt = v->ph3;
        v->z += 0.25f * (n - v->z);
        y = (n - v->z) * 0.9f + 0.3f * sine(dj_wrap(v->ph + 0.0f));
        v->ph = dj_wrap(v->ph + 190.0f * (1.0f / EN_SR));
        a = fm_expf(-bt * 7.0f);
        break;
    }
    }
    v->t += 1.0f;
    *amp = a;
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

static void fx_control(uint32_t n)              /* once a chunk: what the held effects ask for */
{
    const uint64_t m = fx_req;
    uint32_t solo = trk_solo,
             mute = trk_mute | (uint32_t)(m >> FX_MUTE_KICK & 1u) << EN_TR_KICK | (uint32_t)(m >> FX_MUTE_HATS & 1u) << EN_TR_HATS |
                    (uint32_t)(m >> FX_MUTE_SNARE & 1u) << EN_TR_SNARE | (uint32_t)(m >> FX_MUTE_PERC & 1u) << EN_TR_PERC;
    float dt = (float)n * (1.0f / EN_SR);
    int t, want = -1;
    fx_on = m;
    {   /* the pattern gate: its loop's step now, open or closed, for each track it is on (gx_pat, the settings) */
        const uint32_t len = (uint32_t)fm_clampf((float)fxs_v[EN_FXS_GX_LEN], 2.0f, 16.0f), tg = (uint32_t)fxs_v[EN_FXS_GX_TGT];
        const int open = (int)(gx_pat >> ((sixteenth - 1u) % len) & 1u);
        const float closed = 1.0f - fm_clampf((float)fxs_v[EN_FXS_GX_DEPTH] * 0.01f, 0.0f, 1.0f);
        for (t = 0; t < EN_NTRACK; t++)
            gate_f[t] = (m >> FX_GATEX & 1u) && (tg >> t & 1u) && !open ? closed : 1.0f;
        if (m >> FX_PUMP & 1u) {                /* the pump: down on each period, back up over its release, on its tracks */
            static const uint8_t PER[4] = {8, 4, 2, 1};   /* sixteenths: 1/2 note, 1/4, 1/8, 1/16 */
            const uint32_t per = PER[(int)fm_clampf((float)fxs_v[EN_FXS_PM_RATE], 0.0f, 3.0f)], pt = (uint32_t)fxs_v[EN_FXS_PM_TGT];
            const float ph = ((float)((sixteenth - 1u) % per) * step_len + (float)step_t) / ((float)per * step_len),
                        rel = 0.15f + 0.0085f * (float)fxs_v[EN_FXS_PM_REL], dp = (float)fxs_v[EN_FXS_PM_DEPTH] * 0.01f,
                        f = (1.0f - dp) + dp * fm_minf(ph / rel, 1.0f);
            for (t = 0; t < EN_NTRACK; t++)
                if (pt >> t & 1u)
                    gate_f[t] *= f;
        }
    }
    for (t = 0; t < EN_NTRACK; t++) {           /* mute and solo: 5 ms ramps */
        float tgt = (!(mute >> t & 1u) && (!solo || (solo >> t & 1u))) ? gate_f[t] : 0.0f, d = (float)n * (1.0f / 220.0f);
        trk_g[t] = trk_g1[t];
        trk_g1[t] = trk_g[t] + fm_clampf(tgt - trk_g[t], -d, d);
    }
    tg_inv = 1.0f / (float)n;
    lp_amt = fm_clampf(lp_amt + (m >> FX_LP & 1u ? dt * 2.0f : -dt * 6.0f), 0.0f, 1.0f);   /* in 0.5 s, out in 0.17 */
    hp_amt = fm_clampf(hp_amt + (m >> FX_HP & 1u ? dt * 2.0f : -dt * 6.0f), 0.0f, 1.0f);
    throw_key = fm_clampf(throw_key + (m >> FX_THROW & 1u ? dt * 60.0f : -dt * 60.0f), 0.0f, 1.0f);
    vin_m += ((float)(uint32_t)(m >> FX_VINYL & 1u) - vin_m) * fm_minf(dt * 8.0f, 1.0f);   /* the vinyl: ~0.15 s */
    rs_inv = 1.0f / (fm_exp2f((float)fxs_v[EN_FXS_RS_TIME] * 0.036f));          /* 1 .. 12 s to the top (rises a second at a time) */
    riser_amt = fm_clampf(riser_amt + (m >> FX_RISER & 1u ? dt * rs_inv : -dt * 2.0f), 0.0f, 1.0f);   /* (and 0.5 s down) */
    rs_filt = (float)fxs_v[EN_FXS_RS_FILT] * 0.009f;                             /* the high-pass: up to 0.9 */
    rs_wash = (float)fxs_v[EN_FXS_RS_WASH] * 0.005f;                             /* the reverb: up to 0.5 */
    rs_echo = (float)fxs_v[EN_FXS_RS_ECHO] * 0.007f;                             /* the echo: up to 0.7 */
    roll_t = m >> FX_ROLL & 1u ? roll_t + dt * EN_SR / (4.0f * step_len) : 0.0f;   /* beats held */
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
            const float fc = 120.0f * fm_exp2f((float)fxs_v[EN_FXS_SW_DEPTH] * 0.07f * tri);
            if (fxs_v[EN_FXS_SW_TYPE] == 2) {            /* band-pass: a wide base (always), and the resonance a narrow spike on top of it */
                svf_coefs(sw_c, fc, 1.0f);
                svf_coefs(sw_c2, fc, 1.4f - 0.0128f * (float)fxs_v[EN_FXS_SW_RESO]);
            } else {
                svf_coefs(sw_c, fc, 1.4f - 0.0128f * (float)fxs_v[EN_FXS_SW_RESO]);
            }
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
        lp_amt_e = fm_maxf(lp_amt, lpk_s);
        hp_amt_e = fm_maxf(fm_maxf(hp_amt, hpk_s), rs_filt * riser_amt);   /* the build: a high-pass rising */
    }
    echo_fbx = echo_fb + (0.86f - echo_fb) * throw_amt;
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
    } else if (m & FLEXMASK) {                  /* Flex Beat: the lowest pad held */
        int p = 0;
        while (!(m >> (FX_FLEX1 + p) & 1u))
            p++;
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
static uint32_t lk_i, lk_n, lk_si;
static float cp_thr = 0.0f, cp_slope, cp_make = 1.0f, cp_ka = 0.05f, cp_kr = 0.0005f, lk_ceil = 1.0f;
volatile float en_comp_gr, en_lim_gr, en_out_peak;
static float gr_hold, pk_hold;

static void reverb_set(void)                    /* once a chunk, before the tank's job: the type, damping, predelay */
{
    rv_type = par[P_RV_TYPE] < 0 ? 0 : par[P_RV_TYPE] > 2 ? 2 : par[P_RV_TYPE];
    rv_damp = 0.85f - 0.0085f * (float)par[P_RV_DAMP];
    rv_pre = 256 + (PRE_D - 256) * par[P_RV_PRE] / 100;                    /* 6 .. 30 ms (never under a chunk: the line is written as it is read) */
    rv_shim = 0.05f + 0.30f * (float)par[P_RV_X] * 0.01f;                  /* the shimmer's feedback */
    rv_gate_hold = (0.08f + 0.32f * (float)par[P_RV_X] * 0.01f) * EN_SR;   /* the gate's hold: 80 .. 400 ms */
}

static void master_dyn_set(void)                /* once a chunk: the settings, from the parameters */
{
    static const float RATIO[7] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f};
    const int r = par[P_MC_RATIO] < 0 ? 0 : par[P_MC_RATIO] > 6 ? 6 : par[P_MC_RATIO];
    cp_slope = r ? 1.0f - 1.0f / RATIO[r] : 0.0f;
    cp_thr = -30.0f + 0.3f * (float)par[P_MC_THR];                 /* -30 .. 0 dB */
    cp_make = fm_db2lin(0.12f * (float)par[P_MC_MAKE]);            /* 0 .. +12 dB */
    cp_ka = 1.0f - fm_expf(-1.0f / (0.004f * EN_SR));
    cp_kr = 1.0f - fm_expf(-1.0f / ((0.06f + 0.04f * cp_env) * EN_SR));     /* the release: ~60 ms, up to 400 ms at 9 dB of reduction (set once a chunk) */
    lk_ceil = fm_db2lin(-12.0f + 0.12f * (float)par[P_ML_CEIL]);   /* -12 .. 0 dB */
}

static void master_dyn_reset(void)              /* a new engine: nothing delayed, no reduction */
{
    uint32_t k;
    for (k = 0; k < LK; k++)
        lk_l[k] = lk_r[k] = 0.0f, lk_gm[k] = 1.0f;
    for (k = 0; k < LKS; k++)
        lk_sub[k] = 1.0f;
    lk_cur = lk_rs = 1.0f;
    lk_sum = (float)LK;
    lk_i = lk_n = lk_si = 0;
    cp_env = gr_hold = pk_hold = 0.0f;
}

static inline void master_dyn(float *pl, float *pr)
{
    float l = *pl, r = *pr, g = 1.0f, p, gq, avg;
    if (cp_slope > 0.0f) {                      /* the compressor */
        const float d = fm_maxf(fm_fabsf(l), fm_fabsf(r)) + 1e-9f, lv = 6.0206f * fm_log2f(d) - cp_thr;
        float gr = 0.0f;
        if (lv > 3.0f)
            gr = cp_slope * lv;
        else if (lv > -3.0f)
            gr = cp_slope * (lv + 3.0f) * (lv + 3.0f) * (1.0f / 12.0f);
        cp_env = fm_flush(cp_env + (gr - cp_env) * (gr > cp_env ? cp_ka : cp_kr));
        g = fm_db2lin(-cp_env);
        en_comp_gr = cp_env;
        l *= g;
        r *= g;
    } else {
        cp_env = 0.0f;
        en_comp_gr = 0.0f;
    }
    l *= cp_make;
    r *= cp_make;
    /* the limiter: the audio goes into the delay, the gain is figured for what is coming */
    p = fm_maxf(fm_fabsf(l), fm_fabsf(r));
    gq = p > lk_ceil ? lk_ceil / p : 1.0f;
    lk_l[lk_i] = l;
    lk_r[lk_i] = r;
    lk_cur = fm_minf(lk_cur, gq);               /* the window's minimum: the sub-block under way, and the eight before */
    {
        float m = lk_cur;
        uint32_t k;
        for (k = 0; k < LKS; k++)
            m = fm_minf(m, lk_sub[k]);
        avg = m;
    }
    lk_sum += avg - lk_gm[lk_i];                /* smoothed over the window */
    lk_gm[lk_i] = avg;
    if (++lk_n >= 8u) {
        lk_n = 0;
        lk_sub[lk_si] = lk_cur;
        lk_si = (lk_si + 1u) & (LKS - 1u);
        lk_cur = 1.0f;
    }
    lk_i = (lk_i + 1u) & (LK - 1u);
    avg = lk_sum * (1.0f / (float)LK);
    if (avg < lk_rs)                            /* quick down, ~60 ms back up */
        lk_rs = avg;
    else
        lk_rs += (avg - lk_rs) * (1.0f / (0.06f * EN_SR));
    *pl = lk_l[lk_i] * lk_rs;                   /* (lk_i is now the oldest: 64 samples ago) */
    *pr = lk_r[lk_i] * lk_rs;
    gr_hold = fm_flush(fm_maxf(gr_hold * 0.9998f, 1.0f - lk_rs));
    pk_hold = fm_flush(fm_maxf(pk_hold * 0.9997f, fm_maxf(fm_fabsf(*pl), fm_fabsf(*pr))));
}

static inline void fx_master(float *pl, float *pr)   /* one sample of the master, before the gain */
{
    float l = *pl, r = *pr;
    if (bmode == BM_LIVE || bmode == BM_STOP || bmode == BM_VAR)
        rb_write(l, r);
    if (bmode != BM_LIVE) {
        float bl = 0.0f, br = 0.0f, w = 1.0f;
        if (bmode == BM_RPT) {
            rb_read(bm_off, &bl, &br);
            w = fm_minf(fm_minf(bm_off, bm_len - bm_off) * (1.0f / 64.0f), 1.0f);   /* 1.5 ms edges */
            if ((bm_off += 1.0f) >= bm_len)
                bm_off -= bm_len;
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
            float y = 0.0f;
            for (f = 0; f < 2; f++) {
                float *s = tk_s[c][f];
                const float *cc = tk_c[f], v3 = in[c] - s[1], v1 = cc[0] * s[0] + cc[1] * v3, v2 = s[1] + cc[1] * s[0] + cc[2] * v3;
                s[0] = fm_flush(2.0f * v1 - s[0]);
                s[1] = fm_flush(2.0f * v2 - s[1]);
                y += cc[3] * v1 * (f ? 0.7f : 1.0f);
            }
            o[c] = y * 1.8f;
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
            float *s = wh_s[c];
            const float v3 = in[c] - s[1], v1 = wh_c[0] * s[0] + wh_c[1] * v3, v2 = s[1] + wh_c[1] * s[0] + wh_c[2] * v3;
            s[0] = fm_flush(2.0f * v1 - s[0]);
            s[1] = fm_flush(2.0f * v2 - s[1]);
            o[c] = wh_c[3] * v1 * 2.0f;
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
                y = soft_sat(tp_lp[c] * tp_drive) * (1.0f / tp_drive);   /* and saturation (a rational tanh: one division) */
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
                in[c] = (in[c] + fl_wet * y * fl_m) * (1.0f / (1.0f + 0.85f * fl_wet * fl_m));
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
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
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
        case C_SECTION:
            if (a == EN_CMD_BREAK && sec != SEC_GROOVE && sec != SEC_BUILD)
                break;                          /* already down */
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
        case C_TRK:
            trk_mute = (uint8_t)(b & 0xFF);
            trk_solo = (uint8_t)(b >> 8 & 0xFF);
            break;
        case C_GATEPAT: gx_pat = (uint16_t)b; break;
        case C_FILLQ: fill_q = 1; break;
        case C_FLXH: flx_phys = (uint8_t)a; break;
        case C_SHED: en_shed_level = (uint8_t)a; break;
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
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static float padl[BLK] EN_POOL, padr[BLK] EN_POOL;
static float plp[4];
static float pad_ol[BLK] EN_POOL, pad_or[BLK] EN_POOL, pad_rs[BLK] EN_POOL, pad_es[BLK] EN_POOL;   /* the pad's contribution to the mix, the reverb send and the echo send */
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
        pad_rs[j] = (l + r) * 0.6f;
        pad_es[j] = (l + r) * 0.2f;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

static void pad_chorus(uint32_t n)
{
    const float a = en_shed_level ? 0.0f : (float)par[P_PL_CHO] * 0.01f * 0.55f;   /* (no chorus when the load is too high) */
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
                f = rp - (float)i0;
                wl += ch_buf[0][i0] + (ch_buf[0][i0 + 1 >= CH_N ? 0 : i0 + 1] - ch_buf[0][i0]) * f;
                rp = (float)ch_i - d2;
                if (rp < 0.0f)
                    rp += (float)CH_N;
                i0 = (int)rp;
                f = rp - (float)i0;
                wr += ch_buf[1][i0] + (ch_buf[1][i0 + 1 >= CH_N ? 0 : i0 + 1] - ch_buf[1][i0]) * f;
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
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    const float g = pad_vol * 0.5f * pad_lvl * (1.0f + 0.9f * fm_maxf(brk, 0.0f)) * PAD_SOUND[pad_snd()].g *   /* the pad rises in a breakdown */
                    (1.0f + 0.35f * (float)par[P_LK_SNARE] * 0.01f * pad_lk);                    /* and swells with the links */
    uint32_t nb = (n + 31u) >> 5;
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 5.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], amp = v->amp, s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2], s3 = v->ic[3],
              gl, gr, pn, tgt = 0.0f, idx = 0.0f, se = stab_env, ar = 0.01f;
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
            if (!(j & 31u)) {
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
            }
            amp += (tgt - amp) * ar;
            switch (v->voices) {
            case PO_RHODES: {                    /* FM, the modulator at the carrier's pitch: a Rhodes */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f;   /* one carrier: no beating */
                break;
            }
            case PO_STRINGS:                     /* two saws, detuned (Detune) */
                x = 0.62f * (blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[1]));
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
            case PO_SUPER: {                     /* two saws detuned and one an octave up: a supersaw */
                float p2 = 2.0f * p0;
                p2 -= (float)(int)p2;
                x = 0.5f * (blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[1])) + 0.3f * blep_saw(p2, 2.0f * v->inc[0]);
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

static void render_bass(uint32_t n)
{
    uint32_t j;
    const drone_snd_t *DS = &DRONE_SOUND[(unsigned)par[P_DR_INST] < EN_NSOUND ? par[P_DR_INST] : 0];
    const int dest = par[P_DL_DEST], osc = DS->osc;
    const float depth = (float)par[P_DL_DEPTH] * 0.01f, lv = dest == 2 ? 1.0f - 0.8f * depth * (0.5f - 0.5f * drone_lfo.v) : 1.0f,
                drv = 1.4f * DS->drive * (dest == 3 ? 1.0f + 2.5f * depth * (0.5f + 0.5f * drone_lfo.v) : 1.0f),
                g = bass_vol * 0.36f * lv * DS->g / fm_minf(1.0f + 0.15f * (DS->drive - 1.0f), 1.3f),
                dsaw = DS->saw, dsub = DS->sub,
                tgt = (en_playing && !par[P_DR_MAN]) || bass.play ? 1.0f : 0.0f,
                rate = tgt > 0.0f ? 1.0f / ((bass.play ? 0.03f : 2.0f) * EN_SR) : 1.0f / ((par[P_DR_MAN] ? 0.3f : 4.0f) * EN_SR),
                fmi = 0.5f + 2.2f * bass.env + 0.3f * arcm, pw = 0.5f + 0.3f * mod[9].v;
    float p0 = bass.ph[0], p1 = bass.ph[1], p2 = bass.ph[2], sp = bass.sph, mp = bass.mph, amp = bass.amp, f, i0, i1, i2, mi;
    if (amp < 1e-5f && !en_playing && !bass.play) {
        bass.amp = 0.0f;
        en_bass_level = 0.0f;
        return;
    }
    bass.note += (bass.target - bass.note) * fm_minf((float)n * (bass.play ? GLIDE_K * 12.0f : GLIDE_K), 1.0f);
    f = mtof(bass.note + (dest == 1 ? depth * drone_lfo.v : 0.0f));   /* the LFO on the pitch: a semitone at most */
    i0 = f * (osc == DO_FIFTHS ? 1.49831f : osc == DO_REESE || osc == DO_HOOVER ? 0.9862f : 0.99481f) *
         (1.0f / EN_SR);                        /* -9, 0, +9 cents (Reese and Hoover: 24; Fifths: a fifth) */
    i1 = f * (1.0f / EN_SR);
    i2 = f * (osc == DO_REESE || osc == DO_HOOVER ? 1.0140f : 1.00521f) * (1.0f / EN_SR);
    mi = osc == DO_METAL ? i1 * 1.4142f : osc == DO_FMR ? i1 * DRONE_FR[(unsigned)par[P_DR_INST] < EN_NSOUND ? par[P_DR_INST] : 0] : i1 * (1.5f + 2.5f * bass.env);   /* Metal's and FMR's modulator; Sync's slave */
    for (j = 0; j < n; j++) {
        float x;
        if (!(j & 31u)) {                       /* the filter, every 32 samples */
            float oct, fc;
            oct = (float)par[P_DR_CUT] * 0.075f - 1.5f + DS->cut + (dest == 0 ? 2.5f * depth * drone_lfo.v : 0.0f) +
                  4.0f * (float)par[P_DE_AMT] * 0.01f * bass.env + 0.8f * motion * mod[9].v + 0.8f * arcm -
                  2.0f * fm_maxf(brk, 0.0f) - 1.6f * fm_minf(brk, 0.0f) - 0.35f * duck_env * pulse_vol +
                  4.0f * (float)par[P_LK_KICK] * 0.01f * lk_kick;   /* the links: the kick opens it */
            fc = fm_clampf(f * 2.0f * fm_exp2f(oct), 45.0f, 9000.0f);
            svf_coefs(bass.c1, fc, reso_k);
            svf_coefs(bass.c2, fc * 1.3f, 1.4f);
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
            x = sine_any(p1 + (0.3f + 1.6f * bass.env) * sine(mp)) * dsaw * 2.0f;
            break;
        case DO_SYNC:                           /* a saw synced to the note, the envelope sweeping it */
            x = blep_saw(mp, mi) * dsaw * 1.3f;
            break;
        default:                                /* three saws a few cents apart */
            x = (blep_saw(p0, i0) + blep_saw(p1, i1) + blep_saw(p2, i2)) * dsaw;
            break;
        }
        x += dsub * sine(sp);
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
        x = svf_lp(x, bass.c1, &bass.s[0]);
        x = svf_lp(x, bass.c2, &bass.s[2]);
        amp += (tgt - amp) * rate;
        x = fm_tanhf(x * drv) * amp * g * TG(TR_DRONE, j);
        mixl[j] += x;
        mixr[j] += x;
        rsend[j] += x * 0.06f;
        esend[j] += x * 0.04f;
    }
    for (j = 0; j < 4; j++)
        bass.s[j] = fm_flush(bass.s[j]);
    bass.ph[0] = p0;
    bass.ph[1] = p1;
    bass.ph[2] = p2;
    bass.sph = sp;
    bass.mph = mp;
    bass.amp = amp;
    en_bass_level = amp;
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
            float a, y = dj_voice(d, &a);
            const int sustained = d->type == DJ_HORN || d->type == DJ_SIREN || d->type == DJ_DUB || d->type == DJ_RISER || d->type == DJ_ROLL;
            /* a sustained sound: while its key is down (held mode), else for a second or so; the others play out */
            const int gate = !sustained || (d->held && d->hold_mode) || (!d->hold_mode && d->t < 1.2f * EN_SR);
            d->env += ((gate ? 1.0f : 0.0f) - d->env) * (gate ? 0.01f : 0.00025f);
            y = y * a * d->env * gain * TG(TR_ACID, j);
            mixl[j] += y;
            mixr[j] += y;
            rsend[j] += y * ES[ec][0];
            esend[j] += y * ES[ec][1];
            if (sustained && !gate && d->env < 0.003f)
                d->on = 0;
            d->z = fm_flush(d->z);
            d->z2 = fm_flush(d->z2);
        }
        live += d->on;
    }
    en_smp_active = (uint8_t)live;
}

/* the DX voices (the pluck's sounds from CM_NPLAIN): a stage of the chunk's job, with the pad's, into their own buffers */
static void render_fm(uint32_t n)
{
    const int on = par[P_CM_SND] >= CM_NPLAIN && cm_vol > 0.0f;
    const float g = cm_vol * 0.5f * FM_OUT * (float)FM_GAIN[(on ? par[P_CM_SND] - CM_NPLAIN : 0)];
    uint32_t j;
    float lvl = 0.0f;
    if (!on) {
        for (j = 0; j < n; j++)
            fm_ol[j] = fm_or[j] = fm_rs[j] = fm_es[j] = 0.0f;
        fm_ctr += n;
        return;
    }
    for (j = 0; j < n; j++) {
        float x = fm_fifo[(fm_ctr - FM_D + j) & (FM_R - 1)];   /* (rendered a chunk ago, by the other core) */
        const float y = (x < -16.0f ? -16.0f : x > 16.0f ? 16.0f : x) * g * TG(TR_CM, j);
        fm_ol[j] = y;
        fm_or[j] = y;
        fm_rs[j] = y * 0.35f;
        fm_es[j] = y * 0.5f;
        lvl = fm_maxf(lvl, y < 0.0f ? -y : y);
    }
    en_cm_level = fm_minf(lvl * 4.0f, 1.0f);
    fm_ctr += n;
}

static void render_cm(uint32_t n)
{
    const int sn = par[P_CM_SND] < 0 ? 0 : par[P_CM_SND] >= CM_NPLAIN ? CM_NPLAIN - 1 : par[P_CM_SND];
    const cm_snd_t *S = &CM_SOUND[sn];
    const float g = cm_vol * 0.5f * S->g;
    float lvl = 0.0f;
    int v;
    uint32_t j;
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
            cm[v].ph += cm[v].inc;
            if (cm[v].ph >= 1.0f)
                cm[v].ph -= 1.0f;
            cm[v].ps += cm[v].inc * S->fr;
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

static void render_acid(uint32_t n)
{
    uint32_t j;
    const acid_snd_t *AS = &ACID_SOUND[(unsigned)par[P_AC_WAVE] < EN_NACID ? par[P_AC_WAVE] : 0];
    const int osc = AS->osc;
    const float g = acid_vol * 0.75f * AS->g, adrv = AS->drive, res = (float)par[P_AC_RES] * 0.01f, envmod = (float)par[P_AC_ENV] * 0.06f,
                accamt = (float)par[P_AC_ACC] * 0.01f,
                base = (float)par[P_AC_CUT] * 0.066f, k1 = 1.45f - 1.38f * res,
                vel = 0.75f + 0.5f * accamt * acid.acc, glide = acid.sliding ? 0.0012f : 1.0f;
    float ph = acid.ph, inc = acid.inc, amp = acid.amp, lvl = 0.0f, ps = acid.ps, p3 = acid.p3, p4 = acid.p4, p5 = acid.p5, sratio = 1.0f, pw = 0.5f, lz = 1.0f;
    if (amp < 1e-5f && acid.amp_t <= 0.0f) {
        acid.amp = 0.0f;
        en_acid_level = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x, y;
        if (!(j & 31u)) {                       /* the cutoff: its knob, the envelope, an accent's push */
            float oct = base + envmod * acid.env * (1.0f + accamt * acid.acc) + 1.2f * accamt * acid.acc * acid.env;
            float fc = fm_clampf(60.0f * fm_exp2f(oct), 50.0f, 14000.0f);
            svf_coefs(acid.c1, fc, k1);
            svf_coefs(acid.c2, fc * 1.15f, 1.2f);
            sratio = 1.6f + 2.6f * acid.env;    /* Sync: the slave's pitch, swept by the envelope */
            acid.lph += 32.0f * 0.35f / EN_SR;  /* PWM: its width, 0.35 Hz */
            if (acid.lph >= 1.0f)
                acid.lph -= 1.0f;
            pw = 0.5f + 0.38f * sine(acid.lph);
            lz = 1.0f + 7.0f * acid.env * acid.env;   /* Laser: its pitch, falling as the envelope does */
            acid.env *= acid.edec;
        }
        if (acid.gate_left > 0 && --acid.gate_left == 0)
            acid.amp_t = 0.0f;                  /* the gate closes */
        inc += (acid.inc_t - inc) * glide;      /* a slide: ~50 ms */
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
            x = blep_saw(ph, inc) + 0.7f * (ps < 0.5f ? 1.0f : -1.0f);
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
            x = 0.55f * (blep_saw(ph, inc) + blep_saw(ps, inc * 1.0041f)) + 0.6f * (p3 < 0.4f ? 1.0f : -1.0f);
            ps += inc * 1.0041f;
            p3 += inc * 0.5f;
            if (ps >= 1.0f)
                ps -= (float)(int)ps;
            if (p3 >= 1.0f)
                p3 -= 1.0f;
            break;
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
        y = svf_lp(x, acid.c1, &acid.s[0]);
        y = fm_tanhf(y * (1.6f + 1.5f * res) * adrv) / fm_minf(adrv, 1.3f);   /* the squelch: driven between the two */
        y = svf_lp(y, acid.c2, &acid.s[2]);
        amp += (acid.amp_t - amp) * 0.006f;     /* ~4 ms edges */
        y *= amp * vel * g * TG(TR_ACID, j);
        mixl[j] += y;
        mixr[j] += y;
        rsend[j] += y * 0.15f;
        esend[j] += y * 0.35f;
        lvl = fm_maxf(lvl, amp);
    }
    for (j = 0; j < 4; j++)
        acid.s[j] = fm_flush(acid.s[j]);
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
    h = (z->rng ^ z->rng >> 15) * 2246822519u;
    if (vn_hiss > 0.0f) {                       /* the hiss: pink (Paul Kellet's economy filter), only if it is wanted */
        z->b0 = 0.99765f * z->b0 + w * 0.0990460f;
        z->b1 = 0.96300f * z->b1 + w * 0.2965164f;
        z->b2 = 0.57000f * z->b2 + w * 1.0526913f;
        pk = (z->b0 + z->b1 + z->b2 + w * 0.1848f) * 0.22f * vn_hiss;
    }
    if ((float)(h >> 8) * (1.0f / 16777216.0f) < vn_pop) {   /* a tick: a burst of noise that dies in a fraction of a millisecond or two */
        float a;
        h = (h ^ h >> 13) * 3266489917u;        /* another scramble: its size, not the number that chose it */
        a = (float)(h >> 8) * (1.0f / 16777216.0f);          /* 0 .. 1 */
        z->crack = (0.1f + 1.2f * a * a) * vn_amp;           /* most small, now and then a loud one */
        z->r1 = 0.90f + 0.075f * a;             /* its decay: 0.25 ms (small) to 1.5 ms (big) */
        if (a > 0.85f)
            z->y2 += 0.45f * vn_amp * (a - 0.85f) * 6.0f;    /* a loud one brings a dull thud with it */
    } else {
        h = (h ^ h >> 13) * 3266489917u;
        if ((float)(h >> 8) * (1.0f / 16777216.0f) < 5.0f * vn_pop)   /* and the fine dust between: five times as many, very small */
            z->r2 = 0.09f * vn_amp;
    }
    d = (w - 0.5f * z->pw) * 0.5f;              /* white, the lows only eased (not taken out: that was piercing): a tick with a little body */
    o = d * (z->crack + z->r2) + z->y2 + pk;
    z->crack *= z->r1;
    z->r2 *= 0.80f;
    z->y2 *= 0.9965f;
    z->pw = w;
    return o;
}

static float nzl[BLK], nzr[BLK];
static void render_noise(uint32_t n)            /* into its own buffer: the pump ducks it on its own */
{
    uint32_t j;
    const float g = vin_m * vn_g, k = nz_lp_k;   /* the vinyl key fades it in; its settings are en_fxs's */
    if (g <= 0.0f || !en_playing) {
        for (j = 0; j < n; j++)
            nzl[j] = nzr[j] = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float l = nz_sample(&nz[0], nz_color), r = nz_sample(&nz[1], nz_color), tg = TG(TR_PAD, j) * g;
        nz[0].lp += k * (l - nz[0].lp);         /* Tone: a low-pass, 300 Hz .. 19 kHz */
        nz[1].lp += k * (r - nz[1].lp);
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
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
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
        x = hp * amp * att * g * TG(TR_HATS, j);
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
    v->on = amp > 0.0005f;
}

/* the clap: noise through a band-pass, three bursts 8 ms apart, the last with a tail */
static void render_clap(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float s1 = v->x1, s2 = v->x2, amp = v->amp, t = v->t, ph = v->ph;
    const float gap = KS->cl_gap;
    if (!v->on)
        return;
    for (j = 0; j < n; j++) {
        float in, bp, x;
        if (t == gap || t == 2.0f * gap)
            amp = v->pend;                      /* the next burst */
        noise = noise * 1664525u + 1013904223u;
        in = (float)(int32_t)noise * (1.0f / 2147483648.0f);
        bp = in - s2 - 0.7f * s1;               /* Chamberlin SVF: the band-pass is s1 */
        s1 += v->hp * bp;
        s2 += v->hp * s1;
        x = (s1 * 1.6f + 0.35f * sine(ph) * fm_maxf(1.0f - t * (1.0f / 2500.0f), 0.0f)) * amp * g * TG(TR_SNARE, j);
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
    v->on = t < 2.0f * gap || amp > 0.0005f;
}

/* the snare: two tuned sines falling quickly, and noise through a band-pass decaying more slowly */
static void render_snare(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float s1 = v->x1, s2 = v->x2, amp = v->amp, ph = v->ph, ph2 = v->ph2, att = v->att, body = v->idx;
    if (!v->on)
        return;
    if (v->t == 0.0f)
        body = 1.0f;
    for (j = 0; j < n; j++) {
        float in, bp, x;
        noise = noise * 1664525u + 1013904223u;
        in = (float)(int32_t)noise * (1.0f / 2147483648.0f);
        bp = in - s2 - 0.5f * s1;
        s1 += v->hp * bp;
        s2 += v->hp * s1;
        x = ((sine(ph) + 0.6f * sine(ph2)) * body * KS->sn_body + s1 * KS->sn_noise) * amp * att * g * TG(TR_SNARE, j);
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
    v->on = amp > 0.0005f;
}

/* the perc: an FM blip (the modulator at 1.48 times, its index falling), the pitch falling into place */
static void render_perc(drum_t *v, uint32_t n, float g)
{
    uint32_t j;
    float ph = v->ph, ph2 = v->ph2, inc = v->inc, amp = v->amp, idx = v->idx, att = v->att;
    if (!v->on)
        return;
    for (j = 0; j < n; j++) {
        float m = sine(ph2) * idx, x = sine(ph + m + 4.0f) * amp * att * g * TG(TR_PERC, j);
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
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.1f;
    drum_t *v;
    v = &drum[L_KICK];                           /* kick: a sine falling fast to its note, a click, driven */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att, ck = v->idx, gk = g * lane_g[L_KICK] * 1.15f;
        float r1 = v->x1, r2 = v->x2, rl1 = v->mph[0], rl2 = v->mph[1], rp = v->ph2;
        const float dk = drive_k * KK->k_drive, drop = KK->k_drop, cka = KK->k_click, rum = KK->k_rumble;
        const float norm = 0.85f / fm_minf(1.0f + 0.25f * (dk - 1.0f), 1.6f);
        for (j = 0; j < n; j++) {
            float x, nz;
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
            x = fm_tanhf(x * 1.3f * dk) * norm * gk * TG(TR_KICK, j);
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
        v->on = amp > 0.0005f || r1 - r2 > 0.002f;
    }
    render_snare(&drum[L_SNARE], n, g * 0.38f * lane_g[L_SNARE]);
    render_clap(&drum[L_CLAP], n, g * 0.42f * lane_g[L_CLAP]);
    render_hat(&drum[L_HAT], n, g * 0.22f * lane_g[L_HAT], 0.75f, 1.0f);    /* a little right */
    render_hat(&drum[L_OHAT], n, g * 0.18f * lane_g[L_OHAT], 1.0f, 0.75f);  /* a little left */
    render_perc(&drum[L_PERC], n, g * 0.3f * lane_g[L_PERC]);
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
static void mx_begin(int t, uint32_t n)
{
    uint32_t j;
    if (!mx_off(t))
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

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm) * (1.0f + 0.7f * fm_maxf(brk, 0.0f)), eg = echo_send;
    float dk[BLK], de = duck_env, gg = gate_g;
    const float depth = fm_minf(0.55f * KK->duck, 0.85f) * fm_minf(pulse_vol * 1.5f, 1.0f) * trk_g1[TR_KICK];   /* a muted kick ducks nothing */
    const int gate = fx_on >> FX_GATE16 & 1u ? 1 : fx_on >> FX_GATE8 & 1u ? 2 : 0;
    const int odd = (int)((sixteenth - 1u) & 1u);         /* the sixteenth playing now: the second of an eighth? */
    int rv_c2;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    {   /* the tank for this chunk starts now (it reads input older than this chunk's), here or on the other core */
        const dl_t *pd = &dl[L_PRE];
        rv_job.n = (int)n;
        rv_job.r0 = pd->i >= rv_pre ? pd->i - rv_pre : pd->i - rv_pre + pd->n;
        rv_job.fm_on = par[P_CM_SND] >= CM_NPLAIN && cm_vol > 0.0f;
        rv_job.fm_w = fm_ctr & (FM_R - 1);
        pad_lk = lk_snare;
        stage_done = 0;
        rv_c2 = c2_submit && c2_submit(chunk_job, &rv_job);
        if (rv_c2)
            en_c2_jobs++;
        else
            chunk_job(&rv_job);
    }
    mx_begin(1, n);
    render_bass(n);
    mx_end(1, n);
    mx_begin(2, n);
    render_acid(n);
    mx_end(2, n);
    mx_begin(3, n);
    render_cm(n);
    render_fm(n);                               /* the DX voices' samples from a chunk ago (the other core's), into the fm_ buffers added below */
    mx_end(3, n);
    render_dj(n);
    render_noise(n);
    if (rv_c2) {                                /* the pad is the other core's: wait for it (the reverb goes on behind) */
        while (!stage_done)
            if (c2_alive && !c2_alive()) {
                rv_c2 = 0;
                c2_join();                      /* retires it */
                en_c2_lost++;
                for (j = 0; j < n; j++)
                    padl[j] = padr[j] = pad_ol[j] = pad_or[j] = pad_rs[j] = pad_es[j] = fm_ol[j] = fm_or[j] = fm_rs[j] = fm_es[j] = rv_wl[j] = rv_wr[j] = 0.0f;
                plate_init();
                fm_reset();                      /* (its voices were the other core's too) */
                for (j = 0; j < FM_R; j++)
                    fm_fifo[j] = 0.0f;
                break;
            }
        if (c2_fence)
            c2_fence();
    }
    {
        float pl, pr, ps, fl, fr, fs;
        mx_gains(0, &pl, &pr, &ps);
        mx_gains(3, &fl, &fr, &fs);
        for (j = 0; j < n; j++) {
            mixl[j] += pad_ol[j] * pl + fm_ol[j] * fl;
            mixr[j] += pad_or[j] * pr + fm_or[j] * fr;
            rsend[j] += pad_rs[j] * ps + fm_rs[j] * fs;
            esend[j] += pad_es[j] * ps + fm_es[j] * fs;
        }
    }
    pad_midi_flush();
    for (j = 0; j < n; j++) {                   /* the sidechain: the kick pushes the rest down, briefly */
        float gt = 1.0f;
        if (gate == 1)                          /* the gate: the first half of each sixteenth */
            gt = (float)(step_t + (int32_t)j) < 0.5f * step_len ? 1.0f : 0.0f;
        else if (gate == 2)                     /* or of each eighth */
            gt = odd ? 0.0f : 1.0f;

        gg += (gt - gg) * 0.04f;
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
    gate_g = gg;
    mx_begin(4, n);
    if (KK->crush > 0.0f) {                      /* breakcore: the drums crushed (what they add, held and stepped) */
        static float hl, hr;
        static uint32_t hc;
        const uint32_t hold = 2u + (uint32_t)(KK->crush * 4.0f);
        const float c = KK->crush, q = 24.0f;
        static float bl[BLK], br[BLK];      /* static: the audio interrupt's stack is small (and this never re-enters) */
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
        render_drums(n);                        /* (the kick sets duck_env for the next chunk) */
    }
    mx_end(4, n);
    duck_env = fm_flush(de);
    for (j = 0; j < n; j++) {                   /* the echoes; the reverb's input into its line; the dry part of the mix */
        float el, er, xl, xr;
        echoes(esend[j] * eg + throw_amt * 0.5f * (mixl[j] + mixr[j]), &el, &er, &xl, &xr);
        push(&dl[L_PRE], (rsend[j] * rg + (el + er + xl + xr) * 0.35f * rev_send + wash_e * 0.2f * (mixl[j] + mixr[j])) * 0.8f);
        mixl[j] = (mixl[j] + (0.75f * el + 0.15f * xl) * dk[j]) * 0.36f;
        mixr[j] = (mixr[j] + (0.75f * er + 0.15f * xr) * dk[j]) * 0.36f;
        dk[j] *= 1.3f * 0.36f;                  /* what the tank's output is scaled by */
    }
    if (rv_c2 && !c2_join()) {                  /* the other core was lost: this chunk has no reverb, and the tank starts over */
        en_c2_lost++;
        for (j = 0; j < n; j++)
            rv_wl[j] = rv_wr[j] = 0.0f;
        plate_init();
    }
    for (j = 0; j < n; j++) {
        float l = mixl[j] + dk[j] * rv_wl[j], r = mixr[j] + dk[j] * rv_wr[j];
        fx_master(&l, &r);
        master_dyn(&l, &r);
        out[2 * j] = (int32_t)(fm_tanhf(l * gain) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r * gain) * 8300000.0f);
    }
    step_t += (int32_t)n;
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    heads_step(n);
    fx_control(n);
    if (par[P_CM_SND] >= CM_NPLAIN)             /* the DX voices: the patch, the tune, the keys' time */
        fm_set_patch(par[P_CM_SND] - CM_NPLAIN);
    fm_tune = par[P_TUNE] * 13981;
    {   /* how long a DX note may ring on after its key is up: 0.35 s .. 2.5 s by the pluck's Decay (a bell's tail would go on for many) */
        const float d = (float)par[P_CM_DEC] * 0.01f;
        fm_gates(n, (int32_t)((0.35f + 2.2f * d * d) * EN_SR));
    }
    master_dyn_set();
    reverb_set();
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
    if (en_playing && !en_frozen && !change_pending) {
        drift_left -= (float)n;
        if (drift_left <= 0.0f)
            change_pending = 1;
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
        lfo_step(&drone_lfo, par[P_DL_RATE], par[P_DL_SHAPE], (float)n, step_len);
        pad_filt = par[P_PL_DEST] == 0 ? d * pad_lfo.v : 0.0f;
        pad_lvl = par[P_PL_DEST] == 1 ? 1.0f - 0.85f * d * (0.5f - 0.5f * pad_lfo.v) : 1.0f;
        pad_pan = par[P_PL_DEST] == 2 ? 0.9f * d * pad_lfo.v : 0.0f;
        pad_pitch = par[P_PL_DEST] == 3 ? 0.5f * d * pad_lfo.v : 0.0f;
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

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    float gain;
    int32_t *const out0 = out;
    const uint32_t n0 = n;
    drain();
    if (hush && hush_g <= 0.0f) {               /* hushed: the music holds still (the song resumes where it was) */
        for (n = 0; n < 2 * n0; n++)
            out0[n] = 0;
        en_hushed = 1;
        return;
    }
    gain = muted ? 0.0f : (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
    if (hush || hush_g < 1.0f) {                /* the hush: ~6 ms each way */
        const float step = 1.0f / (0.006f * EN_SR), tgt = hush ? 0.0f : 1.0f;
        uint32_t j;
        for (j = 0; j < n0; j++) {
            hush_g = tgt > hush_g ? fm_minf(hush_g + step, 1.0f) : fm_maxf(hush_g - step, 0.0f);
            out0[2 * j] = (int32_t)((float)out0[2 * j] * hush_g);
            out0[2 * j + 1] = (int32_t)((float)out0[2 * j + 1] * hush_g);
        }
    }
    en_hushed = (uint8_t)(hush && hush_g <= 0.0f);
}
