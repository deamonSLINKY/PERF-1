/* SPDX-License-Identifier: GPL-3.0-only */
/* LOF-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too. The panel's handling (knobs, help cards) is AMB-1's, by way of ECO-1.
 *
 *   keys           play the lead (chromatic, or snapped to the scale: Global)
 *   PLAY           play / stop                   REC    chord hold
 *   SAVE           Side B: a breakdown on the next bar; again, back in with a fill. On Global: store the preset
 *                  playing (held: back to the factory's)
 *   FX             the performance effects on the keys, on and off
 *   OCT- / OCT+    the lead's octave; in the sequencer, the lane (with SEL: clear it / the factory's back)
 *   SELECT         the pages (with HOME held: the tempo)
 *   ALGORITHM      the page's instrument (a voice's), kit, DUST character; elsewhere the key's root
 *   PRESETS        the preset, 01-10
 *   HOME ENV LFO EDIT ARP SEQ GLO   Groove; Lead, Daydream; Comp, Comp style; Bass; Answer; Drums, Kit
 *                  (held: the sequencer); Dust, Dust 2, Space, Global
 *   SEL held       shift: + a key, the key's root (on the Drums pages, + white 1-8: the pattern); + SEQ a change
 *                  on the next bar (as Groove 2's Drift);
 *                  + ENV Daydream on / off; + ARP Answer on / off; + HOME the keys play notes or chords */
#include "../dsp/fastmath.h"
#include "crt.c"

enum { V_GROOVE, V_GROOVE2, V_LEAD, V_DREAM, V_DREAM2, V_COMP, V_CSTYLE, V_BASS, V_ANS, V_DRUMS, V_KIT, V_DUST, V_DUST2,
       V_SPACE, V_GLOBAL, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Groove", "Groove 2", "Lead", "Daydream", "Daydream 2", "Comp", "Comp style",
                                              "Bass", "Answer", "Drums", "Kit", "Dust", "Dust 2", "Space", "Global"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_HOME, B_ENV, B_ENV, B_ENV, B_LFO, B_LFO, B_EDIT, B_ARP, B_SEQ, B_SEQ,
                                         B_GLO, B_GLO, B_GLO, B_GLO};
enum { K_LEDS = 100, K_CPU, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_SWING, P_JAZZ, P_BUSY},
    {P_DRIFT, P_DRIFT_DRUMS, P_DRIFT_POOL, P_DRIFT_CHORDS},
    {P_LEAD_LVL, P_LEAD_TONE, P_LEAD_REL, P_LAZY},
    {P_DD_WAIT, P_DD_FILL, P_DD_DENS, P_DD_MOOD},
    {P_DD_RANGE, P_DD_LEGATO, P_DD_REPEAT, P_DD_DYN},
    {P_COMP_LVL, P_COMP_TONE, P_VOICING, P_ROLL},
    {P_COMP_PAT, P_COMP_LEN, P_PUSH, P_SPREAD},
    {P_BASS_LVL, P_BASS_TONE, P_BASS_PAT, P_SLIDE},
    {P_ANS_LVL, P_ANS_TONE, P_CHATTY, P_ECHO_PH},
    {P_DRUM_LVL, P_CRUNCH, P_PATTERN, P_FILL},
    {P_KICK, P_SNARE, P_HATS, P_PERC},
    {P_DUST_LVL, P_AGE, P_SURFACE, P_HISS},
    {P_GRAIN, P_SMEAR, P_DUCK, P_HUM},
    {P_TAPE, P_MELLOW, P_ROOM, P_ECHO},
    {P_ROOT, P_SCALE, P_SNAP, P_MIDI},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Swing", "Jazz", "Busy"},      {"Drift", "Drums", "Pool", "Chords"},
    {"Level", "Tone", "Release", "Lazy"},
    {"Wait", "Fill", "Density", "Mood"},     {"Range", "Legato", "Repeat", "Dynamics"},
    {"Level", "Tone", "Voicing", "Roll"},
    {"Pattern", "Length", "Push", "Spread"}, {"Level", "Tone", "Pattern", "Slide"},
    {"Level", "Tone", "Chatty", "Echo"},     {"Level", "Crunch", "Pattern", "Fill"},
    {"Kick", "Snare", "Hats", "Perc"},       {"Level", "Age", "Surface", "Hiss"},
    {"Grain", "Smear", "Duck", "Hum"},       {"Tape", "Mellow", "Room", "Echo"},
    {"Root", "Scale", "Keys", "MIDI"},
};

/* the screen's colours: a late-night study room, dusk purple and lamp-light peach */
#define K_BG RGB(26, 22, 36)
#define K_PANEL RGB(46, 40, 62)
#define K_LINE RGB(70, 62, 92)
#define K_DIM RGB(150, 138, 170)
#define K_TEXT RGB(242, 234, 222)
#define K_ACC RGB(255, 176, 124)          /* the accent: lamp light */
#define K_TEAL RGB(126, 204, 192)         /* the second colour: the comp, the answer */
#define K_VINYL RGB(18, 16, 22)
#define K_GROOVE RGB(44, 40, 52)
#define K_RUST RGB(200, 84, 32)
#define K_RINGS RGB(232, 156, 92)
#define K_FILL K_ACC                      /* a knob's fill */
#define K_LOCK RGB(196, 140, 255)         /* an effect locked on */
static const uint16_t TAPE_COL[10] = {RGB(232, 120, 96), RGB(120, 168, 220), RGB(214, 170, 90), RGB(150, 200, 120),
                                     RGB(170, 130, 210), RGB(220, 150, 80), RGB(200, 220, 236), RGB(240, 160, 190), RGB(180, 140, 100), RGB(110, 120, 190)};

static struct {
    uint8_t view, seq, lane, fx;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];
    uint32_t down_t[NB];
    uint32_t turn_t;
    uint8_t help;
    uint8_t save_done;
    uint8_t dirty;
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];
    uint32_t boot_t;
    uint8_t splash, splash_step;
    uint32_t lead_keys;                /* the keys playing the lead now (bit = key) */
    uint32_t fx_lock;                  /* the effects locked on (SAVE held in FX: lock mode) */
    uint8_t lock_mode;                 /* SAVE held in FX: keys tapped lock and unlock their effects */
    uint32_t lock_keys;                /* keys pressed to lock or unlock: no effect while they're held */
    uint32_t input_t;                  /* the last touch: a button, a key, a knob, a MIDI note */
    uint8_t saver, sv_shown;           /* the screensaver up; the pose on screen */
    uint32_t sv_t;                     /* input_t when it came up: any touch since takes it down */
} ui;
uint32_t ui_splash_ms = 3200u;
uint32_t ui_saver_ms = 60000u;         /* the screensaver after this long untouched (0: never) */

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : 0;
    }
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS)
        en_param_text(k, v, b);
    else
        b[0] = 0;
}
static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
}

static uint8_t accel_off;
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}
static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

static void set_piece(int i)
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say("PRESET", piece_name(proj.piece));
    mark_dirty();
}

static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
    if (v != V_DRUMS)
        ui.seq = 0;
}

/* the page's instrument (or kit, or character, or the root) on ALGORITHM */
static int algo_param(void)
{
    switch (ui.view) {
    case V_LEAD: case V_DREAM: case V_DREAM2: return P_LEAD_INST;
    case V_COMP: case V_CSTYLE: return P_COMP_INST;
    case V_BASS: return P_BASS_INST;
    case V_ANS: return P_ANS_INST;
    case V_DRUMS: case V_KIT: return P_KIT;
    case V_DUST: case V_DUST2: return P_DUST_CHAR;
    default: return P_ROOT;
    }
}
static const char *algo_word(int p)
{
    switch (p) {
    case P_LEAD_INST: return "LEAD";
    case P_COMP_INST: return "COMP";
    case P_BASS_INST: return "BASS";
    case P_ANS_INST: return "ANSWER";
    case P_KIT: return "KIT";
    case P_DUST_CHAR: return "DUST";
    default: return "KEY";
    }
}

