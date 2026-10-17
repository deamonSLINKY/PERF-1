/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (amb.h): GRAV-1, a star and its planets. One compilation unit, -O2. The plate reverb,
 * the command queue and the fast maths are AMB-1's (Charles Vestal), by way of ECO-1; the system is new. */
#include "amb.h"
#include "fastmath.h"

#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Aeolian", "Dorian", "Phrygian", "Lydian", "Pent min", "Pent maj",
                                             "Hirajoshi", "Whole t."};
#define B5(a, b, c, d, e) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B5(0, 2, 3, 5, 7) | 1u << 8 | 1u << 10, B5(0, 2, 3, 5, 7) | 1u << 9 | 1u << 10, B5(0, 1, 3, 5, 7) | 1u << 8 | 1u << 10,
    B5(0, 2, 4, 6, 7) | 1u << 9 | 1u << 11, B5(0, 3, 5, 7, 10), B5(0, 2, 4, 7, 9), B5(0, 2, 3, 7, 8), B5(0, 2, 4, 6, 8) | 1u << 10};
#undef B5
const char *const EN_INST_NAME[EN_NINST] = {"Drone", "Glass bell", "Pluck", "Soft keys", "Choir", "Laser", "Ether", "Ring hiss",
                                            "Abyss", "Dark choir", "Cello", "Gong", "Toll", "Whisper", "Cold pad", "Dusk organ"};
const char *const EN_COLOUR_NAME[EN_NCOLOUR] = {"White", "Pink", "Brown", "Blue", "Violet", "Grey", "Black", "Yellow"};
const char *const EN_STAR_KIND[EN_NCOLOUR] = {"White star", "Pink star", "Brown dwarf", "Blue giant", "Violet star",
                                              "Grey dwarf", "Black hole", "Yellow star"};
const char *const EN_LANE_NAME[EN_NPL] = {"Kick", "Pulsar", "Sonar", "Geiger", "Rim", "Shaker"};
const char *const EN_BIND_NAME[5] = {"Off", "Pump", "Blip", "Flicker", "Nudge"};
static const char *const THEME_NAME[EN_NTHEMES] = {"Deep space", "Nebula", "Aurora", "Red planet", "Phosphor", "Redtail", "Mono"};

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Planets", 1, 6, 6},     {"Tempo", 30, 90, 52},   {"Wander", 0, 100, 35},  {"Phrase", 0, 6, 0},
    {"Star", 0, 100, 55},     {"Colour", 0, 7, 7},     {"Roar", 0, 100, 35},    {"Flares", 0, 100, 20},
    {"P1 level", 0, 100, 85}, {"P1 inst", 0, 15, 0},    {"P1 ellipse", 0, 45, 5},  {"P1 oct", -2, 2, 0},
    {"P2 level", 0, 100, 55}, {"P2 inst", 0, 15, 1},    {"P2 ellipse", 0, 45, 12}, {"P2 oct", -2, 2, 0},
    {"P3 level", 0, 100, 62}, {"P3 inst", 0, 15, 3},    {"P3 ellipse", 0, 45, 20}, {"P3 oct", -2, 2, 0},
    {"P4 level", 0, 100, 50}, {"P4 inst", 0, 15, 6},    {"P4 ellipse", 0, 45, 30}, {"P4 oct", -2, 2, 0},
    {"P5 level", 0, 100, 40}, {"P5 inst", 0, 15, 4},    {"P5 ellipse", 0, 45, 8},  {"P5 oct", -2, 2, 0},
    {"P6 level", 0, 100, 42}, {"P6 inst", 0, 15, 2},    {"P6 ellipse", 0, 45, 25}, {"P6 oct", -2, 2, 0},
    {"Tide", 0, 100, 70},     {"Reach", 5, 40, 18},    {"Wake", 0, 100, 75},    {"Great yr", 0, 1, 0},
    {"Dust", 0, 100, 25},     {"Asteroids", 0, 100, 20}, {"Comets", 0, 100, 20}, {"Events", 0, 100, 60},
    {"Drums", 0, 100, 70},    {"Evolve", 0, 2, 1},     {"Bank", 0, EN_NBANK - 1, 0},
    {"Bind 1", 0, 4, 1}, {"Bind 2", 0, 4, 2}, {"Bind 3", 0, 4, 0}, {"Bind 4", 0, 4, 4}, {"Bind 5", 0, 4, 0}, {"Bind 6", 0, 4, 3},
#define STEPS6 {"Steps", -32768, 32767, 0}, {"Steps", -32768, 32767, 0}, {"Steps", -32768, 32767, 0}, \
               {"Steps", -32768, 32767, 0}, {"Steps", -32768, 32767, 0}, {"Steps", -32768, 32767, 0}
    STEPS6, STEPS6, STEPS6, STEPS6, STEPS6, STEPS6, STEPS6, STEPS6,
#undef STEPS6
    {"Echo", 0, 100, 35},     {"Reverb", 0, 100, 55},  {"Radio", 0, 100, 25},   {"Width", 0, 100, 70},
    {"Root", 0, 11, 2},       {"Tune", -50, 50, 0},    {"Sync", 0, 1, 1},       {"Mode", 0, EN_NMODES - 1, 1},
    {"MIDI out", 0, 3, 1},    {"Channel", 0, 8, 0},
    {"Theme", 0, EN_NTHEMES - 1, 0}, {"Starfield", 0, 100, 100}, {"Orbits", 0, 100, 100}, {"Tide lines", 0, 100, 100},
    {"Change", 0, 2, 0},      {"Every", 0, 5, 0},      {"Pool", 2, EN_NBANK, 4}, {"Fill", 0, 100, 50},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* ------------------------------------------------------------ the pieces --- */
/* ten systems (the FM-1's PRESETS), one for each of the star's colours and two more. Instruments: 0 Drone,
 * 1 Glass bell, 2 Pluck, 3 Soft keys, 4 Choir, 5 Laser, 6 Ether, 7 Ring hiss, 8 Abyss, 9 Dark choir, 10 Cello,
 * 11 Gong, 12 Toll, 13 Whisper, 14 Cold pad, 15 Dusk organ; bindings: 0 Off, 1 Pump,
 * 2 Blip, 3 Flicker, 4 Nudge */
typedef struct {
    const char *name;
    int8_t colour, planets, root, mode, evolve;
    uint8_t tempo, wander, tide, reach, wake, star, roar, flares, echo, reverb, radio, width, dust, rocks, comets;
    int8_t inst[6], level[6], ecc[6], oct[6], bind[6];
    const char *pat[6];
} piece_t;
static const piece_t PIECES[EN_NPIECES] = {
    {"Sol", 7, 6, 2, 1, 1, 52, 35, 70, 18, 75, 55, 35, 20, 35, 55, 25, 70, 25, 20, 20,
     {0, 1, 3, 6, 4, 2}, {85, 55, 62, 50, 40, 42}, {5, 12, 20, 30, 8, 25}, {0, 0, 0, 0, 0, 0}, {1, 2, 0, 4, 0, 3},
     {"x.......x.......", "....x.......x..x", "x...............", "..x..x.x..x..x.x", "................", "x.x.x.x.x.x.x.x."}},
    {"Lumen", 0, 4, 9, 3, 0, 46, 25, 60, 20, 70, 50, 20, 15, 30, 70, 15, 80, 25, 20, 20,
     {4, 1, 6, 7, 2, 3}, {80, 50, 55, 35, 40, 40}, {10, 20, 15, 5, 20, 20}, {0, 0, 0, 0, 0, 0}, {0, 2, 4, 0, 0, 0},
     {"x...............", "x.....x.....x...", "....x.......x...", "................", "x...x...x...x...", "..x...x...x...x."}},
    {"Rosette", 1, 5, 4, 4, 2, 58, 40, 75, 17, 70, 50, 40, 30, 40, 50, 20, 75, 25, 20, 20,
     {0, 3, 1, 5, 6, 2}, {80, 55, 50, 45, 38, 40}, {5, 15, 15, 35, 10, 20}, {0, 0, 0, 0, 0, 0}, {1, 2, 2, 3, 0, 0},
     {"x.......x.......", "x...x.......x...", "..x.....x...x...", ".x.x.x.x.x.x.x.x", "................", "x..x..x..x..x..x"}},
    {"Ember", 2, 3, 0, 0, 0, 40, 20, 80, 22, 80, 65, 60, 10, 25, 60, 30, 60, 50, 15, 10,
     {8, 9, 7, 2, 3, 1}, {85, 60, 40, 40, 40, 40}, {8, 20, 10, 20, 20, 20}, {-1, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0},
     {"x...............", "x.......x...x...", "................", "..x...x...x...x.", "x...x...x...x...", "x.x.x.x.x.x.x.x."}},
    {"Rigel", 3, 6, 7, 5, 2, 64, 45, 65, 16, 70, 55, 30, 40, 45, 50, 20, 85, 25, 20, 20,
     {4, 1, 5, 3, 2, 6}, {80, 55, 45, 50, 42, 36}, {5, 20, 40, 10, 25, 15}, {0, 0, 0, 0, 0, 0}, {1, 2, 4, 0, 3, 0},
     {"x.......x.......", "..x...x...x...x.", "x..x..x..x..x..x", "x...........x...", ".x.x.x.x.x.x.x.x", "................"}},
    {"Veil", 4, 5, 11, 6, 1, 50, 30, 70, 19, 75, 50, 30, 20, 40, 65, 30, 80, 25, 20, 20,
     {6, 1, 4, 7, 2, 3}, {80, 50, 60, 40, 36, 40}, {15, 10, 5, 20, 30, 20}, {0, 0, -1, 0, 0, 0}, {0, 2, 1, 4, 0, 0},
     {"x...........x...", "x.......x.......", "x...............", "....x.......x...", "................", "x...x...x...x..."}},
    {"Cinder", 5, 4, 5, 2, 1, 44, 25, 80, 22, 80, 45, 30, 10, 30, 70, 35, 70, 25, 20, 20,
     {15, 10, 13, 12, 2, 6}, {85, 60, 40, 36, 40, 40}, {10, 25, 10, 10, 20, 20}, {0, 0, 0, 0, 0, 0}, {1, 3, 0, 0, 0, 0},
     {"x.......x.......", "..x.....x.....x.", "................", "................", "x...x...x...x...", "..x...x...x...x."}},
    /* Horizon: the black hole. The darkest and slowest: C# Phrygian at 30 BPM, every voice a dark one, the
     * drums sparse. Planet 6, a Whisper, is the drifter: small, on a long ellipse, its lane empty, so it sleeps
     * and only sighs when a bigger world swings past it */
    {"Horizon", 6, 6, 1, 2, 1, 30, 25, 80, 24, 55, 60, 50, 15, 50, 85, 55, 80, 30, 20, 15,
     {8, 9, 12, 10, 11, 13}, {85, 50, 45, 50, 40, 25}, {5, 15, 25, 10, 20, 45}, {0, 0, -1, 0, -1, 0}, {1, 0, 2, 4, 0, 0},
     {"x...............", "x.......x.......", "..x.......x.....", "x.....x.....x...", "....x...........", "................"}},
    {"Kuiper", 3, 5, 6, 0, 2, 34, 20, 85, 24, 85, 40, 25, 10, 45, 80, 40, 85, 25, 45, 40,
     {8, 6, 13, 14, 12, 10}, {85, 60, 40, 38, 34, 40}, {5, 30, 10, 15, 20, 20}, {-1, 0, 0, 0, 1, 0}, {1, 4, 0, 0, 0, 0},
     {"x...............", "........x.......", "................", "................", "................", "x...x...x...x..."}},
    {"Nova", 0, 6, 10, 1, 1, 70, 50, 60, 15, 70, 50, 55, 70, 35, 50, 20, 80, 20, 30, 55,
     {0, 3, 2, 1, 5, 4}, {80, 55, 50, 45, 40, 45}, {10, 15, 30, 20, 40, 5}, {0, 0, 0, 0, 0, 0}, {1, 2, 3, 0, 4, 0},
     {"x.......x.......", "..x...x...x...x.", "x..x..x..x..x..x", "x.......x...x...", ".x.x.x.x.x.x.x.x", "................"}},
};

