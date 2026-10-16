/* SPDX-License-Identifier: GPL-3.0-only */
/* GRAV-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too. The panel's handling (knobs, help cards) is AMB-1's, by way of ECO-1.
 *
 *   keys           the scale: each key adds or removes its note (the root stays). On PULSE the white keys
 *                  are the lane's 16 steps and black keys 1-6 pick the lane; on WEATHER black keys 1-3 call
 *                  a dust cloud, an asteroid, a comet
 *   PLAY           grow / rest                   EDIT   the Great Year now: the chord moves
 *   REC            ECLIPSE: the orbits and the weather hold, the space rings on
 *   OCT- / OCT+    the selected planet an octave down / up
 *   SELECT         every page in turn: System, Star, each planet, Gravity, Weather, Pulse, Space, Setup,
 *                  Display, MIDI out (with HOME held: the tempo)
 *   ALGORITHM      the planet (on PULSE: the lane)     PRESETS  the system (ten)
 *   HOME ENV ARP LFO SEQ FX GLO   the pages: SYSTEM, STAR, PLANET, GRAVITY (again: WEATHER), PULSE, SPACE,
 *                  SETUP (again: DISPLAY, then MIDI OUT); SEL the next planet (on PULSE the next lane)
 *   KNOB 1-4       the page's four values (on STAR and each PLANET, KNOB 1 is its level)
 *   SAVE           on the Setup pages: saves all ten systems; elsewhere: starts the system again;
 *                  held 1.5 s: the system back to its recipe */
#include "../dsp/fastmath.h"

/* the pages; SELECT walks them, a planet's page once for each planet */
enum { V_SYSTEM, V_STAR, V_PLANET, V_GRAVITY, V_WEATHER, V_PULSE, V_SPACE, V_SETUP, V_DISPLAY, V_OUT, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"System", "Star", "Planet", "Gravity", "Weather", "Pulse", "Space", "Setup",
                                              "Display", "MIDI out"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ARP, B_LFO, B_LFO, B_SEQ, B_FX, B_GLO, B_GLO, B_GLO};
enum { K_LEDS = 200, K_CPU, K_LANE, K_NONE };
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Planets", "Tempo", "Wander", "Phrase"},
    {"Level", "Colour", "Roar", "Flares"},
    {"Level", "Instrument", "Ellipse", "Octave"},
    {"Tide", "Reach", "Wake", "Great yr"},
    {"Dust", "Asteroids", "Comets", "Events"},
    {"Lane", "Binding", "Drums", "Evolve"},
    {"Echo", "Reverb", "Radio", "Width"},
    {"Root", "Mode", "Tune", "Sync"},
    {"Theme", "Starfield", "Orbits", "Tide lines"},
    {"Sends", "Channels", "Lights", "CPU"},
};

static struct {
    uint8_t view, sel, lane;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];
    uint32_t down_t[NB];
    uint32_t turn_t;
    uint8_t help;
    uint8_t reset_done;
    uint8_t dirty;
    uint32_t act_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];
    uint32_t boot_t;
    uint8_t splash, splash_step;
    uint32_t ev_r;                     /* the engine's events read so far */
    uint16_t pat_seen[EN_NPL];         /* the drums' steps as the engine last had them */
    uint32_t pat_hold;                 /* a step just changed here: the engine's copy catches up first */
    float trail[20][2];                /* the comet's trail */
    uint8_t trail_n;
} ui;
uint32_t ui_splash_ms = 3200u;         /* the boot cards: RedtailWorks, then GRAV-1 (the host's tests set 0) */

/* ------------------------------------------------------------- themes --- */
/* the screen's colours: Deep space (gold and ice), Nebula, Aurora, Red planet, Phosphor, Redtail (HYP-1's
 * purple and orange), Mono. The planets and the star keep their own colours under every theme. */
typedef struct {
    uint16_t bg, panel, line, dim, text, acc, cold, sky;
} theme_t;
static const theme_t THEMES[EN_NTHEMES] = {
    {RGB(4, 6, 11), RGB(22, 26, 38), RGB(44, 50, 70), RGB(140, 147, 170), RGB(228, 231, 240), RGB(242, 201, 107), RGB(156, 195, 255), RGB(205, 214, 255)},
    {RGB(12, 6, 20), RGB(34, 20, 48), RGB(62, 44, 84), RGB(154, 134, 173), RGB(239, 228, 247), RGB(255, 143, 184), RGB(155, 140, 255), RGB(230, 200, 255)},
    {RGB(2, 13, 12), RGB(16, 36, 34), RGB(34, 66, 62), RGB(127, 166, 156), RGB(226, 245, 239), RGB(127, 240, 192), RGB(79, 214, 224), RGB(200, 255, 240)},
    {RGB(16, 6, 5), RGB(40, 20, 16), RGB(74, 44, 36), RGB(165, 138, 125), RGB(245, 230, 221), RGB(255, 154, 74), RGB(210, 96, 60), RGB(255, 217, 196)},
    {RGB(1, 8, 2), RGB(10, 30, 14), RGB(24, 64, 32), RGB(63, 154, 82), RGB(168, 255, 181), RGB(198, 255, 207), RGB(63, 191, 95), RGB(125, 255, 154)},
    {RGB(20, 19, 24), RGB(42, 37, 52), RGB(62, 58, 76), RGB(163, 156, 179), RGB(236, 232, 242), RGB(255, 128, 32), RGB(164, 76, 255), RGB(217, 200, 242)},
    {RGB(0, 0, 0), RGB(30, 30, 30), RGB(60, 60, 60), RGB(138, 138, 138), RGB(240, 240, 240), RGB(255, 255, 255), RGB(150, 150, 150), RGB(190, 190, 190)},
};
static const theme_t *th(void) { return &THEMES[(unsigned)proj.par[P_THEME] < EN_NTHEMES ? proj.par[P_THEME] : 0]; }
#define K_BG (th()->bg)
#define K_PANEL (th()->panel)
#define K_LINE (th()->line)
#define K_DIM (th()->dim)
#define K_TEXT (th()->text)
#define K_ACC (th()->acc)
#define K_COLD (th()->cold)
#define K_SKY (th()->sky)
#define K_ECLIPSE RGB(255, 138, 76)
/* the star's colours: its noise's */
static const uint16_t STAR_C[EN_NCOLOUR] = {RGB(242, 245, 255), RGB(255, 156, 200), RGB(164, 89, 58), RGB(141, 179, 255),
                                            RGB(179, 140, 255), RGB(182, 186, 196), RGB(0, 0, 0), RGB(255, 213, 106)};

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1500u;
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

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}
static int nplanets(void) { return proj.par[P_PLANETS]; }
static int lane_hits(int l)
{
    uint32_t b = (uint16_t)proj.par[P_PAT + l];
    int n = 0;
    while (b) {
        n += (int)(b & 1u);
        b >>= 1;
    }
    return n;
}

