/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (amb.h): ECO-1, the plant. One compilation unit, -O2. The plate reverb, the command
 * queue and the fast maths are AMB-1's (Charles Vestal); the plant is new. */
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
const char *const EN_VOICE_NAME[EN_NVOICE] = {"Glass", "Wood", "Pluck", "Chime", "Reed", "Flute", "Swell", "Kalimba"};
const char *const EN_STEMC_NAME[EN_NSTEMC] = {"Hum", "Organ", "Reed", "Choir"};
const char *const EN_STEMI_NAME[EN_NSTEMI] = {"Root", "Fifth", "Fourth", "Octave", "Third"};

/* the voice bank: two-operator FM (the modulator at RATIO times the carrier), the index falling away,
 * a strike (or a slow swell) and a decay; a little breath for the flute */
static const struct {
    float ratio, idx, idx_t, att_t, dec_t, breath;   /* index; its fall (s); attack (s); decay to -60 dB (s) */
} VOICE[EN_NVOICE] = {
    {3.5f, 1.1f, 0.7f, 0.003f, 3.6f, 0.0f},          /* Glass: a soft glass bell */
    {4.0f, 2.2f, 0.05f, 0.002f, 0.7f, 0.0f},         /* Wood: a marimba bar */
    {1.0f, 1.8f, 0.18f, 0.002f, 1.4f, 0.0f},         /* Pluck: a nylon string, nearly */
    {2.76f, 0.9f, 1.6f, 0.002f, 5.5f, 0.0f},         /* Chime: inharmonic, long */
    {1.0f, 1.3f, 3.0f, 0.14f, 2.6f, 0.0f},           /* Reed: a breathing reed */
    {1.0f, 0.22f, 2.0f, 0.26f, 2.2f, 0.35f},         /* Flute: a sine and breath */
    {2.0f, 0.35f, 4.0f, 0.9f, 4.5f, 0.0f},           /* Swell: slow in, long out */
    {5.0f, 1.5f, 0.045f, 0.001f, 1.5f, 0.0f},        /* Kalimba: a bright tine */
};

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Light", 0, 100, 70},   {"Day", 0, 100, 40},     {"Warmth", 0, 100, 50},  {"Spread", 0, 100, 45},
    {"Low lvl", 0, 100, 70}, {"Low vce", 0, 7, 2},    {"Low oct", -2, 2, 0},   {"Low tone", 0, 100, 45},
    {"Mid lvl", 0, 100, 65}, {"Mid vce", 0, 7, 0},    {"Mid oct", -2, 2, 0},   {"Mid tone", 0, 100, 50},
    {"Top lvl", 0, 100, 55}, {"Top vce", 0, 7, 3},    {"Top oct", -2, 2, 0},   {"Top tone", 0, 100, 40},
    {"Rain", 0, 100, 35},    {"Wind", 0, 100, 25},    {"Gusts", 0, 100, 30},   {"Weather", 0, 100, 55},
    {"Stem", 0, 100, 45},    {"Reverb", 0, 100, 60},  {"Size", 0, 100, 70},    {"Wind verb", 0, 100, 50},
    {"Root", 0, 11, 2},      {"Tune", -50, 50, 0},    {"MIDI out", 0, 3, 1},   {"Mode", 0, EN_NMODES - 1, 1},
    {"Octave", -2, 2, 0},
    {"Colour", 0, EN_NSTEMC - 1, 0}, {"Interval", 0, EN_NSTEMI - 1, 1}, {"Stem oct", -2, 1, 0}, {"Stem tone", 0, 100, 40},
    {"Stem verb", 0, 100, 30},
    {"Species", 0, EN_NSPECIES - 1, 0},
    {"Sync", 0, 1, 1},        {"Grid", 0, 5, 3},       {"Channel", 0, 11, 0},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* the pieces: eight plants (the FM-1's PRESETS). Tune and MIDI out are the player's, not the piece's.
 * Voices: 0 Glass, 1 Wood, 2 Pluck, 3 Chime, 4 Reed, 5 Flute, 6 Swell, 7 Kalimba
 *                       Lig Day War Spr  Low: lvl vce oct ton  Mid: ...           Top: ...
 *                       Ran Wnd Gst Wx   Stm Rev Siz WV   Rt Tu MI Md Oc   Stem: colour interval oct tone verb   Species   Sync Grid Channel */
static const struct {
    const char *name;
    int8_t v[P_NPARAMS];
} PIECES[EN_NPIECES] = {
    {"Fern",       {70, 40, 50, 45,  70, 2, 0, 45,  65, 0, 0, 50,  55, 3, 0, 40,  35, 25, 30, 55,  45, 60, 70, 50,  2, 0, 1, 1, 0,  0, 1, 0, 40, 30, 0,  1, 3, 0}},
    {"Orchid",     {85, 30, 70, 60,  60, 4, 0, 40,  65, 6, 0, 45,  60, 0, 0, 55,  15, 10, 15, 40,  35, 70, 85, 60,  9, 0, 1, 3, 0,  3, 4, 0, 50, 45, 1,  1, 3, 0}},
    {"Moss",       {30, 55, 35, 70,  75, 1, 0, 50,  60, 7, 0, 45,  45, 5, 0, 35,  70, 15, 20, 70,  55, 55, 60, 40,  7, 0, 1, 7, 0,  1, 2, 0, 30, 25, 2,  1, 3, 0}},
    {"Cactus",     {100, 25, 90, 25, 60, 7, -1, 55, 55, 1, 0, 60,  65, 3, 1, 45,  3, 50, 60, 60,   30, 50, 75, 70,  4, 0, 1, 2, 0,  2, 1, 0, 55, 35, 3,  1, 3, 0}},
    {"Willow",     {60, 60, 45, 55,  55, 5, 0, 40,  60, 6, 0, 45,  50, 0, 0, 50,  40, 65, 55, 65,  50, 75, 90, 85,  0, 0, 1, 5, 0,  3, 1, -1, 35, 50, 4,  1, 3, 0}},
    {"Lotus",      {75, 50, 60, 50,  65, 6, -1, 35, 55, 3, 0, 45,  60, 0, 1, 50,  20, 20, 20, 45,  40, 80, 95, 50,  5, 0, 1, 6, 0,  0, 3, 0, 45, 40, 5,  1, 3, 0}},
    {"Ivy",        {55, 35, 55, 80,  70, 2, 0, 50,  70, 2, 0, 55,  60, 7, 0, 45,  50, 40, 40, 50,  40, 55, 65, 55,  10, 0, 1, 4, 0,  1, 1, 0, 40, 30, 6,  1, 3, 0}},
    {"Bamboo",     {50, 45, 50, 60,  65, 1, 0, 45,  60, 4, 0, 50,  55, 3, 0, 45,  90, 85, 90, 80,  45, 65, 80, 90,  3, 0, 1, 5, 0,  2, 2, -1, 35, 45, 7,  1, 3, 0}},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = PIECES[i].v[p];
    *pool = EN_MODE_POOL[PIECES[i].v[P_MODE]];
}

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

static float day_secs(int v) { return 30.0f * fm_exp2f((float)v * 0.059069f); }   /* 30 s .. 30 min */
static int day_bars(int v)                       /* on the clock: 4 .. 512 bars, in powers of two and between */
{
    static const int16_t BARS[15] = {4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512};
    return BARS[(v * 14 + 50) / 100];
}
volatile uint8_t en_ext;

void en_param_text(int i, int v, char *b)
{
    static const char *const MIDI_NAME[4] = {"Off", "Notes", "Notes+CC", "CC"};
    static const char *const SYNC_NAME[2] = {"Free", "Clock"};
    static const char *const GRID_NAME[6] = {"Free", "1/4", "1/8", "1/16", "1/8T", "1/16T"};
    const char *s = 0;
    if (i == P_MIDI)
        s = MIDI_NAME[(unsigned)v < 4u ? v : 0];
    else if (i == P_SYNC)
        s = SYNC_NAME[v ? 1 : 0];
    else if (i == P_GRID)
        s = GRID_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i >= P_LEAF && i < P_RAIN && (i - P_LEAF) % 4 == 1)
        s = EN_VOICE_NAME[(unsigned)v < EN_NVOICE ? v : 0];
    else if (i == P_STEM_COLOUR)
        s = EN_STEMC_NAME[(unsigned)v < EN_NSTEMC ? v : 0];
    else if (i == P_STEM_INT)
        s = EN_STEMI_NAME[(unsigned)v < EN_NSTEMI ? v : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_MCH) {                            /* the channels it takes: 1-5 .. 12-16 */
        b = itoa_s(v + 1, b);
        *b++ = '-';
        itoa_s(v + 5, b);
        return;
    }
    if (i == P_DAY && en_ext) {                  /* on the clock: the day in bars */
        b = itoa_s(day_bars(v), b);
        *b++ = ' ';
        *b++ = 'b';
        *b++ = 'a';
        *b++ = 'r';
        *b++ = 's';
        *b = 0;
        return;
    }
    if (i == P_DAY) {                            /* the day's length */
        int sec = (int)(day_secs(v) + 0.5f);
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
    if ((i == P_TUNE || i == P_OCT || i == P_STEM_OCT || (i >= P_LEAF && i < P_RAIN && (i - P_LEAF) % 4 == 2)) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
int en_scale(uint32_t pool, int *steps)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[n++] = i;
    return n;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_TOUCH, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW };
#define QN 64u
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
void en_touch(void) { post(C_TOUCH, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen;
volatile float en_bpm;
volatile uint8_t en_bus_on;
#ifdef EN_HOST
float en_bus[EN_NBUS * 1024];
#else
float en_bus[EN_NBUS];                          /* the FM-1 has no use for them yet: no room taken */
#endif
volatile float en_day, en_sun, en_soil, en_rain, en_wind, en_stem_level;
volatile float en_leaf_charge[EN_NLEAF], en_leaf_water[EN_NLEAF], en_leaf_glow[EN_NLEAF], en_leaf_sway[EN_NLEAF];
volatile uint8_t en_leaf_note[EN_NLEAF];
volatile uint32_t en_fires, en_drops;

static int16_t par[P_NPARAMS];
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u, noise_l = 22222u, noise_r = 7777u;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }
static inline float wnoise(uint32_t *s)         /* white, -1..1 */
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(int32_t)*s * (1.0f / 2147483648.0f);
}

/* a note on part k (0..2 the leaves, 3 the stem), on its channel, when notes go out */
static void note(int on, int k, uint32_t n, uint32_t vel)
{
    if (par[P_MIDI] == EN_MIDI_NOTES || par[P_MIDI] == EN_MIDI_BOTH)
        en_midi_out((on ? 0x90u : 0x80u) | (uint32_t)((par[P_MCH] + k) & 15), n, on ? vel : 0u);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static float smooth01(float x) { x = fm_clampf(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }

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

/* ------------------------------------------------- slow random motion --- */
#define NMOD 6                                  /* 0..2 the leaves' sway, 3 the wind's base, 4 the whistle, 5 the drone */
static struct {
    float v, tgt, rate;
    uint32_t left;
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 3.0f + 14.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}

/* -------------------------------------------------------------- voices --- */
#define VPL 3                                   /* voices a leaf */
#define NV (EN_NLEAF * VPL)
typedef struct {
    float cph, mph, cinc, minc, amp, adec, idx, idec, att, arate, breath, blp, vib;
    float gl, gr;
    uint8_t on, note, leaf;
} voice_t;
static voice_t vc[NV];
static uint8_t vc_next[EN_NLEAF];
static float leaf_vol[EN_NLEAF], leaf_tone[EN_NLEAF];
static const float LEAF_PAN[EN_NLEAF] = {-0.45f, 0.4f, -0.1f};
static const int LEAF_BASE[EN_NLEAF] = {48, 60, 72};    /* the registers: C3, C4, C5 (plus the root) */
static float vib_amt;                           /* the wind's vibrato */

static void voice_start(int l, int nn, float vel)
{
    voice_t *v = &vc[l * VPL + vc_next[l]];
    int b = par[P_LEAF_VOICE(l)];
    float f, tone = leaf_tone[l], pn;
    vc_next[l] = (uint8_t)((vc_next[l] + 1) % VPL);
    if (v->on)
        note(0, l, v->note, 0);
    if (b < 0 || b >= EN_NVOICE)
        b = 0;
    f = mtof((float)nn);
    v->cinc = f * (1.0f / EN_SR);
    v->minc = v->cinc * VOICE[b].ratio;
    v->cph = 0.0f;
    v->mph = 0.0f;
    v->amp = vel;
    v->adec = fm_expf(-6.9f / (VOICE[b].dec_t * (1.15f - 0.3f * tone) * EN_SR));
    v->idx = VOICE[b].idx * (0.3f + 1.4f * tone) * (0.7f + 0.3f * vel);
    v->idec = fm_expf(-1.0f / (VOICE[b].idx_t * EN_SR));
    v->att = 0.0f;
    v->arate = 1.0f - fm_expf(-1.0f / (VOICE[b].att_t * EN_SR));
    v->breath = VOICE[b].breath;
    v->vib = frand();
    pn = fm_clampf(LEAF_PAN[l] + 0.2f * (frand() - 0.5f), -1.0f, 1.0f);
    v->gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f));
    v->gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f));
    v->note = (uint8_t)nn;
    v->leaf = (uint8_t)l;
    v->on = 1;
    note(1, l, (uint32_t)nn, (uint32_t)(30.0f + 97.0f * vel));
}