/* the eight banks of a system: its pattern, then seven turns of it. 1 later by two steps, 2 doubled half a
 * bar on, 3 thinned to every other hit, 4 a beat later, 5 back to front, 6 the low lanes doubled a beat on and
 * the high ones thinned, 7 a breakdown: the kick and the shaker thinned, the rest silent (their planets asleep) */
static uint32_t rot16(uint32_t b, int k) { k &= 15; return ((b << k) | (b >> (16 - k))) & 0xFFFFu; }
static uint32_t thin16(uint32_t b)
{
    uint32_t o = 0;
    int s, n = 0;
    for (s = 0; s < 16; s++)
        if (b >> s & 1u)
            if (!(n++ & 1))
                o |= 1u << s;
    return o;
}
static uint32_t rev16(uint32_t b)
{
    uint32_t o = 0;
    int s;
    for (s = 0; s < 16; s++)
        if (b >> s & 1u)
            o |= 1u << ((16 - s) & 15);
    return o;
}
static uint32_t bank_turn(int k, int l, uint32_t b)
{
    switch (k) {
    case 1: return rot16(b, 2);
    case 2: return b | rot16(b, 8);
    case 3: return thin16(b);
    case 4: return rot16(b, 4);
    case 5: return rev16(b);
    case 6: return l < 3 ? b | rot16(b, 4) : thin16(b);
    case 7: return l == 0 || l == 5 ? thin16(b) : 0u;
    default: return b;
    }
}
static int16_t pat_bits(const char *s)
{
    uint32_t b = 0;
    int k;
    for (k = 0; k < EN_NSTEP && s[k]; k++)
        if (s[k] == 'x')
            b |= 1u << k;
    return (int16_t)(uint16_t)b;
}

/* each system's Drift: change (0 Off, 1 Next, 2 Random), when (0 the Great Year, 1-5 every 4, 8, 16, 32, 64
 * bars), pool (banks 1..n), fill */
static const uint8_t HOP[EN_NPIECES][4] = {
    {1, 0, 4, 50}, {0, 0, 4, 50}, {2, 3, 4, 40}, {0, 0, 4, 50}, {1, 2, 3, 60},
    {2, 0, 6, 40}, {1, 0, 2, 30}, {2, 0, 3, 20}, {0, 0, 4, 50}, {2, 2, 5, 70}};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *p, uint16_t *pool)
{
    const piece_t *s;
    int k;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    s = &PIECES[i];
    for (k = 0; k < P_NPARAMS; k++)
        p[k] = PARAMS[k].def;
    p[P_PLANETS] = s->planets;
    p[P_TEMPO] = s->tempo;
    p[P_WANDER] = s->wander;
    p[P_STAR] = s->star;
    p[P_COLOUR] = s->colour;
    p[P_ROAR] = s->roar;
    p[P_FLARES] = s->flares;
    for (k = 0; k < EN_NPL; k++) {
        p[P_PL_LVL(k)] = s->level[k];
        p[P_PL_INST(k)] = s->inst[k];
        p[P_PL_ECC(k)] = s->ecc[k];
        p[P_PL_OCT(k)] = s->oct[k];
        p[P_BIND + k] = s->bind[k];
        int b;
        for (b = 0; b < EN_NBANK; b++)
            p[P_PAT_OF(b, k)] = (int16_t)(uint16_t)bank_turn(b, k, (uint16_t)pat_bits(s->pat[k]));
    }
    p[P_TIDE] = s->tide;
    p[P_REACH] = s->reach;
    p[P_WAKE] = s->wake;
    p[P_DUST] = s->dust;
    p[P_ROCKS] = s->rocks;
    p[P_COMETS] = s->comets;
    p[P_EVOLVE] = s->evolve;
    p[P_HOP] = HOP[i][0];
    p[P_HOPWHEN] = HOP[i][1];
    p[P_HOPPOOL] = HOP[i][2];
    p[P_HOPFILL] = HOP[i][3];
    p[P_ECHO] = s->echo;
    p[P_REVERB] = s->reverb;
    p[P_RADIO] = s->radio;
    p[P_WIDTH] = s->width;
    p[P_ROOT] = s->root;
    p[P_MODE] = s->mode;
    *pool = EN_MODE_POOL[s->mode];
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

void en_param_text(int i, int v, char *b)
{
    static const char *const MIDI_NAME[4] = {"Off", "Notes", "Notes+CC", "CC"};
    static const char *const SYNC_NAME[2] = {"Free", "Clock"};
    static const char *const EVOLVE_NAME[3] = {"Off", "Shift", "Morph"};
    static const char *const HOP_NAME[3] = {"Off", "Next", "Random"};
    static const char *const WHEN_NAME[6] = {"Great yr", "4 bars", "8 bars", "16 bars", "32 bars", "64 bars"};
    static const char *const GY_NAME[2] = {"All", "Active"};
    static const char *const PHRASE_NAME[7] = {"Auto", "3", "4", "5", "6", "7", "8"};
    const char *s = 0;
    if (i == P_MIDI)
        s = MIDI_NAME[(unsigned)v < 4u ? v : 0];
    else if (i == P_SYNC)
        s = SYNC_NAME[v ? 1 : 0];
    else if (i == P_EVOLVE)
        s = EVOLVE_NAME[(unsigned)v < 3u ? v : 0];
    else if (i == P_HOP)
        s = HOP_NAME[(unsigned)v < 3u ? v : 0];
    else if (i == P_HOPWHEN)
        s = WHEN_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_GY)
        s = GY_NAME[v ? 1 : 0];
    else if (i == P_PHRASE)
        s = PHRASE_NAME[(unsigned)v < 7u ? v : 0];
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i == P_COLOUR)
        s = EN_COLOUR_NAME[(unsigned)v < EN_NCOLOUR ? v : 0];
    else if (i == P_THEME)
        s = THEME_NAME[(unsigned)v < EN_NTHEMES ? v : 0];
    else if (i >= P_BIND && i < P_BIND + EN_NPL)
        s = EN_BIND_NAME[(unsigned)v < 5u ? v : 0];
    else if (i >= P_PL && i < P_TIDE && (i - P_PL) % 4 == 1)
        s = EN_INST_NAME[(unsigned)v < EN_NINST ? v : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 10) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_MCH) {                            /* the channels it takes: 1-8 .. 9-16 */
        b = itoa_s(v + 1, b);
        *b++ = '-';
        itoa_s(v + 8, b);
        return;
    }
    if (i >= P_PL && i < P_TIDE && (i - P_PL) % 4 == 2) {   /* an ellipse: 0.00 .. 0.45 */
        *b++ = '0';
        *b++ = '.';
        *b++ = (char)('0' + v / 10);
        *b++ = (char)('0' + v % 10);
        *b = 0;
        return;
    }
    if (i == P_REACH) {
        *b++ = '0';
        *b++ = '.';
        *b++ = (char)('0' + v / 10);
        *b++ = (char)('0' + v % 10);
        *b = 0;
        return;
    }
    if (i == P_HOPPOOL) {                        /* the banks Random picks from: 1-n */
        *b++ = '1';
        *b++ = '-';
        itoa_s(v, b);
        return;
    }
    if (i == P_TEMPO) {
        b = itoa_s(v, b);
        *b++ = ' ';
        *b++ = 'b';
        *b++ = 'p';
        *b++ = 'm';
        *b = 0;
        return;
    }
    if (i == P_TUNE || (i >= P_PL && i < P_TIDE && (i - P_PL) % 4 == 3)) {
        if (v > 0)
            *b++ = '+';
        itoa_s(v, b);
        return;
    }
    b = itoa_s(v, b);
    if (i != P_PLANETS) {
        *b++ = '%';
        *b = 0;
    }
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
float en_period_bars(int hits) { return hits > 0 ? 32.0f / (float)hits : 64.0f; }

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_TOUCH, C_SUMMON, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_STRIKE, C_MUTE };
static uint8_t muted;                           /* remote: the FM-1 is playing the browser */
static float mute_g = 1.0f;
#define QN 256u
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
void en_summon(int what) { post(C_SUMMON, what, 0); }
void en_strike(int planet) { post(C_STRIKE, planet, 0); }
void en_mute(int on) { post(C_MUTE, on, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile float en_bpm = 52.0f;
volatile uint8_t en_bus_on;
#ifdef EN_HOST
float en_bus[EN_NBUS * 1024];
#else
float en_bus[EN_NBUS];                          /* the FM-1 has no use for them yet: no room taken */
#endif
volatile float en_pl_x[EN_NPL], en_pl_y[EN_NPL], en_pl_a[EN_NPL], en_pl_om[EN_NPL], en_pl_bias[EN_NPL], en_pl_dist[EN_NPL];
volatile float en_pl_tide[EN_NPL], en_pl_glow[EN_NPL], en_pl_linkc[EN_NPL];
volatile int8_t en_pl_link[EN_NPL];
volatile uint8_t en_pl_awake[EN_NPL], en_pl_notes[EN_NPL], en_pl_note[EN_NPL];
volatile float en_drum_glow[EN_NPL];
volatile uint16_t en_pat[EN_NPL];
volatile uint8_t en_bank;
static int bank_play;                           /* the bank playing; par[P_BANK] the one asked for */
volatile int8_t en_hop_left = -1;
volatile uint32_t en_hops;
static int8_t hop_in = -1;                      /* Drift: bar lines until the bank changes, -1 none */
static uint8_t hop_fill;                        /* this bar is the fill */
static uint32_t hop_bars;                       /* bars since the last change (Every) */
volatile uint8_t en_step;
volatile float en_year, en_star, en_flare, en_roar, en_dust;
volatile uint8_t en_rock_on, en_comet_on;
volatile float en_rock_x, en_rock_y, en_rock_vx, en_rock_vy, en_comet_r, en_comet_ang;
volatile uint8_t en_chord;
volatile uint32_t en_notes, en_hits, en_years, en_greats, en_wakes, en_rocks, en_comets, en_dusts;
volatile uint16_t en_ev[EN_NEV];
volatile uint32_t en_ev_w;

static int16_t par[P_NPARAMS];
static uint32_t pool = 0x6ADu;                  /* Dorian until told */
static int nsteps = 7, steps[12] = {0, 2, 3, 5, 7, 9, 10};
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u, noise_l = 22222u, noise_r = 7777u, noise_s = 31337u;

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
static float lvl(int v) { return (float)(v * v) * 0.0001f; }
static float pct(int v) { return (float)v * 0.01f; }
static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static inline float fm_cos_turns(float t) { return fm_sin_turns(t + 0.25f); }
static float onepole(float fc) { return 1.0f - fm_expf(-FM_TWO_PI * fm_minf(fc, 18000.0f) * (1.0f / EN_SR)); }

static void event(int code, int detail)
{
    uint32_t w = en_ev_w;
    en_ev[w % EN_NEV] = (uint16_t)(code << 8 | (detail & 0xFF));
    BARRIER();
    en_ev_w = w + 1;
}

/* MIDI notes out: planet n on MCH + n, the drums on MCH + 6 */
static void note(int on, int ch, uint32_t n, uint32_t vel)
{
    if (!muted && (par[P_MIDI] == EN_MIDI_NOTES || par[P_MIDI] == EN_MIDI_BOTH))
        en_midi_out((on ? 0x90u : 0x80u) | (uint32_t)((par[P_MCH] + ch) & 15), n, on ? vel : 0u);
}

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
static void pan_gains(float pn, float *gl, float *gr)
{
    pn = fm_clampf(pn, -1.0f, 1.0f);
    *gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f));
    *gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f));
}

/* ---------------------------------------------------------- the mix --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];
#ifdef EN_HOST
static float busb[EN_NBUS][BLK];                /* the voices apart (en_bus_on) */
static uint8_t bus_on;
#define BUS(b, j, x) do { if (bus_on) busb[b][j] += (x); } while (0)
#else
#define bus_on 0
#define BUS(b, j, x) do { (void)(b); } while (0)
#endif

