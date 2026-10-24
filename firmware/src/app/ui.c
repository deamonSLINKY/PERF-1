/* SPDX-License-Identifier: GPL-3.0-only */
/* HYP-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   SELECT         the pages, in order (with HOME held: the tempo)
 *   HOME           Tempo (tempo, evolve, motion, drift)
 *   ENV            Pad, then Pad env, Pad LFO (each press the next)
 *   LFO            Drone, then Drone env, Drone LFO
 *   ARP            Acid, then Acid env, Acid line
 *   SEQ            Drums, then Links; held, the sequencer (six lanes, sixteen steps); with SEL held, the
 *                  links (the drums into the voices) on and off
 *   GLO            Global (root, theme, tune, MIDI out); Mixer and Space are on the SELECT dial only.
 *                  On Global, SAVE stores the piece as it is (patterns and all); held, the piece goes
 *                  back to its factory settings and out of the store
 *   KNOB 1-4       the page's four values (in the sequencer: the lane's level, tune, decay; the pattern)
 *   FX             the effects menu, on and off: while it is on, each key is an effect while held.
 *                  SAVE held is lock mode: each key tapped locks its effect on (or, locked, off again),
 *                  one at a time or together; keys held as SAVE goes down lock too. A locked effect's
 *                  key tapped (no SAVE) turns that one off; closing FX lets them all go. SAVE tapped
 *                  alone is still the breakdown
 *   SAVE           the breakdown on the next bar; again, a four-bar build and the drop; held, the drop
 *                  (on Global: the piece stored, above)
 *   OCT- / OCT+    the octave (3 down .. 3 up) of the voice whose page it is (pad, drone, acid); on any
 *                  other page, the whole song's; in the sequencer, the lane
 *   keys           the scale (each adds or removes its note); in the sequencer, the sixteen white keys
 *                  (the lower row) are the lane's steps: lit where it hits, tapped to turn a hit on or off
 *   SEL            shift, held: white keys 1-6 the pattern (on the bar); PRESETS the piece, on the next
 *                  bar; ALGORITHM the sound of the voice on screen (pad, drone, acid; on the drums' pages the kit); in the sequencer, black keys 1-6 copy the pattern there, OCT- clears the drum's
 *                  lane, OCT+ puts back the piece's own
 *   PLAY           play / stop         REC  FREEZE       EDIT  the next chord
 *   ALGORITHM      the mode            PRESETS  the piece (01 .. 10)
 *
 * Each piece has its own settings and six drum patterns. Power-on starts on a piece at random, as it was
 * stored, or its factory settings nudged a little if it never was (project_start). A store is written
 * at once (store_now: playing, the music hushes around the write); the theme, Tune and MIDI out two
 * seconds after a change. */
#include "../dsp/fastmath.h"

/* --------------------------------------------------------------- pages --- */
enum { V_TEMPO, V_PAD, V_PADENV, V_PADLFO, V_CHORDS, V_DRONE, V_DRENV, V_DRLFO, V_ACID, V_ACENV, V_ACLINE, V_CMEL, V_CMEL2,
       V_DRUMS, V_LINKS, V_KIT, V_DDRIFT, V_GROOVE, V_SPACE, V_REVERB, V_MASTER, V_MIXER, V_GLOBAL, V_JAM, NDIAL, V_SEQ = NDIAL, V_PERF, V_VIZ, NVIEWS };   /* V_PERF: the second home screen; V_VIZ: the visualizers */
static const char *const VIEW_NAME[NVIEWS] = {"Tempo", "Pad", "Pad env", "Pad LFO", "Chords", "Drone", "Drone env",
                                              "Drone LFO", "Acid", "Acid env", "Acid line", "Pluck", "Pluck 2", "Drums", "Links", "Kit", "Drum drift", "Groove", "Space", "Reverb", "Comp/Limit", "Mixer", "Global", "Jam",
                                              "Sequencer", "Perform", "Visual"};
enum { VZ_PLASMA, VZ_KALEIDO, VZ_BARS, VZ_FLUX, VZ_DISCO, VZ_LIFE, VZ_ORBIT, VZ_AUTO, VZ_N };   /* the visualizers (viz.c) */
static const char *const VZ_NAME[VZ_N] = {"PLASMA", "KALEIDO", "BARS", "FLUX", "DISCO", "LIFE", "ORBIT", "AUTO"};
/* the knobs of each page: a parameter, or one of these (the sequencer's: the lane's) */
enum { K_LVL = 200, K_TUNE, K_DEC, K_MXT, K_MXL, K_MXP, K_MXS, K_GRT, K_GRD, K_GRS, K_GRA, K_NONE, K_FXF, K_FXC, K_FXT, K_FXW };   /* K_FX*: the FX menu's own knobs */
static const char *const FXK_LABEL[4] = {"Filter", "Crush", "Throw", "Wash"};

/* the FX edit pages (SEL + FX walks them; FX alone is the performing menu, or closes): each effect's settings on
 * the four knobs, the effect itself held on while the page is up, so it is heard as it is edited. K_FXP + 4 *
 * page + knob. The gate's pattern is on the white keys, as the sequencer's steps. */
enum { K_FXP = 220 };
enum { KN_PLAIN, KN_PRESET, KN_LEN, KN_TGT, KN_PTGT, KN_NAME, KN_SEC, KN_RANGE, KN_NONE };   /* KN_RANGE: 2 .. n; KN_NONE: no knob */
#define NFXP 23
static const char *const RATE_NAME[7] = {"1/2", "1/4", "1/6", "1/8", "1/16", "1/32", "1/64"};          /* the roll's rates */
static const char *const RTIME_NAME[4] = {"1/2 BEAT", "1 BEAT", "2 BEATS", "4 BEATS"};         /* beats at each rate */
static const char *const PRATE_NAME[4] = {"1/2", "1/4", "1/8", "1/16"};                        /* the pump's period, in notes */
static const char *const SSIZE_NAME[3] = {"1/32", "1/16", "1/8"};                               /* the scatter's slice */
static const char *const HSPEED_NAME[3] = {"X1.5", "X2", "X4"};                                 /* half time: how much slower */
static const char *const SWRATE_NAME[7] = {"8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8"};              /* the sweep's period */
static const char *const SWTYPE_NAME[3] = {"LOW", "HIGH", "BAND"};
static const char *const SREVERY_NAME[4] = {"1/4", "1/8", "1/16", "1/32"};                      /* the scratch's period */
static const char *const SREACH_NAME[4] = {"1/16", "1/8", "1/4", "1/2"};                         /* how much of the past it works */
static const char *const SSTYLE_NAME[2] = {"BABY", "PULL"};
static const char *const FGRAIN_NAME[4] = {"1", "2", "3", "4"};                                  /* the freeze's grains */
static const char *const RVLEN_NAME[3] = {"1/2 BEAT", "1 BEAT", "2 BEATS"};                      /* the reverse swell */
static const char *const LFO_SHAPE_NAME_UI[6] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "S&H", "DRIFT"};
static const float ETIME_BEATS_UI[6] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};
static const char *const TKR_NAME[5] = {"2 BARS", "1 BAR", "1/2", "1/4", "1/8"};              /* talk: the vowels' period */
static const char *const SMM_NAME[3] = {"NORMAL", "UP", "DOWN"};                            /* the DJ snips: an octave up or down */
static const char *const SML_NAME[2] = {"ONE SHOT", "HELD"};
static const char *const SMN_NAME[3] = {"DRY", "SOME", "LOTS"};                                  /* how much goes to the echo and the reverb */
static const char *const FXQ_NAME[3] = {"OFF", "BEAT", "BAR"};                                   /* Flex Beat: when it starts */
static const char *const FXM_NAME[2] = {"ONE SHOT", "LOOP"};
static const char *const JV_NAME[4];
static const char *const JM_NAME[3];
static const char *const JO_NAME[5];
static const char *const SAVER_NAME[4] = {"OFF", "1 MIN", "3 MIN", "10 MIN"};                  /* the screensaver: left alone this long while playing */
static const char *const LIGHTS_NAME[2] = {"OFF", "SHOW"};                                       /* the keys' lights on the visualizers */
static const char *const KEYSET_NAME[5] = {"SAME", "+2", "+3", "+5", "+7"};                     /* the drop's key change, in semitones */
static const char *const GENRE_UI_NAME[10] = {"HYPNO", "LOFI", "HOUSE", "DUBSTEP", "JUNGLE", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP"};
static const char *const SCQ_NAME[2] = {"AT ONCE", "NEXT BAR"};                                  /* when a scene comes */
static const char *const DJG_NAME[5] = {"8 BARS", "16 BARS", "32 BARS", "64 BARS", "SYNC"};   /* the auto-DJ's groove (SYNC: until the chord changes) */
static const char *const DJB_NAME[3] = {"SHORT", "HALF", "FULL"};                                /* its breakdown, against the groove */
static const char *const ONOFF_NAME[2] = {"OFF", "ON"};
static const struct {
    const char *name;
    uint8_t bit;                        /* the effect (FX_*) held on while the page is up */
    uint8_t hold;                       /* it is heard only while a key is held (a roll and a riser go on and on) */
    struct {
        const char *label;
        uint8_t kind, idx;              /* KN_*; the setting (EN_FXS_*) */
        const char *const *names;       /* KN_NAME: its names (a setting 0 .. n - 1) */
        uint8_t n;
    } c[4];
} FXP[NFXP] = {
    {"GATE", FX_GATEX, 0, {{"Pattern", KN_PRESET, 0, 0, 0}, {"Length", KN_LEN, EN_FXS_GX_LEN, 0, 0},
                           {"Targets", KN_TGT, 0, 0, 0}, {"Depth", KN_PLAIN, EN_FXS_GX_DEPTH, 0, 0}}},
    {"PHASER", FX_PHASER, 0, {{"Rate", KN_PLAIN, EN_FXS_PH_RATE, 0, 0}, {"Depth", KN_PLAIN, EN_FXS_PH_DEPTH, 0, 0},
                              {"Feedbk", KN_PLAIN, EN_FXS_PH_FB, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_PH_MIX, 0, 0}}},
    {"FLANGER", FX_FLANGER, 0, {{"Rate", KN_PLAIN, EN_FXS_FL_RATE, 0, 0}, {"Depth", KN_PLAIN, EN_FXS_FL_DEPTH, 0, 0},
                                {"Feedbk", KN_PLAIN, EN_FXS_FL_FB, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_FL_MIX, 0, 0}}},
    {"VINYL", FX_VINYL, 0, {{"Level", KN_PLAIN, EN_FXS_VN_LEVEL, 0, 0}, {"Crackle", KN_PLAIN, EN_FXS_VN_CRACK, 0, 0},
                            {"Hiss", KN_PLAIN, EN_FXS_VN_HISS, 0, 0}, {"Tone", KN_PLAIN, EN_FXS_VN_TONE, 0, 0}}},
    {"TAPE", FX_TAPE, 0, {{"Wow", KN_PLAIN, EN_FXS_TP_WOW, 0, 0}, {"Flutter", KN_PLAIN, EN_FXS_TP_FLUT, 0, 0},
                          {"Drive", KN_PLAIN, EN_FXS_TP_DRIVE, 0, 0}, {"Dull", KN_PLAIN, EN_FXS_TP_DULL, 0, 0}}},
    {"ROLL", FX_ROLL, 1, {{"From", KN_NAME, EN_FXS_RL_START, RATE_NAME, 7}, {"To", KN_NAME, EN_FXS_RL_END, RATE_NAME, 7},
                          {"Each", KN_NAME, EN_FXS_RL_TIME, RTIME_NAME, 4}, {"Swell", KN_PLAIN, EN_FXS_RL_SWELL, 0, 0}}},
    {"PUMP", FX_PUMP, 0, {{"Depth", KN_PLAIN, EN_FXS_PM_DEPTH, 0, 0}, {"Release", KN_PLAIN, EN_FXS_PM_REL, 0, 0},
                          {"Every", KN_NAME, EN_FXS_PM_RATE, PRATE_NAME, 4}, {"Targets", KN_PTGT, 0, 0, 0}}},
    {"RISER", FX_RISER, 1, {{"Time", KN_SEC, EN_FXS_RS_TIME, 0, 0}, {"Filter", KN_PLAIN, EN_FXS_RS_FILT, 0, 0},
                            {"Wash", KN_PLAIN, EN_FXS_RS_WASH, 0, 0}, {"Echo", KN_PLAIN, EN_FXS_RS_ECHO, 0, 0}}},
    {"SCATTER", FX_SCATTER, 1, {{"Chaos", KN_PLAIN, EN_FXS_SC_CHAOS, 0, 0}, {"Reverse", KN_PLAIN, EN_FXS_SC_REV, 0, 0},
                                {"Slice", KN_NAME, EN_FXS_SC_SIZE, SSIZE_NAME, 3}, {"Slices", KN_RANGE, EN_FXS_SC_LEN, 0, 8}}},
    {"BRAKE", FX_BRAKE, 1, {{"Time", KN_PLAIN, EN_FXS_BR_TIME, 0, 0}, {"Spinbk", KN_PLAIN, EN_FXS_BR_BACK, 0, 0},
                            {"Return", KN_PLAIN, EN_FXS_BR_RET, 0, 0}, {"Dull", KN_PLAIN, EN_FXS_BR_DULL, 0, 0}}},
    {"HALFTIME", FX_HALF, 1, {{"Speed", KN_NAME, EN_FXS_HF_SPEED, HSPEED_NAME, 3}, {"Slide", KN_PLAIN, EN_FXS_HF_SLIDE, 0, 0},
                              {"", KN_NONE, 0, 0, 0}, {"", KN_NONE, 0, 0, 0}}},
    {"SWEEP", FX_SWEEP, 0, {{"Every", KN_NAME, EN_FXS_SW_RATE, SWRATE_NAME, 7}, {"Depth", KN_PLAIN, EN_FXS_SW_DEPTH, 0, 0},
                            {"Reso", KN_PLAIN, EN_FXS_SW_RESO, 0, 0}, {"Type", KN_NAME, EN_FXS_SW_TYPE, SWTYPE_NAME, 3}}},
    {"SCRATCH", FX_SCRATCH, 1, {{"Every", KN_NAME, EN_FXS_SR_EVERY, SREVERY_NAME, 4}, {"Reach", KN_NAME, EN_FXS_SR_REACH, SREACH_NAME, 4},
                                {"Style", KN_NAME, EN_FXS_SR_STYLE, SSTYLE_NAME, 2}, {"", KN_NONE, 0, 0, 0}}},
    {"FREEZE", FX_FREEZE, 1, {{"Size", KN_PLAIN, EN_FXS_FZ_SIZE, 0, 0}, {"Spread", KN_PLAIN, EN_FXS_FZ_SPREAD, 0, 0},
                              {"Grains", KN_NAME, EN_FXS_FZ_GRAINS, FGRAIN_NAME, 4}, {"", KN_NONE, 0, 0, 0}}},
    {"RV SWELL", FX_RVSWELL, 1, {{"Length", KN_NAME, EN_FXS_RV_LEN, RVLEN_NAME, 3}, {"Mix", KN_PLAIN, EN_FXS_RV_MIX, 0, 0},
                                 {"", KN_NONE, 0, 0, 0}, {"", KN_NONE, 0, 0, 0}}},
    {"RING MOD", FX_RING, 0, {{"Tone", KN_PLAIN, EN_FXS_RM_FREQ, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_RM_MIX, 0, 0},
                              {"", KN_NONE, 0, 0, 0}, {"", KN_NONE, 0, 0, 0}}},
    /* the performing settings: no effect on this page (31), and the settings are the UI's (uiv) */
    {"AUTO DJ", 63, 0, {{"Scenes", KN_NAME, 0, SCQ_NAME, 2}, {"Groove", KN_NAME, 1, DJG_NAME, 5},
                        {"Break", KN_NAME, 2, DJB_NAME, 3}, {"Fill", KN_NAME, 3, ONOFF_NAME, 2}}},
    {"SETTINGS", 63, 0, {{"Key", KN_NAME, 4, KEYSET_NAME, 5}, {"Saver", KN_NAME, 5, SAVER_NAME, 4}, {"Lights", KN_NAME, 6, LIGHTS_NAME, 2},
                         {"Genre", KN_NAME, 11, GENRE_UI_NAME, 10}}},
    {"FLEX", 62, 0, {{"Quant", KN_NAME, EN_FXS_FX_QUANT, FXQ_NAME, 3}, {"Mode", KN_NAME, EN_FXS_FX_MODE, FXM_NAME, 2},
                     {"Mix", KN_PLAIN, EN_FXS_FX_MIX, 0, 0}, {"", KN_NONE, 0, 0, 0}}},
    {"SNIPS", 62, 0, {{"Pitch", KN_NAME, EN_FXS_SM_MODE, SMM_NAME, 3}, {"Play", KN_NAME, EN_FXS_SM_LOOP, SML_NAME, 2},
                      {"Echo", KN_NAME, EN_FXS_SM_LEN, SMN_NAME, 3}, {"Level", KN_PLAIN, EN_FXS_SM_MIX, 0, 0}}},
    {"TALK", FX_TALK, 0, {{"Every", KN_NAME, EN_FXS_TK_RATE, TKR_NAME, 5}, {"Depth", KN_PLAIN, EN_FXS_TK_DEPTH, 0, 0},
                          {"Reso", KN_PLAIN, EN_FXS_TK_RESO, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_TK_MIX, 0, 0}}},
    {"WAH", FX_WAH, 0, {{"Sens", KN_PLAIN, EN_FXS_WH_SENS, 0, 0}, {"Range", KN_PLAIN, EN_FXS_WH_RANGE, 0, 0},
                        {"Reso", KN_PLAIN, EN_FXS_WH_RESO, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_WH_MIX, 0, 0}}},
    /* the JAM page's four knobs (the UI's own: uiv 7 .. 10): not an FX page, the dial never walks to it */
    {"JAM", 63, 0, {{"Voice", KN_NAME, 7, JV_NAME, 4}, {"Keys", KN_NAME, 8, JM_NAME, 3}, {"Octave", KN_NAME, 9, JO_NAME, 5}, {"Answer", KN_NAME, 10, ONOFF_NAME, 2}}},
};
/* the gate's patterns (the loop's length in sixteenths; a pattern of 12 or 10 against the bar of 16 is a polymeter),
 * the last "CUSTOM": the keys' own. euc: a Euclidean pattern of that many hits instead of pat. And its targets. */
#define NGXP 13
#define NTGT 11
static const struct {
    const char *name;
    uint16_t pat;
    uint8_t len, euc;
} GX_PRE[NGXP] = {
    {"3-3-3", 0x5249, 16, 0}, {"TRES", 0x0049, 8, 0}, {"OFFBT", 0x4444, 16, 0}, {"EUC 5", 0, 16, 5},
    {"EUC 7", 0, 16, 7},      {"EUC 9", 0, 16, 9},    {"WALTZ", 0x0111, 12, 0}, {"CHOP", 0x0F0F, 16, 0},
    {"SPARSE", 0x0101, 16, 0}, {"DENSE", 0xEEEE, 16, 0}, {"DOTTED", 0x0249, 12, 0}, {"5 / 10", 0x0155, 10, 0},
    {"CUSTOM", 0, 16, 0}};
static const struct {
    const char *name;
    uint8_t mask;                       /* a bit a track (EN_TR_*) */
} GX_TGT[NTGT] = {{"NO DRUMS", 0xF0}, {"PAD", 0x40}, {"DRONE", 0x10}, {"ACID", 0x20}, {"DRUMS", 0x0F},
                    {"HATS", 0x02}, {"KICK", 0x01}, {"ALL", 0xFF}, {"NO KICK", 0xFE}, {"KICK+DRN", 0x11}, {"PLUCK", 0x80}};
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_PAD, P_BRIGHT, P_PADTONE, P_DETUNE},
    {P_PE_ATK, P_PE_HOLD, P_PE_REL, P_PE_REST},
    {P_PL_RATE, P_PL_DEPTH, P_PL_SHAPE, P_PL_DEST},
    {P_CHORD, P_VOICING, P_PL_CHO, P_CH_LOOP},     /* the harmony together, the pad's chorus, and how many chords repeat (Root is on Global, Drift on Tempo) */
    {P_BASS, P_DR_INST, P_DR_CUT, P_DR_RES},
    {P_DE_TRIG, P_DE_ATK, P_DE_DEC, P_DE_AMT},
    {P_DL_RATE, P_DL_DEPTH, P_DL_SHAPE, P_DL_DEST},
    {P_ACID, P_AC_CUT, P_AC_RES, P_AC_ENV},
    {P_AC_DEC, P_AC_ACC, P_AC_SLIDE, P_AC_WAVE},
    {P_AC_DENS, P_AC_LEN, P_AC_RANGE, P_AC_MUT},
    {P_CM_LVL, P_CM_SND, P_CM_DENS, P_CM_OCT},
    {P_CM_DEC, P_CM_STYLE, P_CM_RATE, P_CM_RANGE},
    {P_PULSE, P_DRIVE, P_SWING, P_PATTERN},
    {P_LK_KICK, P_LK_SNARE, P_LK_HAT, P_LK_PERC},
    {P_KIT_KICK, P_KIT_SNARE, P_KIT_HAT, P_KIT_PERC},
    {P_DD_CHANGE, P_DD_EVERY, P_DD_POOL, P_DD_MUTATE},
    {K_GRT, K_GRD, K_GRS, K_GRA},                  /* the groove: a lane chosen, its density, the pattern it plays, and all lanes' density */
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_RV_TYPE, P_RV_DAMP, P_RV_PRE, P_RV_X},       /* the reverb's own page: its type, damping, predelay, and the type's amount */
    {P_MC_RATIO, P_MC_THR, P_MC_MAKE, P_ML_CEIL},   /* the master bus: compressor and limiter */
    {K_MXT, K_MXL, K_MXP, K_MXS},                  /* the mixer: a layer chosen, its level, its place, its send (the level is also on the layer's own page) */
    {P_ROOT, P_THEME, P_TUNE, P_MIDI},
    {K_NONE, K_NONE, K_NONE, K_NONE},              /* Jam: its knobs are the FXP table's last page (view_knob) */
    {K_LVL, K_TUNE, K_DEC, P_PATTERN},
    {K_FXF, K_FXF + 1, K_FXF + 2, K_FXF + 3},      /* perform: the FX knobs, without the FX menu */
    {K_FXF, K_FXF + 1, K_FXF + 2, K_FXF + 3},      /* the visualizers: the same */
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Level", "Bright", "Sound", "Detune"},
    {"Attack", "Hold", "Release", "Rest"},
    {"Rate", "Depth", "Shape", "Target"},
    {"Chord", "Voicing", "Chorus", "Loop"},
    {"Level", "Sound", "Cutoff", "Reso"},
    {"Trigger", "Attack", "Decay", "Amount"},
    {"Rate", "Depth", "Shape", "Target"},
    {"Level", "Cutoff", "Reso", "Env mod"},
    {"Decay", "Accent", "Slide", "Sound"},
    {"Density", "Length", "Range", "Mutate"},
    {"Level", "Sound", "Density", "Octave"},
    {"Decay", "Style", "Rate", "Range"},
    {"Level", "Drive", "Swing", "Pattern"},
    {"K>Drone", "S>Pad", "H>Acid", "P>Clock"},
    {"Kick", "Snare", "Hats", "Perc"},
    {"Change", "Every", "Pool", "Mutate"},
    {"Lane", "Density", "Source", "All dens"},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Type", "Damping", "Predelay", "Amount"},
    {"Ratio", "Thresh", "Make-up", "Ceiling"},
    {"Track", "Level", "Pan", "Send"},
    {"Root", "Theme", "Tune", "MIDI out"},
    {"Voice", "Keys", "Octave", "Answer"},
    {"Level", "Tune", "Decay", "Pattern"},
    {"Filter", "Crush", "Throw", "Wash"},
    {"Filter", "Crush", "Throw", "Wash"},
};
/* the buttons that lead to pages: each to a group, pressed again the next page in it */
static const struct {
    uint8_t btn, first, n;
} GROUP[] = {
    {B_HOME, V_TEMPO, 1}, {B_ENV, V_PAD, 4}, {B_LFO, V_DRONE, 3}, {B_ARP, V_ACID, 5}, {B_SEQ, V_DRUMS, 5}, {B_GLO, V_GLOBAL, 1},
};
#define NGROUP (int)(sizeof GROUP / sizeof GROUP[0])

static const char *const LANE_SHORT[EN_NLANE] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN", "PERC"};
#define LANE_COL(l) ((l) == 0 ? RGB(255, 160, 60) : (l) == 1 ? RGB(255, 220, 60) : (l) == 2 ? RGB(255, 120, 220) : \
                     (l) == 3 ? RGB(80, 200, 255) : (l) == 4 ? RGB(150, 225, 255) : RGB(190, 140, 255))

/* the FX menu (FX on): each key an effect while it is held. Two banks, the SELECT dial between them. The first: the
 * beat repeats, reverse, tape stop, the drums' mutes (kick, hats, snare, perc), the throw, the fill, the filters, the
 * gates (1/16, 1/8, a pattern), the phaser, the flanger, the tape, the roll (it speeds up), the pump, the riser,
 * vinyl; one spare. The second, the beat effects: the scatter, the brake, the spin-back, half time (and the mutes
 * again). The other tracks' mutes and every solo are on GLO held (MIXK). */
#define FX_NONE 63                     /* a spare key: no effect (the engine's bit 63 is never used) */
#define FILL_KEY 25                    /* F#5 in the first bank: a drum fill in the next bar */
static uint8_t fx_bank;                /* 0 .. 2: the effects, the beat effects, Flex Beat */
static const uint8_t KEY_FX[4][NKEYS] = {
    {FX_SWEEP, FX_GATE16, FX_RING, FX_GATE8, FX_FREEZE, FX_GATEX, FX_RVSWELL,          /* F3 F#3 G3 G#3 A3 A#3 B3 */
     FX_TALK, FX_PHASER, FX_WAH, FX_FLANGER, FX_REV,                                   /* C4 C#4 D4 D#4 E4 */
     FX_STOP, FX_TAPE, FX_MUTE_KICK, FX_ROLL, FX_MUTE_HATS, FX_PUMP, FX_MUTE_SNARE,    /* F4 F#4 G4 G#4 A4 A#4 B4 */
     FX_MUTE_PERC, FX_RISER, FX_THROW, FX_VINYL, FX_FILL,                              /* C5 C#5 D5 D#5 E5 */
     FX_LP, FX_NONE, FX_HP},                                                           /* F5 F#5 G5 */
    {FX_RPT2, FX_RPT4, FX_RPT6, FX_RPT8, FX_RPT16, FX_RPT32, FX_RPT8T,                 /* the repeats together, slow to fast: F3 .. B3 */
     FX_RPT16T, FX_NONE, FX_SCATTER, FX_BRAKE, FX_SPIN,                                /* C4 (the last repeat) C#4 D4 D#4 E4 */
     FX_HALF, FX_SCRATCH, FX_MUTE_KICK, FX_REV, FX_MUTE_HATS, FX_STOP, FX_MUTE_SNARE,  /* F4 F#4 G4 G#4 A4 A#4 B4 (the mutes where they are on the first page) */
     FX_MUTE_PERC, FX_HATROLL, FX_HATTRIP, FX_SNROLL, FX_FLAM,                         /* C5 .. E5: the drum rolls */
     FX_DBLKICK, FX_NONE, FX_NONE},
    {FX_FLEX1, FX_FLEX2, FX_FLEX3, FX_FLEX4, FX_FLEX5, FX_FLEX6, FX_FLEX7,             /* Flex Beat: its fifteen patterns, F3 .. */
     FX_FLEX8, FX_FLEX9, FX_FLEX10, FX_FLEX11, FX_FLEX12, FX_FLEX13, FX_FLEX14, FX_FLEX15,
     FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE},
    {FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE,   /* the sampler: the keys are notes, not effects */
     FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE}};
static const char *const FX_LABEL[EN_NFX] = {
    [FX_RPT2] = "RPT 1/2", "RPT 1/4", "RPT 1/6", "RPT 1/8", "RPT1/16", "RPT1/32", "REVERSE", "TAPESTP",
    "M:KICK", "M:HATS", "M:SNARE", "M:PERC", "LOWPASS", "HIPASS",
    "GATE 16", "GATE 8", "GATE PAT", "THROW", "FILL", "PHASER", "FLANGER", "TAPE", "ROLL", "PUMP", "RISER", "VINYL",
    "SCATTER", "BRAKE", "SPIN BK", "HALFTIME", "SWEEP", "SCRATCH", "FREEZE", "RV SWELL", "RING MOD", "RPT 1/8T", "RPT1/16T", 
    "STUT 16", "STUT 32", "BT REPT", "HF REPT", "BT REV", "SKIP", "TRANCE", "GATE 8", "DOUBLE", "SHUFFLE", "BCK-FWD", "ROLL OUT", "BUILD", "SCRATCH", "DROPOUT", "TALK", "WAH", "HAT ROLL", "HAT TRIP", "SN ROLL", "FLAM", "DBL KICK",
    [FX_NONE] = ""};
/* every kind of effect its own colour (the same in every theme, so a key is found by its colour as well as its name):
 * repeats cyan, filters green, gates and the pump yellow, the drums' mutes red, the colour effects pink, the transport
 * moves (reverse, stops, scatter) orange, the builds violet */
#define KC_REPEAT RGB(80, 200, 255)
#define KC_FILTER RGB(120, 255, 130)
#define KC_GATE RGB(255, 220, 60)
#define KC_MUTE RGB(255, 96, 96)
#define KC_COLOR RGB(255, 120, 220)
#define KC_MOVE RGB(255, 160, 60)
#define KC_BUILD RGB(190, 140, 255)
#define KC_FLEX RGB(110, 240, 200)
static uint16_t fx_color(int fx)
{
    if (fx >= FX_FLEX1 && fx <= FX_FLEX15)
        return KC_FLEX;
    switch (fx) {
    case FX_RPT2: case FX_RPT4: case FX_RPT6: case FX_RPT8: case FX_RPT16: case FX_RPT32: case FX_RPT8T: case FX_RPT16T:
    case FX_HATROLL: case FX_HATTRIP: case FX_SNROLL: case FX_FLAM: case FX_DBLKICK: return KC_REPEAT;
    case FX_LP: case FX_HP: case FX_SWEEP: return KC_FILTER;
    case FX_GATE16: case FX_GATE8: case FX_GATEX: case FX_PUMP: return KC_GATE;
    case FX_MUTE_KICK: case FX_MUTE_HATS: case FX_MUTE_SNARE: case FX_MUTE_PERC: return KC_MUTE;
    case FX_PHASER: case FX_FLANGER: case FX_TAPE: case FX_VINYL: case FX_RING: case FX_TALK: case FX_WAH: return KC_COLOR;
    case FX_RISER: case FX_ROLL: case FX_FILL: case FX_THROW: case FX_RVSWELL: return KC_BUILD;
    default: return KC_MOVE;
    }
}
static inline uint64_t fxbit(int k) { return KEY_FX[fx_bank][k] == FX_NONE ? 0u : FXB(KEY_FX[fx_bank][k]); }
static inline uint32_t h32(uint64_t x) { return (uint32_t)(x ^ x >> 32); }   /* a 64-bit mask, for the hashes */

/* GLO held, the keys: every track's mute and solo, latched (en_trk). A code: 0..6 mute a track (EN_TR_*),
 * 8..14 solo it, 16 / 17 mute / solo the four drums together, 18 mutes and solos back; 20..23 a scene (tap: recall,\n * hold: store), 24 everything back (effects, knobs, mutes), 25 the auto-DJ on / off, 26 its length, 27 its new pieces, 28 undo
 * (and again: redo) the last big change, 29 tap tempo; -1 nothing. */
static const int8_t MIXK[NKEYS] = {
    0, 8, 1, 9, 2, 10, 28,                 /* F3 .. B3: kick, hats, snare (mute, the black key above it solos); undo */
    3, 11, 4, 12, 29,                      /* C4 .. E4: perc, drone; tap tempo */
    5, 13, 6, 14, 16, 17, 7,               /* F4 .. B4: acid, pad, the drums, the pluck's mute */
    20, 21, 22, 23, 24, 25, 15, 27};       /* C5 .. G5: scenes 1-4, reset all, the auto-DJ (on, length, new pieces) */
static const char *const MIX_NAME[8] = {"KICK", "HATS", "SNARE", "PERC", "DRONE", "ACID", "PAD", "PLUCK"};
/* the white keys, in order: the sequencer's steps; the black keys, in order */
static const uint8_t WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
#define TAP_MS 250u                    /* an FX key let go within this: latched; held longer: only played */
#define SEQ_HOLD 400u                  /* SEQ held this long: the sequencer */
#define DROP_HOLD 500u                 /* SAVE held this long: the drop, on the next bar */
#define HELP_HOLD 900u
#define KIT_QUIET 3000u                /* anything left unwritten: this long after a change, while stopped */
#define GLOB_QUIET 2000u               /* the theme, Tune, MIDI out: written this long after a change */
#define FACTORY_HOLD 1500u             /* SAVE held this long on Global: the piece to its factory settings */