/* ------------------------------------------------------------- the plant --- */
/* the sky */
static float day_ph = 0.3f, day_len = 120.0f, light, metab = 1.0f, daylight, prev_daylight;
/* the soil and the leaves */
static float soil = 0.65f;
typedef struct {
    float v;                                    /* the charge: fires at 1 */
    float water;
    int walk;                                   /* where it is in its walk through the scale (steps) */
    int hint;                                   /* a note heard down the stem (steps), -1 none */
    float hint_age;
    float sway_ph;
} leaf_t;
static leaf_t leaf[EN_NLEAF];
static const float LEAF_HEIGHT[EN_NLEAF] = {0.0f, 0.5f, 1.0f};
static const float LEAF_DRINK_T[EN_NLEAF] = {5.0f, 11.0f, 22.0f};  /* how fast each leaf follows the soil (s) */
/* the species' ways: how much the wind shakes it (the music) and sways it (the screen); how thirsty it is,
 * how slowly its leaves take up and give up water, how dry before they wilt; the light it wants (it fires
 * as hard in less: shade plants); how often a drop lands on it, how much of the rain reaches it; whether it
 * blooms when the rain comes; the water it always has (a pond); how loud the wind is where it lives */
static const struct {
    float shake, sway, thirst, store, wilt, sat, hit, rainfill, bloom, floor, windsnd;
} SPECIES[EN_NSPECIES] = {
    {1.0f, 1.0f, 1.2f, 1.0f, 0.08f, 0.6f, 0.06f, 1.0f, 0.0f, 0.0f, 1.0f},     /* Fern: a shade plant, drinks */
    {0.35f, 0.25f, 0.8f, 1.5f, 0.08f, 0.7f, 0.0f, 0.5f, 0.0f, 0.0f, 0.4f},    /* Orchid: on a windowsill */
    {0.3f, 0.15f, 1.5f, 0.7f, 0.12f, 0.4f, 0.10f, 1.0f, 0.0f, 0.0f, 1.0f},    /* Moss: low, wet, in the shade */
    {0.2f, 0.05f, 0.25f, 4.0f, 0.02f, 1.0f, 0.03f, 1.0f, 1.0f, 0.0f, 1.0f},   /* Cactus: stiff, stores water, blooms in rain */
    {1.3f, 1.6f, 1.3f, 1.0f, 0.08f, 1.0f, 0.06f, 1.0f, 0.0f, 0.0f, 1.0f},     /* Willow: all sway */
    {0.6f, 0.5f, 0.0f, 1.0f, 0.08f, 1.0f, 0.15f, 1.0f, 0.0f, 0.85f, 1.0f},    /* Lotus: in a pond */
    {0.7f, 0.6f, 0.7f, 1.2f, 0.06f, 0.8f, 0.06f, 1.0f, 0.0f, 0.0f, 1.0f},     /* Ivy: on a wall, hardy */
    {1.0f, 1.3f, 1.0f, 1.0f, 0.08f, 1.0f, 0.06f, 1.0f, 0.0f, 0.0f, 1.0f},     /* Bamboo: bends */
};
static int sp;                                  /* the species */
static uint8_t pend[EN_NLEAF];                  /* ready, waiting for the grid */
static uint32_t clk_ticks;                      /* MIDI clock ticks since START */
static uint32_t ext_age, since_tick;            /* renders since the last tick; samples since it */
static float tick_gap;                          /* samples between ticks, smoothed (blocks quantize each) */
static uint8_t cc_last[12];
static uint8_t bloomed;                         /* the cactus bloomed for this shower */
/* firings running down the stem */
#define NPOKE 16
static struct {
    float wait, amt;
    int8_t leaf, hint;
} poke[NPOKE];
static float spread;
/* the weather */
static float rain_amt, rain_now, rain_tgt, rain_left, wind_amt, gusts, wind_now, gust, gust_tgt, gust_left;
static uint8_t raining;
static float wx_vol;
static float plant_gate;                        /* growing 1 / resting 0, smoothed for the sounds */