/* ------------------------------------------------------------ the planets --- */
#define NV 14                                   /* voices: two a planet sounding, and room for tails */
#define REST (-128)
typedef struct {
    float ang;                                  /* the year so far, turns 0..1 */
    float period;                               /* its year, in bars */
    float a, a_t;                               /* the orbit's size now, and where it is drifting */
    float om;                                   /* where the orbit is nearest the star (turns) */
    float bias, bias_t;                         /* its place on a shared orbit */
    float x, y, dist, tide, glow, linkc;
    int8_t link;
    int8_t ph[8], pho[8];                       /* its phrase: steps of the scale (REST: none), octaves */
    uint8_t n;                                  /* points on its orbit */
    uint8_t awake, wake_i;
    int8_t held;                                /* the voice a woken planet holds, -1 none */
    float lastf;                                /* its last note's frequency (the Ether glides from it) */
    /* its sound now (once a chunk) */
    float c_lp, c_hp, gl, gr, s_rev, s_echo, mod, blip, nudge;
    float flick_t;
    uint8_t flick;
} planet_t;
static planet_t pl[EN_NPL];
static int chord_i;                             /* the Great Year's chord, in the progression */

static int hits_of(int l)
{
    uint32_t b = (uint16_t)par[P_PAT_OF(bank_play, l)];
    int n = 0;
    while (b) {
        n += (int)(b & 1u);
        b >>= 1;
    }
    return n;
}
static int nplanets(void) { return par[P_PLANETS]; }
static int dormant(int i) { return hits_of(i) == 0; }

/* the instrument bank: FM over sines. RATIO the modulator, IDX its index and IDX_T its fall; ATT the
 * attack; SUS: holds while the note lasts, then REL; else decays to -60 dB in REL. DET a second carrier
 * that many cents up (the drone's and the choir's width), SUB a sine an octave down, SWEEP a fall from that
 * many times the pitch (the laser), VIB vibrato in cents, NOISE a resonant band of noise at the pitch
 * (the ring hiss), REG the register */
static const struct {
    float ratio, idx, idx_t, att, rel, det, sub, sweep, vib, noise, gain;
    int8_t sus, reg;
} INST[EN_NINST] = {
    {1.0f, 1.0f, 0.0f, 2.2f, 4.0f, 8.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.75f, 1, -12},   /* Drone */
    {3.5f, 1.8f, 0.6f, 0.003f, 4.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0, 12},   /* Glass bell */
    {1.0f, 2.2f, 0.15f, 0.002f, 1.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.85f, 0, 0},  /* Pluck */
    {1.0f, 0.55f, 0.8f, 0.01f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0, 0},    /* Soft keys */
    {2.0f, 0.3f, 0.0f, 1.5f, 2.5f, 12.0f, 0.0f, 0.0f, 8.0f, 0.0f, 0.85f, 1, 0},    /* Choir */
    {1.0f, 2.0f, 0.25f, 0.002f, 1.6f, 0.0f, 0.0f, 5.0f, 0.0f, 0.0f, 0.6f, 0, 12},  /* Laser */
    {1.0f, 0.0f, 0.0f, 0.5f, 1.8f, 0.0f, 0.0f, 0.0f, 16.0f, 0.0f, 0.85f, 1, 12},   /* Ether */
    {1.0f, 0.0f, 0.0f, 1.2f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.025f, 0.9f, 1, 12},   /* Ring hiss */
    /* the dark ones */
    {1.0f, 0.35f, 0.0f, 3.0f, 5.0f, 5.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.2f, 1, -24},   /* Abyss: a sub, two octaves down, barely moving */
    {1.0f, 0.45f, 0.0f, 2.2f, 3.5f, 9.0f, 0.3f, 0.0f, 6.0f, 0.0f, 1.0f, 1, -12},  /* Dark choir: low voices, slowly beating */
    {1.0f, 1.5f, 0.0f, 0.6f, 2.2f, 4.0f, 0.0f, 0.0f, 12.0f, 0.0f, 0.75f, 1, -12},  /* Cello: bowed, dark, a vibrato growing in */
    {1.41f, 3.0f, 1.2f, 0.004f, 9.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.35f, 0, -12},/* Gong: inharmonic, long */
    {2.76f, 1.6f, 1.6f, 0.003f, 7.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 1.45f, 0, -12}, /* Toll: a low bell, a hum under it */
    {1.0f, 0.0f, 0.0f, 1.0f, 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 1.6f, 1, 0},     /* Whisper: breath in a wide band */
    {3.0f, 0.4f, 0.0f, 3.0f, 5.0f, 6.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.95f, 1, 0},      /* Cold pad: glassy, slow in, slow out */
    {2.0f, 0.8f, 0.0f, 0.25f, 1.5f, 3.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.8f, 1, -12},   /* Dusk organ: a low reed organ, a sub */
};

typedef struct {
    float cph, mph, c2ph, sph, cinc, inc_t, minc, det, idx, idec, amp, att, arate, rdec, rdec_rel;
    float sweep, sdec, vib, vibd, vibd_t, lp, hp, bl, bb, bf, gain;
    int32_t wait, hold;                          /* samples before it starts; held (-1 = until let go) */
    uint8_t on, pl, inst, note;
} voice_t;
static voice_t vc[NV];
static uint32_t vc_age[NV], vc_clock;

static void voice_off(int k)
{
    if (vc[k].on)
        note(0, vc[k].pl, vc[k].note, 0);
    vc[k].on = 0;
}

static int voice_alloc(int p)                   /* a free voice; else this planet's oldest; else any oldest */
{
    int k, best = -1, mine = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    for (k = 0; k < NV; k++)
        if (!vc[k].on)
            return k;
    for (k = 0; k < NV; k++)
        if (vc[k].pl == p && vc[k].hold >= 0)
            mine++;
    for (k = 0; k < NV; k++) {
        if (vc[k].hold < 0)                      /* a woken planet's held note stays */
            continue;
        if (mine >= 2 && vc[k].pl != p)
            continue;
        if (vc_age[k] < oldest) {
            oldest = vc_age[k];
            best = k;
        }
    }
    if (best < 0)
        best = 0;
    voice_off(best);
    return best;
}

/* a note on planet p: dur in seconds (sustained instruments hold that long), held: until let go */
static int voice_start(int p, int nn, float vel, float dur, int32_t wait, int held)
{
    int k = voice_alloc(p), ins = par[P_PL_INST(p)];
    voice_t *v = &vc[k];
    float f;
    if (ins < 0 || ins >= EN_NINST)
        ins = 0;
    f = mtof((float)nn);
    v->inst = (uint8_t)ins;
    v->pl = (uint8_t)p;
    v->note = (uint8_t)nn;
    v->inc_t = f * (1.0f / EN_SR);
    v->cinc = (ins == 6 && pl[p].lastf > 0.0f) ? pl[p].lastf * (1.0f / EN_SR) : v->inc_t;   /* the Ether glides */
    v->minc = v->inc_t * INST[ins].ratio;
    v->det = fm_exp2f(INST[ins].det * (1.0f / 1200.0f));
    v->cph = v->mph = 0.0f;
    v->c2ph = 0.37f;
    v->sph = 0.0f;
    v->idx = INST[ins].idx;
    v->idec = INST[ins].idx_t > 0.0f ? fm_expf(-1.0f / (INST[ins].idx_t * EN_SR)) : 1.0f;
    v->amp = vel;
    v->att = 0.0f;
    v->arate = 1.0f - fm_expf(-1.0f / (INST[ins].att * 0.4f * EN_SR));
    v->rdec_rel = fm_expf(-6.9f / (INST[ins].rel * EN_SR));
    v->sweep = INST[ins].sweep;
    v->sdec = fm_expf(-1.0f / (0.06f * EN_SR));
    v->vib = frand();
    v->vibd = 0.0f;
    v->vibd_t = INST[ins].vib;
    v->lp = v->hp = v->bl = v->bb = 0.0f;
    v->bf = 2.0f * fm_sinf(FM_PI * fm_minf(f, 6000.0f) * (1.0f / EN_SR));
    v->gain = INST[ins].gain;
    v->wait = wait;
    if (held)
        v->hold = -1;
    else if (INST[ins].sus)
        v->hold = (int32_t)(fm_maxf(dur - INST[ins].att, 0.15f) * EN_SR);
    else
        v->hold = 0;
    v->rdec = INST[ins].sus ? 1.0f : v->rdec_rel;
    v->on = 1;
    vc_age[k] = ++vc_clock;
    pl[p].lastf = f;
    pl[p].glow = 1.0f;
    en_pl_note[p] = (uint8_t)nn;
    note(1, p, (uint32_t)nn, (uint32_t)(30.0f + 97.0f * fm_minf(vel / 0.2f, 1.0f)));
    en_notes++;
    return k;
}
static void voice_release(int k)
{
    if (k < 0 || k >= NV || !vc[k].on)
        return;
    vc[k].hold = 0;
    vc[k].rdec = vc[k].rdec_rel;
}

/* the scale, the chord and a phrase's note */
static int walk(int d)
{
    static const int8_t ST[11] = {-2, -1, -1, -1, 0, 1, 1, 1, 2, -3, 4};
    d += ST[irand(11)];
    return d < -3 ? -3 : d > 9 ? 9 : d;
}
static int chord_shift(void)
{
    static const int8_t PROG[8] = {0, 5, 3, 4, 0, 2, 5, 6};
    return nsteps > 0 ? PROG[chord_i & 7] % nsteps : 0;
}
static int phrase_note(int p, int deg, int oct)
{
    int d = deg + chord_shift(), o, idx, n;
    if (nsteps <= 0)
        return 60;
    o = d >= 0 ? d / nsteps : -((-d + nsteps - 1) / nsteps);
    idx = d - o * nsteps;
    n = 48 + par[P_ROOT] + steps[idx] + 12 * (o + oct + par[P_PL_OCT(p)]) + INST[(unsigned)par[P_PL_INST(p)] < EN_NINST ? par[P_PL_INST(p)] : 0].reg;
    return n < 24 ? 24 : n > 100 ? 100 : n;
}
static int phrase_len(int p)
{
    int h = hits_of(p);
    if (par[P_PHRASE])
        return par[P_PHRASE] + 2;
    return h ? 4 + (4 * (16 - h) + 8) / 16 : 4;
}
static void phrase_new(int p)
{
    int n = phrase_len(p), k, d = 2 * irand(3);
    for (k = 0; k < n; k++) {
        pl[p].ph[k] = (int8_t)((k > 0 && frand() < 0.15f) ? REST : d);
        pl[p].pho[k] = 0;
        d = walk(d);
    }
    pl[p].n = (uint8_t)n;
}
static void phrase_fit(int p)                   /* a new length: keep what there is, walk on from it */
{
    int n = phrase_len(p), k, d = 0;
    if (n == pl[p].n)
        return;
    for (k = 0; k < pl[p].n && k < n; k++)
        if (pl[p].ph[k] != REST)
            d = pl[p].ph[k];
    for (k = pl[p].n; k < n; k++) {
        d = walk(d);
        pl[p].ph[k] = (int8_t)d;
        pl[p].pho[k] = 0;
    }
    pl[p].n = (uint8_t)n;
}
static void phrase_mutate(int p)                /* a year: a note or two change (Wander) */
{
    float w = pct(par[P_WANDER]);
    int n = pl[p].n, c, k, any = 0;
    int m = (int)(w * (float)n * 0.5f + 0.5f);
    for (c = 0; c < m; c++) {
        k = irand(n);
        if (pl[p].ph[k] == REST) {
            int nb = pl[p].ph[(k + n - 1) % n];
            pl[p].ph[k] = (int8_t)walk(nb == REST ? 0 : nb);
        } else if (frand() < 0.12f) {
            pl[p].ph[k] = REST;
        } else {
            int d = pl[p].ph[k] + (frand() < 0.5f ? -1 : 1) * ((w > 0.6f && frand() < 0.3f) ? 2 : 1);
            pl[p].ph[k] = (int8_t)(d < -3 ? -3 : d > 9 ? 9 : d);
            if (frand() < w * 0.15f) {
                int o = pl[p].pho[k] + (frand() < 0.5f ? -1 : 1);
                pl[p].pho[k] = (int8_t)(o < -1 ? -1 : o > 1 ? 1 : o);
            }
        }
    }
    for (k = 0; k < n; k++)
        any |= pl[p].ph[k] != REST;
    if (!any)
        pl[p].ph[0] = 0;
}