/* ------------------------------------------------------------ values --- */
static int view_knob(int v, int i)
{
    static const uint8_t K[NVIEWS][4] = {
        {P_PLANETS, P_TEMPO, P_WANDER, P_PHRASE}, {P_STAR, P_COLOUR, P_ROAR, P_FLARES}, {0, 0, 0, 0},
        {P_TIDE, P_REACH, P_WAKE, P_GY}, {P_DUST, P_ROCKS, P_COMETS, P_EVENTS}, {0, 0, P_DRUMS, P_EVOLVE},
        {P_ECHO, P_REVERB, P_RADIO, P_WIDTH}, {P_ROOT, P_MODE, P_TUNE, P_SYNC}, {P_THEME, P_SKY, P_LINES, P_TLINES},
        {P_MIDI, P_MCH, 0, 0}};
    if (v == V_PLANET)
        return P_PL + 4 * ui.sel + i;
    if (v == V_PULSE && i < 2)
        return i == 0 ? K_LANE : P_BIND + ui.lane;
    if (v == V_OUT && i >= 2)
        return i == 2 ? K_LEDS : K_CPU;
    return K[v][i];
}
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_LANE)
        return ui.lane;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : k == K_LANE ? EN_NPL - 1 : 0;
    }
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_LANE) {
        const char *n = EN_LANE_NAME[v];
        *b++ = (char)('1' + v);
        *b++ = ' ';
        while ((*b++ = *n++))
            ;
    } else {
        b[0] = 0;
    }
}

static void set_mode(int m);
static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k == P_MODE) {
        set_mode(v);
        return;
    }
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
        if (k == P_PLANETS && ui.sel >= v)
            ui.sel = (uint8_t)(v - 1);
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_LANE) {
        ui.lane = (uint8_t)v;
        return;
    }
    mark_dirty();
}

static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
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

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        say("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        say("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void sync_pats(void)                    /* the engine's steps into the project: what Evolve changed */
{
    int l;
    for (l = 0; l < EN_NPL; l++)
        ui.pat_seen[l] = (uint16_t)proj.par[P_PAT + l];
}

static void set_piece(int i)
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    sync_pats();
    ui.pat_hold = plat_ms() + 200u;
    if (ui.sel >= nplanets())
        ui.sel = 0;
    {
        char t[16];
        t[0] = (char)('0' + (proj.piece + 1) / 10);
        t[1] = (char)('0' + (proj.piece + 1) % 10);
        t[2] = 0;
        say(t, piece_name(proj.piece));
    }
    mark_dirty();
}

/* ------------------------------------------------------------ the walk --- */
/* SELECT's list: System, Star, Planet 1..n, Gravity, Weather, Pulse, Space, Setup, Display, MIDI out */
static int walk_len(void) { return NVIEWS - 1 + nplanets(); }
static int walk_pos(void)
{
    if (ui.view < V_PLANET)
        return ui.view;
    if (ui.view == V_PLANET)
        return V_PLANET + ui.sel;
    return ui.view - 1 + nplanets();
}
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}
static void walk_to(int k)
{
    int n = nplanets();
    if (k < V_PLANET)
        set_view(k);
    else if (k < V_PLANET + n) {
        ui.sel = (uint8_t)(k - V_PLANET);
        set_view(V_PLANET);
    } else
        set_view(k - n + 1);
}
static void focus_step(int d)                  /* ALGORITHM, SEL: the planet; on Pulse, the lane */
{
    if (ui.view == V_PULSE) {
        ui.lane = (uint8_t)((ui.lane + d + EN_NPL) % EN_NPL);
        return;
    }
    ui.sel = (uint8_t)((ui.sel + d + nplanets()) % nplanets());
    if (ui.view != V_PLANET)
        set_view(V_PLANET);
}