static int walk_max(void) { return nsteps + nsteps / 2; }  /* an octave and a half */
static int step_note(int l, int s)              /* a leaf's step to its note */
{
    int n;
    if (nsteps <= 0)
        return 60;
    if (s < 0)
        s = 0;
    n = LEAF_BASE[l] + par[P_ROOT] + 12 * (par[P_LEAF_OCT(l)] + par[P_OCT]) + 12 * (s / nsteps) + steps[s % nsteps];
    return n < 12 ? 12 : n > 115 ? 115 : n;
}

static void poke_add(int l, float amt, float wait, int hint)
{
    int i;
    for (i = 0; i < NPOKE; i++)
        if (poke[i].leaf < 0) {
            poke[i].leaf = (int8_t)l;
            poke[i].amt = amt;
            poke[i].wait = wait;
            poke[i].hint = (int8_t)hint;
            return;
        }
}

static int on_grid(void) { return par[P_SYNC] && en_ext && par[P_GRID] && en_playing; }
static void leaf_fire(int l);
static void try_fire(int l)                     /* fire now, or on the grid's next step */
{
    if (on_grid())
        pend[l] = 1;
    else
        leaf_fire(l);
}
static void fire_pending(void)
{
    int l;
    for (l = 0; l < EN_NLEAF; l++)
        if (pend[l]) {
            pend[l] = 0;
            if (en_playing)
                leaf_fire(l);
        }
}
static void leaf_fire(int l)
{
    leaf_t *f = &leaf[l];
    float over = f->v - 1.0f, vel;
    int s, k, wmax = walk_max();
    if (f->hint >= 0 && f->hint_age < 0.8f) {    /* an answer: a third above what came down the stem */
        s = f->hint + 2;
    } else {                                     /* its own walk: wetter climbs, wind leaps */
        int reach = 1 + (int)(wind_now * SPECIES[sp].shake * 2.5f + 0.5f);
        float up = 0.42f + 0.25f * (f->water - 0.5f);
        k = 1 + irand(reach);
        if (frand() < 0.18f)
            k = 0;
        s = f->walk + (frand() < up ? k : -k);
        if (f->walk > wmax * 3 / 4 && frand() < 0.3f)   /* drawn home a little */
            s -= 1;
    }
    while (s > wmax)
        s -= nsteps > 0 ? nsteps : 1;
    if (s < 0)
        s = -s;
    if (s > wmax)
        s = wmax;
    f->walk = s;
    f->hint = -1;
    vel = fm_clampf(0.42f + 0.22f * daylight * light + 2.0f * over + 0.16f * frand(), 0.25f, 1.0f);
    voice_start(l, step_note(l, s), vel);
    en_leaf_note[l] = (uint8_t)step_note(l, s);
    en_leaf_glow[l] = 1.0f;
    en_fires++;
    f->v = -(0.05f + 0.35f * frand());           /* refractory */
    if (spread > 0.0f) {                         /* down the stem to the others */
        for (k = 0; k < EN_NLEAF; k++) {
            int d = k > l ? k - l : l - k;
            if (k == l)
                continue;
            poke_add(k, spread * (d == 1 ? 0.55f : 0.3f), (0.08f + 0.18f * frand()) * (float)d * (1.6f - spread), s);
        }
    }
}