/* ---------------------------------------------------------------- themes --- */
/* five looks for the screen (Global > Theme): a ground, its panel and lines, the text, an accent for what is
 * happening now, a primary for the music in three shades (the pad and the chord; the acid; the drone),
 * the drums' colour, and a colour for the sections and freeze. Each layer has a dark tint for shapes at rest. */
typedef struct {
    uint16_t bg, panel, line, dim, text, acc, pad, pad_t, acid, acid_t, bass, bass_t, pulse, pulse_t, frost;
    uint8_t skin;                      /* 1: the Imp look (the header's coil, the knob strip's tabs and big values) */
} theme_t;
static const theme_t THEMES[EN_NTHEME] = {
    /* Synth: charcoal, a cyberpunk purple, orange */
    {RGB(22, 21, 27), RGB(40, 36, 50), RGB(62, 56, 76), RGB(142, 134, 162), RGB(238, 234, 246), RGB(255, 128, 32),
     RGB(164, 76, 255), RGB(68, 40, 104), RGB(214, 128, 255), RGB(78, 50, 104), RGB(118, 70, 214), RGB(52, 36, 88),
     RGB(206, 196, 226), RGB(66, 60, 82), RGB(255, 150, 64)},
    /* Acid: black, a 303's yellow-green, hot pink */
    {RGB(12, 13, 10), RGB(30, 34, 24), RGB(52, 60, 40), RGB(140, 152, 118), RGB(236, 242, 222), RGB(255, 60, 160),
     RGB(196, 255, 40), RGB(54, 72, 16), RGB(240, 255, 120), RGB(70, 80, 30), RGB(120, 200, 30), RGB(40, 62, 14),
     RGB(210, 220, 190), RGB(56, 62, 46), RGB(255, 90, 180)},
    /* Ice: deep navy, cyan, a cold pink */
    {RGB(10, 16, 28), RGB(22, 34, 54), RGB(38, 56, 84), RGB(120, 150, 186), RGB(226, 240, 255), RGB(255, 84, 130),
     RGB(0, 200, 255), RGB(10, 56, 84), RGB(120, 228, 255), RGB(26, 70, 96), RGB(40, 130, 230), RGB(16, 44, 82),
     RGB(196, 216, 236), RGB(48, 64, 86), RGB(255, 120, 160)},
    /* Ember: black-brown, red, amber */
    {RGB(20, 12, 10), RGB(40, 24, 20), RGB(70, 40, 34), RGB(170, 130, 116), RGB(250, 232, 220), RGB(255, 176, 32),
     RGB(255, 46, 66), RGB(92, 20, 26), RGB(255, 110, 90), RGB(96, 38, 32), RGB(200, 40, 40), RGB(72, 18, 18),
     RGB(230, 200, 186), RGB(82, 56, 50), RGB(255, 200, 80)},
    /* Phosphor: a green screen's black, its green, a brighter green */
    {RGB(6, 14, 8), RGB(14, 32, 18), RGB(26, 58, 32), RGB(80, 160, 96), RGB(170, 255, 190), RGB(220, 255, 120),
     RGB(60, 255, 120), RGB(14, 70, 30), RGB(150, 255, 170), RGB(24, 76, 40), RGB(30, 190, 90), RGB(10, 56, 26),
     RGB(120, 220, 140), RGB(20, 52, 28), RGB(240, 255, 140)},
    /* Imp: warm black, bone, crimson, lime, pink; ember is for "now" (the playhead, the knob you touched) */
    {RGB(20, 14, 12), RGB(42, 30, 27), RGB(78, 56, 49), RGB(166, 140, 126), RGB(244, 232, 200), RGB(255, 212, 71),
     RGB(242, 230, 196), RGB(86, 78, 62), RGB(182, 242, 60), RGB(60, 80, 22), RGB(224, 52, 79), RGB(84, 22, 32),
     RGB(255, 143, 143), RGB(88, 52, 52), RGB(150, 210, 255), 1},
};
static const theme_t *th = &THEMES[0];
#define K_BG (th->bg)
#define K_PANEL (th->panel)
#define K_LINE (th->line)
#define K_DIM (th->dim)
#define K_TEXT (th->text)
#define K_ACC (th->acc)
#define K_PAD (th->pad)
#define K_PAD_T (th->pad_t)
#define K_ACID (th->acid)
#define K_ACID_T (th->acid_t)
#define K_BASS (th->bass)
#define K_BASS_T (th->bass_t)
#define K_PULSE (th->pulse)
#define K_PULSE_T (th->pulse_t)
#define K_FROST (th->frost)

uint32_t ui_splash_ms = 3200u;           /* the boot cards: RedtailWorks, then HYP-1 (the host's tests set 0) */

static struct {
    uint8_t view, last_dial;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t save_held;                 /* SAVE held long enough: the drop was asked for this hold */
    uint8_t seq_held;                  /* SEQ held long enough: the sequencer opened this hold */
    uint8_t fx_menu;                   /* the FX menu is on: the keys are effects, the knobs the FX knobs */
    int8_t fxk[4];                     /* the FX knobs' values (filter -50..50, crush, throw, wash): not stored */
    uint8_t grp_last[8];               /* the page last on in each button's group (a button from elsewhere returns there) */
    uint8_t fx_page;                   /* with the FX menu on: 0 the keys, 1.. an effect's edit page (FXP) */
    uint8_t fxs[EN_NFXS];              /* the effects' settings (en_fxs), mirrored; not stored */
    uint8_t gx_preset, gx_tgt, pm_tgt; /* the gate: its pattern's name (NGXP - 1: the keys' own); its targets; the pump's */
    uint16_t gx_pat;                   /* the gate's pattern: bit s, step s open */
    uint8_t trk_m, trk_s;              /* the tracks muted and soloed (GLO held, the keys): a bit a track, not stored */
    uint64_t scene_lock[4];            /* the four scenes: the effects locked on, the mutes, the solos, the FX knobs; */
    uint8_t scene_m[4], scene_s[4];    /* kept until power-off (not stored) */
    int8_t scene_k[4][4];
    uint8_t scene_set, scene_pend;     /* the slots stored; the keys down that may be a tap (recall) or a hold (store) */
    uint32_t scene_t[4];
    uint64_t undo_lock;                /* UNDO: what the last big change (a scene, reset, clear, the drums' group) left behind */
    uint32_t tap_t[4];                 /* tap tempo: the last taps */
    uint8_t undo_m, undo_s, undo_ok, tap_n;
    int8_t undo_k[4];
    uint8_t dj_on, dj_len, dj_piece;   /* the auto-DJ: on; its groove length (8, 16, 32, 64 bars); a new piece each round */
    uint8_t dj_break, dj_fillon;       /* its breakdown (0 short: 4 bars, 1 half the groove, 2 as long); a drum fill before it */
    uint8_t scene_quant, scene_q;      /* a scene waits for the next bar (1) or comes at once (0); the one waiting: slot + 1 */
    uint8_t key_set, key_on, sec_last; /* the drop's key change (0 same, 1 +2, 2 +3, 3 +5, 4 +7); it is on; the section last seen */
    uint8_t viz;                       /* the visualizer on the SELECT dial (VZ_*) */
    uint8_t centre_imp;                /* the sigil's middle: the eye (0) or the imp (SEL + HOME); kept with the visualizer's byte */
    uint8_t eu_n[EN_NLANE], eu_r[EN_NLANE];   /* the sequencer's Euclid tricks: each lane's hits and rotation, as last set */
    uint8_t flex_last;                 /* the Flex Beat pad pressed last (0..14) */
    uint8_t saver, lights, saver_on, saver_view, saver_viz;   /* the STAGE page: the screensaver (off, 1, 3, 10 minutes), the key lights (off, show); it is on; where it came from */
    uint32_t saver_t;
    float glide_f[4];                  /* a scene's FX knobs gliding: where they are, where they go, which */
    int8_t glide_t[4];
    uint8_t glide_on, glide_busy;
    uint8_t vox_dirty;                 /* the player's DX7 voices changed: written when you leave them alone a moment (voices_save) */
    uint8_t set_dirty;                 /* the effects' / performing settings changed: written when you leave them alone a moment */
    uint32_t fx_open_t;                /* when the FX menu was opened (a second tap soon after goes to the settings) */
    uint8_t fx_lastpg;                 /* the edit page of the effect key pressed last */
    uint8_t mix_trk;                   /* the mixer page's layer: pad, drone, acid, pluck, drums (0..4) */
    uint8_t gr_all;                    /* the groove page's all-lanes density, as last set */
    int8_t pat_key;                    /* a pattern key (SEL + white 1-6) held: a tap keeps the pattern, a hold plays it only while down */
    uint8_t pat_rev, pat_held, pat_up; /* the pattern to go back to (+1), the one held, and the key let go after a hold */
    uint32_t pat_t;
    uint8_t scene_g[4][13];            /* a scene's groove: the lanes' densities, their sources, the pattern */
    uint8_t jam_man;                   /* the voice the JAM page has taken off its line (1 + voice), 0 none */
    uint8_t jam_voice, jam_mode, jam_oct, jam_ans;   /* the JAM page: the voice (pluck, pad, acid, drone), the keys (scale, chords, harp), the octave (0..4: -2..+2), the answer */
    uint8_t jam_n[NKEYS], jam_vc[NKEYS], jam_note[NKEYS][4];   /* what each key is sounding, to let go */
    uint8_t ans_note[8], ans_dt[8], ans_cnt, ans_playing, ans_i;   /* the answer's memory: notes, the sixteenths between them; playing, at which */
    uint32_t ans_last, ans_t;
    uint32_t home_t;                   /* when HOME last went down (twice quickly: the visualizers) */
    uint8_t mood;                      /* the mood macro, 0 (chill) .. 100 (peak); 50 leaves the piece as it is */
    uint8_t dj_phase, dj_fill;         /* groove 0, breakdown 1, build 2; a fill in the bar before the breakdown */
    uint16_t dj_n;                     /* bars in the phase */
    uint64_t fx_mask;                  /* the effects held (and locked), as last sent */
    uint64_t fx_lock;                  /* the effects latched on */
    uint8_t lock_mode;                 /* (the old SAVE lock mode: gone, kept off) */
    uint32_t lock_keys;                /* keys that turned a latched effect off: not played while they're held */
    uint32_t fx_tap;                   /* FX keys down that may yet be a tap (a latch) */
    uint32_t fx_down_t[NKEYS];
    uint8_t lane;                      /* the sequencer's lane (OCT- / OCT+) */
    uint8_t save_global;               /* SAVE went down on Global: the piece is stored when it comes up */
    uint8_t store_wait;                /* a store while playing: the music fading out to write it (store_tick) */
    uint8_t store_kind;                /* and what: 0 the piece's, 1 the settings, 2 the globals (quiet_write) */
    uint32_t store_t;
    char store_msg[12];
    uint8_t factory_done;              /* SAVE held on Global: the piece went back to its factory settings */
    uint32_t shift_keys;               /* keys pressed while SEL (shift) was held: not notes, not effects */
    uint8_t piece_q;                   /* a piece waiting for the bar (SEL + PRESETS): its number + 1 */
    uint8_t shift_hint;                /* SEL held alone: its hint shown this hold */
    uint8_t key_voice[NKEYS];          /* a key playing a voice: its EN_V_* + 1 (the note-off goes there) */
    uint8_t splash, theme;             /* the boot cards are up (2: the HYP-1 card); the theme drawn */
    uint8_t splash_step;               /* the HYP-1 card's steps lit */
    uint32_t boot_t, act_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

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

int ui_dirty(void) { return project_pending(); }

static void edited(void) { proj.edited |= (uint16_t)(1u << proj.piece); }   /* the piece changed */

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

/* GLO held (not with SEL): the keys mute and solo the tracks. Shown from a short hold, or at once on a key. */
static int mix_show(void)
{
    return (ui.btn >> B_GLO & 1u) && !(ui.btn >> B_SEL & 1u) &&
           ((ui.btn_used >> B_GLO & 1u) || plat_ms() - ui.down_t[B_GLO] >= 350u);
}
static int mix_state(int k)
{
    int c = MIXK[k];
    if (c < 0)
        return 0;
    if (c < 8)
        return ui.trk_m >> c & 1u;
    if (c < 16)
        return ui.trk_s >> (c - 8) & 1u;
    if (c >= 20 && c <= 23)
        return ui.scene_set >> (c - 20) & 1u;
    if (c == 28)
        return ui.undo_ok;
    if (c == 25)
        return ui.dj_on;
    if (c == 27)
        return ui.dj_piece;
    return c == 16 ? (ui.trk_m & 15u) == 15u : c == 17 ? (ui.trk_s & 15u) == 15u : 0;
}
static void knob_set(int k, int v);
static void scene_reset(void);
static void undo_save(void)                    /* before a big change: what it will undo to */
{
    int k;
    ui.undo_lock = ui.fx_lock;
    ui.undo_m = ui.trk_m;
    ui.undo_s = ui.trk_s;
    for (k = 0; k < 4; k++)
        ui.undo_k[k] = ui.fxk[k];
    ui.undo_ok = 1;
}
static void undo_swap(void)                    /* UNDO: the state before the last big change, and (again) back */
{
    uint64_t l = ui.fx_lock;
    uint8_t m = ui.trk_m, s = ui.trk_s;
    int8_t kk[4];
    int k;
    if (!ui.undo_ok) {
        say("NOTHING", "TO UNDO");
        return;
    }
    for (k = 0; k < 4; k++)
        kk[k] = ui.fxk[k];
    ui.fx_lock = ui.undo_lock;
    ui.trk_m = ui.undo_m;
    ui.trk_s = ui.undo_s;
    en_trk(ui.trk_m, ui.trk_s);
    for (k = 0; k < 4; k++)
        knob_set(K_FXF + k, ui.undo_k[k]);
    ui.undo_lock = l;
    ui.undo_m = m;
    ui.undo_s = s;
    for (k = 0; k < 4; k++)
        ui.undo_k[k] = kk[k];
    say("UNDO", "(AGAIN: REDO)");
}
static void tap_tempo(void)                    /* four taps or so: the tempo from their average */
{
    const uint32_t now = plat_ms();
    if (ui.tap_n && now - ui.tap_t[(ui.tap_n - 1) & 3u] > 2000u)
        ui.tap_n = 0;                          /* too long since the last: a new count */
    ui.tap_t[ui.tap_n & 3u] = now;
    ui.tap_n++;
    if (ui.tap_n >= 2) {
        const int n = ui.tap_n < 4 ? ui.tap_n : 4;
        const uint32_t last = ui.tap_t[(ui.tap_n - 1) & 3u], first = ui.tap_t[(ui.tap_n - n) & 3u];
        const uint32_t bpm = (60000u * (uint32_t)(n - 1) + (last - first) / 2u) / (last - first);
        char t[12];
        knob_set(P_TEMPO, (int)bpm);
        itoa_u((uint32_t)proj.par[P_TEMPO], t);
        say("TEMPO", t);
    } else {
        say("TAP TEMPO", "TAP AGAIN");
    }
}
static const uint8_t DJ_LEN[5] = {8, 16, 32, 64, 16};   /* the fifth: SYNC, with the drift's chord changes (16 only for the bar of the count) */
#define DJ_G() (DJ_LEN[ui.dj_len % 5u])
static int dj_break_len(int g)
{
    return ui.dj_break == 0 ? 4 : ui.dj_break == 1 ? (g / 2 < 4 ? 4 : g / 2) : g;
}
static void mix_key(int k)
{
    int c = MIXK[k];
    uint8_t *m = &ui.trk_m, *s = &ui.trk_s;
    if (c < 0)
        return;
    if (c < 8) {
        *m ^= (uint8_t)(1u << c);
        say(*m >> c & 1u ? "MUTE" : "UNMUTE", MIX_NAME[c]);
    } else if (c < 16) {
        *s ^= (uint8_t)(1u << (c - 8));
        say(*s >> (c - 8) & 1u ? "SOLO" : "UNSOLO", MIX_NAME[c - 8]);
    } else if (c == 16 || c == 17) {          /* the four drums together: all on, or (all on already) all off */
        uint8_t *x = c == 16 ? m : s;
        undo_save();
        *x = (*x & 15u) == 15u ? (uint8_t)(*x & ~15u) : (uint8_t)(*x | 15u);
        say(c == 16 ? ((*x & 15u) ? "MUTE" : "UNMUTE") : ((*x & 15u) ? "SOLO" : "UNSOLO"), "ALL DRUMS");
    } else if (c == 18) {
        undo_save();
        *m = *s = 0;
        say("ALL TRACKS", "BACK");
    } else if (c >= 20 && c <= 23) {            /* a scene: a tap recalls it, a hold stores (scene_tick) */
        ui.scene_pend |= (uint8_t)(1u << (c - 20));
        ui.scene_t[c - 20] = plat_ms();
    } else if (c == 24) {
        scene_reset();
    } else if (c == 28) {
        undo_swap();
    } else if (c == 29) {
        tap_tempo();
    } else if (c == 25) {
        ui.dj_on = (uint8_t)!ui.dj_on;
        ui.dj_phase = 0;
        ui.dj_n = 0;
        ui.dj_fill = 0;
        say("AUTO-DJ", ui.dj_on ? "ON" : "OFF");
    } else if (c == 26) {
        char t[12];
        ui.dj_len = (uint8_t)((ui.dj_len + 1) % 5);
        if (ui.dj_len == 4) {
            const char *q = "WITH THE CHORDS";
            int n = 0;
            while (*q)
                t[n++] = *q++;
            t[n] = 0;
        } else {
            itoa_u(DJ_LEN[ui.dj_len], t);
            {   /* " BARS" after the number */
                int n = 0;
                const char *q = " BARS";
                while (t[n])
                    n++;
                while (*q)
                    t[n++] = *q++;
                t[n] = 0;
            }
        }
        say("DJ GROOVE", t);
        ui.dj_n = 0;
    } else if (c == 27) {
        ui.dj_piece = (uint8_t)!ui.dj_piece;
        say("DJ NEW PIECES", ui.dj_piece ? "ON" : "OFF");
    }
    en_trk(*m, *s);
}

/* the MOOD macro (the PERFORM screen, the ALGORITHM dial): 0 chill .. 100 peak, 50 leaves the piece as it is. Each setting listed
 * moves by its share of the distance from 50: the engine is given the moved value; the piece (proj.par), the screens and the
 * store keep their own. A setting turned by hand while the mood is off 50 is moved the same way. */
static const struct {
    uint8_t p;
    int8_t d;                                  /* the change at 0 (minus this) and at 100 (plus this) */
} MOOD[] = {{P_AC_DENS, 30}, {P_AC_CUT, 25}, {P_LANE_LVL(3), 25}, {P_LANE_LVL(4), 20}, {P_LANE_LVL(5), 25}, {P_BRIGHT, 20},
            {P_CM_DENS, 25}, {P_CM_LVL, 20}, {P_CM_DEC, -15}, {P_DR_CUT, 20},  {P_DRIVE, 20},  {P_REVERB, -15},     {P_ECHO, -10},        {P_EVOLVE, 20},       {P_PULSE, 10}};
#define NMOOD (int)(sizeof MOOD / sizeof MOOD[0])
static int mood_val(int p, int v)
{
    int i;
    if (ui.mood == 50 || (p == P_CM_LVL && v == 0))   /* (a pluck set to nothing stays nothing) */
        return v;
    for (i = 0; i < NMOOD; i++)
        if (MOOD[i].p == p) {
            const en_param_t *pi = en_param_info(p);
            v += MOOD[i].d * (ui.mood - 50) / 50;
            return v < pi->lo ? pi->lo : v > pi->hi ? pi->hi : v;
        }
    return v;
}
static void mood_apply(void)
{
    int i;
    for (i = 0; i < NMOOD; i++)
        en_set(MOOD[i].p, mood_val(MOOD[i].p, proj.par[MOOD[i].p]));
}

/* the knobs now: the page's, or with the FX menu on, the FX knobs */
static int view_knob(int i)
{
    if (ui.view == V_JAM && !ui.fx_menu)
        return K_FXP + 4 * (NFXP - 1) + i;      /* the JAM page: the last of the FXP table's pages */
    if (ui.fx_menu) {
        if (ui.fx_page && FXP[ui.fx_page - 1].c[i].kind == KN_NONE)
            return K_NONE;
        return ui.fx_page ? K_FXP + 4 * (ui.fx_page - 1) + i : K_FXF + i;
    }
    return VIEW_KNOB[ui.view][i];
}
static const char *view_label(int i)
{
    if (ui.view == V_JAM && !ui.fx_menu)
        return FXP[NFXP - 1].c[i].label;
    if (ui.fx_menu)
        return ui.fx_page ? FXP[ui.fx_page - 1].c[i].label : FXK_LABEL[i];
    return KNOB_LABEL[ui.view][i];
}

/* the FX edit pages' values: a setting, or the gate's pattern / targets (a name), kept here and sent (en_fxs) */
#define FXP_CTL(k) (FXP[((k) - K_FXP) / 4].c[((k) - K_FXP) % 4])
static void gx_load(int p)                     /* a pattern, and its natural length */
{
    const uint16_t pat = GX_PRE[p].euc ? (uint16_t)en_euclid(GX_PRE[p].euc, GX_PRE[p].len, 0) : GX_PRE[p].pat;
    ui.gx_preset = (uint8_t)p;
    ui.gx_pat = pat;
    ui.fxs[EN_FXS_GX_LEN] = GX_PRE[p].len;
    en_gate_pat(pat);
    en_fxs(EN_FXS_GX_LEN, GX_PRE[p].len);
}
static void gx_step(int s)                     /* a step of the gate's pattern on or off: the pattern is then the keys' own */
{
    ui.set_dirty = 1;
    ui.gx_pat ^= (uint16_t)(1u << s);
    ui.gx_preset = NGXP - 1;
    en_gate_pat(ui.gx_pat);
}
static uint8_t *uiv(int i)                     /* the PERFORM page's settings (the UI's own) */
{
    return i == 0 ? &ui.scene_quant : i == 1 ? &ui.dj_len : i == 2 ? &ui.dj_break : i == 3 ? &ui.dj_fillon : i == 4 ? &ui.key_set :
           i == 5 ? &ui.saver : i == 6 ? &ui.lights : i == 7 ? &ui.jam_voice : i == 8 ? &ui.jam_mode : i == 9 ? &ui.jam_oct : i == 10 ? &ui.jam_ans : &proj.genre;   /* 11: the genre (proj's own) */
}
#define FXP_UI(k) (FXP[((k) - K_FXP) / 4].bit == 63)   /* the PERFORM page: no effect, the UI's own settings */
static int fxp_get(int k)
{
    switch (FXP_CTL(k).kind) {
    case KN_PRESET: return ui.gx_preset;
    case KN_TGT: return ui.gx_tgt;
    case KN_PTGT: return ui.pm_tgt;
    default: return FXP_UI(k) ? *uiv(FXP_CTL(k).idx) : ui.fxs[FXP_CTL(k).idx];
    }
}
static void fxp_range(int k, int *lo, int *hi)
{
    const int kind = FXP_CTL(k).kind;
    *lo = kind == KN_LEN || kind == KN_RANGE ? 2 : 0;
    *hi = kind == KN_PRESET ? NGXP - 1 : kind == KN_LEN ? 16 : kind == KN_TGT || kind == KN_PTGT ? NTGT - 1 :
          kind == KN_NAME ? FXP_CTL(k).n - 1 : kind == KN_RANGE ? FXP_CTL(k).n : 100;
}
static void genre_step(int d);
static void fxp_set(int k, int v)
{
    int lo, hi;
    fxp_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v != fxp_get(k))
        ui.set_dirty = 1;                        /* a setting changed: written when it has been left alone a moment (settings_save) */
    switch (FXP_CTL(k).kind) {
    case KN_PRESET:
        if (v != ui.gx_preset && v < NGXP - 1)
            gx_load(v);
        else
            ui.gx_preset = (uint8_t)v;           /* (the end: the keys' own, kept as they are) */
        break;
    case KN_TGT:
        if (v != ui.gx_tgt) {
            ui.gx_tgt = (uint8_t)v;
            en_fxs(EN_FXS_GX_TGT, GX_TGT[v].mask);
        }
        break;
    case KN_PTGT:
        if (v != ui.pm_tgt) {
            ui.pm_tgt = (uint8_t)v;
            en_fxs(EN_FXS_PM_TGT, GX_TGT[v].mask);
        }
        break;
    default:
        if (FXP_UI(k) && FXP_CTL(k).idx == 11) {   /* the genre: a switch of bank (the way GLO + PRESETS does it) */
            if (v != proj.genre)
                genre_step(v - proj.genre);
            break;
        }
        if (FXP_UI(k)) {
            *uiv(FXP_CTL(k).idx) = (uint8_t)v;
            break;
        }
        if (v != ui.fxs[FXP_CTL(k).idx]) {
            ui.fxs[FXP_CTL(k).idx] = (uint8_t)v;
            en_fxs(FXP_CTL(k).idx, v);
            if (FXP_CTL(k).kind == KN_LEN)
                ui.gx_preset = NGXP - 1;
        }
    }
}
static void fxp_text(int k, char *b)
{
    const char *s = 0;
    const int v = fxp_get(k);
    if (FXP_CTL(k).kind == KN_PRESET)
        s = GX_PRE[v].name;
    else if (FXP_CTL(k).kind == KN_TGT || FXP_CTL(k).kind == KN_PTGT)
        s = GX_TGT[v].name;
    else if (FXP_CTL(k).kind == KN_NAME)
        s = FXP_CTL(k).names[v];
    if (s) {
        int n = 0;
        while (*s && n < 9)
            b[n++] = *s++;
        b[n] = 0;
    } else if (FXP_CTL(k).kind == KN_SEC) {       /* seconds: 2 ^ (v * 0.036) */
        itoa_u((uint32_t)(fm_exp2f((float)v * 0.036f) + 0.5f), b);
        {
            int n = 0;
            while (b[n])
                n++;
            b[n] = 'S';
            b[n + 1] = 0;
        }
    } else {
        itoa_u((uint32_t)v, b);
    }
}

/* ------------------------------------------------------------ values --- */
static int knob_par(int k)                     /* a knob's parameter (the sequencer's: its lane's) */
{
    if (k == K_LVL)
        return P_LANE_LVL(ui.lane);
    if (k == K_TUNE)
        return P_LANE_TUNE(ui.lane);
    if (k == K_DEC)
        return P_LANE_DEC(ui.lane);
    if (k == K_MXL) {                            /* the mixer's layer: its level */
        static const uint8_t LV[5] = {P_PAD, P_BASS, P_ACID, P_CM_LVL, P_PULSE};
        return LV[ui.mix_trk % 5u];
    }
    if (k == K_GRD)
        return P_DENS + (int)(ui.lane % EN_NLANE);
    if (k == K_GRS)
        return P_LSRC + (int)(ui.lane % EN_NLANE);
    if (k == K_MXP)
        return P_PAN + (int)(ui.mix_trk % 5u);
    if (k == K_MXS)
        return P_SEND + (int)(ui.mix_trk % 5u);
    return k < P_NPARAMS ? k : -1;
}
static const char *const MXT_NAME[5] = {"PAD", "DRONE", "ACID", "PLUCK", "DRUMS"};
static int knob_get(int k)
{
    int p = knob_par(k);
    if (k == K_MXT)
        return ui.mix_trk;
    if (k == K_GRT)
        return ui.lane;
    if (k == K_GRA)
        return ui.gr_all;
    if (k >= K_FXP)
        return fxp_get(k);
    if (k >= K_FXF)
        return ui.fxk[k - K_FXF];
    return p >= 0 ? proj.par[p] : 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    int p = knob_par(k);
    if (k == K_MXT) {
        *lo = 0;
        *hi = 4;
        return;
    }
    if (k == K_GRT || k == K_GRA) {
        *lo = 0;
        *hi = k == K_GRT ? EN_NLANE - 1 : 100;
        return;
    }
    if (k >= K_FXP) {
        fxp_range(k, lo, hi);
        return;
    }
    if (k >= K_FXF) {                            /* the filter is bipolar, the rest 0..100 */
        *lo = k == K_FXF ? -50 : 0;
        *hi = k == K_FXF ? 50 : 100;
        return;
    }
    *lo = p >= 0 ? en_param_info(p)->lo : 0;
    *hi = p >= 0 ? en_param_info(p)->hi : 0;
}
static void knob_text(int k, char *b)
{
    int p = knob_par(k);
    if (k == K_MXT) {
        const char *s = MXT_NAME[ui.mix_trk % 5u];
        int n = 0;
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_GRT) {                            /* the groove's lane */
        const char *s = LANE_SHORT[ui.lane % EN_NLANE];
        int n = 0;
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_GRA) {
        itoa_u((uint32_t)ui.gr_all, b);
        return;
    }
    if (k >= K_FXP) {
        fxp_text(k, b);
        return;
    }
    if (k >= K_FXF) {
        int v = ui.fxk[k - K_FXF], n = 0;
        if (k == K_FXF) {                        /* "LP 40", "HP 40", or off in the dead zone */
            if (v > -3 && v < 3) {
                b[0] = 'O';
                b[1] = 'F';
                b[2] = 'F';
                b[3] = 0;
                return;
            }
            b[n++] = v < 0 ? 'L' : 'H';
            b[n++] = 'P';
            b[n++] = ' ';
            v = v < 0 ? -v : v;
            v *= 2;
        }
        itoa_u((uint32_t)v, b + n);
        return;
    }
    if (p >= 0)
        en_param_text(p, proj.par[p], b);
    else
        b[0] = 0;
}