/* ------------------------------------------------------------ buttons --- */
static void button(int b)
{
    int v;
    if (b == B_GLO && (ui.view == V_SETUP || ui.view == V_DISPLAY)) {   /* GLO again: Display, then MIDI out */
        set_view(ui.view + 1);
        return;
    }
    if (b == B_LFO && ui.view == V_GRAVITY) {    /* LFO again: Weather */
        set_view(V_WEATHER);
        return;
    }
    for (v = 0; v < NVIEWS; v++)
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_SEL:
        focus_step(1);
        break;
    case B_EDIT:
        en_touch();
        say("GREAT YEAR", en_playing ? "NOW" : "(RESTING)");
        break;
    case B_PLAY:
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("ECLIPSE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);
        break;
    case B_OCTDN: case B_OCTUP: {
        int p = P_PL_OCT(ui.sel);
        char t[8], h[12] = "PLANET ";
        knob_set(p, proj.par[p] + (b == B_OCTUP ? 1 : -1));
        en_param_text(p, proj.par[p], t);
        h[7] = (char)('1' + ui.sel);
        h[8] = 0;
        say(h, t);
        break;
    }
    case B_SAVE:
        if (ui.view >= V_SETUP) {                /* the Setup pages: saved */
            if (project_save() == 0) {
                ui.dirty = 0;
                say("SAVED", "ALL TEN");
            } else {
                say("NOT SAVED", "FLASH ERROR");
            }
        } else {                                 /* elsewhere: the system starts again */
            en_seed(plat_ms());
            en_regrow();
            say("FROM THE START", piece_name(proj.piece));
        }
        break;
    default: break;
    }
}

/* the keys: the scale; on Pulse the steps and lanes; on Weather the events */
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int white_of(int k) { int i; for (i = 0; i < 16; i++) if (WHITE_KEY[i] == k) return i; return -1; }
static int black_of(int k) { int i; for (i = 0; i < 11; i++) if (BLACK_KEY[i] == k) return i; return -1; }
static void key(int k)
{
    int w = white_of(k), bk = black_of(k);
    if (ui.view == V_PULSE) {
        if (w >= 0) {
            int p = P_PAT + ui.lane;
            uint16_t bits = (uint16_t)proj.par[p] ^ (uint16_t)(1u << w);
            proj.par[p] = (int16_t)bits;
            en_set(p, proj.par[p]);
            ui.pat_seen[ui.lane] = bits;
            ui.pat_hold = plat_ms() + 200u;
            mark_dirty();
        } else if (bk >= 0 && bk < EN_NPL) {
            ui.lane = (uint8_t)bk;
            say("LANE", EN_LANE_NAME[bk]);
        }
        return;
    }
    if (ui.view == V_WEATHER && bk >= 0 && bk < 3) {
        static const char *const W[3] = {"DUST CLOUD", "ASTEROID", "COMET"};
        en_summon(bk);
        say(W[bk], "CALLED");
        return;
    }
    toggle_note((5 + k) % 12);
}

/* MIDI in: notes add or remove their note from the scale; the clock */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
    }
}

#define HELP_HOLD 900u
#define RESET_HOLD 1500u
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
        if (btn == 1u << i && i != B_PLAY && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if ((ch & keys) >> i & 1u)
            key((int)i);
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the walk; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_SYSTEM) {
                ui.touched = 1;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[12];
                en_param_text(P_TEMPO, proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int k = walk_pos() + (e > 0 ? 1 : -1);
            if (k >= 0 && k < walk_len())
                walk_to(k);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        focus_step(e > 0 ? 1 : -1);
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        sync_pats();
        ui.pat_hold = plat_ms() + 200u;
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, view_knob(ui.view, (int)i), e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* the engine's news: Evolve's steps into the project, its events as messages */
static void engine_news(void)
{
    int l;
    if ((int32_t)(plat_ms() - ui.pat_hold) >= 0)
        for (l = 0; l < EN_NPL; l++) {
            uint16_t e = en_pat[l];
            if (e != ui.pat_seen[l]) {
                ui.pat_seen[l] = e;
                proj.par[P_PAT + l] = (int16_t)e;
            }
        }
    while (ui.ev_r != en_ev_w) {
        uint16_t ev;
        char t[12] = "PLANET ";
        if (en_ev_w - ui.ev_r > EN_NEV)
            ui.ev_r = en_ev_w - EN_NEV;
        ev = en_ev[ui.ev_r % EN_NEV];
        ui.ev_r++;
        t[7] = (char)('1' + (ev & 0xFFu) % 6u);
        t[8] = 0;
        switch (ev >> 8) {
        case EV_YEAR:                            /* only the planet on the page, or messages never stop */
            if (ui.view == V_PLANET && (ev & 0xFFu) == ui.sel && plat_ms() >= ui.msg_until)
                say(t, "NEW YEAR");
            break;
        case EV_GREAT: say("GREAT YEAR", EN_NOTE_NAME[(ev & 0xFFu) % 12u]); break;
        case EV_DUST: say("DUST CLOUD", 0); break;
        case EV_CLEAR: say("DUST", "CLEARS"); break;
        case EV_ROCK: say("ASTEROID", "CROSSING"); break;
        case EV_COMET: say("COMET", "FALLING"); ui.trail_n = 0; break;
        case EV_BURN: say("COMET", "BURNS UP"); break;
        case EV_WAKE: say(t, "WAKES"); break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the system (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

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

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

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
/* a ring of radius r, its strength 0..1 */
static void ring_px(float cx, float cy, float r, uint16_t c, float s)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 2.0f), x1 = (int32_t)(cx + r + 2.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 2.0f), y1 = (int32_t)(cy + r + 2.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            float a = fm_clampf(1.0f - fm_fabsf(d - r), 0.0f, 1.0f) * s;
            px_blend(x, y, c, (uint32_t)(a * 16.0f + 0.5f));
        }
}
/* a glow: c fading out from r0 to r1 */
static void glow(float cx, float cy, float r0, float r1, uint16_t c, float s)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r1), x1 = (int32_t)(cx + r1 + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r1), y1 = (int32_t)(cy + r1 + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy), a;
            if (d >= r1)
                continue;
            a = d <= r0 ? 1.0f : 1.0f - (d - r0) / (r1 - r0);
            px_blend(x, y, c, (uint32_t)(a * a * s * 16.0f + 0.5f));
        }
}

static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float thk = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - thk, rm = ro - thk * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
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
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, thk * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, thk * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, thk * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, thk * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, thk * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)
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

