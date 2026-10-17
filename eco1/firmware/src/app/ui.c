/* SPDX-License-Identifier: GPL-3.0-only */
/* ECO-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too. The panel's handling (knobs, help cards, saving) is AMB-1's.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the plant grows / rests       EDIT   touch the plant: a charge runs up it
 *   REC            FREEZE: the day and the weather stop, the reverb rings on
 *   OCT- / OCT+    an octave down / up: on a leaf's page that leaf, on the Stem page the stem, else the plant
 *   SELECT         the pages (with HOME held: the day's length)
 *   ALGORITHM      mode (loads a pool)           PRESETS the piece (Root is on the Setup page)
 *   HOME SEL ENV ARP LFO SEQ FX GLO   the pages: PLANT, STEM, LOW LEAF, MIDDLE LEAF, TOP LEAF, WEATHER, SPACE,
 *                  SETUP
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe */
#include "../dsp/fastmath.h"

/* the pages, one a button; SELECT steps through them in this order */
enum { V_PLANT, V_STEM, V_LOW, V_MID, V_TOP, V_WEATHER, V_SPACE, V_SETUP, V_OUT, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Plant", "Stem", "Low leaf", "Mid leaf", "Top leaf", "Weather", "Space", "Setup",
                                              "MIDI out"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEL, B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_GLO, B_GLO};   /* GLO again: MIDI out */
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_LIGHT, P_DAY, P_WARMTH, P_SPREAD},
    {P_STEM, P_STEM_COLOUR, P_STEM_INT, P_STEM_TONE},
    {P_LEAF_LVL(0), P_LEAF_VOICE(0), P_LEAF_OCT(0), P_LEAF_TONE(0)},
    {P_LEAF_LVL(1), P_LEAF_VOICE(1), P_LEAF_OCT(1), P_LEAF_TONE(1)},
    {P_LEAF_LVL(2), P_LEAF_VOICE(2), P_LEAF_OCT(2), P_LEAF_TONE(2)},
    {P_RAIN, P_WIND, P_GUSTS, P_WX},
    {P_REVERB, P_SIZE, P_WINDVERB, P_STEM_VERB},
    {P_ROOT, P_TUNE, P_SYNC, P_GRID},
    {P_MIDI, P_MCH, K_LEDS, K_CPU},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Light", "Day", "Warmth", "Spread"},
    {"Level", "Colour", "Interval", "Tone"},
    {"Level", "Voice", "Octave", "Tone"},
    {"Level", "Voice", "Octave", "Tone"},
    {"Level", "Voice", "Octave", "Tone"},
    {"Rain", "Wind", "Gusts", "Sound"},
    {"Reverb", "Size", "Wind verb", "Stem verb"},
    {"Root", "Tune", "Sync", "Grid"},
    {"Sends", "Channels", "Lights", "CPU"},
};

/* the screen's colours: a night garden. The bands are dark soil and moss; the sky between them
 * follows the day. */