/* the orbits: Kepler's third law (the size grows as the year's two-thirds power), kept apart; planets
 * sharing a year share an orbit, 60 degrees apart; dormant planets just outside the outermost */
static float orbit_r(float P)
{
    const float r2 = fm_powf(2.0f, 2.0f / 3.0f), r64 = fm_powf(64.0f, 2.0f / 3.0f);
    return 0.2f + 0.46f * fm_clampf((fm_powf(P, 2.0f / 3.0f) - r2) / (r64 - r2), 0.0f, 1.0f);
}
static float wrap_half(float t) { return t - fm_floorf(t + 0.5f); }   /* turns to -0.5..0.5 */
static void orbits(int snap)
{
    int n = nplanets(), i, j, k, dk = 0;
    float prev = -1.0f, rout = 0.2f;
    uint8_t done[EN_NPL] = {0};
    for (;;) {                                   /* the live planets, by year, shortest first */
        float P = 1e9f;
        for (i = 0; i < n; i++)
            if (!done[i] && !dormant(i) && pl[i].period < P)
                P = pl[i].period;
        if (P > 1e8f)
            break;
        {
            float r = orbit_r(P);
            int lead = -1;
            if (prev > 0.0f && r < prev + 0.065f)
                r = prev + 0.065f;
            r = fm_minf(r, 0.66f);
            prev = r;
            rout = fm_maxf(rout, r);
            for (i = 0, k = 0; i < n; i++) {
                if (done[i] || dormant(i) || pl[i].period != P)
                    continue;
                done[i] = 1;
                pl[i].a_t = r;
                if (lead < 0) {
                    lead = i;
                    pl[i].bias_t = pl[i].bias;
                } else {                         /* on the lead's orbit, k sixths of a turn on */
                    float want = pl[lead].ang + pl[lead].bias + (float)k / 6.0f - pl[i].ang;
                    pl[i].bias_t = pl[i].bias + wrap_half(want - pl[i].bias);
                }
                k++;
            }
        }
    }
    for (i = 0; i < n; i++)
        if (dormant(i)) {
            pl[i].a_t = fm_minf(0.7f, (prev > 0.0f ? rout : 0.25f) + 0.085f * (float)(++dk));
            pl[i].bias_t = pl[i].bias;
        }
    for (j = n; j < EN_NPL; j++)
        pl[j].bias_t = pl[j].bias;
    if (snap)
        for (i = 0; i < EN_NPL; i++) {
            pl[i].a = pl[i].a_t;
            pl[i].bias = pl[i].bias_t;
        }
}
static void periods(void)                       /* the lanes' hits to the planets' years */
{
    int i;
    for (i = 0; i < EN_NPL; i++) {
        float P = en_period_bars(hits_of(i));
        if (P != pl[i].period)
            pl[i].period = P;
        phrase_fit(i);
        en_pat[i] = (uint16_t)par[P_PAT_OF(bank_play, i)];
    }
    orbits(0);
}
static int outermost(void)
{
    int i, best = -1;
    for (i = 0; i < nplanets(); i++) {
        if (par[P_GY] && dormant(i))
            continue;
        if (best < 0 || pl[i].period > pl[best].period)
            best = i;
    }
    return best;
}

/* ------------------------------------------------------------- the drums --- */
typedef struct {
    float ph, inc, incf, amp, adec, nz, ndec, att, lp, bl, bb, bf, bq;
    int32_t wait;
    uint8_t on;
} drum_t;
static drum_t dr[EN_NPL];
static uint8_t drum_noteon;                     /* lanes whose MIDI note is sounding */
static float drum_vol;

static void drum_hit(int l, int32_t wait)
{
    drum_t *d = &dr[l];
    d->wait = wait;
    d->ph = 0.0f;
    d->att = 1.0f;
    d->lp = d->bl = d->bb = 0.0f;
    d->nz = 0.0f;
    d->ndec = 0.0f;
    d->bq = 1.0f;
    switch (l) {
    case 0: d->inc = 110.0f / EN_SR; d->incf = fm_expf(-1.0f / (0.09f * EN_SR)); d->amp = 0.55f; d->adec = fm_expf(-1.0f / (0.22f * EN_SR)); break;
    case 1: d->inc = 2200.0f / EN_SR; d->incf = 1.0f; d->amp = 0.3f; d->adec = fm_expf(-1.0f / (0.015f * EN_SR));
            d->nz = 0.5f; d->ndec = fm_expf(-1.0f / (0.004f * EN_SR)); d->bf = 2.0f * fm_sinf(FM_PI * 5000.0f / EN_SR); d->bq = 1.2f; break;
    case 2: d->inc = 1250.0f / EN_SR; d->incf = fm_expf(-1.0f / (6.0f * EN_SR)); d->amp = 0.11f; d->adec = fm_expf(-1.0f / (0.35f * EN_SR)); break;
    case 3: d->inc = 0.0f; d->incf = 1.0f; d->amp = 0.0f; d->adec = 0.0f;
            d->nz = 0.45f; d->ndec = fm_expf(-1.0f / (0.0015f * EN_SR)); d->bf = 2.0f * fm_sinf(FM_PI * 4000.0f / EN_SR); d->bq = 0.8f; break;
    case 4: d->inc = 820.0f / EN_SR; d->incf = 1.0f; d->amp = 0.2f; d->adec = fm_expf(-1.0f / (0.025f * EN_SR));
            d->nz = 0.12f; d->ndec = fm_expf(-1.0f / (0.02f * EN_SR)); d->bf = 2.0f * fm_sinf(FM_PI * 2200.0f / EN_SR); d->bq = 0.5f; break;
    default: d->inc = 0.0f; d->incf = 1.0f; d->amp = 0.0f; d->adec = 0.0f;
            d->nz = 0.16f; d->ndec = fm_expf(-1.0f / (0.04f * EN_SR)); d->att = 0.0f; d->bf = 2.0f * fm_sinf(FM_PI * 5800.0f / EN_SR); d->bq = 0.6f; break;
    }
    d->on = 1;
    en_hits++;
    en_drum_glow[l] = 1.0f;
}

static void render_drums(uint32_t n)
{
    int l;
    const float g = drum_vol * 1.1f;
    for (l = 0; l < EN_NPL; l++) {
        drum_t *d = &dr[l];
        uint32_t j = 0;
        float ph, inc, amp, nz, att, bl, bb, send = l == 2 ? 0.6f : 0.25f;
        if (!d->on)
            continue;
        if (d->wait > 0) {
            if ((uint32_t)d->wait >= n) {
                d->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)d->wait;
            d->wait = 0;
        }
        ph = d->ph, inc = d->inc, amp = d->amp, nz = d->nz, att = d->att, bl = d->bl, bb = d->bb;
        for (; j < n; j++) {
            float x = 0.0f;
            if (amp > 0.0f) {
                x = (l == 4 ? 2.0f * fm_fabsf(2.0f * (ph - (float)(int)ph) - 1.0f) - 1.0f : sine(ph)) * amp;   /* the rim: a triangle */
                ph += inc;
                inc *= d->incf;
                if (l == 0 && inc < 42.0f / EN_SR)
                    inc = 42.0f / EN_SR;
                amp *= d->adec;
            }
            if (nz > 0.0f) {                     /* noise through a band */
                float w = wnoise(&noise_s), hi;
                att += (1.0f - att) * 0.0019f;   /* the shaker's 12 ms swell; the others start full */
                bl += d->bf * bb;
                hi = w - bl - d->bq * bb;
                bb += d->bf * hi;
                x += bb * nz * att;
                nz *= d->ndec;
            }
            x *= g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * send;
            BUS(3, j, x);
        }
        if (ph >= 1.0f)
            ph -= (float)(int)ph;
        d->ph = ph, d->inc = inc, d->amp = amp, d->nz = nz, d->att = att, d->bl = fm_flush(bl), d->bb = fm_flush(bb);
        if (amp < 0.0005f && nz < 0.0005f)
            d->on = 0;
    }
}

/* a hit's binding, on its planet */
static void bind(int l)
{
    planet_t *p = &pl[l];
    switch (par[P_BIND + l]) {
    case 1: p->mod = 0.4f; break;                /* pump: ducks, comes back over 0.2 s */
    case 2: p->blip = 1.0f; break;               /* blip: a brightness, falling */
    case 3: p->flick = 6; p->flick_t = 0.0f; break;   /* flicker: six quick cuts */
    case 4: p->nudge = 0.0204f; break;           /* nudge: 35 cents up, gliding back */
    default: break;
    }
}

/* --------------------------------------------------------------- the star --- */
/* its noise in eight colours (HYP-1's): white; pink (Paul Kellet's filter); brown (a leaky sum); blue (pink,
 * differenced); violet (white, differenced); grey (loud at both ends, as the ear hears flat); black (mostly
 * silence: crackle); yellow (pink through a band at 1.8 kHz: a radio's hiss). Each scaled to the same
 * loudness (black quieter). A new colour crossfades in over a second. */
typedef struct {
    float b0, b1, b2, b3, b4, b5, b6, br, pp, pw, crack, x1, x2, y1, y2;
} nz_t;
static nz_t nzs[2];
static const float NZ_NORM[EN_NCOLOUR] = {0.381f, 0.127f, 3.88f, 0.21f, 0.269f, 0.55f, 1.7f, 0.351f};
static float yb0, ya1, ya2;                     /* the yellow's band: a biquad (RBJ, Q 1) */
static inline float noise_colour(int k, nz_t *s, uint32_t *seed)
{
    float w = wnoise(seed), pink, v;
    s->b0 = 0.99886f * s->b0 + w * 0.0555179f;
    s->b1 = 0.99332f * s->b1 + w * 0.0750759f;
    s->b2 = 0.969f * s->b2 + w * 0.153852f;
    s->b3 = 0.8665f * s->b3 + w * 0.3104856f;
    s->b4 = 0.55f * s->b4 + w * 0.5329522f;
    s->b5 = -0.7616f * s->b5 - w * 0.016898f;
    pink = s->b0 + s->b1 + s->b2 + s->b3 + s->b4 + s->b5 + s->b6 + w * 0.5362f;
    s->b6 = w * 0.115926f;
    s->br = (s->br + 0.02f * w) * (1.0f / 1.02f);
    switch (k) {
    case 0: v = w; break;
    case 1: v = pink; break;
    case 2: v = s->br; break;
    case 3: v = pink - s->pp; break;
    case 4: v = w - s->pw; break;
    case 5: v = s->br * 3.0f + (w - s->pw) * 0.35f + w * 0.15f; break;
    case 6:
        if ((*seed >> 9) % 1100u == 0u)
            s->crack = w * 1.6f;
        v = s->br * 0.15f + s->crack;
        s->crack *= 0.86f;
        break;
    default: {
        float y = yb0 * (pink - s->x2) - ya1 * s->y1 - ya2 * s->y2;
        s->x2 = s->x1;
        s->x1 = pink;
        s->y2 = s->y1;
        s->y1 = y;
        v = y;
    }
    }
    s->pp = pink;
    s->pw = w;
    return v * NZ_NORM[k];
}
static struct {
    int k, k_old;
    float xf;                                   /* the new colour's share, 0..1 */
    float amp, amp_t, c, c_t, lp1, lp2, hlp, shelf, shelf_t, sub, sub_t, sph[2];
    float roar, roar_t, roar_left, flare;
} st;

