/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (amb.h): LOF-1, the lofi groovebox. One compilation unit, -O2. The plate reverb, the
 * command queue and the fast maths are AMB-1's (Charles Vestal), by way of ECO-1; the rest is new. */
#include "amb.h"
#include "fastmath.h"
#include "lof_samples.h"

#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

/* ================================================================ tables === */
const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_SCALE_NAME[EN_NSCALES] = {"Major", "Minor", "Dorian", "Mixolyd.", "Pent min", "Pent maj", "Blues"};
const uint16_t EN_SCALE_POOL[EN_NSCALES] = {
    0xAB5u,   /* 0 2 4 5 7 9 11 */
    0x5ADu,   /* 0 2 3 5 7 8 10 */
    0x6ADu,   /* 0 2 3 5 7 9 10 */
    0x6B5u,   /* 0 2 4 5 7 9 10 */
    0x4A9u,   /* 0 3 5 7 10 */
    0x295u,   /* 0 2 4 7 9 */
    0x4E9u,   /* 0 3 5 6 7 10 */
};
/* each scale's seven-note parent, for the chords: its steps */
static const int8_t PARENT[EN_NSCALES][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10},
    {0, 2, 3, 5, 7, 8, 10}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}};

const char *const EN_INST_NAME[EN_NINST] = {
    "Rhodes", "Wurli", "Upright", "Old upright", "Vibes", "Kalimba", "Glock", "Strumstick", "Mute tpt", "Tenor sax",
    "Flute", "Harmonica", "Bell", "Sine", "Warm pad", "Organ", "Strings", "Upright bs", "Sub", "Round", "Pluck",
    "Moog", "Whistle", "Square"};
const uint8_t EN_LEAD_INST[EN_NLEAD] = {I_RHODES, I_WURLI, I_UPRIGHT, I_UPRIGHT2, I_VIBES, I_KALIMBA, I_GLOCK,
                                        I_STRUM, I_MUTETPT, I_TENORSAX, I_FLUTE, I_HARMONICA, I_BELL, I_SINE};
const uint8_t EN_COMP_INST[EN_NCOMP] = {I_RHODES, I_WURLI, I_UPRIGHT, I_UPRIGHT2, I_VIBES, I_STRUM, I_PAD, I_ORGAN,
                                        I_STRINGS};
const uint8_t EN_BASS_INST[EN_NBASS] = {I_UPBASS, I_SUB, I_ROUND, I_PLUCKB, I_MOOG};
const uint8_t EN_ANS_INST[EN_NANS] = {I_MUTETPT, I_TENORSAX, I_FLUTE, I_HARMONICA, I_VIBES, I_KALIMBA, I_GLOCK,
                                      I_BELL, I_WHISTLE, I_SQUARE};
const char *const EN_DUST_NAME[EN_NDUST] = {"Vinyl", "Cassette", "Reel", "Radio"};
const char *const EN_KIT_NAME[EN_NKIT] = {"Dusty", "Tape"};
const char *const EN_LANE_NAME[EN_NLANE] = {"Kick", "Snare", "Rim", "Clap", "Hat", "Open hat", "Shaker", "Perc"};
const char *const EN_TRACK_NAME[EN_NTRACK] = {"Lead", "Comp", "Bass", "Answer", "Drums", "Dust"};
const char *const EN_FX_NAME[EN_NFX] = {
    "STUTTER 1/4", "STUTTER 1/8", "RECORD SKIP", "HALF TIME", "TAPE STOP", "BACKSPIN", "MUFFLE", "RADIO",
    "MUTE LEAD", "MUTE COMP", "MUTE BASS", "MUTE ANSWER", "MUTE DRUMS", "MUTE DUST", "ROOM THROW", "ECHO THROW",
    "WOBBLE", "CRUSH", "FILL", "GRAIN FREEZE", "DROPOUT", "SOLO LEAD", "SOLO COMP", "SOLO BASS", "SOLO ANSWER",
    "SOLO DRUMS", "SOLO DUST"};

/* the instruments. Sampled ones play a set (lof_samples.h); the rest are synthesized:
 *   FM   two operators (the modulator at RATIO), the index falling from IDX to IDX * IDXS over IDXT; a bell
 *        layer (ratio 14, fast) for the electric pianos; DRIVE a little saturation (the Wurli's bark)
 *   SUB  two detuned saws (or a pulse, or a sine and its octave) through a resonant low-pass with an envelope
 *   ORG  three drawbars (8', 4', 2 2/3') with a slow chorus
 * The envelope: ATT attack, DEC the time to fall 60 dB (0: none), SUS the level held, REL the release. */