/* the keys: key k (0 = F3) is MIDI 53 + k */
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int white_of(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE_KEY[i] == k)
            return i;
    return -1;
}
static int black_of(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK_KEY[i] == k)
            return i;
    return -1;
}
static int fx_bit_of(int k)                    /* FX on: what each key holds */
{
    static const uint8_t WFX[16] = {FX_ST4, FX_ST8, FX_SKIP, FX_HALF, FX_STOP, FX_SPIN, FX_MUFFLE, FX_RADIO,
                                    FX_MUTE0, FX_MUTE1, FX_MUTE2, FX_MUTE3, FX_MUTE4, FX_MUTE5, FX_ROOM, FX_ECHO};
    static const uint8_t BFX[11] = {FX_WOBBLE, FX_CRUSH, FX_FILL, FX_FREEZE, FX_DROP, FX_SOLO0, FX_SOLO1, FX_SOLO2,
                                    FX_SOLO3, FX_SOLO4, FX_SOLO5};
    int w = white_of(k), b = black_of(k);
    return w >= 0 ? WFX[w] : b >= 0 ? BFX[b] : -1;
}

static void lead_all_off(void)
{
    int k;
    for (k = 0; k < NKEYS; k++)
        if (ui.lead_keys >> k & 1u)
            en_note(53 + k, 0, 0);
    ui.lead_keys = 0;
}

#ifndef EN_WEB
static void rm_enter(void);                     /* REMOTE, below */
#endif

/* ------------------------------------------------------------ buttons --- */
static void button(int b)
{
    int sel = (int)(ui.btn >> B_SEL & 1u), v;
    if (sel)
        ui.btn_used |= 1u << B_SEL;
    switch (b) {
    case B_HOME:
        if (sel) {                              /* the keys: single notes or whole chords */
            knob_set(P_SNAP, proj.par[P_SNAP] == 2 ? 1 : 2);
            lead_all_off();
            say("KEYS PLAY", proj.par[P_SNAP] == 2 ? "CHORDS" : "NOTES");
            return;
        }
        set_view(ui.view == V_GROOVE ? V_GROOVE2 : V_GROOVE);
        return;
    case B_ENV:
        if (sel) {
            knob_set(P_DD_ON, !proj.par[P_DD_ON]);
            say("DAYDREAM", proj.par[P_DD_ON] ? "ON" : "OFF");
            return;
        }
        set_view(ui.view == V_LEAD ? V_DREAM : ui.view == V_DREAM ? V_DREAM2 : V_LEAD);
        return;
    case B_LFO: set_view(ui.view == V_COMP ? V_CSTYLE : V_COMP); return;
    case B_EDIT: set_view(V_BASS); return;
    case B_ARP:
        if (sel) {
            knob_set(P_ANS_ON, !proj.par[P_ANS_ON]);
            say("ANSWER", proj.par[P_ANS_ON] ? "ON" : "OFF");
            return;
        }
        set_view(V_ANS);
        return;
    case B_SEQ:
        if (sel) {                              /* a change, on the next bar (as Drift's) */
            en_mutate();
            say("CHANGE", "NEXT BAR");
            return;
        }
        if (ui.seq) {                           /* tapped in the sequencer: back to Drums */
            ui.seq = 0;
            say("SEQUENCER", "OFF");
            return;
        }
        set_view(ui.view == V_DRUMS ? V_KIT : V_DRUMS);
        return;
    case B_GLO:
#ifndef EN_WEB
        if (sel) {                              /* shift: remote, the panel plays the browser */
            lead_all_off();
            rm_enter();
            return;
        }
#endif
        v = ui.view >= V_DUST && ui.view < V_GLOBAL ? ui.view + 1 : V_DUST;
        set_view(v);
        return;
    case B_FX:
        if (sel) {                              /* SEL + FX: every lock off */
            ui.fx_lock = 0;
            say("FX LOCKS", "ALL OFF");
            return;
        }
        ui.fx = !ui.fx;
        ui.lock_mode = 0;
        if (ui.fx)
            lead_all_off();
        say("FX KEYS", ui.fx ? "ON" : ui.fx_lock ? "LOCKED STAY ON" : "OFF");
        return;
    case B_REC:
        en_hold(!en_held);
        say("CHORD HOLD", en_held ? "OFF" : "ON");
        return;
    case B_SAVE:
        if (ui.view == V_GLOBAL) {              /* Global: store the preset */
            if (project_save() == 0) {
                ui.dirty = 0;
                say("STORED", piece_name(proj.piece));
            } else {
                say("NOT STORED", "FLASH ERROR");
            }
            return;
        }
        en_side_b();
        say("SIDE B", en_sideb == 0 ? "NEXT BAR" : en_sideb == 1 ? "CANCELLED" : en_sideb == 2 ? "BACK IN" : "");
        return;
    case B_PLAY:
        if (en_playing) {
            en_play(0);
            say("STOP", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("PLAY", 0);
        }
        return;
    case B_OCTDN: case B_OCTUP: {
        int up = b == B_OCTUP;
        if (ui.seq) {
            if (sel) {                          /* clear the lane / the factory's back */
                uint16_t keep = en_pats[proj.par[P_PATTERN]][ui.lane];
                if (up) {
                    uint16_t mine[EN_NPAT][EN_NLANE];
                    memcpy(mine, en_pats, sizeof mine);
                    en_pats_default();
                    keep = en_pats[proj.par[P_PATTERN]][ui.lane];
                    memcpy(en_pats, mine, sizeof mine);
                } else {
                    keep = 0;
                }
                en_pats[proj.par[P_PATTERN]][ui.lane] = keep;
                say(up ? "LANE RESET" : "LANE CLEARED", EN_LANE_NAME[ui.lane]);
                mark_dirty();
            } else {
                ui.lane = (uint8_t)((ui.lane + (up ? 1 : EN_NLANE - 1)) % EN_NLANE);
                say("LANE", EN_LANE_NAME[ui.lane]);
            }
            return;
        }
        {
            char t[8];
            lead_all_off();
            knob_set(P_LEAD_OCT, proj.par[P_LEAD_OCT] + (up ? 1 : -1));
            en_param_text(P_LEAD_OCT, proj.par[P_LEAD_OCT], t);
            say("LEAD OCTAVE", t);
        }
        return;
    }
    default: return;
    }
}

/* the keys: the lead; FX on, the effects; the sequencer, the steps; with SEL, the root (or a pattern) */
static void key_down(int k)
{
    int sel = (int)(ui.btn >> B_SEL & 1u), w = white_of(k);
    if (ui.fx && !ui.seq) {
        int f = fx_bit_of(k);
        if (f < 0)
            return;
        if (ui.lock_mode) {                     /* lock mode: tapped, locked (or locked, unlocked) */
            ui.fx_lock ^= 1u << f;
            ui.lock_keys |= 1u << k;
            ui.btn_used |= 1u << B_SAVE;        /* SAVE let go now is not Side B */
            say(ui.fx_lock >> f & 1u ? "LOCKED" : "UNLOCKED", EN_FX_NAME[f]);
        } else if (ui.fx_lock >> f & 1u) {      /* a locked one tapped: that one off, the others stay */
            ui.fx_lock &= ~(1u << f);
            ui.lock_keys |= 1u << k;
            say("UNLOCKED", EN_FX_NAME[f]);
        }
        return;
    }
    if (ui.seq) {
        if (w >= 0) {
            int p = proj.par[P_PATTERN];
            en_pats[p][ui.lane] ^= (uint16_t)(1u << w);
            mark_dirty();
        }
        return;
    }
    if (sel) {
        ui.btn_used |= 1u << B_SEL;
        if ((ui.view == V_DRUMS || ui.view == V_KIT) && w >= 0 && w < EN_NPAT) {
            char t[4];
            knob_set(P_PATTERN, w);
            t[0] = (char)('1' + w);
            t[1] = 0;
            say("PATTERN", t);
            return;
        }
        knob_set(P_ROOT, (5 + k) % 12);
        say("ROOT", EN_NOTE_NAME[proj.par[P_ROOT]]);
        return;
    }
    ui.lead_keys |= 1u << k;
    en_note(53 + k, 1, 100);
}
static void key_up(int k)
{
    if (ui.lead_keys >> k & 1u) {
        ui.lead_keys &= ~(1u << k);
        en_note(53 + k, 0, 0);
    }
}

/* MIDI in: notes play the lead (any channel); the clock, when Global's MIDI takes it */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if ((st & 0xF0u) == 0x90u && v) {
            en_note((int)n, 1, (int)v);
            ui.input_t = plat_ms();
        }
        else if ((st & 0xF0u) == 0x80u || ((st & 0xF0u) == 0x90u && !v))
            en_note((int)n, 0, 0);
    }
}

