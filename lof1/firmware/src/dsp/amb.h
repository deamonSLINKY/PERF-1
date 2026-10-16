/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: LOF-1, a lofi groovebox for the FM-1 (a RedtailWorks firmware, built on ECO-1 and AMB-1).
 * You play the lead on the keys; a tired jazz band follows you.
 *
 *   HARMONY  you set the key and scale; the engine hears which chord your notes imply (held notes count
 *            most, then the last few played, the current chord a little sticky) and changes on the beat.
 *            With no notes it keeps cycling a progression in the key. REC holds the chord.
 *   LEAD     the keys (and MIDI in), on one of fourteen instruments, sampled or FM.
 *   DAYDREAM when the keys have been still for a while, a ghost takes the lead with phrases of its own;
 *            a key takes it straight back.
 *   COMP     the chord, re-voiced on each change: held, stabs, offbeats or an arpeggio; rolled like a
 *            tired pianist.
 *   BASS     the chord's roots: plain, root and fifth, walking, or dub.
 *   ANSWER   call and response: when you rest after a phrase it answers, inventing or echoing yours.
 *   DRUMS    a sixteen-step sequencer, eight lanes, eight patterns, swing; the Dusty kit (samples) or the
 *            Tape kit (synthesized).
 *   DUST     the record or tape under it all: crackle and pops, hiss and hum, and a granular haze of the
 *            music itself; VINYL, CASSETTE, REEL or RADIO.
 *   SPACE    tape (wow, flutter, saturation), Mellow (a master low-pass), Room (Dattorro's plate), Echo.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f

/* scales: the melody's notes, relative to the root (bit i: root + i semitones); chords come from the
 * scale's seven-note parent (a pentatonic's major or minor, the blues' Dorian) */
enum { SC_MAJOR, SC_MINOR, SC_DORIAN, SC_MIXO, SC_PENT_MIN, SC_PENT_MAJ, SC_BLUES, EN_NSCALES };
extern const char *const EN_SCALE_NAME[EN_NSCALES];
extern const uint16_t EN_SCALE_POOL[EN_NSCALES];
extern const char *const EN_NOTE_NAME[12];

/* the instruments: one table, each voice offers some of them (EN_*_INST lists) */
enum {
    I_RHODES, I_WURLI, I_UPRIGHT, I_UPRIGHT2, I_VIBES, I_KALIMBA, I_GLOCK, I_STRUM, I_MUTETPT, I_TENORSAX,
    I_FLUTE, I_HARMONICA, I_BELL, I_SINE, I_PAD, I_ORGAN, I_STRINGS, I_UPBASS, I_SUB, I_ROUND, I_PLUCKB, I_MOOG,
    I_WHISTLE, I_SQUARE, EN_NINST
};
extern const char *const EN_INST_NAME[EN_NINST];
#define EN_NLEAD 14
#define EN_NCOMP 9
#define EN_NBASS 5
#define EN_NANS 10
extern const uint8_t EN_LEAD_INST[EN_NLEAD], EN_COMP_INST[EN_NCOMP], EN_BASS_INST[EN_NBASS], EN_ANS_INST[EN_NANS];

enum { DUST_VINYL, DUST_CASSETTE, DUST_REEL, DUST_RADIO, EN_NDUST };
extern const char *const EN_DUST_NAME[EN_NDUST];
#define EN_NKIT 2                              /* Dusty (samples), Tape (synthesized) */
extern const char *const EN_KIT_NAME[EN_NKIT];

/* the drums: eight lanes, sixteen steps, eight patterns (the project keeps them) */
#define EN_NLANE 8
#define EN_NSTEP 16
#define EN_NPAT 8
enum { LN_KICK, LN_SNARE, LN_RIM, LN_CLAP, LN_HAT, LN_OHAT, LN_SHAKE, LN_PERC };
extern const char *const EN_LANE_NAME[EN_NLANE];
extern uint16_t en_pats[EN_NPAT][EN_NLANE];    /* bit s: a hit on step s (UI edits, render reads) */
void en_pats_default(void);                    /* the factory patterns into en_pats */

/* parameters: value ranges in en_param_info() */
enum {
    P_TEMPO, P_SWING, P_JAZZ, P_BUSY,                         /* Groove */
    P_LEAD_LVL, P_LEAD_TONE, P_LEAD_REL, P_LAZY,              /* Lead */
    P_DD_WAIT, P_DD_DENS, P_DD_RANGE, P_DD_MOOD,              /* Daydream */
    P_COMP_LVL, P_COMP_TONE, P_VOICING, P_ROLL,               /* Comp */
    P_COMP_PAT, P_COMP_LEN, P_PUSH, P_SPREAD,                 /* Comp style */
    P_BASS_LVL, P_BASS_TONE, P_BASS_PAT, P_SLIDE,             /* Bass */
    P_ANS_LVL, P_ANS_TONE, P_CHATTY, P_ECHO_PH,               /* Answer */
    P_DRUM_LVL, P_CRUNCH, P_PATTERN, P_FILL,                  /* Drums */
    P_KICK, P_SNARE, P_HATS, P_PERC,                          /* Kit */
    P_DUST_LVL, P_AGE, P_SURFACE, P_HISS,                     /* Dust */
    P_GRAIN, P_SMEAR, P_DUCK, P_HUM,                          /* Dust 2 */
    P_TAPE, P_MELLOW, P_ROOM, P_ECHO,                         /* Space */
    P_ROOT, P_SCALE, P_SNAP, P_MIDI,                          /* Global (SNAP: the keys play Chromatic, Scale or Chords) */
    P_LEAD_INST, P_COMP_INST, P_BASS_INST, P_ANS_INST,        /* on ALGORITHM: index into the voice's list */
    P_KIT, P_DUST_CHAR,                                       /* on ALGORITHM */
    P_LEAD_OCT, P_DD_ON, P_ANS_ON, P_TUNE,                    /* OCT- / OCT+; SEL + ENV; SEL + ARP; tune */
    P_DD_FILL, P_DD_LEGATO, P_DD_REPEAT, P_DD_DYN,            /* Daydream 2: how much of the time the ghost plays,
                                                               * its note lengths, its motifs, its accents */
    P_DRIFT, P_DRIFT_DRUMS, P_DRIFT_POOL, P_DRIFT_CHORDS,     /* Groove 2: every Drift bars the music changes: the
                                                               * drums to another pattern (Next or Random, from
                                                               * patterns 1..Pool), the progression to its other */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */
enum { EN_MIDI_OFF, EN_MIDI_OUT, EN_MIDI_CLOCK, EN_MIDI_BOTH };   /* P_MIDI: notes out, clock in */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);                          /* play / stop (VINYL: the needle drop, the run-out) */
void en_note(int note, int on, int vel);       /* the lead: MIDI note (before Snap and the lead's octave) */
void en_hold(int on);                          /* REC: the chord holds */
void en_side_b(void);                          /* SAVE: a breakdown on the next bar; again, back in with a fill */
void en_mute(int on);                          /* remote: silent and no MIDI out (1), or back (0) */
void en_mutate(void);                          /* SEL + SEQ: a change now, on the next bar (as Drift's, with a fill) */
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new tape: the progression from the top */
void en_fx(uint32_t mask);                     /* the held performance effects (FX_*) */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);                        /* MIDI clock in (P_MIDI Clock or Both) */