enum { K_SMP, K_FM, K_SUB, K_ORG };
enum { W_SAW, W_PULSE, W_SINE };
typedef struct {
    uint8_t kind, set;
    float att, dec, sus, rel, gain;
    float ratio, idx, idxs, idxt, bell, drive;      /* FM */
    uint8_t wave;                                   /* SUB */
    float detune, cut, env, res, vib;               /* SUB: cutoff (x the note), its envelope (octaves), resonance */
} inst_t;
static const inst_t INST[EN_NINST] = {
    /* kind  set               att     dec   sus   rel   gain   ratio idx idxs idxt bell drive  wave det cut env res vib */
    {K_FM, 0,                  0.002f, 3.2f, 0.0f, 0.25f, 0.55f, 1.0f, 1.7f, 0.25f, 0.9f, 0.22f, 0.0f, 0, 0, 0, 0, 0, 0},
    {K_FM, 0,                  0.002f, 2.4f, 0.0f, 0.18f, 0.55f, 1.0f, 2.6f, 0.35f, 0.5f, 0.10f, 0.5f, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_UPRIGHT,       0.001f, 2.6f, 0.0f, 0.30f, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_UPRIGHT2,      0.001f, 2.4f, 0.0f, 0.30f, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_VIBES,         0.001f, 3.5f, 0.0f, 0.60f, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_KALIMBA,       0.001f, 0.0f, 1.0f, 0.60f, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_GLOCK,         0.001f, 0.0f, 1.0f, 0.60f, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_STRUM,         0.001f, 0.0f, 1.0f, 0.35f, 0.9f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_MUTETPT,       0.015f, 0.0f, 1.0f, 0.12f, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_TENORSAX,      0.015f, 0.0f, 1.0f, 0.12f, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_FLUTE,         0.025f, 0.0f, 1.0f, 0.15f, 0.8f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SMP, SMP_HARMONICA,     0.015f, 0.0f, 1.0f, 0.10f, 0.7f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_FM, 0,                  0.002f, 4.0f, 0.0f, 0.80f, 0.40f, 3.5f, 1.2f, 0.2f, 1.5f, 0.0f, 0.0f, 0, 0, 0, 0, 0, 0},
    {K_FM, 0,                  0.012f, 0.0f, 0.8f, 0.25f, 0.45f, 1.0f, 0.25f, 0.6f, 0.4f, 0.0f, 0.0f, 0, 0, 0, 0, 0, 0.004f},
    {K_SUB, 0,                 0.60f, 0.0f, 1.0f, 1.40f, 0.30f, 0, 0, 0, 0, 0, 0, W_SAW, 0.006f, 3.0f, 0.4f, 0.1f, 0},
    {K_ORG, 0,                 0.008f, 0.0f, 1.0f, 0.10f, 0.28f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.003f},
    {K_SUB, 0,                 0.30f, 0.0f, 1.0f, 0.90f, 0.26f, 0, 0, 0, 0, 0, 0, W_SAW, 0.012f, 6.0f, 0.3f, 0.05f, 0.002f},
    {K_SMP, SMP_UPBASS,        0.001f, 0.0f, 1.0f, 0.15f, 1.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {K_SUB, 0,                 0.004f, 3.0f, 0.6f, 0.12f, 0.80f, 0, 0, 0, 0, 0, 0, W_SINE, 0, 0, 0, 0, 0},
    {K_FM, 0,                  0.003f, 1.6f, 0.0f, 0.12f, 0.75f, 1.0f, 0.9f, 0.3f, 0.25f, 0.0f, 0.3f, 0, 0, 0, 0, 0, 0},
    {K_FM, 0,                  0.002f, 0.5f, 0.0f, 0.08f, 0.85f, 1.0f, 2.2f, 0.1f, 0.06f, 0.0f, 0.2f, 0, 0, 0, 0, 0, 0},
    {K_SUB, 0,                 0.003f, 1.2f, 0.5f, 0.10f, 0.50f, 0, 0, 0, 0, 0, 0, W_PULSE, 0.004f, 1.5f, 2.6f, 0.45f, 0},
    {K_FM, 0,                  0.045f, 0.0f, 0.9f, 0.20f, 0.40f, 2.0f, 0.12f, 1.0f, 1.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0, 0.006f},
    {K_SUB, 0,                 0.006f, 0.0f, 0.8f, 0.15f, 0.30f, 0, 0, 0, 0, 0, 0, W_PULSE, 0.0f, 3.0f, 1.0f, 0.15f, 0.003f},
};

/* parameters (the screen's names; the pages are the UI's) */
static const en_param_t PARAMS[P_NPARAMS] = {
    {"Tempo", 60, 100, 78},   {"Swing", 0, 100, 55},    {"Jazz", 0, 3, 1},         {"Busy", 0, 100, 45},
    {"Level", 0, 100, 75},    {"Tone", 0, 100, 60},     {"Release", 0, 100, 40},   {"Lazy", 0, 100, 30},
    {"Wait", 0, 3, 1},        {"Density", 0, 100, 45},  {"Range", 0, 100, 50},     {"Mood", 0, 100, 40},
    {"Level", 0, 100, 60},    {"Tone", 0, 100, 50},     {"Voicing", 0, 3, 0},      {"Roll", 0, 100, 35},
    {"Pattern", 0, 3, 0},     {"Length", 0, 100, 60},   {"Push", 0, 100, 40},      {"Spread", 0, 100, 50},
    {"Level", 0, 100, 70},    {"Tone", 0, 100, 45},     {"Pattern", 0, 3, 0},      {"Slide", 0, 100, 20},
    {"Level", 0, 100, 55},    {"Tone", 0, 100, 55},     {"Chatty", 0, 100, 50},    {"Echo", 0, 100, 40},
    {"Level", 0, 100, 70},    {"Crunch", 0, 100, 35},   {"Pattern", 0, 7, 0},      {"Fill", 0, 100, 30},
    {"Kick", 0, 100, 75},     {"Snare", 0, 100, 70},    {"Hats", 0, 100, 55},      {"Perc", 0, 100, 50},
    {"Level", 0, 100, 45},    {"Age", 0, 100, 35},      {"Surface", 0, 100, 55},   {"Hiss", 0, 100, 35},
    {"Grain", 0, 100, 30},    {"Smear", 0, 100, 50},    {"Duck", 0, 100, 40},      {"Hum", 0, 100, 10},
    {"Tape", 0, 100, 35},     {"Mellow", 0, 100, 30},   {"Room", 0, 100, 35},      {"Echo", 0, 100, 20},
    {"Root", 0, 11, 2},       {"Scale", 0, EN_NSCALES - 1, SC_DORIAN}, {"Keys", 0, 2, 1}, {"MIDI", 0, 3, 1},
    {"Lead", 0, EN_NLEAD - 1, 0}, {"Comp", 0, EN_NCOMP - 1, 0}, {"Bass", 0, EN_NBASS - 1, 0}, {"Answer", 0, EN_NANS - 1, 0},
    {"Kit", 0, EN_NKIT - 1, 0}, {"Character", 0, EN_NDUST - 1, 0},
    {"Octave", -2, 2, 0},     {"Daydream", 0, 1, 1},    {"Answer", 0, 1, 1},       {"Tune", -50, 50, 0},
    {"Fill", 0, 100, 45},     {"Legato", 0, 100, 70},   {"Repeat", 0, 100, 35},    {"Dynamics", 0, 100, 40},
    {"Drift", 0, 5, 3},       {"Drums", 0, 2, 0},       {"Pool", 2, 8, 4},         {"Chords", 0, 1, 1},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* the presets, 01-10: the factory's, as changes from the defaults above (their names are only notes here) */
typedef struct {
    uint8_t p;
    int8_t v;
} pv_t;
static const struct {
    const char *name;
    pv_t ch[24];
} PIECES[EN_NPIECES] = {
    {"Study", {{P_TEMPO, 78}, {P_ROOT, 2}, {P_SCALE, SC_DORIAN}, {P_LEAD_INST, 0}, {P_COMP_INST, 0}, {P_BASS_INST, 0},
               {P_ANS_INST, 4}, {P_KIT, 0}, {P_DUST_CHAR, DUST_VINYL}, {P_PATTERN, 0}, {0xFF, 0}}},
    {"Rainy", {{P_TEMPO, 72}, {P_ROOT, 5}, {P_SCALE, SC_MAJOR}, {P_LEAD_INST, 2}, {P_COMP_INST, 0}, {P_BASS_INST, 0},
               {P_ANS_INST, 4}, {P_KIT, 0}, {P_DUST_CHAR, DUST_CASSETTE}, {P_PATTERN, 5}, {P_COMP_PAT, 0},
               {P_HISS, 55}, {P_MELLOW, 40}, {0xFF, 0}}},
    {"Late bus", {{P_TEMPO, 84}, {P_ROOT, 7}, {P_SCALE, SC_MINOR}, {P_LEAD_INST, 8}, {P_COMP_INST, 1},
                  {P_BASS_INST, 0}, {P_BASS_PAT, 2}, {P_ANS_INST, 1}, {P_KIT, 1}, {P_DUST_CHAR, DUST_VINYL},
                  {P_PATTERN, 3}, {P_COMP_PAT, 1}, {0xFF, 0}}},
    {"Sunday", {{P_TEMPO, 88}, {P_ROOT, 0}, {P_SCALE, SC_MAJOR}, {P_LEAD_INST, 5}, {P_COMP_INST, 7}, {P_BASS_INST, 2},
                {P_ANS_INST, 2}, {P_KIT, 0}, {P_DUST_CHAR, DUST_REEL}, {P_PATTERN, 1}, {P_COMP_PAT, 2}, {0xFF, 0}}},
    {"Nightshift", {{P_TEMPO, 70}, {P_ROOT, 9}, {P_SCALE, SC_MINOR}, {P_LEAD_INST, 9}, {P_COMP_INST, 6},
                    {P_BASS_INST, 1}, {P_ANS_INST, 0}, {P_KIT, 1}, {P_DUST_CHAR, DUST_CASSETTE}, {P_PATTERN, 2},
                    {P_ROOM, 50}, {P_ECHO, 30}, {0xFF, 0}}},
    {"Attic radio", {{P_TEMPO, 90}, {P_ROOT, 4}, {P_SCALE, SC_MIXO}, {P_LEAD_INST, 7}, {P_COMP_INST, 5},
                     {P_BASS_INST, 3}, {P_ANS_INST, 3}, {P_KIT, 0}, {P_DUST_CHAR, DUST_RADIO}, {P_PATTERN, 4},
                     {P_COMP_PAT, 3}, {P_MELLOW, 45}, {0xFF, 0}}},
    {"Snowfall", {{P_TEMPO, 64}, {P_ROOT, 10}, {P_SCALE, SC_MAJOR}, {P_LEAD_INST, 6}, {P_COMP_INST, 8},
                  {P_BASS_INST, 1}, {P_ANS_INST, 7}, {P_KIT, 1}, {P_DUST_CHAR, DUST_REEL}, {P_PATTERN, 7},
                  {P_ROOM, 55}, {P_GRAIN, 50}, {0xFF, 0}}},
    {"Daydream", {{P_TEMPO, 76}, {P_ROOT, 4}, {P_SCALE, SC_PENT_MIN}, {P_LEAD_INST, 1}, {P_COMP_INST, 0},
                  {P_BASS_INST, 2}, {P_ANS_INST, 4}, {P_KIT, 0}, {P_DUST_CHAR, DUST_VINYL}, {P_PATTERN, 0},
                  {P_DD_WAIT, 3}, {P_DRIFT, 2}, {P_DRIFT_DRUMS, 2}, {P_DRIFT_POOL, 8}, {0xFF, 0}}},
    {"Coffee shop", {{P_TEMPO, 92}, {P_ROOT, 9}, {P_SCALE, SC_DORIAN}, {P_LEAD_INST, 1}, {P_COMP_INST, 7},
                     {P_BASS_INST, 2}, {P_BASS_PAT, 1}, {P_ANS_INST, 2}, {P_KIT, 1}, {P_DUST_CHAR, DUST_REEL},
                     {P_PATTERN, 6}, {P_COMP_PAT, 2}, {P_JAZZ, 2}, {P_SWING, 40}, {0xFF, 0}}},
    {"3 AM", {{P_TEMPO, 60}, {P_ROOT, 3}, {P_SCALE, SC_MINOR}, {P_LEAD_INST, 13}, {P_COMP_INST, 6}, {P_BASS_INST, 1},
              {P_ANS_INST, 8}, {P_KIT, 0}, {P_DUST_CHAR, DUST_VINYL}, {P_PATTERN, 5}, {P_AGE, 70}, {P_DUST_LVL, 60},
              {P_MELLOW, 50}, {P_ROOM, 50}, {P_GRAIN, 45}, {P_JAZZ, 3}, {P_BUSY, 25}, {0xFF, 0}}},
};
const char *en_piece_name(int i)
{
    static const char *const NUM[EN_NPIECES] = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10"};
    return NUM[(unsigned)i < EN_NPIECES ? i : 0];
}
void en_piece(int i, int16_t *par)
{
    int p, k;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = PARAMS[p].def;
    for (k = 0; k < 24 && PIECES[i].ch[k].p != 0xFF; k++)
        par[PIECES[i].ch[k].p] = PIECES[i].ch[k].v;
    par[P_TUNE] = 0;
}

/* the factory patterns: kick snare rim clap hat ohat shaker perc */
static const char *const PAT_SRC[EN_NPAT][EN_NLANE] = {
    {"x.....x...x.....", "....x.......x...", "................", "................", "x.x.x.x.x.x.x.x.",
     "................", "..x...x...x...x.", "................"},
    {"x......x..x.....", "....x.......x...", "...........x....", "................", "x.x.x.x.x.x.x.xx",
     "................", "................", "................"},
    {"x.........x.....", "........x.......", "................", "................", "x.x.x.x.x.x.x.x.",
     "..............x.", "................", "......x........."},
    {"x.....x.x.......", "................", "....x.......x...", "................", "x.xxx.x.x.xxx.x.",
     "................", "................", "................"},
    {"x..x......x..x..", "....x.......x...", "................", "............x...", "xxx.xxx.xxx.xxx.",
     "................", "................", "................"},
    {"x..........x....", "........x.......", "................", "................", "................",
     "................", "x.x.x.x.x.x.x.x.", "................"},
    {"x.x...x...x..x..", "....x..x....x...", "................", "................", "x.x.x.x.x.x.x.x.",
     "......x.......x.", "................", "...........x...x"},
    {"x.......x.......", "................", "....x.......x...", "................", "................",
     "................", "xxxxxxxxxxxxxxxx", "..............x."},
};
uint16_t en_pats[EN_NPAT][EN_NLANE];
void en_pats_default(void)
{
    int p, l, s;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++) {
            uint16_t m = 0;
            for (s = 0; s < EN_NSTEP; s++)
                if (PAT_SRC[p][l][s] == 'x')
                    m |= (uint16_t)(1u << s);
            en_pats[p][l] = m;
        }
}

/* ================================================================== text === */
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
static void copy9(char *b, const char *s)
{
    int k = 0;
    while (s[k] && k < 11) {
        b[k] = s[k];
        k++;
    }
    b[k] = 0;
}

void en_param_text(int i, int v, char *b)
{
    static const char *const JAZZ[4] = {"Triads", "7ths", "9ths", "11ths"};
    static const char *const VOI[4] = {"Close", "Open", "Shell", "Spread"};
    static const char *const CPAT[4] = {"Held", "Stabs", "Offbeat", "Arp"};
    static const char *const BPAT[4] = {"Roots", "Root-5th", "Walking", "Dub"};
    static const char *const WAIT[4] = {"4 bars", "8 bars", "16 bars", "Always"};
    static const char *const MIDI[4] = {"Off", "Notes", "Clock", "Both"};
    static const char *const ONOFF[2] = {"Off", "On"};
    static const char *const DRIFT[6] = {"Off", "4 bars", "8 bars", "16 bars", "32 bars", "64 bars"};
    static const char *const DDRUM[3] = {"Keep", "Next", "Random"};
    const char *s = 0;
    switch (i) {
    case P_JAZZ: s = JAZZ[v & 3]; break;
    case P_VOICING: s = VOI[v & 3]; break;
    case P_COMP_PAT: s = CPAT[v & 3]; break;
    case P_BASS_PAT: s = BPAT[v & 3]; break;
    case P_DD_WAIT: s = WAIT[v & 3]; break;
    case P_MIDI: s = MIDI[v & 3]; break;
    case P_SNAP: s = v == 2 ? "Chords" : v ? "Scale" : "Chromatic"; break;
    case P_DD_ON: case P_ANS_ON: s = ONOFF[v ? 1 : 0]; break;
    case P_DRIFT: s = DRIFT[(unsigned)v < 6u ? v : 0]; break;
    case P_DRIFT_DRUMS: s = DDRUM[(unsigned)v < 3u ? v : 0]; break;
    case P_DRIFT_CHORDS: s = v ? "Swap" : "Keep"; break;
    case P_ROOT: s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0]; break;
    case P_SCALE: s = EN_SCALE_NAME[(unsigned)v < EN_NSCALES ? v : 0]; break;
    case P_KIT: s = EN_KIT_NAME[v ? 1 : 0]; break;
    case P_DUST_CHAR: s = EN_DUST_NAME[(unsigned)v < EN_NDUST ? v : 0]; break;
    case P_LEAD_INST: s = EN_INST_NAME[EN_LEAD_INST[(unsigned)v < EN_NLEAD ? v : 0]]; break;
    case P_COMP_INST: s = EN_INST_NAME[EN_COMP_INST[(unsigned)v < EN_NCOMP ? v : 0]]; break;
    case P_BASS_INST: s = EN_INST_NAME[EN_BASS_INST[(unsigned)v < EN_NBASS ? v : 0]]; break;
    case P_ANS_INST: s = EN_INST_NAME[EN_ANS_INST[(unsigned)v < EN_NANS ? v : 0]]; break;
    default: break;
    }
    if (s) {
        copy9(b, s);
        return;
    }
    if (i == P_PATTERN) {
        itoa_s(v + 1, b);
        return;
    }
    if (i == P_DRIFT_POOL) {                    /* the patterns it picks from: 1-4 */
        *b++ = '1';
        *b++ = '-';
        itoa_s(v, b);
        return;
    }
    if ((i == P_TUNE || i == P_LEAD_OCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ============================================================== commands === */
enum { C_SET, C_PLAY, C_NOTE, C_HOLD, C_SIDEB, C_SEED, C_PANIC, C_REGROW, C_CLOCK, C_MUTATE, C_MUTE };
#define QN 128u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_note(int n, int on, int vel) { post(C_NOTE, n, (on ? 0x100 : 0) | (vel & 0x7F)); }
void en_hold(int on) { post(C_HOLD, on, 0); }
void en_side_b(void) { post(C_SIDEB, 0, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_mutate(void) { post(C_MUTATE, 0, 0); }
void en_mute(int on) { post(C_MUTE, on, 0); }
static uint8_t muted;                           /* remote: the FM-1 is playing the browser */
static float mute_g = 1.0f;
static volatile uint32_t fx_mask;
void en_fx(uint32_t m) { fx_mask = m; }

/* ================================================================= state === */
volatile uint8_t en_playing, en_held, en_ext, en_sideb, en_dreaming, en_step, en_auto;
volatile float en_idle_bars;
volatile uint8_t en_pattern;
volatile int16_t en_drift_left = -1;
volatile uint32_t en_bar;
volatile int8_t en_chord, en_chord_hist[3] = {-1, -1, -1};
volatile uint8_t en_chord_root, en_chord_q;
volatile uint8_t en_vnote[EN_NVOX], en_vtrk[EN_NVOX];
volatile float en_glow[EN_NTRACK], en_wear, en_spin, en_level, en_bpm;
volatile uint32_t en_notes, en_changes, en_answers, en_dream_notes, en_hits;
volatile uint8_t en_bus_on;
#ifdef EN_HOST
float en_bus[EN_NBUS * 1024];
#else
float en_bus[EN_NBUS];
#endif

static int16_t par[P_NPARAMS];
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u, nz1 = 22222u, nz2 = 7777u, nz3 = 31337u;
static uint32_t now;                            /* samples since boot */

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }
static inline float wnoise(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(int32_t)*s * (1.0f / 2147483648.0f);
}
static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static float lvl(int v) { return (float)(v * v) * 0.0001f; }
static float pc01(int p) { return (float)par[p] * 0.01f; }
static float onepole(float hz) { return 1.0f - fm_expf(-FM_TWO_PI * fm_minf(hz, 18000.0f) * (1.0f / EN_SR)); }

#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* MIDI out: lead, comp, bass, answer on channels 1-4, the drums on 10 */
static void note_out(int trk, int on, int n, int vel)
{
    if (!muted && (par[P_MIDI] == EN_MIDI_OUT || par[P_MIDI] == EN_MIDI_BOTH))
        en_midi_out((on ? 0x90u : 0x80u) | (uint32_t)trk, (uint32_t)n & 127u, on ? (uint32_t)vel : 0u);
}

/* ============================================================== harmony === */
/* the key: root and scale; the chords stack thirds on the scale's seven-note parent */
static int root_pc, scale_i;
static uint16_t mel_pool;                       /* the melody's notes, relative */
static int chord_deg, chord_tones[6], chord_n;  /* the chord now: degree, its tones (MIDI, from the root at C3) */
static int8_t hist[3] = {-1, -1, -1};

static int deg_note(int d)                      /* a degree (any, >= 0) of the parent, in semitones over the root */
{
    return PARENT[scale_i][d % 7] + 12 * (d / 7);
}
static int jazz_n(void) { static const int8_t N[4] = {3, 4, 5, 6}; return N[par[P_JAZZ] & 3]; }

static void chord_build(int d, int *t, int *n)  /* the chord's tones: root, 3rd, 5th, 7th, 9th, 11th as Jazz allows */
{
    int k, m = jazz_n();
    for (k = 0; k < m; k++)
        t[k] = 48 + root_pc + deg_note(d + 2 * k);
    *n = m;
    if (m >= 5) {                               /* ninths and up: the fifth out of the way, as players do */
        for (k = 2; k < m - 1; k++)
            t[k] = t[k + 1];
        *n = m - 1;
    }
}

static int chord_quality(int d)
{
    int r = deg_note(d), t3 = deg_note(d + 2) - r, t5 = deg_note(d + 4) - r, t7 = deg_note(d + 6) - r;
    if (par[P_JAZZ] == 0)
        return t5 == 6 ? EN_Q_DIM : t3 == 4 ? EN_Q_MAJ : EN_Q_MIN;
    if (t5 == 6)
        return EN_Q_HDIM;
    if (t3 == 4)
        return t7 == 11 ? EN_Q_MAJ7 : EN_Q_DOM;
    return EN_Q_MIN7;
}

static void name_of(int rootpc, int q, char *b)
{
    static const char *const Q[7] = {"", "m", "dim", "7", "maj7", "m7", "m7b5"};
    static const char *const EXT[4] = {"", "", "9", "11"};
    const char *n = EN_NOTE_NAME[rootpc % 12], *q2 = Q[q % 7];
    int j = par[P_JAZZ] & 3;
    while (*n)
        *b++ = *n++;
    if (j >= 2 && (q == EN_Q_MIN7 || q == EN_Q_DOM || q == EN_Q_MAJ7)) {
        q2 = q == EN_Q_MIN7 ? "m" : q == EN_Q_MAJ7 ? "maj" : "";
        while (*q2)
            *b++ = *q2++;
        q2 = EXT[j];
    }
    while (*q2)
        *b++ = *q2++;
    *b = 0;
}
void en_chord_name(char *b) { name_of(en_chord_root, en_chord_q, b); }
void en_deg_name(int d, char *b) { name_of((root_pc + deg_note(d)) % 12, chord_quality(d), b); }

static void chord_set(int d)
{
    if (d != chord_deg) {
        hist[2] = hist[1];
        hist[1] = hist[0];
        hist[0] = (int8_t)chord_deg;
        en_chord_hist[0] = hist[0];
        en_chord_hist[1] = hist[1];
        en_chord_hist[2] = hist[2];
        en_changes++;
    }
    chord_deg = d;
    chord_build(d, chord_tones, &chord_n);
    en_chord = (int8_t)d;
    en_chord_root = (uint8_t)((root_pc + deg_note(d)) % 12);
    en_chord_q = (uint8_t)chord_quality(d);
}

/* the progressions it cycles while nobody plays (degrees, a bar each) */
static const int8_t PROG[EN_NSCALES][2][4] = {
    {{1, 4, 0, 5}, {3, 2, 1, 4}},               /* major: ii V I vi; IV iii ii V */
    {{0, 5, 2, 6}, {0, 3, 6, 2}},               /* minor: i VI III VII; i iv VII III */
    {{0, 3, 0, 6}, {0, 2, 3, 6}},               /* dorian: i IV i VII; i III IV VII */
    {{0, 6, 3, 0}, {0, 4, 6, 3}},               /* mixolydian: I bVII IV I */
    {{0, 5, 2, 6}, {0, 3, 6, 2}},
    {{1, 4, 0, 5}, {0, 3, 1, 4}},
    {{0, 3, 0, 6}, {0, 3, 4, 6}},
};
static int prog_i, prog_k;

/* what the lead plays: held notes, and the last ones played with their age */
#define NREC 8
static struct {
    uint8_t note;
    uint32_t t;
    uint8_t down;                               /* on a beat */
} rec[NREC];
static int rec_w;
static uint32_t lead_held[4];                   /* held keys: bit = MIDI note */
static uint32_t last_lead_t;                    /* the last time a key went down (samples) */
static uint8_t lead_active;                     /* someone played in the last two bars */

static float spb;                               /* samples a sixteenth */

static int chord_has(int d, int pc, float *w)   /* how well pitch class pc sits in degree d's chord */
{
    static const float W[6] = {1.0f, 0.9f, 0.7f, 0.6f, 0.3f, 0.25f};
    int k, m = jazz_n();
    for (k = 0; k < m; k++)
        if ((root_pc + deg_note(d + 2 * k)) % 12 == pc) {
            *w = W[k];
            return 1;
        }
    *w = -0.55f;
    return 0;
}

static int follow(void)                         /* the degree that best fits the lead, or -1 to keep */
{
    static const float PRIOR[7] = {0.25f, 0.18f, 0.05f, 0.2f, 0.16f, 0.18f, -0.25f};
    float best = -1e9f, sc[7];
    int d, k, bd = chord_deg, low = -1, nheld = 0, i;
    for (i = 0; i < 128; i++)
        if (lead_held[i >> 5] >> (i & 31) & 1u) {
            if (low < 0)
                low = i;
            nheld++;
        }
    for (d = 0; d < 7; d++) {
        float s = PRIOR[d] + (d == chord_deg ? 0.35f : 0.0f), w;
        for (i = 0; i < 128; i++)
            if (lead_held[i >> 5] >> (i & 31) & 1u) {
                chord_has(d, i % 12, &w);
                s += w * 1.0f;
                if (i == low && nheld >= 2 && (root_pc + deg_note(d)) % 12 == i % 12)
                    s += 0.6f;
            }
        for (k = 0; k < NREC; k++) {
            float age;
            if (!rec[k].note)
                continue;
            age = (float)(now - rec[k].t) / (4.0f * spb);   /* in beats */
            if (age > 6.0f)
                continue;
            chord_has(d, rec[k].note % 12, &w);
            s += w * 0.6f * fm_expf(-age * 0.55f) * (rec[k].down ? 1.4f : 1.0f);
        }
        sc[d] = s;
        if (s > best) {
            best = s;
            bd = d;
        }
    }
    return best > sc[chord_deg] + 0.2f ? bd : -1;
}

static void lead_remember(int n, int on_beat)
{
    rec[rec_w].note = (uint8_t)n;
    rec[rec_w].t = now;
    rec[rec_w].down = (uint8_t)on_beat;
    rec_w = (rec_w + 1) % NREC;
}

/* ================================================================ voices === */
/* IMA ADPCM, as the sets were encoded (lof_samples.h): 4 bits a sample, low nibble first */
static const int16_t ISTEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767};
static const int8_t IIDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

typedef struct {                                /* a sample being read: where, and the decoder's state */
    const smpz_t *z;
    uint32_t k;                                 /* the index of s1 (the sample after the read point) */
    float frac, step, s0, s1;
    int32_t pred;
    int16_t idx;
    uint8_t done;
} smp_t;

static inline float smp_next(smp_t *s)          /* decode sample k, advance */
{
    const smpz_t *z = s->z;
    uint32_t k = s->k;
    int c, st, d;
    if (k >= z->n) {
        if (z->looped) {
            k = z->ls;
            s->pred = z->pred;
            s->idx = z->idx;
        } else {
            s->done = 1;
            return 0.0f;
        }
    }
    c = (SMP_DATA[z->off + (k >> 1)] >> ((k & 1u) * 4u)) & 15;
    st = ISTEP[s->idx];
    d = st >> 3;
    if (c & 4)
        d += st;
    if (c & 2)
        d += st >> 1;
    if (c & 1)
        d += st >> 2;
    s->pred += c & 8 ? -d : d;
    if (s->pred > 32767)
        s->pred = 32767;
    else if (s->pred < -32768)
        s->pred = -32768;
    s->idx = (int16_t)(s->idx + IIDX[c & 7]);
    if (s->idx < 0)
        s->idx = 0;
    else if (s->idx > 88)
        s->idx = 88;
    s->k = k + 1;
    return (float)s->pred * (1.0f / 32768.0f);
}

static void smp_start(smp_t *s, const smpz_t *z, float step)
{
    s->z = z;
    s->k = 0;
    s->pred = 0;
    s->idx = 0;
    s->done = 0;
    s->frac = 0.0f;
    s->step = step;
    s->s0 = 0.0f;
    s->s1 = smp_next(s);
}

static inline float smp_read(smp_t *s)
{
    float x = s->s0 + (s->s1 - s->s0) * s->frac;
    s->frac += s->step;
    while (s->frac >= 1.0f && !s->done) {
        s->frac -= 1.0f;
        s->s0 = s->s1;
        s->s1 = smp_next(s);
    }
    return s->done ? 0.0f : x;
}

static const smpz_t *zone_for(int set, int note)
{
    const smps_t *st = &SMP_SET[set];
    int k;
    for (k = 0; k < st->nz; k++) {
        const smpz_t *z = &SMP_ZONE[st->z0 + k];
        if (note >= z->lo && note <= z->hi)
            return z;
    }
    return &SMP_ZONE[st->z0];
}


#define NTRK 4                                  /* the pitched tracks */
typedef struct {
    uint8_t on, trk, note, tag, inst, rel, att;
    uint32_t age;
    float a, ainc, dmul, sus, rmul, vel, gl, gr;
    float inc, inc_t, glide;                    /* the pitch (turns a sample), gliding to inc_t */
    float cph, mph, idx, idxt, icoef, bph, bmph, bamp;   /* FM */
    float ph2, lp, bp, fenv, fdec;              /* SUB */
    float vib, lpo;                             /* vibrato phase; the track's tone (a one-pole) */
    smp_t s;
} voice_t;
static voice_t vx[EN_NVOX];
static uint32_t vx_age;
static const uint8_t TRK_MAX[NTRK] = {10, 7, 1, 2};
static float trk_lvl[NTRK], trk_tone[NTRK], trk_rel[NTRK];
static uint8_t trk_inst[NTRK];

static int voice_inst(int trk) { return trk_inst[trk]; }

static voice_t *voice_alloc(int trk)
{
    int i, n = 0, best = -1, pass;
    uint32_t oldest;
    for (i = 0; i < EN_NVOX; i++)
        if (vx[i].on && vx[i].trk == trk)
            n++;
    if (n < TRK_MAX[trk])
        for (i = 0; i < EN_NVOX; i++)
            if (!vx[i].on)
                return &vx[i];
    /* the track full (or no voice free): its oldest released note, then its oldest; then anyone's */
    for (pass = 0; pass < 4 && best < 0; pass++) {
        oldest = 0xFFFFFFFFu;
        for (i = 0; i < EN_NVOX; i++) {
            int mine = vx[i].trk == trk;
            if ((pass < 2 && !mine) || ((pass & 1) == 0 && !vx[i].rel))
                continue;
            if (vx[i].age < oldest) {
                oldest = vx[i].age;
                best = i;
            }
        }
    }
    return &vx[best < 0 ? 0 : best];
}

static float env_mul(float secs, float db)      /* per-sample multiplier: db down in secs */
{
    return secs <= 0.0f ? 0.0f : fm_exp2f(-db * 0.16609640f / (secs * EN_SR));
}

static void voice_on(int trk, int note, float vel, int tag, float pan)
{
    const inst_t *in = &INST[voice_inst(trk)];
    voice_t *v;
    float f = mtof((float)note), rl;
    int legato = 0, i;
    if (trk == T_BASS && par[P_SLIDE] > 2) {    /* the bass slides into its next note */
        for (i = 0; i < EN_NVOX; i++)
            if (vx[i].on && vx[i].trk == T_BASS && !vx[i].rel && vx[i].inst == voice_inst(trk) && in->kind != K_SMP) {
                v = &vx[i];
                note_out(trk, 0, v->note, 0);
                v->note = (uint8_t)note;
                v->inc_t = f * (1.0f / EN_SR);
                v->glide = 1.0f - fm_expf(-1.0f / (EN_SR * (0.008f + 0.12f * pc01(P_SLIDE))));
                v->vel = vel;
                legato = 1;
                en_vnote[v - vx] = (uint8_t)note;
                note_out(trk, 1, note, (int)(vel * 127.0f));
                return;
            }
    }
    (void)legato;
    v = voice_alloc(trk);
    if (v->on)
        note_out(v->trk, 0, v->note, 0);
    v->on = 1;
    v->trk = (uint8_t)trk;
    v->note = (uint8_t)note;
    v->tag = (uint8_t)tag;
    v->inst = (uint8_t)voice_inst(trk);
    v->rel = 0;
    v->age = ++vx_age;
    v->vel = vel;
    v->a = 0.0f;
    v->att = 1;
    v->ainc = 1.0f / (fm_maxf(in->att, 0.0005f) * EN_SR);
    v->sus = in->sus;
    v->dmul = in->dec > 0.0f ? env_mul(in->dec * (note < 48 ? 1.3f : note > 72 ? 0.7f : 1.0f), 60.0f) : 1.0f;
    rl = in->rel * (0.4f + 2.2f * trk_rel[trk]);
    v->rmul = env_mul(rl, 60.0f);
    pan = fm_clampf(pan, -1.0f, 1.0f);
    v->gl = fm_sqrtf(0.5f * (1.0f - pan));
    v->gr = fm_sqrtf(0.5f * (1.0f + pan));
    v->inc = v->inc_t = f * (1.0f / EN_SR);
    v->glide = 1.0f;
    v->vib = frand();
    v->lpo = 0.0f;
    switch (in->kind) {
    case K_SMP: {
        const smpz_t *z = zone_for(in->set, note);
        smp_start(&v->s, z, (float)z->rate * (1.0f / 65536.0f) * fm_exp2f(((float)note - z->root) * (1.0f / 12.0f)) * tunefac);
        break;
    }
    case K_FM:
        v->cph = 0.0f;
        v->mph = 0.25f;
        v->idx = in->idx * (0.6f + 0.6f * vel);
        v->idxt = in->idx * in->idxs;
        v->icoef = 1.0f - fm_expf(-1.0f / (fm_maxf(in->idxt, 0.01f) * EN_SR));
        v->bph = v->bmph = 0.0f;
        v->bamp = in->bell * vel;
        break;
    case K_SUB:
        v->cph = frand();
        v->ph2 = frand();
        v->lp = v->bp = 0.0f;
        v->fenv = 1.0f;
        v->fdec = env_mul(0.25f + in->dec * 0.2f, 40.0f);
        break;
    case K_ORG:
        v->cph = 0.0f;
        v->mph = 0.0f;
        v->ph2 = 0.0f;
        break;
    }
    en_vnote[v - vx] = (uint8_t)note;
    en_vtrk[v - vx] = (uint8_t)trk;
    en_glow[trk] = 1.0f;
    note_out(trk, 1, note, (int)(vel * 127.0f));
}

static void voice_off(int trk, int note, int tag)
{
    int i;
    for (i = 0; i < EN_NVOX; i++)
        if (vx[i].on && !vx[i].rel && vx[i].trk == trk && vx[i].note == note && (tag < 0 || vx[i].tag == tag)) {
            vx[i].rel = 1;
            vx[i].att = 0;
            note_out(trk, 0, note, 0);
        }
}

static void track_release(int trk, int tag)     /* every note of a track (of a tag, or all: -1) */
{
    int i;
    for (i = 0; i < EN_NVOX; i++)
        if (vx[i].on && !vx[i].rel && vx[i].trk == trk && (tag < 0 || vx[i].tag == tag)) {
            vx[i].rel = 1;
            vx[i].att = 0;
            note_out(trk, 0, vx[i].note, 0);
        }
}

#define SUBN 32                                 /* the render's step: events land on these */
static float bus_l[NTRK][SUBN], bus_r[NTRK][SUBN];

static void render_voice(voice_t *v, uint32_t n)
{
    const inst_t *in = &INST[v->inst];
    float a = v->a, g = in->gain * trk_lvl[v->trk] * (0.35f + 0.65f * v->vel), gl = v->gl * g, gr = v->gr * g;
    float tc = trk_tone[v->trk], lpo = v->lpo, inc = v->inc;
    float *bl = bus_l[v->trk], *br = bus_r[v->trk];
    uint32_t j;
    float vib = 1.0f;
    if (in->vib > 0.0f) {
        v->vib += (float)n * (5.2f / EN_SR);
        if (v->vib >= 1.0f)
            v->vib -= 1.0f;
        vib = 1.0f + in->vib * fm_sin_turns(v->vib);
    }
    for (j = 0; j < n; j++) {
        float x;
        /* the envelope */
        if (v->att) {
            a += v->ainc;
            if (a >= 1.0f) {
                a = 1.0f;
                v->att = 0;
            }
        } else if (v->rel) {
            a *= v->rmul;
        } else if (v->sus < 1.0f) {
            a = v->sus + (a - v->sus) * v->dmul;
        }
        if (v->glide < 1.0f)
            inc += (v->inc_t - inc) * v->glide;
        switch (in->kind) {
        case K_SMP:
            x = smp_read(&v->s);
            if (v->s.done)
                a = 0.0f;
            break;
        case K_FM: {
            float m = sine(v->mph + 4.0f) * v->idx;
            x = sine(v->cph + m + 4.0f);
            if (v->bamp > 0.0001f) {            /* the tine */
                x += v->bamp * sine(v->bph + sine(v->bmph) * 0.6f + 4.0f);
                v->bph += inc * vib;
                v->bmph += inc * vib * 14.0f;
                if (v->bph >= 1.0f)
                    v->bph -= 1.0f;
                if (v->bmph >= 1.0f)
                    v->bmph -= (float)(int)v->bmph;
                v->bamp *= 0.99965f;
            }
            if (in->drive > 0.0f)
                x = fm_tanhf(x * (1.0f + 2.5f * in->drive * v->idx)) * 0.85f;
            v->cph += inc * vib;
            v->mph += inc * vib * in->ratio;
            if (v->cph >= 1.0f)
                v->cph -= 1.0f;
            if (v->mph >= 1.0f)
                v->mph -= (float)(int)v->mph;
            v->idx += (v->idxt - v->idx) * v->icoef;
            break;
        }
        case K_SUB: {
            float o, f, k, hp;
            if (in->wave == W_SINE) {
                o = sine(v->cph) + 0.25f * sine(v->cph * 2.0f);
            } else if (in->wave == W_PULSE) {
                o = (v->cph < 0.5f ? 0.8f : -0.8f) + 0.4f * (v->ph2 < 0.5f ? 1.0f : -1.0f);
            } else {
                o = (2.0f * v->cph - 1.0f) + (2.0f * v->ph2 - 1.0f);
                o *= 0.6f;
            }
            v->cph += inc * vib;
            v->ph2 += inc * vib * (in->wave == W_PULSE ? 0.5f : 1.0f + in->detune);
            if (v->cph >= 1.0f)
                v->cph -= 1.0f;
            if (v->ph2 >= 1.0f)
                v->ph2 -= 1.0f;
            if (in->wave == W_SINE) {
                x = o;
                break;
            }
            f = inc * EN_SR * in->cut * fm_exp2f(in->env * v->fenv);
            v->fenv *= v->fdec;
            k = 2.0f * fm_sin_pi(fm_minf(f, 12000.0f) * (1.0f / EN_SR));   /* Chamberlin state variable */
            hp = o - v->lp - in->res * 2.0f * v->bp;
            v->bp += k * hp;
            v->lp += k * v->bp;
            v->bp = fm_flush(v->bp);
            v->lp = fm_flush(v->lp);
            x = v->lp;
            break;
        }
        default: {                              /* the organ: 8', 4', 2 2/3' */
            x = 0.6f * sine(v->cph) + 0.4f * sine(v->mph) + 0.25f * sine(v->ph2);
            v->cph += inc * vib;
            v->mph += inc * vib * 2.0f;
            v->ph2 += inc * vib * 3.0f;
            if (v->cph >= 1.0f)
                v->cph -= 1.0f;
            if (v->mph >= 1.0f)
                v->mph -= 1.0f;
            if (v->ph2 >= 1.0f)
                v->ph2 -= 1.0f;
            break;
        }
        }
        lpo += tc * (x * a - lpo);
        bl[j] += lpo * gl;
        br[j] += lpo * gr;
    }
    v->a = a;
    v->inc = inc;
    v->lpo = fm_flush(lpo);
    if (a < 0.0002f && !v->att && (v->rel || v->sus <= 0.0f || (in->kind == K_SMP && v->s.done))) {
        if (!v->rel)
            note_out(v->trk, 0, v->note, 0);
        v->on = 0;
        en_vtrk[v - vx] = 0xFF;
    }
}

/* ============================================================== events === */
/* notes and hits waiting for their sample: the comp's rolled chords, the bass, the answer, the ghost, the
 * drums, a lazy key */
enum { EV_ON, EV_OFF, EV_DRUM };
#define NEV 128
static struct {
    uint32_t t, len;
    uint8_t used, type, trk, note, vel, tag;
    int8_t pan;
} ev[NEV];

static void ev_add(uint32_t t, int type, int trk, int note, float vel, uint32_t len, int tag, float pan)
{
    int i;
    for (i = 0; i < NEV; i++)
        if (!ev[i].used) {
            ev[i].used = 1;
            ev[i].t = t;
            ev[i].len = len;
            ev[i].type = (uint8_t)type;
            ev[i].trk = (uint8_t)trk;
            ev[i].note = (uint8_t)note;
            ev[i].vel = (uint8_t)(fm_clampf(vel, 0.0f, 1.0f) * 127.0f + 0.5f);
            ev[i].tag = (uint8_t)tag;
            ev[i].pan = (int8_t)(fm_clampf(pan, -1.0f, 1.0f) * 100.0f);
            return;
        }
}
static void ev_note(uint32_t t, int trk, int note, float vel, uint32_t len, int tag, float pan)
{
    ev_add(t, EV_ON, trk, note, vel, len, tag, pan);
}
static void ev_cancel(int trk, int tag)        /* notes not played yet: gone */
{
    int i;
    for (i = 0; i < NEV; i++)
        if (ev[i].used && ev[i].trk == trk && ev[i].type == EV_ON && (tag < 0 || ev[i].tag == tag))
            ev[i].used = 0;
}

static void drum_hit(int lane, float vel);
static void ev_due(void)
{
    int i;
    for (i = 0; i < NEV; i++) {
        if (!ev[i].used || (int32_t)(ev[i].t - now) > 0)
            continue;
        ev[i].used = 0;
        switch (ev[i].type) {
        case EV_ON:
            voice_on(ev[i].trk, ev[i].note, (float)ev[i].vel * (1.0f / 127.0f), ev[i].tag, (float)ev[i].pan * 0.01f);
            if (ev[i].len)
                ev_add(now + ev[i].len, EV_OFF, ev[i].trk, ev[i].note, 0.0f, 0, ev[i].tag, 0.0f);
            break;
        case EV_OFF: voice_off(ev[i].trk, ev[i].note, ev[i].tag); break;
        case EV_DRUM: drum_hit(ev[i].note, (float)ev[i].vel * (1.0f / 127.0f)); break;
        }
    }
}

/* ================================================================= drums === */
/* the Dusty kit plays the set's hits (KICK BOOM SNARE TOMSN RIM CLAP HAT OHAT SHAKE); the Tape kit is
 * synthesized. The kick layers the cajon with the bass drum's boom. */
static const int8_t DUSTY_ZONE[EN_NLANE] = {0, 2, 4, 5, 6, 7, 8, 3};
static const float LANE_PAN[EN_NLANE] = {0.0f, 0.05f, -0.2f, 0.15f, 0.3f, 0.3f, -0.35f, -0.25f};
#define NDRUM 12
typedef struct {
    uint8_t on, lane, synth;
    float amp, dec, t, ph, f, lp, bp, hp, x1;
    smp_t s;
} drum_t;
static drum_t dv[NDRUM];
static float drum_l[SUBN], drum_r[SUBN], drum_lvl, lane_lvl[EN_NLANE], kick_env, crunch;
static float crush_hold, crush_cnt;

static drum_t *drum_alloc(int lane)
{
    int i, best = 0;
    float lo = 10.0f;
    for (i = 0; i < NDRUM; i++)
        if (!dv[i].on)
            return &dv[i];
    for (i = 0; i < NDRUM; i++)
        if (dv[i].amp < lo && dv[i].lane != LN_KICK) {
            lo = dv[i].amp;
            best = i;
        }
    (void)lane;
    return &dv[best];
}

static void drum_hit(int lane, float vel)
{
    drum_t *d;
    int i;
    if (lane == LN_HAT)                          /* the closed hat chokes the open one */
        for (i = 0; i < NDRUM; i++)
            if (dv[i].on && dv[i].lane == LN_OHAT)
                dv[i].dec = env_mul(0.03f, 60.0f);
    d = drum_alloc(lane);
    d->on = 1;
    d->lane = (uint8_t)lane;
    d->synth = (uint8_t)(par[P_KIT] == 1);
    d->amp = vel;
    d->t = 0.0f;
    d->ph = 0.0f;
    d->lp = d->bp = d->hp = d->x1 = 0.0f;
    en_hits++;
    en_glow[T_DRUMS] = fm_maxf(en_glow[T_DRUMS], lane == LN_KICK || lane == LN_SNARE ? 1.0f : 0.5f);
    if (lane == LN_KICK)
        kick_env = fm_maxf(kick_env, vel);
    note_out(9, 1, (int)(const uint8_t[]){36, 38, 37, 39, 42, 46, 70, 45}[lane], (int)(vel * 127.0f));
    if (!d->synth) {
        const smpz_t *z = &SMP_ZONE[SMP_SET[SMP_KIT].z0 + DUSTY_ZONE[lane]];
        smp_start(&d->s, z, (float)z->rate * (1.0f / 65536.0f));
        d->dec = 1.0f;
        if (lane == LN_KICK) {                  /* the boom under the cajon */
            drum_t *b = drum_alloc(LN_PERC);
            const smpz_t *zb = &SMP_ZONE[SMP_SET[SMP_KIT].z0 + 1];
            *b = *d;
            b->amp = vel * 0.55f;
            b->lane = LN_KICK;
            smp_start(&b->s, zb, (float)zb->rate * (1.0f / 65536.0f) * 1.12f);
        }
        return;
    }
    switch (lane) {                             /* the Tape kit: decay times */
    case LN_KICK: d->dec = env_mul(0.42f, 60.0f); break;
    case LN_SNARE: d->dec = env_mul(0.22f, 60.0f); break;
    case LN_RIM: d->dec = env_mul(0.05f, 60.0f); break;
    case LN_CLAP: d->dec = env_mul(0.20f, 60.0f); break;
    case LN_HAT: d->dec = env_mul(0.05f, 60.0f); break;
    case LN_OHAT: d->dec = env_mul(0.35f, 60.0f); break;
    case LN_SHAKE: d->dec = env_mul(0.09f, 60.0f); break;
    default: d->dec = env_mul(0.30f, 60.0f); break;
    }
}

static void render_drums(uint32_t n)
{
    int i;
    uint32_t j;
    for (j = 0; j < n; j++)
        drum_l[j] = drum_r[j] = 0.0f;
    for (i = 0; i < NDRUM; i++) {
        drum_t *d = &dv[i];
        float g, gl, gr, pan;
        if (!d->on)
            continue;
        pan = LANE_PAN[d->lane];
        g = lane_lvl[d->lane] * drum_lvl;
        gl = g * (1.0f - pan) * 0.7f;
        gr = g * (1.0f + pan) * 0.7f;
        for (j = 0; j < n; j++) {
            float x, t = d->t;
            if (!d->synth) {
                x = smp_read(&d->s) * d->amp * 1.3f;
                if (d->s.done) {
                    d->on = 0;
                    break;
                }
                d->amp *= d->dec;
            } else {
                float nz = wnoise(&nz1);
                switch (d->lane) {
                case LN_KICK: {
                    float f = 46.0f + 120.0f * fm_expf(-t * (1.0f / (0.032f * EN_SR)));
                    d->ph += f * (1.0f / EN_SR);
                    if (d->ph >= 1.0f)
                        d->ph -= 1.0f;
                    x = fm_tanhf(1.6f * sine(d->ph)) * 0.95f + (t < 60.0f ? nz * 0.25f : 0.0f);
                    break;
                }
                case LN_SNARE:
                    d->ph += 182.0f * (1.0f / EN_SR);
                    if (d->ph >= 1.0f)
                        d->ph -= 1.0f;
                    d->bp += 0.33f * (nz - d->bp);
                    x = 0.55f * (nz - d->bp) * 0.9f + 0.5f * sine(d->ph) * fm_expf(-t * (1.0f / (0.06f * EN_SR)));
                    break;
                case LN_RIM:
                    d->ph += 1650.0f * (1.0f / EN_SR);
                    if (d->ph >= 1.0f)
                        d->ph -= 1.0f;
                    x = 0.6f * sine(d->ph) + 0.25f * nz;
                    break;
                case LN_CLAP: {
                    float burst = (t < 300.0f || (t > 440.0f && t < 740.0f) || (t > 880.0f && t < 1180.0f)) ? 1.0f : 0.45f;
                    d->bp += 0.25f * (nz - d->bp);
                    d->lp += 0.25f * (d->bp - d->lp);
                    x = (d->bp - d->lp) * 2.2f * burst;
                    break;
                }
                case LN_HAT: case LN_OHAT:
                    x = nz - d->x1;
                    d->x1 = nz;
                    x *= 0.5f;
                    break;
                case LN_SHAKE:
                    d->bp += 0.5f * (nz - d->bp);
                    x = (nz - d->bp) * fm_minf(t * (1.0f / 660.0f), 1.0f) * 0.7f;
                    break;
                default: {
                    float f = 110.0f + 40.0f * fm_expf(-t * (1.0f / (0.05f * EN_SR)));
                    d->ph += f * (1.0f / EN_SR);
                    if (d->ph >= 1.0f)
                        d->ph -= 1.0f;
                    x = sine(d->ph) * 0.8f;
                    break;
                }
                }
                d->lp += 0.35f * (x - d->lp);   /* tape: the top rolled off */
                x = d->lp * d->amp;
                d->amp *= d->dec;
                d->t = t + 1.0f;
                if (d->amp < 0.0003f) {
                    d->on = 0;
                    break;
                }
            }
            drum_l[j] += x * gl;
            drum_r[j] += x * gr;
        }
    }
    if (crunch > 0.01f) {                       /* crunch: driven, and a little sample-rate loss */
        float drv = 1.0f + 3.5f * crunch, mk = 1.0f / fm_tanhf(drv * 0.5f) * 0.5f, hold = 1.0f + 5.0f * crunch * crunch;
        for (j = 0; j < n; j++) {
            float l = fm_tanhf(drum_l[j] * drv) * mk, r = fm_tanhf(drum_r[j] * drv) * mk;
            crush_cnt += 1.0f;
            if (crush_cnt >= hold) {
                crush_cnt -= hold;
                crush_hold = 0.5f * (l + r);
            }
            drum_l[j] = l + (crush_hold - l) * crunch * 0.35f;
            drum_r[j] = r + (crush_hold - r) * crunch * 0.35f;
        }
    }
}

/* ============================================================== the band === */
static float step_left;                         /* samples until the next sixteenth */
static uint32_t step_count;                     /* sixteenths since play */
static int pat_now, pat_next;                   /* the drum pattern; a new one waits for the bar */
static uint8_t auto_mode;                       /* nobody playing: the band cycles a progression */
static uint8_t chord_new;                       /* the chord changed on this beat */
static int comp_prev[8], comp_prev_n;           /* the comp's last voicing, for voice leading */
static float drum_gate = 1.0f;
static uint8_t fill_now;                        /* Side B: a fill, back in on the next bar */
static uint32_t drift_bars;                     /* Drift: bars since the last change */
static uint8_t drift_fill, mutate_now;          /* a fill before the change; a change asked for (SEL + SEQ) */
static uint32_t fxm;                            /* the effects held, as the render took them */
static float idle_bars;                         /* bars since a key went down (or since play) */
static float key_bars = 99.0f;                  /* bars since a key went down */

static float swing_delay(int s) { return (s & 1) ? spb * 0.45f * pc01(P_SWING) : 0.0f; }
static uint32_t T(float dt) { return now + (uint32_t)(dt > 0.0f ? dt : 0.0f); }

static int next_auto_deg(void) { return PROG[scale_i][prog_i][(prog_k) & 3]; }

/* a note of the scale near n (the melody's pool) */
static int in_scale(int n) { return mel_pool >> (((n - root_pc) % 12 + 12) % 12) & 1u; }
static int snap_scale(int n)
{
    int k;
    for (k = 0; k < 6; k++) {
        if (in_scale(n - k))
            return n - k;
        if (in_scale(n + k))
            return n + k;
    }
    return n;
}
static int is_chord_tone(int n)
{
    int k;
    for (k = 0; k < chord_n; k++)
        if (((n - chord_tones[k]) % 12 + 12) % 12 == 0)
            return 1;
    return 0;
}
static int nearest_chord_tone(int n)
{
    int k;
    for (k = 0; k < 7; k++) {
        if (is_chord_tone(n - k))
            return n - k;
        if (is_chord_tone(n + k))
            return n + k;
    }
    return n;
}
static int scale_step(int n, int dir)           /* the next note of the scale up (1) or down (-1) */
{
    int k;
    for (k = 1; k < 12; k++)
        if (in_scale(n + dir * k))
            return n + dir * k;
    return n + dir;
}

/* ------------------------------------------------------------------ comp --- */
static int voicing(int *out)
{
    int pcs[6], n = chord_n, k, best = 0, i;
    float bestc = 1e9f, prevc = 0.0f;
    int v = par[P_VOICING] & 3, root = chord_tones[0] % 12;
    for (k = 0; k < n; k++)
        pcs[k] = chord_tones[k] % 12;
    for (k = 0; k < comp_prev_n; k++)
        prevc += (float)comp_prev[k];
    prevc = comp_prev_n ? prevc / (float)comp_prev_n : 62.0f;
    if (v == 2) {                               /* shell: the root low, the third and seventh over it */
        out[0] = 48 + ((root - 48 % 12) + 12) % 12;
        if (out[0] > 54)
            out[0] -= 12;
        out[1] = out[0] + ((pcs[1] - root + 12) % 12);
        out[2] = out[0] + (n > 3 ? ((pcs[3] - root + 12) % 12) : ((pcs[2] - root + 12) % 12));
        if (out[2] <= out[1])
            out[2] += 12;
        out[1] += 12;
        out[2] += out[2] < out[1] ? 12 : 0;
        return 3;
    }
    if (v == 3) {                               /* spread: wide, the root at the bottom, the colours on top */
        int m = 0;
        out[m++] = 48 + ((root - 0) % 12);
        if (out[0] > 53)
            out[0] -= 12;
        for (k = 1; k < n; k++)
            out[m++] = out[0] + 12 + ((pcs[k] - root + 12) % 12) + (k >= 2 ? 12 * (k >= 3) : 0);
        return m;
    }
    for (i = 0; i < n; i++) {                   /* close (or open): the inversion nearest the last chord */
        int t[6], lo, c = 0;
        float cen;
        lo = 53 + ((pcs[i] - 53 % 12 + 24) % 12);
        if (lo > 64)
            lo -= 12;
        t[0] = lo;
        for (k = 1; k < n; k++) {
            int p = pcs[(i + k) % n];
            t[k] = t[k - 1] + ((p - t[k - 1] % 12 + 12) % 12);
            if (t[k] == t[k - 1])
                t[k] += 12;
        }
        for (k = 0; k < n; k++)
            c += t[k];
        cen = (float)c / (float)n;
        cen = fm_fabsf(cen - prevc) + 0.5f * fm_fabsf(cen - 62.0f);
        if (cen < bestc) {
            bestc = cen;
            best = i;
            for (k = 0; k < n; k++)
                out[k] = t[k];
        }
    }
    (void)best;
    if (v == 1 && n >= 3)                       /* open: drop the second from the top */
        out[n - 2] -= 12;
    return n;
}

static void comp_strike(uint32_t t0, float len_steps, float vel)
{
    int notes[8], n = voicing(notes), k;
    float roll = (0.004f + 0.045f * pc01(P_ROLL)) * EN_SR, sp = pc01(P_SPREAD);
    uint32_t len = (uint32_t)(len_steps * spb);
    for (k = 0; k < n; k++) {
        float jit = frand() * 0.004f * EN_SR, pan = n > 1 ? sp * (2.0f * (float)k / (float)(n - 1) - 1.0f) * 0.8f : 0.0f;
        ev_note(t0 + (uint32_t)((float)k * roll + jit), T_COMP, notes[k], vel * (k == n - 1 ? 1.08f : 0.95f),
                len > 0u ? len : 1u, 0, pan);
        comp_prev[k] = notes[k];
    }
    comp_prev_n = n;
}

static void comp_step(int s, uint32_t t0)
{
    float L = pc01(P_COMP_LEN), busy = pc01(P_BUSY), v = 0.5f + 0.12f * frand();
    switch (par[P_COMP_PAT] & 3) {
    case 0:                                     /* held */
        if (s == 0 || chord_new)
            comp_strike(t0, (s == 0 ? 16.0f : 8.0f) * (0.25f + 0.75f * L), v);
        else if (s == 8 && busy > 0.55f && frand() < busy)
            comp_strike(t0, 8.0f * (0.2f + 0.6f * L), v * 0.75f);
        break;
    case 1:                                     /* stabs, on two and four */
        if (s == 4 || s == 12 || (chord_new && s != 0))
            comp_strike(t0, 1.0f + 3.0f * L, v);
        else if ((s == 10 || s == 15) && frand() < busy * 0.4f)
            comp_strike(t0, 1.0f + 1.5f * L, v * 0.7f);
        break;
    case 2:                                     /* offbeats */
        if ((s & 3) == 2)
            comp_strike(t0, 0.8f + 1.6f * L, v * (s == 2 || s == 10 ? 1.0f : 0.85f));
        break;
    default: {                                  /* arpeggio: eighths through the voicing */
        static int ai;
        int notes[8], n;
        if (s & 1)
            break;
        if (chord_new || !comp_prev_n) {
            n = voicing(notes);
            for (ai = 0; ai < n; ai++)
                comp_prev[ai] = notes[ai];
            comp_prev_n = n;
            ai = 0;
        }
        if (comp_prev_n) {
            int k = ai % (2 * comp_prev_n - 2 > 0 ? 2 * comp_prev_n - 2 : 1);
            int idx = k < comp_prev_n ? k : 2 * comp_prev_n - 2 - k;
            ev_note(t0 + (uint32_t)(frand() * 0.004f * EN_SR), T_COMP, comp_prev[idx], v * (s % 4 ? 0.85f : 1.0f),
                    (uint32_t)(spb * (0.8f + 1.6f * L)), 0, (float)(idx - comp_prev_n / 2) * 0.25f * pc01(P_SPREAD));
            ai++;
        }
        break;
    }
    }
}

/* ------------------------------------------------------------------ bass --- */
static int bass_note(int pc)                    /* the root's pitch class in E1 .. D#2 */
{
    return 28 + ((pc - 4) % 12 + 12) % 12;
}
static int next_root_pc(void)
{
    if (auto_mode)
        return (root_pc + deg_note(next_auto_deg())) % 12;
    return en_chord_root;
}

static void bass_step(int s, uint32_t t0)
{
    int r = bass_note(en_chord_root), pat = par[P_BASS_PAT] & 3;
    float busy = pc01(P_BUSY), v = 0.78f + 0.12f * frand(), q = 4.0f * spb;
    if (en_sideb == 2 && s != 0)                /* the breakdown: only the bar's root */
        return;
    switch (pat) {
    case 0:
        if (s == 0 || (chord_new && s != 0))
            ev_note(t0, T_BASS, r, v, (uint32_t)(q * 1.6f), 0, 0.0f);
        else if (s == 8)
            ev_note(t0, T_BASS, r, v * 0.85f, (uint32_t)(q * 1.0f), 0, 0.0f);
        else if (s == 14 && frand() < busy * 0.5f)
            ev_note(t0, T_BASS, r + 12 > 45 ? r : r + 12, v * 0.6f, (uint32_t)(q * 0.4f), 0, 0.0f);
        break;
    case 1: {
        int f = r + 7 > 43 ? r - 5 : r + 7;
        if (s == 0 || (chord_new && s != 0))
            ev_note(t0, T_BASS, r, v, (uint32_t)(q * 1.5f), 0, 0.0f);
        else if (s == 8)
            ev_note(t0, T_BASS, f, v * 0.85f, (uint32_t)(q * 1.2f), 0, 0.0f);
        else if (s == 6 && frand() < busy * 0.45f)
            ev_note(t0, T_BASS, r, v * 0.55f, (uint32_t)(q * 0.35f), 0, 0.0f);
        break;
    }
    case 2: {                                   /* walking: root, a chord tone, another, the approach */
        int b = s >> 2, n;
        if (s & 3)
            break;
        if (b == 0)
            n = r;
        else if (b == 3) {
            int nr = bass_note(next_root_pc());
            n = frand() < 0.5f ? nr + (frand() < 0.5f ? 1 : -1) : scale_step(nr, frand() < 0.5f ? 1 : -1);
        } else {
            int t = chord_tones[b == 1 ? 1 : (chord_n > 2 ? 2 : 1)] % 12;
            n = 28 + ((t - 4) % 12 + 12) % 12;
            if (fm_fabsf((float)(n - r)) > 9.0f)
                n += n > r ? -12 : 12;
        }
        while (n < 26)
            n += 12;
        while (n > 45)
            n -= 12;
        ev_note(t0, T_BASS, n, v * (b == 0 ? 1.0f : 0.85f), (uint32_t)(q * 0.92f), 0, 0.0f);
        break;
    }
    default:                                    /* dub: the root long, an octave pop */
        if (s == 0 || (chord_new && s != 0))
            ev_note(t0, T_BASS, r, v, (uint32_t)(q * 3.0f), 0, 0.0f);
        else if (s == 10 || (s == 6 && frand() < busy * 0.5f))
            ev_note(t0, T_BASS, r + 12, v * 0.6f, (uint32_t)(q * 0.3f), 0, 0.0f);
        break;
    }
}

/* ----------------------------------------------------------------- drums --- */
static const float LANE_VEL[EN_NLANE] = {0.95f, 0.85f, 0.7f, 0.75f, 0.55f, 0.6f, 0.5f, 0.7f};
static void drum_ev(uint32_t t, int lane, float vel)
{
    ev_add(t, EV_DRUM, 0, lane, vel, 0, 0, 0.0f);
}

static void drums_step(int s, uint32_t t0)
{
    int l, fill_bar = (en_bar & 3u) == 3u;
    float busy = pc01(P_BUSY), fill = pc01(P_FILL);
    uint32_t sw = t0;
    if (drum_gate < 0.02f && !fill_now)
        return;
    for (l = 0; l < EN_NLANE; l++)
        if (en_pats[pat_now][l] >> s & 1u) {
            float v = LANE_VEL[l] * (0.88f + 0.2f * frand());
            if (l == LN_HAT || l == LN_SHAKE)
                v *= (s & 3) == 0 ? 1.0f : (s & 1) ? 0.62f : 0.8f;
            drum_ev(sw + (uint32_t)(frand() * 0.003f * EN_SR), l, v);
        }
    if (!(en_pats[pat_now][LN_HAT] >> s & 1u) && (s & 1) && frand() < busy * 0.18f && en_pats[pat_now][LN_HAT])
        drum_ev(sw, LN_HAT, 0.28f);                                  /* ghost hats */
    if ((s == 7 || s == 15) && frand() < busy * 0.2f && en_pats[pat_now][LN_SNARE])
        drum_ev(sw, LN_SNARE, 0.25f);                                /* ghost snares */
    if ((fill_bar && s >= 12 && frand() < fill * 0.8f) || (fxm >> FX_FILL & 1u) || (fill_now && s >= 8) ||
        (drift_fill && s >= 12 && frand() < 0.8f)) {
        int lane = frand() < 0.65f ? LN_SNARE : (frand() < 0.5f ? LN_RIM : LN_PERC);
        drum_ev(sw, lane, 0.45f + 0.35f * (float)(s & 3) * 0.33f);
        if (frand() < 0.4f + 0.4f * fill)                           /* a ratchet */
            drum_ev(sw + (uint32_t)(spb * 0.5f), lane, 0.35f);
    }
}

/* ---------------------------------------------------------- answer, ghost --- */
#define NPH 16
static struct {
    uint8_t note[NPH];
    uint32_t at[NPH];                           /* the sixteenth it fell on (step_count) */
    int n;
    uint32_t last;
    uint8_t answered;
} ph;
static int ans_last = 72;
static uint32_t ans_until;                      /* the answer's phrase lasts until this sixteenth */

static void phrase_note(int note, uint32_t at)
{
    if (ph.n == 0 || at > ph.last + 4u || ph.n >= NPH) {
        ph.n = 0;
        ph.answered = 0;
    }
    ph.note[ph.n] = (uint8_t)note;
    ph.at[ph.n] = at;
    ph.n++;
    ph.last = at;
}

static int ans_centre(void)
{
    switch (EN_ANS_INST[par[P_ANS_INST]]) {
    case I_TENORSAX: return 60;
    case I_MUTETPT: case I_VIBES: case I_HARMONICA: return 70;
    default: return 76;
    }
}

static void answer(uint32_t s0)                 /* a phrase back, from sixteenth s0 */
{
    int L = 2 + irand(1 + (int)(pc01(P_CHATTY) * 4.0f)), k, n, c = ans_centre();
    int imitate = ph.n >= 2 && frand() < pc01(P_ECHO_PH), inv = frand() < 0.3f;
    uint32_t at = s0, step_t;
    float v;
    if (imitate) {                              /* echo: the player's rhythm and contour, from a chord tone */
        L = ph.n > 6 ? 6 : ph.n;
        n = nearest_chord_tone(c - 6 + ((ph.note[0] - c + 6) % 12 + 12) % 12);
    } else {
        n = nearest_chord_tone(ans_last + irand(5) - 2);
    }
    while (n < c - 9)
        n += 12;
    while (n > c + 9)
        n -= 12;
    for (k = 0; k < L; k++) {
        int last = k == L - 1;
        uint32_t len;
        if (k) {
            if (imitate) {
                int iv = ph.note[k] - ph.note[k - 1];
                at += ph.at[k] - ph.at[k - 1];
                n = snap_scale(n + (inv ? -iv : iv));
            } else {
                static const uint8_t GAP[6] = {1, 2, 2, 2, 3, 4};
                at += GAP[irand(6)];
                n = frand() < 0.7f ? scale_step(n, frand() < 0.5f ? 1 : -1) : scale_step(scale_step(n, 1), 1);
            }
            if (n > c + 12)
                n -= 12;
            if (n < c - 12)
                n += 12;
        }
        if (last || (at & 3u) == 0u)
            n = nearest_chord_tone(n);
        len = last ? (uint32_t)(spb * (4.0f + 4.0f * frand())) : (uint32_t)(spb * 1.7f);
        step_t = (uint32_t)((float)(at - step_count) * spb + swing_delay((int)(at & 1u)));
        v = 0.55f + 0.2f * frand() + (last ? 0.05f : 0.0f);
        ev_note(now + step_t + (uint32_t)(frand() * 0.01f * EN_SR), T_ANS, n, v, len, 2, 0.25f);
        ans_last = n;
    }
    ans_until = at + 4u;
    en_answers++;
}

static int ghost_last = 67;
static uint8_t ghost_rest;
static struct {                                 /* the ghost's last phrase, for Repeat: its rhythm and its contour */
    uint8_t at[32];
    int8_t iv[32];
    int n, bars;
} motif;

static void ghost_phrase(void)                  /* Daydream: a bar or two of melody, starting now (a bar line) */
{
    static const uint8_t CELL[10] = {0x0, 0x1, 0x5, 0x4, 0x9, 0x7, 0xA, 0xD, 0xB, 0xF};   /* a beat's sixteenths */
    static const uint8_t W_LO[10] = {5, 6, 3, 2, 2, 0, 1, 1, 0, 0}, W_HI[10] = {1, 2, 4, 2, 3, 3, 3, 4, 3, 4};
    float dens = pc01(P_DD_DENS), mood = pc01(P_DD_MOOD), lazy = pc01(P_LAZY), fill = pc01(P_DD_FILL);
    float legato = 0.3f + 0.7f * pc01(P_DD_LEGATO), dyn = pc01(P_DD_DYN);
    int bars, b, beat, k, n = ghost_last, cen = 64 + 12 * par[P_LEAD_OCT];
    int width = 5 + (int)(10.0f * pc01(P_DD_RANGE)), placed = 0, rest_last, again;
    uint32_t base = step_count;
    uint32_t ats[32];
    int notes[32];
    again = motif.n >= 2 && frand() < pc01(P_DD_REPEAT);
    if (again) {                                /* the motif again, from a chord tone near where we are */
        bars = motif.bars;
        n = nearest_chord_tone(n + irand(5) - 2);
        for (k = 0; k < motif.n; k++) {
            if (k) {
                int iv = motif.iv[k];
                if (frand() < 0.2f)             /* a little varied */
                    iv += irand(3) - 1;
                n = snap_scale(n + iv);
                if (n > cen + width + 3)
                    n -= 12;
                if (n < cen - width - 3)
                    n += 12;
            }
            if ((motif.at[k] & 7u) == 0u)
                n = nearest_chord_tone(n);
            notes[placed] = n;
            ats[placed] = base + motif.at[k];
            placed++;
        }
    } else {
        bars = fill > 0.6f ? 2 : frand() < 0.5f ? 1 : 2;
        rest_last = frand() < 1.0f - 0.9f * fill;   /* the last beat rests (the answer's room), less as Fill rises */
        for (b = 0; b < bars * 4; b++) {
            int tot = 0, w[10], c;
            for (c = 0; c < 10; c++) {
                w[c] = (int)((float)W_LO[c] * (1.0f - dens) + (float)W_HI[c] * dens + 0.5f);
                if (c == 0)
                    w[c] = (int)((float)w[c] * (1.0f - 0.85f * fill) + 0.5f);
                tot += w[c];
            }
            k = irand(tot > 0 ? tot : 1);
            for (c = 0; c < 9 && k >= w[c]; c++)
                k -= w[c];
            if (b == bars * 4 - 1 && placed && rest_last)
                break;
            for (beat = 0; beat < 4; beat++)
                if (CELL[c] >> beat & 1u && placed < 32) {
                    int leap = frand() < mood * 0.6f, dir = frand() < 0.5f ? 1 : -1;
                    if (n > cen + width)
                        dir = -1;
                    if (n < cen - width)
                        dir = 1;
                    n = scale_step(n, dir);
                    if (leap)
                        n = scale_step(scale_step(n, dir), dir);
                    if (beat == 0 && (b & 1) == 0)
                        n = nearest_chord_tone(n);
                    notes[placed] = n;
                    ats[placed] = base + (uint32_t)(b * 4 + beat);
                    placed++;
                }
        }
        if (!placed)
            return;
        notes[placed - 1] = nearest_chord_tone(notes[placed - 1]);
        motif.n = placed;                       /* remembered, for Repeat */
        motif.bars = bars;
        for (k = 0; k < placed; k++) {
            motif.at[k] = (uint8_t)(ats[k] - base);
            motif.iv[k] = (int8_t)(k ? notes[k] - notes[k - 1] : 0);
        }
    }
    for (k = 0; k < placed; k++) {
        uint32_t len, t;
        float v;
        len = k + 1 < placed ? (uint32_t)((float)(ats[k + 1] - ats[k]) * spb * legato) : (uint32_t)(spb * (2.0f + 4.0f * legato));
        if (len < (uint32_t)(spb * 0.3f))
            len = (uint32_t)(spb * 0.3f);
        t = (uint32_t)((float)(ats[k] - step_count) * spb + swing_delay((int)(ats[k] & 1u)) + frand() * lazy * 0.03f * EN_SR);
        v = 0.62f + (frand() - 0.5f) * (0.15f + 0.5f * dyn) + ((ats[k] & 3u) == 0u ? 0.06f + 0.1f * dyn : 0.0f);
        ev_note(now + t, T_LEAD, notes[k], v, len, 1, -0.1f);
        phrase_note(notes[k], ats[k]);
        en_dream_notes++;
    }
    ghost_last = notes[placed - 1];
    {   /* then a rest: none at full Fill, a bar or two when sparse */
        float pr = (1.0f - fill) * 0.85f;
        int rest = frand() < pr ? 1 + (frand() < (1.0f - fill) * 0.6f) : 0;
        ghost_rest = (uint8_t)(bars - 1 + rest);
    }
}

/* ----------------------------------------------------------------- drift --- */
/* every Drift bars the music changes: the drums to another pattern (the bar before it, a fill announces
 * it), the progression to its other one. SEL + SEQ asks for one now (on the next bar). */
static int drift_len(void) { static const int16_t L[6] = {0, 4, 8, 16, 32, 64}; return L[(unsigned)par[P_DRIFT] < 6u ? par[P_DRIFT] : 0]; }

static void mutate(void)
{
    int pool = par[P_DRIFT_POOL] < 2 ? 2 : par[P_DRIFT_POOL] > EN_NPAT ? EN_NPAT : par[P_DRIFT_POOL];
    if (par[P_DRIFT_DRUMS] == 1 || (mutate_now && par[P_DRIFT_DRUMS] == 0)) {
        pat_next = (pat_now + 1) % pool;          /* next (a manual change with Drums on Keep: next too) */
    } else if (par[P_DRIFT_DRUMS] == 2) {
        int p = irand(pool - 1);
        pat_next = p >= pat_now ? p + 1 : p;      /* random: any but this one */
        if (pat_next >= pool)
            pat_next = 0;
    }
    pat_now = pat_next;
    if (par[P_DRIFT_CHORDS]) {
        prog_i ^= 1;
        prog_k = 0;
    }
}

static void drift_bar(void)                     /* at each bar line, after the bar counts */
{
    int len = drift_len();
    if (mutate_now) {
        mutate();
        mutate_now = 0;
        drift_fill = 0;
        drift_bars = 0;
    } else if (len) {
        drift_bars++;
        if ((int)drift_bars >= len) {
            mutate();
            drift_bars = 0;
            drift_fill = 0;
        } else if ((int)drift_bars == len - 1 && par[P_DRIFT_DRUMS]) {
            drift_fill = 1;                     /* the bar before: a fill */
        }
    } else {
        drift_bars = 0;
    }
    en_drift_left = len ? (int16_t)(len - (int)drift_bars) : -1;
    en_pattern = (uint8_t)pat_now;
}

/* --------------------------------------------------------------- the step --- */
static void bar_start(void)
{
    int dream_ok;
    en_bar++;
    pat_now = pat_next;
    drift_bar();
    if (en_sideb == 1)
        en_sideb = 2;
    else if (en_sideb == 3) {
        en_sideb = 0;
        fill_now = 0;
    }
    idle_bars += 1.0f;
    key_bars += 1.0f;
    {   /* Daydream: after Wait bars without a key (or always), the ghost has the lead */
        static const float WAIT[4] = {4.0f, 8.0f, 16.0f, 0.0f};
        int any_held = lead_held[0] | lead_held[1] | lead_held[2] | lead_held[3];
        dream_ok = par[P_DD_ON] && !any_held && idle_bars >= WAIT[par[P_DD_WAIT] & 3];
    }
    en_dreaming = (uint8_t)dream_ok;
    if (dream_ok) {
        if (ghost_rest)
            ghost_rest--;
        else
            ghost_phrase();
    }
    lead_active = !en_dreaming && key_bars < 2.0f;
    en_idle_bars = idle_bars;
}

static void on_step(void)
{
    int s = (int)(step_count & 15u);
    uint32_t t0;
    en_step = (uint8_t)s;
    if (s == 0)
        bar_start();
    t0 = T(swing_delay(s));
    chord_new = 0;
    if ((s & 3) == 0 && !en_held) {             /* the harmony moves on the beat */
        if (lead_active) {
            int nh = 0, i, d;
            for (i = 0; i < 128; i++)
                nh += (int)(lead_held[i >> 5] >> (i & 31) & 1u);
            d = follow();
            if (d >= 0 && (s == 0 || s == 8 || nh >= 2)) {
                chord_set(d);
                chord_new = 1;
            }
            auto_mode = 0;
        } else if (s == 0) {                    /* nobody playing: the progression, a bar a chord */
            if (!auto_mode) {
                auto_mode = 1;
                prog_k = 0;
                while (prog_k < 4 && PROG[scale_i][prog_i][prog_k] != chord_deg)
                    prog_k++;
                prog_k = prog_k < 4 ? prog_k + 1 : 0;
            } else {
                if (next_auto_deg() != chord_deg)
                    chord_new = 1;
                chord_set(next_auto_deg());
                prog_k++;
                if (prog_k >= 4) {
                    prog_k = 0;
                    if (frand() < 0.3f)
                        prog_i ^= 1;
                }
            }
        }
    }
    {
        if (auto_mode && s == 14 && frand() < pc01(P_PUSH) * 0.6f && (par[P_COMP_PAT] & 3) <= 1 && !en_held) {
            int keep = chord_deg;                /* push: the next chord an eighth early */
            chord_build(next_auto_deg(), chord_tones, &chord_n);
            comp_strike(t0, 10.0f * (0.25f + 0.75f * pc01(P_COMP_LEN)), 0.5f);
            chord_build(keep, chord_tones, &chord_n);
        } else {
            comp_step(s, t0);
        }
    }
    en_auto = auto_mode;
    bass_step(s, t0);
    drums_step(s, t0);
    if (par[P_ANS_ON] && ph.n >= 2 && !ph.answered && step_count >= ph.last + 3u && step_count > ans_until &&
        !(lead_held[0] | lead_held[1] | lead_held[2] | lead_held[3]) && (s & 1) == 0) {
        ph.answered = 1;
        if (frand() < 0.45f + 0.55f * pc01(P_CHATTY))
            answer(step_count + 1u);
    }
    step_count++;
}

/* the lead, from the keys or MIDI in */
/* what each key played: one note, or a chord (Keys: Chords); notes shared by two held keys are counted, so
 * letting go of one leaves the other's sounding */
#define KMAX 5
static uint8_t key_notes[128][KMAX], key_cnt[128], note_ref[128];

static int parent_snap(int n, int *deg)         /* n to the scale's seven-note parent; its degree there */
{
    int k, i;
    for (k = 0; k < 6; k++) {
        int c[2] = {n - k, n + k};
        for (i = 0; i < 2; i++) {
            int rel = ((c[i] - root_pc) % 12 + 12) % 12, d;
            for (d = 0; d < 7; d++)
                if (PARENT[scale_i][d] == rel) {
                    *deg = d;
                    return c[i];
                }
        }
    }
    *deg = 0;
    return n;
}

static int key_chord(int n, int *out)           /* Chords: the scale's chord on this key, as Jazz has it */
{
    int d, k, m = jazz_n(), cnt = 0, root = parent_snap(n, &d), t[6];
    for (k = 0; k < m; k++)
        t[k] = root + deg_note(d + 2 * k) - deg_note(d);
    if (m >= 5) {                               /* ninths and up: the fifth left out, as the comp does */
        for (k = 2; k < m - 1; k++)
            t[k] = t[k + 1];
        m--;
    }
    for (k = 0; k < m && cnt < KMAX; k++)
        out[cnt++] = t[k];
    return cnt;
}

static void lead_note_on(int note, float v)
{
    if (note_ref[note]++ == 0) {
        lead_held[note >> 5] |= 1u << (note & 31);
    } else {
        voice_off(T_LEAD, note, 0);             /* struck again: the old one makes way */
    }
    voice_on(T_LEAD, note, v, 0, -0.1f);
}
static void lead_note_off(int note)
{
    if (!note_ref[note])
        return;
    if (--note_ref[note] == 0) {
        lead_held[note >> 5] &= ~(1u << (note & 31));
        voice_off(T_LEAD, note, 0);
    }
}

static void lead_key(int n, int on, int vel)
{
    int notes[KMAX], cnt, k;
    n &= 127;
    if (!on) {
        for (k = 0; k < key_cnt[n]; k++)
            lead_note_off(key_notes[n][k]);
        key_cnt[n] = 0;
        return;
    }
    if (en_dreaming || ghost_rest) {            /* a key takes the lead back */
        ev_cancel(T_LEAD, 1);
        track_release(T_LEAD, 1);
        en_dreaming = 0;
        ghost_rest = 0;
    }
    if (step_count < ans_until) {               /* you cut the answer short */
        ev_cancel(T_ANS, 2);
        track_release(T_ANS, 2);
        ans_until = 0;
    }
    idle_bars = 0.0f;
    key_bars = 0.0f;
    lead_active = 1;
    last_lead_t = now;
    for (k = 0; k < key_cnt[n]; k++)            /* the same key again (MIDI): the old one off first */
        lead_note_off(key_notes[n][k]);
    n += 12 * par[P_LEAD_OCT];
    if (par[P_SNAP] == 2) {
        cnt = key_chord(n, notes);
    } else {
        notes[0] = par[P_SNAP] ? snap_scale(n) : n;
        cnt = 1;
    }
    {
        float lz = pc01(P_LAZY), v = (float)vel * (1.0f / 127.0f);
        v = v * (1.0f - 0.25f * lz) + lz * 0.25f * frand();
        key_cnt[n - 12 * par[P_LEAD_OCT]] = (uint8_t)cnt;
        for (k = 0; k < cnt; k++) {
            int note = notes[k] < 12 ? 12 : notes[k] > 120 ? 120 : notes[k];
            key_notes[n - 12 * par[P_LEAD_OCT]][k] = (uint8_t)note;
            lead_note_on(note, cnt > 1 ? v * (k == cnt - 1 ? 0.95f : 0.82f) : v);
        }
        lead_remember(notes[0], ((step_count & 3u) == 0u));
        phrase_note(notes[cnt > 1 ? cnt - 1 : 0], step_count);   /* a chord's top note carries the melody */
        en_notes++;
    }
}

/* ================================================================== DUST === */
/* the record or the tape under it all. Each character weighs the layers: crackle, pops, surface noise,
 * tape hiss, radio static, the grain's tone */
static const struct {
    float crackle, pops, surface, tape, stat, grain_lp, grain_amt;
} DCH[EN_NDUST] = {
    {1.0f, 1.0f, 1.0f, 0.15f, 0.0f, 3200.0f, 1.0f},     /* VINYL */
    {0.06f, 0.04f, 0.15f, 1.0f, 0.0f, 2400.0f, 1.0f},   /* CASSETTE */
    {0.04f, 0.02f, 0.12f, 0.55f, 0.0f, 4200.0f, 1.4f},  /* REEL */
    {0.30f, 0.0f, 0.0f, 0.0f, 1.0f, 1800.0f, 0.8f},     /* RADIO */
};
static float d_lvl, d_age, d_surf, d_hiss, d_grain, d_smear, d_duck, d_hum;
static int d_char;
static struct {
    float crk, crk_sign, pop, pop_lp, s_lp1, s_lp2, t_hp, t_lp, hum_ph, rumble, st_bp, st_lp, st_am, wh_ph, wh_f;
    float ro_t, ro_left, gate;
} du;
#define GRN 11025                               /* half a second of the mix at 22,050 */
static int16_t grn_buf[GRN] EN_POOL;
static uint32_t grn_w;
static float grn_acc;
#define NGRAIN 4
static struct {
    float pos, rate, len, t, amp, pan;
} gr[NGRAIN];
static float grn_lp_l, grn_lp_r, grain_wait;
static float dust_l[SUBN], dust_r[SUBN];

static void needle_drop(void)
{
    if (d_char == DUST_VINYL) {
        du.pop = 1.0f;
        du.crk = 0.6f;
    }
    du.ro_left = 0.0f;
}

static void grain_spawn(int i)
{
    static const float RATES[6] = {1.0f, 1.0f, 0.5f, 0.5f, 2.0f, 1.0f};
    float len = (0.04f + 0.22f * d_smear) * EN_SR, back = (0.05f + 0.4f * frand()) * 22050.0f;
    gr[i].rate = RATES[irand(6)] * (1.0f + 0.004f * (frand() - 0.5f));
    gr[i].len = len;
    gr[i].t = 0.0f;
    gr[i].pos = (float)grn_w - back - len * 0.5f * gr[i].rate;
    while (gr[i].pos < 0.0f)
        gr[i].pos += (float)GRN;
    gr[i].amp = 0.6f + 0.4f * frand();
    gr[i].pan = frand() * 1.2f - 0.6f;
}

static void render_dust(uint32_t n, const float *mono)
{
    uint32_t j;
    int i;
    const float cr = DCH[d_char].crackle, sf = DCH[d_char].surface, tp = DCH[d_char].tape, st = DCH[d_char].stat;
    float on = (en_playing || du.ro_left > 0.0f) ? 1.0f : 0.0f;
    float pcrk = (6.0f + 90.0f * d_surf * (0.35f + d_age)) * cr * (1.0f / EN_SR);
    float ppop = (0.15f + 2.4f * d_age * d_age) * DCH[d_char].pops * d_surf * (1.0f / EN_SR);
    float glp = onepole(DCH[d_char].grain_lp * (1.0f - 0.45f * d_age)), g_amt = d_grain * DCH[d_char].grain_amt;
    float duck = 1.0f - d_duck * fm_minf(kick_env, 1.0f) * 0.8f;
    float lvl0 = d_lvl * (en_sideb == 2 ? 1.8f : 1.0f) * (en_dreaming ? 1.0f + 0.35f * fm_sin_turns((float)(now % (12u * 44100u)) * (1.0f / (12.0f * EN_SR))) : 1.0f);
    int freeze = fxm >> FX_FREEZE & 1u;
    if (freeze)                                 /* frozen: the haze held, and brought up to be heard */
        g_amt = fm_maxf(g_amt, 0.25f) * 2.2f;
    du.gate += (on - du.gate) * fm_minf((float)n / (on > 0.5f ? 0.3f * EN_SR : 1.5f * EN_SR), 1.0f);
    for (j = 0; j < n; j++) {
        float nz = wnoise(&nz2), nz2_ = wnoise(&nz3), xl = 0.0f, xr = 0.0f, x;
        /* the grain's buffer: the mix, at half rate */
        if (!freeze) {
            grn_acc += mono[j];
            if (j & 1u) {
                float v = fm_clampf(grn_acc * 0.5f, -1.0f, 1.0f);
                grn_buf[grn_w] = (int16_t)(v * 32767.0f);
                grn_w = grn_w + 1u >= GRN ? 0u : grn_w + 1u;
                grn_acc = 0.0f;
            }
        }
        if (du.gate < 0.001f && g_amt <= 0.0f)
            continue;
        /* crackle and pops */
        if (frand() < pcrk) {
            float u = frand();
            du.crk = 0.03f + 0.25f * u * u * u;
            du.crk_sign = frand() < 0.5f ? -1.0f : 1.0f;
        }
        if (frand() < ppop)
            du.pop = 0.35f + 0.6f * frand();
        x = du.crk * du.crk_sign * (0.6f + 0.4f * nz);
        du.crk *= 0.80f;
        du.pop_lp += 0.12f * (nz * du.pop - du.pop_lp);
        x += du.pop_lp * 1.4f;
        du.pop *= 0.9975f;
        xl += x * (0.8f + 0.2f * du.crk_sign);
        xr += x * (0.8f - 0.2f * du.crk_sign);
        /* surface: a soft band of noise */
        du.s_lp1 += 0.35f * (nz - du.s_lp1);
        du.s_lp2 += 0.02f * (du.s_lp1 - du.s_lp2);
        x = (du.s_lp1 - du.s_lp2) * 0.05f * d_surf * sf;
        xl += x;
        xr += x * 0.9f;
        /* tape hiss: bright, the two sides apart */
        x = (nz2_ - du.t_hp) * 0.045f * d_hiss * tp;
        du.t_hp = nz2_ * 0.6f + du.t_hp * 0.4f;
        xl += x;
        xr += (nz - du.t_lp) * 0.045f * d_hiss * tp;
        du.t_lp += 0.6f * (nz - du.t_lp);
        /* radio: static riding a slow swell, and a faint tuning whistle */
        if (st > 0.0f) {
            du.st_bp += 0.25f * (nz - du.st_bp);
            du.st_lp += 0.08f * (du.st_bp - du.st_lp);
            du.st_am += 0.00002f * ((nz2_ > 0.0f ? 1.0f : 0.2f) - du.st_am);
            x = (du.st_bp - du.st_lp) * 0.07f * st * d_hiss * (0.4f + du.st_am);
            du.wh_ph += (1800.0f + 600.0f * fm_sin_turns((float)(now % 441000u) * (1.0f / 441000.0f))) * (1.0f / EN_SR);
            if (du.wh_ph >= 1.0f)
                du.wh_ph -= 1.0f;
            x += 0.004f * st * d_hiss * sine(du.wh_ph);
            xl += x;
            xr += x;
        }
        /* hum and rumble */
        if (d_hum > 0.0f) {
            du.hum_ph += 60.0f * (1.0f / EN_SR);
            if (du.hum_ph >= 1.0f)
                du.hum_ph -= 1.0f;
            du.rumble += 0.004f * (nz - du.rumble);
            x = d_hum * (0.02f * sine(du.hum_ph) + 0.008f * sine(du.hum_ph * 2.0f) + 0.004f * sine(du.hum_ph * 3.0f) +
                         0.4f * du.rumble * (d_char == DUST_VINYL || d_char == DUST_REEL));
            xl += x;
            xr += x;
        }
        /* the run-out groove: a tick each turn, 33 1/3 */
        if (du.ro_left > 0.0f) {
            du.ro_t += 1.0f;
            if (du.ro_t >= 1.8f * EN_SR) {
                du.ro_t = 0.0f;
                du.pop = 0.55f;
            }
            du.ro_left -= 1.0f;
        }
        xl *= du.gate;
        xr *= du.gate;
        /* the grain: the music, smeared back in */
        if (g_amt > 0.0f && (en_playing || du.gate > 0.01f)) {
            float gl = 0.0f, grr = 0.0f;
            for (i = 0; i < NGRAIN; i++) {
                if (gr[i].len <= 0.0f)
                    continue;
                if (gr[i].t >= gr[i].len) {
                    gr[i].len = 0.0f;
                    continue;
                }
                {
                    float w = sine(gr[i].t / gr[i].len * 0.5f), p = gr[i].pos, f;
                    int a = (int)p, b;
                    f = p - (float)a;
                    a %= GRN;
                    b = a + 1 >= GRN ? 0 : a + 1;
                    x = ((float)grn_buf[a] + ((float)grn_buf[b] - (float)grn_buf[a]) * f) * (1.0f / 32768.0f);
                    x *= w * w * gr[i].amp;
                    gl += x * (1.0f - gr[i].pan);
                    grr += x * (1.0f + gr[i].pan);
                    gr[i].pos += gr[i].rate * 0.5f;
                    if (gr[i].pos >= (float)GRN)
                        gr[i].pos -= (float)GRN;
                    gr[i].t += 1.0f;
                }
            }
            grn_lp_l += glp * (gl - grn_lp_l);
            grn_lp_r += glp * (grr - grn_lp_r);
            xl += grn_lp_l * g_amt * 0.5f;
            xr += grn_lp_r * g_amt * 0.5f;
        }
        dust_l[j] = xl * lvl0 * duck;
        dust_r[j] = xr * lvl0 * duck;
    }
    if (du.gate < 0.001f && g_amt <= 0.0f)
        for (j = 0; j < n; j++)
            dust_l[j] = dust_r[j] = 0.0f;
    /* new grains, as many as the smear asks */
    grain_wait -= (float)n;
    if (g_amt > 0.0f && grain_wait <= 0.0f) {
        for (i = 0; i < NGRAIN; i++)
            if (gr[i].len <= 0.0f) {
                grain_spawn(i);
                break;
            }
        grain_wait = (0.02f + 0.18f * (1.0f - d_smear)) * EN_SR * (0.5f + frand());
    }
    du.crk = fm_flush(du.crk);
    du.pop_lp = fm_flush(du.pop_lp);
    grn_lp_l = fm_flush(grn_lp_l);
    grn_lp_r = fm_flush(grn_lp_r);
}

/* ================================================================= space === */
/* Dattorro's plate (AMB-1's) */
#define DS(n) (((n) * 14818 + 5000) / 10000)
#define PRE_N 882
#define EXC DS(16)
#define EXC_MAX (2 * EXC)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC_MAX + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC_MAX + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC_MAX + 2))
static int16_t pl_buf[DL_TOTAL] EN_POOL;     /* 16-bit lines (x 8192: headroom to 4), half the RAM of floats */
#define PL_Q 8192.0f
typedef struct {
    int16_t *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.72f, lfo_s, lfo_c = 1.0f;
static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return (float)d->b[j < 0 ? j + d->n : j] * (1.0f / PL_Q);
}
static inline void push(dl_t *d, float x)
{
    x *= PL_Q;
    x = x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x;
    d->b[d->i] = (int16_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
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
    pl_bw += 0.55f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.00012f * lfo_c, c = lfo_c - 0.00012f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.35f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, 0.5f);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.35f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, 0.5f);
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
}

/* the echo: a dotted eighth, at half rate (16-bit), dark and fading */
#define ECHO_N 17000
static int16_t echo_buf[ECHO_N] EN_POOL;
static uint32_t echo_w;
static float echo_in, echo_out, echo_lp, echo_hp, echo_hold;

/* the tape: wow and flutter (a modulated delay), saturation */
#define WOW_N 1024
static float wow_buf[2][WOW_N] EN_POOL;
static uint32_t wow_w;
static float wow_ph, flt_ph, wow_rnd, wow_rnd_t;

/* the capture: the output, kept for stutter, tape stop and backspin (16-bit, half rate, a second) */
#define CAP_N 22050
static int16_t cap_buf[CAP_N][2] EN_POOL;
static uint32_t cap_w;
static float cap_acc_l, cap_acc_r;
enum { CM_LIVE, CM_STUT, CM_STOP, CM_SPIN, CM_SKIP };
static int cap_mode;
static float cap_rp, cap_rate, cap_xf, cap_len, cap_start, cap_t, skip_clk;

static float mel_l[2], mel_r[2], muf_l, muf_r, muf_l2, muf_r2, muf_c = 1.0f, muf_hz = 20000.0f, rad_hp_l, rad_hp_r, rad_lp_l, rad_lp_r, drop_g = 1.0f, drop_t;
static float crush_l, crush_r, crush_c;
static float trk_gate[EN_NTRACK] = {1, 1, 1, 1, 1, 1};
static float tape_amt, mellow_hz, room_amt, echo_amt, master_lp = 1.0f;
static float mono_mix[SUBN], room_in[SUBN], echo_send[SUBN];

static float cap_read(float p, int ch)
{
    int a, b;
    float f;
    while (p < 0.0f)
        p += (float)CAP_N;
    a = (int)p;
    f = p - (float)a;
    a %= CAP_N;
    b = a + 1 >= CAP_N ? 0 : a + 1;
    return ((float)cap_buf[a][ch] + ((float)cap_buf[b][ch] - (float)cap_buf[a][ch]) * f) * (1.0f / 32768.0f);
}

static void cap_enter(int mode, float len)
{
    if (cap_mode == mode)
        return;
    cap_mode = mode;
    cap_t = 0.0f;
    cap_len = len * 0.5f;
    cap_start = (float)cap_w - cap_len;
    cap_rp = mode == CM_STUT || mode == CM_SKIP ? cap_start : (float)cap_w - 1.0f;   /* stop, spin: from now */
    cap_rate = 1.0f;
}

static void master(uint32_t n, float gain, int32_t *out)
{
    static const float SEND_ROOM[EN_NTRACK] = {0.45f, 0.55f, 0.05f, 0.6f, 0.18f, 0.08f};
    static const float SEND_ECHO[EN_NTRACK] = {0.5f, 0.12f, 0.0f, 0.6f, 0.0f, 0.0f};
    uint32_t j;
    int t, solo = (int)(fxm >> FX_SOLO0 & 63u), mute = (int)(fxm >> FX_MUTE0 & 63u);
    float tg[EN_NTRACK], rs = 0.0f, es = 0.0f, troom, techo;
    float wow_d = tape_amt * (d_char == DUST_CASSETTE ? 1.4f : d_char == DUST_REEL ? 0.8f : 1.0f) * ((fxm >> FX_WOBBLE & 1u) ? 3.5f : 1.0f);
    float flt_d = tape_amt * (d_char == DUST_CASSETTE ? 2.2f : 1.0f);
    float wow_hz = d_char == DUST_VINYL ? 0.555f : d_char == DUST_REEL ? 0.35f : 0.8f;
    float drv = 1.0f + 2.2f * tape_amt, mk = 1.0f / (0.6f + 0.4f * drv);
    float mc = onepole(mellow_hz * master_lp), half = 0.5f;
    troom = (fxm >> FX_ROOM & 1u) ? 1.5f : room_amt;
    techo = (fxm >> FX_ECHO & 1u) ? 1.2f : echo_amt;
    for (t = 0; t < EN_NTRACK; t++) {           /* mutes and solos, eased */
        float want = (solo ? (solo >> t & 1) : !(mute >> t & 1)) ? 1.0f : 0.0f;
        if (t == T_DRUMS)
            want *= drum_gate;
        trk_gate[t] += (want - trk_gate[t]) * fm_minf((float)n / (0.01f * EN_SR), 1.0f);
        tg[t] = trk_gate[t];
    }
    for (j = 0; j < n; j++) {
        float l = 0.0f, r = 0.0f, rl, rr, x;
        for (t = 0; t < NTRK; t++) {
            l += bus_l[t][j] * tg[t];
            r += bus_r[t][j] * tg[t];
            rs = (bus_l[t][j] + bus_r[t][j]) * tg[t];
            room_in[j] += rs * SEND_ROOM[t];
            echo_send[j] += rs * SEND_ECHO[t];
        }
        l += drum_l[j] * tg[T_DRUMS];
        r += drum_r[j] * tg[T_DRUMS];
        room_in[j] += (drum_l[j] + drum_r[j]) * tg[T_DRUMS] * SEND_ROOM[T_DRUMS];
        l += dust_l[j] * tg[T_DUST];
        r += dust_r[j] * tg[T_DUST];
        /* the echo, every other sample */
        if (j & 1u) {
            float in = (echo_in + echo_send[j]) * 0.5f, d = 3.0f * spb * half, rp = (float)echo_w - d, f, e;
            int a, b;
            while (rp < 0.0f)
                rp += (float)ECHO_N;
            a = (int)rp;
            f = rp - (float)a;
            a %= ECHO_N;
            b = a + 1 >= ECHO_N ? 0 : a + 1;
            e = ((float)echo_buf[a] + ((float)echo_buf[b] - (float)echo_buf[a]) * f) * (1.0f / 16384.0f);
            echo_lp += 0.3f * (e - echo_lp);
            echo_hp += 0.02f * (echo_lp - echo_hp);
            x = in * techo + (echo_lp - echo_hp) * 0.45f;
            echo_buf[echo_w] = (int16_t)(fm_clampf(x, -1.9f, 1.9f) * 16384.0f);
            echo_w = echo_w + 1u >= ECHO_N ? 0u : echo_w + 1u;
            echo_hold = echo_lp - echo_hp;
        } else {
            echo_in = echo_send[j];
        }
        echo_out += 0.5f * (echo_hold - echo_out);
        l += echo_out * 0.8f;
        r += echo_out * 0.7f;
        room_in[j] += echo_out * 0.2f;
        reverb(room_in[j] * troom * 0.5f, &rl, &rr);
        l += rl * 0.9f;
        r += rr * 0.9f;
        /* the tape: wow and flutter, then its saturation */
        wow_buf[0][wow_w] = l;
        wow_buf[1][wow_w] = r;
        {
            float d = 220.0f + wow_d * (60.0f * fm_sin_turns(wow_ph) + 25.0f * wow_rnd) + flt_d * 5.0f * fm_sin_turns(flt_ph);
            float rp = (float)wow_w - d, f;
            int a, b;
            if (rp < 0.0f)
                rp += (float)WOW_N;
            a = (int)rp;
            f = rp - (float)a;
            b = a + 1 >= WOW_N ? 0 : a + 1;
            l = wow_buf[0][a] + (wow_buf[0][b] - wow_buf[0][a]) * f;
            r = wow_buf[1][a] + (wow_buf[1][b] - wow_buf[1][a]) * f;
            wow_w = wow_w + 1u >= WOW_N ? 0u : wow_w + 1u;
            wow_ph += wow_hz * (1.0f / EN_SR) * (1.0f + 0.15f * wow_rnd);
            if (wow_ph >= 1.0f)
                wow_ph -= 1.0f;
            flt_ph += 6.3f * (1.0f / EN_SR);
            if (flt_ph >= 1.0f)
                flt_ph -= 1.0f;
        }
        if (tape_amt > 0.01f) {
            l = fm_tanhf(l * drv) * mk;
            r = fm_tanhf(r * drv) * mk;
        }
        /* Mellow: a gentle two-pole low-pass (and Age, and Side B) */
        mel_l[0] += mc * (l - mel_l[0]);
        mel_l[1] += mc * (mel_l[0] - mel_l[1]);
        mel_r[0] += mc * (r - mel_r[0]);
        mel_r[1] += mc * (mel_r[0] - mel_r[1]);
        l = mel_l[1];
        r = mel_r[1];
        /* the held effects: muffle, radio, crush, dropout */
        if (muf_hz < 17000.0f) {                /* muffle: two poles closing in */
            muf_l += muf_c * (l - muf_l);
            muf_r += muf_c * (r - muf_r);
            muf_l2 += muf_c * (muf_l - muf_l2);
            muf_r2 += muf_c * (muf_r - muf_r2);
            l = muf_l2;
            r = muf_r2;
        } else {
            muf_l = muf_l2 = l;
            muf_r = muf_r2 = r;
        }
        if (fxm >> FX_RADIO & 1u) {
            rad_lp_l += 0.32f * (l - rad_lp_l);
            rad_lp_r += 0.32f * (r - rad_lp_r);
            rad_hp_l += 0.07f * (rad_lp_l - rad_hp_l);
            rad_hp_r += 0.07f * (rad_lp_r - rad_hp_r);
            x = (rad_lp_l - rad_hp_l + rad_lp_r - rad_hp_r) * 1.4f;
            l = r = fm_tanhf(x * 1.5f) * 0.8f;
        }
        if (fxm >> FX_CRUSH & 1u) {
            crush_c += 1.0f;
            if (crush_c >= 6.0f) {
                crush_c = 0.0f;
                crush_l = (float)(int)(l * 16.0f) * (1.0f / 16.0f);
                crush_r = (float)(int)(r * 16.0f) * (1.0f / 16.0f);
            }
            l = crush_l;
            r = crush_r;
        }
        l *= drop_g;
        r *= drop_g;
        /* the capture, and what reads it back: stutter, tape stop, backspin */
        if (cap_mode == CM_LIVE || cap_mode == CM_STOP || cap_mode == CM_SPIN) {   /* a loop holds its slice */
            cap_acc_l += l;
            cap_acc_r += r;
            if (j & 1u) {
                cap_buf[cap_w][0] = (int16_t)(fm_clampf(cap_acc_l * 0.5f, -1.0f, 1.0f) * 32767.0f);
                cap_buf[cap_w][1] = (int16_t)(fm_clampf(cap_acc_r * 0.5f, -1.0f, 1.0f) * 32767.0f);
                cap_w = cap_w + 1u >= CAP_N ? 0u : cap_w + 1u;
                cap_acc_l = cap_acc_r = 0.0f;
            }
        }
        if (cap_mode == CM_LIVE)
            cap_xf = fm_maxf(cap_xf - 1.0f / (0.006f * EN_SR), 0.0f);
        else
            cap_xf = fm_minf(cap_xf + 1.0f / (0.004f * EN_SR), 1.0f);
        if (cap_xf > 0.0f) {
            float cl = cap_read(cap_rp, 0), cr2 = cap_read(cap_rp, 1), amp = 1.0f;
            if (cap_mode == CM_STUT || cap_mode == CM_SKIP) {
                cap_rp += 0.5f;
                if (cap_rp >= cap_start + cap_len) {
                    cap_rp -= cap_len;
                    if (cap_mode == CM_SKIP)    /* the needle jumping back a groove */
                        skip_clk = 0.4f;
                }
            } else if (cap_mode == CM_STOP) {
                cap_rate = fm_maxf(1.0f - cap_t / (0.75f * EN_SR), 0.0f);
                cap_rp += 0.5f * cap_rate * cap_rate;
                amp = fm_minf(cap_rate * 4.0f, 1.0f);
            } else if (cap_mode == CM_SPIN) {
                cap_rate = -1.0f - 3.0f * fm_minf(cap_t / (0.35f * EN_SR), 1.0f);
                cap_rp += 0.5f * cap_rate;
                amp = fm_maxf(1.0f - cap_t / (0.7f * EN_SR), 0.0f);
            }
            cap_t += 1.0f;
            l += (cl * amp - l) * cap_xf;
            r += (cr2 * amp - r) * cap_xf;
            if (skip_clk > 0.001f) {
                float c = skip_clk * wnoise(&nz1);
                l += c;
                r += c * 0.8f;
                skip_clk *= 0.93f;
            }
        }
        mute_g += ((muted ? 0.0f : 1.0f) - mute_g) * 0.002f;
        out[2 * j] = (int32_t)(fm_tanhf(l * gain * mute_g) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r * gain * mute_g) * 8300000.0f);
        es += l * l + r * r;
        room_in[j] = 0.0f;
        echo_send[j] = 0.0f;
    }
    en_level += (fm_minf(fm_sqrtf(es / (float)(2 * n)) * gain * 3.0f, 1.0f) - en_level) * 0.15f;
    (void)rs;
}

/* ============================================================ parameters === */
static void key_set(void)
{
    root_pc = par[P_ROOT] % 12;
    scale_i = (unsigned)par[P_SCALE] < EN_NSCALES ? par[P_SCALE] : 0;
    mel_pool = EN_SCALE_POOL[scale_i];
    chord_set(chord_deg);
}
static float tempo_spb(void) { return EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_LEAD_LVL: trk_lvl[T_LEAD] = lvl(v); break;
    case P_COMP_LVL: trk_lvl[T_COMP] = lvl(v) * 0.75f; break;
    case P_BASS_LVL: trk_lvl[T_BASS] = lvl(v); break;
    case P_ANS_LVL: trk_lvl[T_ANS] = lvl(v); break;
    case P_LEAD_TONE: case P_COMP_TONE: case P_BASS_TONE: case P_ANS_TONE: {
        int t = p == P_LEAD_TONE ? T_LEAD : p == P_COMP_TONE ? T_COMP : p == P_BASS_TONE ? T_BASS : T_ANS;
        trk_tone[t] = onepole((t == T_BASS ? 200.0f : 500.0f) * fm_exp2f((float)v * 0.055f));
        break;
    }
    case P_LEAD_REL: trk_rel[T_LEAD] = (float)v * 0.01f; break;
    case P_LEAD_INST: trk_inst[T_LEAD] = EN_LEAD_INST[(unsigned)v < EN_NLEAD ? v : 0]; break;
    case P_COMP_INST: trk_inst[T_COMP] = EN_COMP_INST[(unsigned)v < EN_NCOMP ? v : 0]; break;
    case P_BASS_INST: trk_inst[T_BASS] = EN_BASS_INST[(unsigned)v < EN_NBASS ? v : 0]; break;
    case P_ANS_INST: trk_inst[T_ANS] = EN_ANS_INST[(unsigned)v < EN_NANS ? v : 0]; break;
    case P_DRUM_LVL: drum_lvl = lvl(v); break;
    case P_CRUNCH: crunch = (float)v * 0.01f; break;
    case P_PATTERN: pat_next = v; if (!en_playing) pat_now = v; en_pattern = (uint8_t)v; break;
    case P_KICK: lane_lvl[LN_KICK] = lvl(v); break;
    case P_SNARE: lane_lvl[LN_SNARE] = lane_lvl[LN_RIM] = lane_lvl[LN_CLAP] = lvl(v); break;
    case P_HATS: lane_lvl[LN_HAT] = lane_lvl[LN_OHAT] = lane_lvl[LN_SHAKE] = lvl(v); break;
    case P_PERC: lane_lvl[LN_PERC] = lvl(v); break;
    case P_DUST_LVL: d_lvl = lvl(v) * 1.4f; break;
    case P_AGE: d_age = (float)v * 0.01f; en_wear = d_age; break;
    case P_SURFACE: d_surf = (float)v * 0.01f; break;
    case P_HISS: d_hiss = (float)v * 0.01f; break;
    case P_GRAIN: d_grain = lvl(v); break;
    case P_SMEAR: d_smear = (float)v * 0.01f; break;
    case P_DUCK: d_duck = (float)v * 0.01f; break;
    case P_HUM: d_hum = lvl(v); break;
    case P_DUST_CHAR: d_char = (unsigned)v < EN_NDUST ? v : 0; break;
    case P_TAPE: tape_amt = (float)v * 0.01f; break;
    case P_MELLOW: mellow_hz = 18000.0f * fm_exp2f(-(float)v * 0.048f); break;
    case P_ROOM: room_amt = lvl(v) * 1.2f; break;
    case P_ECHO: echo_amt = lvl(v); break;
    case P_ROOT: case P_SCALE: key_set(); break;
    case P_JAZZ: chord_set(chord_deg); break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO: spb = tempo_spb(); break;
    case P_MIDI: {
        int i;
        for (i = 0; i < 16; i++)
            en_midi_out(0xB0 | (uint32_t)i, 123, 0);
        break;
    }
    default: break;
    }
}