#define HELP_HOLD 900u
#define SEQ_HOLD 600u
#define STORE_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && i != B_SEL && i != B_SEQ && !(i == B_SAVE && (ui.view == V_GLOBAL || ui.fx)) &&
            !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD && (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;
        }
}

/* REMOTE (SEL + GLO): the panel plays LOF-1 in the browser over USB MIDI, and the FM-1 goes quiet. On
 * channel 16: the keys are notes 48 + k, the buttons CC 102 + b (127 down, 0 up), the knobs CC 85 + role
 * (64 + detents), MASTER CC 7, and CC 116 says hello every second. The browser answers with its lights
 * the same way (notes and CC 102 + b) and CC 117, so the panel lights as the emulator's does. SEL + GLO
 * again: back, stopped. Not in the browser's own build (it has no USB). */
#define RM_ST_NOTE 0x9Fu
#define RM_ST_OFF 0x8Fu
#define RM_ST_CC 0xBFu
#define RM_KEY0 48u
#define RM_BTN0 102u
#define RM_ENC0 85u
#define RM_MASTER 7u
#define RM_HELLO 116u
#define RM_ACK 117u
static struct {
    uint8_t on, link, drawn;
    uint32_t btn, keys, ign_b, ign_k;            /* sent as down; held at the start, not sent till let go */
    uint32_t lit_b, lit_k;                       /* the browser's lights */
    uint32_t hello_t, rx_t;
    int master;
} rm;
int ui_remote(void) { return rm.on; }

static void rm_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }


static void rm_draw(void)
{
    crt_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 120, K_BG);
    text_c(120, 4, &FONT_L, "REMOTE", K_ACC);
    text_c(120, 44, &FONT_S, rm.link ? "PLAYING THE BROWSER" : "WAITING FOR THE BROWSER", rm.link ? K_TEAL : K_DIM);
    text_c(120, 66, &FONT_XS, "USB MIDI  //  CHANNEL 16", K_DIM);
    text_c(120, 96, &FONT_S, "SEL + GLO: BACK", K_TEXT);
    cv_show(0, 56);
    rm.drawn = (uint8_t)(1u + rm.link);
}

#ifndef EN_WEB
static void rm_enter(void)
{
    int e;
    memset(&rm, 0, sizeof rm);
    rm.on = 1;
    rm.ign_b = plat_buttons();
    rm.ign_k = plat_keys();
    rm.hello_t = plat_ms() - 1000u;
    rm.master = -1;
    for (e = 0; e < NE; e++)
        (void)plat_enc(e);
    en_play(0);
    en_mute(1);                                  /* after the stop's note-offs */
    plat_leds(0, 0);
    rm_draw();
}
#endif

static void rm_leave(void)
{
    uint32_t i;
    for (i = 0; i < NB; i++)                     /* the browser lets go of what was down */
        if (rm.btn >> i & 1u)
            rm_out(RM_ST_CC, RM_BTN0 + i, 0);
    for (i = 0; i < NKEYS; i++)
        if (rm.keys >> i & 1u)
            rm_out(RM_ST_OFF, RM_KEY0 + i, 0);
    rm_out(RM_ST_CC, RM_HELLO, 0);               /* goodbye */
    rm.on = 0;
    en_mute(0);
    ui.btn = plat_buttons();                     /* what's held now does nothing when let go */
    ui.keys = plat_keys();
    ui.btn_used = ui.btn;
    ui.lead_keys = 0;
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;       /* everything drawn again */
    crt_fill(0, 0, 240, 240, K_BG);
    say("REMOTE", "OFF");
}

static void rm_tick(void)
{
    uint32_t b = plat_buttons(), k = plat_keys(), now = plat_ms(), ch, i, pkt;
    int e;
    while (plat_midi_in(&pkt)) {                 /* the browser's lights (channel 16); the rest, ignored */
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == RM_ST_NOTE || st == RM_ST_OFF) {
            if (n >= RM_KEY0 && n < RM_KEY0 + NKEYS) {
                uint32_t m = 1u << (n - RM_KEY0);
                rm.lit_k = st == RM_ST_NOTE && v ? rm.lit_k | m : rm.lit_k & ~m;
            }
        } else if (st == RM_ST_CC) {
            if (n >= RM_BTN0 && n < RM_BTN0 + NB) {
                uint32_t m = 1u << (n - RM_BTN0);
                rm.lit_b = v ? rm.lit_b | m : rm.lit_b & ~m;
            } else if (n == RM_ACK) {
                rm.rx_t = now;
                if (!rm.link) {
                    rm.link = 1;
                    rm_draw();
                }
                if (!v)
                    rm.lit_b = rm.lit_k = 0;
            }
        }
    }
    if (rm.link && now - rm.rx_t > 3000u) {      /* the browser's gone quiet */
        rm.link = 0;
        rm.lit_b = rm.lit_k = 0;
        rm_draw();
    }
    if ((b >> B_SEL & 1u) && (b >> B_GLO & 1u) && !((rm.ign_b | rm.btn) >> B_GLO & 1u)) {
        rm_leave();                              /* SEL + GLO: back (GLO never sent) */
        return;
    }
    rm.ign_b &= b;
    rm.ign_k &= k;
    b &= ~rm.ign_b;
    k &= ~rm.ign_k;
    if (b >> B_GLO & 1u && b >> B_SEL & 1u)
        b &= ~(1u << B_GLO);
    ch = b ^ rm.btn;
    for (i = 0; i < NB; i++)
        if (ch >> i & 1u)
            rm_out(RM_ST_CC, RM_BTN0 + i, b >> i & 1u ? 127u : 0u);
    rm.btn = b;
    ch = k ^ rm.keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            if (k >> i & 1u)
                rm_out(RM_ST_NOTE, RM_KEY0 + i, 100u);
            else
                rm_out(RM_ST_OFF, RM_KEY0 + i, 0u);
        }
    rm.keys = k;
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc(e);
        if (d)
            rm_out(RM_ST_CC, RM_ENC0 + (uint32_t)e, (uint32_t)(64 + (d > 63 ? 63 : d < -63 ? -63 : d)));
    }
    {
        int m = (int)(plat_master() >> 5);       /* 0 .. 127, sent when it moves by two (the pot's noise) */
        if (m > 127)
            m = 127;
        if (rm.master < 0 || m - rm.master > 1 || rm.master - m > 1 || ((m == 0 || m == 127) && m != rm.master)) {
            rm.master = m;
            rm_out(RM_ST_CC, RM_MASTER, (uint32_t)m);
        }
    }
    if (now - rm.hello_t >= 1000u) {
        rm.hello_t = now;
        rm_out(RM_ST_CC, RM_HELLO, 127u);
    }
    if (b || k)
        ui.act_t = now;
}