static void render_star(uint32_t n)
{
    float a = st.lp1, b = st.lp2, h = st.hlp, amp = st.amp, c = st.c, sh = st.shelf, sub = st.sub;
    float i0 = mtof((float)(24 + par[P_ROOT])) * (1.0f / EN_SR), p0 = st.sph[0], p1 = st.sph[1];
    const float ca = (st.amp_t - amp) * (1.0f / (float)BLK), cc = (st.c_t - c) * (1.0f / (float)BLK);
    const float cs = (st.shelf_t - sh) * (1.0f / (float)BLK), cu = (st.sub_t - sub) * (1.0f / (float)BLK);
    const float hc = onepole(3000.0f);
    uint32_t j;
    if (amp < 1e-6f && st.amp_t < 1e-6f && sub < 1e-6f && st.sub_t < 1e-6f) {
        st.amp = st.amp_t;
        st.sub = st.sub_t;
        return;
    }
    for (j = 0; j < n; j++) {
        float x = noise_colour(st.k, &nzs[0], &noise_l), y;
        if (st.xf < 1.0f) {                      /* the old colour, fading out */
            x = x * st.xf + noise_colour(st.k_old, &nzs[1], &noise_r) * (1.0f - st.xf);
            st.xf += 1.0f / EN_SR;
        }
        a += c * (x - a);
        b += c * (a - b);
        h += hc * (x - h);
        y = (b + sh * (x - h)) * amp + sub * (sine(p0) + 0.45f * sine(p1));
        p0 += i0;
        p1 += i0 * 2.0f;
        if (p0 >= 1.0f) p0 -= 1.0f;
        if (p1 >= 1.0f) p1 -= 1.0f;
        mixl[j] += y;
        mixr[j] += y;
        rsend[j] += y * 0.3f;
        BUS(0, j, y);
        amp += ca;
        c += cc;
        sh += cs;
        sub += cu;
    }
    st.lp1 = fm_flush(a);
    st.lp2 = fm_flush(b);
    st.hlp = fm_flush(h);
    st.amp = st.amp_t;
    st.c = st.c_t;
    st.shelf = st.shelf_t;
    st.sub = st.sub_t;
    st.sph[0] = p0;
    st.sph[1] = p1;
}

/* ------------------------------------------------------------- the voices --- */
static void render_voices(uint32_t n)
{
    int k;
    for (k = 0; k < NV; k++) {
        voice_t *v = &vc[k];
        planet_t *p;
        uint32_t j = 0;
        float cph, mph, c2ph, sph, amp, att, idx, sweep, lp, hp, bl, bb, gl, gr, sr, se, fac;
        int ins, b;
        if (!v->on)
            continue;
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        p = &pl[v->pl];
        ins = v->inst;
        b = v->pl < 3 ? 1 : 2;
        /* the slow things, once a chunk: the glide, the vibrato, the nudge */
        v->cinc += (v->inc_t - v->cinc) * 0.08f;
        v->vibd += (v->vibd_t - v->vibd) * (INST[ins].att >= 0.5f ? 0.005f : 1.0f);   /* the slow ones grow into it */
        v->vib += (float)n * (5.2f / EN_SR);
        if (v->vib >= 1.0f)
            v->vib -= 1.0f;
        fac = (1.0f + p->nudge) * fm_exp2f(v->vibd * (1.0f / 1200.0f) * fm_sin_turns(v->vib));
        cph = v->cph, mph = v->mph, c2ph = v->c2ph, sph = v->sph, amp = v->amp, att = v->att, idx = v->idx;
        sweep = v->sweep, lp = v->lp, hp = v->hp, bl = v->bl, bb = v->bb;
        gl = p->gl * v->gain, gr = p->gr * v->gain, sr = p->s_rev * v->gain, se = p->s_echo * v->gain;
        {
            const float ci = v->cinc * fac, mi = v->minc * fac, det = v->det, ar = v->arate, id = v->idec, sd = v->sdec;
            const float clp = p->c_lp, chp = p->c_hp, bf = v->bf, two = INST[ins].det > 0.0f ? 1.0f : 0.0f;
            const float subg = INST[ins].sub, noise = INST[ins].noise;
            float rd = v->rdec;
            int32_t hold = v->hold;
            for (; j < n; j++) {
                float x, m = 1.0f + sweep;
                if (noise > 0.0f) {              /* a ring of noise at the pitch */
                    float w = wnoise(&noise_r), hi;
                    bl += bf * bb;
                    hi = w - bl - noise * bb;
                    bb += bf * hi;
                    x = bb * 0.22f * (1.0f - 2.0f * noise);
                } else {
                    float mod = sine(mph) * idx;
                    x = sine(cph + mod + 4.0f);
                    if (two > 0.0f) {
                        x = 0.6f * (x + sine(c2ph + mod + 4.0f));
                        c2ph += ci * det * m;
                        if (c2ph >= 1.0f) c2ph -= 1.0f;
                    }
                    if (subg > 0.0f) {
                        x += subg * sine(sph);
                        sph += ci * 0.5f;
                        if (sph >= 1.0f) sph -= 1.0f;
                    }
                    cph += ci * m;
                    mph += mi * m;
                    if (cph >= 1.0f) cph -= (float)(int)cph;
                    if (mph >= 1.0f) mph -= (float)(int)mph;
                    idx *= id;
                }
                sweep *= sd;
                att += (1.0f - att) * ar;
                if (hold > 0)
                    hold--;
                else if (hold == 0)
                    amp *= rd;
                x *= amp * att;
                lp += clp * (x - lp);            /* the planet's distance: its band */
                hp += chp * (lp - hp);
                x = lp - hp;
                mixl[j] += x * gl;
                mixr[j] += x * gr;
                rsend[j] += x * sr;
                esend[j] += x * se;
                BUS(b, j, x * (gl + gr) * 0.7f);
            }
            v->hold = hold;
        }
        v->cph = cph, v->mph = mph, v->c2ph = c2ph, v->sph = sph, v->amp = amp, v->att = att, v->idx = idx;
        v->sweep = sweep, v->lp = fm_flush(lp), v->hp = fm_flush(hp), v->bl = fm_flush(bl), v->bb = fm_flush(bb);
        if (v->hold == 0 && amp < 0.0004f) {
            voice_off(k);
            if (pl[v->pl].held == k)
                pl[v->pl].held = -1;
        }
    }
}

/* ------------------------------------------------------------ the weather --- */
static struct {
    float dust, dust_t, dust_left, dust_len;     /* the cloud: its peak, the time left, its length */
    uint8_t rock, comet;
    float rx, ry, rvx, rvy, rt, rdur;
    float cr, cang, cdir, cnext;
    int cdeg;
    /* their sounds */
    float rb_l, rb_b, rbr, ramp, rgl, rgr, rf;
    float crk, dp[7], dlp, dust_l, dust_r, dust_l2, dust_r2, dlo_l, dlo_r;
} wx;

#define NBELL 6
static struct {
    float cph, mph, inc, idx, amp, gl, gr, ph2;
    uint8_t on;
} bell[NBELL];
static float events_vol;
static float gate;                              /* growing 1 / resting 0, smoothed */

static void comet_bell(int deg, float vol, float pan)
{
    int k, i, best = 0, n;
    float f;
    for (k = 0; k < NBELL; k++)
        if (!bell[k].on)
            break;
    if (k == NBELL) {                            /* all ringing: the quietest goes */
        for (i = 1; i < NBELL; i++)
            if (bell[i].amp < bell[best].amp)
                best = i;
        k = best;
    }
    {
        int o = deg >= 0 ? deg / (nsteps ? nsteps : 1) : 0, idx = deg - o * (nsteps ? nsteps : 1);
        n = 72 + par[P_ROOT] + (nsteps ? steps[idx] : 0) + 12 * o;
        n = n < 60 ? 60 : n > 108 ? 108 : n;
    }
    f = mtof((float)n);
    bell[k].inc = f * (1.0f / EN_SR);
    bell[k].cph = bell[k].mph = bell[k].ph2 = 0.0f;
    bell[k].idx = 0.4f;
    bell[k].amp = vol;
    pan_gains(pan, &bell[k].gl, &bell[k].gr);
    bell[k].on = 1;
}

static void render_bells(uint32_t n)
{
    const float dec = fm_expf(-1.0f / (1.3f * EN_SR)), idec = fm_expf(-1.0f / (0.4f * EN_SR)), g = events_vol * gate;
    int k;
    for (k = 0; k < NBELL; k++) {
        float cph = bell[k].cph, mph = bell[k].mph, ph2 = bell[k].ph2, amp = bell[k].amp, idx = bell[k].idx, inc = bell[k].inc;
        uint32_t j;
        if (!bell[k].on)
            continue;
        for (j = 0; j < n; j++) {
            float x = (sine(cph + sine(mph) * idx + 4.0f) + 0.3f * sine(ph2)) * amp * g;
            cph += inc;
            mph += inc * 3.01f;
            ph2 += inc * 2.004f;
            if (cph >= 1.0f) cph -= 1.0f;
            if (mph >= 1.0f) mph -= (float)(int)mph;
            if (ph2 >= 1.0f) ph2 -= (float)(int)ph2;
            amp *= dec;
            idx *= idec;
            mixl[j] += x * bell[k].gl;
            mixr[j] += x * bell[k].gr;
            rsend[j] += x * 0.7f;
            esend[j] += x * 0.4f;
            BUS(4, j, x);
        }
        bell[k].cph = cph, bell[k].mph = mph, bell[k].ph2 = ph2, bell[k].amp = amp, bell[k].idx = idx;
        if (amp < 0.0003f)
            bell[k].on = 0;
    }
}

/* the asteroid's rumble: brown noise through a low band, louder near the middle, panned with it */
static void render_rock(uint32_t n)
{
    float bl = wx.rb_l, bb = wx.rb_b, br = wx.rbr, a = wx.ramp;
    const float tgt = wx.rock ? events_vol * 1.6f * wx.rf * wx.rf * gate : 0.0f;
    const float f1 = 2.0f * fm_sinf(FM_PI * (160.0f + 360.0f * wx.rf) / EN_SR);
    uint32_t j;
    if (a < 1e-6f && tgt < 1e-6f)
        return;
    for (j = 0; j < n; j++) {
        float w = wnoise(&noise_s), hi, x;
        br = (br + 0.02f * w) * (1.0f / 1.02f);
        bl += f1 * bb;
        hi = br * 4.0f - bl - 1.4f * bb;
        bb += f1 * hi;
        a += (tgt - a) * (1.0f / (0.3f * EN_SR));
        x = bb * a * 0.1f;
        mixl[j] += x * wx.rgl;
        mixr[j] += x * wx.rgr;
        rsend[j] += x * 0.4f;
        BUS(4, j, x);
    }
    wx.rb_l = fm_flush(bl), wx.rb_b = fm_flush(bb), wx.rbr = fm_flush(br), wx.ramp = a;
}

/* the radio: a hiss in a band, coming and going; and now and then a whistler, falling */
static struct {
    float bl, bb, am, am_t, am_left, wph, winc, wdec, wamp, wenv;
} rd;
static float radio_vol;
static void render_radio(uint32_t n)
{
    const float f1 = 2.0f * fm_sinf(FM_PI * 1800.0f / EN_SR), tgt = radio_vol * rd.am * 0.12f * gate;
    float bl = rd.bl, bb = rd.bb;
    uint32_t j;
    if (tgt > 1e-5f) {
        for (j = 0; j < n; j++) {
            float w = wnoise(&noise_l), hi, x;
            bl += f1 * bb;
            hi = w - bl - 0.4f * bb;
            bb += f1 * hi;
            x = bb * tgt * 0.35f;
            mixl[j] += x;
            mixr[j] += x;
            esend[j] += x * 0.5f;
            BUS(4, j, x);
        }
        rd.bl = fm_flush(bl), rd.bb = fm_flush(bb);
    }
    if (rd.wamp > 1e-5f) {
        float ph = rd.wph, inc = rd.winc, a = rd.wamp, e = rd.wenv;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * a * e;
            ph += inc;
            if (ph >= 1.0f) ph -= 1.0f;
            inc *= rd.wdec;
            e += (1.0f - e) * 0.0003f;
            a *= 0.99994f;
            mixl[j] += x;
            mixr[j] += x;
            esend[j] += x;
            rsend[j] += x * 0.8f;
            BUS(4, j, x);
        }
        rd.wph = ph, rd.winc = inc, rd.wamp = a, rd.wenv = e;
    }
}