/* the planets' colours by their warmth, under every theme: molten near the star, then desert, temperate,
 * ocean, and ice far out */
static uint16_t heat(float d)
{
    static const uint16_t H[6] = {RGB(255, 74, 46), RGB(255, 154, 60), RGB(242, 211, 107), RGB(127, 209, 139),
                                  RGB(79, 168, 255), RGB(220, 239, 255)};
    static const float AT[6] = {0.0f, 0.22f, 0.42f, 0.58f, 0.76f, 1.0f};
    int k;
    d = fm_clampf(d, 0.0f, 1.0f);
    for (k = 1; k < 6; k++)
        if (d <= AT[k])
            return mix(H[k - 1], H[k], q16((d - AT[k - 1]) / (AT[k] - AT[k - 1])));
    return H[5];
}

/* the header: the system in a tag, its root and mode; at the right the tempo (EXT on the clock), a dot
 * while unsaved, and the state: six dots (the planets, lit as they play), resting, or ECLIPSE */
static void draw_header(void)
{
    int msg = plat_ms() < ui.msg_until, l;
    uint32_t h = hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                      en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_frozen << 6 | (uint32_t)proj.par[P_MODE] << 8 |
                          (uint32_t)proj.par[P_THEME] << 12 | (uint32_t)en_ext << 20 | (uint32_t)proj.par[P_TEMPO] << 21);
    if (en_ext)
        h = hash(h, (uint32_t)(en_bpm + 0.5f));
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    for (l = 0; l < EN_NPL; l++)
        h = hash(h, (uint32_t)(q16(en_pl_glow[l]) / 4));
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 234, w;
        char c[16];
        if (ui.dirty) {
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            c[0] = 'E', c[1] = 'X', c[2] = 'T', c[3] = ' ';
            itoa_u((uint32_t)(en_bpm + 0.5f), c + 4);
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], c);
        }
        x -= text_w(&FONT_XS, c);
        cv_text(x, 8, &FONT_XS, c, en_ext ? K_ACC : K_DIM);
        x -= 8;
        if (en_frozen) {
            x -= text_w(&FONT_XS, "ECLIPSE");
            cv_text(x, 8, &FONT_XS, "ECLIPSE", K_ECLIPSE);
            x -= 6;
        } else if (en_playing) {
            for (l = nplanets() - 1; l >= 0; l--) {
                x -= 7;
                cv_round(x, 11, 5, 5, 2, mix(K_PANEL, heat(en_pl_dist[l]), 4 + q16(en_pl_glow[l]) * 3 / 4));
            }
        } else {
            int32_t j;
            x -= 10;
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;
        cv_round(4, 4, w, 20, 6, K_ACC);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
        cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
    }
    cv_blit(0, 0);
}

/* ---------------------------------------------------------- the system --- */
/* the orbit plane seen from a little above: x as it is, y flattened; the star in the middle of the band */
#define SCX 120.0f
#define SCY 74.0f
static float zoom = 1.4f;                       /* the view follows the system: its outermost orbit near the edge */
static void proj_xy(float x, float y, float *sx, float *sy)
{
    *sx = SCX + x * 106.0f * zoom;
    *sy = SCY + y * 62.0f * zoom;
}
static float cos_t(float t) { return fm_sin_turns(t + 0.25f); }
static float orbit_r(int i, float t)
{
    float e = (float)proj.par[P_PL_ECC(i)] * 0.01f;
    return en_pl_a[i] * (1.0f - e * e) / (1.0f + e * cos_t(t - en_pl_om[i]));
}

#define NSKY 60
static uint8_t sky_x[NSKY], sky_y[NSKY], sky_b[NSKY];
static void sky_init(void)
{
    uint32_t s = 0x2545F491u;
    int i;
    for (i = 0; i < NSKY; i++) {
        s = s * 1664525u + 1013904223u;
        sky_x[i] = (uint8_t)(s >> 24) % 240u;
        sky_y[i] = (uint8_t)((s >> 16) % (uint32_t)MAIN_H);
        sky_b[i] = (uint8_t)(3u + (s >> 8) % 9u);
    }
}