static int32_t enc_read(int role)         /* a knob's detents; turning it is a touch */
{
    int32_t d = plat_enc(role);
    if (d)
        ui.input_t = plat_ms();
    return d;
}

static void sv_swallow(void);

static void input(void)
{
    uint32_t btn, keys, ch, i, now = plat_ms();
    int32_t e;
    if (ui.saver)                                /* the screensaver up: buttons and knobs only wake it */
        sv_swallow();
    btn = plat_buttons();
    keys = plat_keys();
    if (btn != ui.btn || keys != ui.keys)
        ui.input_t = now;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = now;
            if (i == B_SAVE)
                ui.save_done = 0;
            if (i == B_SAVE && ui.fx) {         /* FX: SAVE held is lock mode; keys already held lock now */
                uint32_t k2, held = keys & ~ui.lock_keys;
                ui.lock_mode = 1;
                for (k2 = 0; k2 < NKEYS; k2++)
                    if (held >> k2 & 1u) {
                        int f = fx_bit_of((int)k2);
                        if (f >= 0)
                            ui.fx_lock |= 1u << f;
                    }
                if (held) {
                    ui.lock_keys |= held;
                    ui.btn_used |= m;
                }
                say("LOCK MODE", "TAP EFFECTS");
            }
            if (i == B_PLAY)
                button((int)i);
        } else {
            if (i == B_SAVE && ui.lock_mode)    /* lock mode over; SAVE tapped alone: Side B, as ever */
                ui.lock_mode = 0;
            if (!(ui.btn_used & m) && i != B_PLAY && i != B_SEL)
                button((int)i);
        }
    }
    /* SEQ held: the sequencer */
    if ((btn >> B_SEQ & 1u) && !(ui.btn_used >> B_SEQ & 1u) && now - ui.down_t[B_SEQ] >= SEQ_HOLD) {
        ui.btn_used |= 1u << B_SEQ;
        set_view(V_DRUMS);
        ui.seq = 1;
        lead_all_off();
        say("SEQUENCER", EN_LANE_NAME[ui.lane]);
    }
    /* SAVE held on Global: the preset back to the factory's (not stored until SAVE) */
    if ((btn >> B_SAVE & 1u) && ui.view == V_GLOBAL && !ui.save_done && now - ui.down_t[B_SAVE] >= STORE_HOLD) {
        ui.save_done = 1;
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("FACTORY", piece_name(proj.piece));
        mark_dirty();
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            if (keys >> i & 1u)
                key_down((int)i);
            else
                key_up((int)i);
        }
    ui.lock_keys &= keys;
    {   /* the effects: those locked, and each key held while FX is on */
        uint32_t m = ui.fx_lock;
        if (ui.fx)
            for (i = 0; i < NKEYS; i++)
                if ((keys & ~ui.lock_keys) >> i & 1u) {
                    int f = fx_bit_of((int)i);
                    if (f >= 0)
                        m |= 1u << f;
                }
        en_fx(m);
    }
    if (btn || keys || ch)
        ui.act_t = now;
    midi_in();
    if ((e = enc_read(EN_SELECT)) != 0) {
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_GROOVE) {
                ui.touched = 0;
                ui.touch_until = now + 1200u;
            } else {
                char t[10];
                en_param_text(P_TEMPO, proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.turn_t = now;
    }
    if ((e = enc_read(EN_ALGO)) != 0) {
        int p = algo_param(), lo, hi, v;
        char t[12];
        knob_range(p, &lo, &hi);
        v = proj.par[p] + (e > 0 ? 1 : -1);
        if (v > hi)
            v = lo;
        if (v < lo)
            v = hi;
        knob_set(p, v);
        en_param_text(p, proj.par[p], t);
        caps(t, t, sizeof t);
        say(algo_word(p), t);
        ui.turn_t = now;
    }
    if ((e = enc_read(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = now;
    }
    for (i = 0; i < 4; i++)
        if ((e = enc_read(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = now + 1200u;
            ui.act_t = ui.turn_t = now;
        }
    help_tick(btn, keys);
}

/* THE BOOT CARDS (as HYP-1's). First RedtailWorks: a pixel red panda (3x) sitting on the name, its
 * ringed tail curled up behind it, and the rings again beneath; then the LOF-1 card. Any button or key: straight in. */
#define PANDA_W 32
#define PANDA_H 25
static const char *const PANDA[PANDA_H] = {
    ".WWW..............WWW...........",
    ".WBBW............WBBW...........",
    ".WBBRRRRRRRRRRRRRRBBW...........",
    "..RRRRRRRRRRRRRRRRRR............",
    ".RRRWWWRRRRRRRRWWWRRR...........",
    ".RRRRRRRRRRRRRRRRRRRR...........",
    ".RRRRKKRRRRRRRRKKRRRR...........",
    ".RWWDKKDRRRRRRDKKDWWR...........",
    ".WWWWDDWWWWWWWWDDWWWW...........",
    ".WWWWWDDWWKKWWDDWWWWW...........",
    "..WWWWWDDWWWWDDWWWWW........DDD.",
    "...DWWWWDDWWDDWWWWD........DDDDD",
    "....DDWWWWWWWWWWDD.........TTTTT",
    "......DDDDDDDDDD...........TTTTT",
    "......DRRRRRRRRD...........RRRRR",
    ".....BRRRRRRRRRRB..........RRRRR",
    "....BBRRRRRRRRRRBB.........TTTTT",
    "....BBBRRRRRRRRBBB........TTTTTT",
    "...BBBBBRRRRRRBBBBB.......RRRRRR",
    "...BBBBBBBBBBBBBBBB......RRRRRR.",
    "...BBBBBBBBBBBBBBBBRRTTRRTTTTT..",
    "...BBBBBBBBBBBBBBBBRRTTRRTTTTT..",
    "....BBBB.BBBB.BBBBRRRTTRRRRRR...",
    "...KKBBB..BB..BBBKKDDTTDDDD.....",
    "...KKKK...KK...KKKK............."
};
static uint16_t panda_col(char c)
{
    switch (c) {
    case 'R': return RGB(200, 84, 32);           /* rust */
    case 'D': return RGB(140, 52, 20);           /* its shade */
    case 'T': return RGB(232, 156, 92);          /* the tail's pale rings */
    case 'W': return RGB(246, 236, 222);         /* the face's white */
    case 'B': return RGB(64, 30, 22);            /* the dark legs and belly, the ears inside */
    case 'K': return RGB(24, 16, 18);            /* eyes, nose, paws */
    default: return 0;
    }
}

static void draw_splash(void)
{
    int i, x, y;
    crt_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 96, K_BG);                     /* the panda */
    for (y = 0; y < PANDA_H; y++)
        for (x = 0; PANDA[y][x]; x++)
            if (PANDA[y][x] != '.')
                cv_rect(120 - PANDA_W * 3 / 2 + x * 3, 96 - PANDA_H * 3 - 4 + y * 3, 3, 3, panda_col(PANDA[y][x]));
    cv_show(0, 0);
    cv_begin(240, 144, K_BG);                    /* the name, the rings, and what's next */
    text_c(120, 0, &FONT_L, "REDTAIL", K_RUST);
    text_c(120, 36, &FONT_L, "WORKS", K_RINGS);
    for (i = 0; i < 9; i++)
        cv_rect(48 + i * 16, 86, 12, 6, i & 1 ? K_RINGS : K_RUST);
    cv_rect(40, 80, 160, 1, K_LINE);
    cv_rect(40, 97, 160, 1, K_LINE);
    cv_rect(40, 80, 1, 18, K_LINE);
    cv_rect(199, 80, 1, 18, K_LINE);
    text_c(120, 110, &FONT_XS, "PRESENTS", K_DIM);
    cv_show(0, 96);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the main picture (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static void scale_name(char *b)
{
    const char *n = EN_NOTE_NAME[proj.par[P_ROOT] % 12], *s = EN_SCALE_NAME[proj.par[P_SCALE] % EN_NSCALES];
    while (*n)
        *b++ = *n++;
    *b++ = ' ';
    while (*s)
        *b++ = *s++;
    *b = 0;
}

/* the header: the tape in a tag, the key; at the right the tempo, the transport and what is on */
static void draw_header(void)
{
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.par[P_SCALE] << 12 |
                                            (uint32_t)proj.par[P_TEMPO] << 16),
                      en_playing | ui.dirty << 1 | (uint32_t)msg << 2 | (uint32_t)en_held << 3 | (uint32_t)en_sideb << 4 |
                          (uint32_t)ui.fx << 7 | (uint32_t)en_dreaming << 8 | (uint32_t)ui.seq << 9 | (uint32_t)en_ext << 10 | (uint32_t)proj.par[P_SNAP] << 11 | (uint32_t)(ui.fx_lock != 0) << 13);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_ext)
        h = hash(h, (uint32_t)(en_bpm + 0.5f));
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        char t[24];
        if (ui.dirty) {
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        itoa_u(en_ext ? (uint32_t)(en_bpm + 0.5f) : (uint32_t)proj.par[P_TEMPO], t);
        x -= text_w(&FONT_XS, t);
        cv_text(x, 8, &FONT_XS, t, en_ext ? K_ACC : K_DIM);
        x -= 8;
        if (en_playing) {                       /* a little equalizer of the tracks */
            int k;
            for (k = 3; k >= 0; k--) {
                int bh = 3 + (int)(en_glow[k == 3 ? T_DRUMS : k] * 9.0f);
                x -= 4;
                cv_rect(x, 19 - bh, 3, bh, k == 3 ? K_DIM : K_ACC);
            }
            x -= 6;
        } else {
            int32_t j;
            x -= 10;
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
            x -= 4;
        }
#define FLAG(word, col) do { x -= text_w(&FONT_XS, word) + 6; cv_text(x, 8, &FONT_XS, word, col); } while (0)
        if (ui.fx)
            FLAG("FX", K_ACC);
        if (ui.fx_lock)
            FLAG("LOCK", K_LOCK);
        if (proj.par[P_SNAP] == 2 && !ui.fx && x > 150)
            FLAG("CHORDS", K_TEAL);
        if (ui.seq)
            FLAG("SEQ", K_ACC);
        if (en_held)
            FLAG("HOLD", K_TEAL);
        if (en_sideb)
            FLAG(en_sideb == 1 ? "B..." : en_sideb == 3 ? "A..." : "SIDE B", K_TEAL);
        if (en_dreaming && x > 150)
            FLAG("DREAM", K_DIM);
#undef FLAG
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;
        cv_round(4, 4, w, 20, 6, TAPE_COL[proj.piece % 10u]);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        scale_name(t);
        if (w + 10 + text_w(&FONT_XS, t) < x - 4)
            cv_text(w + 10, 8, &FONT_XS, t, K_DIM);
    }
    cv_show(0, 0);
}

#include "scene.c"

/* the chord panel, at the right of the scene: the chord now, the ones before, the beat */
static void draw_chord_panel(int32_t x0)
{
    char t[16];
    int32_t cx = x0 + (240 - x0) / 2, k;
    en_chord_name(t);
    text_c(cx, 14, text_w(&FONT_L, t) < 240 - x0 - 6 ? &FONT_L : &FONT_B, t, K_TEXT);
    {
        char a[16], b[16], line[40], *p = line;
        const char *s;
        int n = 0;
        if (en_chord_hist[0] >= 0) {
            en_deg_name(en_chord_hist[0], a);
            for (s = a; *s;)
                *p++ = *s++;
            n++;
        }
        if (en_chord_hist[1] >= 0) {
            en_deg_name(en_chord_hist[1], b);
            *p++ = ' ';
            *p++ = '<';
            *p++ = ' ';
            for (s = b; *s;)
                *p++ = *s++;
            n++;
        }
        *p = 0;
        if (n)
            text_c(cx, 56, &FONT_XS, line, K_DIM);
    }
    for (k = 0; k < 4; k++) {                   /* the beat */
        int on = en_playing && (en_step >> 2) == k;
        cv_round(cx - 30 + k * 16, 78, 12, 6, 3, on ? (k ? K_TEAL : K_ACC) : K_LINE);
    }
    {
        const char *s = !en_playing ? "STOPPED" : en_held ? "CHORD HELD" : en_dreaming ? "DAYDREAMING" :
                        en_sideb == 2 ? "SIDE B" : en_auto ? "ON ITS OWN" : "FOLLOWING YOU";
        text_c(cx, 96, &FONT_XS, s, en_auto || !en_playing ? K_DIM : K_ACC);
    }
    if (ui.view == V_GROOVE2 && en_playing) {
        if (en_drift_left < 0) {
            s_line(cx, 116, "DRIFT OFF");
        } else {
            char w[24] = "CHANGE IN ";
            char *e;
            itoa_u((uint32_t)en_drift_left, w + 10);
            e = w;
            while (*e)
                e++;
            *e++ = ' ';
            *e++ = 'B';
            *e++ = 'A';
            *e++ = 'R';
            if (en_drift_left != 1)
                *e++ = 'S';
            *e = 0;
            s_line(cx, 116, w);
        }
    }
    if (ui.view == V_DREAM || ui.view == V_DREAM2) {
        if (!proj.par[P_DD_ON])
            s_line(cx, 116, "DAYDREAM OFF (SEL+ENV)");
        else if (!en_dreaming && en_playing && proj.par[P_DD_WAIT] < 3) {
            static const uint8_t WAIT[3] = {4, 8, 16};
            int left = (int)WAIT[proj.par[P_DD_WAIT]] - (int)en_idle_bars;
            char w[24] = "DREAMS IN ";
            itoa_u((uint32_t)(left < 1 ? 1 : left), w + 10);
            {
                char *e = w;
                while (*e)
                    e++;
                *e++ = ' ';
                *e++ = 'B';
                *e++ = 'A';
                *e++ = 'R';
                *e++ = left > 1 ? 'S' : 0;
                *e = 0;
            }
            s_line(cx, 116, w);
        }
    }
}

/* a voice's page: its instrument, and its notes on a keyboard of three octaves */
static void draw_voice(int trk)
{
    static const uint16_t COL[4] = {K_ACC, K_TEAL, RGB(214, 170, 90), RGB(170, 150, 230)};
    static const uint8_t LOW[4] = {48, 48, 28, 60};   /* the keyboard's lowest note: C3, C3, E1, C4 */
    static const int PI_[4] = {P_LEAD_INST, P_COMP_INST, P_BASS_INST, P_ANS_INST};
    char t[16], c[16];
    int k, wk = 0, i;
    int32_t kx = 8, ky = 62, ww = 10;
    uint32_t on = 0, on2 = 0;
    int lo = LOW[trk];
    en_param_text(PI_[trk], proj.par[PI_[trk]], t);
    caps(t, t, sizeof t);
    cv_text(10, 8, &FONT_B, t, COL[trk]);
    en_chord_name(c);
    cv_text(230 - text_w(&FONT_B, c), 8, &FONT_B, c, K_TEXT);
    cv_text(10, 34, &FONT_XS, trk == T_ANS && !proj.par[P_ANS_ON] ? "OFF (SEL+ARP)" : trk == 0 ? (proj.par[P_SNAP] == 2 ? "CHORDS ON THE KEYS" : "THE KEYS") :
                                  trk == 1 ? "THE CHORD" : trk == 2 ? "THE ROOTS" : "CALL AND RESPONSE", K_DIM);
    if (trk == T_ANS && en_glow[T_ANS] > 0.3f) {  /* a speech bubble when it speaks */
        cv_round(150, 32, 70, 22, 6, K_PANEL);
        cv_rect(160, 52, 6, 4, K_PANEL);
        text_c(185, 36, &FONT_S, "...!", COL[3]);
    }
    if (trk == T_LEAD && en_dreaming)
        cv_text(150, 34, &FONT_XS, "GHOST PLAYING", K_DIM);
    for (i = 0; i < EN_NVOX; i++)               /* the notes sounding on this track */
        if (en_vtrk[i] == trk) {
            int d = (int)en_vnote[i] - lo;
            if (d >= 0 && d < 32)
                on |= 1u << d;
            else if (d >= 32 && d < 36)
                on2 |= 1u << (d - 32);
        }
    for (k = 0; k < 36; k++) {                  /* the white keys */
        int pc = (lo + k) % 12, black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10, lit;
        if (black)
            continue;
        lit = k < 32 ? (int)(on >> k & 1u) : (int)(on2 >> (k - 32) & 1u);
        cv_round(kx + wk * ww, ky, ww - 1, 58, 2, lit ? COL[trk] : RGB(214, 206, 196));
        wk++;
    }
    wk = 0;
    for (k = 0; k < 36; k++) {                  /* the black keys over them */
        int pc = (lo + k) % 12, black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10, lit;
        if (!black) {
            wk++;
            continue;
        }
        lit = k < 32 ? (int)(on >> k & 1u) : (int)(on2 >> (k - 32) & 1u);
        cv_rect(kx + wk * ww - 3, ky, 6, 34, lit ? COL[trk] : K_VINYL);
    }
    {   /* the octave's name under its C */
        char o[4] = "C0";
        o[1] = (char)('0' + lo / 12 - 1 + (lo % 12 ? 1 : 0));
        cv_text(kx + (lo % 12 ? 2 * ww : 0), ky + 62, &FONT_XS, o, K_DIM);
    }
}

/* the drums: eight lanes of sixteen steps, the playhead, the lane being edited */
static void draw_drums(void)
{
    static const char *const AB[EN_NLANE] = {"KCK", "SNR", "RIM", "CLP", "HAT", "OHT", "SHK", "PRC"};
    int p = proj.par[P_PATTERN], l, s;
    char t[24] = "PATTERN ";
    t[8] = (char)('1' + p);
    t[9] = 0;
    for (l = 0; l < EN_NLANE; l++) {
        int32_t y = 4 + l * 15;
        int sel = ui.seq && l == ui.lane;
        if (sel)
            cv_round(2, y - 2, 236, 15, 3, K_PANEL);
        cv_text(6, y, &FONT_XS, AB[l], sel ? K_ACC : K_DIM);
        for (s = 0; s < EN_NSTEP; s++) {
            int hit = en_pats[p][l] >> s & 1u, head = en_playing && s == en_step;
            int32_t x = 34 + s * 12 + (s >> 2) * 2;
            uint16_t c = hit ? (head ? K_TEXT : (sel ? K_ACC : (l < 2 ? K_ACC : K_TEAL))) : (head ? K_DIM : ((s & 3) ? K_LINE : K_GROOVE));
            if (hit)
                cv_round(x, y, 10, 10, 2, c);
            else
                cv_round(x + 3, y + 3, 4, 4, 2, c);
        }
    }
    cv_text(6, 126, &FONT_XS, t, K_DIM);
    cv_text(240 - 6 - text_w(&FONT_XS, ui.seq ? "WHITE KEYS: STEPS  OCT: LANE" : EN_KIT_NAME[proj.par[P_KIT] & 1]), 126, &FONT_XS,
            ui.seq ? "WHITE KEYS: STEPS  OCT: LANE" : EN_KIT_NAME[proj.par[P_KIT] & 1], ui.seq ? K_ACC : K_DIM);
}

/* a button's card */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("GROOVE; AGAIN: GROOVE 2"); L("(DRIFT). + SELECT: TEMPO"); L("SEL + HOME: THE KEYS PLAY"); L("NOTES OR CHORDS"); break;
    case B_ENV: L("THE LEAD; AGAIN: DAYDREAM,"); L("AGAIN: DAYDREAM 2"); L("SEL + ENV: DAYDREAM"); L("ON AND OFF"); break;
    case B_LFO: L("THE COMP;"); L("AGAIN: ITS STYLE"); break;
    case B_EDIT: L("THE BASS"); L("LEVEL, TONE,"); L("PATTERN, SLIDE"); break;
    case B_ARP: L("THE ANSWER: CALL"); L("AND RESPONSE"); L("SEL + ARP: ON AND OFF"); break;
    case B_GLO: L("DUST, DUST 2,"); L("SPACE, GLOBAL"); L("SEL + GLO: REMOTE: THIS"); L("PANEL PLAYS THE BROWSER"); break;
    case B_FX: L("THE KEYS PLAY EFFECTS"); L("SAVE HELD: LOCK THEM ON"); L("AGAIN: NOTES (LOCKS STAY)"); L("SEL + FX: LOCKS OFF"); break;
    case B_REC: L("CHORD HOLD: THE BAND"); L("STAYS ON THIS CHORD"); L("WHILE YOU SOLO"); break;
    case B_SAVE: L("SIDE B: A BREAKDOWN ON THE"); L("NEXT BAR; AGAIN, BACK IN"); L("GLOBAL: STORE THE PRESET"); L("(HELD: THE FACTORY'S BACK)"); break;
    case B_OCTDN: case B_OCTUP: L("THE LEAD'S OCTAVE"); L("SEQUENCER: THE LANE"); break;
    default: break;
    }
#undef L
    return n;
}
static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_ACC);
    cv_rect(16, 34, 208, 6, K_ACC);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k], K_TEXT);
}