/* the sky, the soil, the weather and the leaves: once a chunk (up to 256 samples) */
static void plant_step(uint32_t n)
{
    const float dt = (float)n * (1.0f / EN_SR);
    float sunv, common;
    int l, i;

    /* the day */
    if (en_playing && !en_frozen && !(par[P_SYNC] && en_ext)) {
        day_ph += dt / day_len;
        if (day_ph >= 1.0f)
            day_ph -= 1.0f;
    }
    sunv = fm_sin_turns(day_ph - 0.25f);          /* up from 6:00 to 18:00 */
    prev_daylight = daylight;
    daylight = smooth01((sunv + 0.12f) * 1.6f);
    if (en_playing && prev_daylight < 0.2f && daylight >= 0.2f)
        for (l = 0; l < EN_NLEAF; l++)           /* dawn: the leaves wake, the top one first */
            poke_add(l, 0.55f + 0.3f * frand(), 0.2f + 0.7f * (float)(EN_NLEAF - 1 - l) + frand(), -1);

    /* the weather (held while frozen) */
    if (!en_frozen) {
        rain_left -= dt;
        if (rain_left <= 0.0f) {
            if (raining || rain_amt <= 0.0f) {   /* a dry spell: shorter the more it rains */
                raining = 0;
                rain_tgt = 0.0f;
                rain_left = rain_amt <= 0.0f ? 5.0f : (8.0f + 140.0f * (1.0f - rain_amt) * (1.0f - rain_amt)) * (0.4f + 1.2f * frand());
            } else {                             /* a shower */
                raining = 1;
                rain_tgt = rain_amt * (0.35f + 0.65f * frand());
                rain_left = (12.0f + 80.0f * rain_amt) * (0.5f + frand());
            }
        }
        if (raining && frand() < dt * 0.25f)     /* it comes and goes within a shower */
            rain_tgt = fm_clampf(rain_amt * (0.35f + 0.65f * frand()), 0.0f, 1.0f);
        gust_left -= dt;
        if (gust_left <= 0.0f) {
            gust_tgt = gust_tgt > 0.0f ? 0.0f : gusts * (0.3f + 0.7f * frand());
            gust_left = gust_tgt > 0.0f ? 1.2f + 4.0f * frand() : (2.0f + 10.0f * frand()) * (1.3f - gusts);
        }
    }
    rain_now += ((en_playing ? rain_tgt : 0.0f) - rain_now) * fm_minf(dt / 4.0f, 1.0f);
    gust += (gust_tgt - gust) * fm_minf(dt / (gust_tgt > gust ? 0.7f : 2.0f), 1.0f);
    wind_now += (fm_clampf(wind_amt * (0.8f + 0.25f * mod[3].v) + gust, 0.0f, 1.0f) - wind_now) * fm_minf(dt / 0.5f, 1.0f);

    /* the soil: rain fills it, sun and warmth dry it */
    soil += dt * (rain_now * 0.07f * SPECIES[sp].rainfill - 0.0022f * SPECIES[sp].thirst * (0.25f + daylight * light) * metab * (0.4f + soil));
    soil = fm_clampf(soil, SPECIES[sp].floor, 1.0f);
    if (SPECIES[sp].bloom > 0.0f && en_playing) {   /* the cactus blooms when the rain comes */
        if (!bloomed && rain_now > 0.08f) {
            bloomed = 1;
            for (l = 0; l < EN_NLEAF; l++)
                poke_add(l, 0.9f, 0.4f + 0.6f * (float)(EN_NLEAF - 1 - l) + 0.5f * frand(), -1);
        } else if (rain_now < 0.02f) {
            bloomed = 0;
        }
    }

    /* the leaves */
    common = wnoise(&noise_l);
    vib_amt = 0.0025f * wind_now * wind_now;
    for (l = 0; l < EN_NLEAF; l++) {
        leaf_t *f = &leaf[l];
        float h = LEAF_HEIGHT[l], hyd, rate, sh;
        /* the low leaf drinks first, the top one last and dries most in the sun */
        float tgt = fm_clampf(soil * (1.25f - 0.55f * h) - 0.08f * h * daylight * light * metab, 0.0f, 1.0f);
        f->water += (tgt - f->water) * fm_minf(dt / (LEAF_DRINK_T[l] * SPECIES[sp].store), 1.0f);
        hyd = smooth01((f->water - SPECIES[sp].wilt) * (1.0f / 0.3f));
        sh = wind_now * (0.4f + 0.6f * h) * SPECIES[sp].shake;   /* the top leaf feels the wind most */
        f->sway_ph += dt * (0.3f + 1.2f * wind_now) * (1.0f + 0.13f * (float)l);
        if (f->sway_ph >= 1.0f)
            f->sway_ph -= 1.0f;
        en_leaf_sway[l] = fm_clampf(wind_now * (0.4f + 0.6f * h) * SPECIES[sp].sway * (0.6f * fm_sin_turns(f->sway_ph) + 0.4f * mod[l].v), -1.5f, 1.5f);
        if (en_playing) {
            float sun_here = daylight * light * (0.55f + 0.45f * h);
            rate = metab * (0.05f + 0.55f * fm_minf(sun_here / SPECIES[sp].sat, 1.2f)) * hyd;
            f->v += dt * (rate - 0.03f * f->v - (hyd < 0.2f ? 0.3f : 0.0f) * f->v);
            /* the wind shakes it: noise shared by the plant and its own, and gusts push */
            f->v += sh * sh * 0.5f * fm_sqrtf(dt) * (0.6f * common + 0.4f * wnoise(&noise_r)) * hyd + gust * 0.25f * SPECIES[sp].shake * dt * hyd * (0.5f + h);
        } else {
            f->v *= 1.0f - fm_minf(dt * 0.2f, 1.0f);
        }
        if (f->hint >= 0)
            f->hint_age += dt;
        if (f->v >= 1.0f && hyd > 0.05f && !pend[l])
            try_fire(l);
        en_leaf_charge[l] = fm_clampf(f->v, 0.0f, 1.0f);
        en_leaf_water[l] = f->water;
        en_leaf_glow[l] *= 1.0f - fm_minf(dt * 3.0f, 1.0f);
    }
    for (i = 0; i < NPOKE; i++) {                /* the firings on their way down the stem */
        if (poke[i].leaf < 0)
            continue;
        poke[i].wait -= dt;
        if (poke[i].wait <= 0.0f) {
            leaf_t *f = &leaf[poke[i].leaf];
            if (en_playing) {
                f->v += poke[i].amt * smooth01((f->water - SPECIES[sp].wilt) * (1.0f / 0.3f));
                if (poke[i].hint >= 0) {
                    f->hint = poke[i].hint;
                    f->hint_age = 0.0f;
                }
                if (f->v >= 1.0f && !pend[poke[i].leaf])
                    try_fire(poke[i].leaf);
            }
            poke[i].leaf = -1;
        }
    }
    en_day = day_ph;
    en_sun = daylight * fm_minf(light, 1.0f);
    en_soil = soil;
    en_rain = rain_now;
    en_wind = wind_now;
}