/* performance effects (FX on): the keys, one bit each */
enum {
    FX_ST4, FX_ST8, FX_SKIP, FX_HALF, FX_STOP, FX_SPIN,      /* white 1-6 (SKIP: a record skipping) */
    FX_MUFFLE, FX_RADIO,                                     /* white 7-8 */
    FX_MUTE0, FX_MUTE1, FX_MUTE2, FX_MUTE3, FX_MUTE4, FX_MUTE5,   /* white 9-14: lead comp bass answer drums dust */
    FX_ROOM, FX_ECHO,                                        /* white 15-16: throws */
    FX_WOBBLE, FX_CRUSH, FX_FILL, FX_FREEZE, FX_DROP,        /* black 1-5 */
    FX_SOLO0, FX_SOLO1, FX_SOLO2, FX_SOLO3, FX_SOLO4, FX_SOLO5,   /* black 6-11 */
    EN_NFX
};
extern const char *const EN_FX_NAME[EN_NFX];
#define EN_NTRACK 6
enum { T_LEAD, T_COMP, T_BASS, T_ANS, T_DRUMS, T_DUST };   /* the tracks */
extern const char *const EN_TRACK_NAME[EN_NTRACK];

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
/* the voices apart (the browser emulator's channels): with en_bus_on, each en_render leaves EN_NBUS floats
 * a frame in en_bus: lead, comp, bass, answer, drums, dust */
#define EN_NBUS 6
extern volatile uint8_t en_bus_on;
extern float en_bus[];
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);   /* the app sends it (USB) */

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_held, en_ext, en_sideb, en_dreaming, en_auto;   /* auto: nobody playing, the band on its own */
extern volatile float en_idle_bars;            /* bars since a key went down */
extern volatile uint8_t en_pattern;            /* the drum pattern playing, 0..7 (Drift changes it: the UI follows) */
extern volatile int16_t en_drift_left;         /* bars until Drift's next change (-1: off) */
extern volatile uint8_t en_step;               /* the sixteenth playing, 0..15 */
extern volatile uint32_t en_bar;               /* bars since play */
extern volatile int8_t en_chord;               /* the chord: its degree in the key, 0..6 */
extern volatile int8_t en_chord_hist[3];       /* the chords before it (-1 none), newest first */
extern volatile uint8_t en_chord_root;         /* its root, a pitch class */
extern volatile uint8_t en_chord_q;            /* its quality: EN_Q_* */
enum { EN_Q_MAJ, EN_Q_MIN, EN_Q_DIM, EN_Q_DOM, EN_Q_MAJ7, EN_Q_MIN7, EN_Q_HDIM };
#define EN_NVOX 24                             /* the voices' pool */
extern volatile uint8_t en_vnote[EN_NVOX];     /* each voice's note (MIDI) */
extern volatile uint8_t en_vtrk[EN_NVOX];      /* and its track (0 lead, 1 comp, 2 bass, 3 answer), 0xFF: free */
extern volatile float en_glow[EN_NTRACK];      /* 0..1: the track just played */
extern volatile float en_wear;                 /* the record's wear (Age), 0..1 */
extern volatile float en_spin;                 /* the record's turn, 0..1 */
extern volatile float en_level;                /* the mix's level, 0..1 (VU) */
extern volatile float en_bpm;
extern volatile uint32_t en_notes, en_changes, en_answers, en_dream_notes, en_hits;   /* counts, for tests */
void en_chord_name(char *buf);                 /* "Dm7", the chord now (12 bytes) */
void en_deg_name(int degree, char *buf);       /* a degree of the key, named the same way */

/* the tapes: recipes for every parameter */
#define EN_NPIECES 10                         /* presets 01-10: factory recipes, stored over by the player */
const char *en_piece_name(int i);             /* "01" .. "10" */
void en_piece(int i, int16_t *par);