/* FX on: the keys' map, laid out as the keyboard: each octave-and-a-bit a row, the black keys above their
 * white ones, what each does on it; the effects held light up */
static void draw_fx_map(void)
{
    static const char *const WN[16] = {"1/4", "1/8", "SKIP", "HALF", "STOP", "SPIN", "MUFF", "RDIO",
                                       "LEAD", "COMP", "BASS", "ANS", "DRUM", "DUST", "ROOM", "ECHO"};
    static const char *const BN[11] = {"WOB", "CRSH", "FILL", "FRZ", "DROP", "LEAD", "COMP", "BASS", "ANS", "DRUM", "DUST"};
    /* each black key sits between two whites: the index of the white to its right */
    static const uint8_t B_RIGHT[11] = {1, 2, 3, 5, 6, 8, 9, 10, 12, 13, 15};
    uint32_t held = ui.keys & ~ui.lock_keys, lk = ui.fx_lock;
    int i, row;
    cv_text(6, 0, &FONT_XS, ui.lock_mode ? "LOCK: TAP KEYS" : "HOLD, OR SAVE+KEY", ui.lock_mode ? K_LOCK : K_ACC);
    cv_round(112, 2, 8, 8, 2, RGB(96, 80, 140));
    cv_text(122, 0, &FONT_XS, "MUTE", K_DIM);
    cv_round(152, 2, 8, 8, 2, RGB(48, 110, 104));
    cv_text(162, 0, &FONT_XS, "SOLO", K_DIM);
    cv_round(192, 2, 8, 8, 2, K_LOCK);
    cv_text(202, 0, &FONT_XS, "LOCK", K_DIM);
    for (row = 0; row < 2; row++) {             /* F3 .. F4, then (F#4) G4 .. G5, shifted to make room */
        int32_t y0 = 16 + row * 64, xw = 27, x0 = row ? 18 : 4;
        for (i = 0; i < 8; i++) {
            int w = row * 8 + i, on = held >> WHITE_KEY[w] & 1u, mute = w >= 8 && w < 14;
            int lock = lk >> fx_bit_of(WHITE_KEY[w]) & 1u;
            int32_t x = x0 + i * xw;
            cv_round(x, y0 + 24, xw - 2, 36, 3, on ? K_ACC : lock ? K_LOCK : mute ? RGB(96, 80, 140) : RGB(214, 206, 196));
            text_c(x + (xw - 2) / 2, y0 + 44, &FONT_XS, WN[w], on || lock ? K_BG : mute ? K_TEXT : K_BG);
        }
        for (i = 0; i < 11; i++) {
            int r = B_RIGHT[i], on = held >> BLACK_KEY[i] & 1u, krow = r == 8 ? 1 : (r - 1) / 8;
            int lock = lk >> fx_bit_of(BLACK_KEY[i]) & 1u;
            int32_t x = x0 + (r - row * 8) * xw - 14;
            if (krow != row)
                continue;
            cv_round(x + 1, y0, 24, 22, 3, on ? K_ACC : lock ? K_LOCK : i >= 5 ? RGB(48, 110, 104) : RGB(92, 84, 110));
            text_c(x + 13, y0 + 6, &FONT_XS, BN[i], on || lock ? K_BG : K_TEXT);
        }
    }
}