/* -------------------------------------------------------------- the stem --- */
/* a drone on the root (two octaves under the middle leaf, moved by its Octave) and an interval over it;
 * as loud as the soil is wet, brighter in daylight, swelling a little when the leaves fire. Each colour
 * is a few partials over each note (the sub-octave, the note, its octave, its twelfth); the choir
 * doubles the notes a few cents apart, so they beat slowly. */
static const float STEMC[EN_NSTEMC][4] = {      /* weights: x0.5, x1, x2, x3 */
    {0.45f, 1.0f, 0.12f, 0.0f},                 /* Hum: round, with a sub */
    {0.35f, 0.8f, 0.6f, 0.4f},                  /* Organ: drawbars */
    {0.0f, 0.8f, 0.12f, 0.55f},                 /* Reed: hollow, odd partials */
    {0.15f, 0.85f, 0.28f, 0.1f},                /* Choir: soft, doubled */
};
static struct {
    float ph[5], amp, lp[2], note, w[4], wt[4], ivl, ivl_t, ivl_amp;
    uint8_t midi_note;
} stem;
static float stem_vol, stem_tone, stem_verb;

static void stem_note(void)
{
    int n = 36 + par[P_ROOT] + 12 * (par[P_OCT] + par[P_STEM_OCT]);
    while (n < 24)
        n += 12;
    if (stem.midi_note && stem.midi_note != (uint8_t)n) {
        note(0, 3, stem.midi_note, 0);
        if (en_playing)
            note(1, 3, (uint32_t)n, 60);
    }
    stem.midi_note = (uint8_t)n;
    stem.note = (float)n;
}

static void stem_interval(void)                 /* the interval over the root, kept to the scale */
{
    static const int SEMI[EN_NSTEMI] = {0, 7, 5, 12, 4};
    int i = par[P_STEM_INT], s = SEMI[(unsigned)i < EN_NSTEMI ? i : 1];
    if (i == 4)                                  /* the scale's third: major, minor, else the fifth */
        s = (pool >> 4 & 1u) ? 4 : (pool >> 3 & 1u) ? 3 : 7;
    if (s && s < 12 && !(pool >> s & 1u))        /* not in the scale: the octave */
        s = 12;
    stem.ivl_t = (float)s;
}

static inline float stem_osc(float ph, const float *w)   /* ph in 0..2 (two turns: the sub's period) */
{
    return w[0] * sine(ph * 0.5f) + w[1] * sine(ph) + w[2] * sine(ph * 2.0f) + w[3] * sine(ph * 3.0f);
}

static float mixl[256], mixr[256], rsend[256];
#ifdef EN_HOST
static float busb[EN_NBUS][256];                 /* the voices apart (en_bus_on) */
static uint8_t bus_on;
#define BUS(b, j, x) do { if (bus_on) busb[b][j] += (x); } while (0)
#else
static float busb[EN_NBUS][1];
#define bus_on 0
#define BUS(b, j, x) do { (void)(b); } while (0)
#endif

static void render_stem(uint32_t n)
{
    const int choir = par[P_STEM_COLOUR] == 3;
    float glow = (en_leaf_glow[0] + en_leaf_glow[1] + en_leaf_glow[2]) * 0.08f, inc0, inc1, dt = (float)n / EN_SR;
    float tgt = stem_vol * plant_gate * (0.2f + 0.8f * soil) * (0.85f + 0.15f * mod[5].v + glow);
    float fc = (70.0f + 1500.0f * stem_tone * stem_tone) * (0.55f + 0.9f * en_sun) + 300.0f * glow;
    float c = 1.0f - fm_expf(-FM_TWO_PI * fm_minf(fc, 9000.0f) * (1.0f / EN_SR));
    float amp = stem.amp, a = stem.lp[0], b = stem.lp[1], p[5], ia, ia_t, send = stem_verb;
    uint32_t j;
    int k;
    for (k = 0; k < 4; k++)                      /* a new colour fades in */
        stem.w[k] += (stem.wt[k] - stem.w[k]) * fm_minf(dt * 2.0f, 1.0f);
    stem.ivl += (stem.ivl_t - stem.ivl) * fm_minf(dt * 4.0f, 1.0f) * (fm_fabsf(stem.ivl_t - stem.ivl) > 0.02f);
    if (fm_fabsf(stem.ivl_t - stem.ivl) <= 0.02f)
        stem.ivl = stem.ivl_t;
    ia_t = stem.ivl_t > 0.0f ? 0.6f : 0.0f;
    if (amp < 1e-5f && tgt < 1e-5f) {
        stem.amp = 0.0f;
        en_stem_level = 0.0f;
        return;
    }
    inc0 = mtof(stem.note) * (1.0f / EN_SR);
    inc1 = inc0 * fm_exp2f(stem.ivl * (1.0f / 12.0f));
    ia = stem.ivl_amp;
    for (k = 0; k < 5; k++)
        p[k] = stem.ph[k];
    for (j = 0; j < n; j++) {
        float x = stem_osc(p[0], stem.w) + ia * stem_osc(p[1], stem.w);
        p[0] += inc0;
        p[1] += inc1;
        if (choir) {                             /* the doubles, a few cents apart */
            x = 0.7f * x + 0.55f * (stem_osc(p[2], stem.w) + ia * stem_osc(p[3], stem.w));
            p[2] += inc0 * 1.0046f;
            p[3] += inc1 * 0.9958f;
        }
        for (k = 0; k < 4; k++)
            if (p[k] >= 2.0f)
                p[k] -= 2.0f;
        a += c * (x - a);
        b += c * (a - b);
        amp += (tgt - amp) * (1.0f / (1.5f * EN_SR));
        ia += (ia_t - ia) * (1.0f / (0.8f * EN_SR));
        x = b * amp * 0.17f;
        mixl[j] += x;
        mixr[j] += x;
        BUS(3, j, x);
        rsend[j] += x * send;
    }
    for (k = 0; k < 5; k++)
        stem.ph[k] = p[k];
    stem.ivl_amp = ia;
    stem.lp[0] = fm_flush(a);
    stem.lp[1] = fm_flush(b);
    stem.amp = amp;
    en_stem_level = amp;
}

