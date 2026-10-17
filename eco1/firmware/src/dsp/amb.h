/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: ECO-1, a small plant whose electrical signals play the music (a RedtailWorks fork of
 * AMB-1). The plant has a stem and three leaves; each leaf is a voice.
 *
 *   LEAVES  each leaf holds a charge, as a plant cell's membrane potential: light and water build it, it
 *           leaks away, and when it crosses its threshold the leaf fires. A firing is a note: its pitch
 *           where the leaf's own slow walk through the scale has got to (wetter leaves climb, wind makes
 *           the steps wilder), its strength how far past the threshold the charge went. A firing runs
 *           down the stem and nudges the other leaves (SPREAD), so phrases call and answer. Each leaf
 *           plays one of eight voices (the bank) in its own register: the low leaf, the middle, the top.
 *           A leaf too dry to fire wilts and goes quiet; it comes back with the rain.
 *   STEM    a drone under it all, as loud as the soil is wet, brighter in daylight: its colour (Hum, Organ,
 *           Reed, Choir), the interval over the root (none, a fifth, a fourth, an octave, the scale's third),
 *           its octave, its tone and how much of it goes to the reverb.
 *   SKY     the day: night, dawn, noon, dusk, on a cycle of DAY length; LIGHT how strong the sun is. Light
 *           charges the leaves (the top leaf most); WARMTH speeds the plant up and dries the soil. At dawn
 *           the leaves wake together.
 *   WEATHER RAIN: how often showers come and how heavy they are; a rain track (hiss and drops) while it
 *           rains, and drops landing on the leaves give them a charge. WIND: how hard it blows, GUSTS how
 *           much it swells and falls; it shakes the leaves (more chaos), sings (a synthesized whistle whose
 *           pitch rises with it, a second tone in strong wind) and opens the space: more reverb, longer
 *           tails, a sway in them.
 *
 * Then Dattorro's plate. FREEZE stops the day and the weather where they are and lets the plate ring on.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLEAF 3                     /* leaves: 0 the low one, 1 the middle, 2 the top */
#define EN_NVOICE 8                    /* the voice bank */
#define EN_NSTEMC 4                    /* the stem's colours: Hum, Organ, Reed, Choir */
/* the species: each its own body and place on the screen, and its own way with the weather */
enum { EN_SP_FERN, EN_SP_ORCHID, EN_SP_MOSS, EN_SP_CACTUS, EN_SP_WILLOW, EN_SP_LOTUS, EN_SP_IVY, EN_SP_BAMBOO, EN_NSPECIES };
#define EN_NSTEMI 5                    /* its intervals: Root, Fifth, Fourth, Octave, Third */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const char *const EN_VOICE_NAME[EN_NVOICE];
extern const char *const EN_STEMC_NAME[EN_NSTEMC];
extern const char *const EN_STEMI_NAME[EN_NSTEMI];

/* parameters: value ranges in en_param_info() */
enum {
    P_LIGHT, P_DAY, P_WARMTH, P_SPREAD,               /* the plant and its sky */
    P_LEAF,                                           /* each leaf: level, voice, octave, tone: P_LEAF + 4 * leaf + 0..3 */
    P_RAIN = P_LEAF + 4 * EN_NLEAF, P_WIND, P_GUSTS, P_WX,    /* weather; WX: the rain's and wind's sound */
    P_STEM, P_REVERB, P_SIZE, P_WINDVERB,             /* the stem's drone; the space, and how far wind opens it */
    P_ROOT, P_TUNE, P_MIDI, P_MODE, P_OCT,            /* setup; the mode on ALGORITHM; OCT- / OCT+ */
    P_STEM_COLOUR, P_STEM_INT, P_STEM_OCT, P_STEM_TONE, P_STEM_VERB,   /* the stem's sound */
    P_SPECIES,                                        /* what the plant is (the piece's; no knob): EN_SP_* */
    P_SYNC, P_GRID, P_MCH,                            /* MIDI clock in: follow it, and the grid the leaves fire on;
                                                       * MIDI out's first channel (P_MIDI: what it sends) */
    P_NPARAMS
};
#define P_LEAF_LVL(l) (P_LEAF + 4 * (l))
#define P_LEAF_VOICE(l) (P_LEAF + 4 * (l) + 1)
#define P_LEAF_OCT(l) (P_LEAF + 4 * (l) + 2)
#define P_LEAF_TONE(l) (P_LEAF + 4 * (l) + 3)
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
void en_play(int on);                          /* the plant grows / rests (the tails ring on) */
void en_mute(int on);                          /* remote: silent and no MIDI out (1), or back (0) */
void en_touch(void);                           /* touch the plant: a charge runs up it from the soil */
void en_freeze(int on);                        /* the day and the weather stop; the plate rings on */
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: the leaves fresh, the soil damp, morning */
/* MIDI clock in. With SYNC on, the plant follows it while it comes: START grows it from the morning,
 * STOP rests it, the day is counted in bars (DAY), and with a GRID the leaves fire on it (a leaf that is
 * ready waits for the next 1/4 .. 1/16). Without clock (or with SYNC off) the plant keeps its own time. */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);