static void draw_main(void)
{
    uint32_t h, i;
    int v = ui.view;
    if (ui.frame % 3u)
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_show(0, MAIN_Y);
        }
        return;
    }
    h = hash(2166136261u, (uint32_t)v | (uint32_t)ui.seq << 5 | (uint32_t)ui.lane << 6 | (uint32_t)en_step << 10 |
                              (uint32_t)en_chord << 16 | (uint32_t)en_playing << 20 | (uint32_t)en_held << 21 |
                              (uint32_t)en_dreaming << 22 | (uint32_t)en_auto << 23 | (uint32_t)en_sideb << 24);
    h = hash(h, (uint32_t)(int32_t)en_drift_left);
    h = hash(h, (uint32_t)en_chord_hist[0] | (uint32_t)en_chord_hist[1] << 8 | (uint32_t)proj.par[P_PATTERN] << 16);
    if (v == V_DRUMS || v == V_KIT)
        for (i = 0; i < EN_NLANE; i++)
            h = hash(h, en_pats[proj.par[P_PATTERN]][i]);
    else if (v == V_LEAD || v == V_COMP || v == V_BASS || v == V_ANS)
        for (i = 0; i < EN_NVOX; i++)
            h = hash(h, (uint32_t)en_vtrk[i] << 8 | en_vnote[i]);
    if (v != V_DRUMS && v != V_KIT && v != V_LEAD && v != V_COMP && v != V_BASS && v != V_ANS && en_playing)
        h = hash(h, ui.frame);                  /* the record turns */
    h = hash(h, (uint32_t)(en_glow[T_ANS] > 0.3f) | (uint32_t)proj.par[P_DUST_CHAR] << 2 | (uint32_t)proj.par[algo_param()] << 5);
    h = hash(h, ui.fx ? 0x46580000u ^ ui.keys ^ ui.fx_lock * 7u ^ (uint32_t)ui.lock_mode << 30 : 0u);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (ui.fx) {
        draw_fx_map();
        cv_show(0, MAIN_Y);
        return;
    }
    switch (v) {
    case V_LEAD: draw_voice(T_LEAD); break;
    case V_COMP: case V_CSTYLE: draw_voice(T_COMP); break;
    case V_BASS: draw_voice(T_BASS); break;
    case V_ANS: draw_voice(T_ANS); break;
    case V_DRUMS: case V_KIT: draw_drums(); break;
    default:
        draw_scene(proj.par[P_DUST_CHAR], TAPE_COL[proj.piece % 10u]);
        draw_chord_panel(122);
        break;
    }
    cv_show(0, MAIN_Y);
}