#define K_BG RGB(22, 26, 23)
#define K_PANEL RGB(40, 48, 41)
#define K_LINE RGB(60, 72, 62)
#define K_DIM RGB(128, 146, 122)
#define K_TEXT RGB(228, 234, 216)
#define K_ACC RGB(156, 218, 110)         /* the accent: the page, the knob being turned, a message's word */
#define K_LEAF RGB(84, 160, 72)          /* a leaf, well watered */
#define K_LEAF_DRY RGB(176, 150, 70)     /* and wilting */
#define K_GLOW RGB(214, 255, 150)        /* firing */
#define K_VEIN RGB(40, 86, 40)
#define K_VEIN_HOT RGB(250, 236, 140)
#define K_STEM RGB(70, 120, 60)
#define K_SUN RGB(255, 206, 96)
#define K_MOON RGB(220, 226, 236)
#define K_RAIN RGB(150, 190, 230)
#define K_FROST RGB(120, 190, 230)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
    uint32_t boot_t;                   /* when the boot cards went up */
    uint8_t splash, splash_step;       /* the boot cards are up (2: the ECO-1 card); its steps lit */
} ui;
uint32_t ui_splash_ms = 3200u;         /* the boot cards: RedtailWorks, then ECO-1 (the host's tests set 0) */

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
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
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

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
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
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
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

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say("PLANT", piece_name(proj.piece));
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    if (b == B_GLO && ui.view == V_SETUP) {      /* GLO again: the MIDI out page */
        set_view(V_OUT);
        return;
    }
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT:
        en_touch();
        say("TOUCH", en_playing ? 0 : "(RESTING)");
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
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP: {             /* the page's voice: a leaf, the stem, else the whole plant */
        int p = ui.view == V_STEM ? P_STEM_OCT : ui.view >= V_LOW && ui.view <= V_TOP ? P_LEAF_OCT(ui.view - V_LOW) : P_OCT;
        char t[8];
        knob_set(p, proj.par[p] + (b == B_OCTUP ? 1 : -1));
        en_param_text(p, proj.par[p], t);
        say(p == P_STEM_OCT ? "STEM OCTAVE" : p == P_OCT ? "OCTAVE" : "LEAF OCTAVE", t);
        break;
    }
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; start and
 * stop grow and rest the plant */
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

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press: it has no card. */
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
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c);
/* REMOTE (SEL + GLO): the panel plays ECO-1 in the browser over USB MIDI, and the FM-1 goes quiet, as on
 * HYP-1 and LOF-1. On channel 16: the keys are notes 48 + k, the buttons CC 102 + b (127 down, 0 up), the
 * knobs CC 85 + role (64 + detents), MASTER CC 7, and CC 116 says hello every second. The browser answers
 * with its lights the same way (notes and CC 102 + b) and CC 117, so the panel lights as the emulator's
 * does. SEL + GLO again: back, the system growing again if it was. Not in the browser's own build. */
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
    uint8_t on, link, was_playing;
    uint32_t btn, keys, ign_b, ign_k;            /* sent as down; held at the start, not sent till let go */
    uint32_t lit_b, lit_k;                       /* the browser's lights */
    uint32_t hello_t, rx_t;
    int master;
} rm;
int ui_remote(void) { return rm.on; }

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c);
static void rm_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

static void rm_draw(void)
{
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 120, K_BG);
    text_c(120, 4, &FONT_L, "REMOTE", K_ACC);
    text_c(120, 44, &FONT_S, rm.link ? "PLAYING THE BROWSER" : "WAITING FOR THE BROWSER", rm.link ? K_SUN : K_DIM);
    text_c(120, 66, &FONT_XS, "USB MIDI  //  CHANNEL 16", K_DIM);
    text_c(120, 96, &FONT_S, "SEL + GLO: BACK", K_TEXT);
    cv_blit(0, 60);
}

#ifndef EN_WEB
static void rm_enter(void)
{
    int e;
    memset(&rm, 0, sizeof rm);
    rm.on = 1;
    rm.was_playing = en_playing;
    rm.ign_b = plat_buttons();
    rm.ign_k = plat_keys();
    rm.hello_t = plat_ms() - 1000u;
    rm.master = -1;
    for (e = 0; e < NE; e++)
        (void)plat_enc(e);
    en_play(0);
    en_mute(1);                                  /* after the rest's note-offs */
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
    if (rm.was_playing)
        en_play(1);
    ui.btn = plat_buttons();                     /* what's held now does nothing when let go */
    ui.keys = plat_keys();
    ui.btn_used = ui.btn;
    ui.act_t = plat_ms();
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;       /* everything drawn again */
    lcd_fill(0, 0, 240, 240, K_BG);
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
}

static int ff_on(void);
static int ff_input(uint32_t btn, uint32_t keys);

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    if (rm.on)                                   /* remote: rm_tick has the panel and the MIDI */
        return;
    if (ff_on()) {                               /* the fireflies up: a touch only wakes the screen */
        ff_input(btn, keys);
        midi_in();
        return;
    }
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
#ifndef EN_WEB
            if (i == B_GLO && (btn >> B_SEL & 1u)) {   /* SEL + GLO: remote, the panel plays the browser */
                ui.btn_used |= m | 1u << B_SEL;  /* letting go does nothing, in any order */
                ui.btn = btn;
                ui.keys = keys;
                ui.help = 0;
                rm_enter();
                return;
            }