/* -------------------------------------------------------------- the leaves --- */
static void render_voices(uint32_t n)
{
    int i;
    for (i = 0; i < NV; i++) {
        voice_t *v = &vc[i];
        float cph = v->cph, mph = v->mph, amp = v->amp, idx = v->idx, att = v->att, blp = v->blp, g, gl, gr, vf;
        int lf;
        uint32_t j;
        if (!v->on)
            continue;
        g = leaf_vol[v->leaf] * 0.34f;
        lf = v->leaf;
        gl = v->gl * g;
        gr = v->gr * g;
        v->vib += (float)n * (5.1f / EN_SR);
        if (v->vib >= 1.0f)
            v->vib -= 1.0f;
        vf = 1.0f + vib_amt * fm_sin_turns(v->vib);
        {
            const float ci = v->cinc * vf, mi = v->minc * vf, ar = v->arate, ad = v->adec, id = v->idec, br = v->breath;
            for (j = 0; j < n; j++) {
                float x = sine(cph + sine(mph) * idx + 4.0f);
                if (br > 0.0f) {                 /* breath: noise, low-passed, under the tone */
                    blp += 0.18f * (wnoise(&noise_r) - blp);
                    x += br * blp * (0.6f + 0.4f * idx);
                }
                x *= amp * att;
                mixl[j] += x * gl;
                mixr[j] += x * gr;
                BUS(lf, j, x * g * 0.75f);
                rsend[j] += x * g * 0.55f;
                cph += ci;
                mph += mi;
                if (cph >= 1.0f)
                    cph -= 1.0f;
                if (mph >= 1.0f)
                    mph -= 1.0f;
                amp *= ad;
                idx *= id;
                att += (1.0f - att) * ar;
            }
        }
        v->cph = cph;
        v->mph = mph;
        v->amp = amp;
        v->idx = idx;
        v->att = att;
        v->blp = fm_flush(blp);
        if (amp < 0.0006f) {
            v->on = 0;
            note(0, v->leaf, v->note, 0);
        }
    }
}

/* ----------------------------------------------------------- the weather --- */
/* the wind: no noise, a whistle (the aeolian tone air makes past a stem), a near-sine FM pair whose
 * pitch climbs with the wind and whose edge (the index) grows; a second tone a fourth up comes in when
 * it blows hard, and the whole flutters a little */
static struct {
    float ph[2], mph[2], f, amp, flt, flt2, pan;
} wh;

static void render_wind(uint32_t n)
{
    const float w = wind_now, tgt_f = 230.0f * fm_exp2f(1.7f * w + 0.18f * w * mod[4].v);
    const float tgt_a = wx_vol * plant_gate * smooth01(w * 1.4f) * (0.35f + 0.65f * w) * SPECIES[sp].windsnd;
    const float idx = 0.02f + 0.32f * w * w, a2 = smooth01((w - 0.5f) * 3.3f);
    float amp = wh.amp, f, p0 = wh.ph[0], p1 = wh.ph[1], m0 = wh.mph[0], m1 = wh.mph[1], flt = wh.flt, flt2 = wh.flt2, gl, gr, pn;
    uint32_t j;
    wh.f += (tgt_f - wh.f) * fm_minf((float)n / (0.6f * EN_SR), 1.0f);    /* gusts slide it up */
    f = wh.f;
    if (amp < 1e-5f && tgt_a < 1e-5f) {
        wh.amp = 0.0f;
        return;
    }
    pn = fm_clampf(0.5f * en_leaf_sway[2], -0.8f, 0.8f);
    gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * 0.032f;
    gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * 0.032f;
    {
        const float i0 = f * (1.0f / EN_SR), i1 = i0 * 1.335f;
        for (j = 0; j < n; j++) {
            float x;
            flt += 0.0009f * (wnoise(&noise_l) - flt);   /* a slow flutter */
            flt2 += 0.002f * (flt * 30.0f - flt2);
            x = sine(p0 + sine(m0) * idx + 4.0f) + a2 * 0.55f * sine(p1 + sine(m1) * idx * 0.7f + 4.0f);
            x *= amp * fm_clampf(1.0f + flt2 * 0.5f, 0.3f, 1.7f);
            mixl[j] += x * gl;
            mixr[j] += x * gr;
            rsend[j] += x * 0.006f;
            BUS(4, j, x * (gl + gr) * 0.7f);
            p0 += i0 * (1.0f + 0.004f * flt2);
            m0 += i0 * 1.003f;
            p1 += i1;
            m1 += i1 * 0.997f;
            if (p0 >= 1.0f) p0 -= 1.0f;
            if (m0 >= 1.0f) m0 -= 1.0f;
            if (p1 >= 1.0f) p1 -= 1.0f;
            if (m1 >= 1.0f) m1 -= 1.0f;
            amp += (tgt_a - amp) * (1.0f / (0.4f * EN_SR));
        }
    }
    wh.ph[0] = p0;
    wh.ph[1] = p1;
    wh.mph[0] = m0;
    wh.mph[1] = m1;
    wh.flt = fm_flush(flt);
    wh.flt2 = fm_flush(flt2);
    wh.amp = amp;
}

/* the rain: a hiss (noise between two low-passes, so a band) and drops, each a short rising chirp (the
 * bubble a drop makes) with a click at its start; heavier rain, more of both */
#define NDROP 10
static struct {
    float ph, inc, rise, amp, dec, clk, gl, gr;
    int32_t wait;
    uint8_t on;
} drop[NDROP];
static float hiss_amp, hs[4];

static void drops_spawn(uint32_t n)
{
    float expect = (rain_now * 45.0f + (rain_now > 0.02f ? 1.5f : 0.0f)) * (float)n * (1.0f / EN_SR);
    int i, k = (int)expect;
    if (frand() < expect - (float)k)
        k++;
    while (k-- > 0) {
        for (i = 0; i < NDROP && drop[i].on; i++)
            ;
        if (i == NDROP)
            return;
        {
            float f = 700.0f + 2600.0f * frand() * frand() + 400.0f * frand(), pn = frand() * 1.6f - 0.8f, a;
            a = wx_vol * plant_gate * (0.25f + 0.75f * frand() * frand());
            drop[i].ph = 0.0f;
            drop[i].inc = f * (1.0f / EN_SR);
            drop[i].rise = 1.0f + (0.25f + 0.5f * frand()) / (0.02f * EN_SR);
            drop[i].amp = a;
            drop[i].dec = fm_expf(-1.0f / ((0.006f + 0.02f * frand()) * EN_SR));
            drop[i].clk = a * 0.8f;
            drop[i].gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f));
            drop[i].gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f));
            drop[i].wait = irand((int)n);
            drop[i].on = 1;
            en_drops++;
            if (en_playing && frand() < SPECIES[sp].hit) {   /* it landed on a leaf */
                int l = irand(EN_NLEAF);
                leaf[l].v += 0.1f + 0.12f * frand();
            }
        }
    }
}