/* the knob strip (X0X's) */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {
        char t[12];
        int k;
        int32_t xd = 237 - NVIEWS * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 6, 3, 4, 4, 2, k == ui.view ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_FILL;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (hi - lo < 8) {
            int n = hi - lo + 1, j, pw = n > 4 ? 5 : 9, gap = n > 4 ? 2 : 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, text_w(&FONT_B, t) > 56 ? &FONT_XS : &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_show(0, KNB_Y);
}

/* the lights: the page, PLAY, REC (hold), FX, SAVE (side B); a track's page button flashes as it plays;
 * the keys: the steps (sequencer), the effects held (FX), else the scale */
static void leds(void)
{
    static const uint8_t TRK_BTN[5] = {B_ENV, B_LFO, B_EDIT, B_ARP, B_SEQ};
    uint32_t b = 0, k = 0, i;
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_held)
        b |= 1u << B_REC;
    if (ui.fx)
        b |= 1u << B_FX;
    if (en_sideb)
        b |= 1u << B_SAVE;
    for (i = 0; i < 5; i++)
        if (en_glow[i] > 0.6f)
            b |= 1u << TRK_BTN[i];
    if (ui.seq) {
        uint16_t m = en_pats[proj.par[P_PATTERN]][ui.lane];
        for (i = 0; i < 16; i++)
            if ((m >> i & 1u) ^ (en_playing && i == en_step))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.fx) {
        k = ui.keys;
    } else if (proj.leds) {
        uint32_t pool = EN_SCALE_POOL[proj.par[P_SCALE] % EN_NSCALES], r = (uint32_t)proj.par[P_ROOT];
        for (i = 0; i < NKEYS; i++)
            if (pool >> (((5u + i) + 12u - r) % 12u) & 1u)
                k |= 1u << i;
        k |= ui.lead_keys;
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* the LOF-1 card: big 5 x 7 pixel letters, lamp light over a rust shadow; a record's worth of steps */
static const uint8_t BIG_GLYPH[5][7] = {
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},  /* L */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  /* O */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  /* F */
    {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00},  /* - */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* 1 */
};
#define SPLASH_STEP_Y 184
static void draw_card(void)
{
    int g, r, c, pass;
    crt_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 144, K_BG);
    for (pass = 0; pass < 2; pass++)
        for (g = 0; g < 5; g++)
            for (r = 0; r < 7; r++)
                for (c = 0; c < 5; c++)
                    if (BIG_GLYPH[g][r] >> (4 - c) & 1u)
                        cv_rect(18 + (g * 6 + c) * 7 + (pass ? 0 : 3), 10 + r * 7 + (pass ? 0 : 3), 7, 7,
                                pass ? K_ACC : K_RUST);
    text_c(120, 76, &FONT_S, "YOU PLAY, THE BAND FOLLOWS", K_TEXT);
    text_c(120, 102, &FONT_XS, EN_VERSION[0] == 'D' ? "EXPERIMENTAL" : "EXPERIMENTAL  //  " EN_VERSION, K_RINGS);
    cv_show(0, 30);
    for (c = 0; c < 16; c++)
        crt_fill(8 + c * 14, SPLASH_STEP_Y, 11, 11, K_PANEL);
}