/* ---------------------------------------------------------------- the echo --- */
/* a dotted eighth, mono, darkened each time round. Kept at half the rate in 16 bits (up to 1.5 s). */
#define ECHO_N 33075
static int16_t echo_buf[ECHO_N] EN_POOL;
static uint32_t echo_w;
static float echo_d = 9000.0f, echo_lp, echo_hold, echo_out, echo_in, echo_vol;
static uint8_t echo_odd;
static inline float echo_step(float in)
{
    if (echo_odd ^= 1) {                         /* every other sample: one in, one out */
        float d = echo_d, rp = (float)echo_w - d, y0, y1, f;
        int i0;
        if (rp < 0.0f)
            rp += (float)ECHO_N;
        i0 = (int)rp;
        f = rp - (float)i0;
        y0 = (float)echo_buf[i0] * (1.0f / 32768.0f);
        y1 = (float)echo_buf[i0 + 1 < ECHO_N ? i0 + 1 : 0] * (1.0f / 32768.0f);
        echo_hold = y0 + (y1 - y0) * f;
        echo_lp += 0.35f * (echo_hold - echo_lp);
        {
            float w = fm_clampf((echo_in + in) * 0.5f + 0.48f * echo_lp, -0.999f, 0.999f);
            echo_buf[echo_w] = (int16_t)(w * 32767.0f);
        }
        if (++echo_w >= ECHO_N)
            echo_w = 0;
    } else {
        echo_in = in;
    }
    echo_out += 0.5f * (echo_hold - echo_out);
    return echo_out;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in AMB-1, after FoMni) */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define EXC DS(16)
#define EXC_MAX (2 * EXC)
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
static float rev_send, rev_in_gain = 1.0f;

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
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.45f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
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
static void scale_set(void) { nsteps = en_scale(pool, steps); }

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    if (p >= P_PAT && p < P_PAT + EN_NPL * EN_NBANK) {
        if ((p - P_PAT) / EN_NPL == bank_play)
            periods();
        return;
    }
    if (p == P_BANK) {                           /* resting: now; playing: at the bar */
        if (!en_playing && v != bank_play) {
            bank_play = v;
            en_bank = (uint8_t)v;
            periods();
        }
        return;
    }
    if (p >= P_PL && p < P_TIDE) {
        int i = (p - P_PL) / 4, k = (p - P_PL) % 4;
        if (k == 1 && pl[i].held >= 0) {         /* a woken planet's held note: let it go, it wakes in the new voice */
            voice_release(pl[i].held);
            pl[i].held = -1;
            pl[i].awake = 0;
        }
        return;
    }
    switch (p) {
    case P_PLANETS: case P_GY: orbits(0); break;
    case P_PHRASE: {
        int i;
        for (i = 0; i < EN_NPL; i++)
            phrase_fit(i);
        break;
    }
    case P_COLOUR:
        if (v != st.k) {
            nzs[1] = nzs[0];
            st.k_old = st.k;
            st.k = v;
            st.xf = 0.0f;
        }
        break;
    case P_DRUMS: drum_vol = lvl(v); break;
    case P_EVENTS: events_vol = lvl(v); break;
    case P_REVERB: rev_send = lvl(v); pl_decay_t = 0.62f + 0.3f * pct(v); break;
    case P_ECHO: echo_vol = lvl(v); break;
    case P_RADIO: radio_vol = pct(v); break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_MIDI: case P_MCH: {                   /* what was sounding stops, on every channel it could be on */
        int i;
        for (i = 0; i < 16; i++)
            en_midi_out(0xB0 | (uint32_t)i, 123, 0);
        break;
    }
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
}

/* a new start: the planets at new places on fresh orbits, new phrases, the first chord */
static void regrow(void)
{
    int i;
    bank_play = par[P_BANK];
    en_bank = (uint8_t)bank_play;
    hop_in = en_hop_left = -1;
    hop_fill = 0;
    hop_bars = 0;
    for (i = 0; i < EN_NPL; i++) {
        pl[i].ang = frand();
        pl[i].om = frand();
        pl[i].bias = pl[i].bias_t = 0.0f;
        pl[i].period = en_period_bars(hits_of(i));
        pl[i].tide = 0.0f;
        pl[i].awake = 0;
        if (pl[i].held >= 0)
            voice_release(pl[i].held);
        pl[i].held = -1;
        pl[i].lastf = 0.0f;
        pl[i].mod = 1.0f;
        phrase_new(i);
    }
    for (i = 0; i < EN_NPL; i++) {
        en_pat[i] = (uint16_t)par[P_PAT_OF(bank_play, i)];
    }
    chord_i = 0;
    orbits(1);
}

static void notes_off(void)
{
    int i;
    if (!par[P_MIDI])
        return;
    for (i = 0; i < 8; i++)
        en_midi_out(0xB0 | (uint32_t)((par[P_MCH] + i) & 15), 123, 0);
}

static void quiet(void)
{
    int i;
    for (i = 0; i < NV; i++)
        vc[i].on = 0;
    for (i = 0; i < EN_NPL; i++) {
        dr[i].on = 0;
        pl[i].held = -1;
        pl[i].awake = 0;
    }
    for (i = 0; i < NBELL; i++)
        bell[i].on = 0;
    st.amp = st.sub = 0.0f;
    rd.wamp = 0.0f;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    {   /* the yellow's band: 1.8 kHz, Q 1 (RBJ), normalized by a0 */
        float w0 = FM_TWO_PI * 1800.0f / EN_SR, al = fm_sinf(w0) * 0.5f, a0 = 1.0f + al;
        yb0 = al / a0;
        ya1 = -2.0f * fm_cosf(w0) / a0;
        ya2 = (1.0f - al) / a0;
    }
    plate_init();
    for (i = 0; i < ECHO_N; i++)
        echo_buf[i] = 0;
    q_r = q_w = 0;
    for (i = 0; i < NV; i++)
        vc[i].on = 0;
    for (i = 0; i < EN_NPL; i++) {
        pl[i].held = -1;
        pl[i].period = 0.0f;
        pl[i].mod = 1.0f;
    }
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    {   /* Sol's drums until told (a planet with no hits would sleep) */
        int16_t pp[P_NPARAMS];
        uint16_t pool0;
        en_piece(0, pp, &pool0);
        for (i = 0; i < EN_NPL * EN_NBANK; i++)
            par[P_PAT + i] = pp[P_PAT + i];
    }
    st.k = st.k_old = par[P_COLOUR];
    st.xf = 1.0f;
    st.roar_left = 0.0f;
    rd.am = 0.5f;
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    regrow();
    pl_decay = pl_decay_t;
    for (i = 0; i < EN_NPL; i++)
        pl[i].a = pl[i].a_t;
}

/* ------------------------------------------------------------ transport --- */
static void play_start(void) { en_playing = 1; }
static void play_stop(void)
{
    int i;
    en_playing = 0;
    for (i = 0; i < EN_NPL; i++)
        if (pl[i].held >= 0) {
            voice_release(pl[i].held);
            pl[i].held = -1;
            pl[i].awake = 0;
        }
    notes_off();
}

/* ------------------------------------------------------------ the system --- */
static float step_frac;                         /* where we are in the current step, 0..1 */
static uint32_t step_count;
static float ext_steps;                         /* MIDI clock: steps the ticks have brought, not yet taken */
static uint32_t clk_ticks, ext_age, since_tick;
static float tick_gap;

static void great_year(void)
{
    chord_i++;
    en_greats++;
    en_chord = (uint8_t)((par[P_ROOT] + (nsteps ? steps[chord_shift()] : 0)) % 12);
    if (par[P_EVOLVE]) {                         /* the drums evolve: a hit moves in two lanes; Drift adds or takes one */
        int c, changed = 0;
        for (c = 0; c < 2; c++) {
            int l = irand(EN_NPL), on[16], m = 0, s;
            uint32_t b = (uint16_t)par[P_PAT_OF(bank_play, l)];
            for (s = 0; s < 16; s++)
                if (b >> s & 1u)
                    on[m++] = s;
            if (m) {
                int k = on[irand(m)], to = (k + (frand() < 0.5f ? 15 : 1)) & 15;
                if (!(b >> to & 1u)) {
                    b = (b & ~(1u << k)) | 1u << to;
                    par[P_PAT_OF(bank_play, l)] = (int16_t)(uint16_t)b;
                    changed = 1;
                }
            }
        }
        if (par[P_EVOLVE] == 2) {
            int l = irand(nplanets()), h = hits_of(l), s, m = 0, pick[16];
            uint32_t b = (uint16_t)par[P_PAT_OF(bank_play, l)];
            if ((frand() < 0.5f && h > 1) || h >= 10) {
                for (s = 0; s < 16; s++)
                    if (b >> s & 1u)
                        pick[m++] = s;
                if (m)
                    b &= ~(1u << pick[irand(m)]);
            } else if (h > 0 || frand() < 0.3f) {
                for (s = 0; s < 16; s++)
                    if (!(b >> s & 1u))
                        pick[m++] = s;
                if (m)
                    b |= 1u << pick[irand(m)];
            }
            par[P_PAT_OF(bank_play, l)] = (int16_t)(uint16_t)b;
            changed = 1;
        }
        if (changed)
            periods();
    }
    if (par[P_HOP] && par[P_HOPWHEN] == 0 && hop_in < 0)
        hop_in = 2;                              /* Drift on the Great Year: a fill next bar, the new bank after */
    event(EV_GREAT, en_chord);
}

/* Drift, at each bar line: count down to the change; the bar before it is the fill */
static void hop_bar(void)
{
    static const uint8_t LEN[6] = {0, 4, 8, 16, 32, 64};
    int len = LEN[(unsigned)par[P_HOPWHEN] < 6u ? par[P_HOPWHEN] : 0];
    if (!par[P_HOP]) {
        hop_in = en_hop_left = -1;
        hop_fill = 0;
        hop_bars = 0;
        return;
    }
    if (hop_in > 0)
        hop_in--;
    if (hop_in == 0) {                           /* the change: the next bank, or another from the pool */
        int pool = par[P_HOPPOOL] < 2 ? 2 : par[P_HOPPOOL], nb = par[P_BANK];
        if (par[P_HOP] == 1 || pool < 2)
            nb = bank_play + 1 >= pool ? 0 : bank_play + 1;
        else
            do
                nb = irand(pool);
            while (nb == bank_play);
        par[P_BANK] = (int16_t)nb;
        en_hops++;
        event(EV_HOP, nb);
        hop_in = -1;
        hop_bars = 0;
    } else if (len && hop_in < 0 && ++hop_bars >= (uint32_t)len - 1u) {
        hop_in = 1;
    }
    hop_fill = hop_in == 1 && par[P_HOPFILL] > 0;
    if (hop_fill)
        event(EV_FILL, bank_play);
    en_hop_left = (int8_t)(hop_in >= 0 ? hop_in : len ? len - (int)hop_bars : -1);
}

static void year(int i)
{
    phrase_mutate(i);
    en_years++;
    event(EV_YEAR, i);
    if (i == outermost())
        great_year();
}