static void render_rain(uint32_t n)
{
    const float tgt = wx_vol * plant_gate * rain_now * (0.4f + 0.6f * rain_now) * 0.22f;
    float a = hiss_amp, h0 = hs[0], h1 = hs[1], h2 = hs[2], h3 = hs[3];
    uint32_t j;
    int i;
    if (a > 1e-5f || tgt > 1e-5f) {
        for (j = 0; j < n; j++) {
            float l = wnoise(&noise_l), r = wnoise(&noise_r), xl, xr;
            h0 += 0.55f * (l - h0);                  /* ~6 kHz */
            h1 += 0.055f * (h0 - h1);                /* ~400 Hz: taken away */
            h2 += 0.55f * (r - h2);
            h3 += 0.055f * (h2 - h3);
            xl = (h0 - h1) * a;
            xr = (h2 - h3) * a;
            mixl[j] += xl;
            mixr[j] += xr;
            BUS(4, j, (xl + xr) * 0.5f);
            rsend[j] += (xl + xr) * 0.15f;
            a += (tgt - a) * (1.0f / (0.8f * EN_SR));
        }
        hs[0] = fm_flush(h0);
        hs[1] = fm_flush(h1);
        hs[2] = fm_flush(h2);
        hs[3] = fm_flush(h3);
        hiss_amp = a;
    }
    for (i = 0; i < NDROP; i++) {
        float ph, inc, amp, clk;
        if (!drop[i].on)
            continue;
        j = 0;
        if (drop[i].wait > 0) {
            if ((uint32_t)drop[i].wait >= n) {
                drop[i].wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)drop[i].wait;
            drop[i].wait = 0;
        }
        ph = drop[i].ph, inc = drop[i].inc, amp = drop[i].amp, clk = drop[i].clk;
        for (; j < n; j++) {
            float x = sine(ph) * amp * 0.2f + clk * wnoise(&noise_r) * 0.1f;
            mixl[j] += x * drop[i].gl;
            mixr[j] += x * drop[i].gr;
            BUS(4, j, x * 0.7f);
            rsend[j] += x * 0.3f;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc *= drop[i].rise;
            amp *= drop[i].dec;
            clk *= 0.75f;
        }
        drop[i].ph = ph, drop[i].inc = inc, drop[i].amp = amp, drop[i].clk = clk;
        if (amp < 0.0003f || inc > 0.4f)
            drop[i].on = 0;
    }
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in AMB-1, after FoMni). The wind sways it: the
 * tank's modulated allpasses swing further (room for four times the paper's excursion) and faster. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define EXC DS(16)
#define EXC_MAX (4 * EXC)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC_MAX + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC_MAX + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC_MAX + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;
static float pl_exc = (float)EXC, pl_lfo = 0.0001f;
static float rev_send, rev_in_gain = 1.0f, wind_verb;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
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

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.6f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + pl_lfo * lfo_c, c = lfo_c - pl_lfo * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + pl_exc * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.45f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + pl_exc * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.45f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------------------ parameters --- */
static float lvl(int v) { return (float)(v * v) * 0.0001f; }

static void scale_set(void)
{
    int l, wm;
    nsteps = en_scale(pool, steps);
    wm = walk_max();
    for (l = 0; l < EN_NLEAF; l++)
        if (leaf[l].walk > wm)
            leaf[l].walk = wm;
}

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    if (p >= P_LEAF && p < P_RAIN) {
        int l = (p - P_LEAF) / 4, k = (p - P_LEAF) % 4;
        if (k == 0)
            leaf_vol[l] = lvl(v);
        else if (k == 3)
            leaf_tone[l] = (float)v * 0.01f;
        return;
    }
    switch (p) {
    case P_LIGHT: light = (float)v * 0.012f; break;
    case P_DAY: day_len = day_secs(v); break;
    case P_WARMTH: metab = 0.5f * fm_exp2f((float)v * 0.02f); break;   /* 0.5 .. 2 */
    case P_SPREAD: spread = (float)v * 0.01f; break;
    case P_RAIN:
        rain_amt = (float)v * 0.01f;
        if (rain_left > 30.0f && !raining)       /* more rain asked for: don't wait out a long dry spell */
            rain_left = 2.0f + 6.0f * frand();
        break;
    case P_WIND: wind_amt = (float)v * 0.01f; break;
    case P_GUSTS: gusts = (float)v * 0.01f; break;
    case P_WX: wx_vol = lvl(v); break;
    case P_STEM: stem_vol = lvl(v); break;
    case P_REVERB: rev_send = lvl(v); break;
    case P_SIZE: pl_decay_t = 0.6f + 0.36f * (float)v * 0.01f; break;
    case P_WINDVERB: wind_verb = (float)v * 0.01f; break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_ROOT: case P_OCT: case P_STEM_OCT: stem_note(); break;
    case P_STEM_COLOUR: {
        int k;
        for (k = 0; k < 4; k++)
            stem.wt[k] = STEMC[(unsigned)v < EN_NSTEMC ? v : 0][k];
        break;
    }
    case P_STEM_INT: stem_interval(); break;
    case P_STEM_TONE: stem_tone = (float)v * 0.01f; break;
    case P_STEM_VERB: stem_verb = (float)v * 0.008f; break;
    case P_SPECIES: sp = v; break;
    case P_MIDI: case P_MCH: {                   /* what was sounding stops, on every channel it could be on */
        int i;
        for (i = 0; i < 16; i++)
            en_midi_out(0xB0 | (uint32_t)i, 123, 0);
        for (i = 0; i < 12; i++)
            cc_last[i] = 0xFF;
        if (en_playing && v && p == P_MIDI)
            note(1, 3, stem.midi_note, 60);
        break;
    }
    case P_SYNC: case P_GRID: fire_pending(); break;
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    stem_interval();
}

static void regrow(void)                        /* a new piece: fresh leaves, damp soil, morning */
{
    int l, i;
    day_ph = 0.3f;
    daylight = prev_daylight = 1.0f;
    soil = 0.65f;
    for (l = 0; l < EN_NLEAF; l++) {
        leaf[l].v = 0.15f + 0.6f * frand();
        leaf[l].water = 0.65f;
        leaf[l].walk = nsteps / 2 + irand(3);
        leaf[l].hint = -1;
        leaf[l].sway_ph = frand();
    }
    for (i = 0; i < NPOKE; i++)
        poke[i].leaf = -1;
    raining = 0;
    rain_tgt = 0.0f;
    rain_left = rain_amt > 0.5f ? 2.0f + 6.0f * frand() : 6.0f + 20.0f * frand();
    gust_tgt = 0.0f;
    gust_left = 2.0f + 4.0f * frand();
}

static void notes_off(void)
{
    int i;
    if (!par[P_MIDI])
        return;
    for (i = 0; i < 5; i++)
        en_midi_out(0xB0 | (uint32_t)((par[P_MCH] + i) & 15), 123, 0);
}