#endif
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if ((ch & keys) >> i & 1u)
            toggle_note((int)((5u + i) % 12u));  /* key 0 is F3 */
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the day's length */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_DAY, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_PLANT) {            /* Day is the Plant page's second knob */
                ui.touched = 1;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[10];
                en_param_text(P_DAY, proj.par[P_DAY], t);
                say("DAY", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the plant (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

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

/* the time of day as a clock, "06:40" */
static void clock_text(char *t)
{
    uint32_t m = (uint32_t)(en_day * 1440.0f) % 1440u;
    t[0] = (char)('0' + m / 600u);
    t[1] = (char)('0' + m / 60u % 10u);
    t[2] = ':';
    t[3] = (char)('0' + m % 60u / 10u);
    t[4] = (char)('0' + m % 10u);
    t[5] = 0;
}

/* the header: the plant in a tag, its root and mode; on the right three dots, one a leaf, lit as
 * they fire (a play triangle while resting), and a dot while there is something unsaved. Messages
 * take its place for a moment. */
static void draw_header(void)
{
    int msg = plat_ms() < ui.msg_until, l;
    uint32_t h = hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                      en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_frozen << 6 | (uint32_t)proj.par[P_MODE] << 8 |
                          (ui.view == V_SETUP ? plat_cpu_pct() << 12 : 0u) | (uint32_t)en_ext << 20);
    if (en_ext)
        h = hash(h, (uint32_t)(en_bpm + 0.5f));
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    for (l = 0; l < EN_NLEAF; l++)
        h = hash(h, (uint32_t)(q16(en_leaf_glow[l]) / 4));
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {                             /* on the clock: its tempo */
            char c[12] = "EXT ";
            itoa_u((uint32_t)(en_bpm + 0.5f), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_ACC);
            x -= 8;
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load too */
            char c[12] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 8;
        }
        if (en_playing) {                         /* the leaves: three dots, lit as they fire */
            for (l = EN_NLEAF - 1; l >= 0; l--) {
                x -= 11;
                cv_round(x, 10, 8, 8, 4, mix(K_PANEL, K_GLOW, q16(en_leaf_glow[l])));
            }
        } else {
            int32_t j;                            /* resting: a play triangle */
            x -= 10;
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        if (en_frozen) {
            x -= text_w(&FONT_XS, "STILL") + 6;
            cv_text(x, 8, &FONT_XS, "STILL", K_FROST);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the plant, in a tag */
        cv_round(4, 4, w, 20, 6, K_ACC);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
        cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
    }
    cv_blit(0, 0);
}

/* the plant in its place (scene.c): the sky (its colour the time of day; the sun or the moon on its arc;
 * stars; grey in the rain), rain and wind, each species its own background, ground and body; over it
 * the clock, the soil's water and the weather in words */
static uint16_t sky_colour(float t, int top)  /* t: the day, 0..1 from midnight */
{
    float s = fm_sin_turns(t - 0.25f), d = fm_clampf((s + 0.12f) * 1.6f, 0.0f, 1.0f), tw = fm_clampf(1.0f - fm_fabsf(s) * 5.0f, 0.0f, 1.0f);
    uint16_t c = top ? mix(RGB(10, 14, 34), RGB(72, 132, 196), q16(d)) : mix(RGB(26, 32, 60), RGB(170, 206, 226), q16(d));
    if (!top)
        c = mix(c, RGB(236, 142, 92), q16(tw * 0.75f));
    else
        c = mix(c, RGB(110, 80, 120), q16(tw * 0.4f));
    return mix(c, top ? RGB(70, 76, 86) : RGB(120, 126, 134), q16(en_rain * 0.7f * d + en_rain * 0.25f));
}

#include "scene.c"
#include "fireflies.c"
static int ff_on(void) { return ff.on; }

static void draw_plant(void)
{
    int sp = proj.par[P_SPECIES];
    draw_scene(sp);
    {   /* the time; under it, the soil's water */
        char c[16];
        clock_text(c);
        cv_text(7, 5, &FONT_S, c, RGB(10, 12, 20));
        cv_text(6, 4, &FONT_S, c, K_TEXT);
        c[0] = 0;
        {
            char *p = c, n[6];
            const char *w = sp == EN_SP_LOTUS ? "POND " : "SOIL ";
            while (*w)
                *p++ = *w++;
            itoa_u((uint32_t)(en_soil * 100.0f + 0.5f), n);
            w = n;
            while (*w)
                *p++ = *w++;
            *p++ = '%';
            *p = 0;
        }
        cv_text(7, GROUND + 3, &FONT_XS, c, RGB(16, 14, 10));
        cv_text(6, GROUND + 2, &FONT_XS, c, K_TEXT);
        if (en_rain > 0.03f || en_wind > 0.15f) {   /* the weather now, at the soil's right */
            char wtxt[24], *p = wtxt;
            const char *s = en_rain > 0.6f ? "POURING" : en_rain > 0.25f ? "RAIN" : en_rain > 0.03f ? "DRIZZLE" : "";
            while (*s)
                *p++ = *s++;
            if (en_wind > 0.15f) {
                if (p != wtxt)
                    *p++ = ' ';
                s = en_wind > 0.7f ? "GALE" : en_wind > 0.4f ? "WINDY" : "BREEZE";
                while (*s)
                    *p++ = *s++;
            }
            *p = 0;
            cv_text(235 - text_w(&FONT_XS, wtxt), GROUND + 3, &FONT_XS, wtxt, RGB(16, 14, 10));
            cv_text(234 - text_w(&FONT_XS, wtxt), GROUND + 2, &FONT_XS, wtxt, K_TEXT);
        }
    }
}

/* a button's card (help_tick): its name, then what it does, in the plant's place */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE PLANT AND ITS SKY"); L("LIGHT, DAY, WARMTH, SPREAD"); L("+ SELECT: THE DAY'S LENGTH"); break;
    case B_ENV: L("THE LOW LEAF"); L("LEVEL, VOICE, OCTAVE, TONE"); break;
    case B_ARP: L("THE MIDDLE LEAF"); L("LEVEL, VOICE, OCTAVE, TONE"); break;
    case B_LFO: L("THE TOP LEAF"); L("LEVEL, VOICE, OCTAVE, TONE"); break;
    case B_SEQ: L("THE WEATHER"); L("RAIN, WIND, GUSTS,"); L("ITS SOUND"); break;
    case B_SEL: L("THE STEM'S DRONE"); L("LEVEL, COLOUR, INTERVAL,"); L("TONE; OCT-/+ ITS OCTAVE"); break;
    case B_FX: L("THE SPACE"); L("REVERB, SIZE, WIND VERB,"); L("STEM VERB"); break;
    case B_GLO: L("SETUP: ROOT, TUNE,"); L("SYNC TO MIDI CLOCK, GRID"); L("AGAIN: MIDI OUT, LIGHTS"); L("SEL + GLO: REMOTE"); break;
    case B_EDIT: L("TOUCH THE PLANT:"); L("A CHARGE RUNS UP IT"); L("FROM THE SOIL"); break;
    case B_REC: L("FREEZE: THE DAY AND"); L("THE WEATHER STOP,"); L("THE REVERB RINGS ON"); break;
    case B_OCTDN: L("AN OCTAVE DOWN: THE PAGE'S"); L("LEAF OR STEM, ELSE THE PLANT"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("AN OCTAVE UP: THE PAGE'S"); L("LEAF OR STEM, ELSE THE PLANT"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PLANTS"); L("KEEP HOLDING: THE PLANT"); L("BACK TO ITS RECIPE"); break;
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
    uint32_t h, i;
    if (ui.frame % 3u)                           /* 20 frames a second */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    h = hash(2166136261u, (uint32_t)(en_day * 1440.0f) | (uint32_t)q16(en_rain) << 12 | (uint32_t)q16(en_wind) << 18 |
                              (uint32_t)q16(en_soil) << 24);
    for (i = 0; i < EN_NLEAF; i++)
        h = hash(h, (uint32_t)q16(en_leaf_glow[i]) | (uint32_t)q16(en_leaf_charge[i]) << 5 | (uint32_t)q16(en_leaf_water[i]) << 10 |
                        (uint32_t)(int32_t)(en_leaf_sway[i] * 40.0f) << 16);
    h = hash(h, ui.frame | (uint32_t)proj.par[P_SPECIES] << 24);   /* the scenes move (water, weather): every frame */
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    draw_plant();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and the page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch | (uint32_t)en_ext << 1);   /* on the clock, Day reads in bars */
    if (ui.view == V_OUT)
        h = hash(h, plat_cpu_pct());
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
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
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_LEAF;
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
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

/* the lights: the page's button, PLAY growing, REC frozen; a leaf's page button flashes as it fires;
 * the keys show the scale */
static void leds(void)
{
    static const uint8_t LEAF_BTN[EN_NLEAF] = {B_ENV, B_ARP, B_LFO};
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    for (i = 0; i < EN_NLEAF; i++)
        if (en_leaf_glow[i] > 0.55f)
            b |= 1u << LEAF_BTN[i];
    if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the plant grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_stem_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* THE BOOT CARDS (as HYP-1's). First RedtailWorks: a pixel red panda (3x) sitting on the name, its
 * ringed tail curled up behind it, and the rings again beneath; then ECO-1 in big pixel letters, a seed's
 * worth of steps greening in (the white keys lighting with them) until the plant wakes. Any button or
 * key: straight to the plant. */
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
#define K_RUST RGB(200, 84, 32)
#define K_RINGS RGB(232, 156, 92)

static void draw_splash(void)
{
    int i, x, y;
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 96, K_BG);                     /* the panda */
    for (y = 0; y < PANDA_H; y++)
        for (x = 0; PANDA[y][x]; x++)
            if (PANDA[y][x] != '.')
                cv_rect(120 - PANDA_W * 3 / 2 + x * 3, 96 - PANDA_H * 3 - 4 + y * 3, 3, 3, panda_col(PANDA[y][x]));
    cv_blit(0, 0);
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
    cv_blit(0, 96);
}

/* the ECO-1 card: big 5 x 7 pixel letters (7 px a pixel), green over a rust shadow */
static const uint8_t BIG_GLYPH[5][7] = {
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  /* E */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},  /* C */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  /* O */
    {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00},  /* - */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* 1 */
};
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
#define SPLASH_STEP_Y 184
static void draw_eco(void)
{
    int g, r, c, pass;
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 144, K_BG);
    for (pass = 0; pass < 2; pass++)             /* the shadow, then the letters */
        for (g = 0; g < 5; g++)
            for (r = 0; r < 7; r++)
                for (c = 0; c < 5; c++)
                    if (BIG_GLYPH[g][r] >> (4 - c) & 1u)
                        cv_rect(18 + (g * 6 + c) * 7 + (pass ? 0 : 3), 10 + r * 7 + (pass ? 0 : 3), 7, 7,
                                pass ? K_ACC : K_RUST);
    text_c(120, 76, &FONT_S, "A PLANT PLAYS THE FM-1", K_TEXT);
    text_c(120, 102, &FONT_XS, EN_VERSION[0] == 'D' ? "EXPERIMENTAL" : "EXPERIMENTAL  //  " EN_VERSION, K_RINGS);
    cv_blit(0, 30);
    for (c = 0; c < 16; c++)
        lcd_fill(8 + c * 14, SPLASH_STEP_Y, 11, 11, K_PANEL);
}

static void splash_tick(void)                    /* the ECO-1 card's steps, greening in */
{
    uint32_t t = plat_ms() - ui.boot_t, t1 = ui_splash_ms * 17u / 32u, n, k = 0, i;
    if (ui.splash == 1) {
        if (t < t1)
            return;
        ui.splash = 2;
        draw_eco();
    }
    n = (t - t1) * 17u / (ui_splash_ms - t1 + 1u);
    if (n > 16u)
        n = 16u;
    for (; ui.splash_step < n; ui.splash_step++)
        lcd_fill(8 + ui.splash_step * 14, SPLASH_STEP_Y, 11, 11, ui.splash_step % 4u ? K_LEAF : K_ACC);
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
    ui.splash = ui_splash_ms > 0u;
    if (ui.splash) {
        draw_splash();
    } else {
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);                          /* it grows from the start */
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
        ui.splash = 0;                           /* the cards' time is up (or skipped): the plant */
        ui.btn = plat_buttons();                 /* a press that skipped them does nothing more */
        ui.keys = plat_keys();
        ui.btn_used = ui.btn;
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);
    }
    if (rm.on) {                                 /* remote: the browser's lights; the screen says so */
        rm_tick();
        if (rm.on) {
            plat_glow(1);
            plat_leds(rm.lit_b, rm.lit_k);
            return;
        }
    }
    input();
    autosave();
    if (!ff.on && !rm.on && ui_flies_ms && !ui.help && plat_ms() - ui.act_t >= ui_flies_ms)
        ff_enter();                              /* half an hour untouched: the fireflies */
    if (ff.on) {
        ff_update();
        if (ui.frame % 3u == 0u)
            ff_draw();
        ff_leds();
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