/* a step: the drums, their bindings, and the planets passing the points of their phrases */
static void step(int32_t at)
{
    int s = (int)(step_count & 15u), l, i;
    const float bars_per_step = 1.0f / 16.0f;
    if (s == 0)
        hop_bar();
    if (s == 0 && par[P_BANK] != bank_play) {     /* a new bank, on the bar */
        bank_play = par[P_BANK];
        en_bank = (uint8_t)bank_play;
        periods();
    }
    en_step = (uint8_t)s;
    if (drum_noteon) {                           /* last step's drum notes end */
        for (l = 0; l < EN_NPL; l++)
            if (drum_noteon >> l & 1u)
                note(0, 6, (uint32_t)(36 + l), 0);
        drum_noteon = 0;
    }
    for (l = 0; l < EN_NPL; l++)
        if ((uint16_t)par[P_PAT_OF(bank_play, l)] >> s & 1u) {
            drum_hit(l, at);
            note(1, 6, (uint32_t)(36 + l), 100);
            drum_noteon |= (uint8_t)(1u << l);
            if (l < nplanets())
                bind(l);
        }
    if (hop_fill && s >= 12) {                    /* the fill: the lanes tumble in, more as the bar ends */
        float pr = (float)par[P_HOPFILL] * 0.01f * 0.2f * (float)(s - 11);
        for (l = 0; l < EN_NPL; l++)
            if (!((uint16_t)par[P_PAT_OF(bank_play, l)] >> s & 1u) && frand() < pr) {
                drum_hit(l, at);
                note(1, 6, (uint32_t)(36 + l), 80);
                drum_noteon |= (uint8_t)(1u << l);
            }
    }
    for (i = 0; i < nplanets(); i++) {
        planet_t *p = &pl[i];
        int k;
        float t0, t1;
        if (dormant(i))
            continue;
        t0 = p->ang + p->bias;
        t1 = t0 + bars_per_step / p->period;
        for (k = 0; k < p->n; k++) {
            float th = (float)k / (float)p->n;
            if (fm_floorf(t1 - th) > fm_floorf(t0 - th) && p->ph[k] != REST) {
                float per = p->period * 240.0f / en_bpm / (float)p->n;
                float vel = 0.13f + 0.05f * frand();
                voice_start(i, phrase_note(i, p->ph[k], p->pho[k]), vel, fm_clampf(per * 0.85f, 0.4f, 9.0f), at, 0);
            }
        }
    }
}

/* the years go round: dsteps of 16th steps this chunk */
static void advance(float dsteps)
{
    int i;
    for (i = 0; i < EN_NPL; i++) {
        planet_t *p = &pl[i];
        if (p->period <= 0.0f)
            continue;
        p->ang += dsteps / (16.0f * p->period);
        while (p->ang >= 1.0f) {
            p->ang -= 1.0f;
            if (i < nplanets())
                year(i);
        }
    }
}

static void tides(float dt)
{
    int n = nplanets(), i, j;
    const float R2 = pct(par[P_REACH]) * pct(par[P_REACH]);
    for (i = 0; i < EN_NPL; i++) {
        planet_t *p = &pl[i];
        float si = pct(par[P_PL_LVL(i)]), sum = 0.0f, best = 0.0f, raw;
        int who = -1;
        if (i >= n) {
            p->tide *= 0.9f;
            p->link = -1;
            continue;
        }
        for (j = 0; j < n; j++) {
            float sj = pct(par[P_PL_LVL(j)]), dx, dy, f, c;
            if (j == i || sj <= si + 0.02f || pl[j].period == p->period)
                continue;
            dx = pl[j].x - p->x;
            dy = pl[j].y - p->y;
            f = R2 / (dx * dx + dy * dy + R2);
            c = sj * f * f;
            sum += c;
            if (c > best) {
                best = c;
                who = j;
            }
        }
        if (wx.rock) {                           /* the asteroid: a heavy body passing */
            float dx = wx.rx - p->x, dy = wx.ry - p->y, f = R2 / (dx * dx + dy * dy + R2), c = 0.9f * f * f;
            sum += c;
            if (c > best) {
                best = c;
                who = EN_NPL;
            }
        }
        raw = fm_clampf(sum * pct(par[P_TIDE]) * 1.6f, 0.0f, 1.0f);
        p->tide += (raw - p->tide) * fm_minf(dt / 0.4f, 1.0f);
        p->link = (int8_t)(best > 0.05f ? who : -1);
        p->linkc = best;
    }
}

static void dust_start(void)
{
    wx.dust_t = 0.6f + 0.4f * frand();
    wx.dust_len = wx.dust_left = 20.0f + 40.0f * frand();
    en_dusts++;
    event(EV_DUST, 0);
}

/* the weather, once a chunk */
static void weather(float dt)
{
    /* a dust cloud thickens through its first half and thins through its second: the Mellow (LOF-1's
     * master low-pass) closing in and opening out again with it */
    if (wx.dust_left > 0.0f) {
        wx.dust_left -= dt;
        if (wx.dust_left <= 0.0f) {
            wx.dust_left = 0.0f;
            wx.dust_t = 0.0f;
            event(EV_CLEAR, 0);
        }
    } else if (frand() < pct(par[P_DUST]) * 0.012f * dt) {
        dust_start();
    }
    wx.dust = wx.dust_left > 0.0f ? wx.dust_t * fm_sin_turns(0.5f * (1.0f - wx.dust_left / wx.dust_len)) : 0.0f;
    if (!wx.rock && frand() < pct(par[P_ROCKS]) * 0.01f * dt) {
        float a0 = frand(), R0 = 1.3f, tx = (frand() - 0.5f) * 0.6f, ty = (frand() - 0.5f) * 0.6f;
        float sx = R0 * fm_cos_turns(a0), sy = R0 * fm_sin_turns(a0), dx = tx - sx, dy = ty - sy;
        float L = fm_sqrtf(dx * dx + dy * dy), sp;
        wx.rdur = 18.0f + 12.0f * frand();
        sp = (L + R0) / wx.rdur;
        wx.rx = sx, wx.ry = sy, wx.rvx = dx / L * sp, wx.rvy = dy / L * sp, wx.rt = 0.0f;
        wx.rock = 1;
        en_rocks++;
        event(EV_ROCK, 0);
    }
    if (wx.rock) {
        wx.rt += dt;
        wx.rx += wx.rvx * dt;
        wx.ry += wx.rvy * dt;
        wx.rf = fm_clampf(1.0f - fm_sqrtf(wx.rx * wx.rx + wx.ry * wx.ry) / 1.3f, 0.0f, 1.0f);
        pan_gains(fm_clampf(wx.rx, -1.0f, 1.0f) * pct(par[P_WIDTH]), &wx.rgl, &wx.rgr);
        if (wx.rt > wx.rdur)
            wx.rock = 0;
    }
    if (!wx.comet && frand() < pct(par[P_COMETS]) * 0.006f * dt) {
        wx.comet = 1;
        wx.cr = 1.15f;
        wx.cang = frand();
        wx.cdir = frand() < 0.5f ? -1.0f : 1.0f;
        wx.cnext = 0.4f;
        wx.cdeg = 2 + irand(3);
        en_comets++;
        event(EV_COMET, 0);
    }
    if (wx.comet) {                              /* it falls in, faster and faster, ringing as it goes */
        wx.cr -= (0.03f + 0.06f * (1.0f - wx.cr)) * dt;
        wx.cang += wx.cdir * dt * 0.22f / fm_powf(fm_maxf(wx.cr, 0.1f), 1.5f) * (1.0f / FM_TWO_PI);
        wx.cang -= fm_floorf(wx.cang);
        wx.cnext -= dt;
        if (wx.cnext <= 0.0f) {
            wx.cnext = 0.22f + 1.4f * wx.cr;
            comet_bell(wx.cdeg, 0.05f * (1.3f - wx.cr), fm_cos_turns(wx.cang) * wx.cr * pct(par[P_WIDTH]));
            wx.cdeg += frand() < 0.7f ? 1 : 2;
            if (wx.cdeg > 16)
                wx.cdeg -= 5;
        }
        if (wx.cr < 0.06f) {                     /* it burns up: a chord of bells, a crackle, a flare */
            int k;
            for (k = 0; k < 4; k++) {
                static const int8_t UP[4] = {0, 2, 4, 7};
                comet_bell(wx.cdeg + UP[k], 0.07f, fm_cos_turns(wx.cang) * 0.5f * pct(par[P_WIDTH]));
            }
            st.flare = 1.0f;
            wx.crk = 1.0f;
            wx.comet = 0;
            event(EV_BURN, 0);
        }
    }
}

/* the dust: the whole mix through a low-pass that closes as the cloud thickens, its lows lifted a little,
 * and a grain under it (crackle and a soft pink) */
static void render_dust(uint32_t n)
{
    const float d = wx.dust, c = onepole(18000.0f * fm_exp2f(-3.6f * d)), lo = onepole(250.0f), shelf = 0.41f * d;
    const float grain = 0.06f * d * gate, burn = wx.crk;
    float l = wx.dust_l, r = wx.dust_r, l2 = wx.dust_l2, r2 = wx.dust_r2, ll = wx.dlo_l, lr = wx.dlo_r, pk = wx.dlp, crk = 0.0f;
    uint32_t j;
    if (d < 0.002f && burn < 0.002f) {
        wx.dust_l = wx.dust_r = wx.dust_l2 = wx.dust_r2 = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float w = wnoise(&noise_s), g;
        if ((noise_s >> 10) % 900u == 0u)
            crk = w * 0.8f;
        pk += 0.15f * (w - pk);
        g = (crk + pk * 0.4f) * grain;
        crk *= 0.85f;
        l += c * (mixl[j] - l);                  /* the Mellow: two gentle poles */
        r += c * (mixr[j] - r);
        l2 += c * (l - l2);
        r2 += c * (r - r2);
        ll += lo * (l2 - ll);
        lr += lo * (r2 - lr);
        mixl[j] = l2 + shelf * ll + g;
        mixr[j] = r2 + shelf * lr + g;
        if (burn > 0.002f) {                     /* the comet's burning: a bright crackle */
            float b = (w - pk) * wx.crk * 0.12f * events_vol;
            mixl[j] += b;
            mixr[j] += b;
            rsend[j] += b;
            wx.crk *= 0.99994f;
        }
    }
    wx.dust_l = fm_flush(l), wx.dust_r = fm_flush(r), wx.dust_l2 = fm_flush(l2), wx.dust_r2 = fm_flush(r2), wx.dlo_l = fm_flush(ll), wx.dlo_r = fm_flush(lr), wx.dlp = fm_flush(pk);
}