static void quiet(void)
{
    int i;
    for (i = 0; i < NV; i++)
        vc[i].on = 0;
    for (i = 0; i < NDROP; i++)
        drop[i].on = 0;
    stem.amp = 0.0f;
    wh.amp = 0.0f;
    hiss_amp = 0.0f;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < NV; i++)
        vc[i].on = 0;
    for (i = 0; i < NDROP; i++)
        drop[i].on = 0;
    stem.midi_note = 0;
    wh.f = 230.0f;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    regrow();
    pl_decay = pl_decay_t;
    for (i = 0; i < 4; i++)
        stem.w[i] = stem.wt[i];
    stem.ivl = stem.ivl_t;
}

/* ------------------------------------------------------------ transport --- */
static void play_start(void)
{
    en_playing = 1;
    note(1, 3, stem.midi_note, 60);
}

static void play_stop(void)
{
    en_playing = 0;
    notes_off();
}

static void touch(void)                         /* a charge runs up the plant from the soil */
{
    int l;
    if (!en_playing)
        return;
    for (l = 0; l < EN_NLEAF; l++)
        poke_add(l, 0.9f + 0.4f * frand(), 0.05f + 0.22f * (float)l + 0.05f * frand(), l ? -1 : -1);
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
                play_start();
            else if (!a && en_playing)
                play_stop();
            break;
        case C_TOUCH: touch(); break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            if (!par[P_SYNC])                     /* free: its own time */
                break;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                static const uint8_t GRID_TICKS[6] = {0, 24, 12, 6, 8, 4};
                int g = GRID_TICKS[(unsigned)par[P_GRID] < 6u ? par[P_GRID] : 0];
                if (since_tick > 0u && since_tick < (uint32_t)EN_SR) {   /* the tempo, smoothed */
                    tick_gap = tick_gap > 0.0f ? tick_gap + ((float)since_tick - tick_gap) * 0.02f : (float)since_tick;
                    en_bpm = EN_SR * 60.0f / (24.0f * tick_gap);
                }
                since_tick = 0;
                en_ext = 1;
                if (en_playing) {
                    clk_ticks++;
                    if (!en_frozen) {             /* the day: so many bars of 96 ticks */
                        day_ph += 1.0f / (96.0f * (float)day_bars(par[P_DAY]));
                        if (day_ph >= 1.0f)
                            day_ph -= 1.0f;
                    }
                    if (g && clk_ticks % (uint32_t)g == 0u)
                        fire_pending();
                }
            } else if (a == EN_CLK_START) {       /* from the top: the morning */
                clk_ticks = 0;
                en_ext = 1;
                day_ph = 0.3f;
                if (!en_playing)
                    play_start();
            } else if (a == EN_CLK_CONTINUE) {
                en_ext = 1;
                if (!en_playing)
                    play_start();
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_REGROW: regrow(); break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    const float dt = (float)n * (1.0f / EN_SR);
    float wv = wind_verb * wind_now;
    mods_step(n);
    plant_step(n);
    plant_gate += ((en_playing ? 1.0f : 0.0f) - plant_gate) * fm_minf(dt / (en_playing ? 1.5f : 4.0f), 1.0f);
    pl_decay += ((en_frozen ? 0.9995f : fm_minf(pl_decay_t + 0.07f * wv, 0.975f)) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    pl_exc += ((float)EXC * (1.0f + 3.0f * wv) - pl_exc) * fm_minf(dt * 2.0f, 1.0f);
    pl_lfo = 0.0001f * (1.0f + 2.5f * wv);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    drops_spawn(n);
}

static void render_chunk(int32_t *out, uint32_t n, float gain, uint32_t at)
{
    uint32_t j;
    int b;
    const float rg = fm_minf(rev_send * (1.0f + 1.4f * wind_verb * wind_now), 1.3f) * rev_in_gain;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = 0.0f;
#ifdef EN_HOST
    bus_on = en_bus_on && at + n <= 1024u;
#endif
    if (bus_on)
        for (b = 0; b < EN_NBUS; b++)
            for (j = 0; j < n; j++)
                busb[b][j] = 0.0f;
    render_stem(n);
    render_voices(n);
    render_wind(n);
    render_rain(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r;
        reverb(rsend[j] * rg * 0.8f, &wl, &wr);
        l = (mixl[j] + 1.3f * wl) * 1.3f * gain;
        r = (mixr[j] + 1.3f * wr) * 1.3f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
        if (bus_on)
            busb[5][j] = 0.65f * (wl + wr);
    }
    if (bus_on)                                   /* the voices apart: at the mix's level, soft-limited */
        for (j = 0; j < n; j++)
            for (b = 0; b < EN_NBUS; b++)
                en_bus[(at + j) * EN_NBUS + (uint32_t)b] = fm_tanhf(busb[b][j] * 1.3f * gain * 0.99f);
}

/* the signals as CCs, when they change: every ~46 ms, a few at a time */
static void cc_out(void)
{
    static uint32_t turn;
    static float spark;
    uint8_t v[12];
    int i, sent = 0;
    if (par[P_MIDI] != EN_MIDI_BOTH && par[P_MIDI] != EN_MIDI_CC)
        return;
    spark = fm_maxf(spark * 0.8f, fm_maxf(en_leaf_glow[0], fm_maxf(en_leaf_glow[1], en_leaf_glow[2])));
    for (i = 0; i < EN_NLEAF; i++) {
        v[i] = (uint8_t)(fm_clampf(en_leaf_charge[i], 0.0f, 1.0f) * 127.0f + 0.5f);
        v[3 + i] = (uint8_t)(fm_clampf(en_leaf_water[i], 0.0f, 1.0f) * 127.0f + 0.5f);
    }
    v[6] = (uint8_t)(soil * 127.0f + 0.5f);
    v[7] = (uint8_t)(fm_clampf(en_sun, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[8] = (uint8_t)(fm_clampf(rain_now, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[9] = (uint8_t)(fm_clampf(wind_now, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[10] = (uint8_t)(fm_clampf(stem.amp / fm_maxf(stem_vol, 0.01f), 0.0f, 1.0f) * 127.0f + 0.5f);
    v[11] = (uint8_t)(fm_clampf(spark, 0.0f, 1.0f) * 127.0f + 0.5f);
    for (i = 0; i < 12 && sent < 6; i++) {        /* round the twelve, at most six a turn */
        int k = (int)((turn + (uint32_t)i) % 12u);
        if (v[k] != cc_last[k]) {
            cc_last[k] = v[k];
            en_midi_out(0xB0u | (uint32_t)((par[P_MCH] + 4) & 15), (uint32_t)(EN_CC_CHARGE + k), v[k]);
            sent++;
        }
    }
    turn += 5u;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    uint32_t at = 0;
    static uint32_t cc_left;
    drain();
    if (en_ext && ++ext_age > 100u) {             /* the clock stopped coming (~0.6 s): our own time */
        en_ext = 0;
        fire_pending();
    }
    since_tick += n;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        control(k);
        render_chunk(out, k, gain, at);
        out += 2 * k;
        at += k;
        n -= k;
    }
    if (cc_left <= at) {
        cc_out();
        cc_left = 2048u;
    } else {
        cc_left -= at;
    }
}