static void knob_set(int k, int v)
{
    int lo, hi, p = knob_par(k);
    if (k == K_MXT) {
        ui.mix_trk = (uint8_t)(v < 0 ? 0 : v > 4 ? 4 : v);
        return;
    }
    if (k == K_GRT) {
        ui.lane = (uint8_t)(v < 0 ? 0 : v > EN_NLANE - 1 ? EN_NLANE - 1 : v);
        return;
    }
    if (k == K_GRA) {                            /* every lane's density at once */
        int l;
        v = v < 0 ? 0 : v > 100 ? 100 : v;
        ui.gr_all = (uint8_t)v;
        for (l = 0; l < EN_NLANE; l++)
            knob_set(P_DENS + l, v);
        return;
    }
    if (k >= K_FXP) {
        fxp_set(k, v);
        return;
    }
    if (k >= K_FXF) {                            /* the FX knobs: the engine's, never stored */
        if (!ui.glide_busy)
            ui.glide_on &= (uint8_t)~(1u << (k - K_FXF));   /* (turned by hand: a gliding one is let go) */
        knob_range(k, &lo, &hi);
        v = v < lo ? lo : v > hi ? hi : v;
        if (v != ui.fxk[k - K_FXF]) {
            ui.fxk[k - K_FXF] = (int8_t)v;
            en_fxk(k - K_FXF, v);
        }
        return;
    }
    if (p < 0)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == proj.par[p])
        return;
    proj.par[p] = (int16_t)v;
    en_set(p, mood_val(p, v));
    if (en_par_global(p))
        project_globals();                       /* the player's: stored at once */
    else
        edited();
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
    if (!e || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------ scenes --- */
/* Four scenes (GLO held, C5 .. D#5): what is locked on in FX, the mutes and solos, the FX knobs. A tap
 * recalls one, a hold of 0.6 s stores the state as it is now. They are kept until power-off. */
#define SCENE_HOLD 600u
static void pat_name(int i, char *t);
static void scene_store(int s)
{
    int k;
    ui.scene_lock[s] = ui.fx_lock;
    ui.scene_m[s] = ui.trk_m;
    ui.scene_s[s] = ui.trk_s;
    for (k = 0; k < 4; k++)
        ui.scene_k[s][k] = ui.fxk[k];
    for (k = 0; k < EN_NLANE; k++) {           /* the groove: densities, sources, the pattern */
        ui.scene_g[s][k] = (uint8_t)proj.par[P_DENS + k];
        ui.scene_g[s][6 + k] = (uint8_t)proj.par[P_LSRC + k];
    }
    ui.scene_g[s][12] = (uint8_t)proj.par[P_PATTERN];
    ui.scene_set |= (uint8_t)(1u << s);
    {
        char t[12] = "SCENE 1";
        t[6] = (char)('1' + s);
        say(t, "STORED");
    }
}
static void scene_apply(int s)
{
    char t[12] = "SCENE 1";
    int k;
    t[6] = (char)('1' + s);
    undo_save();
    ui.fx_lock = ui.scene_lock[s];             /* (the effects follow from the lock, in the next frame) */
    ui.trk_m = ui.scene_m[s];
    ui.trk_s = ui.scene_s[s];
    en_trk(ui.trk_m, ui.trk_s);
    for (k = 0; k < EN_NLANE; k++) {           /* the groove: from the next bar (the engine's own wait) */
        knob_set(P_DENS + k, ui.scene_g[s][k]);
        knob_set(P_LSRC + k, ui.scene_g[s][6 + k]);
    }
    knob_set(P_PATTERN, ui.scene_g[s][12]);
    for (k = 0; k < 4; k++) {                  /* the FX knobs glide there (glide_tick) */
        ui.glide_t[k] = ui.scene_k[s][k];
        ui.glide_f[k] = (float)ui.fxk[k];
    }
    ui.glide_on = 15;
    say(t, 0);
}
static void scene_recall(int s)                /* a scene: on the next bar while playing (a setting), else at once */
{
    char t[12] = "SCENE 1";
    t[6] = (char)('1' + s);
    if (!(ui.scene_set >> s & 1u)) {
        say(t, "EMPTY: HOLD TO STORE");
        return;
    }
    if (ui.scene_quant && en_playing) {
        ui.scene_q = (uint8_t)(s + 1);          /* (another tapped before the bar replaces it) */
        say(t, "NEXT BAR");
    } else {
        ui.scene_q = 0;
        scene_apply(s);
    }
}
static void scene_reset(void)                  /* everything back: no effects locked, no mutes or solos, the FX knobs home */
{
    int k;
    undo_save();
    ui.fx_lock = 0;
    ui.trk_m = ui.trk_s = 0;
    en_trk(0, 0);
    for (k = 0; k < 4; k++)
        knob_set(K_FXF + k, 0);
    say("RESET", "ALL CLEAR");
}
static void scene_tick(void)                   /* a scene key down: held long enough, stored; let go first, recalled */
{
    int s;
    for (s = 0; s < 4; s++)
        if (ui.scene_pend >> s & 1u) {
            if (!(ui.keys >> (19 + s) & 1u)) {
                ui.scene_pend &= (uint8_t)~(1u << s);
                scene_recall(s);
            } else if (plat_ms() - ui.scene_t[s] >= SCENE_HOLD) {
                ui.scene_pend &= (uint8_t)~(1u << s);
                scene_store(s);
            }
        }
}

/* --------------------------------------------------------------- auto-DJ --- */
/* On (GLO held, F5): the sections run by themselves, on the bar clock. A groove (8, 16 or 32 bars, F#5); the
 * bar before it ends a drum fill; then a breakdown (half as long, 4 bars at least), a build (four bars, the
 * riser rising, and the engine drops by itself), and the groove again. With new pieces on (G5), the next piece
 * comes in on that fill's bar, so the breakdown and the drop belong to it. It follows the engine's section, so
 * SAVE pressed by hand just moves it on from there. */
static void dj_bar(void)                       /* a new bar (while playing) */
{
    const int g = DJ_G(), b = dj_break_len(g), sync = ui.dj_len == 4;
    const int ph = en_sec == EN_SEC_GROOVE ? 0 : en_sec == EN_SEC_BREAK ? 1 : 2;
    if (ph != ui.dj_phase) {                   /* the engine moved on (the DJ's request, or SAVE): so does the count */
        ui.dj_phase = (uint8_t)ph;
        ui.dj_n = 0;
    }
    ui.dj_fill = 0;
    ui.dj_n++;
    if (ph == 0 && sync) {                     /* with the chords: the engine fills the bar before a change and breaks on it; the next piece comes in as the fill begins */
        if (en_drift_bars <= 2.0f && ui.dj_piece && !ui.piece_q)
            ui.piece_q = (uint8_t)((proj.piece + 1) % NPIECES + 1);
    } else if (ph == 0) {
        if (ui.dj_n == (uint16_t)(g - 1)) {    /* the fill is the LAST bar of the groove (asked a bar ahead: the engine starts it on the bar line); the break follows at once */
            if (ui.dj_fillon)
                en_fill_next();
            if (ui.dj_piece && !ui.piece_q)
                ui.piece_q = (uint8_t)((proj.piece + 1) % NPIECES + 1);
        }
        if (ui.dj_n >= (uint16_t)g && !en_sec_req)
            en_section(EN_CMD_BREAK);
    } else if (ph == 1) {
        if (ui.dj_n >= (uint16_t)b && !en_sec_req)
            en_section(EN_CMD_BUILD);
    }
}
static void dj_tick(void)                    /* each new bar while playing: a scene waiting, the auto-DJ's count */
{
    static uint8_t last = 255;
    uint8_t p = en_drum_pos;
    if (ui.pat_rev && ui.pat_up && (!en_playing || en_pat_now == ui.pat_held)) {   /* a held pattern, let go: the one before, from the next bar */
        char t[16];
        knob_set(P_PATTERN, ui.pat_rev - 1);
        pat_name(ui.pat_rev - 1, t);
        say(t, en_playing ? "BACK, NEXT BAR" : 0);
        ui.pat_rev = ui.pat_up = 0;
    }
    if (!ui.dj_on)
        ui.dj_fill = 0;
    if (!en_playing) {
        last = 255;
        ui.scene_q = 0;
        return;
    }
    {   /* the engine follows the chords while the DJ is on in SYNC and in a groove */
        static uint8_t sent;
        const uint8_t want = (uint8_t)(ui.dj_on && ui.dj_len == 4 && en_sec == EN_SEC_GROOVE);
        if (want != sent) {
            sent = want;
            en_dj_sync(want);
        }
    }
    if (p != last) {
        if (p == 0 && last != 255) {
            if (ui.scene_q) {
                const int s = ui.scene_q - 1;
                ui.scene_q = 0;
                scene_apply(s);
            }
            if (ui.dj_on)
                dj_bar();
        }
        last = p;
    }
}

/* the key change on the drop (the DROP page: SAME, +2, +3, +5, +7 semitones): when a breakdown or a build gives way to the groove, the
 * root rises for the round; the next breakdown (or a stop, or another piece) puts it back. The engine alone moves; the piece's
 * own root (the Global page) stays. */
static void key_tick(void)
{
    static const uint8_t SH[5] = {0, 2, 3, 5, 7};
    const int sec = en_playing ? en_sec : EN_SEC_GROOVE, sh = SH[ui.key_set % 5];
    if (sec != ui.sec_last) {
        if (sec == EN_SEC_GROOVE && ui.sec_last != EN_SEC_GROOVE && sh && en_playing && !ui.key_on) {
            char t[8] = "+0";
            ui.key_on = 1;
            en_set(P_ROOT, (proj.par[P_ROOT] + sh) % 12);
            t[1] = (char)('0' + sh);
            say("KEY CHANGE", t);
        } else if (sec == EN_SEC_BREAK && ui.key_on) {
            ui.key_on = 0;
            en_set(P_ROOT, proj.par[P_ROOT]);
        }
        ui.sec_last = (uint8_t)sec;
    }
    if (!en_playing && ui.key_on) {
        ui.key_on = 0;
        en_set(P_ROOT, proj.par[P_ROOT]);
    }
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        say("THE ROOT STAYS", "(GLOBAL)");
        return;
    }
    if (pool_count(p) < 3) {
        say("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    edited();
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    edited();
    say("MODE", EN_MODE_NAME[m]);
}

static void caps(char *d, const char *s, int n);

/* a title for each piece, made from two short word lists by its genre and number (the same one every time): "VELVET RAIN" */
static const char *const TITLE_ADJ[16] = {"VELVET", "NEON", "DUSTY", "HOLLOW", "MIDNIGHT", "GLASS", "SLOW", "ELECTRIC",
                                          "PAPER", "COLD", "GOLDEN", "FADED", "LIQUID", "WILD", "QUIET", "STATIC"};
static const char *const TITLE_NOUN[16] = {"RAIN", "TIDE", "ROOM", "SIGNAL", "GARDEN", "ENGINE", "SKY", "ORBIT",
                                           "HIGHWAY", "MIRROR", "HARBOR", "FOREST", "CIRCUIT", "LIGHTS", "ECHO", "DREAM"};
static void piece_title(char *b, int n)
{
    uint32_t h = ((uint32_t)proj.genre * 16u + proj.piece + 1u) * 2654435761u;
    const char *a = TITLE_ADJ[h >> 28 & 15u], *w = TITLE_NOUN[h >> 20 & 15u];
    int k = 0;
    while (*a && k < n - 1)
        b[k++] = *a++;
    if (k < n - 1)
        b[k++] = ' ';
    while (*w && k < n - 1)
        b[k++] = *w++;
    b[k] = 0;
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    ui.key_on = 0;                               /* (project_apply puts the key back) */
    project_apply();
    mood_apply();
    en_seed(plat_ms());
    en_regrow();
    {
        char t[12] = "PIECE ";
        caps(t + 6, piece_name(proj.piece), 4);
        char u[20];
        piece_title(u, sizeof u);
        say(t, u);                               /* (its title; STORED or FACTORY is on the Global page) */
    }
}

/* the genres (GLO held + PRESETS): another bank of ten pieces, the same number playing; the bank's effects come with it
 * (the lofi one: the vinyl and the tape latched on, a gentler wobble and dust). The first bank is the one that is stored. */
static void fxs_put(int i, int v)
{
    ui.fxs[i] = (uint8_t)v;
    en_fxs(i, v);
}
static const uint8_t GENRE_FXS[12] = {EN_FXS_TP_WOW, EN_FXS_TP_FLUT, EN_FXS_TP_DRIVE, EN_FXS_TP_DULL,   /* the settings a genre sets (and a stored set leaves to it) */
                                      EN_FXS_VN_LEVEL, EN_FXS_VN_CRACK, EN_FXS_VN_HISS, EN_FXS_VN_TONE,
                                      EN_FXS_PM_DEPTH, EN_FXS_PM_REL, EN_FXS_PM_RATE, EN_FXS_PM_TGT};
static void genre_fx(int g)
{
#define SET GENRE_FXS
    static const uint8_t LOFI[8] = {25, 18, 30, 45, 42, 40, 42, 38};             /* tape and vinyl, gentler (the vinyl: less level, a darker tone, more hiss than crackle) */
    static const uint8_t HOUSE[4] = {50, 45, 1, 0x70};                           /* the pump: depth, release, every quarter, the chords and bass */
    int i;
    ui.fx_lock &= ~(FXB(FX_VINYL) | FXB(FX_TAPE) | FXB(FX_PUMP) | FXB(FX_PHASER));
    for (i = 0; i < 12; i++)
        fxs_put(SET[i], EN_FXS_DEF[SET[i]]);
    if (g == 1) {                                /* lofi: the vinyl and the tape on */
        for (i = 0; i < 8; i++)
            fxs_put(SET[i], LOFI[i]);
        ui.fx_lock |= FXB(FX_VINYL) | FXB(FX_TAPE);
    } else if (g == 2) {                         /* house: the pump on */
        for (i = 0; i < 4; i++)
            fxs_put(SET[8 + i], HOUSE[i]);
        ui.fx_lock |= FXB(FX_PUMP);
    } else if (g >= 4) {                         /* the other six: each its tape, vinyl and pump (255: as it is), and what is latched (1 vinyl, 2 tape, 4 pump, 8 phaser) */
        static const uint8_t XFX[6][12] = {
            {10, 12, 45, 35, 32, 30, 38, 45, 255, 255, 255, 255},     /* jungle: sampled-from-vinyl grit */
            {5, 5, 25, 20, 255, 255, 255, 255, 30, 30, 1, 0xF0},      /* dnb: a light pump, a very light tape */
            {20, 15, 60, 30, 255, 255, 255, 255, 255, 255, 255, 255}, /* breaks: tape drive, the phaser */
            {25, 10, 25, 40, 255, 255, 255, 255, 30, 35, 1, 0xF0},    /* garage: a light tape and pump */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* electro: the phaser alone */
            {255, 255, 255, 255, 255, 255, 255, 255, 45, 55, 0, 0x50}};     /* trap: a pump under the 808 */
        static const uint8_t XLK[6] = {3, 6, 10, 6, 8, 4};
        for (i = 0; i < 12; i++)
            if (XFX[g - 4][i] != 255)
                fxs_put(SET[i], XFX[g - 4][i]);
        ui.fx_lock |= ((XLK[g - 4] & 1u) ? FXB(FX_VINYL) : 0u) | ((XLK[g - 4] & 2u) ? FXB(FX_TAPE) : 0u) | ((XLK[g - 4] & 4u) ? FXB(FX_PUMP) : 0u) |
                      ((XLK[g - 4] & 8u) ? FXB(FX_PHASER) : 0u);
    }
#undef SET
}
static void genre_step(int d)
{
    const int g = (proj.genre + d + EN_NGENRE) % EN_NGENRE;
    project_set_genre(g);
    ui.key_on = 0;
    project_apply();
    mood_apply();
    en_seed(plat_ms());
    en_regrow();
    ui.piece_q = 0;
    genre_fx(g);
    {
        char u[20];
        piece_title(u, sizeof u);
        say(en_genre_name(g), u);
    }
}

/* into flash now. Writing it stops the chip a moment (it runs from the same flash), so while the music
 * plays it fades out (en_hush), holds still while the store is written, and fades back in where it was:
 * a short pause, not a cut. Stopped: at once. */
static void store_now(const char *done)
{
    int i;
    for (i = 0; done[i] && i < (int)sizeof ui.store_msg - 1; i++)
        ui.store_msg[i] = done[i];
    ui.store_msg[i] = 0;
    ui.store_kind = 0;
    if (en_playing) {
        en_hush(1);
        ui.store_wait = 1;
        ui.store_t = plat_ms();
        say("STORING", 0);
    } else {
        say(ui.store_msg, project_flush() == 0 ? 0 : "NOT WRITTEN");
    }
}
static void settings_save(void);
/* an autosave while the music plays: the same fade out, write, fade in (the write stops the chip, so unfaded it is a crack and an xrun);
 * kind 1: the effects' and performing settings; 2: the theme, Tune, MIDI out */
static void voices_save(void);
static void quiet_write(int kind)
{
    if (!en_playing) {
        if (kind == 1)
            settings_save();
        else if (kind == 3)
            voices_save();
        else
            project_flush_globals();
        return;
    }
    ui.store_msg[0] = 0;
    ui.store_kind = (uint8_t)kind;
    en_hush(1);
    ui.store_wait = 1;
    ui.store_t = plat_ms();
}
static void store_tick(void)                    /* the music faded out (or long enough): write, then back in */
{
    if (!ui.store_wait || (!en_hushed && plat_ms() - ui.store_t < 150u))
        return;
    ui.store_wait = 0;
    if (ui.store_kind == 1) {
        settings_save();
    } else if (ui.store_kind == 3) {
        voices_save();
    } else if (ui.store_kind == 2) {
        project_flush_globals();
    } else {
        const int r = project_flush();
        if (ui.store_msg[0])
            say(ui.store_msg, r == 0 ? 0 : "NOT WRITTEN");
    }
    ui.store_kind = 0;
    en_hush(0);
}

/* an update session is coming (the music already stopped): what waits for flash is written now and a store's fade is undone, since
 * ui_init afterwards starts from nothing (left alone, a store caught mid-fade kept the output hushed and unsaved voices were lost) */
void ui_flush(void)
{
    if (ui.store_wait) {
        ui.store_t = plat_ms() - 150u;
        store_tick();
    }
    if (ui.vox_dirty) {
        ui.vox_dirty = 0;
        voices_save();
    }
    if (ui.set_dirty) {
        ui.set_dirty = 0;
        settings_save();
    }
    if (project_pending())
        project_flush();
    en_hush(0);
}

static void piece_store(void)                   /* Global, SAVE: the piece as it is, stored */
{
    char t[12] = "STORED ";
    if (proj.genre) {
        say("NOT STORED", "ONLY THE FIRST BANK");
        return;
    }
    project_piece_save();
    caps(t + 7, piece_name(proj.piece), 4);
    store_now(t);
}

static void piece_factory(void)                 /* Global, SAVE held: back to the factory settings */
{
    char t[12] = "FACTORY ";
    project_piece_factory();
    project_apply();
    mood_apply();
    en_regrow();
    caps(t + 8, piece_name(proj.piece), 4);
    store_now(t);
}

/* ------------------------------------------------------------ the kit --- */
static int seq_pat(void) { return proj.par[P_PATTERN]; }   /* the pattern the sequencer edits: the one chosen */
static void seq_trick(int k, int32_t e)         /* SEL + K1: the lane as a Euclidean pattern of n hits in 16; K2: turned round; K3: how likely a hit is to sound */
{
    char t[20];
    const int l = ui.lane;
    int n = ui.eu_n[l], r = ui.eu_r[l];
    if (k == 3) {                               /* K4: the lane's length, 1 .. 16 steps: shorter than the bar, it runs against it */
        const int p = P_LLEN + l;
        knob_set(p, proj.par[p] + (int)e);
        itoa_u((uint32_t)proj.par[p], t);
        {
            int j = 0;
            while (t[j])
                j++;
            t[j++] = ' ', t[j++] = 'S', t[j++] = 'T', t[j++] = 'E', t[j++] = 'P', t[j++] = 'S', t[j] = 0;
        }
        say("LANE LENGTH", t);
        return;
    }
    if (k == 2) {
        const int p = P_PROB + l;
        knob_set(p, proj.par[p] + 5 * (int)e);
        itoa_u((uint32_t)proj.par[p], t);
        t[3] = 0;
        {
            int j = 0;
            while (t[j])
                j++;
            t[j++] = '%';
            t[j] = 0;
        }
        say("PROBABILITY", t);
        return;
    }
    if (k == 0)
        n = n + (int)e < 0 ? 0 : n + (int)e > 16 ? 16 : n + (int)e;
    else
        r = (r + (int)e + 16) & 15;
    ui.eu_n[l] = (uint8_t)n;
    ui.eu_r[l] = (uint8_t)r;
    en_pats[seq_pat()][l] = (uint16_t)en_euclid(n, 16, r);
    edited();
    {
        int j = 0;
        const char *s = "EUCLID ";
        while (*s)
            t[j++] = *s++;
        itoa_u((uint32_t)n, t + j);
        while (t[j])
            j++;
        t[j++] = '/';
        t[j++] = '1';
        t[j++] = '6';
        t[j] = 0;
    }
    {
        char u[12] = "ROT ";
        itoa_u((uint32_t)r, u + 4);
        say(t, u);
    }
}

static void step_toggle(int s)                  /* a hit on or off: the lane's step s (its white key) */
{
    volatile uint16_t *p = &en_pats[seq_pat()][ui.lane];
    *p = (uint16_t)(*p ^ (1u << s));
    edited();
}

/* ------------------------------------------------------------- shift --- */
static void pat_name(int i, char *t)            /* "PATTERN 3" */
{
    const char *p = "PATTERN ";
    while (*p)
        *t++ = *p++;
    *t++ = (char)('1' + i);
    *t = 0;
}

/* the order the dial and SEL + FX walk the settings pages in (their numbers stay: the scopes and the keys use them): Talk and Wah beside the phaser and flanger */
static const uint8_t FX_ORDER[NFXP - 1] = {1, 2, 3, 21, 22, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
static int fx_pos(int pg)                       /* where a page is in the walk (0 ..), -1: none */
{
    int i;
    for (i = 0; i < NFXP - 1; i++)
        if (FX_ORDER[i] == pg)
            return i;
    return -1;
}
static int fx_page_of(int fx)                   /* the edit page (1 ..) of an effect, 0: it has none */
{
    int p;
    if (fx == FX_GATE16 || fx == FX_GATE8)
        fx = FX_GATEX;
    for (p = 0; p < NFXP; p++)
        if (FXP[p].bit == fx)
            return p + 1;
    return 0;
}
static int fx_key_page(int key)                 /* the edit page of the effect a key plays (in this bank), 0: none */
{
    const int fx = KEY_FX[fx_bank][key];
    return fx_bank == 3 ? 20 : fx >= FX_FLEX1 && fx <= FX_FLEX15 ? 19 : fx == FX_NONE ? 0 : fx_page_of(fx);
}
static void shift_key(int key)                  /* a key pressed with SEL held */
{
    char t[16];
    int i;
    if (ui.fx_menu && !ui.fx_page) {             /* in the FX menu: SEL + a key opens that effect's settings page */
        const int fx = KEY_FX[fx_bank][key];
        int pg = fx_key_page(key);
        if (fx >= FX_FLEX1 && fx <= FX_FLEX15)
            ui.flex_last = (uint8_t)(fx - FX_FLEX1);
        if (pg) {
            ui.fx_page = (uint8_t)pg;
            say(FXP[pg - 1].name, "SETTINGS");
        } else {
            say(fx == FX_NONE ? "NO EFFECT" : FX_LABEL[fx], "HAS NO SETTINGS");
        }
        return;
    }
    for (i = 0; i < EN_NPAT; i++)
        if (WHITE_KEY[i] == key) {               /* white 1-6: the pattern */
            if (!ui.pat_rev)
                ui.pat_rev = (uint8_t)(proj.par[P_PATTERN] + 1);   /* (a tap keeps it; held, it plays only while down: the key's release says which) */
            ui.pat_key = (int8_t)key;
            ui.pat_held = (uint8_t)i;
            ui.pat_up = 0;
            ui.pat_t = plat_ms();
            knob_set(P_PATTERN, i);
            pat_name(i, t);
            say(t, en_playing && en_pat_now != i ? "NEXT BAR" : 0);
            return;
        }
    if (ui.view != V_SEQ)
        return;
    for (i = 0; i < EN_NPAT; i++)
        if (BLACK_KEY[i] == key) {               /* the sequencer, black 1-6: the pattern copied there */
            int from = seq_pat(), l;
            if (i == from) {
                say("THAT'S THIS", "PATTERN");
                return;
            }
            for (l = 0; l < EN_NLANE; l++)
                en_pats[i][l] = en_pats[from][l];
            edited();
            pat_name(i, t);
            say("COPIED TO", t);
            return;
        }
}

static void shift_lane(int restore)             /* the sequencer: SEL + OCT-, the lane cleared; OCT+, the piece's own */
{
    static uint16_t f[EN_NPAT][EN_NLANE];
    char t[16];
    uint16_t v = 0;
    if (restore) {
        en_piece_pats(proj.piece, f);
        v = f[seq_pat()][ui.lane];
    }
    en_pats[seq_pat()][ui.lane] = v;
    edited();
    caps(t, EN_LANE_NAME[ui.lane], sizeof t);
    say(t, restore ? "FACTORY" : "CLEARED");
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    int g;
    for (g = 0; g < NGROUP; g++)                 /* remembered: the page last on in its group */
        if (v >= GROUP[g].first && v < GROUP[g].first + GROUP[g].n)
            ui.grp_last[g] = (uint8_t)v;
    ui.view = (uint8_t)v;
    if (v < NDIAL)
        ui.last_dial = (uint8_t)v;
    ui.touched = -1;
}

static int voice_of(int v)                      /* the voice page v belongs to (EN_V_*), or -1 */
{
    if (v >= V_PAD && v <= V_CHORDS)
        return EN_V_PAD;
    if (v >= V_DRONE && v <= V_DRLFO)
        return EN_V_DRONE;
    if (v >= V_ACID && v <= V_ACLINE)
        return EN_V_ACID;
    return -1;
}
static const int SOUND_PAR[3] = {P_PADTONE, P_DR_INST, P_AC_WAVE};
static const int HAND_PAR[3] = {P_PAD_MAN, P_DR_MAN, P_AC_MAN};   /* off the algorithm: played by hand */
static int by_hand(int vc) { return vc >= 0 && proj.par[HAND_PAR[vc]] != 0; }
static int kit_view(int v) { return v == V_DRUMS || v == V_LINKS || v == V_KIT || v == V_SEQ; }

static void shift_sound(int d)                  /* SEL + ALGORITHM: the voice's next sound, or the whole kit's */
{
    char a[16], b[16];
    int vc = voice_of(ui.view);
    if (vc >= 0) {
        static const char *const WHO[3] = {"PAD", "DRONE", "ACID"};
        int p = SOUND_PAR[vc];
        const int ns = vc == 2 ? EN_NACID : EN_NSOUND;
        knob_set(p, (proj.par[p] + d + ns) % ns);
        en_param_text(p, proj.par[p], b);
        caps(a, b, sizeof a);
        say(WHO[vc], a);
    } else if (ui.view == V_CMEL || ui.view == V_CMEL2) {   /* the counter-melody's sound */
        const int ns = en_ncm_snd(), cur = proj.par[P_CM_SND] < ns ? proj.par[P_CM_SND] : 0;   /* (a loaded voice that is gone: from the first) */
        knob_set(P_CM_SND, (cur + d + ns) % ns);
        caps(a, EN_CM_SOUND[proj.par[P_CM_SND]], sizeof a);
        say("PLUCK", a);
    } else if (kit_view(ui.view)) {             /* the drums: all four to the next kit */
        int k = (proj.par[P_KIT_KICK] + d + EN_NKIT) % EN_NKIT;
        knob_set(P_KIT_KICK, k);
        knob_set(P_KIT_SNARE, k);
        knob_set(P_KIT_HAT, k);
        knob_set(P_KIT_PERC, k);
        caps(a, EN_KIT_NAME[k], sizeof a);
        say("KIT", a);
    } else {
        say("SOUND", "ON A VOICE'S PAGE");
    }
}

static int oct_par(int v)                       /* what OCT moves on page v */
{
    if (v >= V_PAD && v <= V_CHORDS)
        return P_PAD_OCT;
    if (v >= V_DRONE && v <= V_DRLFO)
        return P_BASSOCT;
    if (v >= V_ACID && v <= V_ACLINE)
        return P_AC_OCT;
    if (v == V_CMEL || v == V_CMEL2)
        return P_CM_OCT;
    return P_OCT;
}

static int group_of(int v)
{
    int g;
    for (g = 0; g < NGROUP; g++)
        if (v >= GROUP[g].first && v < GROUP[g].first + GROUP[g].n)
            return g;
    return -1;
}

static void jam_down(int i);
static void jam_up(int i);
static void jam_tick(void);
static void draw_jam(void);
static int perf_like(void) { return ui.view == V_PERF || ui.view == V_VIZ; }   /* the screens whose keys are the mix, knobs the FX knobs, ALGORITHM the mood */

static void button(int b)
{
    int g;
    if (b == B_HOME) {                           /* HOME: the tempo page; again, the perform screen; and back; twice quickly, the visualizers */
        const uint32_t now = plat_ms();
        if (ui.view == V_VIZ) {                  /* (HOME leaves them: the perform screen) */
            set_view(V_PERF);
            ui.home_t = 0;
            return;
        }
        if (ui.home_t && now - ui.home_t < 450u) {
            ui.home_t = 0;
            set_view(V_VIZ);
            say("VISUAL", VZ_NAME[ui.viz % VZ_N]);
            return;
        }
        ui.home_t = now ? now : 1u;
        set_view(ui.view == V_TEMPO ? V_PERF : V_TEMPO);
        return;
    }
    for (g = 0; g < NGROUP; g++)                 /* a page's button: its page, or the next in its group */
        if (GROUP[g].btn == b) {
            int v = ui.view;
            if (group_of(v) == g && GROUP[g].n > 1)
                set_view(GROUP[g].first + (v - GROUP[g].first + 1) % GROUP[g].n);       /* inside: the next page */
            else if (ui.grp_last[g] >= GROUP[g].first && ui.grp_last[g] < GROUP[g].first + GROUP[g].n)
                set_view(ui.grp_last[g]);                                               /* from elsewhere: back where you were */
            else
                set_view(GROUP[g].first);
            return;
        }
    switch (b) {
    case B_FX: {
        const uint32_t now = plat_ms();
        if (ui.fx_menu && !ui.fx_page && ui.fx_open_t && now - ui.fx_open_t < 350u) {   /* FX twice quickly: the settings of the effect you used last (the first page if none) */
            ui.fx_open_t = 0;
            ui.fx_page = ui.fx_lastpg ? ui.fx_lastpg : 1;
            say(FXP[ui.fx_page - 1].name, "SETTINGS");
            break;
        }
        ui.fx_menu = (uint8_t)!ui.fx_menu;
        ui.fx_page = 0;                          /* (from an edit page too: closed) */
        ui.fx_open_t = ui.fx_menu ? (now ? now : 1u) : 0u;
        say("FX", ui.fx_menu ? "TAP: LATCH  HOLD: PLAY" : ui.fx_lock ? "LATCHED ONES STAY ON" : "OFF");
        break;
    }
    case B_EDIT:
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        break;
    case B_PLAY:
        if (en_playing) {
            en_play(0);
            say("STOPPED", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("PLAYING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        if (ui.fx_menu && b == B_OCTDN) {        /* in FX, OCT-: everything back (effects, knobs, mutes), undoable */
            scene_reset();
            break;
        }
        if (ui.view == V_SEQ) {                  /* the sequencer: the lane */
            ui.lane = (uint8_t)((ui.lane + (b == B_OCTUP ? 1 : EN_NLANE - 1)) % EN_NLANE);
            ui.touched = -1;
        } else {                                 /* the page's voice's octave, or the song's */
            int p = oct_par(ui.view);
            char t[12];                          /* (en_param_text writes up to 10) */
            knob_set(p, proj.par[p] + (b == B_OCTUP ? 1 : -1));
            en_param_text(p, proj.par[p], t);
            say(p == P_PAD_OCT ? "PAD OCTAVE" : p == P_BASSOCT ? "DRONE OCTAVE" : p == P_AC_OCT ? "ACID OCTAVE" : p == P_CM_OCT ? "PLUCK OCTAVE" : "SONG OCTAVE", t);
        }
        break;
    case B_SAVE:                                 /* nothing is saved by it: SAVE is the breakdown (and the build) */
        if (en_sec == EN_SEC_GROOVE && en_sec_req == EN_CMD_BREAK + 1) {
            en_section(EN_CMD_DROP);             /* asked twice before the bar: never mind */
            say("BREAKDOWN", "OFF");
        } else if (en_sec == EN_SEC_GROOVE) {
            en_section(EN_CMD_BREAK);
            say("BREAKDOWN", en_playing ? "NEXT BAR" : 0);
        } else if (en_sec == EN_SEC_BREAK) {
            en_section(EN_CMD_BUILD);
            say("BUILD", "4 BARS, DROP");
        } else {
            en_section(EN_CMD_DROP);
            say("DROP", "NEXT BAR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
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
 * turn or another button puts it away; letting go after its card does nothing else. The buttons whose
 * hold means something else (PLAY, SEL, SEQ, SAVE, FX) have no card. */
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
    if (btn == 1u << B_HOME && !(ui.btn_used & btn) && now - ui.down_t[B_HOME] >= 700u && (int32_t)(ui.turn_t - ui.down_t[B_HOME]) <= 0) {   /* HOME held: the settings */
        int p;
        for (p = 0; p < NFXP; p++)
            if (FXP[p].name[0] == 'S' && FXP[p].name[1] == 'E' && FXP[p].name[2] == 'T')
                break;
        ui.btn_used |= btn;                      /* its release now does nothing */
        ui.fx_menu = 1;
        ui.fx_page = (uint8_t)(p < NFXP ? p + 1 : 1);
        say("SETTINGS", "GENRE, SAVER, LIGHTS, KEY");
        return;
    }
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_HOME && i != B_PLAY && i != B_SEL && i != B_FX && i != B_SAVE && i != B_SEQ &&
            !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD && (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

/* REMOTE (SEL + GLO): the panel plays HYP-1 in the browser over USB MIDI, and the FM-1 goes quiet. On
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

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c);

static void rm_draw(void)
{
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 120, K_BG);
    text_c(120, 4, &FONT_L, "REMOTE", K_ACC);
    text_c(120, 44, &FONT_S, rm.link ? "PLAYING THE BROWSER" : "WAITING FOR THE BROWSER", rm.link ? K_PAD : K_DIM);
    text_c(120, 66, &FONT_XS, "USB MIDI  //  CHANNEL 16", K_DIM);
    text_c(120, 96, &FONT_S, "SEL + GLO: BACK", K_TEXT);
    cv_blit(0, 56);
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
    ui.shift_keys = ui.keys;
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
    if (b || k)
        ui.act_t = now;
}

static int runner_on(void);
static void runner_input(uint32_t btn, uint32_t keys);
static void runner_start(void);

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    if (runner_on()) {                           /* the secret: the game has the panel */
        runner_input(btn, keys);
        return;
    }
    if (ui.saver_on) {                           /* the screensaver: the touch that wakes it only wakes (it was the visualizer's: a key muted a track, a knob moved the filter) */
        const uint32_t nb = btn & ~ui.btn, nk = keys & ~ui.keys;
        int r;
        ui.btn_used |= nb;                       /* its button does nothing when let go, nor held */
        ui.shift_keys |= nk;                     /* its key is no note, effect or mute until let go */
        ui.btn = btn;
        ui.keys = keys;
        for (r = 0; r < NE; r++)
            if (plat_enc(r))
                ui.turn_t = plat_ms();
        if (nb || nk)
            ui.act_t = plat_ms();
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
            if (i == B_SAVE) {
                ui.save_held = 0;
                ui.save_global = 0;
            }
            if (i == B_SAVE && ui.view == V_GLOBAL && !ui.fx_menu) {   /* Global: store the piece */
                ui.save_global = 1;
                ui.factory_done = 0;
                ui.btn_used |= m;              /* not a breakdown */
                ui.save_held = 1;              /* nor a drop */
            }
            if (i == B_FX && (btn >> B_SEL & 1u)) {   /* shift: the effects' edit pages, one by one, then the keys again */
                {   /* (the walk's order, then the keys again) */
                    const int nx = ui.fx_menu ? fx_pos(ui.fx_page) + 1 : 0;
                    ui.fx_page = (uint8_t)(nx >= 0 && nx < NFXP - 1 ? FX_ORDER[nx] : 0);
                }
                ui.fx_menu = 1;
                ui.lock_mode = 0;
                say(ui.fx_page ? FXP[ui.fx_page - 1].name : "FX", ui.fx_page ? "EDIT" : "HOLD A KEY");
                ui.btn_used |= m | 1u << B_SEL;
            }
            if (i == B_SEQ)
                ui.seq_held = 0;
            if (i == B_HOME && (btn >> B_SEL & 1u)) {   /* shift: the sigil's middle, the eye or the imp */
                ui.centre_imp = (uint8_t)!ui.centre_imp;
                ui.set_dirty = 1;
                ui.btn_used |= m | 1u << B_SEL;
                say("SIGIL", ui.centre_imp ? "THE IMP" : "THE EYE");
            }
#ifndef EN_WEB
            if (i == B_GLO && (btn >> B_SEL & 1u)) {   /* shift: remote, the panel plays the browser (stopped only: it stops the music) */
                if (en_playing) {
                    ui.btn_used |= m | 1u << B_SEL;
                    say("REMOTE", "STOP FIRST");
                } else {
                    rm_enter();
                    return;
                }
            }
#endif
            if (i == B_EDIT && (btn >> B_SEL & 1u) && en_playing) {   /* the secret, stopped only (it takes the music over) */
                ui.btn_used |= m | 1u << B_SEL;
                say("STOP FIRST", 0);
            } else if (i == B_EDIT && (btn >> B_SEL & 1u)) {   /* shift: the secret (it starts the music: it plays to the beat) */
                ui.btn_used |= m | 1u << B_SEL;
                en_play(1);
                ui.btn = btn;
                ui.keys = keys;
                runner_start();
                return;
            }
            if (i == B_ENV && (btn >> B_SEL & 1u)) {
                /* shift: the voice whose page is on screen off the algorithm (silent but for the keys on its
                 * pages), or back on it */
                static const char *const WHO[3] = {"PAD", "DRONE", "ACID"};
                int vc = voice_of(ui.view), k;
                if (vc < 0) {
                    say("BY HAND", "ON A VOICE'S PAGE");
                } else {
                    knob_set(HAND_PAR[vc], !proj.par[HAND_PAR[vc]]);
                    if (!proj.par[HAND_PAR[vc]]) /* back on the algorithm: any of its keys let go */
                        for (k = 0; k < NKEYS; k++)
                            if (ui.key_voice[k] == vc + 1) {
                                en_note(vc, 53 + k, 0);
                                ui.key_voice[k] = 0;
                            }
                    say(WHO[vc], proj.par[HAND_PAR[vc]] ? "BY HAND: PLAY THE KEYS" : "BACK WITH THE MUSIC");
                }
                ui.btn_used |= m | 1u << B_SEL;
            }
            if (i == B_SEQ && (btn >> B_SEL & 1u)) {   /* shift: the links on and off */
                knob_set(P_LINKS, !proj.par[P_LINKS]);
                say("LINKS", proj.par[P_LINKS] ? "ON" : "OFF");
                ui.btn_used |= m | 1u << B_SEL;
                ui.seq_held = 1;               /* no sequencer for this hold */
            }
            if (i == B_SEL)
                ui.shift_hint = 0;
            if ((i == B_OCTDN || i == B_OCTUP) && (btn >> B_SEL & 1u) && ui.view == V_SEQ) {
                shift_lane(i == B_OCTUP);      /* shift: the lane cleared, or the piece's own */
                ui.btn_used |= m | 1u << B_SEL;
            }
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (i == B_SAVE && ui.save_global) {
            ui.save_global = 0;                /* Global: SAVE let go, the piece stored (unless held to factory) */
            if (!ui.factory_done)
                piece_store();
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            if (i == B_SEQ && ui.view == V_SEQ)
                set_view(V_DRUMS);             /* SEQ tapped in the sequencer: back to Drums */
            else
                button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    ui.shift_keys &= keys;                     /* let go: no longer the shift's */
    if (ui.pat_key >= 0 && !(keys >> ui.pat_key & 1u)) {   /* the pattern key let go: a tap kept its pattern; a hold sends it back, once it has played */
        ui.pat_key = -1;
        if (plat_ms() - ui.pat_t < TAP_MS)
            ui.pat_rev = 0;
        else
            ui.pat_up = 1;
    }
    if ((btn >> B_SEL & 1u) && (ch & keys)) {  /* SEL held: the keys pressed are the shift's */
        ui.shift_keys |= ch & keys;
        ui.btn_used |= 1u << B_SEL;
        for (i = 0; i < NKEYS; i++)
            if ((ch & keys) >> i & 1u)
                shift_key((int)i);
    }
    if (!(btn >> B_SEL & 1u) && ((btn >> B_GLO & 1u) || (perf_like() && !ui.fx_menu)) &&
        (ch & keys & ~ui.shift_keys)) {        /* GLO held, or the perform screen: the track mix on the keys */
        uint32_t p = ch & keys & ~ui.shift_keys;
        ui.shift_keys |= p;
        if (btn >> B_GLO & 1u)
            ui.btn_used |= 1u << B_GLO;        /* not a trip to the Global page */
        for (i = 0; i < NKEYS; i++)
            if (p >> i & 1u)
                mix_key((int)i);
    }
    if ((btn >> B_GLO & 1u) && !(btn >> B_SEL & 1u) && !(ui.btn_used >> B_GLO & 1u) &&
        plat_ms() - ui.down_t[B_GLO] >= 350u)
        ui.btn_used |= 1u << B_GLO;            /* held for the mix: letting go does nothing either */
    ui.lock_keys &= keys;
    if (!ui.fx_menu || ui.fx_page)
        ui.fx_tap = 0;
    if (ui.fx_menu && ((!ui.fx_page && fx_bank == 3) || ui.fx_page == 20)) {   /* the snips (their bank, and their settings page, to try them): each key down is one (let go: the note off, with the other voices' keys) */
        const uint32_t down = ch & keys & ~ui.shift_keys;
        for (i = 0; i < NKEYS; i++)
            if (down >> i & 1u) {
                en_note(EN_V_SAMPLE, 53 + (int)i, 1);
                ui.key_voice[i] = EN_V_SAMPLE + 1;
            }
    }
    if (ui.fx_menu && !ui.fx_page) {           /* the FX keys: tapped, latched; held, played; a latched one tapped, off */
        const uint32_t down = ch & keys & ~ui.shift_keys, up = ch & ~keys;
        for (i = 0; i < NKEYS; i++) {
            const uint64_t bit = fxbit((int)i);
            if (fx_bank == 0 && i == FILL_KEY && (down >> i & 1u)) {   /* F#5: a drum fill in the next bar */
                ui.fx_open_t = 0;
                en_fill_next();
                say("FILL", "NEXT BAR");
                continue;
            }
            if (!bit)
                continue;
            if (down >> i & 1u) {
                ui.fx_open_t = 0;              /* (a key since the menu opened: FX again closes it) */
                if (ui.fx_lock & bit) {
                    ui.fx_lock &= ~bit;        /* off at once, and not played while it's held */
                    ui.lock_keys |= 1u << i;
                } else {
                    ui.fx_down_t[i] = plat_ms();
                    ui.fx_tap |= 1u << i;
                    if (fx_key_page((int)i))
                        ui.fx_lastpg = (uint8_t)fx_key_page((int)i);
                }
            }
            if ((up >> i & 1u) && (ui.fx_tap >> i & 1u)) {
                ui.fx_tap &= ~(1u << i);
                if (plat_ms() - ui.fx_down_t[i] < TAP_MS)
                    ui.fx_lock |= bit;         /* let go quickly: latched */
            }
        }
    }    {   /* the effects: each key held while the FX menu is on, and those locked */
        uint64_t m = 0;
        if (!ui.fx_menu)
            ui.lock_mode = 0;                  /* FX closed: the locks play on (FX again, a key: that one off) */
        {   /* a Flex Beat one shot is over: a latched pad lets go */
            static uint32_t flx_seen;
            if (en_flx_done != flx_seen) {
                flx_seen = en_flx_done;
                ui.fx_lock &= ~(0x7FFFull << FX_FLEX1);
            }
        }
        m = ui.fx_lock;
        if (ui.fx_menu && ui.fx_page && FXP[ui.fx_page - 1].bit != 63 &&
            (!FXP[ui.fx_page - 1].hold || (keys & ~ui.shift_keys)))   /* an edit page: its effect on, */
            m |= FXB(FXP[ui.fx_page - 1].bit);                                                 /* to be heard as it is edited (a roll or riser: with a key held) */
        {
            uint8_t flxh = 0;                  /* a Flex Beat pad held down by hand (the engine goes on with a one shot while it is) */
            static uint8_t flxh_sent;
            if (ui.fx_menu && !ui.fx_page) {
                for (i = 0; i < NKEYS; i++)
                    if ((keys & ~ui.shift_keys & ~ui.lock_keys) >> i & 1u) {
                        m |= fxbit(i);
                        if (fx_bank == 2 && KEY_FX[2][i] != FX_NONE) {
                            ui.flex_last = (uint8_t)(KEY_FX[2][i] - FX_FLEX1);
                            flxh = 1;
                        }
                    }
            }
            if (flxh != flxh_sent) {
                flxh_sent = flxh;
                en_flx_hold(flxh);
            }
        }
        if (ui.dj_on && en_playing) {              /* the auto-DJ: a fill before the breakdown, the riser in the build */
            if (ui.dj_fill)
                m |= FXB(FX_FILL);
            if (en_sec == EN_SEC_BUILD)
                m |= FXB(FX_RISER);
        }
        if (m != ui.fx_mask) {
            ui.fx_mask = m;
            en_fx(m);
        }
    }
    if (ui.fx_menu && ui.fx_page == 1)           /* the gate's page: the white keys are the pattern's steps */
        for (i = 0; i < NKEYS; i++)
            if ((ch & keys & ~ui.shift_keys) >> i & 1u) {
                int s;
                for (s = 0; s < 16; s++)
                    if (WHITE_KEY[s] == i)
                        gx_step(s);
            }
    if (!ui.fx_menu)
        for (i = 0; i < NKEYS; i++)
            if ((ch & keys & ~ui.shift_keys) >> i & 1u) {
                if (ui.view == V_SEQ) {          /* the sequencer: a white key is a step */
                    int s;
                    for (s = 0; s < 16; s++)
                        if (WHITE_KEY[s] == i)
                            step_toggle(s);
                    for (s = 0; s < 7; s++)      /* black 1-6: the lane plays that pattern's, from the next bar; black 7: it follows the piece's again */
                        if (BLACK_KEY[s] == i) {
                            char t[16] = "";
                            int j = 0;
                            const char *q = LANE_SHORT[ui.lane];
                            while (*q)
                                t[j++] = *q++;
                            t[j] = 0;
                            knob_set(P_LSRC + ui.lane, s < 6 ? s + 1 : 0);
                            if (s < 6) {
                                char u[12] = "PATTERN 1";
                                u[8] = (char)('1' + s);
                                say(t, u);
                            } else {
                                say(t, "FOLLOWS THE PIECE");
                            }
                        }
                } else if (ui.view == V_JAM) {   /* the JAM page: the key plays the voice chosen */
                    jam_down((int)i);
                } else if (by_hand(voice_of(ui.view))) {   /* a voice off the algorithm, its page: the key plays it */
                    ui.key_voice[i] = (uint8_t)(voice_of(ui.view) + 1);
                    en_note(voice_of(ui.view), 53 + (int)i, 1);
                } else {
                    toggle_note((int)((5u + i) % 12u));   /* key 0 is F3 */
                }
            }
    for (i = 0; i < NKEYS; i++)                  /* the JAM page's keys let go */
        if (ui.jam_n[i] && !(keys >> i & 1u))
            jam_up((int)i);
    for (i = 0; i < NKEYS; i++)                  /* let go: its note off, wherever it went */
        if (ui.key_voice[i] && !(keys >> i & 1u)) {
            en_note(ui.key_voice[i] - 1, 53 + (int)i, 0);
            ui.key_voice[i] = 0;
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((btn >> B_SEQ & 1u) && !ui.seq_held && ui.view != V_SEQ && plat_ms() - ui.down_t[B_SEQ] >= SEQ_HOLD) {
        ui.seq_held = 1;                       /* SEQ held: the sequencer */
        ui.btn_used |= 1u << B_SEQ;
        set_view(V_SEQ);
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages (the steps); with HOME held, the tempo */
        if (ui.fx_menu && !ui.fx_page) {         /* the FX keys: the dial is the bank (the effects, the beat effects) */
            fx_bank = (uint8_t)(e > 0 ? (fx_bank < 3 ? fx_bank + 1 : 3) : (fx_bank > 0 ? fx_bank - 1 : 0));
            say(fx_bank == 3 ? "DJ SNIPS" : fx_bank == 2 ? "FLEX BEAT" : fx_bank ? "BEAT FX" : "FX", "HOLD A KEY");
        } else if (ui.fx_menu && ui.fx_page) {   /* the FX edit pages: the dial walks them */
            const int pos = fx_pos(ui.fx_page) + (e > 0 ? 1 : -1);
            if (pos >= 0 && pos < NFXP - 1) {         /* (the JAM page's, the last, is not an FX page) */
                ui.fx_page = FX_ORDER[pos];
                say(FXP[ui.fx_page - 1].name, "EDIT");
            }
        } else if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_TEMPO) {
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else if (ui.view == V_VIZ) {            /* the visualizers: the next one */
            ui.viz = (uint8_t)((ui.viz + (e > 0 ? 1 : VZ_N - 1)) % VZ_N);
            ui.set_dirty = 1;
            say("VISUAL", VZ_NAME[ui.viz]);
        } else {
            int v = (ui.view == V_SEQ ? V_DRUMS : ui.view == V_PERF ? V_TEMPO : (int)ui.view) + (e > 0 ? 1 : -1);   /* (out of the sequencer and the perform screen too) */
            if (v >= 0 && v < NDIAL)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        if (perf_like() && !ui.fx_menu && !(btn >> B_SEL & 1u)) {   /* the perform screen (and the visualizers): the mood */
            int m = ui.mood + 2 * e;
            char t[8];
            ui.mood = (uint8_t)(m < 0 ? 0 : m > 100 ? 100 : m);
            mood_apply();
            itoa_u(ui.mood, t);
            say("MOOD", t);
        } else if (btn >> B_SEL & 1u) {               /* shift: the sound of the voice on screen (drums: the kit) */
            ui.btn_used |= 1u << B_SEL;
            shift_sound(e > 0 ? 1 : -1);
        } else {
            set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        if ((btn >> B_GLO & 1u) && !(btn >> B_SEL & 1u)) {   /* GLO held: the genre */
            ui.btn_used |= 1u << B_GLO;
            genre_step(e > 0 ? 1 : -1);
        } else if ((btn >> B_SEL & 1u) && en_playing) {   /* shift: the piece on the next bar */
            int t = ((ui.piece_q ? ui.piece_q - 1 : proj.piece) + (e > 0 ? 1 : -1) + NPIECES) % NPIECES;
            char u[12] = "PIECE ";
            ui.btn_used |= 1u << B_SEL;
            caps(u + 6, piece_name(t), 4);
            if (t == proj.piece) {
                ui.piece_q = 0;
                say(u, "STAYS");
            } else {
                ui.piece_q = (uint8_t)(t + 1);
                say(u, "NEXT BAR");
                ui.msg_until = plat_ms() + 60000u;   /* until it comes */
            }
        } else {
            if (btn >> B_SEL & 1u)
                ui.btn_used |= 1u << B_SEL;
            ui.piece_q = 0;
            set_piece(proj.piece + (e > 0 ? 1 : -1));
        }
        ui.turn_t = plat_ms();
    }
    if (ui.piece_q && (!en_playing || en_drum_pos == EN_NSTEP - 1)) {   /* the bar's last step: the piece */
        int t = ui.piece_q - 1;
        ui.piece_q = 0;
        set_piece(t);
    }
    if ((btn >> B_SEL & 1u) && !(ui.btn_used >> B_SEL & 1u) && !ui.shift_hint &&
        plat_ms() - ui.down_t[B_SEL] >= 400u) {
        ui.shift_hint = 1;                     /* SEL held alone: what the shift does (draw_shift) */
    }
    if (ui.save_global && !ui.factory_done && (btn >> B_SAVE & 1u)) {
        uint32_t held = plat_ms() - ui.down_t[B_SAVE];
        if (held >= FACTORY_HOLD) {
            ui.factory_done = 1;
            piece_factory();
        } else if (held >= 400u && plat_ms() >= ui.msg_until) {
            say("HOLD:", "FACTORY");           /* what holding on does */
            ui.msg_until = ui.down_t[B_SAVE] + FACTORY_HOLD;
        }
    }
    if ((btn >> B_SAVE & 1u) && !ui.save_held && plat_ms() - ui.down_t[B_SAVE] >= DROP_HOLD) {
        ui.save_held = 1;                      /* held: straight to the drop, no build */
        ui.btn_used |= 1u << B_SAVE;
        if (en_sec != EN_SEC_GROOVE) {
            en_section(EN_CMD_DROP);
            say("DROP", "NEXT BAR");
        } else {
            button(B_SAVE);
        }
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (ui.view == V_SEQ && (btn >> B_SEL & 1u)) {   /* the sequencer's tricks, SEL + a knob: a Euclidean pattern (hits, rotation), the lane's probability and length */
                ui.btn_used |= 1u << B_SEL;
                seq_trick((int)i, e);
                ui.act_t = ui.turn_t = plat_ms();
                continue;
            }
            turn(EN_K1 + (int)i, view_knob((int)i), e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the main area (144), the knobs (68) */
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

/* an outline: a cyberpunk frame with its corners cut */
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x + 2, y, w - 4, 1, c);
    cv_rect(x + 2, y + h - 1, w - 4, 1, c);
    cv_rect(x, y + 2, 1, h - 4, c);
    cv_rect(x + w - 1, y + 2, 1, h - 4, c);
    cv_rect(x + 1, y + 1, 1, 1, c);
    cv_rect(x + w - 2, y + 1, 1, 1, c);
    cv_rect(x + 1, y + h - 2, 1, 1, c);
    cv_rect(x + w - 2, y + h - 2, 1, 1, c);
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
    const float th_ = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th_, rm = ro - th_ * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
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
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th_ * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th_ * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th_ * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th_ * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th_ * 0.5f + 0.6f, fill);
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

static const char *const MASCOT[23];
static uint16_t mascot_color(char ch);
/* the mascot, one pixel a pixel, on a spring like the big dancer's (viz.c), smaller: a hop on the kick (the head lags, the coils spread
 * as he rises, he squashes on landing and wobbles), a squash on the snare, a bigger jump on the drop; asleep in a breakdown (eyes
 * shut, sagging), wide awake in a build (shaking) */
static struct {
    float dh, dhv, lift, lv, kprev, sprev;     /* the head's spring (px, + up) and its speed; the hop's height and speed */
    uint32_t t;
    uint8_t sec;
} hm;
static void mascot_step(void)
{
    const uint32_t now = plat_ms();
    float dt = (float)(now - hm.t) * 0.001f;
    int s;
    const int kedge = en_drum_glow[0] > 0.6f && hm.kprev <= 0.6f, sedge = en_drum_glow[1] > 0.6f && hm.sprev <= 0.6f;
    hm.t = now;
    hm.kprev = en_drum_glow[0], hm.sprev = en_drum_glow[1];
    if (dt > 0.1f)
        dt = 0.1f;
    if (dt <= 0.0f)
        return;
    if (!en_playing) {
        hm.lift = hm.lv = 0.0f;
    } else {
        if (en_sec != hm.sec) {                          /* the drop: a big jump */
            if (en_sec == EN_SEC_GROOVE && hm.sec != EN_SEC_GROOVE && hm.lift <= 0.0f) {
                hm.lv = 52.0f;
                hm.lift = 0.01f;
                hm.dhv -= 40.0f;
            }
            hm.sec = en_sec;
        }
        if (kedge && hm.lift <= 0.0f && en_sec == EN_SEC_GROOVE) {
            hm.lv = 36.0f;
            hm.lift = 0.01f;
            hm.dhv -= 32.0f;                              /* anticipation: the head lags */
        }
        if (sedge && en_sec == EN_SEC_GROOVE && hm.lift <= 0.0f)
            hm.dhv -= 55.0f;                              /* a stomp on the snare: squashed, then up */
    }
    for (s = 0; s < 4; s++) {
        const float h = dt * 0.25f;
        float tg;
        if (hm.lift > 0.0f || hm.lv > 0.0f) {
            hm.lv -= 200.0f * h;
            hm.lift += hm.lv * h;
            if (hm.lift <= 0.0f && hm.lv < 0.0f) {
                hm.dhv += hm.lv * 2.3f;                   /* landing: squashed */
                hm.lift = 0.0f;
                hm.lv = 0.0f;
            }
        }
        tg = hm.lift > 0.0f ? fm_fabsf(hm.lv) * 0.11f : 0.0f;       /* stretched by the speed */
        if (en_playing && en_sec == EN_SEC_BREAK)
            tg = -2.0f;
        hm.dhv += (-300.0f * (hm.dh - tg) - 10.0f * hm.dhv) * h;
        hm.dh += hm.dhv * h;
    }
    hm.dh = fm_clampf(hm.dh, -3.0f, 4.0f);
    hm.lift = fm_clampf(hm.lift, 0.0f, 3.0f);
}
static void draw_mascot(int32_t x, int32_t y, int sleepy)
{
    int r, c;
    for (r = 0; r < 23; r++) {
        /* rows 0..10 the head, then four coils of three rows: each shifted by its share of the stretch */
        const int grp = r <= 10 ? 0 : (r - 8) / 3;
        const int32_t dy = (int32_t)(hm.lift + hm.dh * (float)(4 - grp) * 0.25f + 0.5f);
        for (c = 0; c < 23; c++) {
            char ch = MASCOT[r][c];
            if (ch == '.')
                continue;
            if (ch == 'Y' && sleepy)
                ch = 'K';
            cv_pset(x + c, y + r - dy, ch == 'K' || ch == 'T' ? K_LINE : mascot_color(ch));
        }
    }
}

/* the header: the piece in a tag, its root and mode; on the right, the tempo with a beat light (a play
 * triangle while stopped), the section and a dot while the kit is unsaved. Messages take its place
 * for a moment. */
/* THE IMP SKIN (theme 6; the old five keep the old look): the header gets a beat bar along its top (sixteen ticks, the beats taller, the step
 * playing lit) and a coil along its bottom, the slinky's spring, that stretches on the kick. */
#define IMP_TEAL RGB(111, 224, 210)
static void imp_header(void)
{
    const float kg = fm_clampf(en_drum_glow[0], 0.0f, 1.0f);
    const float amp = 1.0f + 2.2f * kg, st = 0.33f;
    const float ph = (float)(plat_ms() % 1200u) * (FM_TWO_PI / 1200.0f);
    float s = fm_sinf(ph), c = fm_cosf(ph);
    const float ss = fm_sinf(st), cs = fm_cosf(st);
    const uint16_t cc = mix(K_LINE, K_BASS, 7 + q16(kg) * 9 / 16);
    int32_t x;
    int i;
    for (i = 0; i < 16; i++)                     /* the bar's sixteen steps */
        cv_rect(i * 15 + 1, 0, 13, i % 4 ? 1 : 2, en_playing && i == (int)en_drum_pos ? K_ACC : K_LINE);
    for (x = 0; x < 240; x++) {                  /* the coil */
        const int32_t y = 26 + (int32_t)(s * amp + (s < 0.0f ? -0.5f : 0.5f));
        const float ns = s * cs + c * ss;
        c = c * cs - s * ss;
        s = ns;
        cv_pset(x, y, cc);
    }
}

static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h;
    mascot_step();
    if (ui.frame % 2u)                          /* (the spring moves every frame; the header is redrawn thirty times a second) */
        return;
    h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | (uint32_t)en_sec << 2 | (uint32_t)en_sec_req << 9 | (uint32_t)msg << 5 |
                               (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 12 | (uint32_t)project_pending() << 16 |
                               (uint32_t)(proj.edited >> proj.piece & 1u) << 17),
                      (uint32_t)proj.par[P_TEMPO] | (uint32_t)ui.theme << 8 | (uint32_t)(ui.fx_lock != 0) << 12 | (uint32_t)ui.dj_on << 13 | (uint32_t)ui.scene_q << 14 | (uint32_t)ui.key_on << 19 |
                          (uint32_t)((ui.fxk[0] | ui.fxk[1] | ui.fxk[2] | ui.fxk[3]) != 0) << 18);
    h = hash(h, (uint32_t)en_sec_bars | (uint32_t)ui.dj_n << 8 | (uint32_t)ui.dj_len << 24);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    h = hash(h, (uint32_t)(int)(hm.lift * 2.0f) | (uint32_t)((int)(hm.dh * 2.0f) + 16) << 4 |
                    (uint32_t)(en_playing && en_sec == EN_SEC_BUILD ? 1u + (ui.frame / 2u & 1u) : 0u) << 12);
    if (th->skin)                               /* the coil moves: 15 a second, and on the kick; the beat bar on each step */
        h = hash(h, (uint32_t)(plat_ms() / 66u) | (uint32_t)q16(en_drum_glow[0]) << 20 | (uint32_t)(en_playing ? en_drum_pos + 1u : 0u) << 25);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 207, w;
        draw_mascot(212 + (en_playing && en_sec == EN_SEC_BUILD ? (int)(ui.frame / 2u & 1u) * 2 - 1 : 0), 3, en_playing && en_sec == EN_SEC_BREAK);
        if (project_pending()) {                   /* not yet in flash (the theme, Tune, MIDI out: two seconds on) */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_sec == EN_SEC_GROOVE && !en_sec_req && en_playing && ui.dj_on && ui.dj_len != 4 && (int)DJ_G() - (int)ui.dj_n <= 8 && (int)DJ_G() > (int)ui.dj_n) {   /* the auto-DJ: the last bars before its breakdown */
            const int left = (int)DJ_G() - (int)ui.dj_n;
            t[0] = 'B', t[1] = 'R', t[2] = 'E', t[3] = 'A', t[4] = 'K', t[5] = ' ', t[6] = 'I', t[7] = 'N', t[8] = ' ', t[9] = (char)('0' + left), t[10] = 0;
            x -= text_w(&FONT_XS, t);
            cv_text(x, 8, &FONT_XS, t, K_DIM);
            x -= 10;
        }
        if (en_sec != EN_SEC_GROOVE || en_sec_req) {   /* the section, or the one coming on the bar */
            const char *sn = en_sec == EN_SEC_BREAK ? "BREAK" : en_sec == EN_SEC_BUILD ? "BUILD" :
                             en_sec_req == EN_CMD_BREAK + 1 ? "BREAK>" : "";
            if (en_sec != EN_SEC_GROOVE && en_sec_req == EN_CMD_DROP + 1)
                sn = "DROP>";
            else if (en_sec == EN_SEC_BREAK && en_sec_req == EN_CMD_BUILD + 1)
                sn = "BUILD>";
            else if (en_sec == EN_SEC_BUILD && en_sec_bars) {   /* the build: the bars to the drop */
                t[0] = 'D', t[1] = 'R', t[2] = 'O', t[3] = 'P', t[4] = ' ', t[5] = (char)('0' + (en_sec_bars > 9 ? 9 : en_sec_bars)), t[6] = 0;
                sn = t;
            } else if (en_sec == EN_SEC_BREAK && ui.dj_on && ui.dj_len != 4) {   /* the auto-DJ's breakdown: the bars to its build */
                const int left = dj_break_len(DJ_G()) - (int)ui.dj_n;
                if (left > 0 && left <= 9) {
                    t[0] = 'B', t[1] = 'R', t[2] = 'E', t[3] = 'A', t[4] = 'K', t[5] = ' ', t[6] = (char)('0' + left), t[7] = 0;
                    sn = t;
                }
            }
            if (*sn) {
                x -= text_w(&FONT_XS, sn);
                cv_text(x, 8, &FONT_XS, sn, K_FROST);
                x -= 10;
            }
        }
        if (ui.fxk[0] || ui.fxk[1] || ui.fxk[2] || ui.fxk[3]) {   /* an FX knob off its home: said, so a forgotten filter is not a mystery */
            x -= text_w(&FONT_XS, "KNOB");
            cv_text(x, 8, &FONT_XS, "KNOB", K_FROST);
            x -= 10;
        }
        if (ui.key_on) {                          /* the drop's key change is on */
            x -= text_w(&FONT_XS, "KEY");
            cv_text(x, 8, &FONT_XS, "KEY", K_ACC);
            x -= 10;
        }
        if (ui.scene_q) {                         /* a scene waiting for the bar */
            char q[4] = "S1>";
            q[1] = (char)('0' + ui.scene_q);
            x -= text_w(&FONT_XS, q);
            cv_text(x, 8, &FONT_XS, q, K_ACC);
            x -= 10;
        }
        if (ui.dj_on) {                           /* the auto-DJ is running */
            x -= text_w(&FONT_XS, "DJ");
            cv_text(x, 8, &FONT_XS, "DJ", K_ACC);
            x -= 10;
        }
        if (ui.fx_lock && !ui.fx_menu) {          /* effects locked on, the menu closed: they play on */
            x -= text_w(&FONT_XS, "FX");
            cv_text(x, 8, &FONT_XS, "FX", K_PAD);
            x -= 10;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (!en_playing) {
            int32_t j;                            /* stopped: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a tag with a cut corner; a */
        if (proj.edited >> proj.piece & 1u)                   /* mark in it while it has unstored changes */
            w += 7;
        cv_rect(4, 4, w, 20, K_PAD);
        cv_rect(4 + w - 4, 4, 4, 1, K_BG);
        cv_rect(4 + w - 3, 5, 3, 1, K_BG);
        cv_rect(4 + w - 2, 6, 2, 1, K_BG);
        cv_rect(4 + w - 1, 7, 1, 1, K_BG);
        cv_text(10, 5, &FONT_B, piece_name(proj.piece), K_BG);
        if (proj.edited >> proj.piece & 1u)
            cv_round(4 + w - 10, 12, 4, 4, 1, K_BG);
        w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
        cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
    }
    if (th->skin)
        imp_header();
    cv_blit(0, 0);
}

/* the flow: the scale as a ring of twelve, the chord lit in it; the acid loop around it, its steps (the
 * notes, the accents bigger) and its playhead; the layers' levels on the left; the drum pattern on the
 * right, sixteen steps by six lanes, the playhead going across */
#define CX 112
#define CY 74
static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

/* THE SIGIL (the middle of the home screen; it replaced HYP-1's ring of dots): a summoning circle turned so the chord's root is at the
 * top, the chord's tones joined into a figure (a triad a triangle, a seventh a four-sided shape: the chord's kind read as a shape, its name
 * under the eye). Outside, the 4-bar phrase drawing itself round; then the bar's 16 steps (the kick's larger, the acid's gates tinted, the
 * playhead white); a pentagram turning once in 16 bars behind it; runes flaring by the notes the pluck plays. In the middle a demon eye
 * (SEL + HOME: the slinky imp instead): half open in a groove, its slit pupil widening on the kick and its gaze flipping on the snare;
 * shut in a breakdown; wide and trembling in a build, the phrase ring opening in spinning gaps; a white flash and a shock ring on the
 * drop. When the engine sheds load the pentagram and the runes go first. */
static struct {
    float rot;                                   /* the circle's turn (radians), easing to the chord's root */
    uint32_t t, drop_t;
    int8_t gaze;
    uint8_t sn_hi, last_sec, cm_seen, rn_i;
    struct { uint8_t pc, kind; uint32_t t; } rn[5];
} sg = {0.0f, 0, 0, 2, 0, 0, 0, 0, {{0, 0, 0}}};
static void sg_pt(float r, float a, int32_t *x, int32_t *y)
{
    const float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = CX + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = CY - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}
static void sg_line2(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* two pixels thick: read across a dark room */
{
    cv_line(x0, y0, x1, y1, c);
    cv_line(x0 + 1, y0, x1 + 1, y1, c);
    cv_line(x0, y0 + 1, x1, y1 + 1, c);
}
static const uint8_t RUNE[8][5] = {              /* 5 x 5, a row a byte (bit 4 the left) */
    {0x04, 0x0E, 0x15, 0x04, 0x04}, {0x11, 0x0A, 0x04, 0x0A, 0x11}, {0x1C, 0x12, 0x1C, 0x12, 0x1C}, {0x04, 0x06, 0x05, 0x04, 0x04},
    {0x11, 0x1B, 0x15, 0x11, 0x11}, {0x0E, 0x11, 0x04, 0x11, 0x0E}, {0x10, 0x18, 0x14, 0x12, 0x11}, {0x04, 0x0A, 0x11, 0x0A, 0x04}};
static void draw_sigil(void)
{
    static const uint8_t EYE[9] = {12, 12, 11, 11, 10, 9, 7, 5, 2};   /* the eye's half-width, from its middle row out (scaled to its opening) */
    static const uint8_t IRIS[6] = {5, 5, 5, 4, 3, 2};
    const uint32_t now = plat_ms(), abs_pool = pool_abs();
    const float step = FM_TWO_PI / 12.0f;
    const int shed = en_shed_level > 0, play = en_playing;
    uint32_t chord = 0;
    int32_t px[12], py[12];
    int i, n = 0;
    for (i = 0; i < EN_NPAD; i++)
        chord |= 1u << (en_chord[i] % 12u);
    {   /* the turn: the chord's root to the top, the short way round, eased over a fraction of a second */
        float d = -(float)(en_chord_root % 12) * step - sg.rot, k = (float)(now - sg.t) * (1.0f / 160.0f);
        while (d > FM_PI)
            d -= FM_TWO_PI;
        while (d < -FM_PI)
            d += FM_TWO_PI;
        sg.rot += d * (k > 1.0f ? 1.0f : k);
        if (sg.rot > FM_PI)
            sg.rot -= FM_TWO_PI;
        else if (sg.rot < -FM_PI)
            sg.rot += FM_TWO_PI;
        sg.t = now;
    }
    if (play && en_sec == EN_SEC_GROOVE && sg.last_sec != EN_SEC_GROOVE)
        sg.drop_t = now;                         /* the drop */
    sg.last_sec = en_sec;
    {   /* the gaze flips on each snare */
        const int hi = en_drum_glow[1] > 0.5f;
        if (hi && !sg.sn_hi)
            sg.gaze = (int8_t)-sg.gaze;
        sg.sn_hi = (uint8_t)hi;
    }
    if (en_cm_count != sg.cm_seen) {             /* a pluck note: a rune by it */
        sg.cm_seen = en_cm_count;
        sg.rn[sg.rn_i].pc = (uint8_t)(en_cm_note % 12u);
        sg.rn[sg.rn_i].kind = (uint8_t)(en_cm_count & 7u);
        sg.rn[sg.rn_i].t = now;
        sg.rn_i = (uint8_t)((sg.rn_i + 1u) % 5u);
    }
    if (!shed) {                                 /* the pentagram, turning once in 16 bars */
        const float pa = (float)(en_bars & 255u) * (FM_TWO_PI / 256.0f);
        int32_t sx[5], sy[5];
        for (i = 0; i < 5; i++)
            sg_pt(36.0f, pa + (float)i * (FM_TWO_PI / 5.0f), &sx[i], &sy[i]);
        for (i = 0; i < 5; i++)
            cv_line(sx[i], sy[i], sx[(i + 2) % 5], sy[(i + 2) % 5], mix(K_BG, K_LINE, 9));
    }
    {   /* the phrase: 64 dots, filled through 4 bars (thin in a breakdown, opening in spinning gaps in a build) */
        const int lit = play ? (int)(((en_bars >> 4) & 3u) * 16u + (en_bars & 15u)) : -1, spin = (int)(now / 60u);
        for (i = 0; i < 64; i++) {
            int32_t x, y;
            if (play && en_sec == EN_SEC_BUILD && ((i + spin) & 7) < 2)
                continue;
            if (en_sec == EN_SEC_BREAK && (i & 1))
                continue;
            sg_pt(57.0f, (float)i * (FM_TWO_PI / 64.0f), &x, &y);
            if (i <= lit)
                cv_rect(x, y, 2, 2, en_sec == EN_SEC_BREAK ? K_DIM : K_ACC);
            else
                cv_rect(x, y, 1, 1, K_LINE);
        }
    }
    {   /* the bar: the kick's steps, the acid's gates, the playhead */
        const uint32_t kick = en_pats[en_pat_now][0];
        for (i = 0; i < 16; i++) {
            int32_t x, y;
            sg_pt(48.0f, (float)i * (FM_TWO_PI / 16.0f), &x, &y);
            if (play && i == en_drum_pos)
                cv_disc(x, y, 2, K_TEXT);
            else if (kick >> i & 1u)
                cv_disc(x, y, 2, mix(K_BG, RGB(255, 160, 60), 6 + q16(en_drum_glow[0]) * 10 / 16));
            else if (en_acid_gate >> (i % (en_acid_len ? en_acid_len : 16)) & 1u)
                cv_rect(x, y, 2, 2, mix(K_ACID_T, K_ACID, 8));
            else
                cv_rect(x, y, 1, 1, K_LINE);
        }
    }
    for (i = 0; i < 12; i++)                     /* the twelve notes, turned */
        sg_pt(36.0f, (float)i * step + sg.rot, &px[i], &py[i]);
    {   /* the figure: the chord's tones joined round the circle */
        const uint16_t fc = en_frozen ? K_FROST : mix(K_PAD_T, K_PAD, 8 + q16(en_pad_level) * 8 / 16);
        int first = -1, prev = -1;
        for (i = 0; i < 12; i++)
            if (chord >> i & 1u) {
                if (prev >= 0)
                    sg_line2(px[prev], py[prev], px[i], py[i], fc);
                else
                    first = i;
                prev = i;
                n++;
            }
        if (n > 2)
            sg_line2(px[prev], py[prev], px[first], py[first], fc);
    }
    for (i = 0; i < 12; i++) {
        const int in = (int)(abs_pool >> i & 1u), ch = (int)(chord >> i & 1u);
        if (i == (int)(en_chord_root % 12))
            cv_disc(px[i], py[i], 5, K_TEXT);    /* the chord's root: ringed, at the top */
        if (ch)
            cv_disc(px[i], py[i], 3, mix(K_PAD_T, K_PAD, 10 + q16(en_pad_level) * 6 / 16));
        else if (in)
            cv_disc(px[i], py[i], 2, mix(K_PAD_T, K_PAD, 6));
        else
            cv_rect(px[i], py[i], 1, 1, K_LINE);
    }
    if (!shed)                                   /* the runes, fading over 0.7 s */
        for (i = 0; i < 5; i++) {
            const uint32_t age = now - sg.rn[i].t;
            int32_t x, y, r, c;
            if (!sg.rn[i].t || age >= 700u)
                continue;
            sg_pt(44.0f, (float)sg.rn[i].pc * step + sg.rot + 0.16f, &x, &y);
            for (r = 0; r < 5; r++)
                for (c = 0; c < 5; c++)
                    if (RUNE[sg.rn[i].kind][r] >> (4 - c) & 1u)
                        cv_pset(x - 2 + c, y - 2 + r, mix(K_BG, K_ACC, 16 - (int)(age * 16u / 700u)));
        }
    if (ui.centre_imp) {                         /* the imp, summoned into it */
        cv_rect(CX - 13, CY - 14, 27, 27, K_BG);
        draw_mascot(CX - 11, CY - 12, play && en_sec == EN_SEC_BREAK);
    } else {                                     /* the eye */
        const uint32_t dage = now - sg.drop_t;
        const int flash = sg.drop_t && dage < 450u;
        const int open = flash ? 8 : !play || en_sec == EN_SEC_BREAK ? 0 : en_sec == EN_SEC_BUILD ? 8 : 5;
        const int32_t ex = CX + (play && en_sec == EN_SEC_BUILD ? (int32_t)(ui.frame & 1u) * 2 - 1 : 0);
        int j;
        cv_rect(ex - 14, CY - open - 2, 29, 2 * open + 5, K_BG);   /* a dark surround, so the figure never runs through it */
        if (!open) {                             /* shut: a lid and its lashes */
            cv_rect(ex - 11, CY, 23, 1, K_DIM);
            for (j = -2; j <= 2; j++)
                cv_rect(ex + j * 4, CY + 1, 1, 2, K_DIM);
        } else {
            const int ir = open < 5 ? open : 5, gx = flash ? 0 : sg.gaze;
            const int pw = 1 + (int)(fm_clampf(en_drum_glow[0], 0.0f, 1.0f) * 3.0f);
            for (j = -open; j <= open; j++) {
                const int a = j < 0 ? -j : j, w = EYE[a * 8 / open];
                cv_rect(ex - w, CY + j, 2 * w + 1, 1, flash ? K_TEXT : mix(K_BG, RGB(255, 60, 40), 6));
                if (!flash && a <= ir)
                    cv_rect(ex + gx - IRIS[a], CY + j, 2 * IRIS[a] + 1, 1, K_ACC);
            }
            if (!flash)
                cv_rect(ex + gx - pw / 2, CY - ir + 1, pw, 2 * ir - 1, K_BG);   /* the slit, wider on the kick */
        }
        if (flash) {                             /* the shock ring */
            const float r = 20.0f + (float)dage * (40.0f / 450.0f);
            const uint16_t c = mix(K_TEXT, K_BG, (int)(dage * 16u / 450u));
            for (j = 0; j < 48; j++) {
                int32_t x, y;
                sg_pt(r, (float)j * (FM_TWO_PI / 48.0f), &x, &y);
                cv_rect(x, y, 2, 2, c);
            }
        }
    }
    {   /* the chord's name, under the eye */
        char nm[16];
        int pcs[EN_NPAD + 1], k;
        int32_t w;
        pcs[0] = en_chord_root;
        for (k = 0; k < EN_NPAD; k++)
            pcs[k + 1] = en_chord[k] % 12;
        en_chord_name(pcs, EN_NPAD + 1, nm);
        w = text_w(&FONT_XS, nm);
        cv_rect(CX - w / 2 - 2, CY + 15, w + 4, 11, K_BG);
        text_c(CX, CY + 16, &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
    }
}

static void draw_meters(void)                    /* left: the five layers (pad, drone, acid, pluck, drums) */
{
    static const char *const L[5] = {"P", "D", "A", "C", "K"};
    uint16_t c[5], t[5];
    float lv[5], dm = 0.0f;
    int i;
    c[0] = K_PAD, c[1] = K_BASS, c[2] = K_ACID, c[3] = KC_COLOR, c[4] = K_PULSE;
    t[0] = K_PAD_T, t[1] = K_BASS_T, t[2] = K_ACID_T, t[3] = mix(K_BG, KC_COLOR, 4), t[4] = K_PULSE_T;
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[1] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[2] = en_acid_level * (float)proj.par[P_ACID] * 0.01f;
    lv[3] = en_cm_level * (float)proj.par[P_CM_LVL] * 0.01f * 1.6f;
    lv[4] = dm * (float)proj.par[P_PULSE] * 0.01f;
    for (i = 0; i < 5; i++) {
        int32_t x = 6 + i * 9, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 92.0f + 0.5f);
        cv_rect(x, 20, 5, 96, t[i]);
        if (h > 0)
            cv_rect(x, 20 + 96 - h, 5, h, c[i]);
        cv_text(x, 120, &FONT_XS, L[i], K_DIM);
    }
}

static void cpu_line(char *c);
static void draw_kit(void)                       /* right: the pattern playing, small */
{
    int l, s, pos = en_drum_pos, pat = en_pat_now;
    char t[12] = "PAT ";
    for (l = 0; l < EN_NLANE; l++) {
        uint16_t bits = en_pats[pat][l];
        int g = q16(en_drum_glow[l]);
        for (s = 0; s < 16; s++) {
            int32_t x = 170 + s * 4, y = 26 + l * 9;
            int on = (int)(bits >> s & 1u), cur = en_playing && s == pos;
            if (on)
                cv_rect(x, y, 3, 6, en_pulse_rest ? K_LINE : cur ? K_TEXT : mix(K_LINE, LANE_COL(l), 8 + g / 2));
            else
                cv_rect(x + 1, y + 2, 1, 2, cur ? K_DIM : K_LINE);
        }
    }
    itoa_u((uint32_t)pat + 1u, t + 4);
    cv_text(170, 84, &FONT_XS, t, K_TEXT);
    {   /* under it, always: the load (CPU, XR; then C2, the other core's, and S1/S2 if the engine eases off) */
        char c[40];
        int n = 0;
        cpu_line(c);
        while (c[n] && !(c[n] == ' ' && c[n + 1] == 'C' && (c[n + 2] == '1' || c[n + 2] == '2')))
            n++;
        if (c[n]) {
            c[n] = 0;
            cv_text(170, 106, &FONT_XS, c + n + 1, K_DIM);
        }
        cv_text(170, 96, &FONT_XS, c, K_DIM);
    }
    if (en_pat_next != en_pat_now) {             /* the next, waiting for the bar */
        char u[8] = "> ";
        itoa_u((uint32_t)en_pat_next + 1u, u + 2);
        cv_text(206, 84, &FONT_XS, u, K_ACC);
    }
}

/* the sequencer: six lanes of sixteen steps, the lane the keys edit on a tinted band; the playhead a
 * tinted column; the pattern edited above, and the one playing if they differ */
static void draw_seq(void)
{
    int l, s, pat = seq_pat();
    char t[24] = "PATTERN ";
    itoa_u((uint32_t)pat + 1u, t + 8);
    cv_text(6, 1, &FONT_XS, t, K_ACC);
    if (en_pat_now != pat || en_pat_next != pat) {
        char u[24] = "PLAYING ";
        itoa_u((uint32_t)en_pat_now + 1u, u + 8);
        cv_text(120, 1, &FONT_XS, u, K_DIM);
    }
    for (l = 0; l < EN_NLANE; l++) {
        uint16_t bits = en_pats[pat][l];
        int32_t y = 18 + l * 20, sel = l == ui.lane;
        if (sel)                                /* the keys' lane */
            cv_rect(0, y + 1, 240, 17, K_PANEL);
        {
            const int32_t nx = cv_text(4, y + 3, &FONT_XS, LANE_SHORT[l], sel ? K_TEXT : K_DIM);
            if (proj.par[P_LSRC + l]) {          /* its own pattern's lane: the number after the name */
                char d[2] = {(char)('0' + proj.par[P_LSRC + l]), 0};
                cv_text(nx + 1, y + 3, &FONT_XS, d, K_ACC);
            }
        }
        if (proj.par[P_PROB + l] < 100) {        /* thinned: its chance, under the name */
            char pt[6];
            int j = 0;
            itoa_u((uint32_t)proj.par[P_PROB + l], pt);
            while (pt[j])
                j++;
            pt[j++] = '%';
            pt[j] = 0;
            cv_text(4, y + 11, &FONT_XS, pt, K_ACC);
        }
        if (sel)
            cv_rect(1, y + 4, 2, 9, K_ACC);
        {
            const int ll = proj.par[P_LLEN + l];
            if (ll < 16) {                       /* a shorter lane: its length, and where it stops */
                char lt[5] = "L";
                itoa_u((uint32_t)ll, lt + 1);
                cv_text(27, y + 11, &FONT_XS, lt, K_ACC);
                cv_rect(44 + ll * 12 - 3, y, 1, 18, K_ACC);
            }
        }
        for (s = 0; s < 16; s++) {
            int32_t x = 44 + s * 12;
            int on = (int)(bits >> s & 1u), cur = en_playing && s == (int)en_lane_pos[l] && en_pat_now == pat;
            if (s >= proj.par[P_LLEN + l])        /* past the lane's end: dimmed, not played */
                on = on ? -1 : 0;
            if (cur)
                cv_rect(x - 1, y, 12, 18, sel ? K_LINE : K_PANEL);
            if (on > 0)
                cv_round(x + 1, y + 3, 9, 12, 2, sel ? (cur ? K_TEXT : LANE_COL(l)) : mix(K_LINE, LANE_COL(l), 9));
            else if (on < 0)
                cv_round(x + 1, y + 3, 9, 12, 2, K_LINE);
            else
                cv_rect(x + 4, y + 8, 3, 3, (s & 3) == 0 ? K_DIM : K_LINE);
        }
    }
    for (s = 1; s < 4; s++)                     /* the beats */
        cv_rect(44 + s * 48 - 2, 18, 1, 118, K_LINE);
}

static void cpu_line(char *c);

/* a button's card (help_tick): its name, then what it does, in the main area's place */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("TEMPO"); L("TEMPO, EVOLVE, MOTION, DRIFT"); L("+ SELECT: TEMPO"); break;
    case B_ENV: L("PAD; AGAIN: ENV, LFO,"); L("THEN THE CHORDS"); L("SEL + ENV ON A VOICE'S"); L("PAGE: PLAY IT BY HAND"); break;
    case B_ARP: L("THE ACID; AGAIN: ITS ENV,"); L("ITS LINE, THEN THE PLUCK"); L("(THE COUNTER-MELODY)"); L("SEL + ALGO: ITS SOUND"); break;
    case B_LFO: L("THE DRONE; AGAIN: ITS"); L("ENVELOPE, THEN ITS LFO"); L("SEL + ENV: BY HAND"); L("SEL + ALGO: ITS SOUND"); break;
    case B_GLO: {                                /* and the audio's load, for testing on the device */
        static char cpu[40];
        cpu_line(cpu);
        L("GLOBAL");
        L("ROOT, THEME, TUNE, MIDI OUT");
        L("+ SEL: REMOTE (BROWSER)");
        L(cpu);
        break;
    }
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); break;
    case B_SEQ: L("DRUMS; AGAIN: LINKS, KIT,"); L("THEN DRUM DRIFT"); L("HOLD: THE SEQUENCER"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L("AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("AN OCTAVE DOWN: THIS"); L("PAGE'S VOICE, OR THE SONG"); L("SEQUENCER: THE DRUM ABOVE"); break;
    case B_OCTUP: L("AN OCTAVE UP: THIS"); L("PAGE'S VOICE, OR THE SONG"); L("SEQUENCER: THE DRUM BELOW"); break;
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
    cv_rect(16, 14, 208, 26, K_PAD);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k], K_TEXT);
}

/* the FX menu (FX on): the 27 keys, F3 at the top left, down the columns; an effect held, lit */
static void draw_fx(void)
{
    int k;
    {
        const char *hd = fx_bank == 3 ? "DJ SNIPS: HOLD TO PLAY" : fx_bank == 2 ? "FLEX: TAP LATCHES" : fx_bank ? "BEAT: TAP LATCHES" : "FX: TAP LATCHES",
                   *dl = fx_bank == 3 ? "DIAL: FLEX" : fx_bank == 2 ? "DIAL: BEAT / SNIPS" : fx_bank ? "DIAL: FX / FLEX" : "DIAL: BEAT FX";
        cv_text(4, 1, &FONT_XS, hd, K_ACC);
        cv_text(236 - text_w(&FONT_XS, dl), 1, &FONT_XS, dl, K_DIM);
    }
    for (k = 0; k < NKEYS; k++) {
        int32_t x = 2 + (k / 9) * 79, y = 16 + (k % 9) * 14;
        int pc = (5 + k) % 12, oct = 3 + (5 + k) / 12, on = (int)(ui.fx_mask >> KEY_FX[fx_bank][k] & 1u),
            locked = (int)(ui.fx_lock >> KEY_FX[fx_bank][k] & 1u);
        int black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        char t[24];
        int n = 0;
        const char *nm = EN_NOTE_NAME[pc];
        while (*nm)
            t[n++] = *nm++;
        t[n++] = (char)('0' + oct);
        t[n] = 0;
        if (fx_bank == 3) {                    /* the DJ snips: each key's sound, lit while held */
            const int held = (int)(ui.keys >> k & 1u);
            const char *lb = en_dj_name(k);
            if (held)
                cv_round(x - 1, y - 1, 77, 14, 2, KC_MOVE);
            else
                cv_rect(x - 1, y + 1, 2, 10, KC_MOVE);
            cv_text(x + 3, y, &FONT_XS, t, held ? K_BG : black ? K_DIM : K_TEXT);
            cv_text(x + 27, y, &FONT_XS, lb, held ? K_BG : KC_MOVE);
            continue;
        }
        if (fx_bank == 0 && k == FILL_KEY) {     /* F#5: the fill in the next bar (a tap, not an effect) */
            const int held = (int)(ui.keys >> k & 1u);
            if (held)
                cv_round(x - 1, y - 1, 77, 14, 2, KC_MOVE);
            else
                cv_rect(x - 1, y + 1, 2, 10, KC_MOVE);
            cv_text(x + 3, y, &FONT_XS, t, held ? K_BG : K_DIM);
            cv_text(x + 27, y, &FONT_XS, "FILL>", held ? K_BG : KC_MOVE);
            continue;
        }
        if (KEY_FX[fx_bank][k] == FX_NONE) {
            cv_text(x, y, &FONT_XS, t, K_LINE);
            continue;
        }
        {
            const uint16_t kc = fx_color(KEY_FX[fx_bank][k]);
            if (on) {                              /* held: a tile in its colour; latched: with a bar beside it */
                cv_round(x - 1, y - 1, 77, 14, 2, kc);
                if (locked)
                    cv_rect(x + 72, y + 1, 3, 10, K_BG);
            } else {
                cv_rect(x - 1, y + 1, 2, 10, kc);   /* at rest: its colour in a stripe and in the name */
            }
            cv_text(x + 3, y, &FONT_XS, t, on ? K_BG : black ? K_DIM : K_TEXT);
            cv_text(x + 27, y, &FONT_XS, FX_LABEL[KEY_FX[fx_bank][k]], on ? K_BG : kc);
        }
    }
}

/* an FX edit page: the gate's steps (the white keys: lit where it opens, the loop's end marked, the step playing
 * outlined), or the effect's name; the knobs are under it */
static int gx_head(void)                       /* the gate's step playing now */
{
    const uint32_t len = ui.fxs[EN_FXS_GX_LEN] < 2u ? 2u : ui.fxs[EN_FXS_GX_LEN];
    return en_playing ? (int)((en_bars - 1u) % len) : -1;
}
static int draw_fxscope(int p);
static void draw_fxedit(void)
{
    const int p = ui.fx_page - 1;
    const int32_t cw = 14;
    int s;
    cv_text(4, 1, &FONT_XS, "FX EDIT: DIAL OR SEL + FX", K_ACC);
    if (p == 0) {
        const int len = ui.fxs[EN_FXS_GX_LEN], head = gx_head();
        cv_text(4, 18, &FONT_B, "GATE", K_TEXT);
        cv_text(60, 20, &FONT_XS, "WHITE KEYS: STEPS", K_DIM);
        for (s = 0; s < 16; s++) {
            const int32_t x = 8 + s * cw, on = ui.gx_pat >> s & 1u, live = s < len;
            if (live) {
                cv_rect(x, 42, cw - 2, 40, on ? K_PAD : K_LINE);
                if (!on)
                    cv_rect(x + 1, 43, cw - 4, 38, K_BG);
            } else {
                cv_rect(x, 62, cw - 2, 2, K_LINE);       /* past the loop's end: a dash */
            }
            if (s == head)
                cv_rect(x, 86, cw - 2, 3, K_ACC);        /* the step playing */
        }
        cv_text(8, 96, &FONT_XS, "LOOP", K_DIM);
        {
            char t[12];
            int32_t x;
            itoa_u((uint32_t)len, t);
            x = cv_text(40, 96, &FONT_XS, t, K_TEXT);
            cv_text(x + 3, 96, &FONT_XS, len == 16 ? "STEPS: A BAR" : 16 % len == 0 ? "STEPS: IN A BAR" : "STEPS: AGAINST THE BAR", K_DIM);
        }
        cv_text(8, 112, &FONT_XS, "CLOSES", K_DIM);
        cv_text(52, 112, &FONT_XS, GX_TGT[ui.gx_tgt].name, K_TEXT);
    } else {
        cv_text(4, 18, &FONT_B, FXP[p].name, p == 18 ? KC_FLEX : FXP[p].bit < 63 ? fx_color(FXP[p].bit) : K_TEXT);
        if (draw_fxscope(p)) {
            if (p == 18) {
                cv_text(4, 112, &FONT_XS, "A LOOK AT THE PAD YOU PLAYED LAST (NOT EDITED):", K_DIM);
                cv_text(4, 126, &FONT_XS, "TALL = OLDER, BOX = LIVE, DASH = MUTE.", K_DIM);
            } else if (p == 19) {
                cv_text(4, 124, &FONT_XS, "THE KEYS PLAY THEM. THE KNOBS SET HOW.", K_DIM);
            } else {
            cv_text(4, 112, &FONT_XS, FXP[p].hold ? "HOLD ANY KEY TO HEAR IT." : "THE EFFECT IS ON WHILE YOU EDIT.", K_DIM);
            cv_text(4, 126, &FONT_XS, "THE KNOBS SET IT. DIAL: NEXT PAGE.", K_DIM);
            }
        } else {
            cv_text(4, 50, &FONT_XS, p == 16 ? "THE AUTO-DJ AND SCENES: NO EFFECT HERE." : p == 17 ? "THE UNIT'S SETTINGS: NO EFFECT HERE." :
                                     FXP[p].hold ? "HOLD ANY KEY TO HEAR IT." : "THE EFFECT IS ON WHILE YOU EDIT.", K_DIM);
            cv_text(4, 64, &FONT_XS, p == 16 || p == 17 ? "THE KNOBS SET THEM. DIAL: NEXT PAGE." : "THE KNOBS SET IT. DIAL: NEXT PAGE.", K_DIM);
        }
    }
}

/* the second home screen, PERFORM (HOME again): the section and the auto-DJ, the beat, the seven tracks (muted,
 * soloed, playing), the four scenes, what is latched or held in FX. Its keys are the track mix (the GLO layer);
 * its knobs, the FX knobs. */
static void draw_perf(void)
{
    static const char *const TN[8] = {"KCK", "HAT", "SNR", "PRC", "DRN", "ACD", "PAD", "PLK"};
    const int g = DJ_G(), b = dj_break_len(g);
    int i;
    {   /* the section, and the auto-DJ's place in it */
        const char *sn = en_sec == EN_SEC_BREAK ? "BREAK" : en_sec == EN_SEC_BUILD ? "BUILD" : "GROOVE";
        int32_t x = cv_text(6, 1, &FONT_B, sn, en_sec == EN_SEC_GROOVE ? K_TEXT : K_FROST);
        char t[24] = "DJ ";
        int n = 3, len = ui.dj_phase == 0 ? g : ui.dj_phase == 1 ? b : 4;
        if (en_sec_req)
            cv_text(x + 6, 4, &FONT_XS, en_sec_req == EN_CMD_BREAK + 1 ? "BREAK>" : en_sec_req == EN_CMD_BUILD + 1 ? "BUILD>" : "DROP>", K_ACC);
        if (ui.dj_on) {
            itoa_u(ui.dj_n, t + n);
            while (t[n])
                n++;
            t[n++] = '/';
            itoa_u((uint32_t)len, t + n);
            cv_text(232 - text_w(&FONT_XS, t), 4, &FONT_XS, t, K_ACC);
        } else {
            cv_text(232 - text_w(&FONT_XS, "DJ OFF"), 4, &FONT_XS, "DJ OFF", K_DIM);
        }
        if (ui.dj_on) {                           /* how far through the phase */
            const int32_t w = 228, f = len > 0 ? (int32_t)ui.dj_n * w / len : 0;
            cv_rect(6, 36, w, 3, K_LINE);
            cv_rect(6, 36, f > w ? w : f, 3, ui.dj_phase == 0 ? K_PAD : K_FROST);
        }
    }
    for (i = 0; i < 16; i++)                      /* the bar: its sixteen steps, the one playing lit */
        cv_rect(6 + i * 14, 24, 12, 8, en_playing && (int)en_drum_pos == i ? K_ACC : (i & 3) ? K_LINE : K_DIM);
    for (i = 0; i < 8; i++) {                     /* the tracks */
        static const uint8_t LN[8] = {0, 3, 1, 5, 6, 6, 6, 6};   /* the drum lane that shows its hits (6: not a lane) */
        const int32_t x = 4 + i * 29;
        const int muted = (ui.trk_m >> i & 1u) || (i < 4 && (ui.fx_mask >> (FX_MUTE_KICK + i) & 1u));
        const int soloed = ui.trk_s >> i & 1u, dim = muted || (ui.trk_s && !soloed);
        float lvl = i == EN_TR_DRONE ? en_bass_level : i == EN_TR_ACID ? en_acid_level : i == EN_TR_PAD ? en_pad_level :
                    i == EN_TR_PLUCK ? en_cm_level : en_drum_glow[LN[i]];
        int32_t h;
        lvl = lvl > 1.0f ? 1.0f : lvl;
        h = (int32_t)(lvl * 24.0f);
        const uint16_t tc = i == 0 ? LANE_COL(0) : i == 1 ? LANE_COL(3) : i == 2 ? LANE_COL(1) : i == 3 ? LANE_COL(5) :
                            i == 4 ? K_BASS : i == 5 ? K_ACID : i == 7 ? KC_COLOR : K_PAD;
        cv_round(x, 46, 27, 38, 3, soloed ? tc : K_PANEL);
        if (!dim && h > 0)
            cv_rect(x + 3, 78 - h, 21, h, soloed ? K_BG : tc);
        text_c(x + 13, 48, &FONT_XS, TN[i], soloed ? K_BG : dim ? K_DIM : K_TEXT);
        if (muted)
            cv_rect(x + 2, 62, 23, 2, K_ACC);   /* struck through */
    }
    for (i = 0; i < 4; i++) {                     /* the scenes */
        const int32_t x = 4 + i * 58;
        const int set = ui.scene_set >> i & 1u, wait = ui.scene_q == i + 1;
        char t[8] = "SCENE 1";
        t[6] = (char)('1' + i);
        cv_round(x, 88, 54, 20, 3, wait ? K_ACC : set ? K_PAD : K_PANEL);
        text_c(x + 27, 91, &FONT_XS, wait ? "NEXT BAR" : t, wait || set ? K_BG : K_DIM);
    }
    {   /* in FX: what is on (latched ones in the primary, held ones in the accent) */
        int32_t x = 6;
        int n = 0, bit;
        cv_text(6, 110, &FONT_XS, "FX", K_DIM);
        x = 26;
        for (bit = 0; bit < 63; bit++)
            if (ui.fx_mask >> bit & 1u) {
                const char *s = FX_LABEL[bit];
                if (!s || !s[0])
                    continue;
                if (x + text_w(&FONT_XS, s) > 232) {
                    cv_text(x, 110, &FONT_XS, "+", K_DIM);
                    break;
                }
                x = cv_text(x, 110, &FONT_XS, s, fx_color(bit)) + 8;
                n++;
            }
        if (!n)
            cv_text(26, 110, &FONT_XS, "NONE", K_DIM);
    }
    {   /* the mood: a bar, the middle marked (the ALGORITHM dial) */
        char t[8];
        cv_text(6, 126, &FONT_XS, "MOOD", K_DIM);
        cv_rect(46, 131, 150, 5, K_LINE);
        cv_rect(46, 131, ui.mood * 150 / 100, 5, ui.mood > 50 ? K_ACC : K_PAD);
        cv_rect(46 + 74, 128, 2, 11, K_TEXT);
        itoa_u(ui.mood, t);
        cv_text(204, 126, &FONT_XS, t, K_TEXT);
    }
}

/* SEL held alone (after 0.4 s): what the shift does, on this page */
static int shift_show(void) { return (ui.btn >> B_SEL & 1u) && ui.shift_hint && !(ui.btn_used >> B_SEL & 1u); }
static void draw_shift(void)
{
    static const char *const FXG[8] = {"ANY KEY   ITS SETTINGS", "PRESETS   PIECE, NEXT BAR", "ALGO      SOUND / KIT",
                                       "ENV       VOICE BY HAND", "SEQ       LINKS ON / OFF", "FX        EDIT PAGES",
                                       "GLO       REMOTE (BROWSER)", ""};
    static const char *const GEN[8] = {"F3 - D4   PATTERN 1-6", "PRESETS   PIECE, NEXT BAR", "ALGO      SOUND / KIT",
                                       "ENV       VOICE BY HAND", "SEQ       LINKS ON / OFF", "FX        EDIT PAGES",
                                       "GLO       REMOTE (BROWSER)", ""};
    static const char *const SQ[8] = {"WHITE KEYS   PATTERN 1-6", "BLACK KEYS   COPY HERE TO 1-6", "OCT-         CLEAR THIS LANE",
                                      "OCT+         THE LANE'S OWN", "PRESETS      PIECE, NEXT BAR", "ALGO         KIT",
                                      "SEQ          LINKS ON / OFF", "FX           EDIT PAGES"};
    const char *const *l = ui.view == V_SEQ ? SQ : ui.fx_menu && !ui.fx_page ? FXG : GEN;
    int k;
    cv_text(4, 1, &FONT_XS, "SHIFT: SEL +", K_ACC);
    for (k = 0; k < 8; k++)
        if (l[k][0])
            cv_text(8, 18 + k * 15, &FONT_XS, l[k], K_TEXT);
}

/* the load, as text: CPU (the first core), XR (the xruns), C2 and the second core's load (the experimental build), S1/S2 (the engine is
 * easing off to keep up; absent while all is well) */
static void cpu_line(char *c)
{
    int n = 0;
    c[n++] = 'C', c[n++] = 'P', c[n++] = 'U', c[n++] = ' ';
    itoa_u(plat_cpu_pct(), c + n);
    while (c[n])
        n++;
    c[n++] = ' ', c[n++] = 'X', c[n++] = 'R', c[n++] = ' ';
    itoa_u(plat_xruns() > 999u ? 999u : plat_xruns(), c + n);
    while (c[n])
        n++;
    if (plat_core2()) {                          /* C2: the reverb and more on the second core; C1: it fell back to one */
        c[n++] = ' ', c[n++] = 'C', c[n++] = plat_core2() == 1u ? '2' : '1', c[n] = 0;
        if (plat_core2() == 1u) {                /* and how full the other core is */
            c[n++] = ' ';
            itoa_u(plat_cpu1_pct() > 99u ? 99u : plat_cpu1_pct(), c + n);
            while (c[n])
                n++;
        }
    }
    if (en_shed_level)
        c[n++] = ' ', c[n++] = 'S', c[n++] = (char)('0' + en_shed_level);
    c[n] = 0;
}

/* the track mix (GLO held): the keys, as the FX menu's, each its track's mute (M:) or solo (S:), lit while on */
static void draw_mix(void)
{
    int k;
    char c[40];
    cv_text(4, 1, &FONT_XS, "MIX: GLO + KEY", K_ACC);
    cpu_line(c);
    cv_text(118, 1, &FONT_XS, c, K_DIM);
    if (ui.dj_on) {                            /* the auto-DJ: where it is, under the load */
        char d[24];
        const char *pn = ui.dj_phase == 0 ? "DJ GROOVE " : ui.dj_phase == 1 ? "DJ BREAK " : "DJ BUILD ";
        int n = 0;
        while (*pn)
            d[n++] = *pn++;
        itoa_u(ui.dj_n, d + n);
        cv_text(118, 8, &FONT_XS, d, K_ACC);
    }
    for (k = 0; k < NKEYS; k++) {
        int code = MIXK[k], on = mix_state(k);
        int32_t x = 2 + (k / 9) * 79, y = 16 + (k % 9) * 14;
        int pc = (5 + k) % 12, oct = 3 + (5 + k) / 12, black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        char t[24], lb[12];
        int n = 0, j = 0;
        const char *nm = EN_NOTE_NAME[pc];
        if (code < 0)
            continue;
        while (*nm)
            t[n++] = *nm++;
        t[n++] = (char)('0' + oct);
        t[n] = 0;
        if (code < 8 || (code >= 8 && code < 16)) {
            const char *q = MIX_NAME[code < 8 ? code : code - 8];
            lb[j++] = code < 8 ? 'M' : 'S';
            lb[j++] = ':';
            while (*q && j < 11)
                lb[j++] = *q++;
            lb[j] = 0;
        } else if (code == 16 || code == 17) {
            const char *q = code == 16 ? "M:DRUMS" : "S:DRUMS";
            while (*q)
                lb[j++] = *q++;
            lb[j] = 0;
        } else if (code >= 20 && code <= 23) {
            const char *q = "SCENE ";
            while (*q)
                lb[j++] = *q++;
            lb[j++] = (char)('1' + code - 20);
            lb[j] = 0;
        } else if (code == 24 || code == 25 || code == 27 || code == 28 || code == 29) {
            const char *q = code == 24 ? "RESET ALL" : code == 25 ? "AUTO DJ" : code == 27 ? "DJ PIECE" : code == 28 ? "UNDO" : "TAP TEMPO";
            while (*q)
                lb[j++] = *q++;
            lb[j] = 0;
        } else if (code == 26) {
            lb[j++] = 'D';
            lb[j++] = 'J';
            lb[j++] = ' ';
            if (ui.dj_len == 4) {
                lb[j++] = 'S', lb[j++] = 'Y', lb[j++] = 'N', lb[j++] = 'C';
            } else {
                itoa_u(DJ_LEN[ui.dj_len % 5u], lb + j);
                while (lb[j])
                    j++;
                lb[j++] = 'B';
            }
            lb[j] = 0;
        } else {
            const char *q = "CLEAR";
            while (*q)
                lb[j++] = *q++;
            lb[j] = 0;
        }
        {
            const uint16_t kc = code < 8 || code == 16 ? KC_MUTE : (code >= 8 && code < 16) || code == 17 ? KC_FILTER :
                                (code >= 20 && code <= 23) ? KC_REPEAT : (code >= 25 && code <= 27) ? KC_GATE :
                                code == 24 || code == 28 || code == 30 ? KC_MOVE : KC_BUILD;
            if (on)
                cv_round(x - 1, y - 1, 77, 14, 2, kc);
            else
                cv_rect(x - 1, y + 1, 2, 10, kc);
            cv_text(x + 3, y, &FONT_XS, t, on ? K_BG : black ? K_DIM : K_TEXT);
            cv_text(x + 27, y, &FONT_XS, lb, on ? K_BG : kc);
        }
    }
}

#include "runner.c"
static int runner_on(void) { return kr.on; }

/* THE SCOPES: on the pages that shape a sound the main area draws what the knobs make: the envelopes' curves, the
 * LFOs' waves, the filters' response, the echo and the reverb. The shape follows the knobs as they turn; a dot
 * runs along it so the page is alive. (Drawn from the settings, not measured: it shows the idea, not the exact milliseconds.) */
#define SC_X 12
#define SC_W 216
#define SC_Y 24
#define SC_H 90
enum { SCOPE_NONE, SCOPE_PENV, SCOPE_DENV, SCOPE_AENV, SCOPE_PLFO, SCOPE_DLFO, SCOPE_DFILT, SCOPE_AFILT, SCOPE_SPACE, SCOPE_CMEL, SCOPE_MASTER, SCOPE_MIX, SCOPE_REVERB, SCOPE_GROOVE };
static int scope_kind(int v)
{
    switch (v) {
    case V_PADENV: return SCOPE_PENV;
    case V_DRENV: return SCOPE_DENV;
    case V_ACENV: return SCOPE_AENV;
    case V_PADLFO: return SCOPE_PLFO;
    case V_DRLFO: return SCOPE_DLFO;
    case V_DRONE: return SCOPE_DFILT;
    case V_ACID: return SCOPE_AFILT;
    case V_SPACE: return SCOPE_SPACE;
    case V_MASTER: return SCOPE_MASTER;
    case V_REVERB: return SCOPE_REVERB;
    case V_MIXER: return SCOPE_MIX;
    case V_GROOVE: return SCOPE_GROOVE;
    case V_CMEL: case V_CMEL2: return SCOPE_CMEL;
    default: return SCOPE_NONE;
    }
}
static uint32_t scope_tick(void) { return plat_ms() / 90u; }       /* ~11 steps a second for the dot */

static float lfo_wave(int shape, float ph)       /* -1..1; ph in cycles */
{
    float f = ph - fm_floorf(ph);
    switch (shape) {
    case 0: return fm_sinf(ph * FM_TWO_PI);
    case 1: return f < 0.5f ? 4.0f * f - 1.0f : 3.0f - 4.0f * f;
    case 2: return 2.0f * f - 1.0f;
    case 3: return f < 0.5f ? 1.0f : -1.0f;
    case 4: return (float)(hash(2166136261u, (uint32_t)(ph * 4.0f + 100.0f)) >> 8 & 255u) * (1.0f / 127.5f) - 1.0f;
    default: return 0.6f * fm_sinf(ph * FM_TWO_PI) + 0.4f * fm_sinf(ph * 2.7f * FM_TWO_PI + 1.3f);
    }
}

static uint8_t sc_y[SC_W];
static int32_t sc_oy = SC_Y, sc_oh = SC_H;       /* the trace's top and height (the effects' pages draw it smaller) */
static void scope_trace(uint16_t col)            /* sc_y[x] (0 .. SC_H): a filled curve with a bright line, and the baseline */
{
    int32_t x;
    cv_rect(SC_X, sc_oy + sc_oh, SC_W, 1, K_LINE);
    cv_rect(SC_X, sc_oy, 1, sc_oh, K_LINE);
    for (x = 0; x < SC_W; x++) {
        const int32_t y = sc_y[x], py = x ? sc_y[x - 1] : y, hi = y > py ? y : py, lo = y > py ? py : y;
        if (y > 1)
            cv_rect(SC_X + x, sc_oy + sc_oh - y, 1, y, mix(K_BG, col, 4));
        cv_rect(SC_X + x, sc_oy + sc_oh - hi - 1, 1, hi - lo + 2, col);
    }
}
static void scope_dot(float u, uint16_t col)     /* the dot at u (0..1 across) riding the trace */
{
    const int32_t x = (int32_t)(u * (float)(SC_W - 1));
    cv_disc(SC_X + x, sc_oy + sc_oh - (int32_t)sc_y[x], 3, K_TEXT);
    cv_disc(SC_X + x, sc_oy + sc_oh - (int32_t)sc_y[x], 2, col);
}
static float scope_expo(float t, float k) { return fm_exp2f(-t * k); }

/* the same, for the effects' edit pages: the effect drawn from its settings, moving where it moves */
static int draw_fxscope(int p)
{
    const float tk = (float)(scope_tick() % 64u) * (1.0f / 64.0f);
    const uint16_t col = fx_color(FXP[p].bit);
    const int32_t H = 58;
    int32_t x;
#define FS(i) ((float)ui.fxs[i] * 0.01f)
    sc_oy = 46, sc_oh = H;
    switch (p) {
    case 1: case 2: {                            /* the phaser's notches, the flanger's comb, moving with the LFO */
        const int fl = p == 2;
        const float rate = FS(fl ? EN_FXS_FL_RATE : EN_FXS_PH_RATE), depth = FS(fl ? EN_FXS_FL_DEPTH : EN_FXS_PH_DEPTH),
                    fb = FS(fl ? EN_FXS_FL_FB : EN_FXS_PH_FB), mixv = FS(fl ? EN_FXS_FL_MIX : EN_FXS_PH_MIX),
                    sw = 0.5f + 0.5f * fm_sinf(FM_TWO_PI * tk * (float)(1 + (int)(rate * 3.0f)));
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            float y = 1.0f;
            if (fl) {
                float c = 0.5f + 0.5f * fm_cosf(FM_TWO_PI * u * (5.0f + depth * 9.0f * sw));
                c = c * (1.0f - fb * 0.6f) + c * c * fb * 0.6f;
                y = 1.0f - mixv * (1.0f - c) * 0.9f;
            } else {
                int k;
                for (k = 0; k < 3; k++) {
                    const float c = 0.18f + 0.27f * (float)k + depth * 0.14f * (sw * 2.0f - 1.0f), w = 0.055f - 0.035f * fb, d = (u - c) / w;
                    y -= mixv * 0.9f * fm_expf(-d * d);
                }
            }
            sc_y[x] = (uint8_t)(fm_clampf(y, 0.0f, 1.0f) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    }
    case 4:                                      /* the tape's wobble: slow wow, fast flutter */
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            const float y = 0.5f + FS(EN_FXS_TP_WOW) * 0.32f * fm_sinf(FM_TWO_PI * (u * 1.5f + tk)) +
                            FS(EN_FXS_TP_FLUT) * 0.12f * fm_sinf(FM_TWO_PI * (u * 9.0f + tk * 8.0f));
            sc_y[x] = (uint8_t)(fm_clampf(y, 0.0f, 1.0f) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    case 3: {                                    /* the vinyl: hiss, and pops now and then, scrolling */
        const float hiss = FS(EN_FXS_VN_HISS), lv = 0.15f + 0.85f * FS(EN_FXS_VN_LEVEL), ck = FS(EN_FXS_VN_CRACK);
        const uint32_t t0 = scope_tick() * 3u;
        for (x = 0; x < SC_W; x++) {
            const uint32_t n = hash(2166136261u, (uint32_t)x + t0), pop = hash(n >> 7, ((uint32_t)x + t0) / 4u);
            float y = 0.5f + hiss * 0.18f * ((float)(n & 255u) * (1.0f / 127.5f) - 1.0f);
            if ((pop & 127u) < (uint32_t)(ck * 14.0f))
                y += (pop & 256u ? 1.0f : -1.0f) * lv * 0.42f;
            sc_y[x] = (uint8_t)(fm_clampf(y, 0.0f, 1.0f) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    }
    case 6: {                                    /* the pump: the level ducking at each hit, coming back with the release */
        const int dips = 2 << (ui.fxs[EN_FXS_PM_RATE] & 3);
        const float depth = FS(EN_FXS_PM_DEPTH), k = 1.0f + (1.0f - FS(EN_FXS_PM_REL)) * 14.0f;
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1) * (float)dips, f = u - fm_floorf(u);
            sc_y[x] = (uint8_t)((1.0f - depth * scope_expo(f, k)) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    }
    case 9: {                                    /* the brake: the speed to a stop, then spinning back up */
        const float dn = 0.1f + 0.5f * FS(EN_FXS_BR_TIME), up = 0.05f + 0.3f * FS(EN_FXS_BR_RET), a = 0.12f;
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            float y;
            if (u < a)
                y = 1.0f;
            else if (u < a + dn)
                y = (1.0f - (u - a) / dn) * (1.0f - (u - a) / dn);
            else if (u > 1.0f - up)
                y = (u - (1.0f - up)) / up;
            else
                y = 0.0f;
            sc_y[x] = (uint8_t)(y * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    }
    case 7:                                      /* the riser: the filter and the wash climbing to the top */
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            sc_y[x] = (uint8_t)((0.12f + 0.88f * u * u) * (0.35f + 0.65f * FS(EN_FXS_RS_FILT)) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    case 5: {                                    /* the roll: its hits, closer and louder as it speeds up */
        const float s0 = 0.30f * fm_exp2f(-(float)ui.fxs[EN_FXS_RL_START]), s1 = 0.30f * fm_exp2f(-(float)ui.fxs[EN_FXS_RL_END]),
                    sw = FS(EN_FXS_RL_SWELL);
        float u = 0.02f;
        cv_rect(SC_X, sc_oy + sc_oh, SC_W, 1, K_LINE);
        while (u < 1.0f) {
            const int32_t bh = (int32_t)((float)(H - 6) * (0.35f + 0.65f * sw * u + 0.0f * tk));
            cv_rect(SC_X + (int32_t)(u * (float)(SC_W - 6)), sc_oy + sc_oh - bh, 4, bh, col);
            u += fm_maxf(s0 + (s1 - s0) * u, 0.024f);
        }
        return 1;
    }
    case 11: {                                   /* the sweep: the filter's cutoff going up and down */
        const int cyc = 1 << (ui.fxs[EN_FXS_SW_RATE] > 2 ? ui.fxs[EN_FXS_SW_RATE] - 2 : 0);
        const float depth = 0.1f + 0.9f * FS(EN_FXS_SW_DEPTH);
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1) * (float)cyc, f = u - fm_floorf(u), tri = f < 0.5f ? f * 2.0f : 2.0f - f * 2.0f;
            sc_y[x] = (uint8_t)((0.1f + 0.9f * tri * depth) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    }
    case 10:                                     /* half time: the speed falling to a half, a quarter */
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1), sp = 1.0f / (ui.fxs[EN_FXS_HF_SPEED] == 0 ? 1.5f : ui.fxs[EN_FXS_HF_SPEED] == 1 ? 2.0f : 4.0f),
                        sl = 0.03f + 0.4f * FS(EN_FXS_HF_SLIDE), f = fm_clampf((u - 0.2f) / sl, 0.0f, 1.0f);
            sc_y[x] = (uint8_t)((1.0f - (1.0f - sp) * f) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    case 20: {                                   /* talk: the two formants (F1 above, F2) through the cycle */
        static const float VF1[5] = {700.0f, 530.0f, 270.0f, 570.0f, 300.0f}, VF2[5] = {1220.0f, 1840.0f, 2290.0f, 840.0f, 870.0f};
        const float depth = 0.15f + 0.85f * FS(EN_FXS_TK_DEPTH);
        uint8_t f2[SC_W];
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1) * 2.0f, ph = u - fm_floorf(u), tri = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f,
                        pos = fm_clampf(tri * 4.0f * depth, 0.0f, 4.0f);
            const int i = pos >= 4.0f ? 3 : (int)pos;
            const float fr = pos - (float)i, a = VF1[i] + (VF1[i + 1] - VF1[i]) * fr, b = VF2[i] + (VF2[i + 1] - VF2[i]) * fr;
            sc_y[x] = (uint8_t)(a * (float)(H - 6) / 2400.0f);
            f2[x] = (uint8_t)(b * (float)(H - 6) / 2400.0f);
        }
        scope_trace(mix(K_BG, col, 7));
        for (x = 0; x < SC_W; x++)
            cv_rect(SC_X + x, sc_oy + sc_oh - f2[x] - 1, 1, 2, col);
        return 1;
    }
    case 21:                                     /* wah: the band-pass at its lowest, and where the loudness will push it */
        for (x = 0; x < SC_W; x++) {
            const float k = 0.7f - 0.0058f * (float)ui.fxs[EN_FXS_WH_RESO], f = 20.0f * fm_exp2f((float)x / (float)(SC_W - 1) * 10.0f),
                        fc = 280.0f * fm_exp2f(4.0f * FS(EN_FXS_WH_RANGE) * (0.25f + 0.5f * tk)), r = f / fc, a = 1.0f - r * r,
                        m = (r * k) / fm_sqrtf(a * a + r * r * k * k + 1e-6f);
            sc_y[x] = (uint8_t)(fm_clampf(m, 0.0f, 1.0f) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    case 19: {                                   /* the DJ snips: the first key held, by name; or the bank's twenty-seven, dim */
        int s, held = -1;
        for (s = 0; s < 27; s++)
            if (ui.keys >> s & 1u) {
                held = s;
                break;
            }
        if (held >= 0) {
            cv_text(SC_X + 4, sc_oy + 4, &FONT_L, en_dj_name(held), KC_MOVE);
        } else {
            for (s = 0; s < 27; s++)
                cv_text(SC_X + 4 + (s / 7) * 54, sc_oy + 2 + (s % 7) * 11, &FONT_XS, en_dj_name(s), K_DIM);   /* (eleven a row: the caption under it is clear of them) */
        }
        return 1;
    }
    case 18: {                                   /* Flex Beat: the last pad's bar, a column a sixteenth */
        int s;
        cv_rect(SC_X, sc_oy + sc_oh, SC_W, 1, K_LINE);
        for (s = 0; s < 16; s++) {
            const uint8_t b = en_flex_step(ui.flex_last, s);
            const int e = b & 15, rev = b >> 4 & 1, mute = b >> 5 & 1, div = b >> 6 & 3;
            const int32_t x = SC_X + 4 + s * 13, full = H - 4, hh = e == 15 ? full : 12 + (7 - e) * 5;
            if (mute) {
                cv_rect(x, sc_oy + sc_oh - 4, 11, 2, K_LINE);
            } else if (e == 15) {                /* live: a box */
                cv_frame(x, sc_oy + sc_oh - full, 11, full, KC_FLEX);
            } else {
                int k;
                cv_rect(x, sc_oy + sc_oh - hh, 11, hh, rev ? K_ACC : KC_FLEX);
                for (k = 1; k < (1 << div); k++)   /* a stutter: cut in two, four, eight */
                    cv_rect(x + k * 11 / (1 << div), sc_oy + sc_oh - hh, 1, hh, K_BG);
            }
            if (en_playing && en_flx_state == 2 && (int)(en_bars & 15u) == s)   /* (the step playing in the bar: marked) */
                cv_rect(x, sc_oy + sc_oh + 3, 11, 2, K_TEXT);
        }
        return 1;
    }
    case 14:                                     /* the reverse swell: a tail running backwards into the beat */
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            sc_y[x] = (uint8_t)(u * u * u * (0.4f + 0.6f * FS(EN_FXS_RV_MIX)) * (float)(H - 6));
        }
        scope_trace(col);
        return 1;
    default:
        return 0;
    }
#undef FS
}

static void draw_scope(int kind)
{
    const float tk = (float)(scope_tick() % 64u) * (1.0f / 64.0f);
    const char *title = "", *sub = "";
    uint16_t col = K_PAD;
    int32_t x;
    sc_oy = SC_Y, sc_oh = SC_H;
    if (kind == SCOPE_REVERB) {                  /* the reverb: the gap of the predelay, then the tail by its type */
        static const char *const TY[3] = {"PLATE", "SHIMMER", "GATED"};
        const int ty = proj.par[P_RV_TYPE] < 0 ? 0 : proj.par[P_RV_TYPE] > 2 ? 2 : proj.par[P_RV_TYPE];
        const float len = 0.25f + 0.75f * (float)proj.par[P_SIZE] * 0.01f, amt = (float)proj.par[P_RV_X] * 0.01f,
                    gap = (6.0f + 24.0f * (float)proj.par[P_RV_PRE] * 0.01f) * (1.0f / 30.0f) * 0.2f, hold = 0.12f + 0.45f * amt;
        col = th->skin ? IMP_TEAL : KC_COLOR;
        cv_text(6, 2, &FONT_B, "REVERB", col);
        cv_text(6 + text_w(&FONT_B, "REVERB") + 8, 6, &FONT_XS, TY[ty], K_TEXT);
        cv_text(6, SC_Y + SC_H + 6, &FONT_XS, ty == 1 ? "AN OCTAVE UP, BLOOMING IN THE TAIL" : ty == 2 ? "CUT OFF AFTER THE HOLD" : "THE PLATE'S TAIL, AFTER THE PREDELAY", K_DIM);
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            float y = 0.0f;
            if (u >= gap) {
                const float tt = (u - gap) / (1.0f - gap);
                y = 0.8f * scope_expo(tt / len, 4.0f);
                if (ty == 1) {                   /* the bloom: humps late in the tail, as high as the amount */
                    const float b1 = tt - 0.3f, b2 = tt - 0.55f;
                    y += amt * 0.35f * (fm_expf(-b1 * b1 * 90.0f) + 0.7f * fm_expf(-b2 * b2 * 90.0f));
                } else if (ty == 2) {
                    y = tt < hold ? 0.8f * scope_expo(tt / (len * 3.0f), 2.0f) : 0.0f;
                }
                y = fm_minf(y, 1.0f);
            }
            sc_y[x] = (uint8_t)(y * (float)(SC_H - 8));
        }
        scope_trace(col);
        cv_rect(SC_X, SC_Y + SC_H - 6, (int32_t)(gap * (float)SC_W), 2, K_ACC);   /* the predelay */
        return;
    }
    if (kind == SCOPE_GROOVE) {                  /* the groove: each lane as it plays (a hit added: bright; one taken out: a dash), the lane chosen framed, its source and density */
        int l, s;
        cv_text(6, 2, &FONT_B, "GROOVE", K_ACC);
        cv_text(6 + text_w(&FONT_B, "GROOVE") + 8, 6, &FONT_XS, "DENSITY, SOURCE: PER LANE", K_DIM);
        for (l = 0; l < EN_NLANE; l++) {
            const int src = proj.par[P_LSRC + l] ? proj.par[P_LSRC + l] - 1 : en_pat_now, dens = proj.par[P_DENS + l];
            const uint16_t wr = en_pats[src][l], pl = en_lane_mask(l, src, dens);
            const int32_t y = 22 + l * 17, sel = l == (int)ui.lane;
            char tx[6];
            if (sel)
                cv_rect(0, y - 1, 240, 16, K_PANEL);
            cv_text(4, y + 2, &FONT_XS, LANE_SHORT[l], sel ? K_TEXT : K_DIM);
            for (s = 0; s < 16; s++) {
                const int32_t x = 44 + s * 9;
                const int on = pl >> s & 1, was = wr >> s & 1;
                if (on)
                    cv_rect(x, y + 1, 7, 11, was ? LANE_COL(l) : K_TEXT);
                else if (was)
                    cv_rect(x, y + 5, 7, 3, K_LINE);
                else
                    cv_rect(x + 3, y + 6, 1, 1, K_LINE);
                if (en_playing && s == (int)en_lane_pos[l])
                    cv_rect(x, y + 13, 7, 1, K_TEXT);
            }
            tx[0] = proj.par[P_LSRC + l] ? 'P' : '=';
            tx[1] = proj.par[P_LSRC + l] ? (char)('0' + proj.par[P_LSRC + l]) : 0;
            tx[2] = 0;
            cv_text(192, y + 2, &FONT_XS, tx, proj.par[P_LSRC + l] ? K_ACC : K_DIM);
            itoa_u((uint32_t)dens, tx);
            cv_text(212, y + 2, &FONT_XS, tx, dens == 50 ? K_DIM : K_ACC);
        }
        return;
    }
    if (kind == SCOPE_MIX) {                     /* the mixer: a strip a layer, its level set and its level now, the output last */
        static const char *const NAME[6] = {"PAD", "DRN", "ACID", "PLK", "DRUM", "OUT"};
        static const uint8_t TRK[6] = {0x40, 0x10, 0x20, 0x80, 0x0F, 0};
        const uint8_t lv[5] = {(uint8_t)proj.par[P_PAD], (uint8_t)proj.par[P_BASS], (uint8_t)proj.par[P_ACID], (uint8_t)proj.par[P_CM_LVL], (uint8_t)proj.par[P_PULSE]};
        float live[6], dg = 0.0f;
        const uint16_t C[6] = {K_PAD, K_BASS, K_ACID, KC_COLOR, RGB(255, 160, 60), K_ACC};
        int k, d;
        for (d = 0; d < EN_NLANE; d++)
            dg = fm_maxf(dg, en_drum_glow[d]);
        live[0] = en_pad_level, live[1] = en_bass_level, live[2] = en_acid_level, live[3] = en_cm_level, live[4] = dg, live[5] = en_out_peak;
        cv_text(6, 2, &FONT_B, "MIXER", K_PAD);
        cv_text(6 + text_w(&FONT_B, "MIXER") + 8, 6, &FONT_XS, "SET LEVEL, AND WHAT SOUNDS", K_DIM);
        for (k = 0; k < 6; k++) {
            const int32_t sx = 10 + k * 36, sh = SC_H - 18, top = SC_Y + 12;
            const int muted = k < 5 && (ui.trk_m & TRK[k]) == TRK[k], solo = k < 5 && ui.trk_s & TRK[k];
            const int32_t set = k < 5 ? (int32_t)lv[k] * sh / 100 : 0, now = (int32_t)(fm_minf(live[k], 1.0f) * (float)sh);
            char tx[8];
            cv_rect(sx, top, 30, sh, k == ui.mix_trk ? K_TEXT : K_LINE);   /* (the chosen layer framed) */
            cv_rect(sx + 1, top + 1, 28, sh - 2, K_BG);
            if (k < 5) {
                cv_rect(sx + 1, top + sh - 1 - set, 18, set, muted ? K_DIM : mix(K_BG, C[k], 5));   /* the fader: what it is set to */
                cv_rect(sx, top + sh - 1 - set, 20, 2, muted ? K_DIM : C[k]);
                itoa_u((uint32_t)lv[k], tx);
                cv_text(sx + 2, SC_Y - 2, &FONT_XS, tx, muted ? K_DIM : K_TEXT);
            }
            if (now > 0)
                cv_rect(sx + 21, top + sh - 1 - now, 8, now, muted ? K_DIM : C[k]);                  /* the meter: what sounds */
            cv_text(sx + (30 - text_w(&FONT_XS, NAME[k])) / 2, top + sh + 3, &FONT_XS, NAME[k], muted ? K_DIM : solo ? K_ACC : K_TEXT);
            if (muted)
                cv_text(sx + 11, top + sh / 2 - 4, &FONT_XS, "M", K_TEXT);
            if (k < 5) {                         /* its place: a tick under the strip, across its width */
                const int32_t pxm = sx + 2 + (int32_t)proj.par[P_PAN + k] * 26 / 100;
                cv_rect(pxm, top + sh + 13, 2, 3, proj.par[P_PAN + k] == 50 ? K_DIM : K_ACC);
            }
        }
        return;
    }
    if (kind == SCOPE_MASTER) {                  /* the master bus: the compressor's curve (in, out, in dB), where the limiter stops it, and the meters */
        const int PW = 108, PH = SC_H;
        const float thr = -30.0f + 0.3f * (float)proj.par[P_MC_THR], make = 0.12f * (float)proj.par[P_MC_MAKE],
                    ceil = -12.0f + 0.12f * (float)proj.par[P_ML_CEIL];
        static const float RATIO[7] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f};
        const float slope = 1.0f - 1.0f / RATIO[proj.par[P_MC_RATIO] < 0 ? 0 : proj.par[P_MC_RATIO] > 6 ? 6 : proj.par[P_MC_RATIO]];
        const int32_t px = SC_X, py = SC_Y;
        int32_t bx = px + PW + 14, bw = SC_W - PW - 14, y;
        char t[24];
        col = KC_FILTER;
        cv_text(6, 2, &FONT_B, "MASTER", col);
        cv_text(6 + text_w(&FONT_B, "MASTER") + 8, 6, &FONT_XS, "COMPRESSOR, LIMITER", K_DIM);
        cv_rect(px, py, 1, PH, K_LINE);
        cv_rect(px, py + PH, PW, 1, K_LINE);
        cv_rect(px + (int32_t)((thr + 36.0f) * (float)PW / 36.0f), py, 1, PH, mix(K_BG, col, 5));         /* the threshold */
        for (x = 0; x < PW; x++) {
            const float in = -36.0f + 36.0f * (float)x / (float)(PW - 1), lv = in - thr;
            float gr = 0.0f, out;
            int32_t yy;
            if (lv > 3.0f)
                gr = slope * lv;
            else if (lv > -3.0f)
                gr = slope * (lv + 3.0f) * (lv + 3.0f) * (1.0f / 12.0f);
            out = in - gr + make;
            if (out > ceil)
                out = ceil;
            yy = (int32_t)((out + 36.0f) * (float)PH / 48.0f);
            yy = yy < 0 ? 0 : yy > PH - 2 ? PH - 2 : yy;
            if (x % 3 == 0)                      /* unity, dotted */
                cv_rect(px + x, py + PH - (int32_t)((in + 36.0f) * (float)PH / 48.0f), 1, 1, K_DIM);
            cv_rect(px + x, py + PH - yy - 1, 1, 3, col);
        }
        y = py + PH - (int32_t)((ceil + 36.0f) * (float)PH / 48.0f);                                           /* the ceiling */
        cv_rect(px, y, PW, 1, mix(K_BG, K_ACC, 6));
        /* the meters, as they are now: the compressor's reduction, the limiter's, the output's peak */
        {
            const float gr = en_comp_gr, lg = en_lim_gr, pk = en_out_peak;
            int32_t w;
            cv_text(bx, py, &FONT_XS, "COMP", K_DIM);
            w = (int32_t)(fm_minf(gr / 18.0f, 1.0f) * (float)bw);
            cv_rect(bx, py + 14, bw, 8, K_LINE);
            if (w > 0)
                cv_rect(bx + bw - w, py + 14, w, 8, KC_FILTER);
            {   /* the reduction as "-3.2 dB" ("0 dB" when none) */
                const uint32_t g10 = (uint32_t)(gr * 10.0f + 0.5f);
                int j = 0;
                if (g10) {
                    t[j++] = '-';
                    itoa_u(g10 / 10u, t + j);
                    while (t[j])
                        j++;
                    t[j++] = '.';
                    t[j++] = (char)('0' + g10 % 10u);
                } else {
                    t[j++] = '0';
                }
                t[j++] = ' ', t[j++] = 'd', t[j++] = 'B', t[j] = 0;
                cv_text(bx, py + 26, &FONT_XS, t, K_TEXT);
            }
            cv_text(bx, py + 42, &FONT_XS, "LIMIT", K_DIM);
            w = (int32_t)(fm_minf(lg / 12.0f, 1.0f) * (float)bw);
            cv_rect(bx, py + 56, bw, 8, K_LINE);
            if (w > 0)
                cv_rect(bx + bw - w, py + 56, w, 8, K_ACC);
            cv_text(bx, py + 68, &FONT_XS, "OUT", K_DIM);
            w = (int32_t)(fm_minf(pk, 1.0f) * (float)bw);
            cv_rect(bx, py + 80, bw, 6, K_LINE);
            if (w > 0)
                cv_rect(bx, py + 80, w, 6, pk > 0.9f ? K_ACC : K_PAD);
        }
        cv_text(6, SC_Y + SC_H + 6, &FONT_XS, "IN ACROSS, OUT UP: KNEE, CEILING", K_DIM);
        return;
    }
    if (kind == SCOPE_CMEL) {                    /* the counter-melody: its decay, and the line it plays over a bar (the chord's tones, low to high) */
        static const uint8_t RATE[4] = {4, 3, 2, 1};
        const int st = proj.par[P_CM_STYLE], ri = proj.par[P_CM_RATE] & 3, n = 6;
        const float T = 0.1f * fm_exp2f((float)proj.par[P_CM_DEC] * 0.045f);
        int s, k;
        col = th->skin ? IMP_TEAL : KC_COLOR;
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            sc_y[x] = (uint8_t)(scope_expo(u * 1.2f / T, 10.0f) * (float)(SC_H - 8) * 0.45f);
        }
        cv_text(6, 2, &FONT_B, "PLUCK", col);
        cv_text(6 + text_w(&FONT_B, "PLUCK") + 8, 6, &FONT_XS, EN_CM_STYLE[(unsigned)st < EN_NCMSTYLE ? st : 0], K_DIM);
        cv_text(6, SC_Y + SC_H + 6, &FONT_XS, "A BAR: THE CHORD'S TONES, AND THE RING", K_DIM);
        scope_trace(mix(K_BG, col, 8));
        for (s = 0, k = 0; s < 16; s += RATE[ri], k++) {
            int idx;
            const int per = 2 * n - 2, q = k % per;
            switch (st) {
            case 1: idx = n - 1 - k % n; break;
            case 2: idx = q < n ? q : per - q; break;
            case 3: idx = (int)(hash(31u, (uint32_t)k) >> 9) % n; break;
            case 4: idx = (2 * k) % n; break;
            case 5: idx = (3 * k) % n; break;
            default: idx = k % n; break;
            }
            cv_disc(SC_X + 6 + s * 13, SC_Y + SC_H - 8 - idx * (SC_H - 22) / (n - 1), 4, col);
        }
        return;
    }
    if (kind == SCOPE_PENV || kind == SCOPE_DENV || kind == SCOPE_AENV) {
        float seg[5];                            /* the curve's pieces as widths; their ends */
        if (kind == SCOPE_PENV) {                /* attack, hold, release, rest */
            const float a = (float)proj.par[P_PE_ATK] * 0.01f, h = (float)proj.par[P_PE_HOLD] * 0.01f,
                        r = (float)proj.par[P_PE_REL] * 0.01f, s = (float)proj.par[P_PE_REST] * 0.01f;
            float tot;
            seg[0] = 0.06f + 0.35f * a, seg[1] = 0.04f + 0.30f * h, seg[2] = 0.08f + 0.50f * r, seg[3] = 0.04f + 0.30f * s;
            tot = seg[0] + seg[1] + seg[2] + seg[3];
            seg[0] /= tot, seg[1] /= tot, seg[2] /= tot, seg[3] /= tot;
            title = "PAD ENVELOPE", sub = "ATTACK  HOLD  RELEASE  REST";
            for (x = 0; x < SC_W; x++) {
                float u = (float)x / (float)(SC_W - 1), y;
                if (u < seg[0]) {
                    const float t = u / seg[0];
                    y = 1.0f - (1.0f - t) * (1.0f - t);
                } else if (u < seg[0] + seg[1]) {
                    y = 1.0f;
                } else if (u < seg[0] + seg[1] + seg[2]) {
                    const float t = (u - seg[0] - seg[1]) / seg[2];
                    y = (1.0f - t) * (1.0f - t);
                } else {
                    y = 0.0f;
                }
                sc_y[x] = (uint8_t)(y * (float)(SC_H - 8));
            }
        } else if (kind == SCOPE_DENV) {         /* the drone's: attack then decay, as high as the amount */
            const float a = (float)proj.par[P_DE_ATK] * 0.01f, d = (float)proj.par[P_DE_DEC] * 0.01f,
                        m = 0.15f + 0.85f * (float)proj.par[P_DE_AMT] * 0.01f, wa = 0.05f + 0.30f * a, wd = 0.65f;
            col = K_BASS;
            title = "DRONE ENVELOPE", sub = "ATTACK  DECAY  AMOUNT";
            for (x = 0; x < SC_W; x++) {
                const float u = (float)x / (float)(SC_W - 1);
                float y;
                if (u < wa)
                    y = u / wa;
                else
                    y = scope_expo((u - wa) / wd, 1.0f + (1.0f - d) * 9.0f);
                sc_y[x] = (uint8_t)(y * m * (float)(SC_H - 8));
            }
        } else {                                 /* the acid's: a note, then an accented one (taller), each decaying */
            const float d = (float)proj.par[P_AC_DEC] * 0.01f, ac = (float)proj.par[P_AC_ACC] * 0.01f;
            col = K_ACID;
            title = "ACID ENVELOPE", sub = "DECAY  ACCENT  SLIDE";
            for (x = 0; x < SC_W; x++) {
                const float u = (float)x / (float)(SC_W - 1), lu = u < 0.5f ? u * 2.0f : u * 2.0f - 1.0f;
                const float amp = u < 0.5f ? 0.55f : 0.55f + 0.45f * ac;
                sc_y[x] = (uint8_t)(scope_expo(lu, 1.5f + (1.0f - d) * 9.0f) * amp * (float)(SC_H - 8));
            }
        }
        cv_text(6, 2, &FONT_B, title, col);
        cv_text(6, SC_Y + SC_H + 6, &FONT_XS, sub, K_DIM);
        scope_trace(col);
        scope_dot(tk, col);
        return;
    }
    if (kind == SCOPE_PLFO || kind == SCOPE_DLFO) {
        const int pl = kind == SCOPE_PLFO, shape = proj.par[pl ? P_PL_SHAPE : P_DL_SHAPE],
                  rate = proj.par[pl ? P_PL_RATE : P_DL_RATE] & 7, dep = proj.par[pl ? P_PL_DEPTH : P_DL_DEPTH];
        const float cyc = 8.0f * fm_exp2f(-(float)rate * 0.5f), amp = 0.06f + 0.94f * (float)dep * 0.01f;
        col = pl ? K_PAD : K_BASS;
        title = pl ? "PAD LFO" : "DRONE LFO";
        sub = LFO_SHAPE_NAME_UI[(unsigned)shape < 6u ? shape : 0];
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1), w = lfo_wave(shape, u * cyc + tk);
            sc_y[x] = (uint8_t)((0.5f + 0.5f * w * amp) * (float)(SC_H - 8));
        }
        cv_text(6, 2, &FONT_B, title, col);
        cv_text(6 + text_w(&FONT_B, title) + 8, 6, &FONT_XS, sub, K_DIM);
        scope_trace(col);
        cv_rect(SC_X, SC_Y + SC_H / 2, SC_W, 1, K_LINE);
        return;
    }
    if (kind == SCOPE_DFILT || kind == SCOPE_AFILT) {
        const int ac = kind == SCOPE_AFILT, cut = proj.par[ac ? P_AC_CUT : P_DR_CUT], res = proj.par[ac ? P_AC_RES : P_DR_RES],
                  env = ac ? proj.par[P_AC_ENV] : 0;
        int pass;
        col = ac ? K_ACID : K_BASS;
        title = ac ? "ACID FILTER" : "DRONE FILTER";
        sub = ac ? "CUTOFF  RESO  ENV MOD (DIM: PEAK)" : "CUTOFF  RESO";
        cv_text(6, 2, &FONT_B, title, col);
        cv_text(6, SC_Y + SC_H + 6, &FONT_XS, sub, K_DIM);
        for (pass = 0; pass < (ac && env > 4 ? 2 : 1); pass++) {
            const float oct = 8.4f * (float)cut * 0.01f + (pass ? 4.0f * (float)env * 0.01f : 0.0f), fc = 60.0f * fm_exp2f(oct),
                        q = 0.6f + (float)res * 0.09f, top = (q > 1.0f ? q : 1.0f) * 1.12f;
            for (x = 0; x < SC_W; x++) {
                const float f = 20.0f * fm_exp2f((float)x / (float)(SC_W - 1) * 10.0f), r = f / fc, a = 1.0f - r * r,
                            m = 1.0f / fm_sqrtf(a * a + r * r / (q * q) + 1e-6f);
                sc_y[x] = (uint8_t)(fm_clampf(m / top, 0.0f, 1.0f) * (float)(SC_H - 6));
            }
            scope_trace(pass == 0 && ac && env > 4 ? mix(K_BG, col, 7) : col);
        }
        return;
    }
    {   /* Space: the echo's taps and the reverb's tail */
        const float eb = ETIME_BEATS_UI[(unsigned)proj.par[P_ETIME] < 6u ? proj.par[P_ETIME] : 0], fb = 0.15f + 0.7f * (float)proj.par[P_ECHO] * 0.01f,
                    rv = (float)proj.par[P_REVERB] * 0.01f, len = 0.25f + 0.75f * (float)proj.par[P_SIZE] * 0.01f;
        const int32_t px = (int32_t)(eb * 34.0f);
        int k;
        col = KC_REPEAT;
        cv_text(6, 2, &FONT_B, "SPACE", col);
        cv_text(6, SC_Y + SC_H + 6, &FONT_XS, "THE HIT, ITS ECHOES, THE REVERB'S TAIL", K_DIM);
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            sc_y[x] = (uint8_t)(rv * 0.55f * scope_expo(u / len, 4.0f) * (float)(SC_H - 8));
        }
        scope_trace(KC_COLOR);
        for (k = 0; k < 7; k++) {
            const int32_t bx = SC_X + 4 + k * px, bh = (int32_t)((float)(SC_H - 8) * (k ? fm_powf(fb, (float)k) * 0.85f : 1.0f));
            if (bx > SC_X + SC_W - 3 || bh < 2)
                break;
            cv_rect(bx, SC_Y + SC_H - bh, 3, bh, k ? col : K_TEXT);
        }
    }
}

#include "viz.c"

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)proj.par[P_ROOT] << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if (ui.frame % 2u)                           /* 30 frames a second is plenty */
        return;
    h = hash(h, ui.theme);
    if (!mix_show() && shift_show()) {           /* SEL held: what the shift does */
        h = hash(0x53484654u, (uint32_t)ui.theme | (uint32_t)(ui.view == V_SEQ) << 8 | (uint32_t)(ui.fx_menu && !ui.fx_page) << 9);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_shift();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (mix_show()) {                            /* GLO held: the track mix */
        h = hash(hash(hash(0x4D495821u, (uint32_t)ui.trk_m | (uint32_t)ui.trk_s << 7 | (uint32_t)ui.theme << 14), plat_cpu_pct() | plat_xruns() << 8 | plat_core2() << 24 | plat_cpu1_pct() << 26 | (uint32_t)en_shed_level << 30),
                 (uint32_t)ui.scene_set | (uint32_t)ui.dj_on << 4 | (uint32_t)ui.dj_len << 27 | (uint32_t)ui.dj_piece << 7 |
                     (uint32_t)ui.dj_phase << 8 | (uint32_t)ui.dj_n << 12);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_mix();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.fx_menu) {
        h = hash(hash(0x46584D4Eu, h32(ui.fx_mask) ^ (uint32_t)ui.theme << 28 ^ (uint32_t)ui.lock_mode << 31), h32(ui.fx_lock));
        h = hash(h, (uint32_t)fx_bank << 29);
        if (fx_bank == 3)
            h = hash(h, ui.keys | (uint32_t)ui.fxs[EN_FXS_SM_MODE] << 28);
        h = hash(h, ui.fx_page | (uint32_t)ui.gx_pat << 4 | (uint32_t)ui.fxs[EN_FXS_GX_LEN] << 20 | (uint32_t)ui.gx_tgt << 25);
        if (ui.fx_page == 1)
            h = hash(h, (uint32_t)(gx_head() + 1));
        h = hash(h, ui.flex_last);
        if (ui.fx_page > 1) {                     /* a page's drawing follows its settings, and moves */
            int j;
            h = hash(h, scope_tick());
            for (j = 0; j < EN_NFXS; j++)
                h = hash(h, ui.fxs[j]);
        }
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            if (ui.fx_page)
                draw_fxedit();
            else
                draw_fx();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.help) {
        h = hash(0x48454C50u, ui.help | (uint32_t)ui.theme << 8);
        if (ui.help - 1 == B_GLO)                /* the load, as it changes */
            h = hash(h, plat_cpu_pct() | plat_xruns() << 8 | plat_core2() << 24 | plat_cpu1_pct() << 26);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_JAM) {                      /* the JAM page: the keyboard, its scale, what is held */
        h = hash(hash(0x4A414D31u, ui.keys | (uint32_t)ui.jam_voice << 27 | (uint32_t)ui.theme << 29), pool_abs() | (uint32_t)ui.jam_mode << 12 | (uint32_t)ui.jam_oct << 14 |
                                                                                                        (uint32_t)ui.jam_ans << 17 | (uint32_t)ui.ans_cnt << 18 | (uint32_t)ui.ans_playing << 22 | (uint32_t)ui.ans_i << 23);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_jam();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_VIZ) {                      /* the visualizers: fifteen frames a second */
        ui.sig[1] = 0;
        if (ui.frame % 4u == 0u)
            draw_viz();
        return;
    }
    if (ui.view == V_PERF) {
        h = hash(h, (uint32_t)ui.trk_m | (uint32_t)ui.trk_s << 7 | (uint32_t)ui.scene_set << 14 | (uint32_t)ui.dj_on << 18 |
                        (uint32_t)ui.dj_phase << 19 | (uint32_t)ui.dj_len << 27 | (uint32_t)en_sec << 23 | (uint32_t)en_sec_req << 25);
        h = hash(h, h32(ui.fx_mask) | ((uint32_t)ui.scene_q << 28 ^ (uint32_t)ui.mood << 8 ^ (uint32_t)ui.key_on << 16));
        h = hash(h, h32(ui.fx_lock) ^ (uint32_t)ui.dj_n << 20 ^ (uint32_t)en_drum_pos << 27);
        for (i = 0; i < EN_NLANE; i++)
            h = hash(h, (uint32_t)q16(en_drum_glow[i]));
        h = hash(h, (uint32_t)q16(en_pad_level) | (uint32_t)q16(en_bass_level) << 5 | (uint32_t)q16(en_acid_level) << 10 | (uint32_t)en_playing << 15 |
                        (uint32_t)q16(en_cm_level) << 16);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_perf();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (scope_kind(ui.view)) {                   /* a page that shapes a sound: its curve */
        h = hash(hash(0x53434F50u, ui.view | (uint32_t)ui.theme << 8), scope_tick());
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)knob_get(view_knob((int)i)) + 100u);
        h = hash(h, (uint32_t)proj.par[P_CM_DEC] | (uint32_t)proj.par[P_CM_STYLE] << 8 | (uint32_t)proj.par[P_CM_RATE] << 12 | (uint32_t)proj.par[P_CM_RANGE] << 16);
        h = hash(h, (uint32_t)proj.par[P_AC_ENV] | (uint32_t)proj.par[P_ECHO] << 8 | (uint32_t)proj.par[P_SIZE] << 16 |
                        (uint32_t)proj.par[P_PL_RATE] << 24);
        if (ui.view == V_GROOVE) {
            int l;
            for (l = 0; l < EN_NLANE; l++)
                h = hash(h, (uint32_t)proj.par[P_DENS + l] | (uint32_t)proj.par[P_LSRC + l] << 8 | (uint32_t)en_pats[en_pat_now][l] << 16 | (uint32_t)en_lane_pos[l] << 11);
            h = hash(h, (uint32_t)ui.lane | (uint32_t)en_pat_now << 4 | (uint32_t)en_playing << 8);
        }
        if (ui.view == V_MASTER)
            h = hash(h, (uint32_t)(en_comp_gr * 2.0f) | (uint32_t)(en_lim_gr * 2.0f) << 8 | (uint32_t)(en_out_peak * 40.0f) << 16);
        if (ui.view == V_MIXER) {
            int d;
            float dg = 0.0f;
            for (d = 0; d < EN_NLANE; d++)
                dg = fm_maxf(dg, en_drum_glow[d]);
            h = hash(h, (uint32_t)(en_pad_level * 20.0f) | (uint32_t)(en_bass_level * 20.0f) << 5 | (uint32_t)(en_acid_level * 20.0f) << 10 |
                            (uint32_t)(en_cm_level * 20.0f) << 15 | (uint32_t)(dg * 20.0f) << 20 | (uint32_t)(en_out_peak * 20.0f) << 25);
            h = hash(h, (uint32_t)ui.trk_m | (uint32_t)ui.trk_s << 8 | (uint32_t)proj.par[P_CM_LVL] << 16);
            h = hash(h, (uint32_t)ui.mix_trk | (uint32_t)proj.par[P_PAN] << 4 | (uint32_t)proj.par[P_PAN1] << 11 | (uint32_t)proj.par[P_PAN2] << 18 | (uint32_t)proj.par[P_PAN3] << 25);
            h = hash(h, (uint32_t)proj.par[P_PAN4]);
        }
        h = hash(h, (uint32_t)(proj.par[P_PL_DEPTH] | proj.par[P_DL_SHAPE] << 8 | proj.par[P_DL_RATE] << 12 | proj.par[P_DL_DEPTH] << 16));
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_scope(scope_kind(ui.view));
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    for (i = 0; i < EN_NLANE; i++)
        h = hash(h, en_pats[ui.view == V_SEQ ? seq_pat() : en_pat_now][i] | (uint32_t)q16(en_drum_glow[i]) << 16);
    h = hash(h, en_drum_pos | (uint32_t)en_pat_now << 8 | (uint32_t)en_pat_next << 12 | (uint32_t)en_pulse_rest << 16 |
                    (uint32_t)ui.view << 20);
    if (ui.view == V_SEQ) {
        h = hash(h, ui.lane | (uint32_t)proj.par[P_PATTERN] << 16);
        for (i = 0; i < EN_NLANE; i++)
            h = hash(h, (uint32_t)proj.par[P_PROB + i] | (uint32_t)proj.par[P_LLEN + i] << 8 | (uint32_t)proj.par[P_LSRC + i] << 16);
        if (h == ui.sig[1])
            return;
        ui.sig[1] = h;
        cv_begin(240, MAIN_H, K_BG);
        draw_seq();
        cv_blit(0, MAIN_Y);
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    h = hash(h, en_acid_gate | (uint32_t)en_acid_acc << 16);
    h = hash(h, en_acid_pos | (uint32_t)en_acid_len << 8 | (uint32_t)q16(en_acid_glow) << 16);
    h = hash(h, (uint32_t)q16(en_cm_level));
    h = hash(h, (uint32_t)(en_drift * 40.0f) | (uint32_t)q16(en_pad_level) << 8 | (uint32_t)q16(en_bass_level) << 13 |
                    (uint32_t)q16(en_acid_level) << 18);
    for (i = 0; i < 5; i++)
        h = hash(h, (uint32_t)proj.par[i]);
    h = hash(h, (en_playing ? plat_ms() / 66u : 0u) | (uint32_t)en_sec << 28 | (uint32_t)ui.centre_imp << 30 | (uint32_t)en_shed_level << 26);   /* the sigil moves: 15 frames a second while playing */
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (en_frozen) {                             /* the way to the next chord, top right (held: frozen) */
        cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
    } else {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_rect(232 - w, 8, w, 4, K_LINE);
        if (f > 0)
            cv_rect(232 - w, 8, f, 4, K_PAD);
    }
    draw_sigil();
    draw_meters();
    draw_kit();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and a dot a page at its right end; under it a
 * cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The knob being
 * turned is drawn on a tint. Only redrawn when what it shows changes. */
static uint16_t view_color(void)                /* the page's voice in its colour: the pad, the drone, the acid, the drums */
{
    const int g = group_of(ui.view == V_SEQ ? V_DRUMS : ui.view);
    switch (g >= 0 ? GROUP[g].btn : -1) {
    case B_LFO: return K_BASS;
    case B_ARP: return ui.view == V_CMEL || ui.view == V_CMEL2 ? (th->skin ? IMP_TEAL : KC_COLOR) : K_ACID;
    case B_SEQ: return th->skin ? K_PULSE : RGB(255, 160, 60);
    case B_HOME: return KC_REPEAT;
    default: return ui.view == V_JAM ? KC_MOVE : perf_like() ? KC_FILTER : K_PAD;
    }
}

/* the accordion tabs (the Imp skin's knob strip): a chip a group of pages; the one you are in opens to its name and a pip a page */
static void imp_tabs(void)
{
    static const struct { const char *name; char ltr; uint8_t first, n; } TB[10] = {
        {"TEMPO", 'T', V_TEMPO, 1}, {"PAD", 'P', V_PAD, 4}, {"DRONE", 'D', V_DRONE, 3}, {"ACID", 'A', V_ACID, 3}, {"PLUCK", 'L', V_CMEL, 2},
        {"DRUMS", 'R', V_DRUMS, 5}, {"SPACE", 'S', V_SPACE, 2}, {"MIX", 'M', V_MASTER, 2}, {"GLOBAL", 'G', V_GLOBAL, 1}, {"JAM", 'J', V_JAM, 1}};
    const uint16_t col[10] = {K_TEXT, K_PAD, K_BASS, K_ACID, IMP_TEAL, K_PULSE, K_FROST, K_DIM, K_TEXT, KC_MOVE};
    const int v = ui.view == V_SEQ ? V_DRUMS : ui.view >= V_PERF ? V_TEMPO : (int)ui.view;
    int32_t x = 3;
    int i;
    for (i = 0; i < 10; i++) {
        const int on = v >= TB[i].first && v < TB[i].first + TB[i].n;
        if (on) {
            const int32_t w = text_w(&FONT_XS, TB[i].name) + 9 + (TB[i].n > 1 ? TB[i].n * 5 + 1 : 0);
            int p;
            char l[2] = {0, 0};
            cv_round(x, 12, w, 12, 2, col[i]);
            cv_text(x + 4, 13, &FONT_XS, TB[i].name, K_BG);
            for (p = 0; TB[i].n > 1 && p < TB[i].n; p++)
                cv_rect(x + w - 3 - (TB[i].n - p) * 5 + 1, 16, 3, 5, p == v - TB[i].first ? K_BG : mix(col[i], K_BG, 9));
            (void)l;
            x += w + 3;
        } else {
            char l[2] = {TB[i].ltr, 0};
            cv_rect(x, 12, 14, 12, K_PANEL);
            cv_text(x + 4, 13, &FONT_XS, l, K_DIM);
            cv_rect(x, 22, 14, 2, col[i]);
            x += 17;
        }
    }
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | (uint32_t)ui.lane << 16 | (uint32_t)ui.theme << 24), i;
    h = hash(h, (uint32_t)proj.par[P_PADTONE] | (uint32_t)proj.par[P_DR_INST] << 3 | (uint32_t)proj.par[P_AC_WAVE] << 6 |
                (uint32_t)(proj.par[P_KIT_KICK] | proj.par[P_KIT_SNARE] << 3 | proj.par[P_KIT_HAT] << 6) << 9 |
                (uint32_t)proj.par[P_KIT_PERC] << 18);
    h = hash(h, (uint32_t)(proj.par[P_PAD_MAN] | proj.par[P_DR_MAN] << 1 | proj.par[P_AC_MAN] << 2) << 24 |
                (uint32_t)proj.par[P_LINKS] << 8 |
                    (uint32_t)(proj.par[oct_par(ui.view)] + 8) << 12);
    if (ui.view == V_GLOBAL)
        h = hash(h, (uint32_t)proj.piece | (uint32_t)(proj.edited >> proj.piece & 1u) << 8 |
                        (uint32_t)project_saved(proj.piece) << 9);
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(view_knob((int)i)) + 100u);
    h = hash(h, (uint32_t)now_touch | (uint32_t)ui.fx_menu << 1 | (uint32_t)ui.fx_page << 2 | (uint32_t)fx_bank << 10);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[24];
        int k;
        const int ndots = ui.fx_menu ? (ui.fx_page ? NFXP - 1 : 4) : NDIAL;   /* the dots: the pages; in the FX menu the banks, in an effect's settings the edit pages */
        int32_t xd = 237 - ndots * 5, xt;
        if (ui.view == V_SEQ) {
            char u[24] = "SEQ: ";
            caps(u + 5, EN_LANE_NAME[ui.lane], (int)sizeof u - 5);
            caps(t, u, sizeof t);
        } else if (ui.fx_menu) {
            caps(t, ui.fx_page ? FXP[ui.fx_page - 1].name : "FX", sizeof t);
        } else {
            caps(t, VIEW_NAME[ui.view], sizeof t);
        }
        int32_t xl = 0;                          /* where the left text ends: the page's name only if it clears it */
        xt = xd - 5 - text_w(&FONT_XS, t);
        if (ui.view == V_GLOBAL) {               /* Global: the piece's state, and what SAVE does */
            char u[24];
            int32_t xs;
            caps(u, piece_name(proj.piece), 4);
            if (proj.edited >> proj.piece & 1u) {
                const char *q = " SAVE: STORE";
                int j = 0;
                while (u[j])                     /* (after the tag, "01" or "L03") */
                    j++;
                while (*q && j < 23)
                    u[j++] = *q++;
                u[j] = 0;
            } else {
                const char *q = project_saved(proj.piece) ? " STORED" : " FACTORY";
                int j = 0;
                while (u[j])
                    j++;
                while (*q && j < 23)
                    u[j++] = *q++;
                u[j] = 0;
            }
            xs = cv_text(4, -1, &FONT_XS, u, proj.edited >> proj.piece & 1u ? K_TEXT : K_DIM);
            if (xt - 9 > xs + 4)
                cv_rect(xs + 4, 5, xt - 9 - xs - 4, 1, K_LINE);
            xl = xs;
        } else {                                 /* the voice's sound (the drums: the kit), then the rule */
            char u[16], t[12];
            int32_t xs;
            int vc = voice_of(ui.view);
            if (vc >= 0) {
                en_param_text(SOUND_PAR[vc], proj.par[SOUND_PAR[vc]], t);
                caps(u, t, sizeof u);
            } else if (ui.view == V_CMEL || ui.view == V_CMEL2) {
                caps(u, EN_CM_SOUND[(unsigned)proj.par[P_CM_SND] < EN_NCMSND ? proj.par[P_CM_SND] : 0], sizeof u);
            } else if (kit_view(ui.view)) {
                int k = proj.par[P_KIT_KICK];
                int same = proj.par[P_KIT_SNARE] == k && proj.par[P_KIT_HAT] == k && proj.par[P_KIT_PERC] == k;
                caps(u, same ? EN_KIT_NAME[(unsigned)k < EN_NKIT ? k : 0] : "Mixed kit", sizeof u);
            } else {
                caps(u, "PERF-1", sizeof u);
            }
            xs = cv_text(4, -1, &FONT_XS, u, K_DIM);
            if (by_hand(vc))                     /* the voice off the algorithm: said beside its sound */
                xs = cv_text(xs + 5, -1, &FONT_XS, "BY HAND", K_ACC);
            else if (proj.par[P_LINKS])          /* the links on: said beside it */
                xs = cv_text(xs + 5, -1, &FONT_XS, "+LINK", K_ACC);
            else if (ui.view == V_LINKS)
                xs = cv_text(xs + 5, -1, &FONT_XS, "LINKS OFF", K_DIM);
            {   /* the octave OCT moves here, if it isn't home: "OCT+1" (the song's, off a voice: "SONG-2") */
                int op = oct_par(ui.view), ov = proj.par[op];
                if (ov) {
                    char o[10];
                    const char *pre = op == P_OCT ? "SONG" : "OCT";
                    int j = 0;
                    while (*pre)
                        o[j++] = *pre++;
                    o[j++] = ov > 0 ? '+' : '-';
                    o[j++] = (char)('0' + (ov > 0 ? ov : -ov));
                    o[j] = 0;
                    if (xs + 5 + text_w(&FONT_XS, o) < xt - 8)
                        xs = cv_text(xs + 5, -1, &FONT_XS, o, K_TEXT);
                }
            }
            if (xt - 9 > xs + 4)
                cv_rect(xs + 4, 5, xt - 9 - xs - 4, 1, K_LINE);
            xl = xs;
        }
        if (xt >= xl + 6)                        /* (a long sound name, BY HAND or the store's state would run into it) */
            cv_text(xt, -1, &FONT_XS, t, K_ACC);
        if (th->skin && !ui.fx_menu)
            imp_tabs();                          /* (the Imp skin: tabs, a chip a group) */
        else for (k = 0; k < ndots; k++) {
            int cur, same;
            if (ui.fx_menu) {
                cur = k == (ui.fx_page ? fx_pos(ui.fx_page) : (int)fx_bank);
                same = 0;
            } else {
                cur = k == ui.view || (ui.view == V_SEQ && k == V_DRUMS);
                same = group_of(k) >= 0 && group_of(k) == group_of(ui.view == V_SEQ ? V_DRUMS : ui.view);
            }
            cv_rect(xd + k * 5, th->skin ? 17 : 3, 3, 4, cur ? K_ACC : same ? K_DIM : K_LINE);
        }
    }
    for (i = 0; i < 4; i++) {
        int k = view_knob((int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : view_color();
        if (k == K_NONE)
            continue;
        if (th->skin) {                          /* the Imp skin: the label, the value large, a bar under it */
            char vt[16];
            int32_t bw = 0;
            knob_range(k, &lo, &hi);
            v = knob_get(k);
            if (i)
                cv_rect(x, 27, 1, 40, K_LINE);
            if (hot)
                cv_round(x + 3, 25, 54, 43, 3, K_PANEL);
            caps(t, view_label((int)i), sizeof t);
            text_c(cx, 26, &FONT_XS, t, hot ? K_TEXT : K_DIM);
            knob_text(k, t);
            caps(vt, t, sizeof vt);
            if (text_w(&FONT_M, vt) <= 56)
                text_c(cx, 34, &FONT_M, vt, hot ? K_ACC : K_TEXT);
            else
                text_c(cx, 39, &FONT_B, vt, hot ? K_ACC : K_TEXT);
            cv_rect(cx - 24, 63, 48, 3, hot ? K_BG : K_LINE);
            if (hi > lo) {
                if (lo < 0) {                    /* bipolar: from the middle */
                    const int32_t m = 24 + (int32_t)(v - lo) * 48 / (hi - lo) - 24;
                    cv_rect(cx - 24 + (m < 24 ? m : 24), 63, m < 24 ? 24 - m : m - 24, 3, fill);
                } else {
                    bw = (int32_t)(v - lo) * 48 / (hi - lo);
                    cv_rect(cx - 24, 63, bw, 3, fill);
                }
            }
            continue;
        }
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 3, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, view_label((int)i), sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (hi - lo < 6) {                       /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_rect(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, j == v - lo ? fill : K_LINE);
        } else {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    int g = group_of(ui.view == V_SEQ ? V_DRUMS : ui.view);
    if (g >= 0)
        b |= 1u << GROUP[g].btn;
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (ui.fx_lock && !ui.fx_menu && (ui.frame / 16u) & 1u)
        b |= 1u << B_FX;                         /* effects locked on, the menu closed: FX blinks */
    if (en_sec != EN_SEC_GROOVE || (en_sec_req && (ui.frame / 8u) & 1u))   /* a section; blinking: on its way */
        b |= 1u << B_SAVE;
    if (ui.btn >> B_SEL & 1u) {                  /* shift: the pattern chosen; the one playing blinks if not */
        b |= 1u << B_SEL;
        if (ui.fx_menu)
            b |= 1u << B_FX;
        k |= 1u << WHITE_KEY[proj.par[P_PATTERN] % EN_NPAT];
        if (en_pat_now != proj.par[P_PATTERN] && (ui.frame / 8u) & 1u)
            k |= 1u << WHITE_KEY[en_pat_now % EN_NPAT];
        if (ui.view == V_SEQ)                    /* and the copy's targets */
            for (i = 0; i < EN_NPAT; i++)
                if ((int)i != seq_pat())
                    k |= 1u << BLACK_KEY[i];
    } else if (ui.view == V_VIZ && ui.lights && !mix_show() && !ui.fx_menu) {   /* the light show (STAGE: Lights) */
        const int st = en_playing ? (int)en_drum_pos : (int)((ui.frame / 6u) & 15u);
        if (en_sec == EN_SEC_BREAK) {            /* a breakdown breathes: the white keys and the black ones in turn */
            const uint32_t ph = (ui.frame / 24u) & 1u;
            for (i = 0; i < 16; i++)
                if ((i & 1u) == ph)
                    k |= 1u << WHITE_KEY[i];
            for (i = 0; i < 11; i++)
                if ((i & 1u) != ph)
                    k |= 1u << BLACK_KEY[i];
        } else if (en_sec == EN_SEC_BUILD) {     /* a build fills the white keys from the left, over its bars */
            const uint32_t n = 1u + (en_bars % 64u) / 4u;
            for (i = 0; i < 16 && i < n; i++)
                k |= 1u << WHITE_KEY[i];
        } else {                                 /* a groove: two chasers; the black keys flash with the kick and the snare */
            k |= 1u << WHITE_KEY[st] | 1u << WHITE_KEY[(st + 8) & 15];
            if (en_drum_glow[0] > 0.5f)
                for (i = 0; i < 11; i += 2)
                    k |= 1u << BLACK_KEY[i];
            if (en_drum_glow[1] > 0.5f)
                for (i = 1; i < 11; i += 2)
                    k |= 1u << BLACK_KEY[i];
        }
    } else if (mix_show() || (perf_like() && !ui.fx_menu)) {   /* GLO held, or the perform screen: the keys muted and soloed */
        for (i = 0; i < NKEYS; i++)
            if (mix_state((int)i))
                k |= 1u << i;
    } else if (ui.fx_menu && ui.fx_page == 1) {  /* the gate's page: the steps open, the one playing blinking */
        const int len = ui.fxs[EN_FXS_GX_LEN], head = gx_head();
        b |= 1u << B_FX;
        for (i = 0; i < 16; i++)
            if (((int)i < len && (ui.gx_pat >> i & 1u)) ^ (int)((int)i == head))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.fx_menu && ui.fx_page) {       /* another effect's page: FX lit */
        b |= 1u << B_FX;
    } else if (ui.fx_menu && fx_bank == 3) {     /* the sampler: the keys held */
        b |= 1u << B_FX;
        k |= ui.keys;
    } else if (ui.fx_menu) {                     /* FX: lit; the keys held */
        b |= 1u << B_FX;
        for (i = 0; i < NKEYS; i++)
            if (ui.fx_mask >> KEY_FX[fx_bank][i] & 1u)
                k |= 1u << i;
    } else if (ui.view == V_SEQ) {               /* the sequencer: the lane's hits on the white keys, the playhead */
        uint16_t bits = en_pats[seq_pat()][ui.lane];
        for (i = 0; i < 16; i++)
            if ((bits >> i & 1u) ^ (uint32_t)(en_playing && i == en_drum_pos))
                k |= 1u << WHITE_KEY[i];
    } else {
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
        if (ui.view == V_JAM)
            k |= ui.keys;                        /* the JAM page: the scale, and the keys held */
    }
    plat_glow(1);
    plat_leds(b, k);
}

/* the FX knobs glide to a scene's values (a second or so) instead of jumping; a knob turned by hand takes its own back */
static void glide_tick(void)
{
    int k;
    if (!ui.glide_on)
        return;
    ui.glide_busy = 1;
    for (k = 0; k < 4; k++)
        if (ui.glide_on >> k & 1u) {
            float d = (float)ui.glide_t[k] - ui.glide_f[k];
            if (d > -0.6f && d < 0.6f) {
                knob_set(K_FXF + k, ui.glide_t[k]);
                ui.glide_on &= (uint8_t)~(1u << k);
            } else {
                ui.glide_f[k] += d * 0.06f;
                knob_set(K_FXF + k, (int)(ui.glide_f[k] + (ui.glide_f[k] < 0.0f ? -0.5f : 0.5f)));
            }
        }
    ui.glide_busy = 0;
}


/* THE JAM PAGE (the SELECT dial, after Global): the keys are an instrument. VOICE: the pluck, the pad, the acid, the drone. KEYS: SCALE (every key a
 * note of the scale: a black key is the scale tone at or below it, so there are no wrong notes), CHORDS (every key a triad of the scale), HARP (the
 * keys are the scale's tones, one a key, up from the root). OCTAVE. ANSWER: when you stop (after at least two notes), the pluck plays your phrase
 * back an octave up on the next bar, once, and waits for the next. The scale is still the page's own: this page never edits it. */
static const char *const JV_NAME[4] = {"PLUCK", "PAD", "ACID", "DRONE"};
static const char *const JM_NAME[3] = {"SCALE", "CHORDS", "HARP"};
static const char *const JO_NAME[5] = {"-2", "-1", "0", "+1", "+2"};
static int jam_snap(int m, uint32_t pool)       /* the note, or the scale tone below it */
{
    int d;
    for (d = 0; d < 12; d++)
        if (pool >> (((m - d) % 12 + 12) % 12) & 1u)
            return m - d;
    return m;
}
static int jam_notes(int i, uint8_t *out)       /* the notes of key i: 1 (scale, harp) or 3 (chords) */
{
    const uint32_t pool = pool_abs();
    const int oct = 12 * ((int)ui.jam_oct - 2), base = 53 + i + oct;
    int m, r, n = 0, cnt = 0, t[5];
    if (ui.jam_mode == 2) {                      /* harp: the i-th tone of the scale up from the root, from C3 */
        for (m = 48 + proj.par[P_ROOT]; m < 128; m++)
            if (pool >> (m % 12) & 1u) {
                if (cnt == i) {
                    r = m + oct;
                    out[0] = (uint8_t)(r < 0 ? 0 : r > 127 ? 127 : r);
                    return 1;
                }
                cnt++;
            }
        out[0] = 60;
        return 1;
    }
    r = jam_snap(base, pool);
    if (r < 0)
        r = 0;
    out[n++] = (uint8_t)(r > 127 ? 127 : r);
    if (ui.jam_mode == 1) {                      /* a triad of the scale: its third and fifth (the 2nd and 4th tones above) */
        for (m = r + 1, cnt = 0; m < r + 25 && cnt < 5; m++)
            if (pool >> (m % 12) & 1u)
                t[cnt++] = m;
        if (cnt >= 4) {
            out[n++] = (uint8_t)(t[1] > 127 ? 127 : t[1]);
            out[n++] = (uint8_t)(t[3] > 127 ? 127 : t[3]);
        }
    }
    return n;
}
static void jam_rec(int note)                    /* the ANSWER's memory: the notes played, and the sixteenths between them */
{
    const uint32_t now = en_playing ? en_bars : 0u;
    if (!ui.jam_ans || ui.ans_playing)
        return;
    if (ui.ans_cnt == 0 || now - ui.ans_last > 32u || ui.ans_cnt >= 8) {
        ui.ans_cnt = 0;
        ui.ans_dt[0] = 0;
    } else {
        ui.ans_dt[ui.ans_cnt] = (uint8_t)(now - ui.ans_last > 16u ? 16u : now - ui.ans_last);
    }
    ui.ans_note[ui.ans_cnt++] = (uint8_t)note;
    ui.ans_last = now;
}
static void jam_down(int i)
{
    static const uint8_t VC[4] = {EN_V_PLUCK, EN_V_PAD, EN_V_ACID, EN_V_DRONE};
    static const int8_t OFS[4] = {0, 0, 12, 24};   /* the engine plays the acid an octave and the drone two below the key */
    uint8_t nn[4];
    const int n = jam_notes(i, nn), vc = ui.jam_voice & 3;
    int k;
    if (vc == 0 && proj.par[P_CM_LVL] < 15)
        knob_set(P_CM_LVL, 40);                  /* the pluck has to be audible to be played */
    ui.jam_n[i] = (uint8_t)n;
    ui.jam_vc[i] = VC[vc];
    for (k = 0; k < n; k++) {
        const int key = nn[k] + OFS[vc];
        ui.jam_note[i][k] = (uint8_t)(key > 127 ? 127 : key);
        en_note(VC[vc], ui.jam_note[i][k], 1);
    }
    jam_rec(nn[0]);
}
static void jam_up(int i)
{
    int k;
    for (k = 0; k < ui.jam_n[i]; k++)
        en_note(ui.jam_vc[i], ui.jam_note[i][k], 0);
    ui.jam_n[i] = 0;
}
static void jam_tick(void)                       /* the answer, on the sixteenths */
{
    static uint32_t seen;
    int k, held = 0;
    if (en_bars == seen)
        return;
    seen = en_bars;
    if (!ui.jam_ans || !en_playing) {
        ui.ans_playing = 0;
        return;
    }
    for (k = 0; k < NKEYS; k++)
        held |= ui.jam_n[k];
    if (!ui.ans_playing) {
        if (ui.ans_cnt >= 2 && !held && en_bars - ui.ans_last >= 12u && (en_bars & 15u) == 0u) {   /* quiet for a while, a bar line: answer */
            ui.ans_playing = 1;
            ui.ans_i = 0;
            ui.ans_t = 0;
            if (proj.par[P_CM_LVL] < 15)
                knob_set(P_CM_LVL, 40);
        } else {
            return;
        }
    }
    while (ui.ans_i < ui.ans_cnt) {              /* each note at its time */
        uint32_t cum = 0;
        for (k = 0; k <= ui.ans_i; k++)
            cum += ui.ans_dt[k];
        if (ui.ans_t < cum)
            break;
        en_note(EN_V_PLUCK, ui.ans_note[ui.ans_i] + 12, 1);
        ui.ans_i++;
    }
    ui.ans_t++;
    if (ui.ans_i >= ui.ans_cnt) {                /* answered: the memory empties */
        ui.ans_playing = 0;
        ui.ans_cnt = 0;
    }
}
static void draw_jam(void)
{
    const uint32_t pool = pool_abs();
    int w, j;
    char t[16];
    cv_text(6, 2, &FONT_B, JV_NAME[ui.jam_voice & 3], KC_MOVE);
    cv_text(6 + text_w(&FONT_B, JV_NAME[ui.jam_voice & 3]) + 8, 6, &FONT_XS, JM_NAME[ui.jam_mode % 3], K_DIM);
    for (w = 0; w < 16; w++) {                   /* the keyboard: the white keys, then the black keys over them; the scale lit, the keys held bright */
        const int idx = WHITE_KEY[w], pc = (5 + idx) % 12, held = (int)(ui.keys >> idx & 1u), in = (int)(pool >> pc & 1u);
        cv_rect(8 + w * 14, 30, 13, 66, held ? KC_MOVE : in ? mix(K_PANEL, K_PAD, 9) : K_PANEL);
        if (idx == 0 || pc == 0)
            cv_rect(8 + w * 14 + 4, 90, 5, 4, K_TEXT);   /* a C marked */
    }
    for (j = 0; j < 11; j++) {
        const int idx = BLACK_KEY[j], pc = (5 + idx) % 12, held = (int)(ui.keys >> idx & 1u), in = (int)(pool >> pc & 1u);
        int lw = 0;
        for (w = 0; w < 16; w++)
            if (WHITE_KEY[w] == idx - 1)
                lw = w;
        cv_rect(8 + lw * 14 + 9, 30, 10, 42, held ? KC_MOVE : in ? K_PAD : K_BG);
        cv_frame(8 + lw * 14 + 9, 30, 10, 42, K_LINE);
    }
    cv_text(6, 104, &FONT_XS, "OCTAVE", K_DIM);
    cv_text(52, 104, &FONT_XS, JO_NAME[ui.jam_oct % 5], K_TEXT);
    if (ui.jam_ans) {                            /* the answer: what it has heard (a dot a note), and whether it is playing it */
        cv_text(100, 104, &FONT_XS, ui.ans_playing ? "ANSWERING" : ui.ans_cnt ? "LISTENING" : "ANSWER ON", ui.ans_playing ? K_ACC : K_TEXT);
        for (j = 0; j < ui.ans_cnt && j < 8; j++)
            cv_disc(176 + j * 7, 108, 2, ui.ans_playing && j < ui.ans_i ? K_ACC : KC_MOVE);
    }
    itoa_u((uint32_t)(proj.par[P_CM_LVL]), t);
    cv_text(6, 124, &FONT_XS, "THE KEYS PLAY ONLY THE SCALE'S NOTES", K_DIM);
}

/* the screensaver (STAGE page): left alone while playing, the visualizers take the screen (the auto one); any key, button or dial turn brings the
 * page back (that touch is a touch as well) */
static void saver_tick(void)
{
    static const uint32_t MS[4] = {0u, 60000u, 180000u, 600000u};
    const uint32_t now = plat_ms(), last = ui.act_t > ui.turn_t ? ui.act_t : ui.turn_t;
    if (ui.saver_on) {
        if (last > ui.saver_t || !en_playing) {
            ui.saver_on = 0;
            set_view(ui.saver_view);
            ui.viz = ui.saver_viz;
            ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;   /* the page, its header and knobs, redrawn */
            lcd_fill(0, 0, 240, 240, K_BG);
        }
        return;
    }
    if (ui.saver && ui.saver < 4 && en_playing && ui.view != V_VIZ && !ui.fx_menu && !ui.help && !ui.btn && !ui.keys && now - last >= MS[ui.saver]) {
        ui.saver_on = 1;
        ui.saver_view = ui.view;
        ui.saver_viz = ui.viz;
        ui.saver_t = now;
        ui.viz = VZ_AUTO;
        set_view(V_VIZ);
        lcd_fill(0, 0, 240, 240, K_BG);          /* the whole screen is the picture's: the header and the knobs go */
    }
}

/* THE SETTINGS' MEMORY: the effects' settings (the FX edit pages), the performing settings (scenes, the DJ, the key change, the saver, the lights,
 * JAM) and the visualizer chosen are kept in flash (OBJ_SET) a moment after you stop changing them, and come back at power-on. A genre sets its own
 * tape, vinyl and pump settings when you switch to it (genre_fx): those are kept as they were saved while you are in another genre, so what you
 * set in HYPNO is what comes back. Not stored: what is latched on, the FX knobs, the mood. */
#define SET_MAGIC 0x31544553u                    /* "SET1" */
typedef struct {
    uint32_t magic;
    uint16_t nfxs, nui;
    uint8_t fxs[EN_NFXS];
    uint8_t uiv[12];
    uint8_t gx_preset, gx_tgt, pm_tgt, viz;
    uint16_t gx_pat, rsv;
} set_t;
typedef char set_fits[sizeof(set_t) <= PLAT_STORE_MAX ? 1 : -1];
static set_t set_disk;                           /* as it was last read or written */
static int set_have;
/* the player's DX7 voices: the engine's slots, 128 bytes each (en_user_get), in flash (OBJ_VOICES) */
#define VOX_MAGIC 0x31584F56u                    /* "VOX1" */
typedef struct { uint32_t magic, n; uint8_t v[EN_NUSER][128]; } vox_t;
typedef char vox_fits[sizeof(vox_t) <= PLAT_STORE_MAX ? 1 : -1];
static void voices_save(void)
{
    vox_t x;
    int i, n = en_user_count();
    memset(&x, 0, sizeof x);
    x.magic = VOX_MAGIC;
    x.n = (uint32_t)n;
    for (i = 0; i < n; i++)
        memcpy(x.v[i], en_user_get(i), 128);
    plat_store_save(OBJ_VOICES, &x, sizeof x);
}
static void voices_load(void)
{
    static vox_t x;
    uint32_t i;
    if (plat_store_load(OBJ_VOICES, &x, sizeof x) != (int)sizeof x || x.magic != VOX_MAGIC || x.n > EN_NUSER)
        return;
    for (i = 0; i < x.n; i++)
        en_user_voice((int)i, x.v[i]);
}
void ui_param_set(int p, int v)               /* the editor (main_fm1.c px_service): a parameter as its knob sets it */
{
    if ((unsigned)p < (unsigned)P_NPARAMS) {
        knob_set(p, v);
        ui.act_t = plat_ms();
    }
}
int ui_param_get(int p) { return (unsigned)p < (unsigned)P_NPARAMS ? proj.par[p] : 0; }
void ui_voices_changed(int n, int kind)
{
    char t[16] = "";
    if (n < 0) {
        say("DX7 VOICES", n == -1 ? "BAD CHECKSUM" : "NOT TAKEN");
        return;
    }
    ui.vox_dirty = 1;
    if (kind == 4 || (kind == 3 && n == 0)) {
        say("DX7 VOICES", "CLEARED");
        return;
    }
    itoa_u((uint32_t)n, t);
    {
        int j = 0;
        const char *s = kind == 1 ? " LOADED" : kind == 2 ? " ADDED" : " SET";
        while (t[j])
            j++;
        while (*s && j < 15)
            t[j++] = *s++;
        t[j] = 0;
    }
    say("DX7 VOICES", t);
}
static void settings_save(void)
{
    set_t s;
    int i;
    memset(&s, 0, sizeof s);
    s.magic = SET_MAGIC;
    s.nfxs = EN_NFXS;
    s.nui = 12;
    for (i = 0; i < EN_NFXS; i++)
        s.fxs[i] = ui.fxs[i];
    if (proj.genre && set_have)                  /* in another genre: its own tape / vinyl / pump settings are not the player's */
        for (i = 0; i < 12; i++)
            s.fxs[GENRE_FXS[i]] = set_disk.fxs[GENRE_FXS[i]];
    for (i = 0; i < 11; i++)
        s.uiv[i] = *uiv(i);
    s.gx_preset = ui.gx_preset;
    s.gx_tgt = ui.gx_tgt;
    s.pm_tgt = ui.pm_tgt;
    s.gx_pat = ui.gx_pat;
    s.viz = (uint8_t)(ui.viz | ui.centre_imp << 7);
    if (plat_store_save(OBJ_SET, &s, sizeof s) == 0) {
        set_disk = s;
        set_have = 1;
    }
}
static void settings_load(void)
{
    set_t s;
    int p, c;
    if (plat_store_load(OBJ_SET, &s, sizeof s) != (int)sizeof s || s.magic != SET_MAGIC || s.nfxs != EN_NFXS || s.nui != 12)
        return;
    set_disk = s;
    set_have = 1;
    for (p = 0; p < NFXP; p++)
        for (c = 0; c < 4; c++) {
            const int k = K_FXP + 4 * p + c, kind = FXP[p].c[c].kind;
            if (kind == KN_NONE || kind == KN_PRESET)
                continue;
            if (FXP[p].bit == 63) {              /* the UI's own (not the genre: 11) */
                if (FXP[p].c[c].idx < 11)
                    fxp_set(k, s.uiv[FXP[p].c[c].idx]);
            } else if (kind == KN_TGT) {
                fxp_set(k, s.gx_tgt);
            } else if (kind == KN_PTGT) {
                fxp_set(k, s.pm_tgt);
            } else {
                fxp_set(k, s.fxs[FXP[p].c[c].idx]);
            }
        }
    if (s.gx_preset < NGXP) {                    /* the gate's pattern, after its length (which says "the keys' own") */
        ui.gx_preset = s.gx_preset;
        ui.gx_pat = s.gx_pat;
        en_gate_pat(ui.gx_pat);
    }
    ui.viz = (uint8_t)((s.viz & 0x7Fu) % VZ_N);
    ui.centre_imp = (uint8_t)(s.viz >> 7);
    ui.set_dirty = 0;
}

/* what's left waiting for flash (a store is written at once, store_now): stopped, all of it after a
 * quiet moment; playing, the player's three (theme, Tune, MIDI out), with a moment's gap */
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (ui.vox_dirty && !ui.btn && !ui.keys && !ui.store_wait && now - ui.act_t >= (en_playing ? 2 * GLOB_QUIET : GLOB_QUIET)) {
        ui.vox_dirty = 0;
        quiet_write(3);                          /* the player's DX7 voices: kept (playing: faded) */
        return;
    }
    if (ui.set_dirty && !ui.btn && !ui.keys && !ui.store_wait && now - ui.act_t >= (en_playing ? 2 * GLOB_QUIET : GLOB_QUIET)) {   /* (playing: longer, each write is a dip) */
        ui.set_dirty = 0;
        quiet_write(1);                          /* the effects' and performing settings: a moment after the last change (playing: faded) */
        return;
    }
    if (!project_pending() || ui.btn || ui.keys || ui.store_wait)
        return;
    if (!en_playing && now - ui.act_t >= KIT_QUIET)
        project_flush();                         /* stopped: all of it */
    else if (project_pending_globals() && now - ui.act_t >= (en_playing ? 2 * GLOB_QUIET : GLOB_QUIET))
        quiet_write(2);                          /* playing: the theme, Tune, MIDI out (faded) */
}

/* THE BOOT CARDS. First deamonSLINKY (the maker): its mascot, a slinky with demon horns (a 23 x 20 sprite, 4 px a pixel; the Hellfire
 * colorway of the logo concept), the name in two colours, a ringed bar and "PRESENTS". Then PERF-1 in big pixel letters, what it is,
 * and the credit (a fork of HYP-1 by RedtailWorks), with sixteen steps filling in (the white keys lighting with them) until the
 * music starts. Any button or key: straight to the music.
 * Sprite letters: K outline, H horn, h horn shade, Y eye, R body, r body shade, L highlight, T tail, . nothing. */
static const char *const MASCOT[23] = {
    "..K............K.......",
    ".KHK..........KHK......",
    "..KHK........KHK.......",
    "..KhK.KKKKKK.KhK.......",
    "...KhKRRRRRRKhK........",
    "..KRLLRRRRRRRRRK.......",
    ".KRLRKKRRRRKKRRRK......",
    ".KRRRYYRRRRYYRRRK......",
    ".KRRRKRRRRRRKRRRK......",
    ".KrrrKHKKKKHKrrrK...K..",
    "..KKKKKKKKKKKKKK...KRK.",
    "..KRLLRKKKKrrrrK..KRLRK",
    "..KrrrrRLRRKKKK...KKKKK",
    "...KKKKrrrrRRRRK....T..",
    "..KRLLRKKKKrrrrK....T..",
    "..KrrrrRLRRKKKK.....T..",
    "...KKKKrrrrRRRRK...T...",
    "..KRLLRKKKKrrrrK..T....",
    "..KrrrrRLRRKKKK...T....",
    "...KKKKrrrrRRRRKTT.....",
    "..KRLLRKKKKrrrrK.......",
    "..KrrrrRLRRKKKKK.......",
    "...KKKKKKKKKKKK........"};
static uint16_t mascot_color(char ch)
{
    switch (ch) {
    case 'K': case 'T': return RGB(27, 16, 34);
    case 'H': return RGB(242, 230, 196);
    case 'h': return RGB(196, 173, 126);
    case 'Y': return RGB(255, 212, 71);
    case 'R': return RGB(224, 52, 79);
    case 'r': return RGB(156, 29, 58);
    case 'L': return RGB(255, 143, 143);
    default: return 0;
    }
}

static void splash_mascot(int hop, int blink)    /* the mascot, hopped up hop pixels, its eyes shut when blinking */
{
    int x, y;
    cv_begin(240, 108, K_BG);
    for (y = 0; y < 23; y++)
        for (x = 0; x < 23; x++)
            if (MASCOT[y][x] != '.')
                cv_rect(120 - 23 * 2 + x * 4, 108 - 23 * 4 + y * 4 - hop, 4, 4, mascot_color(blink && MASCOT[y][x] == 'Y' ? 'K' : MASCOT[y][x]));
    cv_blit(0, 0);
}

static void draw_splash(void)
{
    int i;
    lcd_fill(0, 0, 240, 240, K_BG);
    splash_mascot(0, 0);
    cv_begin(240, 132, K_BG);                    /* the name, the rings, and what's next */
    text_c(120, 0, &FONT_L, "DEAMON", RGB(242, 230, 196));
    text_c(120, 36, &FONT_L, "SLINKY", RGB(255, 143, 143));
    for (i = 0; i < 9; i++)
        cv_rect(48 + i * 16, 86, 12, 6, i & 1 ? RGB(224, 52, 79) : RGB(156, 29, 58));
    cv_frame(40, 80, 160, 18, K_LINE);
    text_c(120, 110, &FONT_XS, "PRESENTS", K_DIM);
    cv_blit(0, 108);
}

/* the second card: PERF-1, big 5 x 7 pixel letters (6 px a pixel), an orange shadow under the purple */
#define BIG_N 6
#define BIG_PX 6
static const uint8_t BIG_GLYPH[BIG_N][7] = {
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  /* P */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  /* E */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},  /* R */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  /* F */
    {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00},  /* - */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* 1 */
};
#define SPLASH_STEP_Y 184
static void draw_hyp(void)
{
    int g, r, c, pass;
    lcd_fill(0, 0, 240, 240, K_BG);
    cv_begin(240, 140, K_BG);
    for (pass = 0; pass < 2; pass++)             /* the shadow, then the letters */
        for (g = 0; g < BIG_N; g++)
            for (r = 0; r < 7; r++)
                for (c = 0; c < 5; c++)
                    if (BIG_GLYPH[g][r] >> (4 - c) & 1u)
                        cv_rect(12 + (g * 6 + c) * BIG_PX + (pass ? 0 : 3), 24 + r * BIG_PX + (pass ? 0 : 3), BIG_PX, BIG_PX,
                                pass ? K_PAD : K_ACC);
    text_c(120, 86, &FONT_S, "GENERATIVE  //  LIVE", K_TEXT);
    cv_blit(0, 6);
    cv_begin(240, 52, K_BG);                     /* (a canvas is at most 144 rows) */
    text_c(120, 0, &FONT_XS, EN_VERSION[0] == 'D' ? "FM-1 CUSTOM FIRMWARE" : "FM-1 CUSTOM FIRMWARE  //  " EN_VERSION, K_TEXT);
    text_c(120, 16, &FONT_XS, "BY DEAMONSLINKY", RGB(255, 143, 143));
    text_c(120, 32, &FONT_XS, "BASED ON HYP-1 BY REDTAILWORKS", K_DIM);
    cv_blit(0, 120);
    for (c = 0; c < 16; c++)
        lcd_fill(8 + c * 14, SPLASH_STEP_Y, 11, 11, K_PANEL);
}

static void splash_tick(void)                    /* the HYP-1 card's steps, filling in to the music */
{
    uint32_t t = plat_ms() - ui.boot_t, t1 = ui_splash_ms * 17u / 32u, n, k = 0, i;
    if (ui.splash == 1) {
        static uint32_t last = 0xFFFFFFFFu;
        if (t < t1) {                              /* the mascot hops twice, then blinks */
            const int hop = t < t1 * 3u / 5u ? (int)(14.0f * fm_fabsf(fm_sinf(FM_PI * (float)t / (float)(t1 * 3u / 10u + 1u)))) : 0,
                      blink = t > t1 * 3u / 4u && t < t1 * 7u / 8u;
            const uint32_t s = (uint32_t)hop | (uint32_t)blink << 8;
            if (s != last) {
                last = s;
                splash_mascot(hop, blink);
            }
            return;
        }
        last = 0xFFFFFFFFu;
        ui.splash = 2;
        draw_hyp();
    }
    n = (t - t1) * 17u / (ui_splash_ms - t1 + 1u);
    if (n > 16u)
        n = 16u;
    for (; ui.splash_step < n; ui.splash_step++)
        lcd_fill(8 + ui.splash_step * 14, SPLASH_STEP_Y, 11, 11, ui.splash_step % 4u ? K_PAD : K_ACC);
    for (i = 0; i < n; i++)
        k |= 1u << WHITE_KEY[i];
    plat_glow(1);
    plat_leds(n ? 1u << B_PLAY : 0u, k);
}

void ui_init(void)
{
    if (rm.on)
        en_mute(0);
    memset(&rm, 0, sizeof rm);
    memset(&ui, 0, sizeof ui);
    ui.pat_key = -1;                             /* (no pattern key held) */
    ui.gr_all = 50;
    {   /* the effects' settings start at the engine's defaults; the gate at its first pattern, on the synths */
        int i;
        for (i = 0; i < EN_NFXS; i++)
            ui.fxs[i] = (uint8_t)EN_FXS_DEF[i];
        ui.gx_pat = GX_PRE[0].pat;
        ui.gx_preset = 0;
        ui.gx_tgt = 0;
        ui.scene_quant = 1;                    /* a scene on the next bar */
        ui.dj_len = 1;                         /* the auto-DJ: 16 bars of groove, a half-length breakdown, a fill before it */
        ui.dj_break = 1;
        ui.dj_fillon = 1;
        ui.mood = 50;                          /* the mood macro: the middle leaves the piece as it is */
        ui.jam_oct = 2;                        /* the JAM page: octave 0 */
    }
    settings_load();                             /* the effects' and performing settings as you left them */
    voices_load();                               /* and the player's DX7 voices */
    ui.touched = -1;
    ui.boot_t = plat_ms();
    ui.theme = (uint8_t)proj.par[P_THEME];
    th = &THEMES[ui.theme < EN_NTHEME ? ui.theme : 0];
    ui.splash = ui_splash_ms > 0u;
    if (ui.splash) {
        draw_splash();
    } else {
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);                          /* it plays from the start */
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
        ui.splash = 0;                           /* the cards' time is up (or skipped): the UI, and the music */
        ui.btn = plat_buttons();                 /* a press that skipped them does nothing more */
        ui.keys = plat_keys();
        ui.btn_used = ui.btn;
        ui.shift_keys = ui.keys;
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing)
            en_play(1);
    }
    if (ui.theme != (uint8_t)proj.par[P_THEME]) {    /* a new theme: everything again */
        ui.theme = (uint8_t)proj.par[P_THEME];
        th = &THEMES[ui.theme < EN_NTHEME ? ui.theme : 0];
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
        lcd_fill(0, 0, 240, 240, K_BG);
    }
    store_tick();
    {   /* drum drift changed the pattern: the knob follows, and says so */
        static uint32_t seen;
        if (en_dd_changes != seen) {
            char t[12] = "PATTERN ";
            seen = en_dd_changes;
            proj.par[P_PATTERN] = (int16_t)en_pat_next;
            t[8] = (char)('1' + en_pat_next % 10u);
            t[9] = 0;
            if (plat_ms() >= ui.msg_until)
                say("DRUM DRIFT", t);
        }
    }
    if (rm.on) {                                 /* remote: the browser's screen, the browser's lights */
        rm_tick();
        if (rm.on) {
            plat_glow(1);
            plat_leds(rm.lit_b, rm.lit_k);
            return;
        }
    }
    input();
    scene_tick();
    dj_tick();
    key_tick();
    glide_tick();
    saver_tick();
    jam_tick();
    {   /* a render that ran out of time (an xrun, or the longest render of the last second reaching the deadline): the engine drops the chorus and holds the DX voices to four,
         * then two; and eases back a step after twenty seconds without one. (It went by the average CPU before, then by how close the peaks came: the logs showed both sit high in the
         * heavy pieces, so it never eased back, while the cracks themselves were rare.) */
        static uint32_t over_t, calm_t, last_xr;
        const uint32_t now = plat_ms(), pk = plat_peak_pct(), xr = plat_xruns();
        const int xrun = xr != last_xr;
        last_xr = xr;
        if (!en_playing) {
            over_t = calm_t = now;
        } else if (xrun || pk >= 100u) {
            calm_t = now;
            if ((xrun || now - over_t > 600u) && en_shed_level < 2) {
                en_shed(en_shed_level + 1);
                over_t = now;
            }
        } else {
            over_t = now;
            if (now - calm_t > 20000u && en_shed_level > 0) {   /* twenty seconds without a crack: a step back (and if it cracks again, a step up: the peaks sit at 90-95 % of the time in the heavy pieces, so they cannot be the gauge) */
                en_shed(en_shed_level - 1);
                calm_t = now;
            }
        }
    }
    {   /* the JAM page plays one voice by hand: its own line stops (the engine's hand switch, not the piece's setting), and comes back when you leave */
        static const uint8_t MAN[4] = {P_CM_MAN, P_PAD_MAN, P_AC_MAN, P_DR_MAN};
        const int want = ui.view == V_JAM ? 1 + (ui.jam_voice & 3) : 0;
        if (want != ui.jam_man) {
            if (ui.jam_man)
                en_set(MAN[ui.jam_man - 1], proj.par[MAN[ui.jam_man - 1]]);
            if (want)
                en_set(MAN[want - 1], 1);
            ui.jam_man = (uint8_t)want;
        }
    }
    autosave();
    if (kr.on) {                                 /* the secret: the whole screen, twenty frames a second */
        runner_update();
        if (ui.frame % 3u == 0u) {
            runner_draw(0);
            cv_blit(0, 0);
            runner_draw(1);
            cv_blit(0, 120);
        }
        runner_leds();
        ui.frame++;
        return;
    }
    if (!ui.saver_on)
        draw_header();                           /* (the screensaver has the whole screen) */
    draw_main();
    if (!ui.saver_on)
        draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void)
{
    if (rm.on)
        rm_tick();
    else
        input();
}