static void splash_tick(void)
{
    uint32_t t = plat_ms() - ui.boot_t, t1 = ui_splash_ms * 17u / 32u, n, k = 0, i;
    if (ui.splash == 1) {
        if (t < t1)
            return;
        ui.splash = 2;
        draw_card();
    }
    n = (t - t1) * 17u / (ui_splash_ms - t1 + 1u);
    if (n > 16u)
        n = 16u;
    for (; ui.splash_step < n; ui.splash_step++)
        crt_fill(8 + ui.splash_step * 14, SPLASH_STEP_Y, 11, 11, ui.splash_step % 4u ? K_TEAL : K_ACC);
    for (i = 0; i < n; i++)
        k |= 1u << WHITE_KEY[i];
    plat_glow(1);
    plat_leds(n ? 1u << B_PLAY : 0u, k);
}

/* the screensaver: a minute with nobody touching it and Red leans on the tree with a coffee, nodding to
 * the beat (down on each beat, up between). Only the head box changes as he nods. A button or a knob wakes
 * it and does nothing else; a key (or a MIDI note) wakes it and plays. */
#include "saver_art.h"

static void sv_decode(const uint8_t *s, uint32_t n_px, uint32_t W, uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh,
                      uint32_t dx, uint32_t dy, uint32_t stride)
{   /* the pixels of a run-length coded image W wide that fall in the rectangle rx, ry, rw x rh, into the
     * canvas at dx, dy (the canvas stride wide) */
    uint32_t p = 0, end = (ry + rh) * W;
    if (end > n_px)
        end = n_px;
    while (p < end) {
        uint32_t b = *s++, cnt = 1u;
        uint16_t c;
        if (b & 0x80u) {
            cnt = (b & 0x7Fu) + 2u;
            b = *s++;
        }
        c = SV_PAL[b & 0x7Fu];
        while (cnt) {
            uint32_t x = p % W, y = p / W, take = W - x, a, e;
            if (take > cnt)
                take = cnt;
            if (y >= ry && y < ry + rh) {
                a = x > rx ? x : rx;
                e = x + take < rx + rw ? x + take : rx + rw;
                for (; a < e; a++)
                    cv_px[(dy + y - ry) * stride + dx + a - rx] = c;
            }
            p += take;
            cnt -= take;
        }
    }
}

/* screen rows y0 .. y1-1 of the saver, as it is now (the pose on screen: ui.sv_shown) */
#define SV_SPARE 8                               /* rows above and below for the warp to read */
static void sv_rows(int32_t y0, int32_t y1)
{
    if (y0 < 0)
        y0 = 0;
    if (y1 > 240)
        y1 = 240;
    while (y0 < y1) {
        int32_t d1 = y1 - y0 > 128 ? y0 + 128 : y1, s0 = y0 - SV_SPARE, s1 = d1 + SV_SPARE, h0, h1;
        if (s0 < 0)
            s0 = 0;
        if (s1 > 240)
            s1 = 240;
        cv_begin(240, (uint32_t)(s1 - s0), 0);
        sv_decode(SV_SCENE, 240u * 240u, 240u, 0, (uint32_t)s0, 240, (uint32_t)(s1 - s0), 0, 0, 240);
        h0 = s0 > SV_HY ? s0 : SV_HY;            /* the nodding head, where it overlaps */
        h1 = s1 < SV_HY + SV_HH ? s1 : SV_HY + SV_HH;
        if (ui.sv_shown && h0 < h1)
            sv_decode(ui.sv_shown == 1 ? SV_HEAD2 : SV_HEAD3, SV_HW * SV_HH, SV_HW, 0, (uint32_t)(h0 - SV_HY), SV_HW,
                      (uint32_t)(h1 - h0), SV_HX, (uint32_t)(h0 - s0), 240);
        crt_out(0, 240, (uint32_t)y0, (uint32_t)(d1 - y0), cv_px, (uint32_t)s0, (uint32_t)(s1 - s0), 0);
        y0 = d1;
    }
}

static int sv_pose(void)
{
    uint32_t beat, half;
    if (en_playing) {
        beat = en_bar * 4u + en_step / 4u;
        half = (en_step & 3u) >= 2u;
    } else {                                     /* stopped: a slow nod of his own */
        uint32_t t = plat_ms() / 700u;
        beat = t / 2u;
        half = t & 1u;
    }
    return !half ? 1 : (beat & 1u) ? 2 : 0;
}

static void sv_enter(void)
{
    ui.saver = 1;
    ui.sv_t = ui.input_t;
    ui.help = 0;
    ui.sv_shown = 0;
    sv_rows(0, 240);
}

static void sv_leave(void)
{
    ui.saver = 0;
    crt_fill(0, 0, 240, 240, K_BG);
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;       /* everything drawn again */
}

/* the roll, drifting down the screen (about 20 s a pass, a pause between): what it passes is drawn again */
static void crt_tick(void)
{
    int32_t lo, hi, y = (int32_t)(plat_ms() / 160u * 2u % (240u + 2u * CRT_ROLL_H + 60u)) - CRT_ROLL_H;
    if (!crt_roll_to(y, &lo, &hi))
        return;
    if (rm.on)
        rm_draw();
    else if (ui.saver)
        sv_rows(lo, hi + 1);
    else {
        if (lo < HDR_H)
            ui.sig[0] = 0;
        if (hi >= MAIN_Y && lo < MAIN_Y + MAIN_H)
            ui.sig[1] = 0;
        if (hi >= KNB_Y)
            ui.sig[2] = 0;
    }
}

/* the saver up: a button pressed now only wakes it (no edge for input(), its release does nothing);
 * the knobs' detents go nowhere. Keys and MIDI pass through to input(). */
static void sv_swallow(void)
{
    uint32_t btn = plat_buttons(), now = plat_ms(), fresh = btn & ~ui.btn, i;
    int r, woke = 0;
    for (r = 0; r < NE; r++)
        if (plat_enc(r))
            woke = 1;
    if (fresh) {
        ui.btn_used |= fresh;
        ui.save_done = 1;
        for (i = 0; i < NB; i++)
            if (fresh >> i & 1u)
                ui.down_t[i] = now;
        woke = 1;
    }
    ui.btn = btn;
    if (woke)
        ui.input_t = now;
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.boot_t = plat_ms();
    ui.input_t = ui.boot_t;
    ui.splash = ui_splash_ms > 0u;
    if (ui.splash) {
        draw_splash();
    } else {
        crt_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);
    }
}

void ui_frame(void)
{
    if (rm.on) {                                 /* remote: the browser's screen, the browser's lights */
        crt_tick();
        rm_tick();
        if (rm.on) {
            plat_glow(1);
            plat_leds(rm.lit_b, rm.lit_k);
            return;
        }
    }
    if (en_pattern != (uint8_t)proj.par[P_PATTERN] && en_playing)   /* Drift changed the drums: follow it */
        proj.par[P_PATTERN] = (int16_t)en_pattern;
    if (ui.splash) {
        uint32_t held = plat_buttons() | plat_keys();
        if (plat_ms() - ui.boot_t < ui_splash_ms && !held) {
            splash_tick();
            return;
        }
        ui.splash = 0;
        ui.btn = plat_buttons();
        ui.keys = plat_keys();
        ui.btn_used = ui.btn;
        ui.input_t = plat_ms();
        crt_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);
    }
    input();
    crt_tick();
    if (ui.saver && ui.input_t != ui.sv_t)
        sv_leave();
    else if (!ui.saver && ui_saver_ms && plat_ms() - ui.input_t >= ui_saver_ms)
        sv_enter();
    if (ui.saver) {
        int f = sv_pose();
        if (f != ui.sv_shown) {
            ui.sv_shown = (uint8_t)f;
            sv_rows(SV_HY, SV_HY + SV_HH);
        }
        leds();
        ui.frame++;
        return;
    }
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