static void quiet(void)
{
    int i;
    for (i = 0; i < EN_NVOX; i++) {
        vx[i].on = 0;
        en_vtrk[i] = 0xFF;
    }
    for (i = 0; i < NDRUM; i++)
        dv[i].on = 0;
    for (i = 0; i < NEV; i++)
        ev[i].used = 0;
    for (i = 0; i < 4; i++)
        lead_held[i] = 0;
    for (i = 0; i < 128; i++)
        key_cnt[i] = note_ref[i] = 0;
    if (par[P_MIDI])
        for (i = 0; i < 16; i++)
            en_midi_out(0xB0 | (uint32_t)i, 123, 0);
}

static void regrow(void)                        /* a new tape: the progression from the top */
{
    prog_i = 0;
    prog_k = 0;
    auto_mode = 1;
    chord_set(PROG[scale_i][0][0]);
    prog_k = 1;
    hist[0] = hist[1] = hist[2] = -1;
    en_chord_hist[0] = en_chord_hist[1] = en_chord_hist[2] = -1;
    comp_prev_n = 0;
    ph.n = 0;
    idle_bars = 0.0f;
    key_bars = 99.0f;
    ghost_rest = 0;
    en_dreaming = 0;
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    for (i = 0; i < ECHO_N; i++)
        echo_buf[i] = 0;
    for (i = 0; i < GRN; i++)
        grn_buf[i] = 0;
    for (i = 0; i < CAP_N; i++)
        cap_buf[i][0] = cap_buf[i][1] = 0;
    for (i = 0; i < WOW_N; i++)
        wow_buf[0][i] = wow_buf[1][i] = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < EN_NVOX; i++) {
        vx[i].on = 0;
        en_vtrk[i] = 0xFF;
    }
    en_pats_default();
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    spb = tempo_spb();
    key_set();
    regrow();
}

