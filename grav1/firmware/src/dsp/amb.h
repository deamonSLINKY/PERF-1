/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: GRAV-1, a star and its planets play the FM-1 (a RedtailWorks fork of AMB-1, by way of
 * HYP-1 and ECO-1).
 *
 *   STAR     in the middle, a bed of coloured noise (white, pink, brown, blue, violet, grey, black crackle,
 *            yellow radio hiss), drawn in its own colour. It roars now and then (louder, lower, a sub under
 *            it) and flares (a bright burst); its LEVEL is the ceiling for both.
 *   PLANETS  one to six, each a voice: its instrument from the bank (Drone, Glass bell, Pluck, Soft keys,
 *            Choir, Laser, Ether, Ring hiss), its level (also its size, so its gravity), its orbit's
 *            ellipse and its octave. And the dark ones: Abyss, Dark choir, Cello, Gong, Toll, Whisper,
 *            Cold pad, Dusk organ. A planet plays a short phrase as it passes the points around its
 *            orbit, kept to the scale, a note or two changing each year (each orbit). Near the star it
 *            sounds warm and dry, far out thin, cold and wet.
 *   PULSE    a drum sequencer, six lanes of 16 steps (Kick, Pulsar, Sonar, Geiger, Rim, Shaker). Lane n is
 *            bound to planet n: its hit count sets the planet's year (32 / hits bars; more hits, a faster
 *            orbit, closer in, by Kepler's third law), and each hit does the lane's BINDING to the planet
 *            (pump, blip, flicker, nudge). A lane with no hits leaves its planet dormant on the outer orbit,
 *            silent until a larger neighbour's tide wakes it.
 *   GRAVITY  a larger planet swells a smaller one as it passes, peaking at the closest approach (TIDE how
 *            much, REACH how far). Planets on the same orbit sit 60 degrees apart and never pass. The
 *            outermost planet's year is the GREAT YEAR: the chord moves, and the drums evolve.
 *   WEATHER  dust clouds (the whole system warmer, softer, grainy), stray asteroids (a heavy body crossing:
 *            tides as it passes, a rumble), comets (falling into the star, ringing bells, burning up).
 *   SPACE    a dotted-eighth echo, Dattorro's plate, a radio (band hiss and whistlers), the stereo width.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NPL 6                       /* planets, and drum lanes */
#define EN_NINST 16                    /* the instrument bank: eight bright, eight dark */
#define EN_NCOLOUR 8                   /* the star's noise colours */
#define EN_NSTEP 16
#define EN_NBANK 8                     /* drum banks, each six lanes of 16 steps */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_AEOLIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_PENT_MIN, MODE_PENT_MAJ, MODE_HIRAJOSHI,
       MODE_WHOLE, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const char *const EN_INST_NAME[EN_NINST];
extern const char *const EN_COLOUR_NAME[EN_NCOLOUR];
extern const char *const EN_STAR_KIND[EN_NCOLOUR];
extern const char *const EN_LANE_NAME[EN_NPL];
extern const char *const EN_BIND_NAME[5];

/* parameters: value ranges in en_param_info() */
enum {
    P_PLANETS, P_TEMPO, P_WANDER, P_PHRASE,           /* the system */
    P_STAR, P_COLOUR, P_ROAR, P_FLARES,               /* the star */
    P_PL,                                             /* each planet: level, instrument, ellipse, octave: P_PL + 4 * i + 0..3 */
    P_TIDE = P_PL + 4 * EN_NPL, P_REACH, P_WAKE, P_GY,                 /* gravity */
    P_DUST, P_ROCKS, P_COMETS, P_EVENTS,              /* the weather */
    P_DRUMS, P_EVOLVE, P_BANK,                        /* the Pulse; the bank playing (a new one waits for the bar) */
    P_BIND,                                           /* each lane's binding: P_BIND + lane */
    P_PAT = P_BIND + EN_NPL,                          /* each bank's lanes' 16 steps (bit s: step s): P_PAT_OF(bank, lane) */
    P_ECHO = P_PAT + EN_NPL * EN_NBANK, P_REVERB, P_RADIO, P_WIDTH,    /* space */
    P_ROOT, P_TUNE, P_SYNC, P_MODE,                   /* setup; the mode loads a pool */
    P_MIDI, P_MCH,                                    /* MIDI out: what it sends, its first channel */
    P_THEME, P_SKY, P_LINES, P_TLINES,                /* the screen (the engine keeps them only to name them) */
    P_HOP, P_HOPWHEN, P_HOPPOOL, P_HOPFILL,           /* Drift: the drums change bank (Off, Next, Random), when
                                                         (the Great Year, or every 4-64 bars), from banks 1..pool,
                                                         with a fill the bar before */
    P_NPARAMS
};
#define P_PAT_OF(b, l) (P_PAT + EN_NPL * (b) + (l))
#define P_PL_LVL(i) (P_PL + 4 * (i))
#define P_PL_INST(i) (P_PL + 4 * (i) + 1)
#define P_PL_ECC(i) (P_PL + 4 * (i) + 2)
#define P_PL_OCT(i) (P_PL + 4 * (i) + 3)
#define EN_NTHEMES 7
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
void en_play(int on);                          /* the system grows / rests (the tails ring on) */
void en_touch(void);                           /* EDIT: the Great Year now (the chord moves) */
void en_summon(int what);                      /* weather now: 0 a dust cloud (or its end), 1 an asteroid, 2 a comet */
void en_freeze(int on);                        /* ECLIPSE: the orbits and the weather hold, the space rings on */
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_mute(int on);                          /* remote: silent and no MIDI out (1), or back (0) */
void en_strike(int planet);                    /* the planet plays a note of its phrase now (the game) */
void en_regrow(void);                          /* a new start: the planets on fresh orbits, new phrases */
/* MIDI clock in. With SYNC on, the system follows it while it comes: 24 ticks a beat move the orbits and
 * the steps, START grows it from step one, STOP rests it. Without clock (or SYNC off), its own tempo. */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);