static void draw_star(void)
{
    int k = proj.par[P_COLOUR];
    float r = 3.0f + en_star * 6.5f + en_flare * 2.0f;
    if (k == 6) {                                /* the black hole: a dark disc, its accretion ring, sparks */
        int i, j;
        glow(SCX, SCY, r * 0.8f, r * 3.0f, RGB(255, 170, 90), 0.22f);
        for (j = 0; j < 3; j++) {
            float rx = r * (1.7f + 0.35f * (float)j), ry = r * (0.5f + 0.12f * (float)j);
            uint16_t c = RGB(255, 190 - j * 30, 110 - j * 25);
            for (i = 0; i < 96; i++) {
                float t = (float)i / 96.0f;
                px_blend((int32_t)(SCX + rx * cos_t(t)), (int32_t)(SCY + ry * fm_sin_turns(t)), c, (uint32_t)(11 - 3 * j));
            }
        }
        disc(SCX, SCY, r, RGB(0, 0, 0));
        ring_px(SCX, SCY, r, RGB(255, 200, 140), 0.8f);
        if (en_playing && !en_frozen && (ui.frame / 3u) % 3u == 0u) {
            float t = (float)((ui.frame * 37u) % 100u) * 0.01f;
            px_blend((int32_t)(SCX + r * 1.8f * cos_t(t)), (int32_t)(SCY + r * 0.55f * fm_sin_turns(t)), RGB(255, 230, 190), 16);
        }
        return;
    }
    {
        uint16_t c = STAR_C[(unsigned)k < EN_NCOLOUR ? k : 0];
        int dwarf = k == 2, i;
        glow(SCX, SCY, r * 0.6f, r * (dwarf ? 2.2f : 3.2f), c, dwarf ? 0.3f : 0.45f);
        if (en_flare > 0.05f)                    /* a flare: rays */
            for (i = 0; i < 10; i++) {
                float t = (float)i / 10.0f + (float)ui.frame * 0.002f, l = r * (1.4f + en_flare * 1.8f);
                cv_line((int32_t)(SCX + r * cos_t(t)), (int32_t)(SCY + r * fm_sin_turns(t)),
                        (int32_t)(SCX + l * cos_t(t)), (int32_t)(SCY + l * fm_sin_turns(t)), mix(K_BG, c, q16(en_flare * 0.8f)));
            }
        disc(SCX, SCY, r, mix(c, RGB(0, 0, 0), dwarf ? 7 : 3));
        disc(SCX - r * 0.15f, SCY - r * 0.15f, r * 0.62f, c);
        disc(SCX - r * 0.25f, SCY - r * 0.25f, r * 0.25f, dwarf ? RGB(224, 160, 124) : RGB(255, 255, 255));
    }
}

static void draw_planet(int i)
{
    float sx, sy, lv = (float)proj.par[P_PL_LVL(i)] * 0.01f, depth = 0.85f + 0.15f * (en_pl_y[i] / 0.75f + 1.0f);
    float s = (1.3f + lv * 3.6f) * depth * (1.0f + 0.35f * en_pl_glow[i]), ts = en_pl_tide[i];
    int dorm = lane_hits(i) == 0, hl = (ui.view == V_PLANET && ui.sel == i) || (ui.view == V_PULSE && ui.lane == i);
    uint16_t c = heat(en_pl_dist[i]);
    proj_xy(en_pl_x[i], en_pl_y[i], &sx, &sy);
    if (ts > 0.04f)
        ring_px(sx, sy, s + 2.0f + ts * 6.0f, c, fm_minf(0.8f, ts));
    if (dorm && !en_pl_awake[i]) {
        ring_px(sx, sy, s, c, 0.8f);
    } else {
        disc(sx, sy, s, dorm ? mix(K_BG, c, 6 + q16(ts * 2.0f) * 10 / 16) : c);
    }
    if (hl) {
        int k;
        for (k = 0; k < 16; k += 2) {            /* a dashed ring */
            float t = (float)k / 16.0f;
            px_blend((int32_t)(sx + (s + 4.0f) * cos_t(t)), (int32_t)(sy + (s + 4.0f) * fm_sin_turns(t)), K_ACC, 16);
            px_blend((int32_t)(sx + (s + 4.0f) * cos_t(t + 0.02f)), (int32_t)(sy + (s + 4.0f) * fm_sin_turns(t + 0.02f)), K_ACC, 12);
        }
    }
    if (ui.view == V_PLANET || ui.view == V_PULSE || hl) {
        char t[2] = {(char)('1' + i), 0};
        cv_text((int32_t)(sx + s + 2.0f), (int32_t)(sy - s - 12.0f), &FONT_XS, t, hl ? K_ACC : K_DIM);
    }
}