/* ============================================================= transport === */
static void play_start(void)
{
    en_playing = 1;
    step_count = 0;
    step_left = 0.0f;
    en_bar = (uint32_t)-1;
    pat_now = pat_next;
    idle_bars = 0.0f;
    needle_drop();
}
static void play_stop(void)
{
    int t;
    en_playing = 0;
    for (t = 0; t < NTRK; t++)
        if (t != T_LEAD)
            track_release(t, -1);
    track_release(T_LEAD, 1);
    for (t = 0; t < NEV; t++)
        if (ev[t].used && ev[t].type != EV_OFF)
            ev[t].used = 0;
    en_dreaming = 0;
    if (d_char == DUST_VINYL)                   /* the run-out groove, a few turns */
        du.ro_left = 7.0f * EN_SR;
    if (en_sideb)
        en_sideb = 0;
}

static uint32_t ext_age;                        /* renders since the last clock message (EXT goes after ~0.7 s) */
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
        case C_PLAY:
            if (a && !en_playing)
                play_start();
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NOTE: lead_key(a, b >> 8 & 1, b & 0x7F); break;
        case C_HOLD: en_held = (uint8_t)(a != 0); break;
        case C_SIDEB:
            if (en_sideb == 0)
                en_sideb = 1;
            else if (en_sideb == 1)
                en_sideb = 0;
            else if (en_sideb == 2) {
                en_sideb = 3;
                fill_now = 1;
            }
            break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_PANIC: quiet(); break;
        case C_MUTE: muted = (uint8_t)(a != 0); break;
        case C_MUTATE:
            mutate_now = 1;
            drift_fill = 1;
            break;
        case C_REGROW: regrow(); break;
        case C_CLOCK:
            if (par[P_MIDI] != EN_MIDI_CLOCK && par[P_MIDI] != EN_MIDI_BOTH)
                break;
            if (a == EN_CLK_TICK) {
                static uint32_t ticks, last_t;
                if (last_t && now - last_t < (uint32_t)EN_SR)
                    en_bpm += (EN_SR * 60.0f / (24.0f * (float)(now - last_t)) - en_bpm) * 0.05f;
                last_t = now;
                en_ext = 1;
                ext_age = 0;
                if (en_playing && ++ticks % 6u == 0u) {
                    spb = EN_SR * 60.0f / (fm_maxf(en_bpm, 40.0f) * 4.0f);
                    on_step();
                }
            } else if (a == EN_CLK_START) {
                en_ext = 1;
                ext_age = 0;
                if (en_playing)
                    play_stop();
                play_start();
            } else if (a == EN_CLK_CONTINUE) {
                en_ext = 1;
                ext_age = 0;
                if (!en_playing)
                    play_start();
            } else if (en_playing) {
                play_stop();
            }
            break;
        }
    }
}