enum { EN_MIDI_OFF, EN_MIDI_NOTES, EN_MIDI_BOTH, EN_MIDI_CC };   /* P_MIDI: what goes out */
/* MIDI out (USB): planet n's notes on channel MCH + n (n 0..5), the drums on MCH + 6 (notes 36..41, a
 * lane each); the signals as CCs on MCH + 7, about 20 times a second when they change: */
enum { EN_CC_TIDE = 20,     /* 20..25: each planet's tide */
       EN_CC_ROAR = 26, EN_CC_DUST = 27, EN_CC_YEAR = 28, EN_CC_FLARE = 29 };
extern volatile float en_bpm;                  /* the tempo now: the clock's while it comes, else TEMPO (UI) */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
/* the voices apart (the browser emulator): with en_bus_on, each en_render also leaves its frames in
 * en_bus, six floats a frame: the star, planets 1-3, planets 4-6, the drums, the weather and the radio,
 * the echo and the reverb; dry, mono, at the mix's level */
#define EN_NBUS 6
extern volatile uint8_t en_bus_on;
extern float en_bus[];                         /* EN_NBUS * 1024 on the host and in the browser */

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile float en_pl_x[EN_NPL], en_pl_y[EN_NPL];   /* where each planet is: the star at 0, about -1..1 */
extern volatile float en_pl_a[EN_NPL];         /* its orbit's size now (the semi-major axis, drifting to a new one) */
extern volatile float en_pl_om[EN_NPL];        /* where its orbit is nearest the star (turns) */
extern volatile float en_pl_bias[EN_NPL];      /* its place shifted on a shared orbit (turns; the notes' points move with it) */
extern volatile float en_pl_dist[EN_NPL];      /* its distance, 0 at the star .. 1 far out */
extern volatile float en_pl_tide[EN_NPL];      /* its tide now, 0..1 */
extern volatile float en_pl_glow[EN_NPL];      /* 0..1: it just played */
extern volatile int8_t en_pl_link[EN_NPL];     /* who swells it most: a planet, EN_NPL the asteroid, -1 none */
extern volatile float en_pl_linkc[EN_NPL];     /* and how much */
extern volatile uint8_t en_pl_awake[EN_NPL];   /* a dormant planet, woken by a tide */
extern volatile uint8_t en_pl_notes[EN_NPL];   /* how many points its phrase has on its orbit */
extern volatile uint8_t en_pl_note[EN_NPL];    /* its last note (MIDI) */
extern volatile float en_drum_glow[EN_NPL];     /* 0..1: each lane just hit */
extern volatile uint16_t en_pat[EN_NPL];       /* the playing bank's steps as they are now (Evolve changes them) */
extern volatile uint8_t en_bank;               /* the bank playing (P_BANK may be waiting for the bar) */
extern volatile uint8_t en_step;               /* the step playing, 0..15 */
extern volatile int8_t en_hop_left;            /* Drift: bar lines until the bank changes (1: the fill bar), -1 none */
extern volatile uint32_t en_hops;              /* bank changes since start */
extern volatile float en_year;                 /* the Great Year: how far round it is, 0..1 */
extern volatile float en_star;                 /* the star's size: its level, its roar, a flare (0..1.5) */
extern volatile float en_flare, en_roar;
extern volatile float en_dust;                 /* the dust cloud now, 0..1 */
extern volatile uint8_t en_rock_on, en_comet_on;
extern volatile float en_rock_x, en_rock_y, en_rock_vx, en_rock_vy;   /* the asteroid (vx, vy: per second) */
extern volatile float en_comet_r, en_comet_ang;                       /* the comet: its distance and angle (turns) */
extern volatile uint8_t en_chord;              /* the Great Year's chord: its root, a pitch class */
extern volatile uint32_t en_notes;             /* planet notes since start (for tests) */
extern volatile uint32_t en_hits;              /* drum hits since start */
extern volatile uint32_t en_years, en_greats;  /* years (any planet) and Great Years */
extern volatile uint32_t en_wakes, en_rocks, en_comets, en_dusts;

/* what happened, for the screen's messages: codes in a ring, en_ev_w counts them */
enum { EV_YEAR = 1, EV_GREAT, EV_DUST, EV_CLEAR, EV_ROCK, EV_COMET, EV_BURN, EV_WAKE, EV_HOP, EV_FILL };
#define EN_NEV 16
extern volatile uint16_t en_ev[EN_NEV];        /* code << 8 | detail (a planet, the chord's root) */
extern volatile uint32_t en_ev_w;

/* the pure parts, for the tests and the screen */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
float en_period_bars(int hits);                /* a lane's hits to its planet's year, in bars */

/* the pieces: recipes for every parameter (Tune, MIDI and the screen's are the player's) */
#define EN_NPIECES 10
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