static void draw_system(void)
{
    const int n = nplanets();
    const float sky = (float)proj.par[P_SKY] * 0.01f, lines = (float)proj.par[P_LINES] * 0.01f;
    const float tl = (float)proj.par[P_TLINES] * 0.01f;
    uint16_t bg = K_BG;
    int i, k, order[EN_NPL];
    {   /* the zoom: the farthest any orbit swings, kept inside the band */
        float far = 0.2f, z;
        for (i = 0; i < n; i++) {
            float e = (float)proj.par[P_PL_ECC(i)] * 0.01f, apo = en_pl_a[i] * (1.0f + e);
            if (apo > far)
                far = apo;
        }
        z = fm_clampf(0.97f / far, 1.0f, 2.6f);
        zoom += (z - zoom) * 0.03f;
    }
    if (en_dust > 0.01f)                         /* a dust cloud: the sky goes brown */
        bg = mix(bg, RGB(120, 86, 54), q16(en_dust * 0.3f));
    cv_begin(240, MAIN_H, bg);
    if (sky > 0.0f)
        for (k = 0; k < NSKY; k++) {
            int tw = (int)((ui.frame / 3u + (uint32_t)k * 7u) % 23u);
            px_blend(sky_x[k], sky_y[k], K_SKY, (uint32_t)((float)(sky_b[k] - (tw < 3 ? 2 : 0)) * sky + 0.5f));
        }
    {   /* the Great Year: a thin line along the top */
        int32_t w = (int32_t)(en_year * 232.0f);
        cv_rect(4, 1, 232, 1, mix(bg, K_COLD, 3));
        cv_rect(4, 1, w, 1, mix(bg, K_COLD, 12));
    }
    if (lines > 0.0f)                            /* the orbits, and the points the phrases sound at */
        for (i = 0; i < n; i++) {
            int dorm = lane_hits(i) == 0, hl = (ui.view == V_PLANET && ui.sel == i) || (ui.view == V_PULSE && ui.lane == i);
            uint16_t c = hl ? mix(bg, K_ACC, 9) : mix(bg, K_LINE, q16(lines));
            float px = 0.0f, py = 0.0f;
            for (k = 0; k <= 48; k++) {
                float t = (float)k / 48.0f, r = orbit_r(i, t), x, y;
                proj_xy(r * cos_t(t), r * fm_sin_turns(t), &x, &y);
                if (k && (!dorm || (k & 1)))
                    cv_line((int32_t)px, (int32_t)py, (int32_t)x, (int32_t)y, c);
                px = x, py = y;
            }
            if (!dorm) {
                int m = en_pl_notes[i];
                for (k = 0; k < m; k++) {
                    float t = (float)k / (float)m, r = orbit_r(i, t), x, y;
                    proj_xy(r * cos_t(t), r * fm_sin_turns(t), &x, &y);
                    cv_rect((int32_t)x, (int32_t)y, 2, 2, mix(bg, K_DIM, q16(lines * 0.8f)));
                }
            }
        }
    if (tl > 0.0f)                               /* the tides: a line from who swells a planet */
        for (i = 0; i < n; i++) {
            int j = en_pl_link[i];
            float a = fm_minf(0.6f, en_pl_tide[i] * en_pl_linkc[i] * 2.2f) * tl, x0, y0, x1, y1;
            if (j < 0 || a < 0.04f)
                continue;
            if (j == EN_NPL)
                proj_xy(en_rock_x, en_rock_y, &x0, &y0);
            else
                proj_xy(en_pl_x[j], en_pl_y[j], &x0, &y0);
            proj_xy(en_pl_x[i], en_pl_y[i], &x1, &y1);
            cv_line((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, mix(bg, K_COLD, q16(a * 1.4f)));
        }
    for (i = 0; i < n; i++)                      /* far side first, then the star, then the near side */
        order[i] = i;
    for (i = 1; i < n; i++)
        for (k = i; k > 0 && en_pl_y[order[k]] < en_pl_y[order[k - 1]]; k--) {
            int t = order[k];
            order[k] = order[k - 1];
            order[k - 1] = t;
        }
    for (i = 0; i < n; i++)
        if (en_pl_y[order[i]] < 0.0f)
            draw_planet(order[i]);
    draw_star();
    for (i = 0; i < n; i++)
        if (en_pl_y[order[i]] >= 0.0f)
            draw_planet(order[i]);
    if (en_dust > 0.01f)                         /* drifting dust */
        for (k = 0; k < 40; k++) {
            uint32_t x = ((uint32_t)k * 53u + ui.frame * (1u + (uint32_t)k % 3u) / 2u) % 240u, y = (uint32_t)k * 97u % (uint32_t)MAIN_H;
            px_blend((int32_t)x, (int32_t)y, RGB(214, 178, 132), (uint32_t)(en_dust * 12.0f));
        }
    if (en_rock_on) {                            /* the asteroid, its wake behind it */
        float x, y, tx, ty;
        proj_xy(en_rock_x, en_rock_y, &x, &y);
        proj_xy(en_rock_x - en_rock_vx * 2.2f, en_rock_y - en_rock_vy * 2.2f, &tx, &ty);
        cv_line((int32_t)tx, (int32_t)ty, (int32_t)x, (int32_t)y, mix(bg, RGB(170, 160, 150), 6));
        disc(x, y, 2.4f, RGB(164, 154, 143));
        px_blend((int32_t)x - 1, (int32_t)y - 1, RGB(210, 200, 190), 12);
    }
    if (en_comet_on) {                           /* the comet: its trail, its tail away from the star, its head */
        float cx, cy, len = 6.0f + (1.15f - en_comet_r) * 24.0f, dx, dy, dl;
        proj_xy(en_comet_r * cos_t(en_comet_ang), en_comet_r * fm_sin_turns(en_comet_ang), &cx, &cy);
        for (k = 0; k < ui.trail_n; k++) {
            float x, y;
            proj_xy(ui.trail[k][0], ui.trail[k][1], &x, &y);
            px_blend((int32_t)x, (int32_t)y, RGB(190, 232, 255), (uint32_t)(1 + k * 6 / (ui.trail_n ? ui.trail_n : 1)));
        }
        dx = cx - SCX, dy = cy - SCY, dl = fm_sqrtf(dx * dx + dy * dy) + 1e-3f;
        for (k = 0; k < 8; k++) {
            float t = (float)k / 8.0f;
            px_blend((int32_t)(cx + dx / dl * len * t), (int32_t)(cy + dy / dl * len * t), RGB(200, 238, 255), (uint32_t)(14.0f * (1.0f - t)));
            px_blend((int32_t)(cx + dx / dl * len * t) + 1, (int32_t)(cy + dy / dl * len * t), RGB(160, 220, 255), (uint32_t)(8.0f * (1.0f - t)));
        }
        cv_line((int32_t)cx, (int32_t)cy, (int32_t)(cx + dx / dl * len), (int32_t)(cy + dy / dl * len), mix(bg, RGB(200, 238, 255), 5));
        disc(cx, cy, 1.6f, RGB(255, 255, 255));
    }
    if (en_frozen || !en_playing) {              /* the state, top centre */
        const char *s = en_frozen ? "ECLIPSE" : "RESTING";
        text_c(120, 4, &FONT_XS, s, en_frozen ? K_ECLIPSE : K_DIM);
    }
}

/* a button's card (help_tick): its name, then what it does */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE SYSTEM: PLANETS,"); L("TEMPO, WANDER, PHRASE"); L("+ SELECT: THE TEMPO"); break;
    case B_ENV: L("THE STAR: LEVEL,"); L("COLOUR, ROAR, FLARES"); break;
    case B_ARP: L("A PLANET: LEVEL, INSTRUMENT,"); L("ELLIPSE, OCTAVE"); L("ALGORITHM / SEL: WHICH"); break;
    case B_LFO: L("GRAVITY: TIDE, REACH,"); L("WAKE, GREAT YEAR"); L("AGAIN: THE WEATHER"); break;
    case B_SEQ: L("THE PULSE: WHITE KEYS"); L("THE STEPS, BLACK 1-6"); L("THE LANE"); break;
    case B_SEL: L("THE NEXT PLANET"); L("(ON PULSE: THE NEXT LANE)"); break;
    case B_FX: L("THE SPACE: ECHO,"); L("REVERB, RADIO, WIDTH"); break;
    case B_GLO: L("SETUP: ROOT, MODE,"); L("TUNE, SYNC"); L("AGAIN: DISPLAY, MIDI OUT"); break;
    case B_EDIT: L("THE GREAT YEAR NOW:"); L("THE CHORD MOVES,"); L("THE DRUMS EVOLVE"); break;
    case B_REC: L("ECLIPSE: THE ORBITS"); L("AND THE WEATHER HOLD,"); L("THE SPACE RINGS ON"); break;
    case B_OCTDN: L("THE PLANET AN"); L("OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE PLANET AN"); L("OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("ON SETUP: SAVES ALL TEN"); L("ELSEWHERE: FROM THE START"); L("HOLD: BACK TO THE RECIPE"); break;
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

static void draw_main(void)
{
    if (ui.frame % 3u)                           /* 20 frames a second */
        return;
    if (en_comet_on && ui.frame % 6u == 0u) {    /* the comet's trail */
        float x = en_comet_r * cos_t(en_comet_ang), y = en_comet_r * fm_sin_turns(en_comet_ang);
        if (ui.trail_n == 20) {
            int k;
            for (k = 0; k < 19; k++) {
                ui.trail[k][0] = ui.trail[k + 1][0];
                ui.trail[k][1] = ui.trail[k + 1][1];
            }
            ui.trail_n = 19;
        }
        ui.trail[ui.trail_n][0] = x;
        ui.trail[ui.trail_n][1] = y;
        ui.trail_n++;
    }
    if (ui.help) {
        uint32_t h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    ui.sig[1] = 0;
    draw_system();
    cv_blit(0, MAIN_Y);
}

/* the knob strip: a rule with the page's name and the walk's dots at its right end; a cell a knob */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | (uint32_t)ui.sel << 12 | (uint32_t)ui.lane << 16 |
                                       (uint32_t)proj.par[P_THEME] << 20), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(view_knob(ui.view, (int)i)));
    h = hash(h, (uint32_t)now_touch | (uint32_t)nplanets() << 1);
    if (ui.view == V_OUT)
        h = hash(h, plat_cpu_pct());
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {
        char t[16];
        int k, nw = walk_len(), at = walk_pos();
        int32_t xd = 237 - nw * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        if (ui.view == V_PLANET) {
            int m = 0;
            while (t[m])
                m++;
            t[m] = ' ';
            t[m + 1] = (char)('1' + ui.sel);
            t[m + 2] = 0;
        }
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < nw; k++)
            cv_round(xd + k * 6, 3, 4, 4, 2, k == at ? K_ACC : (k >= V_PLANET && k < V_PLANET + nplanets()) ? mix(K_LINE, K_COLD, 6) : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = view_knob(ui.view, (int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_COLD;
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
        if (k != K_CPU && hi - lo < 6) {
            int nn = hi - lo + 1, j, pw = nn > 4 ? 6 : 9, gap = 3, w0 = nn * (pw + gap) - gap;
            for (j = 0; j < nn; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        {
            const felucca_font_t *f = text_w(&FONT_B, t) > 56 ? &FONT_XS : &FONT_B;
            text_c(cx, f == &FONT_B ? 50 : 51, f, t, hot ? K_ACC : K_TEXT);
        }
    }
    cv_blit(0, KNB_Y);
}

/* the lights: the page's button (ARP flashes as its planet plays), PLAY growing, REC in eclipse; the keys
 * the scale, or on PULSE the lane's steps (the step playing blinks) and its lane, on WEATHER the events */
static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (en_pl_glow[ui.sel] > 0.55f)
        b |= 1u << B_ARP;
    if (proj.leds) {
        if (ui.view == V_PULSE) {
            uint32_t bits = (uint16_t)proj.par[P_PAT + ui.lane];
            for (i = 0; i < 16; i++) {
                int on = (int)(bits >> i & 1u);
                if (en_playing && !en_frozen && i == en_step)
                    on = !on;
                if (on)
                    k |= 1u << WHITE_KEY[i];
            }
            k |= 1u << BLACK_KEY[ui.lane];
        } else {
            for (i = 0; i < NKEYS; i++)
                if (ap >> ((5u + i) % 12u) & 1u)
                    k |= 1u << i;
            if (ui.view == V_WEATHER) {
                uint8_t on[3] = {(uint8_t)(en_dust > 0.05f), en_rock_on, en_comet_on};
                for (i = 0; i < 3; i++)
                    k = on[i] ? k | 1u << BLACK_KEY[i] : k & ~(1u << BLACK_KEY[i]);
            }
        }
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* THE BOOT CARDS (as HYP-1's and ECO-1's). First RedtailWorks: a pixel red panda on the name; then GRAV-1
 * in big pixel letters, its steps lighting in like planets until the system starts. Any button or key:
 * straight in. */
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
    case 'R': return RGB(200, 84, 32);
    case 'D': return RGB(140, 52, 20);
    case 'T': return RGB(232, 156, 92);
    case 'W': return RGB(246, 236, 222);
    case 'B': return RGB(64, 30, 22);
    case 'K': return RGB(24, 16, 18);
    default: return 0;
    }
}
#define K_RUST RGB(200, 84, 32)
#define K_RINGS RGB(232, 156, 92)

static void draw_splash(void)
{
    int i, x, y;
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 96, K_BG);
    for (y = 0; y < PANDA_H; y++)
        for (x = 0; PANDA[y][x]; x++)
            if (PANDA[y][x] != '.')
                cv_rect(120 - PANDA_W * 3 / 2 + x * 3, 96 - PANDA_H * 3 - 4 + y * 3, 3, 3, panda_col(PANDA[y][x]));
    cv_blit(0, 0);
    cv_begin(240, 144, K_BG);
    text_c(120, 0, &FONT_L, "REDTAIL", K_RUST);
    text_c(120, 36, &FONT_L, "WORKS", K_RINGS);
    for (i = 0; i < 9; i++)
        cv_rect(48 + i * 16, 86, 12, 6, i & 1 ? K_RINGS : K_RUST);
    cv_rect(40, 80, 160, 1, K_LINE);
    cv_rect(40, 97, 160, 1, K_LINE);
    cv_rect(40, 80, 1, 18, K_LINE);
    cv_rect(199, 80, 1, 18, K_LINE);
    text_c(120, 110, &FONT_XS, "PRESENTS", K_DIM);
    cv_blit(0, 96);
}

/* the GRAV-1 card: big 5 x 7 pixel letters (6 px a pixel), gold over a rust shadow */
static const uint8_t BIG_GLYPH[6][7] = {
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},  /* G */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},  /* R */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  /* A */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},  /* V */
    {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00},  /* - */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* 1 */
};
#define SPLASH_STEP_Y 184
static void draw_grav(void)
{
    int g, r, c, pass;
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 144, K_BG);
    for (pass = 0; pass < 2; pass++)
        for (g = 0; g < 6; g++)
            for (r = 0; r < 7; r++)
                for (c = 0; c < 5; c++)
                    if (BIG_GLYPH[g][r] >> (4 - c) & 1u)
                        cv_rect(15 + (g * 6 + c) * 6 + (pass ? 0 : 3), 10 + r * 6 + (pass ? 0 : 3), 6, 6,
                                pass ? RGB(242, 201, 107) : K_RUST);
    text_c(120, 70, &FONT_S, "A STAR AND ITS PLANETS", K_TEXT);
    text_c(120, 88, &FONT_S, "PLAY THE FM-1", K_TEXT);
    text_c(120, 112, &FONT_XS, EN_VERSION[0] == 'D' ? "EXPERIMENTAL" : "EXPERIMENTAL  //  " EN_VERSION, K_RINGS);
    cv_blit(0, 24);
    for (c = 0; c < 16; c++)
        lcd_fill(8 + c * 14, SPLASH_STEP_Y, 11, 11, K_PANEL);
}