/* ================================================================ render === */
static void control(uint32_t n)                 /* once a sub-block: the slow things */
{
    const float dt = (float)n * (1.0f / EN_SR);
    uint32_t m = fx_mask;
    int t;
    if ((m ^ fxm) >> FX_ST4 & 7u || (m ^ fxm) >> FX_STOP & 3u) {   /* the capture: which reader */
        if (m >> FX_SPIN & 1u)
            cap_enter(CM_SPIN, 0.0f);
        else if (m >> FX_STOP & 1u)
            cap_enter(CM_STOP, 0.0f);
        else if (m >> FX_SKIP & 1u)
            cap_enter(CM_SKIP, 0.43f * EN_SR);  /* a skip: about a quarter of a turn at 33 1/3 */
        else if (m >> FX_ST8 & 1u)
            cap_enter(CM_STUT, 2.0f * spb);
        else if (m >> FX_ST4 & 1u)
            cap_enter(CM_STUT, 4.0f * spb);
        else
            cap_mode = CM_LIVE;
    }
    fxm = m;
    for (t = 0; t < EN_NTRACK; t++)
        en_glow[t] *= fm_expf(-dt * 4.0f);
    kick_env *= fm_expf(-dt * 5.0f);
    drum_gate += ((en_sideb == 2 && !fill_now ? 0.0f : 1.0f) - drum_gate) * fm_minf(dt * 6.0f, 1.0f);
    master_lp += ((en_sideb == 2 ? 0.3f : 1.0f) * (1.0f - 0.55f * d_age) * (d_char == DUST_CASSETTE ? 0.75f : 1.0f) - master_lp) *
                 fm_minf(dt * 2.0f, 1.0f);
    {   /* muffle: the cutoff glides down an octave every ~0.2 s to 300 Hz while held, back up 4x as fast */
        int on = (int)(fxm >> FX_MUFFLE & 1u);
        float want = on ? 300.0f : 20000.0f, oct = dt * (on ? 5.0f : 20.0f);
        muf_hz *= fm_exp2f(want < muf_hz ? -oct : oct);
        muf_hz = on ? fm_maxf(muf_hz, 300.0f) : fm_minf(muf_hz, 20000.0f);
        muf_c = onepole(muf_hz);
    }
    {   /* dropouts: the cassette's, now and then as it ages; FX: lots. Each a dip of 40-160 ms */
        static uint8_t dropping;
        drop_t -= dt;
        if (drop_t <= 0.0f) {
            int fx = (int)(fxm >> FX_DROP & 1u), worn = d_char == DUST_CASSETTE && d_age > 0.4f && en_playing;
            if (dropping || !(fx || worn)) {
                dropping = 0;
                drop_t = fx ? 0.06f + 0.25f * frand() : 0.2f + 0.3f * frand();
            } else if (frand() < (fx ? 0.7f : (d_age - 0.4f) * 0.15f)) {
                dropping = 1;
                drop_t = 0.04f + 0.12f * frand();
            } else {
                drop_t = 0.1f + 0.2f * frand();
            }
        }
        drop_g += ((dropping ? 0.15f : 1.0f) - drop_g) * fm_minf(dt * 70.0f, 1.0f);
    }
    wow_rnd_t -= dt;
    if (wow_rnd_t <= 0.0f) {
        wow_rnd_t = 0.5f + 1.5f * frand();
        wow_rnd = frand() * 2.0f - 1.0f;
    }
    if (en_playing)
        en_spin += dt * (d_char == DUST_VINYL ? 33.333f / 60.0f : 0.4f);
    if (en_spin >= 1.0f)
        en_spin -= 1.0f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.3f;
    uint32_t at = 0;
    drain();
    if (en_ext && ++ext_age > 120u) {
        en_ext = 0;
        spb = tempo_spb();
    }
    while (n) {
        uint32_t k = n > SUBN ? SUBN : n, j;
        int t, i;
        control(k);
        if (en_playing && !en_ext) {
            float sp = tempo_spb() * ((fxm >> FX_HALF & 1u) ? 2.0f : 1.0f);
            spb = sp;
            step_left -= (float)k;
            while (step_left <= 0.0f) {
                on_step();
                step_left += spb;
            }
        }
        ev_due();
        for (t = 0; t < NTRK; t++)
            for (j = 0; j < k; j++)
                bus_l[t][j] = bus_r[t][j] = 0.0f;
        for (i = 0; i < EN_NVOX; i++)
            if (vx[i].on)
                render_voice(&vx[i], k);
        render_drums(k);
        for (j = 0; j < k; j++)
            mono_mix[j] = bus_l[0][j] + bus_r[0][j] + bus_l[1][j] + bus_r[1][j] + bus_l[2][j] + bus_r[2][j] +
                          bus_l[3][j] + bus_r[3][j] + drum_l[j] + drum_r[j];
        render_dust(k, mono_mix);
#ifdef EN_HOST
        if (en_bus_on && at + k <= 1024u)
            for (j = 0; j < k; j++) {
                float *bb = &en_bus[(at + j) * EN_NBUS];
                for (t = 0; t < NTRK; t++)
                    bb[t] = fm_tanhf((bus_l[t][j] + bus_r[t][j]) * 0.5f * gain);
                bb[4] = fm_tanhf((drum_l[j] + drum_r[j]) * 0.5f * gain);
                bb[5] = fm_tanhf((dust_l[j] + dust_r[j]) * 0.5f * gain);
            }
#endif
        master(k, gain, out);
        now += k;
        out += 2 * k;
        at += k;
        n -= k;
    }
}