enum { EN_MIDI_OFF, EN_MIDI_NOTES, EN_MIDI_BOTH, EN_MIDI_CC };   /* P_MIDI: what goes out */
/* MIDI out (USB): the leaves' notes on channels MCH, +1, +2 (low, mid, top), the stem's on +3; the signals
 * as CCs on +4, about 20 times a second when they change: */
enum { EN_CC_CHARGE = 20,   /* 20..22: each leaf's charge, building to its firing */
       EN_CC_WATER = 23,    /* 23..25: each leaf's water */
       EN_CC_SOIL = 26, EN_CC_LIGHT = 27, EN_CC_RAIN = 28, EN_CC_WIND = 29, EN_CC_STEM = 30,
       EN_CC_SPARK = 31 };  /* 127 as any leaf fires, falling away */
extern volatile float en_bpm;                  /* the clock's tempo while it comes (UI) */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
/* the voices apart, for a computer recording each on its own track (the browser emulator; one day a
 * multichannel USB audio): with en_bus_on, each en_render also leaves its frames in en_bus, six floats a
 * frame: the low, mid and top leaves, the stem, the weather (rain and wind), the reverb; dry, mono, at
 * the mix's level */
#define EN_NBUS 6
extern volatile uint8_t en_bus_on;
extern float en_bus[];                         /* EN_NBUS * 1024 on the host and in the browser */

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2. Each leaf on its own
 * channel (1, 2, 3), the stem on 4. */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile float en_day;                  /* the time of day, 0..1 from midnight */
extern volatile float en_sun;                  /* daylight now, 0..1 (times Light) */
extern volatile float en_soil;                 /* the soil's water, 0..1 */
extern volatile float en_rain;                 /* how hard it rains now, 0..1 */
extern volatile float en_wind;                 /* how hard the wind blows now, 0..1 */
extern volatile float en_leaf_charge[EN_NLEAF];/* each leaf's charge, 0..1 (the threshold) */
extern volatile float en_leaf_water[EN_NLEAF]; /* and its water, 0..1 (below ~0.15: wilted) */
extern volatile float en_leaf_glow[EN_NLEAF];  /* 0..1: it just fired */
extern volatile uint8_t en_leaf_note[EN_NLEAF];/* its last note (MIDI) */
extern volatile float en_leaf_sway[EN_NLEAF];  /* -1..1: how the wind moves it */
extern volatile float en_stem_level;
extern volatile uint32_t en_fires;             /* firings since start (for tests) */
extern volatile uint32_t en_drops;             /* raindrops since start (for tests) */

/* the pure parts, for the tests and the screen */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