static void splash_tick(void)
{
    static const uint16_t H[6] = {RGB(255, 74, 46), RGB(255, 154, 60), RGB(242, 211, 107), RGB(127, 209, 139),
                                  RGB(79, 168, 255), RGB(220, 239, 255)};
    uint32_t t = plat_ms() - ui.boot_t, t1 = ui_splash_ms * 17u / 32u, n, k = 0, i;
    if (ui.splash == 1) {
        if (t < t1)
            return;
        ui.splash = 2;
        draw_grav();
    }
    n = (t - t1) * 17u / (ui_splash_ms - t1 + 1u);
    if (n > 16u)
        n = 16u;
    for (; ui.splash_step < n; ui.splash_step++)
        lcd_fill(8 + ui.splash_step * 14, SPLASH_STEP_Y, 11, 11, H[ui.splash_step * 6u / 16u]);
    for (i = 0; i < n; i++)
        k |= 1u << WHITE_KEY[i];
    plat_glow(1);
    plat_leds(n ? 1u << B_PLAY : 0u, k);
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.boot_t = plat_ms();
    ui.ev_r = en_ev_w;
    sky_init();
    sync_pats();
    ui.splash = ui_splash_ms > 0u;
    if (ui.splash) {
        draw_splash();
    } else {
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);
    }
}

void ui_frame(void)
{
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
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);
    }
    input();
    engine_news();
    if (ui.sel >= nplanets())
        ui.sel = (uint8_t)(nplanets() - 1);
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