/* ------------------------------------------------------------- once a chunk --- */
static void control(uint32_t n)
{
    const float dt = (float)n * (1.0f / EN_SR);
    const int run = en_playing && !en_frozen, np = nplanets();
    float dsteps = 0.0f;
    int i;

    /* the tempo and the steps */
    if (par[P_SYNC] && en_ext) {
        dsteps = run ? ext_steps : 0.0f;
        ext_steps = 0.0f;
    } else {
        en_bpm = (float)par[P_TEMPO];
        if (run)
            dsteps = (float)n * en_bpm * (4.0f / 60.0f) * (1.0f / EN_SR);
    }
    if (dsteps > 0.0f) {                         /* each step this chunk, at its sample */
        float rate = dsteps / (float)n, left = dsteps, at = 0.0f;
        while (step_frac + left >= 1.0f) {
            float need = 1.0f - step_frac;
            at += need / rate;
            left -= need;
            advance(need);
            step_frac = 0.0f;
            step_count++;
            step((int32_t)fm_minf(at, (float)(n - 1)));
        }
        step_frac += left;
        advance(left);
    }

    /* the orbits: places, sizes drifting, the shared orbits settling */
    {
        const float spb = 240.0f / fm_maxf(en_bpm, 20.0f);
        for (i = 0; i < EN_NPL; i++) {
            planet_t *p = &pl[i];
            float e = (float)par[P_PL_ECC(i)] * 0.01f, tot, r;
            p->a += (p->a_t - p->a) * fm_minf(dt / (spb * 0.8f), 1.0f);
            p->bias += (p->bias_t - p->bias) * fm_minf(dt / 4.0f, 1.0f);
            tot = p->ang + p->bias;
            r = p->a * (1.0f - e * e) / (1.0f + e * fm_cos_turns(tot - p->om));
            p->x = r * fm_cos_turns(tot);
            p->y = r * fm_sin_turns(tot);
            p->dist = fm_clampf(r / 0.75f, 0.0f, 1.0f);
            p->glow *= 1.0f - fm_minf(dt * 3.0f, 1.0f);
            en_drum_glow[i] *= 1.0f - fm_minf(dt * 8.0f, 1.0f);
        }
    }
    if (run)                                     /* the weather holds in an eclipse, and while resting */
        weather(dt);
    tides(dt);

    /* each planet's sound: its distance, its tide, its binding */
    for (i = 0; i < EN_NPL; i++) {
        planet_t *p = &pl[i];
        int act = i < np, dorm = dormant(i);
        float d = p->dist, ts = p->tide, base, tg, fc, dry, pn, gl, gr, w = pct(par[P_WIDTH]);
        if (act && dorm && en_playing && !en_frozen) {   /* a dormant planet wakes in a tide, sleeps after */
            if (!p->awake && ts > 0.16f) {
                int k, kk = 0, any = 0;
                for (k = 0; k < p->n; k++)
                    any |= p->ph[k] != REST;
                if (any) {
                    do {
                        kk = p->wake_i++ % (p->n ? p->n : 1);
                    } while (p->ph[kk] == REST);
                }
                p->awake = 1;
                p->held = (int8_t)voice_start(i, phrase_note(i, any ? p->ph[kk] : 0, p->pho[kk]), 0.16f, 0.0f, 0, 1);
                en_wakes++;
                event(EV_WAKE, i);
            } else if (p->awake && ts < 0.05f) {
                p->awake = 0;
                voice_release(p->held);
                p->held = -1;
            }
        } else if (p->awake) {
            p->awake = 0;
            voice_release(p->held);
            p->held = -1;
        }
        /* the binding's movement */
        p->mod += (1.0f - p->mod) * fm_minf(dt / 0.2f, 1.0f);
        if (p->flick) {
            p->flick_t += dt;
            if (p->flick_t >= 0.045f) {
                p->flick_t = 0.0f;
                p->flick--;
            }
        }
        p->blip *= 1.0f - fm_minf(dt / 0.12f, 1.0f);
        p->nudge *= 1.0f - fm_minf(dt / 0.12f, 1.0f);
        base = act ? lvl(par[P_PL_LVL(i)]) * 0.3f * gate * p->mod * (p->flick & 1u ? 0.35f : 1.0f) : 0.0f;
        tg = dorm ? pct(par[P_WAKE]) * fm_clampf(ts * 2.4f, 0.0f, 1.2f) : 1.0f + 0.9f * ts;
        tg *= 1.0f + 0.5f * p->blip;
        fc = fm_minf((2800.0f + 6500.0f * d) * (1.0f + 1.5f * ts) * (1.0f + 3.0f * p->blip), 16000.0f);
        p->c_lp = onepole(fc);
        p->c_hp = onepole(40.0f + 560.0f * d * fm_sqrtf(d));
        dry = 1.0f - 0.45f * d;
        pn = fm_clampf(p->x / 0.75f, -1.0f, 1.0f) * w;
        pan_gains(pn, &gl, &gr);
        p->gl = gl * dry * base * tg;
        p->gr = gr * dry * base * tg;
        p->s_rev = (0.08f + 0.8f * d) * base * tg;
        p->s_echo = (0.04f + 0.4f * d) * base * tg;
        en_pl_x[i] = p->x;
        en_pl_y[i] = p->y;
        en_pl_a[i] = p->a;
        en_pl_om[i] = p->om;
        en_pl_bias[i] = p->bias;
        en_pl_dist[i] = p->dist;
        en_pl_tide[i] = p->tide;
        en_pl_glow[i] = p->glow;
        en_pl_link[i] = p->link;
        en_pl_linkc[i] = p->linkc;
        en_pl_awake[i] = p->awake;
        en_pl_notes[i] = p->n;
    }

    /* the star: its roar wanders, flares come; its level the ceiling */
    st.roar_left -= dt;
    if (st.roar_left <= 0.0f) {
        float r = pct(par[P_ROAR]);
        st.roar_t = r * (0.15f + 0.85f * frand() * frand());
        st.roar_left = 4.0f + 8.0f * frand();
        if (frand() < r * 0.25f) {               /* now and then it builds into a roar */
            st.roar_t = fm_minf(1.0f, 0.7f + r * 0.5f);
            st.roar_left = 8.0f + 6.0f * frand();
        }
    }
    st.roar += (st.roar_t - st.roar) * fm_minf(dt / 3.0f, 1.0f);
    if (run && frand() < pct(par[P_FLARES]) * 0.035f * dt)
        st.flare = 1.0f;
    st.flare *= 1.0f - fm_minf(dt * 1.4f, 1.0f);
    {
        const float dim = en_frozen ? 0.45f : 1.0f, lv = lvl(par[P_STAR]) * 0.3f * dim * gate;
        st.amp_t = lv * (0.5f + 0.5f * st.roar) * (1.0f + 0.3f * st.flare);
        st.c_t = onepole(fm_maxf(300.0f, 2600.0f - 1700.0f * st.roar + 7000.0f * st.flare));
        st.shelf_t = st.flare * 1.5f;
        st.sub_t = lvl(par[P_STAR]) * st.roar * 0.16f * dim * gate;
    }
    en_roar = st.roar;
    en_flare = st.flare;
    en_star = pct(par[P_STAR]) * (1.0f + 0.5f * st.roar) + 0.3f * st.flare;

    /* the radio: its hiss comes and goes; a whistler now and then */
    rd.am_left -= dt;
    if (rd.am_left <= 0.0f) {
        rd.am_t = 0.2f + 0.8f * frand();
        rd.am_left = 1.0f + 5.0f * frand();
    }
    rd.am += (rd.am_t - rd.am) * fm_minf(dt / 1.5f, 1.0f);
    if (run && rd.wamp < 1e-4f && frand() < radio_vol * 0.05f * dt) {
        float f0 = 4800.0f + 1500.0f * frand(), f1 = 280.0f + 200.0f * frand(), T = 1.2f + 1.3f * frand();
        rd.winc = f0 * (1.0f / EN_SR);
        rd.wdec = fm_expf(fm_logf(f1 / f0) / (T * EN_SR));
        rd.wamp = 0.035f * radio_vol * gate;
        rd.wenv = 0.0f;
        rd.wph = 0.0f;
    }

    /* the Great Year's progress, the drums' steps */
    {
        int om = outermost();
        en_year = om >= 0 ? pl[om].ang : 0.0f;
    }
    en_dust = wx.dust;
    en_rock_on = wx.rock;
    en_rock_x = wx.rx, en_rock_y = wx.ry, en_rock_vx = wx.rvx, en_rock_vy = wx.rvy;
    en_comet_on = wx.comet;
    en_comet_r = wx.cr, en_comet_ang = wx.cang;

    /* the space */
    gate += ((en_playing ? 1.0f : 0.0f) - gate) * fm_minf(dt / (en_playing ? 1.0f : 1.5f), 1.0f);
    pl_decay += ((en_frozen ? 0.97f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += 0.01f * (1.0f - rev_in_gain);
    echo_d = fm_clampf(0.75f * 60.0f / fm_maxf(en_bpm, 20.0f) * (EN_SR * 0.5f), 200.0f, (float)(ECHO_N - 2));
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
        case C_TOUCH:
            if (en_playing)
                great_year();
            break;
        case C_SUMMON:
            if (a == 0) {
                if (wx.dust_left > 0.0f) {       /* called off: it thins from where it is, as fast as it came */
                    float age = wx.dust_len - wx.dust_left;
                    if (age < wx.dust_len * 0.5f)
                        wx.dust_left = age;
                } else {
                    dust_start();
                }
            } else {                             /* the next chunk's weather finds its chance certain */
                uint8_t keep = a == 1 ? wx.rock : wx.comet;
                if (!keep) {
                    int16_t was = par[a == 1 ? P_ROCKS : P_COMETS];
                    par[a == 1 ? P_ROCKS : P_COMETS] = 30000;
                    weather(1.0f);
                    par[a == 1 ? P_ROCKS : P_COMETS] = was;
                }
            }
            break;
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
                if (since_tick > 0u && since_tick < (uint32_t)EN_SR) {   /* the tempo, smoothed */
                    tick_gap = tick_gap > 0.0f ? tick_gap + ((float)since_tick - tick_gap) * 0.02f : (float)since_tick;
                    en_bpm = EN_SR * 60.0f / (24.0f * tick_gap);
                }
                since_tick = 0;
                en_ext = 1;
                if (en_playing) {
                    clk_ticks++;
                    ext_steps += 1.0f / 6.0f;    /* six ticks a sixteenth */
                }
            } else if (a == EN_CLK_START) {       /* from the top: step one */
                clk_ticks = 0;
                en_ext = 1;
                step_frac = 0.999f;
                step_count = 0xFFFFFFFFu;
                ext_steps = 0.0f;
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
        case C_MUTE: muted = (uint8_t)(a != 0); break;
        case C_STRIKE:                           /* a note of its phrase, a little louder than its own */
            if (a < nplanets() && pl[a].n > 0) {
                int k = irand(pl[a].n);
                voice_start(a, phrase_note(a, pl[a].ph[k] == REST ? 0 : pl[a].ph[k], pl[a].pho[k]), 0.2f, 1.6f, 0, 0);
            }
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
static void render_chunk(int32_t *out, uint32_t n, float gain, uint32_t at)
{
    uint32_t j;
    int b;
    const float rg = fm_minf(rev_send, 1.0f) * rev_in_gain * (en_frozen ? 0.0f : 1.0f);
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
#ifdef EN_HOST
    bus_on = en_bus_on && at + n <= 1024u;
#else
    (void)at;
#endif
#ifdef EN_HOST
    if (bus_on)
        for (b = 0; b < EN_NBUS; b++)
            for (j = 0; j < n; j++)
                busb[b][j] = 0.0f;
#else
    (void)b;
#endif
    render_star(n);
    render_voices(n);
    render_drums(n);
    render_bells(n);
    render_rock(n);
    render_radio(n);
    render_dust(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, e = echo_step(esend[j] * (en_frozen ? 0.0f : 1.0f)) * echo_vol * 2.2f;
        reverb((rsend[j] + e * 0.3f) * rg * 0.8f, &wl, &wr);
        mute_g += ((muted ? 0.0f : 1.0f) - mute_g) * 0.002f;
        l = (mixl[j] + e + 1.3f * wl) * 1.3f * gain * mute_g;
        r = (mixr[j] + e + 1.3f * wr) * 1.3f * gain * mute_g;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
#ifdef EN_HOST
        if (bus_on)
            busb[5][j] = 0.65f * (wl + wr) + e;
#endif
    }
#ifdef EN_HOST
    if (bus_on)                                   /* the voices apart: at the mix's level, soft-limited */
        for (j = 0; j < n; j++)
            for (b = 0; b < EN_NBUS; b++)
                en_bus[(at + j) * EN_NBUS + (uint32_t)b] = fm_tanhf(busb[b][j] * 1.3f * gain * 0.99f);
#endif
}

/* the signals as CCs, when they change: every ~46 ms, a few at a time */
static void cc_out(void)
{
    static uint8_t last[10] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    static uint32_t turn;
    uint8_t v[10];
    int i, sent = 0;
    if (muted || (par[P_MIDI] != EN_MIDI_BOTH && par[P_MIDI] != EN_MIDI_CC))
        return;
    for (i = 0; i < EN_NPL; i++)
        v[i] = (uint8_t)(fm_clampf(pl[i].tide, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[6] = (uint8_t)(fm_clampf(st.roar, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[7] = (uint8_t)(fm_clampf(wx.dust, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[8] = (uint8_t)(fm_clampf(en_year, 0.0f, 1.0f) * 127.0f + 0.5f);
    v[9] = (uint8_t)(fm_clampf(st.flare, 0.0f, 1.0f) * 127.0f + 0.5f);
    for (i = 0; i < 10 && sent < 5; i++) {
        int k = (int)((turn + (uint32_t)i) % 10u);
        if (v[k] != last[k]) {
            last[k] = v[k];
            en_midi_out(0xB0u | (uint32_t)((par[P_MCH] + 7) & 15), (uint32_t)(EN_CC_TIDE + k), v[k]);
            sent++;
        }
    }
    turn += 3u;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    uint32_t at = 0;
    static uint32_t cc_left;
    drain();
    if (en_ext && ++ext_age > 100u)              /* the clock stopped coming (~0.6 s): our own time */
        en_ext = 0;
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
