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
 *   keys           play the voice whose page it is (the Jam page's voice elsewhere, with its keys and octave);
 *                  on Global, the scale (each adds or removes its note, 0.17.6); in the sequencer, the sixteen white keys
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
enum { V_TEMPO, V_PAD, V_PADENV, V_PADLFO, V_CHORDS, V_PROG, V_DRONE, V_DRENV, V_DRLFO, V_DRBASS, V_BLSTEP, V_BLANS, V_ACID, V_ACENV, V_ACLINE, V_ACANS, V_ACSTEP, V_CMEL, V_CMEL2, V_CMVU, V_CMSTEP,
       V_DRUMS, V_LINKS, V_KIT, V_DDRIFT, V_GROOVE, V_STEPS, V_FEEL, V_PHRASE, V_SPACE, V_REVERB, V_MASTER, V_MIXER, V_GLOBAL, V_SETL, V_CODE, V_JAM, NDIAL, V_SEQ = NDIAL, V_PERF, V_VIZ, NVIEWS };   /* V_PERF: the second home screen; V_VIZ: the visualizers */
static const char *const VIEW_NAME[NVIEWS] = {"Tempo", "Pad", "Pad env", "Pad LFO", "Chords", "Progression", "Bass", "Bass env",
                                              "Bass LFO", "Bass line", "Bass steps", "Bass answer", "Acid", "Acid env", "Acid line", "Acid answer", "Acid steps", "Pluck", "Pluck 2", "Deja vu", "Pluck steps", "Drums", "Links", "Kit", "Drum drift", "Groove", "Steps", "Feel", "Phrase", "Space", "Reverb", "Comp/Limit", "Mixer", "Global", "Set list", "Code", "Jam",
                                              "Sequencer", "Perform", "Visual"};
enum { VZ_PLASMA, VZ_KALEIDO, VZ_BARS, VZ_FLUX, VZ_DISCO, VZ_LIFE, VZ_ORBIT, VZ_TRIP, VZ_THEORY, VZ_AUTO, VZ_N };   /* the visualizers (viz.c; TRIP and THEORY 0.16.10, THEORY not in AUTO's turn) */
static const char *const VZ_NAME[VZ_N] = {"PLASMA", "KALEIDO", "BARS", "FLUX", "DISCO", "LIFE", "ORBIT", "TRIP", "THEORY", "AUTO"};
/* the knobs of each page: a parameter, or one of these (the sequencer's: the lane's) */
enum { K_LVL = 200, K_TUNE, K_DEC, K_MXT, K_MXL, K_MXP, K_MXS, K_GRT, K_GRD, K_GRS, K_GRA, K_ACT, K_ACD, K_ACE, K_ACL, K_STR, K_STP, K_STV, K_PCOND, K_ASTY, K_ASNO, K_ASOC, K_PSNO, K_PSLV, K_BSTY, K_BSNO, K_BSOC, K_PGSL, K_PGCH, K_PGBR, K_PGPL, K_PGTY, K_SLSL, K_SLGN, K_SLPC, K_SLON, K_GENRE, K_JSND, K_PLRT, K_DLRT, K_CDPOS, K_CDCHR, K_CDROLL, K_SLPT, K_SLEN, K_BAT, K_BAE, K_NONE, K_FXF, K_FXC, K_FXT, K_FXW };   /* K_GENRE: the genre on Global; K_JSND: the sound of the voice JAM plays (0.17.4); K_FX*: the FX menu's own knobs */
static const char *const FXK_LABEL[4] = {"Filter", "Crush", "Throw", "Wash"};

/* the FX edit pages (SEL + FX walks them; FX alone is the performing menu, or closes): each effect's settings on
 * the four knobs, the effect itself held on while the page is up, so it is heard as it is edited. K_FXP + 4 *
 * page + knob. The gate's pattern is on the white keys, as the sequencer's steps. */
enum { K_FXP = K_FXW + 1 };   /* (0.15.16: it was a fixed 230, and the Progression page's four knob codes (0.15.6) moved Throw and Wash onto it: they turned the
                                * GATE page's pattern and length) */
_Static_assert((int)K_FXP > (int)K_FXW, "the FX pages' knob codes come after every other knob code");
enum { KN_PLAIN, KN_PRESET, KN_LEN, KN_TGT, KN_PTGT, KN_NAME, KN_SEC, KN_RANGE, KN_PAR, KN_NONE };   /* KN_RANGE: 2 .. n; KN_PAR: an engine parameter (idx), the dial pages' knob code; KN_NONE: no knob */
#define NFXP 41
#define FXP_MIDIIN 25                       /* the MIDI IN page's place in FXP (0 ..): its panel */
#define FXP_SCREEN 30                       /* the SCREEN page (0.16.1): zoom, saver, lights, scanlines */
#define FXP_SAMPLE 31                       /* the SAMPLE page (0.16.1): PERF's sounds one at a time, for a sampler */
static const char *const RATE_NAME[7] = {"1/2", "1/4", "1/6", "1/8", "1/16", "1/32", "1/64"};          /* the roll's rates */
static const char *const RTIME_NAME[4] = {"1/2 BEAT", "1 BEAT", "2 BEATS", "4 BEATS"};         /* beats at each rate */
static const char *const PRATE_NAME[5] = {"1/2", "1/4", "1/8", "1/16", "KICK"};                        /* the pump's period, in notes */
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
static const char *const LOOK_NAME[4] = {"PLAIN", "SCANLINES", "CALM", "CALM+SCAN"};     /* (0.17.40) SCREEN's Look: the scanlines; Calm, the screen's small motion off (no imp up top, no knob tick, no page fade) */
static const char *const LIGHTS_NAME[2] = {"OFF", "SHOW"};                                       /* the keys' lights on the visualizers */
static const char *const USBL_NAME[2] = {"KNOB", "FIXED"};
static const char *const FORM_NAME[4] = {"Song", "Few rests", "No rests", "Loop"};   /* (0.17.70: Song with its rests half as often, or none) */
static const char *const START_NAME[2] = {"Last", "Random"};                                       /* the Settings page's Start (0.16.26): power on to the piece last played, or one at random */                                          /* the Settings page's Form (0.16.26): a piece moves through its patterns, or loops its first */                                          /* the thirty built-in DX7 voices: Melodee's set, or the human-made one */
static const char *const RPP_NAME[25] = {"-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0",
                                         "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11", "+12"};   /* the repeat's pitch, semitones reached over four beats held */
static const char *const RPG_NAME[4] = {"FULL", "3/4", "HALF", "1/4"};                              /* the repeat's gate: the part of the slice that sounds */
static const char *const RPR_NAME[3] = {"OFF", "BEAT", "BAR"};                                      /* the repeat's slip refresh */                                       /* the USB audio's level: the master knob's, or fixed (a recording that does not move with the knob) */
static const char *const KEYSET_NAME[5] = {"SAME", "+2", "+3", "+5", "+7"};                     /* the drop's key change, in semitones */
static const char *const GENRE_UI_NAME[EN_NGENRE] = {"HYPNO", "LOFI", "HOUSE", "DUBSTEP", "DUB", "DNB", "BREAKS", "GARAGE", "ELECTRO", "TRAP", "DISCO", "FUNK", "REGGAE", "SYNTHWV", "AMAPI", "TRIPHOP", "INDUS", "PSY", "FOREST", "HIPHOP", "TRANCE", "AMBIENT"};
static const char *const SCQ_NAME[2] = {"AT ONCE", "NEXT BAR"};                                  /* when a scene comes */
static const char *const DJG_NAME[5] = {"8 BARS", "16 BARS", "32 BARS", "64 BARS", "SYNC"};   /* the auto-DJ's groove (SYNC: until the chord changes) */
static const char *const DJB_NAME[3] = {"SHORT", "HALF", "FULL"};                                /* its breakdown, against the groove */
static const char *const ROUNDS_NAME[4] = {"1", "2", "4", "8"};                                  /* the SET page: rounds of the auto-DJ a piece gets before the next */
static const uint8_t DJ_ROUNDS[4] = {1, 2, 4, 8};
static const char *const RAMP_NAME[5] = {"OFF", "+2 BPM", "+4 BPM", "+8 BPM", "+12 BPM"};        /* the tempo's climb over a piece's rounds */
static const uint8_t RAMP_BPM[5] = {0, 2, 4, 8, 12};
static const char *const INTRO_NAME[6] = {"OFF", "1 MIN", "2 MIN", "4 MIN", "8 MIN", "BREAK"};    /* the slow progression from Play; BREAK (0.17.22, a Reddit
                                                                                                    * player who moves between pieces through a breakdown): a piece begins in its breakdown */
static const uint16_t INTRO_S[6] = {0, 60, 120, 240, 480, 0};
static const char *const ONOFF_NAME[2] = {"OFF", "ON"};
static const char *const TILT_NAME[4] = {"OFF", "GENRE", "WARMER", "BRIGHTER"};
static const char *const JC_NAME[6] = {"OFF", "I", "II", "I+II", "ROT SLOW", "ROT FAST"};   /* the CHORUS page (0.17.44): the Juno's three; (0.17.49) a rotary's two speeds */
static const char *const JCT_NAME[3] = {"PLUCK", "ACID", "BOTH"};
static const char *const LKM_NAME[6] = {"OFF", "AUTO", "LEAD", "FOLLOW", "CABLE LEAD", "CABLE FOLLOW"};   /* the LINK page (0.17.47): AUTO, the unit that is the USB host
                                                                                 * leads; CABLE (0.17.55, link_wire.c): a plain USB-C cable, no USB, the port's two wires */
static const char *const LKP_NAME[5] = {"ALL", "BEAT", "TOP", "DRUMS", "NO DRUMS"};   /* what this unit plays when linked: BEAT the drums and the bass, TOP the pad, pluck and acid */
static const char *const LKS_NAME[2] = {"OWN", "FOLLOW"};
static const char *const RZR_NAME[3] = {"LOW", "MID", "HIGH"};   /* the RESONATOR page (0.17.51): the bands' register, and what goes in */
static const char *const RZT_NAME[6] = {"PERC", "HATS", "SNARE", "PERC+HATS", "DRUMS", "PLUCK"};
static uint32_t lk_heard;                        /* (0.17.47) THE LINK (lk_tick): when the partner's hello came last (ms; 0 never) */
static uint8_t lk_pk, lk_role, lk_lo[3];         /* its hello (the unit, + 64 if it leads); 0 alone, 1 leading, 2 following, 3 linked with no lead (or two);
                                                  * the low bits waiting for their pair (the scale's, the chord's two) */
static uint16_t lk_cm = 0xFFFFu;                 /* the lead's chord taken last (its notes, its root << 12) */
static const char *const XMIX_NAME[5] = {"CUT", "4 BARS", "8 BARS", "16 BARS", "32 BARS"};       /* the SET page's Mix: how long the next piece glides to its tempo */
static const uint8_t XMIX_BARS[5] = {0, 4, 8, 16, 32};
static const char *const SMPM_NAME[4] = {"ALL", "DRUMS", "SYNTHS", "BY KEY"};                   /* the SAMPLE page: what a run plays, or a key a sound */
static const char *const SMPG_NAME[4] = {"0.5 S", "1 S", "2 S", "4 S"};
static const uint16_t SMP_GAP_MS[4] = {500, 1000, 2000, 4000};
static const char *const MIN_NAME[5] = {"KEYS", "PADS", "M:S", "MPK", "CHORDS"};   /* (KEYS was SCALE until 0.17.7: its notes changed the scale anywhere) */
static const char *const CC_NAME[4] = {"OFF", "IN", "OUT", "IN+OUT"};
static const char *const FT_NAME[6] = {"LOW", "HIGH", "BAND", "NOTCH", "PEAK", "303"};   /* (303: the acid's alone, 0.17.17) */
static const char *const TF_NAME[26] = {"LOW", "HIGH", "BAND", "NOTCH", "PEAK", "MORPH LP", "MORPH 5", "MORPH 10", "MORPH 15", "MORPH 20", "MORPH 25",
                                        "MORPH 30", "MORPH 35", "MORPH 40", "MORPH 45", "MORPH BP", "MORPH 55", "MORPH 60", "MORPH 65", "MORPH 70",
                                        "MORPH 75", "MORPH 80", "MORPH 85", "MORPH 90", "MORPH 95", "MORPH HP"};   /* (0.17.72) the punch-in FILTER's: past PEAK, Morph from low (0) through band (50) to high (100) */
static const char *const RDM_NAME[3] = {"KIT", "OFF", "CUSTOM"};                       /* (0.17.72) the RETURNS page: the returns' duck as the kit has it, none, or its own */
static const char *const RDR_NAME[6] = {"1/16", "1/8", "3/16", "1/4", "1/2", "1 BAR"};  /* its release: back up in that many sixteenths (the kit's: 3/16) */
static const char *const LAND_NAME[4] = {"NOW", "BEAT", "2 BEATS", "BAR"};             /* (0.17.72) the snips' Land: the riser, the rewind and the roll peak on the next such line */
static const char *const SQT_NAME[3] = {"ACID", "BASS", "BOTH"};   /* SCREAM's target (0.16.4) */
static const char *const AFA_NAME[4] = {"OFF", "GENTLE", "MEDIUM", "WILD"};   /* AUTO FX (0.16.8): how much, how often, how long, which */
static const char *const AFE_NAME[4] = {"4 BARS", "8 BARS", "16 BARS", "32 BARS"};
static const char *const AFL_NAME[4] = {"A BEAT", "HALF BAR", "A BAR", "2 BARS"};
static const char *const AFP_NAME[3] = {"GENRE", "ANY", "YOURS"};   /* (YOURS, 0.17.44: the gesture recorder's, kept) */
static const char *const MCH_NAME[17] = {"ALL", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
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
                          {"Every", KN_NAME, EN_FXS_PM_RATE, PRATE_NAME, 5}, {"Targets", KN_PTGT, 0, 0, 0}}},
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
    {"SETTINGS", 63, 0, {{"Key", KN_NAME, 4, KEYSET_NAME, 5}, {"", KN_NONE, 0, 0, 0}, {"Form", KN_NAME, 28, FORM_NAME, 4},
                         {"Start", KN_NAME, 29, START_NAME, 2}}},   /* (the genre moved to Global's fourth knob in 0.14.7); (0.17.75: Voices gone, the DY set always); Saver and Lights on SCREEN (0.16.1) */
    {"FLEX", 62, 0, {{"Quant", KN_NAME, EN_FXS_FX_QUANT, FXQ_NAME, 3}, {"Mode", KN_NAME, EN_FXS_FX_MODE, FXM_NAME, 2},
                     {"Mix", KN_PLAIN, EN_FXS_FX_MIX, 0, 0}, {"", KN_NONE, 0, 0, 0}}},
    {"SNIPS", 62, 0, {{"Pitch", KN_NAME, EN_FXS_SM_MODE, SMM_NAME, 3}, {"Play", KN_NAME, EN_FXS_SM_LOOP, SML_NAME, 2},
                      {"Echo", KN_NAME, EN_FXS_SM_LEN, SMN_NAME, 3}, {"Level", KN_PLAIN, EN_FXS_SM_MIX, 0, 0}}},
    {"TALK", FX_TALK, 0, {{"Every", KN_NAME, EN_FXS_TK_RATE, TKR_NAME, 5}, {"Depth", KN_PLAIN, EN_FXS_TK_DEPTH, 0, 0},
                          {"Reso", KN_PLAIN, EN_FXS_TK_RESO, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_TK_MIX, 0, 0}}},
    {"WAH", FX_WAH, 0, {{"Sens", KN_PLAIN, EN_FXS_WH_SENS, 0, 0}, {"Range", KN_PLAIN, EN_FXS_WH_RANGE, 0, 0},
                        {"Reso", KN_PLAIN, EN_FXS_WH_RESO, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_WH_MIX, 0, 0}}},
    /* the JAM page's four knobs (the UI's own: uiv 7 .. 10): not an FX page, the dial never walks to it */
    {"REPEAT", FX_RPT2, 1, {{"Pitch", KN_NAME, EN_FXS_RP_PITCH, RPP_NAME, 25}, {"Decay", KN_PLAIN, EN_FXS_RP_DECAY, 0, 0},
                            {"Gate", KN_NAME, EN_FXS_RP_GATE, RPG_NAME, 4}, {"Refresh", KN_NAME, EN_FXS_RP_REFRESH, RPR_NAME, 3}}},   /* the eight repeat keys' page (fx_page_of maps them all here) */
    {"USB & MIDI", 63, 0, {{"Level", KN_NAME, 12, USBL_NAME, 2}, {"MIDI out", KN_PAR, P_MIDI, 0, 0}, {"MIDI map", KN_NAME, 18, EN_MAP_NAME, EN_NMAP}, {"", KN_NONE, 0, 0, 0}}},   /* (Scanlines on SCREEN, 0.16.1) */   /* the cable: the USB audio's level (the knob's, or fixed), MIDI out (it was Global's); the screen's scanlines; and where MIDI out's parts go (GM, or a sampler's own layout) */
    {"SET", 63, 0, {{"Rounds", KN_NAME, 15, ROUNDS_NAME, 4}, {"Ramp", KN_NAME, 16, RAMP_NAME, 5}, {"Intro", KN_NAME, 17, INTRO_NAME, 6}, {"Mix", KN_NAME, 24, XMIX_NAME, 5}}},   /* the set: rounds a piece, the tempo's climb, the intro; the mix into the next piece (0.16.1) */
    {"MIDI IN", 63, 0, {{"Notes", KN_NAME, 20, MIN_NAME, 5}, {"Channel", KN_NAME, 19, MCH_NAME, 17}, {"USB host", KN_NAME, 21, ONOFF_NAME, 2}, {"Knob CCs", KN_NAME, 22, CC_NAME, 4}}},   /* MIDI in's notes: the scale (as before), or PADS (a pad controller plays PERF: pads_in); the channel listened to; the USB port as the host of a USB-MIDI device (not kept: every power-on is a device) */
    {"FILTERS", FX_SPARE14, 0, {{"Pad", KN_NAME, EN_FXS_FT_PAD, FT_NAME, 5}, {"Bass", KN_NAME, EN_FXS_FT_BASS, FT_NAME, 5}, {"Acid", KN_NAME, EN_FXS_FT_ACID, FT_NAME, 6}, {"", KN_NONE, 0, 0, 0}}},   /* the synths' filter types (0.15.15; a spare effect bit: nothing held) */
    {"FILTER", FX_TFILT, 0, {{"Type", KN_NAME, EN_FXS_TF_TYPE, TF_NAME, 26}, {"Cutoff", KN_PLAIN, EN_FXS_TF_CUT, 0, 0}, {"Reso", KN_PLAIN, EN_FXS_TF_RESO, 0, 0},
                             {"Targets", KN_NAME, EN_FXS_TF_TGT, EN_PT_NAME, EN_NPT}}},   /* (0.15.19) the punch-ins on tracks: a multimode filter on the tracks chosen */
    {"DELAY", FX_TDELAY, 0, {{"Send", KN_PLAIN, EN_FXS_TD_SEND, 0, 0}, {"Repeats", KN_PLAIN, EN_FXS_TD_FB, 0, 0}, {"Targets", KN_NAME, EN_FXS_TD_TGT, EN_PT_NAME, EN_NPT},
                             {"", KN_NONE, 0, 0, 0}}},   /* those tracks into the echo (at the piece's echo time) */
    {"PHASE", FX_TPHASE, 0, {{"Rate", KN_PLAIN, EN_FXS_TP_RATE, 0, 0}, {"Depth", KN_PLAIN, EN_FXS_TP_DEPTH, 0, 0}, {"Mix", KN_PLAIN, EN_FXS_TP_MIX, 0, 0},
                             {"Targets", KN_NAME, EN_FXS_TP_TGT, EN_PT_NAME, EN_NPT}}},   /* a phaser on them */
    {"SCREEN", 63, 0, {{"Zoom", KN_NAME, 23, ONOFF_NAME, 2}, {"Saver", KN_NAME, 5, SAVER_NAME, 4}, {"Lights", KN_NAME, 6, LIGHTS_NAME, 2},
                       {"Look", KN_NAME, 14, LOOK_NAME, 4}}},   /* (0.17.40: Scanlines became Look: the scanlines, and Calm) (0.16.1) the screen's settings together; Zoom: a knob turned shows its name and value across the screen */
    {"SAMPLE", 63, 0, {{"Sounds", KN_NAME, 25, SMPM_NAME, 4}, {"FX", KN_NAME, 26, ONOFF_NAME, 2}, {"Gap", KN_NAME, 27, SMPG_NAME, 4},
                       {"", KN_NONE, 0, 0, 0}}},   /* (0.16.1) the sampling helper: PERF's drums and voices one at a time, for a sampler (not kept) */
    {"SCREAM", FX_SPARE13, 0, {{"Drive", KN_PLAIN, EN_FXS_SQ_DRIVE, 0, 0}, {"Scream", KN_PLAIN, EN_FXS_SQ_AMT, 0, 0}, {"Wow", KN_PLAIN, EN_FXS_SQ_WOW, 0, 0},
                       {"Target", KN_NAME, EN_FXS_SQ_TGT, SQT_NAME, 3}}},   /* (0.16.4; a spare effect bit, as FILTERS: nothing held) the acid's and the bass's filters pushed to scream: drive into it, resonance towards singing, accents opening it */
    {"AUTO FX", FX_SPARE12, 0, {{"Auto FX", KN_NAME, EN_FXS_AF_AMT, AFA_NAME, 4}, {"Every", KN_NAME, EN_FXS_AF_EVERY, AFE_NAME, 4},
                        {"Length", KN_NAME, EN_FXS_AF_LEN, AFL_NAME, 4}, {"Pick", KN_NAME, EN_FXS_AF_PICK, AFP_NAME, 3}}},   /* (0.16.8; a spare effect bit: nothing held) effects played by
                                                    * themselves, ending on the phrase lines (af_tick) */
    {"SOUND", FX_SPARE11, 0, {{"Vintage", KN_PLAIN, EN_FXS_VINTAGE, 0, 0}, {"Polish", KN_NAME, EN_FXS_POLISH, ONOFF_NAME, 2},
                       {"Tilt", KN_NAME, EN_FXS_TILT, TILT_NAME, 4}, {"Width", KN_PLAIN, EN_FXS_WIDTH, 0, 0}}},   /* (0.17.30; a spare effect bit: nothing held) the voices' wandering pitch,
                                                    * the master's polish (the lows mono, the peaks rounded), its tilt, the width up top */
    {"CHORUS", FX_SPARE10, 0, {{"Type", KN_NAME, EN_FXS_JC_TYPE, JC_NAME, 6}, {"Target", KN_NAME, EN_FXS_JC_TGT, JCT_NAME, 3}, {"", KN_NONE, 0, 0, 0}, {"", KN_NONE, 0, 0, 0}}},   /* (0.17.44; a spare effect bit: nothing held) the Juno-style chorus: off, I, II, I+II, on the pluck, the acid or both */
    {"LINK", FX_SPARE9, 0, {{"Link", KN_NAME, EN_FXS_LK_MODE, LKM_NAME, 6}, {"Plays", KN_NAME, EN_FXS_LK_PLAYS, LKP_NAME, 5},
                     {"Sections", KN_NAME, EN_FXS_LK_SEC, LKS_NAME, 2}, {"", KN_NONE, 0, 0, 0}}},   /* (0.17.47; a spare effect bit: nothing held) two units as one band: who leads,
                                                    * what this one plays, the lead's breakdowns and drops followed */
    {"RESONATOR", FX_SPARE8, 0, {{"Amount", KN_PLAIN, EN_FXS_RZ_AMT, 0, 0}, {"Decay", KN_PLAIN, EN_FXS_RZ_DEC, 0, 0},
                     {"Register", KN_NAME, EN_FXS_RZ_REG, RZR_NAME, 3}, {"Targets", KN_NAME, EN_FXS_RZ_TGT, RZT_NAME, 6}}},   /* (0.17.51; a spare effect bit: nothing held)
                                                    * three bands tuned to the chord, rung by the tracks chosen */
    {"RETURNS", FX_SPARE7, 0, {{"Duck", KN_NAME, EN_FXS_RD_MODE, RDM_NAME, 3}, {"Depth", KN_PLAIN, EN_FXS_RD_DEPTH, 0, 0},
                     {"Release", KN_NAME, EN_FXS_RD_REL, RDR_NAME, 6}, {"", KN_NONE, 0, 0, 0}}},   /* (0.17.72, ChatGPT's; a spare effect bit: nothing held) the echo's and
                                                    * the reverb's returns ducked by the kick: as the kit has it, not at all, or Custom: its own depth and release */
    {"SNIP LAND", 62, 0, {{"Land", KN_NAME, EN_FXS_SM_LAND, LAND_NAME, 4}, {"", KN_NONE, 0, 0, 0}, {"", KN_NONE, 0, 0, 0}, {"", KN_NONE, 0, 0, 0}}},   /* (0.17.72, after SNIPS)
                                                    * the riser, the rewind and the roll timed to peak on the next beat, two beats or bar */
    {"JAM", 63, 0, {{"Voice", KN_NAME, 7, JV_NAME, 4}, {"Keys", KN_NAME, 8, JM_NAME, 3}, {"Octave", KN_NAME, 9, JO_NAME, 5}, {"Sound", KN_PAR, K_JSND, 0, 0}}},   /* (the last: ui_knob_param counts on it) (0.17.4: Sound, the voice's, for Answer) */
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
} GX_TGT[NTGT] = {{"NO DRUMS", 0xF0}, {"PAD", 0x40}, {"BASS", 0x10}, {"ACID", 0x20}, {"DRUMS", 0x0F},
                    {"HATS", 0x02}, {"KICK", 0x01}, {"ALL", 0xFF}, {"NO KICK", 0xFE}, {"KICK+BASS", 0x11}, {"PLUCK", 0x80}};
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_PAD, P_BRIGHT, P_PADTONE, P_DETUNE},
    {P_PE_ATK, P_PE_HOLD, P_PE_REL, P_PE_REST},
    {K_PLRT, P_PL_DEPTH, P_PL_SHAPE, P_PL_DEST},
    {P_CHORD, P_VOICING, P_PL_CHO, P_CH_LOOP},     /* the harmony together, the pad's chorus, and how many chords repeat (Root is on Global, Drift on Tempo) */
    {K_PGCH, K_PGTY, K_PGBR, K_PGPL},              /* the progression by hand (0.15.6, after DUB-1's Chords page): the slot chosen on the white keys, its chord (a degree of
                                                    * the scale, or none: the chord before holds), its type (0.16.6: the piece's or a kind of its own; the slot knob gave
                                                    * way, the keys choose it), its bars, and Play: the genre's walk or the slots in turn */
    {P_BASS, P_DR_INST, P_DR_CUT, P_DR_RES},
    {P_DE_TRIG, P_DE_ATK, P_DE_DEC, P_DE_AMT},
    {K_DLRT, P_DL_DEPTH, P_DL_SHAPE, P_DL_DEST},
    {P_BL_MODE, P_BL_LEN, P_BL_MOVE, P_BL_GLIDE},   /* the drone as a bass line: its mode, how long a note holds, how much it moves, its glide */
    {K_BSTY, K_BSNO, K_BSOC, P_BL_MODE},           /* the bass steps (0.16.29, issue #6): the step chosen (a white key): its kind, its note (scale steps above the chord's root), its
                                                    * octave; and the bass line's mode (Steps plays them; an edit turns it to Steps) */
    {K_BAT, K_BAE, K_ACT, P_BL_MODE},              /* (0.17.30) the bass's answer: its kind, when (EN_FXS_BA_*, kept with the piece); the acid answer's kind beside it (call and response);
                                                    * the line's mode (Hold has no line to answer with) */
    {P_ACID, P_AC_CUT, P_AC_RES, P_AC_ENV},
    {P_AC_DEC, P_AC_ACC, P_AC_SLIDE, P_AC_WAVE},
    {P_AC_DENS, P_AC_LEN, P_AC_RANGE, P_AC_MUT},
    {K_ACT, K_ACE, K_ACD, K_ACL},                 /* the answer: its kind, how often (bars or loops of the line: P_AC_EVERY and P_AC_CHANCE as one knob, 0.17.21), which way (one value with the kind: P_AC_RESP), its level (EN_FXS_AC_ANS, the unit's) */
    {K_ASTY, K_ASNO, K_ASOC, P_AC_LOCK},           /* the acid's steps: the step chosen (a white key): its kind, its note, its octave; and whether the line is frozen (held, and stored with the piece) */
    {P_CM_LVL, P_CM_SND, P_CM_DENS, P_CM_OCT},
    {P_CM_DEC, P_CM_STYLE, P_CM_RATE, P_CM_RANGE},
    {P_CM_VU, P_CM_LOOP, P_CM_LEAP, P_CM_MOTIF},   /* deja vu: how often a pluck note is the one the loop heard last time, the loop's length, leaps, and the motif (how often a bar develops the phrase's first bar) */
    {K_PSLV, K_PSNO, P_CM_OCT, P_CM_LOCK},         /* the pluck's steps, laid out as the acid's: the step chosen (a white key): its kind (rest, soft, note, accent), its note (the chord's
                                                    * tones, low to high); the pluck's octave; and whether the line is frozen (held, and stored with the piece) */
    {P_PULSE, P_DRIVE, P_SWING, P_PATTERN},
    {P_LK_KICK, P_LK_SNARE, P_LK_HAT, K_NONE},     /* (0.17.50: P>Clock retired) */
    {P_KIT_KICK, P_KIT_SNARE, P_KIT_HAT, P_KIT_PERC},
    {P_DD_CHANGE, P_DD_EVERY, P_DD_POOL, P_DD_MUTATE},
    {K_GRT, K_GRD, K_GRS, K_GRA},                  /* the groove: a lane chosen, its density, the pattern it plays, and all lanes' density */
    {K_GRT, K_STR, K_STP, K_STV},                  /* the steps: the lane, and the step chosen (a white key): its ratchet, its push, its level */
    {P_GHOST, P_ACCENT, P_PAD_COMP, P_HUMAN},      /* the feel: the genre's ghost notes, how much accents and ghosts count, the pad's comping rhythm, looseness */
    {P_CHAIN, P_SLICE, P_TIME, K_PCOND},           /* the phrase: a chain of patterns, a slice permutation, half or double time, and the lane chosen's condition */
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_RV_TYPE, P_RV_DAMP, P_RV_PRE, P_RV_X},       /* the reverb's own page: its type, damping, predelay, and the type's amount */
    {P_MC_RATIO, P_MC_THR, P_MC_MAKE, P_ML_CEIL},   /* the master bus: compressor and limiter */
    {K_MXT, K_MXL, K_MXP, K_MXS},                  /* the mixer: a layer chosen, its level, its place, its send (the level is also on the layer's own page) */
    {P_ROOT, P_THEME, P_TUNE, K_GENRE},            /* global: the song's root, the look, the tuning, the genre (MIDI out is on the USB & MIDI settings page since 0.14.7) */
    {K_SLSL, K_SLGN, K_SLPC, K_SLON},              /* the set list (0.15.21): the slot (or a white key), its piece (0.17.13: genre by genre, one knob), its bars (song mode), and
                                                    * Play: off, loop, once (the auto-DJ and PRESETS go through it); the black keys 1-5 its parts */
    {K_CDPOS, K_CDCHR, K_CDROLL, K_NONE},          /* the track code (0.17.11): the place in it, the letter there, a new roll of the dice; PLAY starts it */
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
    {"Chord", "Type", "Bars", "Play"},
    {"Level", "Sound", "Cutoff", "Reso"},
    {"Trigger", "Attack", "Decay", "Amount"},
    {"Rate", "Depth", "Shape", "Target"},
    {"Mode", "Length", "Move", "Glide"},
    {"Step", "Note", "Octave", "Mode"},                 /* the bass steps (0.16.29) */
    {"Answer", "Every", "Acid ans", "Mode"},            /* the bass answer (0.17.30) */
    {"Level", "Cutoff", "Reso", "Env mod"},
    {"Decay", "Accent", "Slide", "Sound"},
    {"Density", "Length", "Range", "Mutate"},
    {"Answer", "Every", "Direction", "Level"},
    {"Step", "Note", "Octave", "Frozen"},
    {"Level", "Sound", "Density", "Octave"},
    {"Decay", "Style", "Rate", "Range"},
    {"Pluck vu", "Loop", "Leaps", "Motif"},
    {"Step", "Note", "Octave", "Frozen"},
    {"Level", "Drive", "Swing", "Pattern"},
    {"K>Bass", "S>Pad", "H>Acid", ""},
    {"Kick", "Snare", "Hats", "Perc"},
    {"Change", "Every", "Pool", "Mutate"},
    {"Lane", "Density", "Source", "All dens"},
    {"Lane", "Ratchet", "Push", "Level"},
    {"Ghosts", "Accents", "Pad comp", "Humanize"},
    {"Chain", "Slice", "Time", "Cond"},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Type", "Damping", "Predelay", "Amount"},
    {"Ratio", "Thresh", "Make-up", "Ceiling"},
    {"Track", "Level", "Pan", "Send"},
    {"Root", "Theme", "Tune", "Genre"},
    {"Slot", "Piece", "Bars", "Play"},
    {"Place", "Letter", "Roll", ""},
    {"Voice", "Keys", "Octave", "Sound"},
    {"Level", "Tune", "Decay", "Pattern"},
    {"Filter", "Crush", "Throw", "Wash"},
    {"Filter", "Crush", "Throw", "Wash"},
};
/* the buttons that lead to pages: each to a group, pressed again the next page in it */
static const struct {
    uint8_t btn, first, n;
} GROUP[] = {
    {B_HOME, V_TEMPO, 1}, {B_ENV, V_PAD, 5}, {B_LFO, V_DRONE, 6}, {B_ARP, V_ACID, 9}, {B_SEQ, V_DRUMS, 8}, {B_GLO, V_GLOBAL, 3},
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
     FX_STOP, FX_TAPE, FX_TFILT, FX_ROLL, FX_TDELAY, FX_PUMP, FX_TPHASE,               /* F4 F#4 G4 G#4 A4 A#4 B4 (0.15.19: the punch-ins on tracks where the drums' mutes were) */
     FX_MDRUMS, FX_RISER, FX_THROW, FX_VINYL, FX_FILL,                                 /* C5 C#5 D5 D#5 E5 */
     FX_LP, FX_NONE, FX_HP},                                                           /* F5 F#5 G5 */
    {FX_RPT2, FX_RPT4, FX_RPT6, FX_RPT8, FX_RPT16, FX_RPT32, FX_RPT8T,                 /* the repeats together, slow to fast: F3 .. B3 */
     FX_RPT16T, FX_NONE, FX_SCATTER, FX_BRAKE, FX_SPIN,                                /* C4 (the last repeat) C#4 D4 D#4 E4 */
     FX_HALF, FX_SCRATCH, FX_MUTE_KICK, FX_REV, FX_MUTE_HATS, FX_STOP, FX_MUTE_SNARE,  /* F4 F#4 G4 G#4 A4 A#4 B4 (the drums' single mutes: the first page has the punch-ins there since 0.15.19) */
     FX_MUTE_PERC, FX_HATROLL, FX_HATTRIP, FX_SNROLL, FX_FLAM,                         /* C5 .. E5: the drum rolls */
     FX_DBLKICK, FX_NONE, FX_NONE},
    {FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX,                    /* Flex Beat: a pattern on each key (FLEX_PAT), F3 .. G5 */
     FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX,
     FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX, FX_FLEX},
    {FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE,   /* the sampler: the keys are notes, not effects */
     FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE, FX_NONE}};
/* the Flex bank: every key is the one engine bit (FX_FLEX); the pattern each plays is a number (en_flex_select), so a bank can hold any number of them */
static const uint8_t FLEX_PAT[NKEYS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
static const char *const FLEX_LABEL[EN_NFLEX] = {"BT REPT", "HF REPT", "BT REV", "SKIP", "TRANCE", "GATE 8", "DOUBLE", "SHUFFLE", "BCK-FWD",
                                                 "ROLL OUT", "BUILD", "SCRATCH", "DROPOUT",
                                                 "JUMP BK", "HALVES", "SWAP", "MIRROR", "LAG", "SLOWMO", "SKIP 3", "STUT UP", "STUT DN", "TRESILLO",
                                                 "OFFBEAT", "CUT 4", "REV 8", "HALF GAP"};
static const char *const FX_LABEL[EN_NFX] = {
    [FX_RPT2] = "RPT 1/2", "RPT 1/4", "RPT 1/6", "RPT 1/8", "RPT1/16", "RPT1/32", "REVERSE", "TAPESTP",
    "M:KICK", "M:HATS", "M:SNARE", "M:PERC", "LOWPASS", "HIPASS",
    "GATE 16", "GATE 8", "GATE PAT", "THROW", "FILL", "PHASER", "FLANGER", "TAPE", "ROLL", "PUMP", "RISER", "VINYL",
    "SCATTER", "BRAKE", "SPIN BK", "HALFTIME", "SWEEP", "SCRATCH", "FREEZE", "RV SWELL", "RING MOD", "RPT 1/8T", "RPT1/16T", 
    "FLEX", "FILTER", "DELAY", "PHASE", "M:DRUMS", "", "", "", "", "", "", "", "", "", "", "TALK", "WAH", "HAT ROLL", "HAT TRIP", "SN ROLL", "FLAM", "DBL KICK",
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
#define KC_CHORD RGB(80, 200, 255)                 /* the chord playing on the JAM keyboard (0.15.21): apart from the keys held (KC_MOVE) */
static uint16_t fx_color(int fx)
{
    if (fx == FX_FLEX)
        return KC_FLEX;
    switch (fx) {
    case FX_RPT2: case FX_RPT4: case FX_RPT6: case FX_RPT8: case FX_RPT16: case FX_RPT32: case FX_RPT8T: case FX_RPT16T:
    case FX_HATROLL: case FX_HATTRIP: case FX_SNROLL: case FX_FLAM: case FX_DBLKICK: return KC_REPEAT;
    case FX_LP: case FX_HP: case FX_SWEEP: case FX_TFILT: return KC_FILTER;
    case FX_GATE16: case FX_GATE8: case FX_GATEX: case FX_PUMP: return KC_GATE;
    case FX_MUTE_KICK: case FX_MUTE_HATS: case FX_MUTE_SNARE: case FX_MUTE_PERC: case FX_MDRUMS: return KC_MUTE;
    case FX_PHASER: case FX_FLANGER: case FX_TAPE: case FX_VINYL: case FX_RING: case FX_TALK: case FX_WAH: case FX_TPHASE: return KC_COLOR;
    case FX_RISER: case FX_ROLL: case FX_FILL: case FX_THROW: case FX_RVSWELL: case FX_TDELAY: return KC_BUILD;
    default: return KC_MOVE;
    }
}
static inline uint64_t fxbit(int k) { return KEY_FX[fx_bank][k] == FX_NONE ? 0u : FXB(KEY_FX[fx_bank][k]); }
static inline int key_flex(int k) { return fx_bank == 2 && KEY_FX[2][k] == FX_FLEX ? FLEX_PAT[k] : -1; }   /* the Flex pattern a key plays here, -1: not a Flex key */
static inline uint32_t h32(uint64_t x) { return (uint32_t)(x ^ x >> 32); }   /* a 64-bit mask, for the hashes */

/* GLO held, the keys: every track's mute and solo, latched (en_trk). A code: 0..6 mute a track (EN_TR_*),
 * 8..14 solo it, 16 / 17 mute / solo the four drums together, 18 mutes and solos back; 20..23 a scene (tap: recall,\n * hold: store), 24 everything back (effects, knobs, mutes), 25 the auto-DJ on / off, 26 its length, 27 its new pieces, 28 undo
 * (and again: redo) the last big change, 29 tap tempo; -1 nothing. */
static const int8_t MIXK[NKEYS] = {
    0, 8, 1, 32, 2, 30, 28,                /* F3 .. B3: kick (mute, the black key above it solos), hats, snare (mutes; their black keys, 0.17.22: freeze the bass
                                            * (Pedal) and the acid line, the maker's idea for a Reddit player: a drum's solo was little use); undo */
    3, 31, 4, 12, 29,                      /* C4 .. E4: perc (mute; its black key freezes the pluck line), drone; tap tempo */
    5, 13, 6, 14, 16, 17, 7,               /* F4 .. B4: acid, pad, the drums, the pluck's mute */
    20, 21, 22, 23, 24, 25, 15, 27};       /* C5 .. G5: scenes 1-4, reset all, the auto-DJ (on, length, new pieces) */
static const char *const MIX_NAME[8] = {"KICK", "HATS", "SNARE", "PERC", "BASS", "ACID", "PAD", "PLUCK"};
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
#define FACTORY_ALL_HOLD 6000u         /* (0.17.23) held on this long, stopped: every stored piece of every genre to factory */

/* ---------------------------------------------------------------- themes --- */
/* seven palettes for the screen (Global > Theme; the layout, Console since 0.17.3, is the same in all of them): a ground, its panel and lines, the text, an accent for what is
 * happening now, a primary for the music in three shades (the pad and the chord; the acid; the drone),
 * the drums' colour, and a colour for the sections and freeze. Each layer has a dark tint for shapes at rest.
 * (0.17.40) The tints lean 10-14 degrees toward blue, as a shadow does (Phosphor's stay one phosphor); the ground,
 * panel and line stay apart once cut to 565 (tests/host/palette_test.py checks both). */
typedef struct {
    uint16_t bg, panel, line, dim, text, acc, pad, pad_t, acid, acid_t, bass, bass_t, pulse, pulse_t, frost, pluck;
} theme_t;
static const theme_t THEMES[EN_NTHEME] = {
    /* Console (0.17.3, the default): a desk's charcoal, amber for now, a colour a channel */
    {RGB(14, 15, 18), RGB(24, 26, 31), RGB(46, 49, 58), RGB(139, 142, 152), RGB(242, 239, 230), RGB(255, 176, 0),
     RGB(165, 139, 255), RGB(44, 44, 84), RGB(184, 245, 58), RGB(37, 70, 16), RGB(255, 90, 95), RGB(80, 26, 43),
     RGB(255, 154, 60), RGB(90, 36, 22), RGB(150, 210, 255), RGB(63, 216, 255)},
    /* Imp: warm black, bone, crimson, lime, pink; ember is for "now" (the playhead, the knob you touched) */
    {RGB(20, 14, 12), RGB(42, 30, 27), RGB(78, 56, 49), RGB(166, 140, 126), RGB(244, 232, 200), RGB(255, 212, 71),
     RGB(242, 230, 196), RGB(86, 72, 62), RGB(182, 242, 60), RGB(46, 80, 22), RGB(224, 52, 79), RGB(84, 22, 46),
     RGB(255, 143, 143), RGB(88, 52, 60), RGB(150, 210, 255), RGB(111, 224, 210)},
    /* Synth (Synth Imp, 0.14.18): demon synthwave: a night sky, crimson to magenta neon, a banded sun on the horizon behind the sigil, the imp in tube light */
    {RGB(12, 3, 24), RGB(28, 6, 32), RGB(69, 18, 63), RGB(154, 95, 134), RGB(255, 233, 242), RGB(255, 225, 74),
     RGB(192, 140, 255), RGB(50, 36, 96), RGB(182, 255, 46), RGB(39, 80, 16), RGB(255, 47, 95), RGB(84, 16, 47),
     RGB(255, 138, 61), RGB(88, 33, 24), RGB(150, 210, 255), RGB(62, 240, 255)},
    /* Acid: black, a 303's yellow-green, hot pink */
    {RGB(12, 13, 10), RGB(30, 34, 24), RGB(52, 60, 40), RGB(140, 152, 118), RGB(236, 242, 222), RGB(255, 60, 160),
     RGB(196, 255, 40), RGB(41, 72, 16), RGB(240, 255, 120), RGB(58, 80, 30), RGB(120, 200, 30), RGB(29, 62, 14),
     RGB(210, 220, 190), RGB(52, 62, 46), RGB(255, 90, 180), RGB(110, 230, 240)},
    /* Ice: deep navy, cyan, a cold pink */
    {RGB(10, 16, 28), RGB(22, 34, 54), RGB(38, 56, 84), RGB(120, 150, 186), RGB(226, 240, 255), RGB(255, 84, 130),
     RGB(0, 200, 255), RGB(10, 41, 84), RGB(120, 228, 255), RGB(26, 56, 96), RGB(40, 130, 230), RGB(16, 31, 82),
     RGB(196, 216, 236), RGB(48, 56, 86), RGB(255, 120, 160), RGB(255, 196, 110)},
    /* Ember: black-brown, red, amber */
    {RGB(20, 12, 10), RGB(40, 24, 20), RGB(70, 40, 34), RGB(170, 130, 116), RGB(250, 232, 220), RGB(255, 176, 32),
     RGB(255, 46, 66), RGB(92, 20, 43), RGB(255, 110, 90), RGB(96, 32, 41), RGB(200, 40, 40), RGB(72, 18, 31),
     RGB(230, 200, 186), RGB(82, 50, 51), RGB(255, 200, 80), RGB(110, 220, 200)},
    /* Phosphor: a green screen's black, its green, a brighter green */
    {RGB(6, 14, 8), RGB(14, 32, 18), RGB(26, 58, 32), RGB(80, 160, 96), RGB(170, 255, 190), RGB(220, 255, 120),
     RGB(60, 255, 120), RGB(14, 70, 30), RGB(150, 255, 170), RGB(24, 76, 40), RGB(30, 190, 90), RGB(10, 56, 26),
     RGB(120, 220, 140), RGB(20, 52, 28), RGB(240, 255, 140), RGB(200, 240, 150)},
};
static const theme_t *th = &THEMES[0];
#define LOOK_SYNTH (th == &THEMES[2])           /* the Synth theme has a look of its own: the horizon behind the sigil, the neon imp */
#define LOOK_CONSOLE 1                          /* 0.17.3: the Console layout is every theme's (the old HYP-1 frame's code is left for now, unused) */
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
    uint8_t af_fx, af_live, af_user, af_sec, af_said, af_last[2];   /* AUTO FX (af_tick): the effect planned, sounding now, one played by hand, the section seen,
                                                                  * its name shown, the last two */
    uint32_t af_start, af_end, af_bar, af_quiet, af_rng;   /* where it starts and ends (sixteenths, en_bars; end 0: none), the bar planned on, the bar it
                                                            * may plan again from (after a drop), its dice */
    uint8_t dj_break, dj_fillon;       /* its breakdown (0 short: 4 bars, 1 half the groove, 2 as long); a drum fill before it */
    uint8_t scene_quant, scene_q;      /* a scene waits for the next bar (1) or comes at once (0); the one waiting: slot + 1 */
    uint8_t key_set, key_on, sec_last; /* the drop's key change (0 same, 1 +2, 2 +3, 3 +5, 4 +7); it is on; the section last seen */
    uint8_t viz;                       /* the visualizer on the SELECT dial (VZ_*) */
    uint8_t centre_imp;                /* the sigil's middle: the eye (0) or the imp (SEL + HOME); kept with the visualizer's byte */
    uint8_t eu_n[EN_NLANE], eu_r[EN_NLANE];   /* the sequencer's Euclid tricks: each lane's hits and rotation, as last set */
    uint8_t flex_last;                 /* the Flex Beat pad pressed last (0..14) */
    uint8_t flex_lock;                 /* the Flex pattern that is latched (with the one FX_FLEX bit in fx_lock) */
    uint8_t flex_cur;                  /* the pattern the engine was last told to play (en_flex_select) */
    uint8_t saver, lights, saver_on, saver_view, saver_viz;   /* the STAGE page: the screensaver (off, 1, 3, 10 minutes), the key lights (off, show); it is on; where it came from */
    uint8_t usb_fixed;                           /* the USB page: 1, the USB audio at a fixed level, not the master knob's */
    uint8_t form;
    uint8_t start_rand;                          /* the Settings page's Start: 0 Last, 1 Random (the global record's rsv, bit 8: 0.16.26) */                                /* the Settings page's Form: 0 Song, 1 Loop (kept in the global record's last byte, bit 2: 0.16.26) */
    uint8_t fm_dy;                               /* (the Settings page's Voices until 0.17.75: kept for uiv(13)'s place, nothing reads it) */
    uint8_t scan;                                /* Scanlines: every other screen row darker (set_t.usb's bit 2) */
    uint8_t midi_map;                            /* MIDI out's map (EN_MAP_*: GM, Model:Samples, MPC Sample, SP-404; set_t.usb's bits 12-14) */
    uint8_t min_pads, min_ch;                    /* MIDI in: 1, a pad controller's notes play PERF (pads_in; set_t.usb's bit 15); 2, a Model:Samples' channels do (ms_in;
                                                  * set_t.viz's bit 3); 3, an Akai MPK mini (mpk_*; bit 6); the channel listened to, 0 all (set_t.uiv[11]) */
    uint8_t cc_mode;                             /* the FX knobs as CCs (0.15.22): bit 0 in, bit 1 out (set_t.viz's bits 4-5) */
    int8_t cc_sent[4];                           /* the FX knobs as last sent (or received: not sent back) */
    uint8_t usb_host;                            /* the USB port is the host of a USB-MIDI device (plat_usb_host); never kept */
    uint32_t min_last, min_n;                    /* MIDI in: the last message (not clock), as a USB-MIDI packet, and how many (the MIDI IN page shows them) */
    uint8_t pad_note[48];                        /* the notes the melodic pads started (banks B-D), for their note-offs */
    uint32_t saver_t;
    float glide_f[4];                  /* a scene's FX knobs gliding: where they are, where they go, which */
    int8_t glide_t[4];
    uint8_t glide_on, glide_busy;
    uint8_t vox_dirty;                 /* the player's DX7 voices changed: written when you leave them alone a moment (voices_save) */
    uint8_t set_dirty;                 /* the effects' / performing settings changed: written when you leave them alone a moment */
    uint8_t set_lazy;                  /* a cosmetic setting changed (the visualizer, the sigil): written when the music is stopped or with the next write, never as a dip in the music of its own */
    uint32_t fx_open_t;                /* when the FX menu was opened (a second tap soon after goes to the settings) */
    uint8_t fx_lastpg;                 /* the edit page of the effect key pressed last */
    uint8_t mix_trk;                   /* the mixer page's layer: pad, drone, acid, pluck, drums (0..4) */
    uint8_t gr_all;                    /* the groove page's all-lanes density, as last set */
    int8_t pat_key;                    /* a pattern key (SEL + white 1-6) held: a tap keeps the pattern, a hold plays it only while down */
    uint8_t pat_rev, pat_held, pat_up; /* the pattern to go back to (+1), the one held, and the key let go after a hold */
    uint32_t pat_t;
    uint8_t scene_g[4][13];            /* a scene's groove: the lanes' densities, their sources, the pattern */
    uint8_t jam_man;                   /* the voices taken off their line by hand (a bit a JAM voice: the JAM page's, a phrase's; 0.17.45) */
    uint8_t jam_voice, jam_mode, jam_oct, jam_ans;   /* the JAM page: the voice (pluck, pad, acid, drone), the keys (scale, chords, harp), the octave (0..4: -2..+2), the answer */
    uint8_t jam_n[NKEYS], jam_vc[NKEYS], jam_note[NKEYS][4];   /* what each key is sounding, to let go */
    uint16_t cap_t[32];                          /* Capture's memory (SEL + REC on Jam): the last 32 keys played: the sixteenth (en_bars), the note heard, the key and voice, how long it was held (sixteenths; 0: still down) */
    uint8_t cap_note[32], cap_kv[32], cap_len[32], cap_w;
    uint8_t cap_deg[32];                         /* (0.17.31) and the chord each was played over (en_deg) */
    uint32_t home_t;                   /* when HOME last went down (twice quickly: the visualizers) */
    uint8_t mood;                      /* the mood macro, 0 (chill) .. 100 (peak); 50 leaves the piece as it is */
    uint8_t dj_phase, dj_fill;         /* groove 0, breakdown 1, build 2; a fill in the bar before the breakdown */
    uint8_t dj_rounds, dj_ramp, intro; /* the SET page: rounds a piece gets (1, 2, 4, 8), the tempo's climb over them, the intro (kept in set_t.usb's bits 3 ..) */
    uint8_t dj_round;                  /* the round the piece is in (from 0) */
    int16_t dj_t0, dj_tlast;           /* the ramp: the piece's tempo at its start, and the last tempo the ramp set (another: the player turned it, so it starts again from there) */
    int16_t mx_from, mx_to, mx_last;   /* the mix (0.16.1, the SET page's Mix): a piece on the bar starts at the tempo playing and glides to its own */
    uint8_t mx_bars, mx_n;             /* over mx_bars bars (0: no glide), mx_n of them gone */
    uint8_t smp_mode, smp_fx, smp_gap; /* the SAMPLE page (0.16.1): what it plays (all, drums, synths, by key), with the effects or dry, the gap between sounds */
    uint8_t smp_dry, smp_vc, smp_nn, smp_note[3];   /* dry now; the voice and notes held */
    int8_t smp_cur;                    /* the sound playing (-1: none) */
    uint32_t smp_t0, smp_off;          /* the run's start (0: none); when the held notes let go */
    uint16_t dj_n;                     /* bars in the phase */
    uint64_t fx_mask;                  /* the effects held (and locked), as last sent */
    uint64_t fx_lock;                  /* the effects latched on */
    uint8_t lock_mode;                 /* (the old SAVE lock mode: gone, kept off) */
    uint32_t lock_keys;                /* keys that turned a latched effect off: not played while they're held */
    uint32_t fx_tap;                   /* FX keys down that may yet be a tap (a latch) */
    uint32_t fx_down_t[NKEYS];
    uint8_t lane;                      /* the sequencer's lane (OCT- / OCT+) */
    uint8_t stp;                       /* the Steps page's step (a white key) */
    uint8_t astp;                      /* the Acid steps page's step (a white key) */
    uint8_t pstp;                      /* the Pluck steps page's */
    uint8_t bstp;                      /* the Bass steps page's (0.16.29) */
    uint8_t pgsl;                      /* the Progression page's slot */
    uint8_t slsl, sl_pos, sl_q;        /* the set list: the slot chosen; the one playing (0xFF none); one waiting for the bar (+ 1) */
    uint8_t as_off[16], as_oct[16];    /* its working copy of the line while it is edited (the engine takes the edits a chunk later) */
    uint16_t as_gate, as_acc, as_slide;
    uint32_t as_t;                     /* when it was last edited */
    uint8_t save_global;               /* SAVE went down on Global: the piece is stored when it comes up */
    uint8_t store_wait;                /* a store while playing: the music fading out to write it (store_tick) */
    uint8_t store_kind;                /* and what: 0 the piece's, 1 the settings, 2 the globals (quiet_write) */
    uint32_t store_t;
    char store_msg[12];
    uint8_t factory_done;              /* SEL + SAVE held on Global: the piece went back to its factory settings */
    uint8_t save_fact;                 /* SAVE went down on Global with SEL held: a factory reset if held on, never a store */
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
    uint32_t touch_until, tick_until;  /* (0.17.40 tick_until: a step just made, the touched knob a moment brighter) */
    uint32_t imp_until;                /* (0.17.40) the imp in the Console header: a drop, a break, a build, for a moment */
    uint8_t imp_sec;                   /* (the section it last came up for) */
    uint8_t fade_view;                 /* (0.17.40) the page the main band last showed, and when it changed (main_blit) */
    uint32_t fade_t;
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
/* the gesture recorder's (0.17.31; below, at gesture_tick) */
static int8_t gs_v[64][4], gs_a[4], gs_home[4];   /* the knobs a sixteenth; as they were when armed; before AUTO FX played it */
static uint8_t gs_fx[64];                        /* (0.17.44) the effect held a sixteenth: its bit + 1, 0 none */
#define AF_GS 0xFFu                              /* AUTO FX's plan: your gesture (0.17.44), not an effect */
static uint8_t gs_st, gs_len, gs_bar, gs_pos, gs_stop, gs_mask, gs_busy, gs_w, gs_hand, gs_fxnow, gs_shot;   /* gs_st: 0 off, 1 armed, 2 recording, 3 looping,
                                                 * 4 kept for AUTO FX (Pick: YOURS); gs_hand: the effect played by hand now (bit + 1); gs_fxnow: the one the
                                                 * gesture plays now; gs_shot: AUTO FX is playing it */
static uint32_t gs_t;
/* the phrase recorder's (0.17.45; below, at phrase_key): 0 off, 1 armed, 2 recording, 3 playing; the JAM voice; the bar it began (in sixteenths); the
 * bar SEL + REC ended it in (+1, 0: not yet); each key's step while held (+1) */
static uint8_t ph_st, ph_vc, ph_ks[NKEYS];
static uint32_t ph_b0, ph_endb;
static void phrase_key(void);
static void ph_close(uint32_t len, int late);
static void phrase_tick(void);

/* TRACK CODES (0.17.11): nine letters for a track: its genre, its piece (as the recipe), its key, mode and tempo, and the seed every
 * generator starts from (a check letter last). A start from one plays the same track, note for note, from the top: on this firmware,
 * hands off (a knob turned, a key played, the auto-DJ's moves change it from there). PLAY starts a fresh track, a new code each time;
 * the Code page (GLO, third) shows the one playing, takes one typed in (K1 the place, K2 the letter), rolls a new one (K3); PLAY
 * there starts it (or the one playing again, from the top). A piece changed from its recipe: its code gives the recipe ("EDITED"). */
#define CODE_N 9
#define CODE_M40 0xFFFFFFFFFFull
#define CODE_MUL 0x5DEECE66Dull                  /* odd: the bits scrambled (a seed's neighbour looks nothing like it) */
static const char CODE_ABC[33] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
typedef struct { uint8_t genre, piece, root, mode, tempo; uint16_t seed; } tcode_t;
static uint16_t code_seed;                       /* the track playing's seed */
static uint8_t af_fresh;                         /* a start from the top is on its way: AUTO FX waits for its first bar (the bars before it are the old track's) */
static uint8_t code_in[CODE_N], code_pos, code_typed, code_roll = 128, code_clean;   /* the Code page's letters (typed: not the one playing), the place; the start was the recipe's */
static uint16_t code_pats[EN_NPAT][EN_NLANE];    /* the drum patterns as the track started (drum drift changes them as it plays; PLAY starts from these) */
static uint64_t code_inv(uint64_t a)
{
    uint64_t x = a;
    int i;
    for (i = 0; i < 6; i++)
        x *= 2u - a * x;
    return x;
}
static int code_check(const uint8_t *g)          /* the check letter: odd weights, so any one letter wrong shows */
{
    int i, c = 7;
    for (i = 0; i < CODE_N - 1; i++)
        c += (2 * i + 1) * g[i];
    return c & 31;
}
static void code_pack(const tcode_t *t, uint8_t *g)
{
    uint64_t v = (uint64_t)t->genre | (uint64_t)t->piece << 5 | (uint64_t)t->seed << 9 | (uint64_t)t->root << 25 |
                 (uint64_t)t->mode << 29 | (uint64_t)(t->tempo - 70) << 33;
    int i;
    v = v * CODE_MUL & CODE_M40;
    v ^= v >> 20;
    for (i = 0; i < CODE_N - 1; i++)
        g[i] = (uint8_t)(v >> (35 - 5 * i) & 31u);
    g[CODE_N - 1] = (uint8_t)code_check(g);
}
static int code_unpack(const uint8_t *g, tcode_t *t)   /* 0: a code of this firmware's */
{
    uint64_t v = 0;
    int i;
    if (code_check(g) != g[CODE_N - 1])
        return -1;
    for (i = 0; i < CODE_N - 1; i++)
        v = v << 5 | g[i];
    v ^= v >> 20;
    v = v * code_inv(CODE_MUL) & CODE_M40;
    t->genre = (uint8_t)(v & 31u);
    t->piece = (uint8_t)(v >> 5 & 15u);
    t->seed = (uint16_t)(v >> 9);
    t->root = (uint8_t)(v >> 25 & 15u);
    t->mode = (uint8_t)(v >> 29 & 15u);
    t->tempo = (uint8_t)((v >> 33 & 127u) + 70u);
    return t->genre < EN_NGENRE && t->piece < NPIECES && t->root < 12 && t->mode < EN_NMODES && t->tempo <= 190 ? 0 : -1;
}
static void code_text(const uint8_t *g, char *b)    /* "XXXX-XXXXX" */
{
    int i, n = 0;
    for (i = 0; i < CODE_N; i++) {
        if (i == 4)
            b[n++] = '-';
        b[n++] = CODE_ABC[g[i] & 31];
    }
    b[n] = 0;
}
static void code_now(uint8_t *g)                 /* the track playing's */
{
    tcode_t t;
    t.genre = proj.genre;
    t.piece = proj.piece;
    t.seed = code_seed;
    t.root = (uint8_t)proj.par[P_ROOT];
    t.mode = (uint8_t)proj.par[P_MODE];
    t.tempo = (uint8_t)proj.par[P_TEMPO];
    code_pack(&t, g);
}
static int code_par_match(void)                  /* the piece's settings as its recipe's (the key, mode and tempo aside: the code has them) */
{
    int16_t r[P_NPARAMS];
    uint16_t pool;
    int p;
    en_piece_g(proj.genre, proj.piece, r, &pool);
    for (p = 0; p < P_NPARAMS; p++)
        if (!en_par_sticky(p) && p != P_TEMPO && p != P_ROOT && p != P_MODE && p != P_PATTERN && proj.par[p] != r[p])
            return 0;   /* (the pattern playing is where the song is, not a setting) */
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE] % EN_NMODES];
}
static int code_exact(void) { return code_clean && code_par_match(); }
static void code_pats_keep(void)                 /* a pattern edited by hand: the start keeps it */
{
    int p, l;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            code_pats[p][l] = en_pats[p][l];
}
static void track_fresh(int roll, int taken)     /* the track from its start: a new seed (roll), the patterns as taken up (taken) or as the last start had them */
{
    static uint16_t rp[EN_NPAT][EN_NLANE];
    static uint8_t rm[EN_NLANE][16];
    int p, l, same = 1;
    if (roll)
        code_seed = (uint16_t)(((plat_ms() + 1u) ^ (uint32_t)code_seed * 40503u) * 2654435761u >> 16);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++) {
            if (taken)
                code_pats[p][l] = en_pats[p][l];
            else
                en_pats[p][l] = code_pats[p][l];
        }
    en_piece_pats_g(proj.genre, proj.piece, rp);
    en_piece_mods_g(proj.genre, proj.piece, rm);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            same &= code_pats[p][l] == rp[p][l];
    for (l = 0; l < EN_NLANE; l++)
        for (p = 0; p < 16; p++)
            same &= en_mod[l][p] == rm[l][p];
    for (p = 0; p < EN_PRG_LEN; p++)
        same &= proj.prg[p] == 0;
    for (p = 0; p < EN_PRK_LEN; p++)
        same &= proj.prk[p] == 0;
    for (p = 0; p < 16; p++)
        same &= proj.bs[p] == 0;
    code_clean = (uint8_t)same;
    proj.par[P_PATTERN] = proj.slot[proj.piece].par[P_PATTERN];   /* from the piece's first pattern (drum drift moves on from it) */
    en_set(P_PATTERN, proj.par[P_PATTERN]);
    en_seed_exact(code_seed);
    en_regrow();
    ui.af_rng = (uint32_t)code_seed * 2654435761u | 1u;   /* AUTO FX from the start too: its dice, nothing planned */
    ui.af_end = ui.af_start = ui.af_quiet = 0;
    ui.af_bar = 0;
    ui.af_live = ui.af_said = ui.af_last[0] = ui.af_last[1] = 0;
    ui.af_sec = EN_SEC_GROOVE;
    code_typed = 0;
}
/* THE SET LIST (0.15.21): sixteen slots, each a genre's piece (glob.sl, kept with the player's three). With Play on, the auto-DJ's next piece is
 * the list's next slot (across genres), and PRESETS steps through the list instead of the bank (with SEL: on the next bar). */
static int sl_find(void)                        /* the slot of the piece playing, or 0xFF */
{
    const int e = proj.genre * NPIECES + proj.piece + 1;
    int s;
    if (ui.sl_pos < SL_N && glob.sl[ui.sl_pos] == e)
        return ui.sl_pos;
    for (s = 0; s < SL_N; s++)
        if (glob.sl[s] == e)
            return s;
    return 0xFF;
}
static int sl_step(int from, int d)             /* the slot set after (d 1) or before (d -1) slot from (0xFF: from the start), round the list; -1 none */
{
    int s, i = from < SL_N ? from : d > 0 ? SL_N - 1 : 0;
    for (s = 0; s < SL_N; s++) {
        i = (i + d + SL_N) % SL_N;
        if (glob.sl[i])
            return i;
    }
    return -1;
}
static int sl_play(void) { return glob.sl_on && sl_step(0xFF, 1) >= 0; }
/* SONG MODE (0.17.13, stage 1): a place can have a length in bars and parts left out. With Play on, a place with bars plays exactly that many (from the bar
 * it came in on), then the next comes in on its last step; Once stops at the list's end (back on the first, ready), Loop goes round. Its parts as set:
 * the others muted while it plays. Auto (0 bars): the auto-DJ's rounds, as before. */
#define SL_NBARS 11
#define SL_NPART 5
static const uint8_t SL_BARS[SL_NBARS] = {0, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128};
static const char *const SL_PART[SL_NPART] = {"PAD", "BASS", "ACID", "PLUCK", "DRUMS"};
static const uint8_t SL_PARTM[SL_NPART] = {0x40, 0x10, 0x20, 0x80, 0x0F};   /* their tracks (ui.trk_m) */
static uint32_t sl_b0;                          /* the bar the place playing came in on */
static uint8_t sl_stop;                         /* the list's end: stop on this bar's last step */
static uint8_t sl_fresh;                        /* PLAY pressed: the engine's bars are the old run's until it starts (wait for them to come round) */
static uint32_t sl_bar_now(void) { return en_bars ? (en_bars - 1u) >> 4 : 0u; }   /* the bar playing (en_bars counts the steps played) */
static int sl_bars_i(int b)
{
    int i;
    for (i = SL_NBARS - 1; i > 0 && SL_BARS[i] > b; i--)
        ;
    return i;
}
static int sl_exact(void) { return sl_play() && ui.sl_pos < SL_N && glob.sl_bars[ui.sl_pos]; }
static void mood_apply(void);
static void sl_parts(int s)                     /* place s's parts: the ones it leaves out muted */
{
    uint8_t m = 0;
    int i;
    for (i = 0; i < SL_NPART; i++)
        if (glob.sl_mute[s % SL_N] >> i & 1u)
            m |= SL_PARTM[i];
    ui.trk_m = m;
    ui.trk_s = 0;
    en_trk(m, 0);
    if (glob.sl_pat[s % SL_N]) {                /* (0.17.14) its pattern (from the next bar) */
        proj.par[P_PATTERN] = (int16_t)(glob.sl_pat[s % SL_N] - 1);
        en_set(P_PATTERN, proj.par[P_PATTERN]);
    }
    if (glob.sl_mood[s % SL_N]) {               /* and its energy */
        ui.mood = (uint8_t)(glob.sl_mood[s % SL_N] - 1);
        mood_apply();
    }
}
static void sl_start(void)                      /* a start from the top (PLAY, power-on, a code): the place playing's bars count from it, its parts, pattern, energy */
{
    sl_b0 = 0;
    sl_fresh = 1;
    if (sl_play() && ui.sl_pos < SL_N)
        sl_parts(ui.sl_pos);
}
static int sl_differs(int a, int b)             /* places a and b play something different (the piece, its parts, pattern or energy) */
{
    return glob.sl[a] != glob.sl[b] || glob.sl_mute[a] != glob.sl_mute[b] || glob.sl_pat[a] != glob.sl_pat[b] || glob.sl_mood[a] != glob.sl_mood[b];
}
/* (0.17.61, the 1.0 list: song mode that sounds arranged) how place a hands over to place b: 2 a lift (b brings the drums in, or its energy is a good
 * step up: a build over a's last bars, the drop on b's first), 1 a fall (b leaves the drums out: it comes in as a breakdown, the groove letting go on
 * the one), 0 neither (a fill, as before) */
static int sl_drums(int s) { return !(glob.sl_mute[s % SL_N] >> 4 & 1u); }
static int sl_turn(int a, int b)
{
    if (sl_drums(b) && (!sl_drums(a) || (glob.sl_mood[a] && glob.sl_mood[b] >= glob.sl_mood[a] + 25)))
        return 2;
    return sl_drums(a) && !sl_drums(b) ? 1 : 0;
}
static void sl_tick(void)                       /* each frame while playing: the place's last bar asks for the next */
{
    static uint32_t fill_at = 0xFFFFFFFFu, bld_at = 0xFFFFFFFFu;   /* the bar a fill was asked in (once a place); the place a build was asked in */
    uint32_t bar;
    int n, s;
    en_arrange = (uint8_t)!sl_exact();          /* (0.17.16) a place with its own bars is arranged by the player: Song's own rests off */
    if (sl_fresh) {
        if (!en_playing || en_bars >= 16u)
            return;
        sl_fresh = 0;
    }
    if (!en_playing || !sl_exact() || ui.sl_q || sl_stop || sl_bar_now() < sl_b0)
        return;
    bar = sl_bar_now() - sl_b0;
    n = glob.sl_bars[ui.sl_pos];
    {
        const int nx = sl_step(ui.sl_pos, 1), go = nx >= 0 && !(glob.sl_end && nx <= (int)ui.sl_pos) && sl_differs(ui.sl_pos, nx);
        const int bl = n / 2 < en_build_len() ? n / 2 : en_build_len();   /* (0.17.61) the build: the genre's, at most half the place */
        if (go && sl_turn(ui.sl_pos, nx) == 2 && bl >= 1) {
            if ((int)bar == n - bl - 1 && bld_at != sl_b0) {   /* the bar before it: asked (it starts on the bar, and ends with the drop on the next place's first) */
                bld_at = sl_b0;
                en_build_bars(bl);
            }
            if ((int)bar >= n - bl - 1 && (ui.trk_m & SL_PARTM[4]) && ((int)bar > n - bl - 1 || en_drum_pos == EN_NSTEP - 1)) {
                ui.trk_m &= (uint8_t)~SL_PARTM[4];   /* a place without its drums: they come in for the build (its roll is how they enter) */
                en_trk(ui.trk_m, ui.trk_s);
            }
        } else if ((int)bar == n - 2 && fill_at != sl_b0 + bar) {   /* (0.17.16) the bar before the last: a fill into the next place, if it changes anything */
            fill_at = sl_b0 + bar;
            if (go)
                en_fill_next();
        }
    }
    if ((int)bar < n - 1)
        return;
    s = sl_step(ui.sl_pos, 1);
    if (glob.sl_end && s <= (int)ui.sl_pos)
        sl_stop = 1;                            /* Once: the end */
    else if (s >= 0)
        ui.sl_q = (uint8_t)(s + 1);
}

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static const uint8_t KEY_SH[5] = {0, 2, 3, 5, 7};   /* the drop's key change: semitones up */
static int sound_root(void)                     /* the root that sounds: the piece's, or while the drop's key change is on, moved up (the keys, the ring and JAM follow it) */
{
    return ui.key_on ? (proj.par[P_ROOT] + KEY_SH[ui.key_set % 5]) % 12 : proj.par[P_ROOT];
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)sound_root();
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
           ((ui.btn_used >> B_GLO & 1u) || plat_ms() - ui.down_t[B_GLO] >= 400u);
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
    if (c == 30 || c == 31)                    /* the acid's, the pluck's line frozen */
        return proj.par[c == 30 ? P_AC_LOCK : P_CM_LOCK] != 0;
    if (c == 32)                               /* the bass on Pedal */
        return glob.sl_rsv[2] & 1u;
    return c == 16 ? (ui.trk_m & 15u) == 15u : c == 17 ? (ui.trk_s & 15u) == 15u : 0;
}
static void knob_set(int k, int v);
static int knob_get(int k);
static void knob_range(int k, int *lo, int *hi);
static void knob_text(int k, char *b);
static void jam_capture(void);
static void jam_capture_v(int vc, int bar, int selacc);
static void gesture_key(void);
static void acid_keep(void);
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
static int dj_break_len(int g)                  /* the breakdown's bars: short, half the groove or full, then made so that it and the genre's build end on an
                                                 * 8-bar line (0.15.13: groove 16 + break 8 + build 4 was 28 bars, and a DJ mixing in lost the grid) */
{
    const int b = ui.dj_break == 0 ? 4 : ui.dj_break == 1 ? (g / 2 < 4 ? 4 : g / 2) : g, bl = en_build_len();
    const int t = (b + bl + 4) / 8 * 8;          /* the nearest multiple of 8 (a tie goes up) */
    return t - bl >= 2 ? t - bl : t + 8 - bl;
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
        ui.dj_round = 0;
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
    } else if (c == 30 || c == 31) {           /* the acid's or the pluck's line frozen as it is, or let go (as on its steps page; stored with the piece) */
        const int p = c == 30 ? P_AC_LOCK : P_CM_LOCK, on = !proj.par[p];
        knob_set(p, on);
        say(c == 30 ? "ACID LINE" : "PLUCK LINE", on ? "FROZEN" : "FREE");
    } else if (c == 32) {                      /* the bass held on the key while the chords move (Pedal, as on the Progression page) */
        glob.sl_rsv[2] = (uint8_t)((glob.sl_rsv[2] ^ 1u) & 0x3Fu);   /* (bit 3 is Calm's, 0.17.40; 4-5 the rests', 0.17.70) */
        en_pedal(glob.sl_rsv[2] & 3);
        project_globals();
        say("BASS", glob.sl_rsv[2] & 1u ? "PEDAL: HOLDS THE KEY" : "FOLLOWS THE CHORDS");
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
    if (ui.view == V_SETL && (ui.btn >> B_SEL & 1u) && (i == 1 || i == 2))
        return i == 1 ? K_SLPT : K_SLEN;        /* (0.17.14) SEL held: the place's pattern and energy */
    return VIEW_KNOB[ui.view][i];
}
static const char *view_label(int i)
{
    if (ui.view == V_JAM && !ui.fx_menu)
        return FXP[NFXP - 1].c[i].label;
    if (ui.fx_menu)
        return ui.fx_page ? FXP[ui.fx_page - 1].c[i].label : FXK_LABEL[i];
    if (ui.view == V_SETL && (ui.btn >> B_SEL & 1u) && (i == 1 || i == 2))
        return i == 1 ? "Pattern" : "Energy";
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
/* THE PIECE'S EFFECT SETTINGS (0.16.28): the FILTERS page's types and SCREAM's are the piece's, kept with it (project.c PX_FXS) */
void ui_fx_snapshot(uint8_t *v)
{
    int k;
    for (k = 0; k < PX_NFX; k++)
        v[k] = ui.fxs[PX_FXS[k]];
}
/* each one's top: the FILTERS page's three types (the pad's and the bass's five, the acid's six), SCREAM's Drive, Scream, Wow (0-100) and Target (3) */
static const uint8_t PX_FXTOP[PX_NFX] = {4, 4, 5, 100, 100, 100, 2, 6, 9};
void ui_fx_apply(void)
{
    uint8_t v[PX_NFX];
    int k;
    project_fx_get(v);
    for (k = 0; k < PX_NFX; k++)
        if (v[k] > PX_FXTOP[k])                  /* (0.17.26) a shared or damaged piece's byte out of range: its top, not a read past a name table */
            v[k] = PX_FXTOP[k];
    for (k = 0; k < PX_NFX; k++)
        if (ui.fxs[PX_FXS[k]] != v[k]) {
            ui.fxs[PX_FXS[k]] = v[k];
            en_fxs(PX_FXS[k], v[k]);
        }
}
static uint8_t *uiv(int i)                     /* the PERFORM page's settings (the UI's own) */
{
    return i == 0 ? &ui.scene_quant : i == 1 ? &ui.dj_len : i == 2 ? &ui.dj_break : i == 3 ? &ui.dj_fillon : i == 4 ? &ui.key_set :
           i == 5 ? &ui.saver : i == 6 ? &ui.lights : i == 7 ? &ui.jam_voice : i == 8 ? &ui.jam_mode : i == 9 ? &ui.jam_oct : i == 10 ? &ui.jam_ans :
           i == 12 ? &ui.usb_fixed : i == 13 ? &ui.fm_dy : i == 14 ? &ui.scan : i == 15 ? &ui.dj_rounds : i == 16 ? &ui.dj_ramp : i == 17 ? &ui.intro :
           i == 18 ? &ui.midi_map : i == 19 ? &ui.min_ch : i == 20 ? &ui.min_pads : i == 21 ? &ui.usb_host : i == 22 ? &ui.cc_mode :
           i == 23 ? &glob.sl_rsv[0] : i == 24 ? &glob.sl_rsv[1] : i == 25 ? &ui.smp_mode : i == 26 ? &ui.smp_fx : i == 27 ? &ui.smp_gap : i == 28 ? &ui.form : i == 29 ? &ui.start_rand : &proj.genre;   /* 23 zoom, 24 the mix (0.16.1: the global record's spare bytes) */   /* 11: the genre (proj's own); 12: the USB audio's level, 13: the voice set, 14: scanlines, 15-17 the SET page, 18 the MIDI map (kept in set_t.usb) */
}
#define FXP_UI(k) (FXP[((k) - K_FXP) / 4].bit == 63)   /* the PERFORM page: no effect, the UI's own settings */
static int fxp_get(int k)
{
    switch (FXP_CTL(k).kind) {
    case KN_PAR: return knob_get(FXP_CTL(k).idx);
    case KN_PRESET: return ui.gx_preset;
    case KN_TGT: return ui.gx_tgt;
    case KN_PTGT: return ui.pm_tgt;
    default: return FXP_UI(k) ? *uiv(FXP_CTL(k).idx) : ui.fxs[FXP_CTL(k).idx];
    }
}
static void fxp_range(int k, int *lo, int *hi)
{
    const int kind = FXP_CTL(k).kind;
    if (kind == KN_PAR) {
        knob_range(FXP_CTL(k).idx, lo, hi);
        return;
    }
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
    if (v != fxp_get(k) && FXP_CTL(k).kind != KN_PAR)
        ui.set_dirty = 1;                        /* a setting changed: written when it has been left alone a moment (settings_save) */
    switch (FXP_CTL(k).kind) {
    case KN_PAR:                                 /* an engine parameter: the project's, the dial pages' way */
        knob_set(FXP_CTL(k).idx, v);
        break;
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
            if (FXP_CTL(k).idx == 12)
                en_usb_fixed = (uint8_t)v;       /* the engine renders the USB audio at unity when a computer is recording */
            else if (FXP_CTL(k).idx == 23 || FXP_CTL(k).idx == 24)
                project_globals();               /* (zoom and the mix: the global record, stored at once) */
            else if (FXP_CTL(k).idx == 29)
                project_start_random_set(v);
            else if (FXP_CTL(k).idx == 14 && (glob.sl_rsv[2] >> 3 & 1u) != (unsigned)(v >> 1 & 1)) {   /* (0.17.40) Look's Calm: the global record's bit 3 */
                glob.sl_rsv[2] = (uint8_t)((glob.sl_rsv[2] & ~8u) | (v & 2 ? 8u : 0u));
                project_globals();
            }
            else if (FXP_CTL(k).idx == 28) {     /* Form: the engine at once, the global record's bit 2 (Loop) and (0.17.70) bits 4-5 (the rests) */
                en_song = (uint8_t)(v != 3);
                en_rests = (uint8_t)(v < 3 ? 2 - v : 2);
                glob.sl_rsv[2] = (uint8_t)((glob.sl_rsv[2] & ~0x34u) | (v == 3 ? 4u : (unsigned)v << 4));
                project_globals();
            }
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
    if (FXP_CTL(k).kind == KN_PAR) {
        knob_text(FXP_CTL(k).idx, b);
        return;
    }
    if (FXP_CTL(k).kind == KN_PRESET)
        s = GX_PRE[v].name;
    else if (FXP_CTL(k).kind == KN_TGT || FXP_CTL(k).kind == KN_PTGT)
        s = GX_TGT[v].name;
    else if (FXP_CTL(k).kind == KN_NAME)
        s = FXP_CTL(k).idx == EN_FXS_FT_PAD && !en_pad_filtered() ? "NO FILTER" : FXP_CTL(k).names[v < FXP_CTL(k).n ? v : FXP_CTL(k).n - 1];   /* (0.17.18) the pad's sound has none; (0.17.26) never past the table */
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
/* the LFOs' Rate knobs (0.17.10): the rates in order of speed (the engine's numbers keep their old order, so stored pieces mean the same) */
#define NLRT 16
static const uint8_t LRT_ORDER[NLRT] = {11, 10, 9, 8, 12, 13, 0, 14, 1, 15, 2, 3, 4, 5, 6, 7};   /* 1/16T 1/16 1/8T 1/8 1/4T 1/8. 1/4 1/4. 1/2 1/2. 1 .. 32 bars */
static int lrt_pos(int v)
{
    int i;
    for (i = 0; i < NLRT; i++)
        if (LRT_ORDER[i] == v)
            return i;
    return 6;
}
static int knob_par(int k)                     /* a knob's parameter (the sequencer's: its lane's) */
{
    if (k == K_PLRT || k == K_DLRT)
        return k == K_PLRT ? P_PL_RATE : P_DL_RATE;
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
    if (k == K_PCOND)
        return P_COND + (int)(ui.lane % EN_NLANE);
    if (k == K_MXP)
        return P_PAN + (int)(ui.mix_trk % 5u);
    if (k == K_MXS)
        return P_SEND + (int)(ui.mix_trk % 5u);
    if (k == K_JSND) {                           /* JAM's fourth knob (0.17.4): the sound of the voice it plays (pluck, pad, acid, bass) */
        static const uint8_t JS[4] = {P_CM_SND, P_PADTONE, P_AC_WAVE, P_DR_INST};
        return JS[ui.jam_voice & 3u];
    }
    return k < P_NPARAMS ? k : -1;
}
/* the acid steps' working copy: while the player edits, the page works on its own copy (the engine takes an edit a chunk later); otherwise it reads the engine's */
static void as_sync(void)
{
    int s;
    if (plat_ms() - ui.as_t < 400u)
        return;
    for (s = 0; s < 16; s++) {
        ui.as_off[s] = (uint8_t)(en_acid_off[s] & 15);
        ui.as_oct[s] = (uint8_t)(en_acid_oct[s] & 3);
    }
    ui.as_gate = en_acid_gate;
    ui.as_acc = en_acid_acc;
    ui.as_slide = en_acid_slide;
}
static int as_kind(int s)                       /* 0 rest, 1 note, 2 accent, 3 slide, 4 accent and slide */
{
    const int g = ui.as_gate >> s & 1, a = ui.as_acc >> s & 1, sl = ui.as_slide >> s & 1;
    return !g ? 0 : a ? (sl ? 4 : 2) : (sl ? 3 : 1);
}
static void as_put(int s, int note, int oct, int kind)
{
    const uint16_t m = (uint16_t)(1u << s);
    ui.as_off[s] = (uint8_t)note;
    ui.as_oct[s] = (uint8_t)oct;
    ui.as_gate = (uint16_t)(kind ? ui.as_gate | m : ui.as_gate & ~m);
    ui.as_acc = (uint16_t)(kind == 2 || kind == 4 ? ui.as_acc | m : ui.as_acc & ~m);
    ui.as_slide = (uint16_t)(kind == 3 || kind == 4 ? ui.as_slide | m : ui.as_slide & ~m);
    ui.as_t = plat_ms();
    en_acid_step(s, note, oct, kind != 0, kind == 2 || kind == 4, kind == 3 || kind == 4);
    knob_set(P_AC_LOCK, 1);                     /* an edited line is the player's: held, and stored with the piece */
    edited();
}
/* the Progression page (0.15.6): proj.prg is the piece's; while none of its slots is set, the page shows the chords heard last (the genre's walk),
 * and the first edit makes them the piece's */
static int pg_any(const uint8_t *b)              /* the piece's own: a slot set, or Play on Loop (all of them emptied by hand) */
{
    int s;
    if (en_prg_get(b, EN_PRG_PLAY, 1))
        return 1;
    for (s = 0; s < EN_PRG_N; s++)
        if (en_prg_get(b, 3 * s, 3))
            return 1;
    return 0;
}
static const uint8_t *pg_view(uint8_t *tmp)
{
    if (pg_any(proj.prg))
        return proj.prg;
    memcpy(tmp, proj.prg, EN_PRG_LEN);
    en_prog_capture(tmp);
    return tmp;
}
/* the Circle fills (0.16.20, after issues #2 and #3: progressions that move round the circle of fifths, saved like any): the first five
 * black keys on the Progression page write one into the slots, each chord a fifth down from the one before as the scale allows (its
 * degrees: in minor ii is ii o and V v, or V where the genre cadences): ii V I I, vi ii V I, iii vi ii V I I, the turnaround I vi ii V,
 * and the whole diatonic circle I IV vii iii vi ii V I. Black keys 6-11 (0.16.21, from Steven's chord chart, issue #3): the common
 * progressions I IV V V, I I IV vi, I IV I V, I iii IV vi, I vi IV, IV V IV I. Each chord takes the bars of the slot chosen; the slots
 * after it are emptied, so the loop is the fill; the types stay as they were. A seven-note scale only (a pentatonic has no circle to walk). */
#define PGF_N 11
static const uint8_t PGF[PGF_N][EN_PRG_N] = {{2, 5, 1, 1}, {6, 2, 5, 1}, {3, 6, 2, 5, 1, 1}, {1, 6, 2, 5}, {1, 4, 7, 3, 6, 2, 5, 1},
                                            {1, 4, 5, 5}, {1, 1, 4, 6}, {1, 4, 1, 5}, {1, 3, 4, 6}, {1, 6, 4}, {4, 5, 4, 1}};
static const char *const PGF_NAME[PGF_N] = {"ii V I", "vi ii V I", "iii vi ii V I", "I vi ii V", "THE WHOLE CIRCLE",
                                            "I IV V V", "I I IV vi", "I IV I V", "I iii IV vi", "I vi IV", "IV V IV I"};
static int pg_scale(void);
static void pg_fill(int n)
{
    int s, br;
    if (pg_scale() != 7) {
        say("FILL", "NEEDS 7 NOTES");
        return;
    }
    if (!pg_any(proj.prg))
        en_prog_capture(proj.prg);               /* (as an edit does: the chords heard last become the piece's first) */
    br = en_prg_get(proj.prg, 24 + 2 * (ui.pgsl & 7), 2);
    for (s = 0; s < EN_PRG_N; s++) {
        en_prg_put(proj.prg, 3 * s, 3, PGF[n][s]);
        if (PGF[n][s])
            en_prg_put(proj.prg, 24 + 2 * s, 2, br);
    }
    en_prg_put(proj.prg, EN_PRG_PLAY, 1, 1);
    en_prog(proj.prg);
    edited();
    ui.pgsl = 0;
    say(n < 5 ? "CIRCLE" : "PROGRESSION", PGF_NAME[n]);
}
static int pg_scale(void)                       /* the degrees there are: the scale's notes (seven at most) */
{
    int st[12];
    const int m = en_scale(proj.pool, st);
    return m > 7 ? 7 : m;
}
static int pk_get(int s) { return (int)(((uint32_t)proj.prk[0] | (uint32_t)proj.prk[1] << 8 | (uint32_t)proj.prk[2] << 16) >> 3 * s & 7u); }   /* a slot's type */
static void pk_put(int s, int v)
{
    uint32_t k = (uint32_t)proj.prk[0] | (uint32_t)proj.prk[1] << 8 | (uint32_t)proj.prk[2] << 16;
    k = (k & ~(7u << 3 * s)) | (uint32_t)(v & 7) << 3 * s;
    proj.prk[0] = (uint8_t)k, proj.prk[1] = (uint8_t)(k >> 8), proj.prk[2] = (uint8_t)(k >> 16);
}
static const char *pg_name(int d, int kind, char *b)   /* a slot's chord (1 ..) of its type (0 the piece's): its name, "Am7" */
{
    int pcs[4];
    const int n = en_prog_chord(d - 1, kind, pcs);
    return en_chord_name(pcs, n, b);
}
static const char *pg_roman(int d, char *b)     /* and its degree, lower case when it is minor (as the piece's chord type has it) */
{
    static const char *const R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    int pcs[4], k = 0;
    const int n = en_prog_chord(d - 1, EN_CH_TRIAD, pcs), minor = n > 1 && (pcs[1] - pcs[0] + 12) % 12 == 3;
    const char *r = R[(d - 1) % 7];
    while (r[k]) {
        b[k] = (char)(minor ? r[k] - 'A' + 'a' : r[k]);
        k++;
    }
    b[k] = 0;
    return b;
}
static const char *const MXT_NAME[5] = {"PAD", "BASS", "ACID", "PLUCK", "DRUMS"};
#define BA_NT 7
#define BA_NE 10
static const char *const BA_TNAME[BA_NT] = {"Off", "Octave", "Fifth", "Run", "Busy", "Sparse", "Slide"};
static const char *const BA_ENAME[BA_NE] = {"2 bars", "3 bars", "4 bars", "5 bars", "6 bars", "7 bars", "8 bars", "Drift", "With acid", "After acid"};
static int knob_get(int k)
{
    int p = knob_par(k);
    if (k == K_MXT)
        return ui.mix_trk;
    if (k == K_PLRT || k == K_DLRT)              /* (the place in speed order) */
        return lrt_pos(proj.par[p]);
    if (p == P_CM_SND)                           /* (0.17.25: the place on the pluck's list: the thirty more DX voices come before the loaded ones) */
        return en_cm_snd_pos(proj.par[p]);
    if (k == K_GENRE)
        return proj.genre;
    if (k == K_ACT)                              /* the answer's kind (P_AC_RESP is kind * 5 + direction) */
        return proj.par[P_AC_RESP] / 5;
    if (k == K_ACD)
        return proj.par[P_AC_RESP] >= 5 ? proj.par[P_AC_RESP] % 5 : 3;
    if (k == K_ACE)                              /* every: 0 .. 7 counting bars, 8 .. 15 counting loops of the line */
        return (proj.par[P_AC_EVERY] & 7) + (proj.par[P_AC_CHANCE] ? 8 : 0);
    if (k == K_ACL)
        return ui.fxs[EN_FXS_AC_ANS];
    if (k == K_BAT || k == K_BAE)                /* (0.17.30) the bass's answer */
        return ui.fxs[k == K_BAT ? EN_FXS_BA_TYPE : EN_FXS_BA_EVERY];
    if (k == K_GRT)
        return ui.lane;
    if (k == K_GRA)
        return ui.gr_all;
    if (k == K_ASTY || k == K_ASNO || k == K_ASOC) {
        as_sync();
        if (k == K_ASNO)
            return ui.as_off[ui.astp & 15];
        if (k == K_ASOC)
            return ui.as_oct[ui.astp & 15];
        return as_kind(ui.astp & 15);
    }
    if (k == K_PGSL)
        return ui.pgsl & 7;
    if (k == K_SLSL)
        return ui.slsl % SL_N;
    if (k == K_CDPOS)
        return code_pos;
    if (k == K_CDCHR) {
        if (!code_typed)
            code_now(code_in);
        return code_in[code_pos % CODE_N];
    }
    if (k == K_CDROLL)
        return code_roll;
    if (k == K_SLGN)                             /* the piece: genre * NPIECES + piece + 1 (0 Empty) */
        return glob.sl[ui.slsl % SL_N];
    if (k == K_SLPC)                             /* its bars: the place in SL_BARS */
        return sl_bars_i(glob.sl_bars[ui.slsl % SL_N]);
    if (k == K_SLON)
        return glob.sl_on ? 1 + (glob.sl_end & 1u) : 0;
    if (k == K_SLPT)
        return glob.sl_pat[ui.slsl % SL_N];
    if (k == K_SLEN)                             /* (0 as it is; then 0, 5 .. 100) */
        return glob.sl_mood[ui.slsl % SL_N] ? (glob.sl_mood[ui.slsl % SL_N] - 1) / 5 + 1 : 0;
    if (k == K_PGTY)
        return pk_get(ui.pgsl & 7);
    if (k == K_PGCH || k == K_PGBR || k == K_PGPL) {
        uint8_t tmp[EN_PRG_LEN];
        const uint8_t *b = pg_view(tmp);
        return k == K_PGCH ? en_prg_get(b, 3 * (ui.pgsl & 7), 3) : k == K_PGBR ? en_prg_get(b, 24 + 2 * (ui.pgsl & 7), 2) : en_prg_get(proj.prg, EN_PRG_PLAY, 1);
    }
    if (k == K_BSTY || k == K_BSNO || k == K_BSOC) {
        const int v = en_bs[ui.bstp & 15];
        return k == K_BSTY ? EN_BS_KIND(v) : k == K_BSNO ? EN_BS_NOTE(v) : EN_BS_OCT(v);
    }
    if (k == K_PSNO)
        return en_cm_rung[ui.pstp & 15];
    if (k == K_PSLV)
        return en_cm_lvl[ui.pstp & 15];
    if (k == K_STR)
        return en_mod[ui.lane % EN_NLANE][ui.stp & 15] & 7;
    if (k == K_STP)
        return en_mod[ui.lane % EN_NLANE][ui.stp & 15] >> 3 & 7;
    if (k == K_STV)
        return en_mod[ui.lane % EN_NLANE][ui.stp & 15] >> 6;
    if (k >= K_FXP)
        return fxp_get(k);
    if (k >= K_FXF)
        return ui.fxk[k - K_FXF];
    return p >= 0 ? proj.par[p] : 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    int p = knob_par(k);
    if (p == P_CM_SND) {                         /* the places on the pluck's list: what can be chosen now */
        *lo = 0;
        *hi = en_ncm_snd() - 1;
        return;
    }
    if (k == K_MXT) {
        *lo = 0;
        *hi = 4;
        return;
    }
    if (k == K_PLRT || k == K_DLRT) {
        *lo = 0;
        *hi = NLRT - 1;
        return;
    }
    if (k == K_BAT || k == K_BAE) {
        *lo = 0;
        *hi = k == K_BAT ? BA_NT - 1 : BA_NE - 1;
        return;
    }
    if (k == K_ACT || k == K_ACD || k == K_ACE || k == K_ACL) {
        *lo = 0;
        *hi = k == K_ACT ? 8 : k == K_ACD ? 4 : k == K_ACE ? 15 : 3;
        return;
    }
    if (k == K_GENRE) {
        *lo = 0;
        *hi = EN_NGENRE - 1;
        return;
    }
    if (k == K_GRT || k == K_GRA) {
        *lo = 0;
        *hi = k == K_GRT ? EN_NLANE - 1 : 100;
        return;
    }
    if (k == K_STR || k == K_STP || k == K_STV) {
        *lo = 0;
        *hi = k == K_STV ? 3 : 7;
        return;
    }
    if (k == K_ASTY || k == K_ASNO || k == K_ASOC) {
        *lo = 0;
        *hi = k == K_ASTY ? 4 : k == K_ASNO ? 15 : 3;   /* (0.17.78: the note 0 .. 15 scale steps, as the engine and the studio take it; was 11) */
        return;
    }
    if (k == K_PSNO || k == K_PSLV) {
        *lo = 0;
        *hi = k == K_PSNO ? 11 : 3;
        return;
    }
    if (k == K_BSTY || k == K_BSNO || k == K_BSOC) {
        *lo = 0;
        *hi = k == K_BSTY ? 5 : k == K_BSNO ? 7 : 2;
        return;
    }
    if (k == K_SLPT || k == K_SLEN) {
        *lo = 0;
        *hi = k == K_SLPT ? EN_NPAT : 21;
        return;
    }
    if (k == K_CDPOS || k == K_CDCHR || k == K_CDROLL) {
        *lo = 0;
        *hi = k == K_CDPOS ? CODE_N - 1 : k == K_CDCHR ? 31 : 255;
        return;
    }
    if (k == K_SLSL || k == K_SLGN || k == K_SLPC || k == K_SLON) {
        *lo = 0;
        *hi = k == K_SLSL ? SL_N - 1 : k == K_SLGN ? EN_NGENRE * NPIECES : k == K_SLPC ? SL_NBARS - 1 : 2;
        return;
    }
    if (k == K_PGSL || k == K_PGCH || k == K_PGBR || k == K_PGPL || k == K_PGTY) {
        *lo = 0;
        *hi = k == K_PGSL ? EN_PRG_N - 1 : k == K_PGCH ? pg_scale() : k == K_PGBR ? 3 : k == K_PGTY ? EN_NCHORD - 1 : 1;
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
    if (k == K_GENRE) {
        const char *s = GENRE_UI_NAME[(unsigned)proj.genre < (unsigned)EN_NGENRE ? proj.genre : 0];
        int n = 0;
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_BAT || k == K_BAE) {              /* (0.17.30) the bass's answer: its kind, when */
        const int v = knob_get(k);
        const char *s = k == K_BAT ? BA_TNAME[v < BA_NT ? v : 0] : BA_ENAME[v < BA_NE ? v : 0];
        int n = 0;
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_ACE || k == K_ACL) {              /* every: "4 bars", "4 loops", "Drift" (the bar before the drums change), "Drift lp"; the level */
        static const char *const AL[4] = {"Same", "+2 dB", "+4 dB", "+6 dB"};
        const int v = knob_get(k), e = v & 7;
        int n = 0;
        const char *s = k == K_ACL ? AL[v & 3] : e == 7 ? (v >= 8 ? "Drift lp" : "Drift") : v >= 8 ? " loops" : " bars";
        if (k == K_ACE && e < 7)
            b[n++] = (char)('2' + e);
        while (*s)
            b[n++] = *s++;
        b[n] = 0;
        return;
    }
    if (k == K_ACT || k == K_ACD) {              /* the answer's kind, and its direction (which only the octave and the fifth have) */
        static const char *const AT[9] = {"Off", "Octave", "Fifth", "Third", "Mirror", "Reverse", "Rotate", "Sparse", "Accents"};
        static const char *const AD[5] = {"Down 2", "Down", "Same", "Up", "Up 2"};
        const char *s = k == K_ACT ? AT[knob_get(K_ACT)] : proj.par[P_AC_RESP] >= 5 ? AD[proj.par[P_AC_RESP] % 5] : "-";
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
    if (k == K_SLPT || k == K_SLEN) {            /* a place's pattern (the piece's, 1 .. 6) and energy (as it is, 0 .. 100) */
        const int v = knob_get(k);
        if (!v) {
            const char *s = k == K_SLPT ? "Piece's" : "As is";
            int n = 0;
            while (s[n]) {
                b[n] = s[n];
                n++;
            }
            b[n] = 0;
            return;
        }
        itoa_u(k == K_SLPT ? (uint32_t)v : (uint32_t)(v - 1) * 5u, b);
        return;
    }
    if (k == K_CDPOS || k == K_CDCHR || k == K_CDROLL) {   /* the code: the place (1 .. 9), the letter there, the roll */
        if (k == K_CDROLL) {
            b[0] = 'N', b[1] = 'e', b[2] = 'w', b[3] = 0;
            return;
        }
        if (k == K_CDPOS) {
            itoa_u((uint32_t)code_pos + 1u, b);
            return;
        }
        b[0] = CODE_ABC[knob_get(k) & 31];
        b[1] = 0;
        return;
    }
    if (k == K_SLSL || k == K_SLGN || k == K_SLPC || k == K_SLON) {   /* the set list: the slot (1 ..), its piece (a genre's short name and number, or Empty), its bars
                                                 * (Auto, 4 ..), Play (Off, Loop, Once) */
        const int v = knob_get(k);
        const char *s = k == K_SLON ? (v == 2 ? "Once" : v ? "Loop" : "Off") : k == K_SLGN && !v ? "Empty" : k == K_SLPC && !v ? "Auto" : 0;
        int n = 0;
        if (k == K_SLGN && v) {                  /* "HOUSE 05" (the genre's name cut to fit) */
            const char *g = en_genre_name((v - 1) / NPIECES);
            char t[4];
            while (g[n] && n < 6) {
                b[n] = g[n];
                n++;
            }
            b[n++] = ' ';
            itoa_u((uint32_t)(v - 1) % NPIECES + 1u, t);
            if (!t[1])
                b[n++] = '0';
            b[n++] = t[0];
            if (t[1])
                b[n++] = t[1];
            b[n] = 0;
            return;
        }
        if (!s) {
            itoa_u(k == K_SLPC ? SL_BARS[v % SL_NBARS] : (uint32_t)v + 1u, b);
            return;
        }
        while (s[n] && n < 11) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_PGSL || k == K_PGCH || k == K_PGBR || k == K_PGPL || k == K_PGTY) {   /* the progression: the slot (1 ..), its chord, its type, its bars, Play */
        static const char *const BR[4] = {"1 bar", "2 bars", "4 bars", "8 bars"};
        static const char *const TY[EN_NCHORD] = {"Piece's", "Triad", "7th", "9th", "Sus", "Dub", "Fourths", "11th"};
        const int v = knob_get(k);
        const char *s = k == K_PGBR ? BR[v & 3] : k == K_PGPL ? (v ? "Loop" : "Walk") : k == K_PGCH && !v ? "Hold" : k == K_PGTY ? TY[(unsigned)v < EN_NCHORD ? v : 0] : 0;
        int n = 0;
        if (k == K_PGSL) {
            itoa_u((uint32_t)v + 1u, b);
            return;
        }
        if (!s)
            s = pg_name(v, pk_get(ui.pgsl & 7), b);
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_BSTY || k == K_BSNO || k == K_BSOC) {   /* the bass step: its kind, its note above the chord's root, its octave */
        static const char *const BK[6] = {"Rest", "Note", "Accent", "Glide", "Acc+glide", "Hold"};   /* (Hold: 0.17.42) */
        static const char *const BN[8] = {"Root", "2nd", "3rd", "4th", "5th", "6th", "7th", "Octave"};
        static const char *const BO[3] = {"Down", "Mid", "Up"};
        const int v = knob_get(k);
        const char *s = k == K_BSTY ? BK[v < 0 ? 0 : v > 5 ? 5 : v] : k == K_BSNO ? BN[v & 7] : BO[v < 0 ? 0 : v > 2 ? 2 : v];
        int n = 0;
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_PSNO || k == K_PSLV) {            /* the pluck's step: its rung (1 ..) or its level */
        static const char *const PL[4] = {"Rest", "Soft", "Note", "Accent"};
        const int v = knob_get(k);
        int n = 0;
        if (k == K_PSNO) {
            itoa_u((uint32_t)v + 1u, b);
            return;
        }
        while (PL[v & 3][n]) {
            b[n] = PL[v & 3][n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_ASTY || k == K_ASNO || k == K_ASOC) {
        static const char *const AK[5] = {"Rest", "Note", "Accent", "Slide", "Acc+Slide"};
        const int v = knob_get(k);
        const char *s = k == K_ASTY ? AK[v] : 0;
        int n = 0;
        if (!s) {
            itoa_u((uint32_t)v, b);
            return;
        }
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
        return;
    }
    if (k == K_STR || k == K_STP || k == K_STV) {
        static const char *const RN[8] = {"Off", "x2", "x3", "x4", "x6", "Flam", "Swell 3", "Swell 4"};
        static const char *const LN[4] = {"Normal", "Ghost", "Soft", "Accent"};
        const int v = knob_get(k);
        const char *s = k == K_STR ? RN[v] : k == K_STV ? LN[v] : 0;
        int n = 0;
        if (!s) {
            itoa_u((uint32_t)v, b);
            return;
        }
        while (s[n]) {
            b[n] = s[n];
            n++;
        }
        b[n] = 0;
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
    if (k == K_PCOND) {                          /* the lane chosen's condition: its two letters, then the condition */
        char c[12];
        int n = 0;
        const char *s = LANE_SHORT[ui.lane % EN_NLANE];
        b[n++] = s[0];
        b[n++] = s[1] ? s[1] : ' ';
        b[n++] = ' ';
        en_param_text(p, proj.par[p], c);
        for (k = 0; c[k] && n < 9; k++)
            b[n++] = c[k];
        b[n] = 0;
        return;
    }
    if (p >= 0)
        en_param_text(p, proj.par[p], b);
    else
        b[0] = 0;
}

static void ps_freeze(void)                     /* the pluck line held: deja vu's loop as sixteen sixteenths, every note the same next time, and kept with the piece */
{
    if (proj.par[P_CM_RATE] != 3)
        knob_set(P_CM_RATE, 3);
    if (proj.par[P_CM_LOOP] != 2)
        knob_set(P_CM_LOOP, 2);
    if (proj.par[P_CM_VU] != 100)
        knob_set(P_CM_VU, 100);
    if (!proj.par[P_CM_LOCK]) {                  /* (set here, not through knob_set: its freeze hook calls this) */
        proj.par[P_CM_LOCK] = 1;
        en_set(P_CM_LOCK, 1);
        edited();
    }
}
static void knob_set(int k, int v)
{
    int lo, hi, p = knob_par(k);
    if (p == P_CM_SND) {                         /* a place on the list: its sound */
        const int ns = en_ncm_snd();
        v = en_cm_snd_at(v < 0 ? 0 : v > ns - 1 ? ns - 1 : v);
        if (v == proj.par[p])
            return;
        proj.par[p] = (int16_t)v;
        en_set(p, v);
        edited();
        return;
    }
    if (k == K_PLRT || k == K_DLRT) {            /* the place in speed order: the engine's number */
        knob_set(p, LRT_ORDER[v < 0 ? 0 : v > NLRT - 1 ? NLRT - 1 : v]);
        return;
    }
    if (k == P_CM_LOCK && v && !proj.par[P_CM_LOCK]) {   /* freezing the pluck line: the loop's terms first */
        ps_freeze();
        return;
    }
    if (k == K_PGSL) {
        ui.pgsl = (uint8_t)(v < 0 ? 0 : v > EN_PRG_N - 1 ? EN_PRG_N - 1 : v);
        return;
    }
    if (k == K_SLSL) {
        ui.slsl = (uint8_t)(v < 0 ? 0 : v > SL_N - 1 ? SL_N - 1 : v);
        return;
    }
    if (k == K_CDPOS) {
        code_pos = (uint8_t)(v < 0 ? 0 : v > CODE_N - 1 ? CODE_N - 1 : v);
        return;
    }
    if (k == K_SLPT || k == K_SLEN) {            /* a place's pattern and energy: kept with the list; the place playing hears it at once */
        const int c = ui.slsl % SL_N;
        if (k == K_SLPT)
            glob.sl_pat[c] = (uint8_t)(v < 0 ? 0 : v > EN_NPAT ? EN_NPAT : v);
        else
            glob.sl_mood[c] = (uint8_t)(v <= 0 ? 0 : (v > 21 ? 20 : v - 1) * 5 + 1);
        project_globals();
        if (sl_play() && c == ui.sl_pos)
            sl_parts(c);
        return;
    }
    if (k == K_CDCHR || k == K_CDROLL) {         /* a letter typed (the check letter follows the others), or a new roll: the track playing's, another seed */
        if (!code_typed)
            code_now(code_in);
        if (k == K_CDCHR) {
            code_in[code_pos % CODE_N] = (uint8_t)(v < 0 ? 0 : v > 31 ? 31 : v);
            if (code_pos < CODE_N - 1)
                code_in[CODE_N - 1] = (uint8_t)code_check(code_in);
        } else {
            tcode_t t;
            code_roll = 128;                     /* (the knob sits in the middle: it turns for ever) */
            if (code_unpack(code_in, &t))
                code_now(code_in), code_unpack(code_in, &t);
            t.seed = (uint16_t)((plat_ms() + (uint32_t)t.seed * 40503u) * 2654435761u >> 16);
            code_pack(&t, code_in);
        }
        code_typed = 1;
        return;
    }
    if (k == K_SLGN || k == K_SLPC || k == K_SLON) {   /* the set list: kept with the player's three (written a moment after) */
        uint8_t *const e = &glob.sl[ui.slsl % SL_N];
        if (k == K_SLON) {
            glob.sl_on = (uint8_t)(v > 0);
            glob.sl_end = (uint8_t)(v == 2);
            ui.sl_pos = (uint8_t)sl_find();      /* (on: it goes on from the piece playing, if it is in the list) */
            ui.sl_q = 0;
            sl_b0 = sl_bar_now();                /* (its bars count from here) */
        } else if (k == K_SLPC) {
            glob.sl_bars[ui.slsl % SL_N] = (uint8_t)SL_BARS[v < 0 ? 0 : v > SL_NBARS - 1 ? SL_NBARS - 1 : v];
        } else if (v <= 0) {
            *e = 0;                              /* Empty */
        } else if (!*e) {
            *e = (uint8_t)(proj.genre * NPIECES + proj.piece + 1);   /* an empty slot turned: the piece playing goes in */
        } else {
            *e = (uint8_t)(v > EN_NGENRE * NPIECES ? EN_NGENRE * NPIECES : v);
        }
        project_globals();
        return;
    }
    if (k == K_PGCH || k == K_PGBR || k == K_PGPL || k == K_PGTY) {   /* the progression: a chord, its type or its bars set makes the slots the piece's and plays them; Play: the walk
                                                 * or the slots */
        const int s = ui.pgsl & 7;
        if (!pg_any(proj.prg) && (k != K_PGPL || v > 0))
            en_prog_capture(proj.prg);           /* (the chords heard last, as the page showed them) */
        if (k == K_PGTY) {
            pk_put(s, v < 0 ? 0 : v > EN_NCHORD - 1 ? EN_NCHORD - 1 : v);
            en_prog_kinds(proj.prk);
        } else if (k == K_PGCH)
            en_prg_put(proj.prg, 3 * s, 3, v < 0 ? 0 : v > pg_scale() ? pg_scale() : v);
        else if (k == K_PGBR)
            en_prg_put(proj.prg, 24 + 2 * s, 2, v < 0 ? 0 : v > 3 ? 3 : v);
        en_prg_put(proj.prg, EN_PRG_PLAY, 1, k == K_PGPL ? v > 0 : 1);
        en_prog(proj.prg);
        edited();
        return;
    }
    if (k == K_BSTY || k == K_BSNO || k == K_BSOC) {   /* the bass step chosen; an edit plays the steps (the bass line's mode to Steps) */
        const int s = ui.bstp & 15, o = en_bs[s];
        int kind = EN_BS_KIND(o), note = EN_BS_NOTE(o), oct = EN_BS_OCT(o);
        if (k == K_BSTY)
            kind = v < 0 ? 0 : v > EN_BS_HOLD ? EN_BS_HOLD : v;
        else if (k == K_BSNO)
            note = v < 0 ? 0 : v > 7 ? 7 : v;
        else
            oct = v < 0 ? 0 : v > 2 ? 2 : v;
        if (k != K_BSTY && !kind)
            kind = 1;                            /* (a rest given a note is a note) */
        en_bass_step(s, EN_BS_MAKE(kind, note, oct));
        en_bs[s] = EN_BS_MAKE(kind, note, oct);  /* (the page shows it at once) */
        if (proj.par[P_BL_MODE] != EN_BL_STEPS)
            knob_set(P_BL_MODE, EN_BL_STEPS);
        edited();
        return;
    }
    if (k == K_PSNO || k == K_PSLV) {            /* the pluck's step chosen: its rung or its level; an edit freezes the line */
        const int s = ui.pstp & 15;
        int rung = en_cm_rung[s], lvl = en_cm_lvl[s];
        if (k == K_PSNO)
            rung = v < 0 ? 0 : v > 11 ? 11 : v;
        else
            lvl = v < 0 ? 0 : v > 3 ? 3 : v;
        ps_freeze();
        en_pluck_step(s, rung, lvl);
        edited();
        return;
    }
    if (k == K_MXT) {
        ui.mix_trk = (uint8_t)(v < 0 ? 0 : v > 4 ? 4 : v);
        return;
    }
    if (k == K_GENRE) {                          /* the genre: a switch of bank (the way GLO + PRESETS does it) */
        v = v < 0 ? 0 : v > EN_NGENRE - 1 ? EN_NGENRE - 1 : v;
        if (v != proj.genre)
            genre_step(v - proj.genre);
        return;
    }
    if (k == K_ACE) {                            /* every and what it counts, as one knob */
        v = v < 0 ? 0 : v > 15 ? 15 : v;
        knob_set(P_AC_EVERY, v & 7);
        knob_set(P_AC_CHANCE, v >= 8);
        return;
    }
    if (k == K_BAT || k == K_BAE) {              /* (0.17.30) the bass's answer: the piece's, kept with it as its FILTERS are (keep() takes it, project.c PX_FXS) */
        const int i = k == K_BAT ? EN_FXS_BA_TYPE : EN_FXS_BA_EVERY;
        v = v < 0 ? 0 : v > (k == K_BAT ? BA_NT : BA_NE) - 1 ? (k == K_BAT ? BA_NT : BA_NE) - 1 : v;
        if (v != ui.fxs[i])
            ui.set_dirty = 1;
        ui.fxs[i] = (uint8_t)v;
        en_fxs(i, v);
        return;
    }
    if (k == K_ACL) {                            /* the answer's level: the unit's setting, kept with the effects' settings */
        v = v < 0 ? 0 : v > 3 ? 3 : v;
        if (v != ui.fxs[EN_FXS_AC_ANS])
            ui.set_dirty = 1;                    /* (kept a moment after it is left alone, settings_save) */
        ui.fxs[EN_FXS_AC_ANS] = (uint8_t)v;
        en_fxs(EN_FXS_AC_ANS, v);
        return;
    }
    if (k == K_ACT || k == K_ACD) {              /* one value: 0 off, 1 octave up, 2 fifth up, 3 mirror, 4 sparse, 5 octave down, 6 fifth down */
        const int cur = proj.par[P_AC_RESP], ty = k == K_ACT ? (v < 0 ? 0 : v > 8 ? 8 : v) : cur / 5, dr = k == K_ACD ? (v < 0 ? 0 : v > 4 ? 4 : v) : (cur >= 5 ? cur % 5 : 3);
        if (k == K_ACD && cur < 5)                /* (no answer: the direction has nothing to turn) */
            return;
        knob_set(P_AC_RESP, ty ? ty * 5 + dr : 0);
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
    if (k == K_ASTY || k == K_ASNO || k == K_ASOC) {   /* the acid's step chosen: its kind, note or octave; an edit freezes the line */
        const int s = ui.astp & 15;
        int kind, note, oct;
        as_sync();
        kind = as_kind(s);
        note = ui.as_off[s];
        oct = ui.as_oct[s];
        if (k == K_ASTY)
            kind = v < 0 ? 0 : v > 4 ? 4 : v;
        else if (k == K_ASNO)
            note = v < 0 ? 0 : v > 15 ? 15 : v;
        else
            oct = v < 0 ? 0 : v > 3 ? 3 : v;
        as_put(s, note, oct, kind);
        return;
    }
    if (k == K_STR || k == K_STP || k == K_STV) {   /* the chosen step's trick: ratchet (bits 0-2), push (3-5), level (6-7) */
        const int sh = k == K_STR ? 0 : k == K_STP ? 3 : 6, mx = k == K_STV ? 3 : 7;
        volatile uint8_t *m = &en_mod[ui.lane % EN_NLANE][ui.stp & 15];
        v = v < 0 ? 0 : v > mx ? mx : v;
        if ((int)(*m >> sh & (uint8_t)mx) != v) {
            *m = (uint8_t)((*m & ~(mx << sh)) | v << sh);
            edited();
        }
        return;
    }
    if (k >= K_FXP) {
        fxp_set(k, v);
        return;
    }
    if (k >= K_FXF) {                            /* the FX knobs: the engine's, never stored */
        if (!ui.glide_busy)
            ui.glide_on &= (uint8_t)~(1u << (k - K_FXF));   /* (turned by hand: a gliding one is let go) */
        if (!gs_busy && !ui.glide_busy)
            gs_mask &= (uint8_t)~(1u << (k - K_FXF));      /* (and the gesture's, 0.17.31) */
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
static void dj_next(void)                       /* the auto-DJ's next piece, on the bar: the set list's next slot when it plays, else the bank's next */
{
    const int s = sl_play() ? sl_step(ui.sl_pos, 1) : -1;
    if (s >= 0)
        ui.sl_q = (uint8_t)(s + 1);
    else
        ui.piece_q = (uint8_t)((proj.piece + 1) % NPIECES + 1);
}
/* THE MIX (the SET page's Mix, 0.16.1): a piece that comes on the bar (the auto-DJ's, the set list's, SEL + PRESETS) starts at the tempo that was
 * playing and glides to its own over 4 to 32 bars, as a DJ brings the next record in at the old one's speed and lets it go back to its own. A tempo
 * turned by hand stops it there. */
static void mix_start(int from)
{
    const int bars = XMIX_BARS[glob.sl_rsv[1] % 5u];
    ui.mx_bars = 0;
    if (!bars || from == proj.par[P_TEMPO])
        return;
    ui.mx_from = (int16_t)from;
    ui.mx_to = proj.par[P_TEMPO];
    ui.mx_bars = (uint8_t)bars;
    ui.mx_n = 0;
    proj.par[P_TEMPO] = (int16_t)from;
    en_set(P_TEMPO, from);
    ui.mx_last = (int16_t)from;
}
static void mix_tick(void)                       /* each frame: the glide, by the bar and the step */
{
    int t;
    if (!ui.mx_bars)
        return;
    if (!en_playing || proj.par[P_TEMPO] != ui.mx_last) {
        ui.mx_bars = 0;                          /* stopped, or turned by hand */
        return;
    }
    {
        const int span = ui.mx_bars * EN_NSTEP, at0 = (ui.mx_n - 1) * EN_NSTEP + en_drum_pos, at = at0 < 0 ? 0 : at0;
        const int d = (ui.mx_to - ui.mx_from) * at;
        if (at >= span) {
            t = ui.mx_to;
            ui.mx_bars = 0;
        } else {
            t = ui.mx_from + (d >= 0 ? (d + span / 2) / span : -((-d + span / 2) / span));
        }
    }
    if (t != proj.par[P_TEMPO]) {
        proj.par[P_TEMPO] = (int16_t)t;
        en_set(P_TEMPO, t);
    }
    ui.mx_last = (int16_t)t;
}
static void dj_bar(void)                       /* a new bar (while playing) */
{
    const int g = DJ_G(), b = dj_break_len(g), sync = ui.dj_len == 4, rounds = DJ_ROUNDS[ui.dj_rounds & 3];
    const int ph = en_sec == EN_SEC_GROOVE ? 0 : en_sec == EN_SEC_BREAK ? 1 : 2;
    const int last_round = ui.dj_round + 1 >= rounds;   /* the piece's last round: the next piece follows its drop */
    if (ph != ui.dj_phase) {                   /* the engine moved on (the DJ's request, or SAVE): so does the count */
        if (ph == 0)
            ui.dj_round++;                     /* back in the groove: a round done */
        ui.dj_phase = (uint8_t)ph;
        ui.dj_n = 0;
    }
    ui.dj_fill = 0;
    if (en_intro_k < 1.0f) {                   /* the intro: the groove holds until everything is in */
        ui.dj_n = 0;
        return;
    }
    ui.dj_n++;
    if (ph == 0 && ui.dj_ramp && !ui.mx_bars) {   /* the ramp: the tempo climbs over the piece's rounds of groove; turned by hand, it climbs on from there (not while the mix glides) */
        const int bpm = RAMP_BPM[ui.dj_ramp % 5u], total = rounds * g, prog = (ui.dj_round < rounds ? ui.dj_round : rounds - 1) * g + (int)ui.dj_n;
        int t;
        if (proj.par[P_TEMPO] != ui.dj_tlast)
            ui.dj_t0 = (int16_t)(proj.par[P_TEMPO] - bpm * prog / total);
        t = ui.dj_t0 + bpm * (prog > total ? total : prog) / total;
        t = t < 70 ? 70 : t > 190 ? 190 : t;
        if (t != proj.par[P_TEMPO]) {
            proj.par[P_TEMPO] = (int16_t)t;
            en_set(P_TEMPO, t);
        }
        ui.dj_tlast = (int16_t)t;
    }
    if (ph == 0 && sync) {                     /* with the chords: the engine fills the bar before a change and breaks on it; the next piece comes in as the fill begins */
        if (en_drift_bars <= 2.0f && (ui.dj_piece || sl_play()) && !sl_exact() && last_round && !ui.piece_q && !ui.sl_q)
            dj_next();
    } else if (ph == 0) {
        if (ui.dj_n == (uint16_t)(g - 1)) {    /* the fill is the LAST bar of the groove (asked a bar ahead: the engine starts it on the bar line); the break follows at once */
            if (ui.dj_fillon)
                en_fill_next();
            if ((ui.dj_piece || sl_play()) && !sl_exact() && last_round && !ui.piece_q && !ui.sl_q)
                dj_next();
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
    {   /* the intro's length and the MIDI map to the engine when the setting changes (and once at boot) */
        static uint8_t isent = 255, msent = 255, hsent;
        if (ui.intro != isent) {
            isent = ui.intro;
            en_intro(INTRO_S[ui.intro % 6u]);
        }
        if (ui.midi_map != msent) {
            msent = ui.midi_map;
            en_midi_map(ui.midi_map);
        }
        if (ui.usb_host != hsent) {
            hsent = ui.usb_host;
            plat_usb_host(ui.usb_host);
        }
        {   /* what the USB host has to say */
            static const char *const HM[7] = {0, "ON: PLUG IN A DEVICE", "OFF: USB AS BEFORE", "DEVICE CONNECTED", "NO MIDI DEVICE FOUND",
                                              "LOW SPEED: NOT SUPPORTED", "DEVICE LOST: LOOKING"};
            const int m = plat_usb_host_msg();
            if (m == PLAT_UH_NOMIDI) {           /* what it found instead is on the MIDI IN page (the header has no room for it) */
                say("NO MIDI DEVICE", "SEE MIDI IN");
                ui.msg_until = plat_ms() + 4000u;
            } else if (m > 0 && m < 7) {
                say("USB HOST", HM[m]);
                ui.msg_until = plat_ms() + 2500u;
            }
        }
    }
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
            if (ui.mx_bars && ui.mx_n < 255)
                ui.mx_n++;                       /* the mix's glide: a bar more */
            if (ui.dj_on)
                dj_bar();
        }
        last = p;
    }
}

/* AUTO FX (0.16.8; the idea after DARK-1's Auto FX, by Polystellarflow, rebuilt for PERF-1's sections and genres): now and then an effect plays by itself and lets
 * go, timed to END on a phrase line (every 4, 8, 16 or 32 bars, as set: its last beat, half bar, bar or two bars), as a DJ leads into the next phrase. Which:
 * one that suits the section (the groove's gates, repeats and sweeps, the breakdown's washes, the build's rolls), four times as likely when it is the genre's
 * own (dub's throws and tape, house's pump and filters, breaks' scratches and repeats, trap's hat rolls and half time ...), up to the amount set (Gentle: the
 * washes and sweeps; Medium: and the gates, filters, repeats and rolls; Wild: and the stops, brakes, scatters, half time), and never one of the last two.
 * At the end of a build (Medium and up) a stop, a reverse, a brake or a spin-back leads into the drop; the four bars after a drop are left alone. It keeps
 * out of the way: nothing while an effect is played or latched by hand (that ends one under way), in the FX menu, in the intro, or stopped; with the
 * auto-DJ on, its fill and riser are left to it. */
#define AG(g) (1u << (g))                       /* the genres: 0 HYPNO, 1 LOFI, 2 HOUSE, 3 DUBSTEP, 4 DUB, 5 DNB, 6 BREAKS, 7 GARAGE, 8 ELECTRO, 9 TRAP, 10 DISCO,
                                                 * 11 FUNK, 12 REGGAE, 13 SYNTHWAVE, 14 AMAPIANO, 15 TRIP-HOP, 16 INDUSTRIAL, 17 PSY, 18 FOREST */
static const struct {
    uint8_t fx, lvl, sec, len;                   /* the effect; the amount it comes in at (0 gentle, 1 medium, 2 wild); the sections (1 groove, 2 breakdown, 4 build);
                                                 * the longest it may take, sixteenths (a throw or a stop is short however long the setting) */
    uint32_t fav;                                /* the genres it is theirs in */
} AFX[] = {
    {FX_PHASER, 0, 3, 32, AG(13) | AG(17) | AG(18) | AG(10) | AG(2)},
    {FX_FLANGER, 0, 3, 32, AG(13) | AG(17) | AG(18) | AG(0)},
    {FX_TAPE, 0, 3, 32, AG(1) | AG(15) | AG(4) | AG(12) | AG(13)},
    {FX_THROW, 0, 3, 4, AG(4) | AG(12) | AG(3) | AG(14) | AG(2)},
    {FX_SWEEP, 0, 5, 32, AG(0) | AG(2) | AG(17) | AG(18) | AG(16)},
    {FX_RVSWELL, 0, 6, 16, AG(4) | AG(15) | AG(1) | AG(3)},
    {FX_FILL, 0, 1, 16, AG(6) | AG(11) | AG(5) | AG(7)},
    {FX_LP, 1, 3, 16, AG(2) | AG(10) | AG(7) | AG(0)},
    {FX_HP, 1, 5, 16, AG(2) | AG(0) | AG(17) | AG(5)},
    {FX_GATE16, 1, 1, 16, AG(0) | AG(17) | AG(18) | AG(16) | AG(13)},
    {FX_GATE8, 1, 1, 16, AG(2) | AG(10) | AG(13) | AG(7)},
    {FX_PUMP, 1, 1, 32, AG(2) | AG(10) | AG(7) | AG(14)},
    {FX_RPT8, 1, 1, 8, AG(6) | AG(5) | AG(8) | AG(11)},
    {FX_RPT16, 1, 5, 8, AG(5) | AG(6) | AG(9) | AG(3) | AG(17)},
    {FX_RPT8T, 1, 1, 8, AG(14) | AG(9) | AG(7)},
    {FX_HATROLL, 1, 5, 8, AG(9) | AG(18) | AG(5) | AG(16)},
    {FX_SNROLL, 1, 4, 16, AG(3) | AG(9) | AG(5) | AG(6)},
    {FX_ROLL, 1, 4, 16, AG(2) | AG(0) | AG(17) | AG(10)},
    {FX_FREEZE, 1, 2, 16, AG(0) | AG(15) | AG(16) | AG(18)},
    {FX_RING, 2, 3, 8, AG(16) | AG(18) | AG(8)},
    {FX_SCATTER, 2, 1, 8, AG(6) | AG(5) | AG(8) | AG(16)},
    {FX_SCRATCH, 2, 1, 8, AG(6) | AG(11) | AG(8) | AG(15) | AG(1)},
    {FX_HALF, 2, 1, 16, AG(3) | AG(9) | AG(15)},
    {FX_DBLKICK, 2, 1, 8, AG(0) | AG(16) | AG(18) | AG(17)},
    {FX_REV, 2, 1, 4, AG(5) | AG(6) | AG(3)},
    {FX_STOP, 2, 1, 4, AG(3) | AG(9) | AG(4) | AG(1) | AG(15)},
    {FX_BRAKE, 2, 1, 8, AG(1) | AG(15) | AG(4) | AG(12)},
    {FX_SPIN, 2, 1, 4, AG(4) | AG(12) | AG(6)},
};
#define NAFX ((int)(sizeof AFX / sizeof AFX[0]))
static uint32_t af_dice(void)
{
    ui.af_rng ^= ui.af_rng << 13;
    ui.af_rng ^= ui.af_rng >> 17;
    ui.af_rng ^= ui.af_rng << 5;
    return ui.af_rng;
}
static void af_tick(void)
{
    static const uint8_t CHANCE[4] = {0, 50, 70, 90};   /* how often a phrase line gets one, by the amount */
    const int amt = ui.fxs[EN_FXS_AF_AMT] & 3, len_set = 4 << (ui.fxs[EN_FXS_AF_LEN] & 3), every = 4 << (ui.fxs[EN_FXS_AF_EVERY] & 3);
    const uint32_t s16 = en_bars, bar = s16 >> 4;
    int i, n = 0, tot = 0, pick, len, nb, predrop;
    uint8_t cand[NAFX], w[NAFX];
    if (!ui.af_rng)
        ui.af_rng = plat_ms() * 2654435761u | 1u;
    if (af_fresh) {
        if (!en_playing || en_bars >= 16u)
            return;
        af_fresh = 0;
    }
    if (en_sec != ui.af_sec) {                   /* a drop: four bars left alone, and nothing carried over */
        if (en_sec == EN_SEC_GROOVE && ui.af_sec == EN_SEC_BUILD) {
            ui.af_quiet = bar + 4u;
            ui.af_end = 0;
        }
        ui.af_sec = en_sec;
    }
    if (!amt || !en_playing || ui.fx_menu || ui.af_user || en_intro_k < 1.0f) {
        ui.af_live = 0;
        if (!amt || !en_playing || ui.af_user)
            ui.af_end = 0;                      /* (by hand: the one under way ends, and no other till the next line) */
        ui.af_bar = bar;
        return;
    }
    if (ui.af_end && s16 >= ui.af_end)
        ui.af_end = 0;
    ui.af_live = (uint8_t)(ui.af_end && s16 >= ui.af_start);
    if (ui.af_live && !ui.af_said) {
        ui.af_said = 1;
        say("AUTO FX", ui.af_fx == AF_GS ? "YOUR GESTURE" : EN_FX_NAME[ui.af_fx]);
    }
    if (bar == ui.af_bar)
        return;
    ui.af_bar = bar;                             /* a new bar: is this one to lead into a phrase line? */
    if (ui.af_end || bar < ui.af_quiet)
        return;
    predrop = en_sec == EN_SEC_BUILD && en_sec_bars == 1 && amt >= 2;
    if (!predrop && ui.fxs[EN_FXS_AF_PICK] == 2 && gs_st == 4 && gs_len) {   /* (0.17.44) Pick YOURS: the gesture kept, its whole length, ending on the line */
        nb = gs_len / EN_NSTEP;
        if ((bar + (uint32_t)nb) % (uint32_t)every || af_dice() % 100u >= CHANCE[amt])
            return;
        ui.af_fx = AF_GS;
        ui.af_end = (bar + (uint32_t)nb) * 16u;
        ui.af_start = ui.af_end - gs_len;
        ui.af_said = 0;
        return;
    }
    nb = len_set > 16 ? 2 : 1;                   /* (two bars long: planned two bars before the line) */
    if (predrop)
        nb = 1;
    else if ((bar + (uint32_t)nb) % (uint32_t)every || af_dice() % 100u >= CHANCE[amt])
        return;
    for (i = 0; i < NAFX; i++) {
        const uint8_t fx = AFX[i].fx;
        if (AFX[i].lvl >= amt || fx == ui.af_last[0] || fx == ui.af_last[1])
            continue;
        if (predrop ? !(fx == FX_STOP || fx == FX_REV || fx == FX_BRAKE || fx == FX_SPIN) : !(AFX[i].sec >> en_sec & 1u))
            continue;
        if (ui.dj_on && fx == FX_FILL)
            continue;                            /* (the auto-DJ's own) */
        cand[n] = (uint8_t)i;
        w[n] = (uint8_t)(!(ui.fxs[EN_FXS_AF_PICK] & 1) && (AFX[i].fav >> (proj.genre % 32u) & 1u) ? 4 : 1);
        tot += w[n++];
    }
    if (!n)
        return;
    for (pick = (int)(af_dice() % (uint32_t)tot), i = 0; i < n - 1 && pick >= w[i]; i++)
        pick -= w[i];
    i = cand[i];
    len = predrop ? AFX[i].len : len_set < AFX[i].len ? len_set : AFX[i].len;
    ui.af_fx = AFX[i].fx;
    ui.af_last[1] = ui.af_last[0];
    ui.af_last[0] = ui.af_fx;
    ui.af_end = (bar + (uint32_t)nb) * 16u;      /* it ends on the line */
    ui.af_start = ui.af_end - (uint32_t)len;
    ui.af_said = 0;
}

/* the key change on the drop (the DROP page: SAME, +2, +3, +5, +7 semitones): when a breakdown or a build gives way to the groove, the
 * root rises for the round; the next breakdown (or a stop, or another piece) puts it back. The engine alone moves; the piece's
 * own root (the Global page) stays. */
static void key_tick(void)
{
    const int sec = en_playing ? en_sec : EN_SEC_GROOVE, sh = KEY_SH[ui.key_set % 5];
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
    int rel = (pc - sound_root() + 12) % 12;
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
    ui.dj_round = 0;                             /* (the auto-DJ's rounds and its ramp start over with the piece) */
    project_piece(i);
    ui.key_on = 0;                               /* (project_apply puts the key back) */
    project_apply();
    mood_apply();
    track_fresh(1, 1);
    if (ui.intro == 5 && en_playing && !ui.dj_on)   /* Intro BREAK: the new piece in its breakdown; SAVE builds and drops it */
        en_section(EN_CMD_BREAK);
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
    static const uint8_t LOFI[8] = {25, 18, 30, 45, 40, 28, 42, 38};             /* tape and vinyl, gentler (the vinyl: less level, a darker tone, more hiss than crackle) */
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
    } else if (g >= 4 && g - 4 < 18) {           /* the others: each its tape, vinyl and pump (255: as it is), and what is latched (1 vinyl, 2 tape, 4 pump, 8 phaser) */
        static const uint8_t XFX[18][12] = {
            {14, 16, 22, 38, 255, 255, 255, 255, 255, 255, 255, 255},   /* dub: a worn tape (and the long echo) */
            {5, 5, 25, 20, 255, 255, 255, 255, 30, 30, 1, 0xF0},      /* dnb: a light pump, a very light tape */
            {20, 15, 60, 30, 255, 255, 255, 255, 255, 255, 255, 255}, /* breaks: tape drive, the phaser */
            {25, 10, 25, 40, 255, 255, 255, 255, 30, 35, 1, 0xF0},    /* garage: a light tape and pump */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* electro: the phaser alone */
            {255, 255, 255, 255, 255, 255, 255, 255, 45, 55, 0, 0x50},      /* trap: a pump under the 808 */
            {255, 255, 255, 255, 255, 255, 255, 255, 35, 45, 1, 0x70},      /* disco: a light pump on the quarters */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* funk: nothing latched */
            {12, 14, 20, 35, 255, 255, 255, 255, 255, 255, 255, 255},       /* reggae: a light tape (and the long echo) */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* synthwave: nothing latched (the chorus and the gated reverb are in the piece) */
            {255, 255, 255, 255, 255, 255, 255, 255, 30, 40, 1, 0x70},      /* amapiano: a soft pump */
            {18, 14, 28, 40, 30, 22, 36, 36, 255, 255, 255, 255},           /* trip-hop: the vinyl and the tape, gentler than lofi's */
            {20, 10, 70, 25, 255, 255, 255, 255, 255, 255, 255, 255},       /* industrial: a hard tape drive */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* psy: nothing latched (the echo and the shimmer are in the piece) */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* dark forest: the same */
            {16, 12, 30, 35, 20, 15, 40, 40, 255, 255, 255, 255},           /* (0.17.35) hiphop: a light tape and a little dust, less than lofi's */
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},   /* trance: nothing latched */
            {10, 12, 10, 30, 255, 255, 255, 255, 255, 255, 255, 255}};      /* ambient: a soft tape wow */
        static const uint8_t XLK[18] = {2, 6, 10, 6, 8, 4, 4, 0, 2, 0, 4, 3, 2, 0, 0, 3, 0, 2};
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
    if (ui.store_wait) {                         /* a store is being written (the fade): the genre's object is the one it goes to */
        say("STORING", "ONE MOMENT");
        return;
    }
    const int g = (proj.genre + d + EN_NGENRE) % EN_NGENRE;
    if (project_set_genre(g) != 0) {             /* (0.17.48, the audit) the outgoing genre's changes not written: it stays, nothing lost */
        say("NOT WRITTEN", "GENRE KEPT");
        return;
    }
    en_genre(g);
    ui.key_on = 0;
    project_apply();
    mood_apply();
    track_fresh(1, 1);
    ui.piece_q = 0;
    genre_fx(g);
    {
        char u[20];
        piece_title(u, sizeof u);
        say(en_genre_name(g), u);
    }
}
static void code_start(const tcode_t *t)        /* a track code: its genre, its piece as the recipe, its key, mode and tempo; from the top */
{
    char b[12];
    uint8_t g[CODE_N];
    if (ui.store_wait) {
        say("STORING", "ONE MOMENT");
        return;
    }
    if (en_playing)
        en_play(0);
    if (t->genre != proj.genre)
        genre_step((int)t->genre - (int)proj.genre);
    ui.dj_round = 0;
    project_piece_recipe(t->piece);
    proj.par[P_ROOT] = t->root;
    proj.par[P_MODE] = t->mode;
    proj.par[P_TEMPO] = t->tempo;
    proj.pool = EN_MODE_POOL[t->mode];
    ui.key_on = 0;
    project_apply();
    mood_apply();
    ui.piece_q = 0;
    code_seed = t->seed;
    track_fresh(0, 1);
    af_fresh = 1;
    sl_start();
    en_play(1);
    code_now(g);
    code_text(g, b);
    say("TRACK", b);
}
static void code_go(void)                        /* the Code page's letters: started, or said wrong */
{
    tcode_t t;
    if (code_unpack(code_in, &t)) {
        say("NOT A CODE", "CHECK THE LETTERS");
        return;
    }
    code_start(&t);
}
int ui_code_now(char *b)                         /* (the host, the remote) the track playing's code: 1 if it gives the piece as it is */
{
    uint8_t g[CODE_N];
    code_now(g);
    code_text(g, b);
    return code_exact();
}
int ui_code_play(const char *s)                  /* (the host, the remote) a code started: 0, or -1 not one */
{
    uint8_t g[CODE_N];
    tcode_t t;
    int n = 0;
    for (; *s && n < CODE_N; s++) {
        char c = *s;
        const char *q;
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        if (c == 'O')
            c = '0';
        else if (c == 'I' || c == 'L')
            c = '1';
        else if (c == 'U')
            c = 'V';
        for (q = CODE_ABC; *q && *q != c; q++)
            ;
        if (*q)
            g[n++] = (uint8_t)(q - CODE_ABC);
    }
    if (n != CODE_N || code_unpack(g, &t))
        return -1;
    code_start(&t);
    return 0;
}

static void sl_go(int s)                        /* the set list's slot s now: its genre, then its piece (said as SET 3/8 and its title) */
{
    const int e = glob.sl[s % SL_N], g = (e - 1) / NPIECES, p = (e - 1) % NPIECES;
    int i, n = 0, k = 0;
    if (!e)
        return;
    ui.sl_pos = (uint8_t)(s % SL_N);
    if (g != proj.genre)
        genre_step(g - proj.genre);
    if (g == proj.genre && p != proj.piece)
        set_piece(p);
    sl_b0 = en_playing ? sl_bar_now() + 1u : 0u;   /* (it comes in on the bar's last step: its first bar is the next) */
    if (glob.sl_on)
        sl_parts(s);
    for (i = 0; i < SL_N; i++)
        if (glob.sl[i]) {
            n++;
            if (i <= s)
                k = n;
        }
    {
        char u[12] = "SET ", v[20];
        int j = 4;
        itoa_u((uint32_t)k, u + j);
        while (u[j])
            j++;
        u[j++] = '/';
        itoa_u((uint32_t)n, u + j);
        piece_title(v, sizeof v);
        say(u, v);
    }
}

/* into flash now. Writing it stops the chip a moment (it runs from the same flash), so while the music
 * plays it fades out (en_hush), holds still while the store is written, and fades back in where it was:
 * a short pause, not a cut. Stopped: at once. */
static void store_now(const char *done)
{
    int i;
    if (ui.store_wait && ui.store_kind == 4) {   /* a restore is waiting for its fade: a store here would take its place and wedge it */
        say("RESTORING", "ONE MOMENT");
        return;
    }
    for (i = 0; done[i] && i < (int)sizeof ui.store_msg - 1; i++)
        ui.store_msg[i] = done[i];
    ui.store_msg[i] = 0;
    ui.store_kind = 0;
    if (en_playing && !plat_store_seamless()) {
        en_hush(1);
        ui.store_wait = 1;
        ui.store_t = plat_ms();
        say("STORING", 0);
    } else {
        say(ui.store_msg, project_flush() == 0 ? 0 : "NOT WRITTEN");
    }
}
static void settings_save(void);
static const uint8_t *rs_buf;                    /* a restore waiting to be written (backup_proto.h): the object, its bytes and how long */
static uint32_t rs_obj, rs_len;
static uint8_t rs_state;                         /* 0 none, 1 waiting for the music to fade, 2 written, 3 not written */
/* an autosave while the music plays: the same fade out, write, fade in (the write stops the chip, so unfaded it is a crack and an xrun);
 * kind 1: the effects' and performing settings; 2: the theme, Tune, MIDI out */
static void voices_save(void);
static void quiet_write(int kind)
{
    if (!en_playing || plat_store_seamless()) {  /* (seamless: the audio goes on through the write, nothing to fade) */
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
static void store_tick(int force)               /* the music faded out (or long enough): write, then back in */
{
    static uint32_t hushed_at;
    if (!ui.store_wait) {
        hushed_at = 0;
        return;
    }
    if (!force) {
        if (!en_hushed && plat_ms() - ui.store_t < 150u)
            return;
        if (en_hushed) {                         /* faded right out in the render: the audio already queued still plays out (about two halves), and a write
                                                  * stops the chip for ~20 ms: wait it out, or the last of the fade loops through the write as a click */
            if (!hushed_at)
                hushed_at = plat_ms();
            if (plat_ms() - hushed_at < 35u)
                return;
        }
    }
    hushed_at = 0;
    ui.store_wait = 0;
    if (ui.store_kind == 1) {
        settings_save();
    } else if (ui.store_kind == 3) {
        voices_save();
    } else if (ui.store_kind == 2) {
        project_flush_globals();
    } else if (ui.store_kind == 4) {
        rs_state = plat_store_restore(rs_obj, rs_buf, rs_len) == 0 ? 2 : 3;
    } else {
        const int r = project_flush();
        if (ui.store_msg[0])
            say(ui.store_msg, r == 0 ? 0 : "NOT WRITTEN");
    }
    ui.store_kind = 0;
    en_hush(0);
}

/* RESTORE (backup_proto.h, main_fm1.c): an object from a backup, checked as the loaders check it, then written (the music fades out for it as for any store).
 * Afterwards nothing else is written until the unit restarts: what is in RAM is the old state. */
void ui_restore_start(uint32_t obj, const uint8_t *b, uint32_t len)
{
    if (ui.store_wait)
        store_tick(1);
    rs_obj = obj, rs_buf = b, rs_len = len;
    rs_state = 1;
    ui.store_kind = 4;
    if (en_playing && !plat_store_seamless()) {
        en_hush(1);
        ui.store_wait = 1;
        ui.store_t = plat_ms();
        say("RESTORING", 0);
    } else {
        rs_state = plat_store_restore(rs_obj, rs_buf, rs_len) == 0 ? 2 : 3;
        ui.store_kind = 0;
        say("RESTORING", rs_state == 2 ? "DONE" : "NOT WRITTEN");
    }
}
int ui_restore_state(void) { return rs_state; }

/* an update session is coming (the music already stopped): what waits for flash is written now and a store's fade is undone, since
 * ui_init afterwards starts from nothing (left alone, a store caught mid-fade kept the output hushed and unsaved voices were lost) */
void ui_flush(void)
{
    if (ui.store_wait) {
        store_tick(1);
    }
    if (ui.vox_dirty) {
        ui.vox_dirty = 0;
        voices_save();
    }
    if (ui.set_dirty || ui.set_lazy) {
        ui.set_dirty = ui.set_lazy = 0;
        settings_save();
    }
    if (project_pending())
        project_flush();
    en_hush(0);
}

static void piece_store(void)                   /* Global, SAVE: the piece as it is, stored */
{
    char t[12] = "STORED ";
    project_piece_save();
    caps(t + 7, piece_name(proj.piece), 4);
    store_now(t);
}

static struct { uint8_t g, g0, p0; uint16_t n; } fall;   /* (0.17.23) the factory reset of everything: the next genre + 1 (0 idle), where it began, how many */
static void factory_all_tick(void)
{
    const int g = fall.g - 1;
    if (!fall.g || ui.store_wait)
        return;
    if (g < EN_NGENRE) {                         /* one genre a tick: loaded, its stored pieces cleared, written */
        if (project_set_genre(g) != 0) {         /* (0.17.48) a write failed: stopped where it is */
            fall.g = 0;
            say("FACTORY STOPPED", "NOT WRITTEN");
            return;
        }
        fall.n = (uint16_t)(fall.n + project_factory_genre());
        project_flush();
        fall.g++;
        say("FACTORY: ALL PIECES", en_genre_name(g));
        return;
    }
    fall.g = 0;                                  /* done: back where it began */
    project_set_genre(fall.g0);
    project_piece(fall.p0);
    en_genre(fall.g0);
    project_apply();
    mood_apply();
    en_regrow();
    {
        char t[20] = "ALL GENRES, ";
        itoa_u(fall.n, t + 12);
        say("FACTORY DONE", fall.n ? t : "NOTHING WAS STORED");
    }
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

static void piece_empty(void)                   /* (0.17.31) Global, SEL + OCT-: the piece emptied, to build up (SEQ, Bass steps, Acid steps, Pluck steps, Progression) */
{
    project_piece_empty();
    project_apply();
    mood_apply();
    track_fresh(1, 1);
    say("EMPTY PIECE", "SAVE KEEPS IT");
}

/* ------------------------------------------------------------ the kit --- */
static int seq_pat(void) { return en_playing ? en_pat_now % EN_NPAT : proj.par[P_PATTERN]; }   /* the pattern the sequencer shows and edits: playing, the one
                                                 * playing (0.17.24: the song's moves too, the maker's jam); stopped, the one chosen */
static int seq_src(int l) { return en_playing ? en_lane_src[l] % EN_NPAT : seq_pat(); }   /* a lane's: playing, where it really plays from (its own pattern, the song's anchor) */
static int play_src(int l) { return en_playing ? en_lane_src[l] % EN_NPAT : proj.par[P_LSRC + l] ? proj.par[P_LSRC + l] - 1 : en_pat_now % EN_NPAT; }
static uint16_t lane_plan(int l)                /* a lane as it will play: its pattern at its density, within its length */
{
    const int ll = proj.par[P_LLEN + l];
    const uint16_t m = en_lane_mask(l, play_src(l), proj.par[P_DENS + l]);
    return ll >= 1 && ll < 16 ? (uint16_t)(m & ((1u << ll) - 1u)) : m;
}
/* what a lane really plays this bar (0.17.24, "at least on Home it should show what is actually playing"): the steps gone by as they sounded (fills,
 * ghosts, the breakdown, the chances), the rest as they will (its pattern at its density; in a breakdown or a build, as they went a bar ago) */
static uint16_t lane_now(int l)
{
    const uint16_t plan = lane_plan(l);
    uint16_t past;
    if (!en_playing)
        return plan;
    past = (uint16_t)((2u << (en_lane_pos[l] & 15u)) - 1u);
    return (uint16_t)((en_played[l] & past) | ((en_sec == EN_SEC_GROOVE ? plan : en_played[l]) & ~past));
}
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
    en_pats[seq_src(l)][l] = (uint16_t)en_euclid(n, 16, r);
    edited();
    code_pats_keep();
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
    volatile uint16_t *p = &en_pats[seq_src(ui.lane)][ui.lane];
    *p = (uint16_t)(*p ^ (1u << s));
    edited();
    code_pats_keep();
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

/* the order the dial and SEL + FX walk the effects' pages in (their numbers stay: the scopes and the keys use them): Talk and Wah beside the phaser and flanger.
 * The settings are a walk of their own since 0.16.1 (HOME held opens it; the dial walks it): the unit's, the auto-DJ's, the set's, USB and MIDI, the synths'
 * filters, the screen's, the sampling helper. */
static const uint8_t FX_ORDER[] = {1, 2, 3, 21, 22, 4, 5, 6, 7, 8, 9, 10, 11, 23, 12, 13, 14, 15, 16, 28, 29, 30, 19, 20, 40};   /* (40: SNIP LAND, 0.17.72) */   /* (23: the Repeat page, with the beat effects; 28-30: the punch-ins on tracks, FILTER, DELAY, PHASE (SEL + their key opens each)) */
static const uint8_t ST_ORDER[] = {27, 33, 18, 17, 34, 25, 24, 26, 31, 32, 35, 36, 38, 39, 37};   /* (0.17.72: RETURNS, page 39, after RESONATOR) */   /* (0.17.51: RESONATOR, page 38, after CHORUS, before LINK) */   /* FILTERS, SCREAM (first since 0.16.28: they are the piece's now, and a new piece resets them), SETTINGS, AUTO DJ, AUTO FX, SET, USB & MIDI, MIDI IN, SCREEN, SAMPLE, SOUND (0.17.30, last: the walk's places before it stay) */
#define FX_N ((int)sizeof FX_ORDER)
#define ST_N ((int)sizeof ST_ORDER)
static int st_pos(int pg)                       /* where a page is in the settings' walk (0 ..), -1: not a setting */
{
    int i;
    for (i = 0; i < ST_N; i++)
        if (ST_ORDER[i] == pg)
            return i;
    return -1;
}
static int fx_pos(int pg)                       /* where a page is in its walk (0 ..), the effects' or the settings', -1: none */
{
    int i;
    if (st_pos(pg) >= 0)
        return st_pos(pg);
    for (i = 0; i < FX_N; i++)
        if (FX_ORDER[i] == pg)
            return i;
    return -1;
}
static int fx_page_of(int fx)                   /* the edit page (1 ..) of an effect, 0: it has none */
{
    int p;
    if (fx == FX_GATE16 || fx == FX_GATE8)
        fx = FX_GATEX;
    if ((fx >= FX_RPT2 && fx <= FX_RPT32) || fx == FX_RPT8T || fx == FX_RPT16T)   /* the eight repeats share one page */
        fx = FX_RPT2;
    for (p = 0; p < NFXP; p++)
        if (FXP[p].bit == fx)
            return p + 1;
    return 0;
}
static int fx_key_page(int key)                 /* the edit page of the effect a key plays (in this bank), 0: none */
{
    const int fx = KEY_FX[fx_bank][key];
    return fx_bank == 3 ? 20 : fx == FX_FLEX ? 19 : fx == FX_NONE ? 0 : fx_page_of(fx);
}
static int perf_like(void);
static void shift_key(int key)                  /* a key pressed with SEL held */
{
    char t[16];
    int i;
    if (ui.fx_menu && !ui.fx_page) {             /* in the FX menu: SEL + a key opens that effect's settings page */
        const int fx = KEY_FX[fx_bank][key];
        int pg = fx_key_page(key);
        if (fx == FX_FLEX)
            ui.flex_last = FLEX_PAT[key];
        if (pg) {
            ui.fx_page = (uint8_t)pg;
            say(FXP[pg - 1].name, "SETTINGS");
        } else {
            say(fx == FX_NONE ? "NO EFFECT" : fx == FX_FLEX ? FLEX_LABEL[FLEX_PAT[key]] : FX_LABEL[fx], "HAS NO SETTINGS");
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
    if (perf_like() && (key == 13 || key == 25 || key == 17)) {   /* (0.17.23, a Reddit player: move one part on from Perform) the part's solo key */
        if (key == 17) {                         /* A#4 (the drums'): the next of the six patterns, from the next bar */
            const int p = (proj.par[P_PATTERN] + 1) % EN_NPAT;
            knob_set(P_PATTERN, p);
            pat_name(p, t);
            say(t, en_playing ? "NEXT BAR" : 0);
        } else {
            const int pl = key == 25, lk = proj.par[pl ? P_CM_LOCK : P_AC_LOCK];
            if (!lk)
                en_new_part(pl);
            say(pl ? "PLUCK LINE" : "ACID LINE", lk ? "FROZEN: KEPT" : "NEW");
        }
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
            code_pats_keep();
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
        v = f[seq_src(ui.lane)][ui.lane];
    }
    en_pats[seq_src(ui.lane)][ui.lane] = v;
    edited();
    code_pats_keep();
    caps(t, EN_LANE_NAME[ui.lane], sizeof t);
    say(t, restore ? "FACTORY" : "CLEARED");
}

/* ------------------------------------------------------------ buttons --- */
/* the tab chain along the bottom (imp_tabs), and the breadcrumb's names: a tab a voice or group, with its pages */
static const struct { const char *name; char ltr; uint8_t first, n; } TB[10] = {
    {"TEMPO", 'T', V_TEMPO, 1}, {"PAD", 'P', V_PAD, 5}, {"BASS", 'B', V_DRONE, 6}, {"ACID", 'A', V_ACID, 5}, {"PLUCK", 'L', V_CMEL, 4},
    {"DRUMS", 'R', V_DRUMS, 8}, {"SPACE", 'S', V_SPACE, 2}, {"MIX", 'M', V_MASTER, 2}, {"GLOBAL", 'G', V_GLOBAL, 3}, {"JAM", 'J', V_JAM, 1}};
static void set_view(int v)
{
    int g;
    if (v == V_BLSTEP)
        en_bass_capture();                       /* (the Bass steps page: the line heard, if none are set) */
    for (g = 0; g < NGROUP; g++)                 /* remembered: the page last on in its group */
        if (v >= GROUP[g].first && v < GROUP[g].first + GROUP[g].n)
            ui.grp_last[g] = (uint8_t)v;
    if (v != ui.view && v < NDIAL) {             /* the breadcrumb (0.14.7): "DRUMS  PHRASE 8/8" in the header for a moment, on every change of page in a group of more than one */
        int t;
        for (t = 0; t < 10; t++)
            if (v >= TB[t].first && v < TB[t].first + TB[t].n && TB[t].n > 1) {
                char b[24];
                int n;
                caps(b, VIEW_NAME[v], 18);
                for (n = 0; b[n] && b[n] == TB[t].name[n]; n++)
                    ;
                if (b[n] || TB[t].name[n]) {         /* (0.17.39: the group's first page, of the group's name, not said twice: "PAD 1/5") */
                    for (n = 0; b[n]; n++)
                        ;
                    b[n++] = ' ';
                } else
                    n = 0;
                b[n++] = (char)('1' + (v - TB[t].first));
                b[n++] = '/';
                b[n++] = (char)('0' + TB[t].n);
                b[n] = 0;
                say(TB[t].name, b);
                break;
            }
    }
    ui.view = (uint8_t)v;
    if (v < NDIAL)
        ui.last_dial = (uint8_t)v;
    ui.touched = -1;
}

static int voice_of(int v)                      /* the voice page v belongs to (EN_V_*), or -1 */
{
    if (v >= V_PAD && v <= V_CHORDS)
        return EN_V_PAD;
    if (v >= V_DRONE && v <= V_BLANS)
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
        static const char *const WHO[3] = {"PAD", "BASS", "ACID"};
        int p = SOUND_PAR[vc];
        const int ns = vc == 2 ? EN_NACID : EN_NSOUND;
        knob_set(p, (proj.par[p] + d + ns) % ns);
        en_param_text(p, proj.par[p], b);
        caps(a, b, sizeof a);
        say(WHO[vc], a);
    } else if (ui.view == V_CMEL || ui.view == V_CMEL2 || ui.view == V_CMVU || ui.view == V_CMSTEP) {   /* the counter-melody's sound */
        const int ns = en_ncm_snd(), cur = en_cm_snd_pos(proj.par[P_CM_SND]);   /* (the place on the list; a loaded voice that is gone: from the first) */
        knob_set(P_CM_SND, (cur + d + ns) % ns);
        caps(a, EN_CM_SOUND[proj.par[P_CM_SND]], sizeof a);
        say("PLUCK", a);
    } else if (kit_view(ui.view)) {             /* the drums: all four to the next kit */
        int k = (proj.par[P_KIT_KICK] + d + EN_NKITS) % EN_NKITS;
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
    if (v >= V_DRONE && v <= V_BLANS)
        return P_BASSOCT;
    if ((v >= V_ACID && v <= V_ACLINE) || v == V_ACSTEP)
        return P_AC_OCT;
    if (v == V_CMEL || v == V_CMEL2 || v == V_CMVU || v == V_CMSTEP)
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
static void jam_down_v(int i, int vc);
static void jam_up(int i);
static int key_voice(void)                       /* the voice the keys play here (0.17.6; JAM's numbering: 0 pluck, 1 pad, 2 acid, 3 bass) */
{
    const int v = ui.view;
    if (v >= V_PAD && v <= V_PROG)
        return 1;
    if (v >= V_DRONE && v <= V_BLANS)
        return 3;
    if (v >= V_ACID && v <= V_ACSTEP)
        return 2;
    if (v >= V_CMEL && v <= V_CMSTEP)
        return 0;
    return ui.jam_voice & 3;
}
static void draw_jam(void);
static int perf_like(void) { return ui.view == V_PERF || ui.view == V_VIZ; }   /* the screens whose keys are the mix, knobs the FX knobs, ALGORITHM the mood */

static int key_on(int k)                        /* the effect on the FX menu's key k is sounding (a Flex key: its own pattern) */
{
    const int fp = key_flex(k);
    return (int)(ui.fx_mask >> KEY_FX[fx_bank][k] & 1u) && (fp < 0 || fp == ui.flex_cur);
}
static int key_locked(int k)                    /* ... and latched */
{
    const int fp = key_flex(k);
    return (int)(ui.fx_lock >> KEY_FX[fx_bank][k] & 1u) && (fp < 0 || fp == ui.flex_lock);
}

/* THE SWELL (0.17.22, a Reddit player's "temporary mood and FX"): a SEL tap on Perform or a visualizer lifts the mood, the throw and the wash
 * over a bar, then lets them back to where they were over four bars. SEL again on the way: straight into the way back. A turn of any knob
 * ends it where it is (the turned one is yours). */
static struct {
    uint32_t t0;                                 /* when it began (ms), 0 none */
    uint8_t mood, thr, wash;                     /* where they were */
} sw;
static uint32_t sw_bar(void) { return 240000u / (uint32_t)(proj.par[P_TEMPO] > 30 ? proj.par[P_TEMPO] : 30); }   /* a bar, ms */
static void swell_apply(float e)
{
    const int m = (int)sw.mood + (int)(30.0f * e + 0.5f), t = (int)sw.thr + (int)(45.0f * e + 0.5f), w = (int)sw.wash + (int)(45.0f * e + 0.5f);
    ui.mood = (uint8_t)(m > 100 ? 100 : m);
    mood_apply();
    ui.glide_busy = 1;                           /* (not a hand's turn: a glide stays) */
    knob_set(K_FXF + 2, t > 100 ? 100 : t);
    knob_set(K_FXF + 3, w > 100 ? 100 : w);
    ui.glide_busy = 0;
}
static void swell_tap(void)
{
    const uint32_t now = plat_ms(), bar = sw_bar();
    if (sw.t0) {                                 /* on its way: back from where it is */
        const uint32_t t = now - sw.t0;
        const float e = t < bar ? (float)t / (float)bar : 1.0f - (float)(t - bar) / (4.0f * (float)bar);
        if (t < bar)
            sw.t0 = now - bar - (uint32_t)((1.0f - e) * 4.0f * (float)bar);
        say("SWELL", "BACK");
        return;
    }
    sw.t0 = now ? now : 1u;
    sw.mood = ui.mood;
    sw.thr = (uint8_t)ui.fxk[2];
    sw.wash = (uint8_t)ui.fxk[3];
    say("SWELL", "UP A BAR, BACK OVER FOUR");
}
static void swell_tick(void)
{
    uint32_t t, bar;
    float e;
    if (!sw.t0)
        return;
    if ((int32_t)(ui.turn_t - sw.t0) > 0) {      /* a knob turned: it ends here */
        sw.t0 = 0;
        return;
    }
    t = plat_ms() - sw.t0;
    bar = sw_bar();
    e = t < bar ? (float)t / (float)bar : 1.0f - (float)(t - bar) / (4.0f * (float)bar);
    if (e <= 0.0f) {
        swell_apply(0.0f);                       /* back exactly where they were */
        sw.t0 = 0;
        return;
    }
    swell_apply(e);
}

static void button(int b)
{
    int g;
    if (b == B_SEL) {                            /* SEL tapped alone (0.17.22): on Perform or a visualizer, the swell */
        if (perf_like() && !ui.fx_menu && plat_ms() - ui.down_t[B_SEL] < 400u)
            swell_tap();
        return;
    }
    if (b == B_HOME) {                           /* HOME: the tempo page; again, the perform screen; and back; twice quickly, the visualizers */
        const uint32_t now = plat_ms();
        if (ui.fx_menu) {                        /* HOME leaves the FX menu too (0.14.7; latched effects play on), then does its usual */
            ui.fx_menu = 0;
            ui.fx_page = 0;
            ui.fx_open_t = 0;
        }
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
            if (ui.fx_menu) {                    /* a page's button leaves the FX menu (latched effects play on) */
                ui.fx_menu = 0;
                ui.fx_page = 0;
                ui.fx_open_t = 0;
            }
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
        if (ui.view == V_CODE && code_typed) {   /* the Code page, a code typed or rolled: it starts (playing or not) */
            code_go();
        } else if (en_playing) {
            en_play(0);
            say("STOPPED", 0);
        } else {
            char t[12];
            uint8_t g[CODE_N];
            track_fresh(ui.view != V_CODE, 0);   /* a fresh track (the Code page: the one playing again, from the top) */
            af_fresh = 1;
            sl_start();
            en_play(1);
            if (ui.intro == 5 && !ui.dj_on)      /* Intro BREAK: from Play, the breakdown first */
                en_section(EN_CMD_BREAK);
            code_now(g);
            code_text(g, t);
            say("PLAYING", t);
        }
        break;
    case B_REC:
        if (ui.view == V_JAM && !ui.fx_menu) {   /* (0.17.46, "can we make rec all you need to hit on the jam page?") on JAM: the phrase recorder */
            phrase_key();
            break;
        }
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        if (ui.fx_menu && !ui.fx_page && b == B_OCTDN) {   /* in FX, OCT-: everything back (effects, knobs, mutes), undoable */
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
            say(p == P_PAD_OCT ? "PAD OCTAVE" : p == P_BASSOCT ? "BASS OCTAVE" : p == P_AC_OCT ? "ACID OCTAVE" : p == P_CM_OCT ? "PLUCK OCTAVE" : "SONG OCTAVE", t);
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

/* PADS ON MIDI IN (0.15.8, the MIDI IN page's Notes: PADS): a pad controller plays PERF, sixteen pads a bank from note 36 (an MPC's default
 * pad map: bank A 36-51, B 52-67 ...; most pad controllers start at 36 too). Bank A: pads 1-6 the drums (kick, snare, clap, closed hat, open
 * hat, perc), 7 all the drums muted or back, 9 breakdown, 10 build, 11 drop, 12 the next chord, 13 a fill, 14 freeze, 15 the auto-DJ, 16 the
 * next piece (on the bar). Banks B, C, D: the pluck, the bass, the acid, a pad a note of the scale up from the root (as the Jam page's HARP),
 * so every pad is in key. Bank E: pads 1-7 the chord on the scale's degrees I-VII, at once. */
#define PADS_BASE 36
static int pads_tone(int p, int base)           /* the p-th tone of the scale up from the root, from base */
{
    const uint32_t pool = pool_abs();
    int m, cnt = 0;
    for (m = base + sound_root(); m < 128; m++)
        if (pool >> (m % 12) & 1u) {
            if (cnt == p)
                return m;
            cnt++;
        }
    return 60;
}
static uint8_t tap_n[6];                         /* the drums tapped from MIDI, each (the status log's: did a stray kick come in, 0.16.6) */
static void pads_in(int on, int n, int v)
{
    static const uint8_t VC[3] = {EN_V_PLUCK, EN_V_DRONE, EN_V_ACID};
    static const int8_t OFS[3] = {0, 24, 12};    /* (the engine plays the drone two octaves and the acid one below the key, as on Jam) */
    const int k = n - PADS_BASE, bank = k >> 4, p = k & 15;
    if (k < 0 || bank > 4)
        return;
    if (bank >= 1 && bank <= 3) {
        uint8_t *mem = &ui.pad_note[(bank - 1) * 16 + p];
        if (on) {
            const int note = pads_tone(p, 48) + OFS[bank - 1];
            if (bank == 1 && proj.par[P_CM_LVL] < 15)
                knob_set(P_CM_LVL, 40);          /* the pluck has to be audible to be played */
            if (*mem)
                en_note(VC[bank - 1], *mem, 0);
            *mem = (uint8_t)(note > 127 ? 127 : note);
            en_note_v(VC[bank - 1], *mem, v);    /* (the pad's velocity: 0.15.23, they all played at 100) */
        } else if (*mem) {
            en_note(VC[bank - 1], *mem, 0);
            *mem = 0;
        }
        return;
    }
    if (!on)
        return;
    if (bank == 4) {
        if (p < 7) {
            static const char *const RN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
            en_chord_deg(p);
            say("CHORD", RN[p]);
        }
        return;
    }
    if (p < 6) {
        tap_n[p]++;
        en_drum_tap(p, v);
        return;
    }
    switch (p) {
    case 6:
        undo_save();
        ui.trk_m = (ui.trk_m & 15u) == 15u ? (uint8_t)(ui.trk_m & ~15u) : (uint8_t)(ui.trk_m | 15u);
        en_trk(ui.trk_m, ui.trk_s);              /* (0.16.12: it was only shown, the engine never heard it) */
        say((ui.trk_m & 15u) ? "MUTE" : "UNMUTE", "ALL DRUMS");
        break;
    case 8: en_section(EN_CMD_BREAK); say("BREAKDOWN", "NEXT BAR"); break;
    case 9: en_section(EN_CMD_BUILD); say("BUILD", "NEXT BAR"); break;
    case 10: en_section(EN_CMD_DROP); say("DROP", "NEXT BAR"); break;
    case 11: en_next(); say("NEXT CHORD", "ON THE BAR"); break;
    case 12: en_fill_next(); say("FILL", "NEXT BAR"); break;
    case 13: say("FREEZE", en_frozen ? "OFF" : "ON"); en_freeze(!en_frozen); break;
    case 14:
        ui.dj_on = (uint8_t)!ui.dj_on;
        ui.dj_phase = 0;
        ui.dj_n = 0;
        ui.dj_round = 0;
        ui.dj_fill = 0;
        say("AUTO-DJ", ui.dj_on ? "ON" : "OFF");
        break;
    case 15:
        if (en_playing) {
            ui.piece_q = (uint8_t)((proj.piece + 1) % NPIECES + 1);
            say("NEXT PIECE", "NEXT BAR");
        }
        break;
    default: break;
    }
}

/* A Model:Samples (MIDI IN's Notes: M:S, 0.15.22): its six tracks send on channels 1-6, each at its own note, so the channel says what: a trig on
 * track 1 the next chord, 2 a fill, 3 a breakdown, 4 a build, 5 the drop, 6 the next piece (the sections and pieces on the bar, as the pads' are) */
static void ms_in(int ch)
{
    static const uint8_t PAD[6] = {11, 12, 8, 9, 10, 15};
    if (ch < 6)
        pads_in(1, PADS_BASE + PAD[ch], 100);
}
/* the FX knobs as CCs (MIDI IN's Knob CCs, 0.15.22), the numbers a Model:Samples' knobs send: its filter (74, open in the middle, as Filter here),
 * resonance (71: Crush), delay send (12: Throw), reverb send (13: Wash); and the mod wheel (1) for the mood. Out: the same four, as they turn. */
static const uint8_t CC_NUM[4] = {74, 71, 12, 13};
static void cc_in(int cc, int v)
{
    int k;
    if (cc == 1) {
        ui.mood = (uint8_t)((v * 100 + 63) / 127);
        mood_apply();
        return;
    }
    for (k = 0; k < 4; k++)
        if (cc == CC_NUM[k]) {
            const int x = k == 0 ? (v == 64 ? 0 : (v - 64) * 50 / (v < 64 ? 64 : 63)) : (v * 100 + 63) / 127;
            knob_set(K_FXF + k, x);
            ui.cc_sent[k] = ui.fxk[k];           /* (not sent back out) */
        }
}
static void cc_out(void)                        /* each frame: an FX knob moved since it was last sent goes out */
{
    int k;
    if (!(ui.cc_mode & 2u))
        return;
    for (k = 0; k < 4; k++)
        if (ui.fxk[k] != ui.cc_sent[k]) {
            const int v = ui.fxk[k];
            ui.cc_sent[k] = (int8_t)v;
            en_midi_cc(CC_NUM[k], (uint32_t)(k == 0 ? (v <= 0 ? 64 + v * 64 / 50 : 64 + v * 63 / 50) : (v * 127 + 50) / 100));
        }
}

/* An Akai MPK mini (MIDI IN's Notes: MPK, 0.15.23; its factory program: keys on channel 1, pads on 10 from note 36, knobs CC 24-31 on the MK4 (70-77 on
 * older ones: both are taken), the mod wheel CC 1).
 * The keys play the JAM page's voice, with their velocity, snapped down to the scale (the JAM page's Chords: a triad of the scale); the pads are the
 * pads' bank A (pads_in: 1-6 the drums, 7 all the drums muted, 9 breakdown .. 16 the next piece: the MPK's bank B is pads 9-16); the knobs Filter,
 * Crush, Throw, Wash, the acid's cutoff and reso, the bass's cutoff and the mood (0.15.26; it was the reverb); the pitch bend and the mod wheel (a vibrato)
 * on the voice the keys play (en_bend, en_modw). */
static int jam_snap(int m, uint32_t pool);
#define MPK_N 8
/* THE SAMPLING HELPER (the SAMPLE settings page, 0.16.1): PERF's sounds one at a time for a sampler (an MPC sampling its input, say), so a project keeps
 * them without the FM-1. ALL, DRUMS or SYNTHS: a key starts a run (a key again stops it): the music stops, a count-in of 1.5 s, then each sound once,
 * Gap apart: the six drums, then the pad (the scale's triad on the root), the bass, the acid and the pluck on the root, held for three quarters of the
 * gap; recorded as one sample, it chops into pads. BY KEY: white keys 1-6 the drums, 7-10 the pad, bass, acid and pluck, held as long as the key.
 * FX OFF (dry): no echo, reverb, latched effects or FX knobs while the page is up. */
#define SMP_N 10
#define SMP_COUNT 1500u
static const char *const SMP_NAME[SMP_N] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "PERC", "PAD", "BASS", "ACID", "PLUCK"};
static void smp_release(void)                    /* the held notes let go */
{
    int k;
    for (k = 0; k < ui.smp_nn; k++)
        en_note(ui.smp_vc, ui.smp_note[k], 0);
    ui.smp_nn = 0;
    ui.smp_off = 0;
}
static void smp_play(int it)                     /* a sound: a drum, or a voice's note (the pad: the triad), held */
{
    static const uint8_t VC[4] = {EN_V_PAD, EN_V_DRONE, EN_V_ACID, EN_V_PLUCK};
    static const int8_t OFS[4] = {0, 24, 12, 0};   /* (the engine plays the drone two octaves and the acid one below the key, as on Jam) */
    int k;
    smp_release();
    if (en_playing)
        en_play(0);                              /* the sound alone */
    ui.smp_cur = (int8_t)it;
    if (it < 6) {
        en_drum_tap(it, 110);
        return;
    }
    it -= 6;
    if (it == 3 && proj.par[P_CM_LVL] < 15)
        knob_set(P_CM_LVL, 40);                  /* the pluck has to be audible */
    ui.smp_vc = VC[it];
    ui.smp_note[ui.smp_nn++] = (uint8_t)(pads_tone(0, 48) + OFS[it]);
    if (it == 0) {
        ui.smp_note[ui.smp_nn++] = (uint8_t)pads_tone(2, 48);
        ui.smp_note[ui.smp_nn++] = (uint8_t)pads_tone(4, 48);
    }
    for (k = 0; k < ui.smp_nn; k++)
        en_note_v(ui.smp_vc, ui.smp_note[k], 100);
}
static int smp_first(void) { return ui.smp_mode == 2 ? 6 : 0; }
static int smp_last(void) { return ui.smp_mode == 1 ? 5 : SMP_N - 1; }
static void smp_stop(void)
{
    smp_release();
    ui.smp_t0 = 0;
    ui.smp_cur = -1;
}
static void smp_keys(uint32_t down, uint32_t up)   /* the SAMPLE page's keys */
{
    int s;
    if (ui.smp_mode != 3) {                      /* a run: any key starts it, any key stops it */
        if (!down)
            return;
        if (ui.smp_t0) {
            smp_stop();
            say("SAMPLE", "STOPPED");
        } else {
            if (en_playing)
                en_play(0);
            ui.smp_t0 = plat_ms() | 1u;
            ui.smp_cur = -1;
            say("SAMPLE", "RECORD NOW");
        }
        return;
    }
    for (s = 0; s < SMP_N; s++) {                /* by key: white 1-10, a sound each */
        const int key = WHITE_KEY[s];
        if (down >> key & 1u)
            smp_play(s);
        else if ((up >> key & 1u) && ui.smp_cur == s) {
            smp_release();
            ui.smp_cur = -1;
        }
    }
}
static void smp_tick(void)                       /* each frame: the run's next sound, the notes' let-go, dry while the page is up */
{
    const int on = ui.fx_menu && ui.fx_page == FXP_SAMPLE + 1, dry = on && !ui.smp_fx, gap = SMP_GAP_MS[ui.smp_gap & 3];
    const uint32_t now = plat_ms();
    if (dry != ui.smp_dry) {                     /* dry: the sends and the FX knobs to nothing (the effects: ui_input's mask), and back */
        int k;
        ui.smp_dry = (uint8_t)dry;
        en_set(P_ECHO, dry ? 0 : proj.par[P_ECHO]);
        en_set(P_REVERB, dry ? 0 : proj.par[P_REVERB]);
        for (k = 0; k < 4; k++)
            en_fxk(k, dry ? 0 : ui.fxk[k]);
    }
    if (!on) {
        if (ui.smp_t0 || ui.smp_nn)
            smp_stop();
        return;
    }
    if (ui.smp_off && (int32_t)(now - ui.smp_off) >= 0)
        smp_release();
    if (ui.smp_t0 && ui.smp_mode != 3) {
        const int32_t t = (int32_t)(now - ui.smp_t0) - (int32_t)SMP_COUNT;
        const int i = t < 0 ? -1 : smp_first() + t / gap;
        if (i > smp_last() && !ui.smp_nn && t >= (smp_last() - smp_first() + 1) * gap) {
            smp_stop();
            say("SAMPLE", "DONE");
        } else if (i >= 0 && i <= smp_last() && i != ui.smp_cur) {
            smp_play(i);
            if (ui.smp_nn)
                ui.smp_off = (now + (uint32_t)(gap * 3 / 4)) | 1u;
        }
    }
}
static const uint8_t MPK_VC[4] = {EN_V_PLUCK, EN_V_PAD, EN_V_ACID, EN_V_DRONE};   /* the JAM page's voice, as the engine's */
static struct { uint8_t in, n, vc, out[3]; } mpk[MPK_N];   /* the keys held: the note in, and the notes played for it */
static void midi_key(int on, int n, int v, int vc);
static void mpk_key(int on, int n, int v) { midi_key(on, n, v, ui.jam_voice & 3); }
static void midi_key(int on, int n, int v, int vc)   /* a note from outside on voice vc (JAM's numbering), snapped to the scale as on JAM; the MPK's keys,
                                                      * and since 0.17.7 any keyboard's in the MIDI IN page's default mode */
{
    static const int8_t OFS[4] = {0, 0, 12, 24};   /* (the engine plays the acid an octave and the drone two below the key, as on JAM) */
    const uint8_t *const VC = MPK_VC;
    const uint32_t pool = pool_abs();
    int s, k, m, cnt, t[5], r;
    vc &= 3;
    for (s = 0; s < MPK_N; s++)                  /* this note let go (or played again) */
        if (mpk[s].n && mpk[s].in == n) {
            for (k = 0; k < mpk[s].n; k++)
                en_note(mpk[s].vc, mpk[s].out[k], 0);
            mpk[s].n = 0;
        }
    if (!on)
        return;
    for (s = 0; s < MPK_N && mpk[s].n; s++)
        ;
    if (s == MPK_N) {                            /* nine held: the first one goes */
        s = 0;
        for (k = 0; k < mpk[0].n; k++)
            en_note(mpk[0].vc, mpk[0].out[k], 0);
    }
    if (vc == 0 && proj.par[P_CM_LVL] < 15)
        knob_set(P_CM_LVL, 40);                  /* the pluck has to be audible to be played */
    r = jam_snap(n, pool);
    mpk[s].in = (uint8_t)n;
    mpk[s].vc = VC[vc];
    mpk[s].out[0] = (uint8_t)(r < 0 ? 0 : r);
    mpk[s].n = 1;
    if (ui.jam_mode == 1) {                      /* a triad of the scale: its third and fifth */
        for (m = r + 1, cnt = 0; m < r + 25 && cnt < 5; m++)
            if (pool >> (m % 12) & 1u)
                t[cnt++] = m;
        if (cnt >= 4) {
            mpk[s].out[1] = (uint8_t)(t[1] > 127 ? 127 : t[1]);
            mpk[s].out[2] = (uint8_t)(t[3] > 127 ? 127 : t[3]);
            mpk[s].n = 3;
        }
    }
    for (k = 0; k < mpk[s].n; k++) {
        m = mpk[s].out[k] + OFS[vc];
        mpk[s].out[k] = (uint8_t)(m > 127 ? 127 : m);
        en_note_v(VC[vc], mpk[s].out[k], v);
    }
}
static void mpk_cc(int cc, int v)               /* the knobs: CC 24-31 (the MK4's factory program, found on the maker's: 0.15.25), or 70-77 (older MPK minis') */
{
    static const uint8_t P[3] = {P_AC_CUT, P_AC_RES, P_DR_CUT};
    const int k = cc >= 24 && cc <= 31 ? cc - 24 : cc >= 70 && cc <= 77 ? cc - 70 : -1;
    /* the MK4's knobs are endless and send steps, not places (0.15.27: they jumped end to end): 1 .. 63 that many clicks right, 127 .. 65 left
     * (two's complement); the older MPK minis' 70-77 send places, 0 .. 127 */
    const int rel = cc >= 24 && cc <= 31, d = v < 64 ? v : v - 128;
    int lo, hi;
    if (cc == 1) {                               /* the mod wheel: a vibrato on the voice the keys play (0.15.26; it was the mood) */
        en_modw(MPK_VC[ui.jam_voice & 3], v);
    } else if (k >= 0 && k < 4 && rel) {         /* the FX knobs, a click 2 of their 100 */
        knob_set(K_FXF + k, ui.fxk[k] + 2 * d);
        ui.cc_sent[k] = ui.fxk[k];               /* (not sent back out) */
    } else if (k >= 0 && k < 4) {                /* as Knob CCs' */
        static const uint8_t AS[4] = {74, 71, 12, 13};
        cc_in(AS[k], v);
    } else if (k == 7) {                         /* the eighth: the mood (0.15.26, the maker's ask; it was the reverb) */
        if (rel) {
            const int m = (int)ui.mood + 2 * d;
            ui.mood = (uint8_t)(m < 0 ? 0 : m > 100 ? 100 : m);
            mood_apply();
        } else {
            cc_in(1, v);
        }
    } else if (k >= 4) {
        const int p = P[k - 4];
        knob_range(p, &lo, &hi);
        if (rel) {
            const int step = (hi - lo) / 50 > 1 ? (hi - lo) / 50 : 1;   /* (about fifty clicks end to end, as on the FX knobs) */
            knob_set(p, knob_get(p) + step * d);
        } else {
            knob_set(p, lo + (v * (hi - lo) + 63) / 127);
        }
    }
}

/* THE PLAYER'S MIDI MAP (0.16.11): any controller's notes and CCs as the panel's buttons, keys and knobs, learned in the emulator (web/emu: MIDI learn)
 * and sent from the Install page into flash (OBJ_MMAP). An entry: the channel, a CC (0x10) or a note, how a knob's CC counts (bits 5-6: 0 a place,
 * 1 64 +- clicks, 2 1 up / 127 down) | the note or CC | the control (0-13 a button B_*, 32 + k a key, 64 + e a knob EN_*). A learned message is
 * taken before anything else MIDI does, and acts as the panel's own: a button or key held while its note or CC (64 and over) is down, a knob
 * turned (a place: a click every two steps, as the emulator counts). The panel reads the hardware and the map together (in_buttons and the rest,
 * standing in for plat_buttons in this file). MASTER is the hardware's alone (the emulator's learned MASTER is not sent).
 * A SETTING (0.16.18): bit 7 of the first byte set, a CC, the control byte a parameter (0 .. P_NPARAMS - 1): the CC sets that setting straight, on any
 * page, as its knob would (a place: 0 .. 127 over its range; 64 +- n or 1 up / 127 down: steps), learned on the editor page. A map with settings is
 * "MAP2", so a firmware before them refuses it whole instead of taking a setting for a button. */
#define MM_MAGIC 0x3150414Du                     /* "MAP1" */
#define MM_MAGIC2 0x3250414Du                    /* "MAP2": with settings */
#define MM_MAX 48
typedef struct { uint32_t magic; uint8_t n, pad[3]; uint8_t e[MM_MAX][3]; } mmap_t;
static mmap_t mm;
static uint32_t mm_b, mm_k;                      /* the buttons and keys held from the map */
static int16_t mm_e[NE];                         /* the knobs' clicks from the map, not yet read */
static uint8_t mm_last[MM_MAX], mm_acc[MM_MAX];  /* a place-sending knob: its last value (+1; 0 none yet) and its half click */
static int mm_ok(const uint8_t *b, uint32_t len)   /* a map as the Install page sends it, every entry a control there is (bytes: a restore's may be unaligned) */
{
    uint32_t i;
    const uint32_t mg = len >= 4u ? (uint32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24) : 0u;
    if (len != sizeof(mmap_t) || (mg != MM_MAGIC && mg != MM_MAGIC2) || b[4] > MM_MAX)
        return 0;
    for (i = 0; i < b[4]; i++) {
        const uint8_t *e = b + 8u + 3u * i;
        const uint32_t t = e[2];
        if ((e[0] & 0x80u) ? !(e[0] & 0x10u) || t >= (uint32_t)P_NPARAMS   /* a setting: a CC, a parameter there is */
                         : !(t < NB || (t >= 32u && t < 32u + NKEYS) || (t >= 64u && t < 64u + NE)))
            return 0;
        if (e[1] > 127u || (e[0] >> 5 & 3u) == 3u)
            return 0;
    }
    return 1;
}
static void mm_load(void)
{
    if (plat_store_load(OBJ_MMAP, &mm, sizeof mm) != (int)sizeof mm || !mm_ok((const uint8_t *)&mm, sizeof mm))
        mm.n = 0;
}
static void set_mode(int m);
static void mm_set(int p, uint32_t mode, uint32_t v)   /* a learned setting (0.16.18): as its knob sets it, wherever the panel is */
{
    int lo, hi, x, i;
    char t[12];
    knob_range(p, &lo, &hi);
    if (mode == 0u)                              /* a place: the CC's 0 .. 127 over the setting's range */
        x = lo + ((int)v * (hi - lo) + 63) / 127;
    else {                                       /* steps: one a click (a long range: about a hundred clicks end to end) */
        const int d = mode == 1u ? (int)v - 64 : v < 64u ? (int)v : (int)v - 128;
        x = knob_get(p) + d * ((hi - lo) / 100 > 1 ? (hi - lo) / 100 : 1);
    }
    x = x < lo ? lo : x > hi ? hi : x;
    if (x == knob_get(p))                        /* (the knob's own count: the pluck's sound is a place on its list) */
        return;
    if (p == P_MODE)                             /* (the scale: its notes too, as the ALGORITHM dial sets it) */
        set_mode(x);
    else
        knob_set(p, x);
    ui.act_t = plat_ms();
    for (i = 0; i < 4; i++)                      /* on the page: that knob lit (and zoomed, with ZOOM on) */
        if (view_knob(i) != K_NONE && knob_par(view_knob(i)) == p) {
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u, ui.tick_until = plat_ms() + 90u;
            return;
        }
    if (p != P_MODE) {                           /* elsewhere: its name and value a moment */
        en_param_text(p, proj.par[p], t);
        say(en_param_info(p)->name, t);
    }
}
static int mm_apply(uint32_t st, uint32_t n, uint32_t v)   /* a learned message: acted on (1) */
{
    const uint32_t ty = st & 0xF0u, cc = ty == 0xB0u;
    uint32_t i;
    if (!mm.n || (ty != 0x90u && ty != 0x80u && !cc))
        return 0;
    for (i = 0; i < mm.n; i++) {
        const uint8_t *e = mm.e[i];
        const uint32_t t = e[2];
        if ((e[0] & 15u) != (st & 15u) || (uint32_t)((e[0] >> 4) & 1u) != cc || e[1] != n)
            continue;
        if (e[0] & 0x80u)                        /* a setting (0.16.18) */
            mm_set((int)t, e[0] >> 5 & 3u, v);
        else if (t < 64u) {                           /* a button or a key: held while down */
            const int down = cc ? v >= 64u : ty == 0x90u && v;
            uint32_t *h = t < NB ? &mm_b : &mm_k;
            const uint32_t bit = 1u << (t < NB ? t : t - 32u);
            *h = down ? *h | bit : *h & ~bit;
        } else if (cc) {                         /* a knob: clicks */
            const uint32_t mode = e[0] >> 5 & 3u;
            int d;
            if (mode == 1u)
                d = (int)v - 64;
            else if (mode == 2u)
                d = v < 64u ? (int)v : (int)v - 128;
            else {
                d = mm_last[i] ? (int)v - (int)(mm_last[i] - 1u) + (int8_t)mm_acc[i] : 0;
                mm_last[i] = (uint8_t)(v + 1u);
                mm_acc[i] = (uint8_t)(int8_t)(d % 2);
                d /= 2;
            }
            mm_e[t - 64u] = (int16_t)(mm_e[t - 64u] + d);
        }
        return 1;
    }
    return 0;
}
static uint32_t in_buttons(void) { return plat_buttons() | mm_b; }
static uint32_t in_keys(void) { return plat_keys() | mm_k; }
static int32_t in_enc(int role)
{
    int32_t d = plat_enc(role);
    if ((unsigned)role < NE) {
        d += mm_e[role];
        mm_e[role] = 0;
    }
    return d;
}
#define plat_buttons in_buttons                  /* (the panel, from here on: the hardware and the map) */
#define plat_keys in_keys
#define plat_enc in_enc

/* MIDI IN's CHORDS (0.17.31, a YouTube commenter: "following chords played by MIDI in, Yamaha arranger chord recognition as the basics, four
 * finger bass inversions"): the keys below middle C are an arranger's left hand. What they hold is read as a chord, a moment after the last key
 * goes down (so a chord's keys landing one by one are read once), and the song follows it until the next: the pad plays it, the scale bends to
 * it, the bass plays its lowest key (an inversion, or a slash chord: C/D). One key: the scale's chord on it (a key outside the scale: major).
 * Letting go keeps it (an arranger's memory). From middle C up the keys play the voice, as KEYS does. Another Notes setting: the walk again. */
static const struct { uint16_t iv; char nm[6]; } CHORD_T[] = {   /* intervals over the root (bit 0 the root), the most usual first */
    {0x091, ""}, {0x089, "m"}, {0x491, "7"}, {0x891, "maj7"}, {0x489, "m7"}, {0x049, "dim"}, {0x111, "aug"}, {0x0A1, "sus4"}, {0x085, "sus2"},
    {0x449, "m7b5"}, {0x249, "dim7"}, {0x889, "mmaj7"}, {0x4A1, "7sus4"}, {0x291, "6"}, {0x289, "m6"}, {0x095, "add9"}, {0x08D, "madd9"},
    {0x495, "9"}, {0x895, "maj9"}, {0x48D, "m9"}, {0x451, "7b5"}, {0x511, "7#5"}, {0x411, "7"}, {0x811, "maj7"}, {0x409, "m7"}, {0x081, "5"}};
static const char *const PC_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static uint64_t ci_held;                         /* the keys held below middle C */
static uint32_t ci_due;                          /* when to read them (0: nothing new) */
static uint8_t ci_was;                           /* the Notes setting a moment ago (leaving CHORDS: the walk again) */
static int ci_match(uint32_t m, int r)           /* the chord on root r these pitch classes are, or -1 */
{
    const uint32_t iv = ((m >> r) | (m << (12 - r))) & 0xFFFu;
    int k;
    for (k = 0; k < (int)(sizeof CHORD_T / sizeof CHORD_T[0]); k++)
        if (CHORD_T[k].iv == iv)
            return k;
    return -1;
}
static void ci_read(void)
{
    uint32_t m = 0, m2;
    int lo = -1, n, k, r = -1, t = -1, cnt = 0, bass;
    char nm[16];
    for (n = 0; n < 60; n++)
        if (ci_held >> n & 1u) {
            if (lo < 0)
                lo = n;
            m |= 1u << (n % 12);
        }
    if (lo < 0)
        return;
    bass = lo % 12;
    for (k = 0; k < 12; k++)
        cnt += (int)(m >> k & 1u);
    if (cnt == 1 || (cnt == 2 && ci_match(m, bass) >= 0)) {   /* one key (or it and its fifth): the scale's chord on it, or major */
        const uint32_t pool = pool_abs();
        if (pool >> bass & 1u) {
            int pcs[4], nn, d = 0, j;
            for (j = 0; j < 12; j++) {      /* its degree: the scale's tones counted up from the key's root */
                const int pc = (proj.par[P_ROOT] + j) % 12;
                if (pc == bass)
                    break;
                d += (int)(pool >> pc & 1u);
            }
            nn = en_prog_chord(d, 0, pcs);
            m = 0;
            for (j = 0; j < nn; j++)
                m |= 1u << (pcs[j] % 12);
            r = pcs[0] % 12;
            t = ci_match(m, r);
        } else {
            m = 1u << bass | 1u << ((bass + 4) % 12) | 1u << ((bass + 7) % 12);
            r = bass;
            t = 0;
        }
    } else {
        if ((t = ci_match(m, bass)) >= 0) {      /* the lowest key the root: as played */
            r = bass;
        } else {                                 /* else an inversion (the root among the others) or a chord over another bass (a slash chord):
                                                  * the plainer of the two (C E G over D is C/D, not Cadd9/D; E G C is C/E) */
            int t1 = -1, r1 = -1, t2 = -1, r2 = -1;
            for (k = 0; k < 12 && t1 < 0; k++)
                if ((m >> k & 1u) && (t1 = ci_match(m, k)) >= 0)
                    r1 = k;
            m2 = m & ~(1u << bass);
            for (k = 0; k < 12 && t2 < 0; k++)
                if ((m2 >> k & 1u) && (t2 = ci_match(m2, k)) >= 0)
                    r2 = k;
            if (t2 >= 0 && (t1 < 0 || t2 < t1)) {
                t = t2;
                r = r2;
                m = m2;
            } else {
                t = t1;
                r = r1;
            }
        }
    }
    if (t < 0) {
        say("CHORD?", "NOT ONE I KNOW");
        return;
    }
    en_ext_chord(r, bass == r ? -1 : bass, m | 1u << r);
    {
        const char *a = PC_NAME[r], *b = CHORD_T[t].nm;
        int i = 0;
        while (*a && i < 15)
            nm[i++] = *a++;
        while (*b && i < 15)
            nm[i++] = *b++;
        if (bass != r && i < 12) {
            a = PC_NAME[bass];
            nm[i++] = '/';
            while (*a && i < 15)
                nm[i++] = *a++;
        }
        nm[i] = 0;
        say("CHORD IN", nm);                     /* (as written: "Cm7" and "CM7" are two chords) */
    }
}
static void ci_key(int on, int n)                /* a key below middle C */
{
    if (on) {
        ci_held |= 1ull << n;
        ci_due = plat_ms() + 30u;
        if (!ci_due)
            ci_due = 1u;
    } else {
        ci_held &= ~(1ull << n);
    }
}
static void ci_tick(void)
{
    if (ui.min_pads != ci_was) {
        if (ci_was == 4) {
            en_ext_chord(0, -1, 0);              /* CHORDS off: the walk again, from the chord playing */
            ci_held = 0;
            ci_due = 0;
        }
        ci_was = ui.min_pads;
    }
    if (ci_due && (int32_t)(plat_ms() - ci_due) >= 0) {
        ci_due = 0;
        ci_read();
    }
}

/* THE LINK (0.17.47; the messages: amb.c, lk_send). A follower takes the lead's key, mode, scale and chord (the chord as one from outside, as
 * MIDI in's CHORDS: its own walk stops while linked) and, with Sections on FOLLOW, its breakdowns, builds and drops on the same bar. */
static void lk_in(int n, int v)                  /* n: the CC + 82 (102 hello .. 113 the section) */
{
    if (n == 102) {
        lk_heard = plat_ms() | 1u;
        lk_pk = (uint8_t)v;
        return;
    }
    if (lk_role != 2)
        return;                                  /* (only a follower takes the lead's) */
    switch (n) {
    case 105:
        if (v < 12 && proj.par[P_ROOT] != v) {
            proj.par[P_ROOT] = (int16_t)v;
            en_set(P_ROOT, v);
        }
        break;
    case 106:
        if (v < EN_NMODES && proj.par[P_MODE] != v) {
            proj.par[P_MODE] = (int16_t)v;
            en_set(P_MODE, v);
        }
        break;
    case 107: case 109: case 110:
        lk_lo[n == 107 ? 0 : n - 108] = (uint8_t)v;
        break;
    case 108: {
        const uint32_t p = (uint32_t)lk_lo[0] | (uint32_t)v << 6;
        if ((p & 1u) && pool_count(p) >= 3 && p != proj.pool) {
            proj.pool = (uint16_t)p;
            en_pool(p);
        }
        break;
    }
    case 111: {
        const uint32_t m = (uint32_t)lk_lo[1] | (uint32_t)lk_lo[2] << 6;
        if (v < 12 && m && (m | (uint32_t)v << 12) != lk_cm) {   /* (the four-bar repeat of the same chord: left alone) */
            lk_cm = (uint16_t)(m | (uint32_t)v << 12);
            en_ext_chord(v, -1, m | 1u << v);
        }
        break;
    }
    case 112:
        if (v >= 1 && v <= 3 && ui.fxs[EN_FXS_LK_SEC])
            en_section(v - 1);
        break;
    case 113:                                    /* the lead's section now: one missed, caught up on the next bar */
        if (v <= 2 && ui.fxs[EN_FXS_LK_SEC] && en_playing && v != en_sec && !en_sec_req)
            en_section(v == EN_SEC_BREAK ? EN_CMD_BREAK : v == EN_SEC_BUILD ? EN_CMD_BUILD : EN_CMD_DROP);
        break;
    }
}
static int lk_mode(void)                         /* the Link setting as OFF / AUTO / LEAD / FOLLOW (CABLE LEAD leads, CABLE FOLLOW follows) */
{
    const int m = ui.fxs[EN_FXS_LK_MODE];
    return m >= 4 ? m - 2 : m;
}
static int lk_want(void) { return lk_mode() == 2 || (lk_mode() == 1 && ui.usb_host); }   /* this one would lead */
static void lk_tick(void)                        /* who leads, what this one plays: to the engine when it changes */
{
    static const uint8_t PLAYS_MUTE[5] = {0x00u, 0xE0u, 0x1Fu, 0xF0u, 0x0Fu};   /* the tracks left to the partner (EN_TR_*: kick, hats, snare, perc, bass, acid, pad, pluck) */
    static uint8_t q_s = 0xFFu, m_s = 0xFFu;
    const int mode = lk_mode(), want = lk_want();
    if (plat_wire(ui.fxs[EN_FXS_LK_MODE] >= 4 ? ui.fxs[EN_FXS_LK_MODE] - 3 : 0) < 0) {   /* (CABLE: the port's wires, the lead's D+) */
        ui.fxs[EN_FXS_LK_MODE] = 0;
        say("LINK CABLE", "USB HOST OFF FIRST");
        return;
    }
    const int heard = mode && lk_heard && plat_ms() - lk_heard < 3500u, pl = heard && (lk_pk & 64u);
    const int role = !heard ? 0 : want && !pl ? 1 : !want && pl ? 2 : 3;
    uint8_t q, m;
    if (role != lk_role) {
        if (lk_role == 2)
            en_ext_chord(0, -1, 0);              /* (no longer following: its own chords again, from this one) */
        if (role == 1 || role == 2)
            say("LINKED", role == 1 ? "THIS ONE LEADS" : "FOLLOWING THE LEAD");
        else if (role == 3)
            say("LINKED", want ? "TWO LEADS: SET ONE" : "NO LEAD: SET ONE");
        else
            say("LINK", "PARTNER GONE: ALONE");
        lk_role = (uint8_t)role;
        lk_cm = 0xFFFFu;
    }
    q = (uint8_t)(mode ? EN_LK_HELLO | (want ? EN_LK_WANT : 0) : 0);
    if (role)
        q |= EN_LK_ON;
    q |= role == 1 ? EN_LK_LEAD : role == 2 ? EN_LK_FOLLOW : 0;
    m = role ? PLAYS_MUTE[ui.fxs[EN_FXS_LK_PLAYS] % 5u] : 0u;
    if (q != q_s || m != m_s) {
        q_s = q;
        m_s = m;
        en_link(q, m);
    }
}

/* MIDI in (USB and the TRS jack): on the channel chosen (MIDI IN page; all by default), notes add or remove their note from the scale
 * (channel 1 only, as before), or with PADS play PERF (pads_in); clock and start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        const int ch_ok = !ui.min_ch || (st & 15u) == (uint32_t)(ui.min_ch - 1);
        if (st < 0xF8u) {
            ui.min_last = pkt;
            ui.min_n++;
        }
        if (st == 0xBFu && n >= 20u && n <= 31u && ui.fxs[EN_FXS_LK_MODE]) {   /* (0.17.47) the link's (channel 16, CC 20 .. 31) */
            lk_in((int)n + 82, (int)v);          /* (lk_in counts them from 102) */
            continue;
        }
        if (lk_role == 1 && st >= 0xF8u)         /* (leading: the tempo is this one's) */
            continue;
        if (st < 0xF0u && mm_apply(st, n, v))   /* the player's map first (0.16.11) */
            continue;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if ((st & 0xF0u) == 0xB0u && n == 64u && ch_ok) {   /* CC 64 (a sustain pedal, an editor's button): REC's Freeze while it is down, in any mode (0.16.6) */
            if ((v >= 64u) != (en_frozen != 0)) {
                say("FREEZE", v >= 64u ? "ON" : "OFF");
                en_freeze(v >= 64u);
            }
        } else if (ui.min_pads == 3 && (st & 0xF0u) == 0xB0u)
            mpk_cc((int)n, (int)v);
        else if (ui.min_pads == 3 && (st & 0xF0u) == 0xE0u)   /* its pitch bend: on the voice the keys play (14 bits, 8192 the middle) */
            en_bend(MPK_VC[ui.jam_voice & 3], (int)(v << 7 | n) - 8192);
        else if (ui.min_pads == 3 && ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u)) {
            if ((st & 15u) == 9u)                /* its pads (channel 10): PERF's pads, bank A from note 36 */
                pads_in((st & 0xF0u) == 0x90u && v, (int)n, (int)v);
            else
                mpk_key((st & 0xF0u) == 0x90u && v, (int)n, (int)v);
        } else if ((st & 0xF0u) == 0xB0u && (ui.cc_mode & 1u) && ch_ok)
            cc_in((int)n, (int)v);
        else if (ui.min_pads == 2 && (st & 0xF0u) == 0x90u && v)
            ms_in((int)(st & 15u));
        else if (ui.min_pads == 1 && ch_ok && ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u))
            pads_in((st & 0xF0u) == 0x90u && v, (int)n, (int)v);
        else if (ui.min_pads == 4 && ch_ok && ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u)) {   /* CHORDS (0.17.31): below middle C the chord, above the voice */
            if (n < 60u)
                ci_key((st & 0xF0u) == 0x90u && v, (int)n);
            else
                midi_key((st & 0xF0u) == 0x90u && v, (int)n, (int)v, key_voice());
        }
        else if (!ui.min_pads && ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u) && ch_ok && (ui.min_ch || !(st & 15u))) {
            /* a keyboard (channel 1, or the one chosen): on Global its notes put the scale's in and out, as the keys there do; anywhere else
             * they play the voice the keys play (0.17.7: they changed the scale on every page) */
            if (ui.view == V_GLOBAL && !ui.fx_menu) {
                if ((st & 0xF0u) == 0x90u && v)
                    toggle_note((int)(n % 12u));
            } else
                midi_key((st & 0xF0u) == 0x90u && v, (int)n, (int)v, key_voice());
        }
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. The buttons whose
 * hold means something else (PLAY, SEL, SEQ, SAVE, FX: the effects' settings) have no card. */
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
        ui.btn_used |= btn;                      /* its release now does nothing */
        ui.fx_menu = 1;
        ui.fx_page = ST_ORDER[0];                /* the settings' walk (0.16.1: their own; the dial goes through them) */
        say("SETTINGS", "DIAL: NEXT PAGE");
        return;
    }
    if (btn == 1u << B_FX && !(ui.btn_used & btn) && now - ui.down_t[B_FX] >= 500u && (int32_t)(ui.turn_t - ui.down_t[B_FX]) <= 0) {   /* FX held: the effects' settings, from anywhere (0.17.20, as HOME held is the unit's) */
        ui.btn_used |= btn;                      /* its release now does nothing */
        ui.fx_menu = 1;
        ui.lock_mode = 0;
        ui.fx_open_t = 0;
        ui.fx_page = ui.fx_lastpg ? ui.fx_lastpg : FX_ORDER[0];   /* the effect you used last, else the first; the dial walks them */
        say(FXP[ui.fx_page - 1].name, "DIAL: NEXT EFFECT");
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

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
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
                ui.save_fact = (uint8_t)(btn >> B_SEL & 1u);
                if (ui.save_fact)
                    ui.btn_used |= 1u << B_SEL;       /* SEL as the modifier: letting go of it does nothing */
                ui.btn_used |= m;              /* not a breakdown */
                ui.save_held = 1;              /* nor a drop */
            }
            if (i == B_FX && (btn >> B_SEL & 1u)) {   /* shift: the effects' edit pages, one by one, then the keys again */
                {   /* (the walk's order, then the keys again) */
                    const int nx = ui.fx_menu && st_pos(ui.fx_page) < 0 ? fx_pos(ui.fx_page) + 1 : 0;   /* (from a setting: the first effect) */
                    ui.fx_page = (uint8_t)(nx >= 0 && nx < FX_N ? FX_ORDER[nx] : 0);
                }
                ui.fx_menu = 1;
                ui.lock_mode = 0;
                say(ui.fx_page ? FXP[ui.fx_page - 1].name : "FX", ui.fx_page ? "EDIT" : "HOLD A KEY");
                ui.btn_used |= m | 1u << B_SEL;
            }
            if (i == B_SEQ)
                ui.seq_held = 0;
            if (i == B_HOME && (btn >> B_SEL & 1u))   /* (1.0) shift: nothing. It turned the sigil's middle (the eye or the imp), which the Console
                                                       * layout doesn't draw (0.17.3); its bit stays in the stored settings */
                ui.btn_used |= m | 1u << B_SEL;
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
            if (i == B_EDIT && (btn >> B_SEL & 1u))   /* (shift + EDIT was HYP-1's game, gone in 0.17.79 for the boot picture's flash: it does nothing) */
                ui.btn_used |= m | 1u << B_SEL;
            if (i == B_REC && (btn >> B_SEL & 1u)) {   /* shift: Capture: the bar played on Jam into the acid line or the pluck loop; in the FX menu, the gesture */
                ui.btn_used |= m | 1u << B_SEL;
                if (ui.fx_menu)
                    gesture_key();
                else if (ui.view >= V_ACID && ui.view <= V_ACSTEP)
                    acid_keep();               /* (0.17.31) on an Acid page: the answer kept as the line */
                else if (ph_st == 3 && ui.view != V_JAM)
                    phrase_key();              /* (a phrase playing: let go; on JAM, REC alone) */
                else
                    jam_capture();
                continue;
            }
            if (i == B_ARP && (btn >> B_SEL & 1u)) {   /* shift (0.17.22): the acid answers on the next bar, whatever its count */
                ui.btn_used |= m | 1u << B_SEL;
                if (proj.par[P_AC_RESP] < 5) {
                    say("NO ANSWER SET", "ACID ANSWER: ARP x4");
                } else {
                    en_acid_ans_now();
                    say("ACID ANSWER", proj.par[P_AC_CHANCE] ? "NEXT TIME ROUND" : "NEXT BAR");
                }
            }
            if (i == B_ENV && (btn >> B_SEL & 1u)) {
                /* shift: the voice whose page is on screen off the algorithm (silent but for the keys on its
                 * pages), or back on it */
                static const char *const WHO[3] = {"PAD", "BASS", "ACID"};
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
            if (i == B_OCTDN && (btn >> B_SEL & 1u) && ui.view == V_GLOBAL && !ui.fx_menu) {
                piece_empty();                 /* shift on Global: the piece emptied (0.17.31) */
                ui.btn_used |= m | 1u << B_SEL;
            }
            if ((i == B_OCTDN || i == B_OCTUP) && (btn >> B_SEL & 1u) && ui.view == V_SEQ) {
                shift_lane(i == B_OCTUP);      /* shift: the lane cleared, or the piece's own */
                ui.btn_used |= m | 1u << B_SEL;
            }
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (i == B_SAVE && ui.save_global) {
            ui.save_global = 0;                /* Global: SAVE let go, the piece stored (unless held to factory) */
            if (!ui.factory_done && !ui.save_fact)
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
    if ((btn >> B_SEL & 1u) && (ch & keys) && ui.view == V_JAM && !ui.fx_menu) {   /* the JAM page: SEL held, the keys play accents (jam_down), not the shift */
        ui.btn_used |= 1u << B_SEL;
    } else if ((btn >> B_SEL & 1u) && (ch & keys)) {  /* SEL held: the keys pressed are the shift's */
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
    if ((btn >> B_EDIT & 1u) && !(btn >> B_SEL & 1u) && !(ui.btn_used >> B_EDIT & 1u) && plat_ms() - ui.down_t[B_EDIT] >= 400u) {
        ui.btn_used |= 1u << B_EDIT;           /* held: the Progression page (a tap: the next chord) */
        ui.fx_menu = 0;
        ui.fx_page = 0;
        set_view(V_PROG);
        say("PROGRESSION", "WHITE KEYS: A SLOT");
    }
    if ((btn >> B_GLO & 1u) && !(btn >> B_SEL & 1u) && !(ui.btn_used >> B_GLO & 1u) &&
        plat_ms() - ui.down_t[B_GLO] >= 400u)
        ui.btn_used |= 1u << B_GLO;            /* held for the mix: letting go does nothing either */
    ui.lock_keys &= keys;
    if (!ui.fx_menu || ui.fx_page)
        ui.fx_tap = 0;
    if (ui.fx_menu && ((!ui.fx_page && fx_bank == 3) || ui.fx_page == 20 || ui.fx_page == 40)) {   /* the snips (their bank, and their settings page, to try them): each key down is one (let go: the note off, with the other voices' keys) */
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
                if ((ui.fx_lock & bit) && (key_flex((int)i) < 0 || ui.flex_lock == key_flex((int)i))) {   /* (Flex: only the latched pattern's own key) */
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
                if (plat_ms() - ui.fx_down_t[i] < TAP_MS) {
                    ui.fx_lock |= bit;         /* let go quickly: latched */
                    if (key_flex((int)i) >= 0)
                        ui.flex_lock = (uint8_t)key_flex((int)i);
                }
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
                ui.fx_lock &= ~FXB(FX_FLEX);
            }
        }
        m = ui.fx_lock;
        if (ui.fx_menu && ui.fx_page && FXP[ui.fx_page - 1].bit != 63 &&
            (!FXP[ui.fx_page - 1].hold || (keys & ~ui.shift_keys)))   /* an edit page: its effect on, */
            m |= FXB(FXP[ui.fx_page - 1].bit);                                                 /* to be heard as it is edited (a roll or riser: with a key held) */
        {
            uint8_t flxh = 0;                  /* a Flex Beat pad held down by hand (the engine goes on with a one shot while it is) */
            static uint8_t flxh_sent;
            int fsel = (m >> FX_FLEX & 1u) ? ui.flex_lock : -1;   /* the Flex pattern to play: a pad held (the lowest), else the latched one */
            if (ui.fx_menu && !ui.fx_page) {
                for (i = 0; i < NKEYS; i++)
                    if ((keys & ~ui.shift_keys & ~ui.lock_keys) >> i & 1u) {
                        m |= fxbit(i);
                        if (key_flex((int)i) >= 0 && !flxh) {
                            ui.flex_last = FLEX_PAT[i];
                            fsel = FLEX_PAT[i];
                            flxh = 1;
                        }
                    }
            }
            if (fsel >= 0 && (m >> FX_FLEX & 1u) && fsel != ui.flex_cur) {
                ui.flex_cur = (uint8_t)fsel;
                en_flex_select(fsel);          /* (before the bit goes out) */
            }
            if (flxh != flxh_sent) {
                flxh_sent = flxh;
                en_flx_hold(flxh);
            }
        }
        ui.af_user = m != 0;                     /* (an effect played or latched by hand: AUTO FX gives way) */
        {   /* (0.17.44) the gesture recorder: the effect the hand plays now (the lowest), and the one the gesture plays when the hand plays none */
            int b = 0;
            if (m)
                while (!(m >> b & 1u))
                    b++;
            gs_hand = (uint8_t)(m ? b + 1 : 0);
            if (gs_fxnow && !m)
                m |= FXB(gs_fxnow - 1u);
        }
        if (ui.af_live && !ui.af_user && ui.af_fx != AF_GS)
            m |= FXB(ui.af_fx);
        if (ui.dj_on && en_playing) {              /* the auto-DJ: a fill before the breakdown, the riser in the build */
            if (ui.dj_fill)
                m |= FXB(FX_FILL);
            if (en_sec == EN_SEC_BUILD)
                m |= FXB(FX_RISER);
        }
        if (ui.smp_dry)
            m = 0;                               /* (the SAMPLE page, dry: nothing on) */
        if (m != ui.fx_mask) {
            ui.fx_mask = m;
            en_fx(m);
        }
    }
    if (ui.fx_menu && ui.fx_page == FXP_SAMPLE + 1)   /* the SAMPLE page: a key a sound, or a run */
        smp_keys(ch & keys & ~ui.shift_keys, ch & ~keys);
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
                } else if (ui.view == V_ACSTEP) { /* the Acid steps page: a white key chooses the step */
                    int s;
                    for (s = 0; s < 16; s++)
                        if (WHITE_KEY[s] == i)
                            ui.astp = (uint8_t)s;
                } else if (ui.view == V_SETL) {   /* the Set list page: a white key chooses the slot; black 1-5 its parts in or out (0.17.13) */
                    int s;
                    for (s = 0; s < SL_N; s++)
                        if (WHITE_KEY[s] == i)
                            ui.slsl = (uint8_t)s;
                    for (s = 0; s < SL_NPART; s++)
                        if (BLACK_KEY[s] == i) {
                            const int c = ui.slsl % SL_N;
                            glob.sl_mute[c] ^= (uint8_t)(1u << s);
                            project_globals();
                            if (sl_play() && c == ui.sl_pos)
                                sl_parts(c);     /* (the place playing: heard at once) */
                            say(SL_PART[s], glob.sl_mute[c] >> s & 1u ? "OUT" : "IN");
                        }
                } else if (ui.view == V_PROG) {   /* the Progression page: the first eight white keys choose the slot, the first five black keys
                                                   * write a Circle fill (0.16.20) */
                    int s;
                    for (s = 0; s < EN_PRG_N; s++)
                        if (WHITE_KEY[s] == i)
                            ui.pgsl = (uint8_t)s;
                    for (s = 0; s < PGF_N; s++)
                        if (BLACK_KEY[s] == i)
                            pg_fill(s);
                    for (s = 0; s < 2; s++)       /* white keys 9 and 10: the bass, the pad, follow the chords or hold the key (PEDAL, 0.16.21) */
                        if (WHITE_KEY[8 + s] == i) {
                            glob.sl_rsv[2] = (uint8_t)((glob.sl_rsv[2] ^ (1u << s)) & 0x3Fu);   /* (bit 2 is Form's, bit 3 Calm's, 4-5 the rests') */
                            en_pedal(glob.sl_rsv[2] & 3);
                            project_globals();
                            say(s ? "PAD" : "BASS", glob.sl_rsv[2] >> s & 1u ? "PEDAL: HOLDS THE KEY" : "FOLLOWS THE CHORDS");
                        }
                } else if (ui.view == V_BLSTEP) { /* the Bass steps page: likewise */
                    int s;
                    for (s = 0; s < 16; s++)
                        if (WHITE_KEY[s] == i)
                            ui.bstp = (uint8_t)s;
                } else if (ui.view == V_CMSTEP) { /* the Pluck steps page: likewise */
                    int s;
                    for (s = 0; s < 16; s++)
                        if (WHITE_KEY[s] == i)
                            ui.pstp = (uint8_t)s;
                } else if (ui.view == V_STEPS) { /* the Steps page: a white key chooses the step */
                    int s;
                    for (s = 0; s < 16; s++)
                        if (WHITE_KEY[s] == i)
                            ui.stp = (uint8_t)s;
                } else if (ui.view == V_JAM) {   /* the JAM page: the key plays the voice chosen */
                    jam_down((int)i);
                } else if (by_hand(voice_of(ui.view))) {   /* a voice off the algorithm, its page: the key plays it */
                    ui.key_voice[i] = (uint8_t)(voice_of(ui.view) + 1);
                    en_note(voice_of(ui.view), 53 + (int)i, 1);
                } else if (ui.view == V_GLOBAL) {   /* Global: the scale's notes in and out (0.17.6: only here; elsewhere a key played a
                                                     * voice by mistake changed the scale) */
                    toggle_note((int)((5u + i) % 12u));   /* key 0 is F3 */
                } else {                         /* anywhere else: the key plays the page's voice (Jam's elsewhere), as Jam plays it */
                    jam_down_v((int)i, key_voice());
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
    cc_out();
    if ((btn >> B_SEQ & 1u) && !ui.seq_held && ui.view != V_SEQ && plat_ms() - ui.down_t[B_SEQ] >= SEQ_HOLD) {
        ui.seq_held = 1;                       /* SEQ held: the sequencer */
        ui.btn_used |= 1u << B_SEQ;
        set_view(V_SEQ);
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages (the steps); with HOME held, the tempo */
        if (!(btn >> B_HOME & 1u) && ui.fx_menu && !ui.fx_page) {   /* the FX keys: the dial is the bank (the effects, the beat effects) (HOME held: the tempo) */
            fx_bank = (uint8_t)(e > 0 ? (fx_bank < 3 ? fx_bank + 1 : 3) : (fx_bank > 0 ? fx_bank - 1 : 0));
            say(fx_bank == 3 ? "DJ SNIPS" : fx_bank == 2 ? "FLEX BEAT" : fx_bank ? "BEAT FX" : "FX", "HOLD A KEY");
        } else if (!(btn >> B_HOME & 1u) && ui.fx_menu && ui.fx_page) {   /* the FX edit pages: the dial walks them */
            const int st = st_pos(ui.fx_page) >= 0, pos = fx_pos(ui.fx_page) + (e > 0 ? 1 : -1);
            if (pos >= 0 && pos < (st ? ST_N : FX_N)) {   /* (a setting: the settings' walk; an effect: the effects') */
                ui.fx_page = st ? ST_ORDER[pos] : FX_ORDER[pos];
                say(FXP[ui.fx_page - 1].name, st ? "SETTINGS" : "EDIT");
            }
        } else if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_TEMPO) {
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u, ui.tick_until = plat_ms() + 90u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else if (ui.view == V_VIZ) {            /* the visualizers: the next one */
            ui.viz = (uint8_t)((ui.viz + (e > 0 ? 1 : VZ_N - 1)) % VZ_N);
            ui.set_lazy = 1;
            say("VISUAL", VZ_NAME[ui.viz]);
        } else {
            int v = (ui.view == V_SEQ ? V_DRUMS : ui.view == V_PERF ? V_TEMPO : (int)ui.view) + (e > 0 ? 1 : -1);   /* (out of the sequencer and the perform screen too) */
            if (v >= 0 && v < NDIAL)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        if (btn >> B_SEL & 1u) {                      /* shift: the sound of the voice on screen (drums: the kit) */
            ui.btn_used |= 1u << B_SEL;
            shift_sound(e > 0 ? 1 : -1);
        } else if ((ui.view == V_CHORDS || ui.view == V_GLOBAL) && !ui.fx_menu) {   /* (0.17.16) the mode only where the scale is edited */
            set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        } else {                                      /* everywhere else the mood (the maker: a turn changing the scale by accident) */
            int m = ui.mood + 2 * e;
            char t[8];
            ui.mood = (uint8_t)(m < 0 ? 0 : m > 100 ? 100 : m);
            mood_apply();
            itoa_u(ui.mood, t);
            say("MOOD", t);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        if ((btn >> B_GLO & 1u) && !(btn >> B_SEL & 1u)) {   /* GLO held: the genre */
            ui.btn_used |= 1u << B_GLO;
            genre_step(e > 0 ? 1 : -1);
        } else if (sl_play()) {                   /* the set list playing: its next or last slot (SEL, playing: on the next bar) */
            const int q = (btn >> B_SEL & 1u) && en_playing, s = sl_step(q && ui.sl_q ? ui.sl_q - 1 : ui.sl_pos, e > 0 ? 1 : -1);
            if (btn >> B_SEL & 1u)
                ui.btn_used |= 1u << B_SEL;
            ui.piece_q = 0;
            if (s >= 0 && q) {
                char u[12] = "SET ";
                itoa_u((uint32_t)s + 1u, u + 4);
                ui.sl_q = (uint8_t)(s + 1);
                say(u, "NEXT BAR");
                ui.msg_until = plat_ms() + 60000u;
            } else if (s >= 0) {
                ui.sl_q = 0;
                sl_go(s);
            }
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
    sl_tick();
    if (sl_stop && (!en_playing || en_drum_pos == EN_NSTEP - 1)) {   /* Once, at the list's end: stopped, the first place ready */
        const int f = sl_step(0xFF, 1);
        sl_stop = 0;
        if (en_playing)
            en_play(0);
        if (f >= 0)
            sl_go(f);
        say("SET LIST", "THE END");
    }
    if (ui.sl_q && !ui.store_wait && (!en_playing || en_drum_pos == EN_NSTEP - 1)) {   /* the bar's last step: the set list's slot */
        const int s = ui.sl_q - 1, t0 = proj.par[P_TEMPO], pl = en_playing, from = ui.sl_pos, sec_was = en_sec;
        ui.sl_q = 0;
        sl_go(s);
        if (pl)
            mix_start(t0);
        if (pl && glob.sl_on && from < SL_N && glob.sl_bars[from]) {   /* (0.17.61) song mode: a place without the drums comes in as a breakdown (or stays
                                                 * down); one with them gets its drop (a new piece restarts the engine's section: a build's own drop would be lost) */
            if (!sl_drums(s) && (sl_turn(from, s) == 1 || sec_was == EN_SEC_BREAK))
                en_section(EN_CMD_BREAK);
            else if (sl_drums(s) && sec_was != EN_SEC_GROOVE)
                en_drop();
        }
    }
    if (ui.piece_q && (!en_playing || en_drum_pos == EN_NSTEP - 1)) {   /* the bar's last step: the piece */
        const int t = ui.piece_q - 1, t0 = proj.par[P_TEMPO], pl = en_playing;
        ui.piece_q = 0;
        set_piece(t);
        if (pl)
            mix_start(t0);
    }
    if ((btn >> B_SEL & 1u) && !(ui.btn_used >> B_SEL & 1u) && !ui.shift_hint && !(ui.view == V_JAM && !ui.fx_menu) &&
        plat_ms() - ui.down_t[B_SEL] >= 400u) {   /* (not on JAM: SEL is the accent there, held before the key) */
        ui.shift_hint = 1;                     /* SEL held alone: what the shift does (draw_shift) */
    }
    if (ui.save_global && ui.save_fact && !ui.factory_done && (btn >> B_SAVE & 1u)) {
        uint32_t held = plat_ms() - ui.down_t[B_SAVE];
        if (held >= FACTORY_HOLD) {
            ui.factory_done = 1;
            piece_factory();
            ui.msg_until = 0;
        } else if (held >= 400u && plat_ms() >= ui.msg_until) {
            say("KEEP HOLDING:", "FACTORY");   /* what holding on does */
            ui.msg_until = ui.down_t[B_SAVE] + FACTORY_HOLD;
        }
    }
    if (ui.save_global && ui.save_fact && ui.factory_done == 1 && (btn >> B_SAVE & 1u)) {   /* (0.17.23) held on: every piece, every genre */
        const uint32_t held = plat_ms() - ui.down_t[B_SAVE];
        if (held >= FACTORY_ALL_HOLD) {
            ui.factory_done = 2;
            if (en_playing) {
                say("ALL PIECES TO FACTORY", "STOP FIRST");
            } else {
                fall.g = 1;
                fall.g0 = proj.genre;
                fall.p0 = proj.piece;
                fall.n = 0;
            }
        } else if (held >= FACTORY_HOLD + 1500u && plat_ms() >= ui.msg_until) {
            say("KEEP HOLDING:", "EVERY PIECE, ALL GENRES");
            ui.msg_until = ui.down_t[B_SAVE] + FACTORY_ALL_HOLD;
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
            if (ui.view == V_SETL && (i == 1 || i == 2) && (btn >> B_SEL & 1u))
                ui.btn_used |= 1u << B_SEL;      /* (SEL + KNOB2, KNOB3: the place's pattern and energy, view_knob) */
            if (ui.view == V_TEMPO && i == 3 && (btn >> B_SEL & 1u)) {   /* SEL + Drift: the chord's time in bars (right) or in seconds (left), in any genre; the genre's own is kept as 0 */
                const int def = en_genre_bars() ? 1 : 2, nw = e > 0 ? 1 : 2;
                ui.btn_used |= 1u << B_SEL;
                en_prg_put(proj.prg, EN_PRG_UNIT, 2, nw == def ? 0 : nw);
                en_prog(proj.prg);
                edited();
                say("DRIFT", nw == 1 ? "IN BARS" : "IN SECONDS");
                ui.act_t = ui.turn_t = plat_ms();
                continue;
            }
            if (ui.view == V_ACLINE && i == 3 && (btn >> B_SEL & 1u)) {   /* SEL + Mutate: what a mutation changes (all, the notes, the rhythm, the accents; it was the Deja vu page's fourth knob) */
                char t[16];
                ui.btn_used |= 1u << B_SEL;
                turn(EN_K1 + (int)i, P_AC_CHG, e);
                knob_text(P_AC_CHG, t);
                say("MUTATES", t);
                ui.act_t = ui.turn_t = plat_ms();
                continue;
            }
            turn(EN_K1 + (int)i, view_knob((int)i), e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u, ui.tick_until = plat_ms() + 90u;
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
    if (LOOK_CONSOLE && r > 1)                   /* the Console layout: a desk's square keys, every chip and tag at once */
        r = 1;
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

/* a pixel blended over the canvas (16 steps from the canvas to the colour) */
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
/* (0.17.39) a panel with depth: lit from the top left, a pixel of light along its top and left edges and of shade along its bottom and
 * right (sunk: the other way round, a track or a screen set into the desk). Square corners, the Console's */
/* (0.17.40) a fill in n flat bands from c0 at the top to c1 at the bottom: a light falling off down a panel, at a flat fill's cost (no dither: text sits on it) */
static void cv_vgrad(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c0, uint16_t c1, int n)
{
    int i;
    for (i = 0; i < n; i++)
        cv_rect(x, y + h * i / n, w, h * (i + 1) / n - h * i / n, mix(c0, c1, n > 1 ? 16 * i / (n - 1) : 0));
}
/* (0.17.40) the panel with its light falling off: top at the top edge, bot at the bottom, in four bands */
static void cv_panel_g(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t top, uint16_t bot, int sunk)
{
    uint16_t hi = mix(top, th->text, 3), lo = mix(bot, 0x0000, 9);
    if (sunk)
        hi = mix(top, 0x0000, 11), lo = mix(bot, th->text, 3);
    if (top == bot)
        cv_rect(x, y, w, h, top);
    else
        cv_vgrad(x, y, w, h, top, bot, 4);
    cv_rect(x, y, w, 1, hi);
    cv_rect(x, y + 1, 1, h - 2, hi);
    cv_rect(x, y + h - 1, w, 1, lo);
    cv_rect(x + w - 1, y + 1, 1, h - 2, lo);
}
static void cv_panel(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t base, int sunk) { cv_panel_g(x, y, w, h, base, base, sunk); }
/* (0.17.40) THE focus cue, one for every page: n rings of col outside a box, each fainter into bg (what is "now": the knob being
 * turned, the lit tab, the playhead). Rings of flat colour, no square roots */
static void cv_glow(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t col, uint16_t bg, int n)
{
    int i;
    for (i = 1; i <= n; i++) {
        const uint16_t c = mix(col, bg, 2 + 4 * i);
        cv_rect(x - i, y - i, w + 2 * i, 1, c);
        cv_rect(x - i, y + h + i - 1, w + 2 * i, 1, c);
        cv_rect(x - i, y - i + 1, 1, h + 2 * i - 2, c);
        cv_rect(x + w + i - 1, y - i + 1, 1, h + 2 * i - 2, c);
    }
}
/* (0.17.40) Look (SCREEN): bit 0 the scanlines, bit 1 Calm: no imp in the header, no knob tick, no page fade */
#define UI_CALM (ui.scan >> 1 & 1u)
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
        /* rows 0..10 the head, then four coils of three rows: each shifted by its share of the stretch. Within a coil the share runs down
         * across its width (the spring winds down to the right), so the stretch parts the coils along the winding, not straight across */
        const int grp = r <= 10 ? 0 : (r - 8) / 3;
        const int32_t dy0 = (int32_t)(hm.lift + hm.dh * (float)(4 - grp) * 0.25f + 0.5f);
        for (c = 0; c < 23; c++) {
            char ch = MASCOT[r][c];
            int32_t dy = dy0;
            if (ch == '.')
                continue;
            if ((ch == 'Y' || ch == 'E') && sleepy && r > 4)
                ch = 'K';
            if (grp > 0 && c < 16)
                dy = (int32_t)(hm.lift + hm.dh * ((float)(4 - grp) - fm_clampf((float)(c - 2) * (1.0f / 13.0f), 0.0f, 1.0f)) * 0.25f + 0.5f);
            cv_pset(x + c, y + r - dy, (ch == 'K' || ch == 'T') && !LOOK_SYNTH ? K_LINE : mascot_color(ch));
        }
    }
}

/* the header: the piece in a tag, its root and mode; on the right, the tempo with a beat light (a play
 * triangle while stopped), the section and a dot while the kit is unsaved. Messages take its place
 * for a moment. */
/* THE HEADER'S EXTRAS: the header has a beat bar along its top (sixteen ticks, the beats taller, the step
 * playing lit) and a coil along its bottom, the slinky's spring, that stretches on the kick. */
#define IMP_TEAL (th->pluck)                    /* the pluck's colour in the theme */
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
    int msg = plat_ms() < ui.msg_until, imp, hp = -1;
    uint32_t h;
    if (en_sec != ui.imp_sec) {                 /* (0.17.40) a drop, a break, a build: the imp comes up in the Console header for a moment */
        if (en_playing && !UI_CALM)
            ui.imp_until = plat_ms() + 1400u;
        ui.imp_sec = en_sec;
    }
    imp = !LOOK_CONSOLE || plat_ms() < ui.imp_until;
    if (imp)                                    /* (the Console header, the rest of the time: no imp, nothing to spring) */
        mascot_step();
    if (msg && ui.save_global && ui.save_fact && ui.factory_done < 2 && (ui.btn >> B_SAVE & 1u)) {   /* (0.17.40) a hold to factory: how far along, under the words */
        const uint32_t held = plat_ms() - ui.down_t[B_SAVE], tgt = ui.factory_done ? FACTORY_ALL_HOLD : FACTORY_HOLD;
        hp = held >= tgt ? 16 : (int)(held * 16u / tgt);
    }
    if (ui.frame % 2u)                          /* (the spring moves every frame; the header is redrawn thirty times a second) */
        return;
    h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | (uint32_t)en_sec << 2 | (uint32_t)en_sec_req << 9 | (uint32_t)msg << 5 |
                               (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 12 | (uint32_t)project_pending() << 16 |
                               (uint32_t)(proj.edited >> proj.piece & 1u) << 17),
                      (uint32_t)proj.par[P_TEMPO] | (uint32_t)ui.theme << 8 | (uint32_t)(ui.fx_lock != 0) << 12 | (uint32_t)ui.dj_on << 13 | (uint32_t)ui.scene_q << 14 | (uint32_t)ui.key_on << 19 |
                          (uint32_t)((ui.fxk[0] | ui.fxk[1] | ui.fxk[2] | ui.fxk[3]) != 0) << 18 | (uint32_t)(gs_st & 3u) << 20 | (uint32_t)(gs_mask != 0) << 22 | (uint32_t)gs_shot << 23 | (uint32_t)(gs_st >> 2) << 24 | (uint32_t)ph_st << 25 | (uint32_t)lk_role << 27);
    h = hash(h, (uint32_t)en_sec_bars | (uint32_t)ui.dj_n << 8 | (uint32_t)ui.dj_len << 24);
    h = hash(h, (uint32_t)(en_intro_k * 64.0f) | (uint32_t)ui.dj_round << 8 | (uint32_t)ui.dj_rounds << 12);   /* the intro's countdown, the round */
    h = hash(h, (plat_battery() >= 591u ? 3u : plat_battery() >= 561u ? 2u : plat_battery() >= 531u ? 1u : 0u) | (uint32_t)plat_usb_power() << 4 | (uint32_t)(plat_battery() != 0xFFFFFFFFu) << 5);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    h = hash(h, (uint32_t)imp | (uint32_t)(hp + 1) << 1);
    if (imp)                                    /* 0.17.39: the imp, its hop and its clock only where it is drawn (the Console header redrew 15 times a second for nothing) */
        h = hash(hash(h, (uint32_t)(int)(hm.lift * 2.0f) | (uint32_t)((int)(hm.dh * 2.0f) + 16) << 4 |
                             (uint32_t)(en_playing && en_sec == EN_SEC_BUILD ? 1u + (ui.frame / 2u & 1u) : 0u) << 12),
                 (uint32_t)(plat_ms() / 66u) | (uint32_t)q16(en_drum_glow[0]) << 20);
    h = hash(h, (uint32_t)(en_playing ? en_drum_pos + 1u : 0u) << 25);
    h = hash(h, ui.piece_q);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
        if (hp >= 0) {                           /* the hold's progress: sixteen cells filling */
            int j;
            for (j = 0; j < 16; j++)
                cv_rect(10 + j * 14, 23, 12, 2, j < hp ? K_ACC : K_LINE);
        }
    } else {
        int32_t x = LOOK_CONSOLE && !imp ? 236 : 207, w;   /* (the Console layout: no imp up here, the status runs to the edge; 0.17.40: but for its moments) */
        const uint32_t bv = plat_battery();
        if (bv != 0xFFFFFFFFu && bv > 100u) {      /* the battery, left of the mascot: a cell, its charge in three steps (the stock firmware's own: the divider's reading under 531 is the last, 561 and 591 the others), a bolt in it while USB powers the unit */
            const int lvl = bv >= 591u ? 3 : bv >= 561u ? 2 : bv >= 531u ? 1 : 0;
            const uint16_t bc = lvl == 0 ? K_ACC : lvl == 1 ? K_FROST : K_DIM;
            int j;
            cv_rect(x - 14, 12, 12, 7, bc);
            cv_rect(x - 13, 13, 10, 5, K_BG);
            cv_rect(x - 2, 14, 2, 3, bc);
            for (j = 0; j < lvl; j++)
                cv_rect(x - 12 + 3 * j, 14, 2, 3, K_TEXT);
            if (plat_usb_power()) {                /* a bolt over it */
                cv_rect(x - 8, 13, 2, 3, K_ACC);
                cv_rect(x - 9, 15, 4, 1, K_ACC);
                cv_rect(x - 8, 16, 2, 2, K_ACC);
            }
            x -= 20;
        }
        if (imp)
            draw_mascot(212 +(en_playing && en_sec == EN_SEC_BUILD ? (int)(ui.frame / 2u & 1u) * 2 - 1 : 0), 3, en_playing && en_sec == EN_SEC_BREAK);
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
        if (en_playing && en_intro_k < 1.0f) {   /* the intro: how long until everything is in */
            const int secs = (int)((1.0f - en_intro_k) * (float)INTRO_S[ui.intro % 6u] + 0.99f), m = secs / 60, s = secs % 60;
            t[0] = 'I', t[1] = 'N', t[2] = 'T', t[3] = 'R', t[4] = 'O', t[5] = ' ', t[6] = (char)('0' + m), t[7] = ':', t[8] = (char)('0' + s / 10), t[9] = (char)('0' + s % 10), t[10] = 0;
            x -= text_w(&FONT_XS, t);
            cv_text(x, 8, &FONT_XS, t, K_ACC);
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
        if (ph_st) {                              /* (0.17.45) the phrase recorder: armed (ARM), writing (REC), playing (PHR) */
            const char *g = ph_st == 3 ? "PHR" : ph_st == 2 ? "REC" : "ARM";
            x -= text_w(&FONT_XS, g);
            cv_text(x, 8, &FONT_XS, g, ph_st == 2 ? K_ACID : K_ACC);
            x -= 10;
        }
        if (gs_st == 1 || gs_st == 2 || (gs_st == 3 && gs_mask) || gs_shot) {   /* the gesture recorder: armed (ARM), writing (REC), looping (LOOP), AUTO FX's (AUTO) */
            const char *g = gs_shot ? "AUTO" : gs_st == 3 ? "LOOP" : gs_st == 2 ? "REC" : "ARM";
            x -= text_w(&FONT_XS, g);
            cv_text(x, 8, &FONT_XS, g, gs_st == 2 ? K_ACID : K_ACC);
            x -= 10;
        } else if (ui.fxk[0] || ui.fxk[1] || ui.fxk[2] || ui.fxk[3]) {   /* an FX knob off its home: said, so a forgotten filter is not a mystery */
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
        if (LOOK_CONSOLE) {                       /* the Console layout: the piece's name in the accent, no tag; unstored changes a dot after it */
            w = cv_text(6, 5, &FONT_B, piece_name(proj.piece), K_ACC) - 4;
            if (proj.edited >> proj.piece & 1u) {
                cv_round(w + 7, 12, 4, 4, 1, K_ACC);
                w += 8;
            }
        } else {
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
        }
        w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
        w = cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
        if (ui.piece_q) {                         /* a piece waiting for the bar: 07>08 */
            char q[8] = ">";
            int j = 1;
            const char *s = piece_name(ui.piece_q - 1);
            while (*s && j < 7)
                q[j++] = *s++;
            q[j] = 0;
            cv_text(w + 8, 8, &FONT_XS, q, K_ACC);
        }
    }
    if (LOOK_CONSOLE) {                          /* the Console look: the bar's steps on top, a plain rule under (no coil) */
        int i;
        for (i = 0; i < 16; i++)
            cv_rect(i * 15 + 1, 0, 13, i % 4 ? 1 : 2, en_playing && i == (int)en_drum_pos ? K_ACC : K_LINE);
        cv_rect(0, 27, 240, 1, K_LINE);
    } else
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
    if (ui.centre_imp) {                         /* the imp, summoned into it (in the Synth look he stands on the sun, no box) */
        if (!LOOK_SYNTH)
            cv_rect(CX - 13, CY - 14, 27, 27, K_BG);
        draw_mascot(CX - 11, CY - 12, play && en_sec == EN_SEC_BREAK);
    } else if (LOOK_SYNTH) {                     /* the scanner (the Synth look): a slit whose opening follows the section, a light sweeping across it once in two beats */
        const uint32_t dage = now - sg.drop_t;
        const int flash = play && sg.drop_t && dage < 450u;
        const int open = flash ? 12 : !play ? 3 : en_sec == EN_SEC_BREAK ? 1 : en_sec == EN_SEC_BUILD ? 8 : 5;
        const int32_t ex = CX + (play && en_sec == EN_SEC_BUILD ? (int32_t)(ui.frame & 1u) * 2 - 1 : 0);
        const int w = 34, x0 = (int)ex - w / 2, y0 = CY - open / 2 - 2;
        const int tempo = proj.par[P_TEMPO] > 0 ? proj.par[P_TEMPO] : 120, two = 120000 / tempo;
        const float ang = play ? (float)(now % (uint32_t)two) * (FM_TWO_PI / (float)two) : 0.0f;
        const float sw = (fm_sinf(ang) + 1.0f) * 0.5f;
        const int fwd = fm_cosf(ang) > 0.0f, sx = x0 + 2 + (int)(sw * (float)(w - 8));
        int j;
        if (flash) {
            cv_rect(x0 - 2, CY - 8, w + 4, 14, K_TEXT);
        } else {
            cv_rect(x0, y0, w, open + 2, RGB(0, 0, 0));
            cv_rect(x0 - 1, y0 - 1, w + 2, 1, mix(K_BG, th->bass, 11));   /* its frame, in the bass's colour */
            cv_rect(x0 - 1, y0 + open + 2, w + 2, 1, mix(K_BG, th->bass, 11));
            cv_rect(x0 - 1, y0, 1, open + 2, mix(K_BG, th->bass, 11));
            cv_rect(x0 + w, y0, 1, open + 2, mix(K_BG, th->bass, 11));
            for (j = 4; j >= 0; j--)             /* the light and its trail */
                cv_rect(sx - (fwd ? j * 3 : -j * 3), y0 + 1, 4, open, mix(RGB(0, 0, 0), th->bass, 16 - j * 3));
            cv_rect(sx + 1, y0 + 1 + open / 2, 2, 1, K_TEXT);
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

static void draw_meters(void)                    /* left: the five layers (pad, bass, acid, pluck, drums) */
{
    static const char *const L[5] = {"P", "B", "A", "C", "D"};
    uint16_t c[5], t[5];
    float lv[5], dm = 0.0f;
    int i;
    c[0] = K_PAD, c[1] = K_BASS, c[2] = K_ACID, c[3] = IMP_TEAL, c[4] = K_PULSE;
    t[0] = K_PAD_T, t[1] = K_BASS_T, t[2] = K_ACID_T, t[3] = mix(K_BG, IMP_TEAL, 4), t[4] = K_PULSE_T;
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[1] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[2] = en_acid_level * (float)proj.par[P_ACID] * 0.01f;
    lv[3] = en_cm_level * (float)proj.par[P_CM_LVL] * 0.01f * 1.6f;
    lv[4] = dm * (float)proj.par[P_PULSE] * 0.01f;
    for (i = 0; i < 5; i++) {
        int32_t x = 6 + i * 9, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 92.0f + 0.5f);
        if (LOOK_SYNTH) {                        /* LED segments: 32 of them, the top four in the accent */
            const int lit = (int)(fm_clampf(lv[i], 0.0f, 1.0f) * 32.0f + 0.5f);
            int sgm;
            for (sgm = 0; sgm < 32; sgm++)
                cv_rect(x, 20 + 96 - (sgm + 1) * 3, 5, 2, sgm < lit ? (sgm >= 28 ? K_ACC : c[i]) : t[i]);
        } else {
            cv_rect(x, 20, 5, 96, t[i]);
            if (h > 0)
                cv_rect(x, 20 + 96 - h, 5, h, c[i]);
        }
        cv_text(x, 120, &FONT_XS, L[i], K_DIM);
    }
}

static void cpu_line(char *c);
static void draw_kit(void)                       /* right: the pattern playing, small */
{
    int l, s, pos = en_drum_pos, pat = en_pat_now;
    char t[12] = "PAT ";
    for (l = 0; l < EN_NLANE; l++) {
        uint16_t bits = lane_now(l);
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
        if (en_playing) {                        /* (the one playing is shown: the one coming) */
            u[0] = 'N', u[1] = 'E', u[2] = 'X', u[3] = 'T', u[4] = ' ', u[5] = 0;
            itoa_u((uint32_t)en_pat_next + 1u, u + 5);
        }
        cv_text(120, 1, &FONT_XS, u, K_DIM);
    }
    for (l = 0; l < EN_NLANE; l++) {
        uint16_t bits = en_pats[seq_src(l)][l];
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
            int on = (int)(bits >> s & 1u), cur = en_playing && s == (int)en_lane_pos[l];
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
    case B_LFO: L("THE BASS; AGAIN: ITS"); L("ENVELOPE, THEN ITS LFO"); L("SEL + ENV: BY HAND"); L("SEL + ALGO: ITS SOUND"); break;
    case B_GLO: {                                /* and the audio's load, for testing on the device */
        static char cpu[40];
        cpu_line(cpu);
        L("GLOBAL; AGAIN: SET LIST, CODE");
        L("ROOT, THEME; KEYS: THE SCALE");
        L("+ SEL: REMOTE (BROWSER)");
        L(cpu);
        break;
    }
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); L("HOLD: THE PROGRESSION"); break;
    case B_SEQ: L("DRUMS; AGAIN: LINKS, KIT,"); L("THEN DRUM DRIFT"); L("HOLD: THE SEQUENCER"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L("JAM: REC RECORDS A PHRASE"); L("SEL + REC: CAPTURE THE BAR;"); L("ACID: KEEP IT; FX: GESTURE"); break;
    case B_OCTDN: L("AN OCTAVE DOWN: THIS"); L("PAGE'S VOICE, OR THE SONG"); L("SEQUENCER: THE DRUM ABOVE"); L("GLO, SEL + OCT-: EMPTY PIECE"); break;
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
    if (LOOK_CONSOLE) {                          /* (0.17.8) a sheet: the button's name large on a strip, what it does in lines with a mark each */
        cv_rect(4, 4, 232, 136, K_PANEL);
        cv_rect(4, 4, 232, 26, mix(K_PANEL, K_ACC, 3));
        cv_rect(4, 4, 4, 26, K_ACC);
        cv_text(14, 7, &FONT_B, BTN_LABEL[ui.help - 1], K_TEXT);
        cv_text(232 - text_w(&FONT_XS, "HELP"), 9, &FONT_XS, "HELP", K_ACC);
        for (k = 0; k < n; k++) {
            cv_rect(14, 47 + k * 22, 4, 4, K_ACC);
            cv_text(24, 38 + k * 22, &FONT_S, l[k], K_TEXT);
        }
        return;
    }
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_rect(16, 14, 208, 26, K_PAD);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k], K_TEXT);
}

/* the FX menu (FX on): the 27 keys, F3 at the top left, down the columns; an effect held, lit */
static void console_fx(void);
static void con_tile(int k, const char *lb, uint16_t kc, int on, int locked);
static int32_t con_chip(int32_t x, const char *t, int lit);
static void draw_fx(void)
{
    int k;
    if (LOOK_CONSOLE) {
        console_fx();
        return;
    }
    {
        const char *hd = fx_bank == 3 ? "DJ SNIPS: HOLD TO PLAY" : fx_bank == 2 ? "FLEX: TAP LATCHES" : fx_bank ? "BEAT: TAP LATCHES" : "FX: TAP LATCHES",
                   *dl = fx_bank == 3 ? "DIAL: FLEX" : fx_bank == 2 ? "DIAL: BEAT / SNIPS" : fx_bank ? "DIAL: FX / FLEX" : "DIAL: BEAT FX";
        cv_text(4, 1, &FONT_XS, hd, K_ACC);
        cv_text(236 - text_w(&FONT_XS, dl), 1, &FONT_XS, dl, K_DIM);
    }
    for (k = 0; k < NKEYS; k++) {
        int32_t x = 2 + (k / 9) * 79, y = 16 + (k % 9) * 14;
        int pc = (5 + k) % 12, oct = 3 + (5 + k) / 12, on = key_on(k),
            locked = key_locked(k);
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
            cv_text(x + 27, y, &FONT_XS, key_flex(k) >= 0 ? FLEX_LABEL[key_flex(k)] : FX_LABEL[KEY_FX[fx_bank][k]], on ? K_BG : kc);
        }
    }
}

/* the Console layout's FX menu (0.17.6): the banks as tabs, the keys as tiles in the menu's columns (F3 top left, down), the black keys' rows
 * darker; an effect's colour on its stripe and name, the whole tile when it sounds, a dark foot when it is latched */
static void console_fx(void)
{
    static const char *const BANK[4] = {"FX", "BEAT", "FLEX", "SNIPS"};
    int k;
    int32_t x = 2;
    for (k = 0; k < 4; k++)                      /* the banks: the dial walks them */
        x = con_chip(x, BANK[k], k == fx_bank);
    {
        const char *hd = fx_bank == 3 ? "HOLD TO PLAY" : ui.lock_mode ? "LOCK: TAP A KEY" : "TAP LATCHES";
        cv_text(236 - text_w(&FONT_XS, hd), -2, &FONT_XS, hd, ui.lock_mode ? K_ACC : K_DIM);
    }
    for (k = 0; k < NKEYS; k++) {
        const int32_t tx = 2 + (k / 9) * 79, ty = 16 + (k % 9) * 14;
        const int pc = (5 + k) % 12;
        const int black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const uint16_t base = black ? mix(K_BG, K_PANEL, 3) : mix(K_PANEL, K_TEXT, 2);   /* (0.17.23) the black keys dark, the white ones lighter: told apart at a glance (a Reddit player hit Tape stop by mistake) */
        const char *lb, *t = EN_NOTE_NAME[pc];   /* (the note without its octave: the columns say where) */
        uint16_t kc;
        int on, locked = 0;
        (void)tx, (void)ty, (void)base, (void)t;
        if (fx_bank == 3) {
            lb = en_dj_name(k), kc = KC_MOVE, on = (int)(ui.keys >> k & 1u);
        } else if (fx_bank == 0 && k == FILL_KEY) {
            lb = "FILL>", kc = KC_MOVE, on = (int)(ui.keys >> k & 1u);
        } else if (KEY_FX[fx_bank][k] == FX_NONE) {
            cv_rect(tx, ty, 76, 13, mix(K_BG, base, 6));
            cv_text(tx + 4, ty - 1, &FONT_XS, t, K_LINE);
            continue;
        } else {
            lb = key_flex(k) >= 0 ? FLEX_LABEL[key_flex(k)] : FX_LABEL[KEY_FX[fx_bank][k]];
            kc = fx_color(KEY_FX[fx_bank][k]), on = key_on(k), locked = key_locked(k);
        }
        con_tile(k, lb, kc, on, locked);         /* (latched: a dark line along its foot) */
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
/* the Console layout's settings pages: the page's four settings as rows, large (its name, its value, a fader; the one turned lit), the walk
 * through the settings down the right, this page lit: where you are, and what the dial reaches next */
static void console_sheet(void)
{
    const int at = st_pos(ui.fx_page), hot = plat_ms() < ui.touch_until ? ui.touched : -1;
    char t[16], u[16];
    int i;
    caps(t, FXP[ui.fx_page - 1].name, sizeof t);
    cv_text(4, 0, &FONT_B, t, K_ACC);
    for (i = 0; i < 4; i++) {
        const int k = view_knob(i);
        const int32_t y = 24 + i * 29;
        int lo, hi, v;
        if (k == K_NONE)
            continue;
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(u, view_label(i), sizeof u);
        knob_text(k, t);
        caps(t, t, sizeof t);
        cv_rect(4, y, 174, 27, i == hot ? K_PANEL : mix(K_BG, K_PANEL, 9));
        cv_text(8, y - 1, &FONT_XS, u, i == hot ? K_TEXT : K_DIM);
        cv_text(174 - text_w(&FONT_B, t), y + 3, &FONT_B, t, i == hot ? K_ACC : K_TEXT);
        cv_rect(8, y + 22, 166, 2, K_LINE);
        if (hi > lo)
            cv_rect(8, y + 22, (int32_t)(v - lo) * 166 / (hi - lo), 2, i == hot ? K_ACC : K_DIM);
    }
    {   /* the walk: eleven rows of 11 clear the strip below; (0.17.47) more pages than that, the rows follow the page (CHORUS was hidden) */
        const int first = ST_N <= 11 || at < 10 ? 0 : at - 10 > ST_N - 11 ? ST_N - 11 : at - 10;
        for (i = first; i < ST_N && i < first + 11; i++) {
            const int32_t y = 24 + (i - first) * 11;
            caps(t, FXP[ST_ORDER[i] - 1].name, 9);
            cv_rect(184, y, 52, 10, i == at ? K_ACC : K_PANEL);
            cv_text(187, y - 3, &FONT_XS, t, i == at ? K_BG : K_DIM);
        }
    }
    if (ui.fx_page == 37) {                      /* LINK (0.17.47): who is there, who leads */
        const int want = lk_want();
        enum { LK_Y = 106, LK_DY = 12 };         /* (a third line: the cable's packets) */
        int32_t x = cv_text(8, LK_Y, &FONT_XS, "PARTNER ", K_DIM);
        cv_text(x, LK_Y, &FONT_XS, !ui.fxs[EN_FXS_LK_MODE] ? "LINK OFF" : !lk_role ? "NONE HEARD" : (lk_pk & 63u) == 2u ? "PERF-1 SMK" : "PERF-1 FM-1",
                lk_role ? K_ACC : K_DIM);
        x = cv_text(8, LK_Y + LK_DY, &FONT_XS, "THIS ONE ", K_DIM);
        cv_text(x, LK_Y + LK_DY, &FONT_XS, lk_role == 1 ? "LEADS" : lk_role == 2 ? "FOLLOWS" : lk_role == 3 ? (want ? "TWO LEADS: SET ONE" : "NO LEAD: SET ONE") :
                lk_mode() == 1 ? (ui.usb_host ? "WOULD LEAD (HOST)" : "WOULD FOLLOW") : want ? "WOULD LEAD" : "-",
                lk_role == 1 || lk_role == 2 ? K_TEXT : lk_role ? K_ACID : K_DIM);
        if (ui.fxs[EN_FXS_LK_MODE] >= 4) {       /* the cable: packets out, in, refused */
            char d[40];
            plat_wire_diag(d);
            cv_text(8, LK_Y + 2 * LK_DY, &FONT_XS, d, K_TEXT);
        }
    }
}

static void draw_fxedit(void)
{
    const int p = ui.fx_page - 1;
    const int32_t cw = 14;
    int s;
    if (LOOK_CONSOLE && st_pos(ui.fx_page) >= 0 && p != FXP_MIDIIN && p != FXP_SAMPLE) {
        console_sheet();
        return;
    }
    cv_text(4, 1, &FONT_XS, st_pos(ui.fx_page) >= 0 ? "SETTINGS: DIAL FOR THE NEXT" : "FX EDIT: DIAL OR SEL + FX", K_ACC);
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
        } else if (p == FXP_SAMPLE) {            /* SAMPLE (0.16.1): the sounds, in the run's order, the one sounding lit */
            static const char *const SN[SMP_N] = {"KICK", "SNARE", "CLAP", "HAT", "O HAT", "PERC", "PAD", "BASS", "ACID", "PLUCK"};
            const int run = ui.smp_t0 != 0, cnt = run && plat_ms() - ui.smp_t0 < SMP_COUNT;
            int k;
            for (k = 0; k < SMP_N; k++) {
                const int32_t x = 3 + (k % 5) * 47, y = 42 + (k / 5) * 22;
                const int in = ui.smp_mode == 3 || (k >= smp_first() && k <= smp_last());
                const int lit = ui.smp_mode == 3 ? (int)(ui.keys >> WHITE_KEY[k] & 1u) : run && !cnt && ui.smp_cur == k;
                cv_round(x, y, 44, 19, 3, lit ? K_ACC : K_PANEL);
                text_c(x + 22, y + 2, &FONT_XS, SN[k], lit ? K_BG : in ? K_TEXT : K_DIM);
            }
            if (ui.smp_mode == 3) {
                cv_text(4, 92, &FONT_XS, "WHITE KEYS 1-10: A SOUND EACH,", K_DIM);
                cv_text(4, 106, &FONT_XS, "HELD AS LONG AS THE KEY.", K_DIM);
            } else if (run) {
                cv_text(4, 92, &FONT_XS, cnt ? "COUNT-IN ..." : "RECORDING: ONE SOUND A GAP.", K_ACC);
                cv_text(4, 106, &FONT_XS, "A KEY: STOP.", K_DIM);
            } else {
                cv_text(4, 92, &FONT_XS, "A KEY: START (1.5 S COUNT-IN).", K_DIM);
                cv_text(4, 106, &FONT_XS, "RECORD IT AS ONE SAMPLE, THEN CHOP.", K_DIM);
            }
            cv_text(4, 122, &FONT_XS, ui.smp_fx ? "WITH THE EFFECTS. THE MUSIC STOPS." : "DRY: NO ECHO, REVERB OR FX. MUSIC STOPS.", K_DIM);
        } else if (p == FXP_SCREEN) {            /* SCREEN (0.16.1) */
            cv_text(4, 50, &FONT_XS, "THE SCREEN'S SETTINGS. ZOOM: A KNOB", K_DIM);
            cv_text(4, 64, &FONT_XS, "TURNED SHOWS ITS NAME AND VALUE", K_DIM);
            cv_text(4, 78, &FONT_XS, "LARGE, FOR PLAYING AT A DISTANCE.", K_DIM);
            cv_text(4, 98, &FONT_XS, "THE KNOBS SET THEM. DIAL: NEXT PAGE.", K_DIM);
        } else if (p == FXP_MIDIIN + 1) {        /* FILTERS: what the types do */
            cv_text(4, 46, &FONT_XS, "EACH SYNTH'S FILTER TYPE:", K_DIM);
            cv_text(4, 60, &FONT_XS, "LOW (AS BEFORE), HIGH, BAND,", K_DIM);
            cv_text(4, 74, &FONT_XS, "NOTCH OR PEAK.", K_DIM);
            cv_text(4, 94, &FONT_XS, "THE PAD'S: ITS SAW SOUNDS", K_DIM);
            cv_text(4, 108, &FONT_XS, "(STRINGS, TRANCE, RAVE ...).", K_DIM);
            cv_text(4, 122, &FONT_XS, "KEPT FOR EVERY PIECE.", K_DIM);
        } else if (p == FXP_MIDIIN) {            /* MIDI IN (0.15.11): what comes in, and the USB host's state and what it found */
            static const char *const HS[4] = {"OFF", "LOOKING FOR A DEVICE", "DEVICE CONNECTED", "NO MIDI DEVICE"};
            static const char *const KIND[8] = {"NOTE OFF", "NOTE ON", "AFTERTOUCH", "CC", "PROGRAM", "PRESSURE", "PITCH BEND", "?"};
            const int hs = plat_usb_host_state() & 3;
            char t[40];
            int n = 0;
            cv_text(4, 46, &FONT_XS, "LAST IN", K_DIM);
            if (!ui.min_n) {
                cv_text(64, 46, &FONT_XS, "NOTHING YET", K_DIM);
            } else {
                const uint32_t st = ui.min_last >> 8 & 0xFFu, d1 = ui.min_last >> 16 & 0x7Fu, d2 = ui.min_last >> 24 & 0x7Fu;
                const char *k = KIND[st >= 0x80u && st < 0xF0u ? (st >> 4) - 8u : 7u];
                t[n++] = 'C';
                t[n++] = 'H';
                itoa_u((st & 15u) + 1u, t + n);
                while (t[n])
                    n++;
                t[n++] = ' ';
                while (*k)
                    t[n++] = *k++;
                t[n++] = ' ';
                itoa_u(d1, t + n);
                while (t[n])
                    n++;
                t[n++] = ' ';
                itoa_u(d2, t + n);
                cv_text(64, 46, &FONT_XS, t, K_TEXT);
            }
            cv_text(4, 62, &FONT_XS, "CLOCK IN", K_DIM);
            cv_text(64, 62, &FONT_XS, en_ext ? "YES: THE TEMPO FOLLOWS IT" : "NO", en_ext ? K_ACC : K_DIM);
            cv_text(4, 78, &FONT_XS, "USB HOST", K_DIM);
            cv_text(64, 78, &FONT_XS, HS[hs], hs == 2 ? K_ACC : hs == 3 ? K_TEXT : K_DIM);
            if (hs == 3) {
                plat_usb_host_diag(t);
                cv_text(64, 92, &FONT_XS, t, K_TEXT);
            }
            cv_text(4, 112, &FONT_XS, "USB HOST NEEDS A POWERED HUB; ITS", K_DIM);
            cv_text(4, 126, &FONT_XS, "COMPUTER USB IS OFF WHILE IT'S ON.", K_DIM);
        } else {
            const int spare = FXP[p].bit >= FX_SPARE5 && FXP[p].bit <= FX_SPARE14;   /* (a settings page on a spare bit: FILTERS, SCREAM, AUTO FX; nothing held) */
            cv_text(4, 50, &FONT_XS, p == 16 ? "THE AUTO-DJ AND SCENES: NO EFFECT HERE." : p == 17 ? "THE UNIT'S SETTINGS: NO EFFECT HERE." :
                                     p == 33 ? "EFFECTS PLAYED BY THEMSELVES, EACH" : spare ? "SETTINGS, KEPT: NO KEY TO HOLD." :
                                     FXP[p].hold ? "HOLD ANY KEY TO HEAR IT." : "THE EFFECT IS ON WHILE YOU EDIT.", K_DIM);
            cv_text(4, 64, &FONT_XS, p == 33 ? "ENDING ON A PHRASE LINE. DIAL: NEXT." : p == 16 || p == 17 || spare ? "THE KNOBS SET THEM. DIAL: NEXT PAGE." :
                                     "THE KNOBS SET IT. DIAL: NEXT PAGE.", K_DIM);
            if (p == 33 && ui.fxs[EN_FXS_AF_AMT])   /* AUTO FX: what it played last (it waits while the FX menu is open) */
                cv_text(4, 78, &FONT_XS, ui.af_said ? EN_FX_NAME[ui.af_last[0]] : "NOTHING YET", ui.af_said ? K_ACC : K_DIM);
        }
    }
}

/* the second home screen, PERFORM (HOME again): the section and the auto-DJ, the beat, the seven tracks (muted,
 * soloed, playing), the four scenes, what is latched or held in FX. Its keys are the track mix (the GLO layer);
 * its knobs, the FX knobs. */
static void draw_perf(void)
{
    static const char *const TN[8] = {"KCK", "HAT", "SNR", "PRC", "BAS", "ACD", "PAD", "PLK"};
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
            if (DJ_ROUNDS[ui.dj_rounds & 3] > 1) {   /* and the round */
                while (t[n])
                    n++;
                t[n++] = ' ';
                t[n++] = 'R';
                t[n++] = (char)('1' + (ui.dj_round < 8 ? ui.dj_round : 7));
                t[n++] = '/';
                t[n++] = (char)('0' + DJ_ROUNDS[ui.dj_rounds & 3]);
                t[n] = 0;
            }
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
    if (LOOK_CONSOLE) {                           /* the Console layout: the eight tracks as channel strips, the scenes as keys, the mood a fader */
        static const uint8_t LN[8] = {0, 3, 1, 5, 6, 6, 6, 6};
        int k;
        for (i = 0; i < 8; i++) {
            const int32_t x = 2 + i * 30;
            const int muted = (ui.trk_m >> i & 1u) || (i < 4 && (ui.fx_mask >> (FX_MUTE_KICK + i) & 1u));
            const int soloed = ui.trk_s >> i & 1u, dim = muted || (ui.trk_s && !soloed);
            const uint16_t tc = i == 0 ? LANE_COL(0) : i == 1 ? LANE_COL(3) : i == 2 ? LANE_COL(1) : i == 3 ? LANE_COL(5) :
                                i == 4 ? K_BASS : i == 5 ? K_ACID : i == 7 ? IMP_TEAL : K_PAD;
            float lvl = i == EN_TR_DRONE ? en_bass_level : i == EN_TR_ACID ? en_acid_level : i == EN_TR_PAD ? en_pad_level :
                        i == EN_TR_PLUCK ? en_cm_level : en_drum_glow[LN[i]];
            const int n = dim ? 0 : (int)(fm_clampf(lvl, 0.0f, 1.0f) * 8.0f + 0.5f);
            cv_rect(x, 42, 28, 64, K_PANEL);
            cv_rect(x, 42, 28, 2, dim ? K_LINE : tc);
            text_c(x + 14, 42, &FONT_XS, TN[i], dim ? K_DIM : K_TEXT);
            for (k = 0; k < 8; k++)
                cv_rect(x + 6, 90 - k * 5, 16, 3, k < n ? (k >= 7 ? K_BASS : k >= 5 ? K_ACC : tc) : K_LINE);
            cv_rect(x + 3, 95, 22, 9, muted ? K_BASS : soloed ? K_ACC : K_BG);
            text_c(x + 14, 91, &FONT_XS, muted ? "M" : soloed ? "S" : "", K_BG);
        }
        for (i = 0; i < 4; i++) {                 /* the scenes */
            const int32_t x = 2 + i * 60;
            const int set = ui.scene_set >> i & 1u, wait = ui.scene_q == i + 1;
            char t[8] = "SCENE 1";
            t[6] = (char)('1' + i);
            cv_rect(x, 108, 56, 14, wait ? K_ACC : set ? K_PAD : K_PANEL);
            text_c(x + 28, 107, &FONT_XS, wait ? "NEXT BAR" : t, wait || set ? K_BG : K_DIM);
        }
        {   /* what is on in FX, and the mood */
            int32_t x = 26;
            int bit, nfx = 0;
            char t[8];
            cv_text(2, 122, &FONT_XS, "FX", K_DIM);
            for (bit = 0; bit < 63; bit++)
                if (ui.fx_mask >> bit & 1u) {
                    const char *s = FX_LABEL[bit];
                    if (!s || !s[0])
                        continue;
                    if (x + text_w(&FONT_XS, s) > 120) {
                        cv_text(x, 122, &FONT_XS, "+", K_DIM);
                        break;
                    }
                    x = cv_text(x, 122, &FONT_XS, s, fx_color(bit)) + 6;
                    nfx++;
                }
            if (!nfx)
                cv_text(26, 122, &FONT_XS, "NONE", K_DIM);
            cv_text(126, 122, &FONT_XS, "MOOD", K_DIM);
            cv_rect(160, 131, 60, 2, K_LINE);
            cv_rect(160, 131, ui.mood * 60 / 100, 2, ui.mood > 50 ? K_ACC : K_PAD);
            cv_rect(160 + ui.mood * 60 / 100 - 2, 127, 5, 10, K_TEXT);
            itoa_u(ui.mood, t);
            cv_text(238 - text_w(&FONT_XS, t), 122, &FONT_XS, t, K_TEXT);
        }
        return;
    }
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
                            i == 4 ? K_BASS : i == 5 ? K_ACID : i == 7 ? IMP_TEAL : K_PAD;
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
                                       "GLO       REMOTE (BROWSER)", "ARP       ANSWER NEXT BAR"};
    static const char *const GEN[8] = {"F3 - D4   PATTERN 1-6", "PRESETS   PIECE, NEXT BAR", "ALGO      SOUND / KIT",
                                       "ENV       VOICE BY HAND", "SEQ       LINKS ON / OFF", "ARP       ANSWER NEXT BAR",
                                       "GLO       REMOTE (BROWSER)", "REC       CAPTURE (JAM)"};
    static const char *const SQ[8] = {"WHITE KEYS   PATTERN 1-6", "BLACK KEYS   COPY HERE TO 1-6", "OCT-         CLEAR THIS LANE",
                                      "OCT+         THE LANE'S OWN", "PRESETS      PIECE, NEXT BAR", "ALGO         KIT",
                                      "SEQ          LINKS ON / OFF", "FX           EDIT PAGES"};
    static const char *const PF[8] = {"F3 - D4   DRUM PATTERN 1-6", "A#4       DRUMS: NEXT PATTERN", "F#4       NEW ACID LINE",
                                      "F#5       NEW PLUCK LINE", "ARP       ANSWER NEXT BAR", "SEL TAP   THE SWELL",
                                      "PRESETS   PIECE, NEXT BAR", "ENV       VOICE BY HAND"};   /* (0.17.23) Perform and the visualizers */
    const char *const *l = ui.view == V_SEQ ? SQ : ui.fx_menu && !ui.fx_page ? FXG : perf_like() ? PF : GEN;
    int k;
    if (LOOK_CONSOLE) {                          /* (0.17.9) a tab; each line the key on a chip, what it does beside it */
        const int32_t x = con_chip(2, "SHIFT", 1);
        cv_text(x + 2, -2, &FONT_XS, "SEL HELD +", K_DIM);
        for (k = 0; k < 8; k++) {
            const char *q = ui.view == V_ACLINE && l == GEN && k == 0 ? "MUTATE    WHAT IT CHANGES" : l[k];
            char key[16];
            int j = 0;
            const int32_t y = 17 + k * 15;
            if (!q[0])
                continue;
            while (*q && !(q[0] == ' ' && q[1] == ' ') && j < 15)   /* the key: up to the gap */
                key[j++] = *q++;
            key[j] = 0;
            while (*q == ' ')
                q++;
            cv_rect(4, y, 232, 13, k & 1 ? mix(K_BG, K_PANEL, 6) : K_PANEL);
            cv_rect(4, y, 2, 13, K_ACC);
            cv_text(10, y - 1, &FONT_XS, key, K_ACC);
            cv_text(100, y - 1, &FONT_XS, q, K_TEXT);
        }
        return;
    }
    cv_text(4, 1, &FONT_XS, "SHIFT: SEL +", K_ACC);
    for (k = 0; k < 8; k++)
        if (l[k][0])
            cv_text(8, 18 + k * 15, &FONT_XS, ui.view == V_ACLINE && l == GEN && k == 0 ? "MUTATE    WHAT IT CHANGES" : l[k], K_TEXT);   /* (the Acid line page: its fourth knob's shift) */
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
    if (en_q_drops) {                            /* (0.17.48) commands the engine never got (the queue full): never seen; say if it shows */
        c[n++] = ' ', c[n++] = 'Q', c[n++] = 'D', c[n++] = ' ';
        itoa_u(en_q_drops > 999u ? 999u : en_q_drops, c + n);
        while (c[n])
            n++;
    }
    c[n] = 0;
}

/* the Console look's key tiles (0.17.9: the FX menu's, the mix's): key k in the menus' columns (F3 top left, down), its note, its label in its
 * colour on a stripe; the whole tile lit while on, a dark foot while latched; the black keys' rows darker */
/* (0.17.40) 9 x 9 one-bit icons (bit 8 the left column): the mix keys' mute, solo, freeze and scene */
static const uint16_t ICON9[4][9] = {
    {0x010, 0x030, 0x075, 0x1F2, 0x1F5, 0x070, 0x030, 0x010, 0x000},   /* mute: a speaker, crossed */
    {0x07C, 0x082, 0x101, 0x101, 0x183, 0x1C7, 0x1C7, 0x183, 0x000},   /* solo: headphones */
    {0x010, 0x092, 0x054, 0x038, 0x1FF, 0x038, 0x054, 0x092, 0x010},   /* freeze: a snowflake */
    {0x1EF, 0x1EF, 0x1EF, 0x000, 0x1EF, 0x1EF, 0x1EF, 0x000, 0x000},   /* scene: four squares */
};
static void cv_icon(int32_t x, int32_t y, int i, uint16_t c)
{
    int r, b;
    for (r = 0; r < 9; r++)
        for (b = 0; b < 9; b++)
            if (ICON9[i & 3][r] >> (8 - b) & 1u)
                cv_pset(x + b, y + r, c);
}
static void con_tile(int k, const char *lb, uint16_t kc, int on, int locked)
{
    const int32_t tx = 2 + (k / 9) * 79, ty = 16 + (k % 9) * 14;
    const int pc = (5 + k) % 12;
    const int black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    const uint16_t base = black ? mix(K_BG, K_PANEL, 3) : mix(K_PANEL, K_TEXT, 2);   /* (0.17.23) the black keys dark, the white ones lighter: told apart at a glance (a Reddit player hit Tape stop by mistake) */
    cv_panel(tx, ty, 76, 13, on ? kc : base, 0);
    if (!on)
        cv_rect(tx, ty, 2, 13, kc);
    if (locked)
        cv_rect(tx + 2, ty + 11, 72, 2, K_BG);
    cv_text(tx + 4, ty - 1, &FONT_XS, EN_NOTE_NAME[pc], on ? K_BG : K_DIM);
    if (lb[0] >= 1 && lb[0] <= 4) {              /* (0.17.40) a label that starts with an icon: mute, solo, freeze, scene, in place of "M:" "S:" "F:" */
        cv_icon(tx + 22, ty + 2, lb[0] - 1, on ? K_BG : kc);
        cv_text_fit(tx + 33, ty - 1, &FONT_XS, lb + 1, on ? K_BG : kc, 42, 0);
        return;
    }
    cv_text_fit(tx + 22, ty - 1, &FONT_XS, lb, on ? K_BG : kc, 53, 0);   /* (0.17.39: its own column, "C# PHASER" not "C#PHASER"; TAP TEMPO fitted) */
}
static int32_t con_chip(int32_t x, const char *t, int lit)   /* a tab along the top: lit (the accent, dark text) or not; where it ends */
{
    const int32_t w = text_w(&FONT_XS, t) + 10;
    cv_panel(x, 1, w, 12, lit ? K_ACC : K_PANEL, 0);
    cv_text(x + 5, -2, &FONT_XS, t, lit ? K_BG : K_DIM);
    return x + w + 2;
}

/* the track mix (GLO held): the keys, as the FX menu's, each its track's mute (M:) or solo (S:), lit while on */
static void draw_mix(void)
{
    int k;
    char c[40];
    cpu_line(c);
    if (LOOK_CONSOLE) {                          /* (0.17.9) a tab, what the keys do, the load at the right */
        const int32_t x = con_chip(2, "MIX", 1);
        cv_text(x + 2, -2, &FONT_XS, "GLO + KEY", K_DIM);
        if (en_shed_level || plat_xruns())       /* 0.17.39: the load only when it matters (an xrun, the engine easing off; SEL's help has it always) */
            cv_text(236 - text_w(&FONT_XS, c), -2, &FONT_XS, c, K_ACC);
    } else {
        cv_text(4, 1, &FONT_XS, "MIX: GLO + KEY", K_ACC);
        cv_text(118, 1, &FONT_XS, c, K_DIM);
    }
    if (ui.dj_on) {                            /* the auto-DJ: where it is, under the load */
        char d[24];
        const char *pn = ui.dj_phase == 0 ? "DJ GROOVE " : ui.dj_phase == 1 ? "DJ BREAK " : "DJ BUILD ";
        int n = 0;
        while (*pn)
            d[n++] = *pn++;
        itoa_u(ui.dj_n, d + n);
        if (DJ_ROUNDS[ui.dj_rounds & 3] > 1) {   /* the round */
            while (d[n])
                n++;
            d[n++] = ' ';
            d[n++] = 'R';
            d[n++] = (char)('1' + (ui.dj_round < 8 ? ui.dj_round : 7));
            d[n++] = '/';
            d[n++] = (char)('0' + DJ_ROUNDS[ui.dj_rounds & 3]);
            d[n] = 0;
        }
        if (LOOK_CONSOLE)                        /* (the Console look: in place of the load, which it sits over) */
            cv_rect(118, 0, 120, 14, K_BG);
        cv_text(LOOK_CONSOLE ? 236 - text_w(&FONT_XS, d) : 118, LOOK_CONSOLE ? -2 : 8, &FONT_XS, d, K_ACC);
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
            if (LOOK_CONSOLE)                    /* (0.17.40) the Console tiles: an icon, mute or solo */
                lb[j++] = code < 8 ? '\1' : '\2';
            else
                lb[j++] = code < 8 ? 'M' : 'S', lb[j++] = ':';
            while (*q && j < 11)
                lb[j++] = *q++;
            lb[j] = 0;
        } else if (code == 16 || code == 17) {
            const char *q = LOOK_CONSOLE ? (code == 16 ? "\1DRUMS" : "\2DRUMS") : code == 16 ? "M:DRUMS" : "S:DRUMS";
            while (*q)
                lb[j++] = *q++;
            lb[j] = 0;
        } else if (code >= 20 && code <= 23) {
            const char *q = LOOK_CONSOLE ? "\4SCENE " : "SCENE ";
            while (*q)
                lb[j++] = *q++;
            lb[j++] = (char)('1' + code - 20);
            lb[j] = 0;
        } else if (code >= 30 && code <= 32) {
            const char *q = LOOK_CONSOLE ? (code == 30 ? "\3ACID" : code == 31 ? "\3PLUCK" : "\3BASS") : code == 30 ? "F:ACID" : code == 31 ? "F:PLUCK" : "F:BASS";
            while (*q)
                lb[j++] = *q++;
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
            if (LOOK_CONSOLE) {
                con_tile(k, lb, kc, on, 0);
                continue;
            }
            if (on)
                cv_round(x - 1, y - 1, 77, 14, 2, kc);
            else
                cv_rect(x - 1, y + 1, 2, 10, kc);
            cv_text(x + 3, y, &FONT_XS, t, on ? K_BG : black ? K_DIM : K_TEXT);
            cv_text(x + 27, y, &FONT_XS, lb, on ? K_BG : kc);
        }
    }
}

/* two drawing helpers on the canvas (from HYP-1's game, KICK RUNNER, which left in 0.17.79): a soft glow, and a smooth line */
static void aa_glow(float cx, float cy, float r0, float r1, uint16_t c, float s)   /* c fading out from r0 to r1 */
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
static void aa_seg(float x0, float y0, float x1, float y1, uint16_t c, float s)
{
    float dx = x1 - x0, dy = y1 - y0, ax = dx < 0.0f ? -dx : dx, ay = dy < 0.0f ? -dy : dy, t, step;
    int n, k;
    if (s <= 0.02f)
        return;
    if (ax >= ay) {
        if (x1 < x0) {
            t = x0, x0 = x1, x1 = t;
            t = y0, y0 = y1, y1 = t;
        }
        n = (int)(x1 - x0) + 1;
        step = ax > 0.0f ? (y1 - y0) / (x1 - x0) : 0.0f;
        for (k = 0; k <= n; k++) {
            float x = x0 + (float)k, y = y0 + step * (float)k, f;
            int32_t iy;
            if (x > x1 + 0.5f)
                break;
            iy = (int32_t)fm_floorf(y);
            f = y - (float)iy;
            px_blend((int32_t)x, iy, c, (uint32_t)((1.0f - f) * s * 16.0f + 0.5f));
            px_blend((int32_t)x, iy + 1, c, (uint32_t)(f * s * 16.0f + 0.5f));
        }
    } else {
        if (y1 < y0) {
            t = x0, x0 = x1, x1 = t;
            t = y0, y0 = y1, y1 = t;
        }
        n = (int)(y1 - y0) + 1;
        step = (x1 - x0) / (y1 - y0);
        for (k = 0; k <= n; k++) {
            float y = y0 + (float)k, x = x0 + step * (float)k, f;
            int32_t ix;
            if (y > y1 + 0.5f)
                break;
            ix = (int32_t)fm_floorf(x);
            f = x - (float)ix;
            px_blend(ix, (int32_t)y, c, (uint32_t)((1.0f - f) * s * 16.0f + 0.5f));
            px_blend(ix + 1, (int32_t)y, c, (uint32_t)(f * s * 16.0f + 0.5f));
        }
    }
}

/* THE SCOPES: on the pages that shape a sound the main area draws what the knobs make: the envelopes' curves, the
 * LFOs' waves, the filters' response, the echo and the reverb. The shape follows the knobs as they turn; a dot
 * runs along it so the page is alive. (Drawn from the settings, not measured: it shows the idea, not the exact milliseconds.) */
#define SC_X 12
#define SC_W 216
#define SC_Y 24
#define SC_H 90
enum { SCOPE_NONE, SCOPE_PENV, SCOPE_DENV, SCOPE_AENV, SCOPE_PLFO, SCOPE_DLFO, SCOPE_DFILT, SCOPE_AFILT, SCOPE_SPACE, SCOPE_CMEL, SCOPE_MASTER, SCOPE_MIX, SCOPE_REVERB, SCOPE_GROOVE, SCOPE_ACANS, SCOPE_STEPS, SCOPE_ACSTEP, SCOPE_CMSTEP, SCOPE_PROG, SCOPE_SETL, SCOPE_BLSTEP };
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
    case V_STEPS: return SCOPE_STEPS;
    case V_ACSTEP: return SCOPE_ACSTEP;
    case V_CMSTEP: return SCOPE_CMSTEP;
    case V_BLSTEP: return SCOPE_BLSTEP;
    case V_PROG: return SCOPE_PROG;
    case V_SETL: return SCOPE_SETL;
    case V_ACANS: return SCOPE_ACANS;
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
#define SC_SCREEN mix(K_BG, K_PANEL, 7)           /* the Console look's scope screen (0.17.8) */
static void scope_trace(uint16_t col)            /* sc_y[x] (0 .. SC_H): a filled curve with a bright line, and the baseline */
{
    int32_t x;
    const uint16_t fill = LOOK_CONSOLE ? mix(SC_SCREEN, col, 3) : mix(K_BG, col, 4);   /* (Console: a faint fill on the screen) */
    cv_rect(SC_X, sc_oy + sc_oh, SC_W, 1, K_LINE);
    if (!LOOK_CONSOLE)
        cv_rect(SC_X, sc_oy, 1, sc_oh, K_LINE);
    for (x = 0; x < SC_W; x++) {
        const int32_t y = sc_y[x];
        if (y > 1)
            cv_rect(SC_X + x, sc_oy + sc_oh - y, 1, y, fill);
    }
    for (x = 1; x < SC_W; x++) {                 /* (0.17.39) the line smooth: two anti-aliased strokes a column, as thick as the old one */
        const float x0 = (float)(SC_X + x - 1), y0 = (float)(sc_oy + sc_oh - sc_y[x - 1]) - 1.0f, y1 = (float)(sc_oy + sc_oh - sc_y[x]) - 1.0f;
        aa_seg(x0, y0, x0 + 1.0f, y1, col, 1.0f);
        aa_seg(x0, y0 + 1.0f, x0 + 1.0f, y1 + 1.0f, col, 1.0f);
    }
}

/* the picture pages' frame (0.17.8): the title and what it says, and the line under the picture. The Console look: the title small in its
 * colour on a panel strip with a tab of it, the curves on a dark screen with a grid; the old look: the title large */
static int32_t sc_tx;                            /* where the title ends */
static void sc_title(const char *t, uint16_t col)
{
    if (LOOK_CONSOLE) {
        cv_panel(2, 0, 236, 17, K_PANEL, 0);
        cv_rect(2, 0, 3, 17, col);
        sc_tx = cv_text(10, 0, &FONT_XS, t, col);
    } else {
        sc_tx = cv_text(6, 2, &FONT_B, t, col);
    }
}
static void sc_sub(const char *t, uint16_t col) { cv_text(sc_tx + 8, LOOK_CONSOLE ? 0 : 6, &FONT_XS, t, col); }
static void sc_foot(const char *t, uint16_t col) { cv_text(6, SC_Y + SC_H + 6, &FONT_XS, t, col); }
static int sc_screen_kind(int k)                 /* the pages drawn as a curve on the scope's screen */
{
    return k == SCOPE_PENV || k == SCOPE_DENV || k == SCOPE_AENV || k == SCOPE_PLFO || k == SCOPE_DLFO || k == SCOPE_DFILT || k == SCOPE_AFILT ||
           k == SCOPE_SPACE || k == SCOPE_REVERB || k == SCOPE_CMEL;
}
static void sc_screen(void)                      /* the screen: dark, eight columns and four rows of grid */
{
    int k;
    cv_panel_g(SC_X - 1, sc_oy - 1, SC_W + 2, sc_oh + 2, mix(SC_SCREEN, 0x0000, 5), SC_SCREEN, 1);   /* (0.17.40: darker under its lip) */
    for (k = 1; k < 8; k++)
        cv_rect(SC_X + k * SC_W / 8, sc_oy, 1, sc_oh, mix(SC_SCREEN, K_LINE, k == 4 ? 9 : 5));
    for (k = 1; k < 4; k++)
        cv_rect(SC_X, sc_oy + k * sc_oh / 4, SC_W, 1, mix(SC_SCREEN, K_LINE, 5));
}

/* the step pages and Answer (0.17.9): sixteen steps as a roll, the line pages' look. A block a note at its height (lv: 0 a rest, 1 soft, 2 a note,
 * 3 an accent, taller), a slide a line into its note (slide_in: bit s, step s glides in from the one before); the step chosen lit with a mark at
 * its foot, the step playing white with a mark at its head, past the length dim. lo, span: the notes' range (span 0: from the notes) */
static void step_roll(int32_t y0, int32_t h, uint16_t col, const int16_t *pit, const uint8_t *lv, uint32_t slide_in, int len, int cur, int pos, int lo,
                      int span)
{
    const int32_t x0 = 8, cw = 14;
    int st, px = -1, py = 0;
    if (span <= 0) {
        int hi = -1000;
        lo = 1000;
        for (st = 0; st < 16; st++)
            if (lv[st]) {
                lo = pit[st] < lo ? pit[st] : lo;
                hi = pit[st] > hi ? pit[st] : hi;
            }
        span = hi - lo;
    }
    if (span < 7)
        span = 7;
    cv_rect(x0 - 2, y0, 16 * cw + 4, h, mix(K_PANEL, col, 2));
    for (st = 0; st < 16; st++) {
        const int32_t x = x0 + st * cw;
        if (st == cur)
            cv_rect(x, y0, cw, h, mix(K_PANEL, col, 5));
        else if (st >= len)
            cv_rect(x, y0, cw, h, mix(K_BG, K_PANEL, 6));
        cv_rect(x, y0, 1, h, st % 4 ? mix(K_PANEL, K_LINE, 8) : K_LINE);
        if (st == pos)
            cv_rect(x + 1, y0, cw - 1, 2, K_TEXT);
        if (st == cur)
            cv_rect(x + 1, y0 + h - 2, cw - 1, 2, K_ACC);
    }
    for (st = 0; st < 16; st++) {
        const int32_t x = x0 + st * cw + 2;
        int32_t y;
        uint16_t c;
        if (!lv[st]) {
            px = -1;
            continue;
        }
        y = y0 + h - 9 - (int32_t)(pit[st] - lo) * (h - 16) / span;
        y = y < y0 + 4 ? y0 + 4 : y > y0 + h - 9 ? y0 + h - 9 : y;
        c = st >= len ? K_LINE : st == pos ? K_TEXT : lv[st] == 3 ? col : lv[st] == 1 ? mix(K_PANEL, col, 6) : mix(K_PANEL, col, 11);
        if (px >= 0 && (slide_in >> st & 1u)) {  /* a slide: a line from the note before into this one */
            aa_seg((float)px, (float)py + 1.0f, (float)x, (float)y + 1.0f, c, 1.0f);   /* (0.17.39: smooth) */
            aa_seg((float)px, (float)py + 2.0f, (float)x, (float)y + 2.0f, c, 1.0f);
        }
        cv_rect(x, y - (lv[st] == 3 ? 1 : 0), cw - 4, lv[st] == 3 ? 6 : 4, c);
        px = x + cw - 5, py = y;
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
/* (0.17.19) a filter type's response at r = f / fc, as the engine's SVF sounds it (q = 1 / k): 0 low, 1 high, 2 band (1 at its centre), 3 notch,
 * 4 peak (low less high), 5 the 303 (two poles resonant, two flat); two: the 24 dB types, a flat second stage (above the corner for the low-pass,
 * below for the high-pass, as the voices run it) */
static float filt_mag(int t, float r, float q, int two)
{
    const float a = 1.0f - r * r, d = fm_sqrtf(a * a + r * r / (q * q) + 1e-9f);
    float m;
    switch (t) {
    case 1: m = r * r / d; break;
    case 2: m = r / q / d; break;
    case 3: m = fm_fabsf(a) / d; break;
    case 4: m = (1.0f + r * r) / d; break;
    default: m = 1.0f / d; break;
    }
    if ((two && t <= 1) || t == 5) {
        const float r2 = t == 1 ? r * 1.2f : r * (1.0f / 1.2f), a2 = 1.0f - r2 * r2, d2 = fm_sqrtf(a2 * a2 + r2 * r2 * 1.44f + 1e-9f);
        m *= t == 1 ? r2 * r2 / d2 : 1.0f / d2;
    }
    return m;
}
/* the response over 20 Hz .. 20 kHz into sc_y (h: the trace's height), in decibels: -36 dB at the foot, the top at +6 dB or the peak (with room),
 * so a flat passband sits at the same height whatever the type and a slope shows beside a resonant peak */
static float filt_db(int t, int32_t x, float fc, float q, int two)
{
    return 6.0206f * fm_log2f(filt_mag(t, 20.0f * fm_exp2f((float)x / (float)(SC_W - 1) * 10.0f) / fc, q, two) + 1e-6f);
}
static void filt_curve(int t, float fc, float q, int two, int32_t h)   /* (worked out twice, its top then the trace: no array, RAM is short) */
{
    float top = 12.0f;                           /* (0.17.20: the pass band lower, about 60% up; the maker: "a little high") */
    int32_t x;
    for (x = 0; x < SC_W; x++) {
        const float m = filt_db(t, x, fc, q, two);
        top = m > top ? m : top;
    }
    top += 9.0f;
    for (x = 0; x < SC_W; x++)
        sc_y[x] = (uint8_t)(fm_clampf((filt_db(t, x, fc, q, two) + 36.0f) / (top + 36.0f), 0.0f, 1.0f) * (float)h);
}
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
        const int dips = ui.fxs[EN_FXS_PM_RATE] >= 4 ? 4 : 2 << (ui.fxs[EN_FXS_PM_RATE] & 3);   /* (Kick: shown as four to the bar) */
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
    case 27:                                     /* (0.17.19) the punch-in FILTER: its type at its cutoff and resonance */
        if (FXP[p].bit != FX_TFILT)
            return 0;
        filt_curve(ui.fxs[EN_FXS_TF_TYPE] < 5 ? ui.fxs[EN_FXS_TF_TYPE] : ui.fxs[EN_FXS_TF_TYPE] < 10 ? 0 : ui.fxs[EN_FXS_TF_TYPE] < 21 ? 2 : 1,   /* (Morph: its nearest) */
                   40.0f * fm_exp2f((float)ui.fxs[EN_FXS_TF_CUT] * 0.0864f),
                   1.0f / (2.0f - 0.0195f * (float)ui.fxs[EN_FXS_TF_RESO]), 0, H - 6);
        scope_trace(col);
        return 1;
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
    if (LOOK_CONSOLE && sc_screen_kind(kind))
        sc_screen();                             /* (the Console look: a dark screen with a grid behind the curve) */
    if (kind == SCOPE_REVERB) {                  /* the reverb: the gap of the predelay, then the tail by its type */
        static const char *const TY[3] = {"PLATE", "SHIMMER", "GATED"};
        const int ty = proj.par[P_RV_TYPE] < 0 ? 0 : proj.par[P_RV_TYPE] > 2 ? 2 : proj.par[P_RV_TYPE];
        const float len = 0.25f + 0.75f * (float)proj.par[P_SIZE] * 0.01f, amt = (float)proj.par[P_RV_X] * 0.01f,
                    gap = (6.0f + 24.0f * (float)proj.par[P_RV_PRE] * 0.01f) * (1.0f / 30.0f) * 0.2f, hold = 0.12f + 0.45f * amt;
        col = IMP_TEAL;
        sc_title("REVERB", col);
        sc_sub(TY[ty], K_TEXT);
        sc_foot(ty == 1 ? "AN OCTAVE UP, BLOOMING IN THE TAIL" : ty == 2 ? "CUT OFF AFTER THE HOLD" : "THE PLATE'S TAIL, AFTER THE PREDELAY", K_DIM);
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
    if (kind == SCOPE_ACANS) {                   /* the acid's answer: the line as it is played (the call), and as it is answered, a dot a step at its note's height */
        static const char *const RN[9] = {"AS IT IS", "OCTAVE", "FIFTH", "THIRD", "MIRROR", "REVERSE", "ROTATE", "SPARSE", "ACCENTS"};
        static const char *const RD[5] = {"DOWN 2", "DOWN", "SAME REGISTER", "UP", "UP 2"};
        const int resp = proj.par[P_AC_RESP], rk = resp / 5, len = en_acid_len ? en_acid_len : 16;
        int row, s;
        static const char *const ORD[16] = {"EVERY 2ND BAR", "EVERY 3RD BAR", "EVERY 4TH BAR", "EVERY 5TH BAR", "EVERY 6TH BAR", "EVERY 7TH BAR", "EVERY 8TH BAR", "BAR BEFORE A DRUM CHANGE",
                                            "EVERY 2ND LOOP", "EVERY 3RD LOOP", "EVERY 4TH LOOP", "EVERY 5TH LOOP", "EVERY 6TH LOOP", "EVERY 7TH LOOP", "EVERY 8TH LOOP", "LOOP AFTER A DRUM FILL"};
        const char *tx = ORD[(proj.par[P_AC_EVERY] < 0 ? 0 : proj.par[P_AC_EVERY] > 7 ? 7 : proj.par[P_AC_EVERY]) + (proj.par[P_AC_CHANCE] ? 8 : 0)];
        sc_title("ANSWER", K_ACID);
        sc_sub(resp ? "THE LINE ASKS, THEN ANSWERS" : "OFF: TURN ANSWER", K_DIM);
        int16_t ap[2][16];
        uint8_t al[2][16];
        int lo = 127, hi = 0;
        for (row = 0; row < 2; row++)            /* (both on one scale, so an octave up is seen up) */
            for (s = 0; s < 16; s++) {
                const int n = en_acid_preview(s, row ? resp : 0);
                ap[row][s] = (int16_t)(n < 0 ? 0 : n);
                al[row][s] = (uint8_t)(n >= 0 ? 2 : 0);
                if (n >= 0) {
                    lo = n < lo ? n : lo;
                    hi = n > hi ? n : hi;
                }
            }
        for (row = 0; row < 2; row++) {
            const int32_t top = row ? 83 : 31, h = 36;
            const int r = row ? resp : 0;
            if (row && resp) {
                char lb[28];
                const char *q = RN[rk], *d = RD[resp % 5];
                int j = 0;
                while (*q)
                    lb[j++] = *q++;
                lb[j++] = ' ';
                while (*d && j < 26)
                    lb[j++] = *d++;
                lb[j] = 0;
                cv_text(4, top - 14, &FONT_XS, lb, en_acid_ans ? K_TEXT : K_ACC);
            } else {
                cv_text(4, top - 14, &FONT_XS, row ? RN[0] : "CALL", en_acid_ans && !row ? K_DIM : row ? K_ACC : K_ACID);
            }
            if (row)
                cv_text(236 - text_w(&FONT_XS, tx), top - 14, &FONT_XS, tx, K_DIM);
            (void)r;
            step_roll(top, h, row ? K_ACC : K_ACID, ap[row], al[row], row ? 0u : ((uint32_t)en_acid_slide << 1) & 0xFFFFu, len, -1,
                      en_playing ? (int)en_acid_pos : -1, lo, hi - lo);   /* (the call's slides; an answer's are its own) */
        }
        return;
    }
    if (kind == SCOPE_GROOVE) {                  /* the groove: each lane as it plays (a hit added: bright; one taken out: a dash), the lane chosen framed, its source and density */
        int l, s;
        sc_title("GROOVE", K_ACC);
        sc_sub("DENSITY, SOURCE: PER LANE", K_DIM);
        if (LOOK_CONSOLE) {                      /* (0.17.12) a strip a lane: its name in its colour, the hits in beats of four (added: bright; taken out: a dash), the step
                                                  * playing lit; where it plays from as a chip (= the piece's pattern, P2 its own), its density as a meter (the tick: 50) */
            for (l = 0; l < EN_NLANE; l++) {
                const int src = play_src(l), dens = proj.par[P_DENS + l];
                const uint16_t wr = en_pats[src][l], pl = en_lane_mask(l, src, dens), lc = LANE_COL(l);
                const int32_t y = 20 + l * 18, sel = l == (int)ui.lane, own = proj.par[P_LSRC + l] != 0;
                char tx[4];
                cv_rect(2, y, 236, 16, sel ? mix(K_PANEL, lc, 3) : K_PANEL);
                cv_rect(2, y, 3, 16, lc);
                cv_text(8, y + 1, &FONT_XS, LANE_SHORT[l], sel ? K_TEXT : lc);
                for (s = 0; s < 16; s++) {
                    const int32_t x = 46 + s * 9 + (s >> 2) * 2;
                    const int on = pl >> s & 1, was = wr >> s & 1;
                    if (en_playing && s == (int)en_lane_pos[l])
                        cv_rect(x - 1, y + 1, 9, 14, mix(K_PANEL, K_TEXT, 4));
                    if (on)
                        cv_rect(x, y + 3, 7, 10, was ? lc : K_TEXT);
                    else if (was)
                        cv_rect(x, y + 7, 7, 2, K_LINE);
                    else
                        cv_rect(x + 3, y + 8, 1, 1, mix(K_PANEL, K_LINE, 8));
                }
                tx[0] = own ? 'P' : '=';
                tx[1] = own ? (char)('0' + proj.par[P_LSRC + l]) : 0;
                tx[2] = 0;
                cv_rect(198, y + 3, 14, 10, own ? K_ACC : mix(K_BG, K_PANEL, 8));
                text_c(205, y + 1, &FONT_XS, tx, own ? K_BG : K_DIM);
                cv_rect(216, y + 6, 20, 4, K_LINE);
                cv_rect(216, y + 6, dens * 20 / 100, 4, dens == 50 ? K_DIM : lc);
                cv_rect(226, y + 4, 1, 8, K_TEXT);
            }
            return;
        }
        for (l = 0; l < EN_NLANE; l++) {
            const int src = play_src(l), dens = proj.par[P_DENS + l];
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
    if (kind == SCOPE_STEPS) {                   /* the steps: the lane's sixteen steps as they play, each with its level (height), push (shift) and ratchet (the bars under it); the step chosen framed */
        static const uint8_t NH[8] = {1, 2, 3, 4, 6, 2, 3, 4};                            /* a ratchet's hits */
        static const uint8_t RV[8][6] = {{9, 0, 0, 0, 0, 0}, {9, 7, 0, 0, 0, 0}, {9, 7, 5, 0, 0, 0}, {9, 7, 5, 4, 0, 0},
                                         {9, 8, 7, 6, 5, 4}, {3, 9, 0, 0, 0, 0}, {4, 6, 9, 0, 0, 0}, {3, 5, 7, 9, 0, 0}};   /* and their heights */
        static const uint8_t LH[4] = {36, 12, 22, 46};                                    /* a level's height */
        const int l = ui.lane % EN_NLANE, src = play_src(l);
        const uint16_t pl = en_lane_mask(l, src, proj.par[P_DENS + l]);
        int s;
        char tx[24];
        sc_title("STEPS", LANE_COL(l));
        sc_sub("RATCHET, PUSH, LEVEL: PER STEP", K_DIM);
        cv_text(4, 18, &FONT_XS, LANE_SHORT[l], LANE_COL(l));
        cv_text(34, 18, &FONT_XS, "KEYS CHOOSE THE STEP", K_DIM);
        if (LOOK_CONSOLE)                        /* (0.17.10) the steps on a panel in the lane's tint, as the rolls */
            cv_rect(13, 33, 213, 72, mix(K_PANEL, LANE_COL(l), 2));
        for (s = 0; s < 16; s++) {
            const int32_t x0 = 16 + s * 13;
            const uint32_t md = en_mod[l][s];
            const int on = pl >> s & 1, rt = (int)(md & 7u), pu = (int)(md >> 3 & 7u), lv = (int)(md >> 6), cur = s == (int)(ui.stp & 15u);
            if (cur)
                cv_rect(x0 - 1, 34, 13, 70, LOOK_CONSOLE ? mix(K_PANEL, LANE_COL(l), 5) : K_PANEL);
            if (s % 4 == 0)
                cv_rect(x0 - 1, 33, 1, 72, K_LINE);
            if (on)
                cv_rect(x0 + pu, 80 - LH[lv], 6, LH[lv], lv == 1 ? mix(K_BG, LANE_COL(l), 10) : LANE_COL(l));
            else
                cv_rect(x0 + 4, 79, 2, 1, K_LINE);
            if (md && !on)
                cv_rect(x0 + 2, 84, 7, 1, K_DIM);      /* (a trick on a step with no hit: it waits for one) */
            if (pu)
                cv_rect(x0, 81, 1 + pu, 1, K_TEXT);     /* the push, from the grid line to where it lands */
            if (rt) {
                int h;
                for (h = 0; h < NH[rt]; h++) {
                    const int32_t hh = (int32_t)RV[rt][h], hx = x0 + (NH[rt] > 1 ? h * 10 / (NH[rt] - 1) : 0);
                    cv_rect(hx, 96 - hh, 2, hh, on ? K_ACC : K_DIM);
                }
            }
            if (en_playing && s == (int)en_lane_pos[l])
                cv_rect(x0, 100, 11, 2, K_TEXT);
            if (cur)
                cv_rect(x0 - 1, 103, 13, 1, K_ACC);
        }
        {
            static const char *const RN[8] = {"NO RATCHET", "X2", "X3", "X4", "X6", "FLAM", "SWELL 3", "SWELL 4"};
            static const char *const LN[4] = {"NORMAL", "GHOST", "SOFT", "ACCENT"};
            const uint32_t md = en_mod[l][ui.stp & 15];
            int n = 0;
            const char *p = "STEP ";
            while (*p)
                tx[n++] = *p++;
            itoa_u((uint32_t)(ui.stp & 15u) + 1u, tx + n);
            cv_text(4, 112, &FONT_XS, tx, K_TEXT);
            cv_text(48, 112, &FONT_XS, RN[md & 7u], md & 7u ? K_ACC : K_DIM);
            n = 0;
            tx[n++] = 'P';
            tx[n++] = 'U';
            tx[n++] = 'S';
            tx[n++] = 'H';
            tx[n++] = ' ';
            itoa_u(md >> 3 & 7u, tx + n);
            cv_text(126, 112, &FONT_XS, tx, md >> 3 & 7u ? K_ACC : K_DIM);
            cv_text(176, 112, &FONT_XS, LN[md >> 6], md >> 6 ? K_ACC : K_DIM);
        }
        return;
    }
    if (kind == SCOPE_ACSTEP) {                  /* the acid steps: the line as it plays, a bar a step (its height the note), the accent bright, a slide joined to the next; the step chosen framed */
        const int len = proj.par[P_AC_LEN] < 1 ? 1 : proj.par[P_AC_LEN] > 16 ? 16 : proj.par[P_AC_LEN];
        int s;
        char tx[24];
        as_sync();
        sc_title("ACID STEPS", K_ACID);
        sc_sub(proj.par[P_AC_LOCK] ? "FROZEN" : "FREE: IT EVOLVES", proj.par[P_AC_LOCK] ? K_ACC : K_DIM);
        {   /* the roll: a step's height its note and octave, an accent taller, a slide drawn into the next (0.17.9) */
            int16_t pit[16];
            uint8_t lv[16];
            for (s = 0; s < 16; s++) {
                const int k = as_kind(s);
                pit[s] = (int16_t)(ui.as_off[s] + 7 * ui.as_oct[s]);
                lv[s] = (uint8_t)(!k ? 0 : k == 2 || k == 4 ? 3 : 2);
            }
            step_roll(22, 86, K_ACID, pit, lv, ((uint32_t)ui.as_slide << 1) & 0xFFFFu, len, ui.astp & 15, en_playing ? (int)en_acid_pos : -1, 0, 0);
        }
        {
            static const char *const KN[5] = {"REST", "NOTE", "ACCENT", "SLIDE", "ACCENT + SLIDE"};
            int n = 0;
            const char *q = "STEP ";
            while (*q)
                tx[n++] = *q++;
            itoa_u((uint32_t)(ui.astp & 15u) + 1u, tx + n);
            cv_text(4, 112, &FONT_XS, tx, K_TEXT);
            cv_text(48, 112, &FONT_XS, KN[as_kind(ui.astp & 15)], as_kind(ui.astp & 15) ? K_ACC : K_DIM);
            n = 0;
            tx[n++] = '+';
            itoa_u((uint32_t)ui.as_off[ui.astp & 15], tx + n);
            cv_text(136, 112, &FONT_XS, tx, K_DIM);
        }
        return;
    }
    if (kind == SCOPE_SETL) {                    /* the set list: sixteen slots in two columns, each its genre and piece; the one playing lit, the one chosen on a bar */
        int s, n = 0;
        char tx[24];
        for (s = 0; s < SL_N; s++)
            n += glob.sl[s] != 0;
        sc_title("SET LIST", K_ACC);
        sc_sub(glob.sl_on ? (glob.sl_end ? "PLAYING THROUGH, ONCE" : "PLAYING THROUGH, ROUND") : n ? "PLAY: OFF" : "EMPTY", glob.sl_on ? K_ACC : K_DIM);
        if (LOOK_CONSOLE) {                      /* (0.17.12) a strip a slot, two columns of eight: its genre's colour as a tab, its name, the piece as a chip; the one playing lit,
                                                  * the one chosen with a foot; an empty one a line */
            for (s = 0; s < SL_N; s++) {
                const int32_t x0 = 2 + (s >> 3) * 119, y0 = 20 + (s & 7) * 14;
                const int e = glob.sl[s], cur = s == (int)(ui.slsl % SL_N), now = glob.sl_on && s == ui.sl_pos;
                itoa_u((uint32_t)s + 1u, tx);
                if (e) {
                    const int g = (e - 1) / NPIECES;
                    const uint16_t gc = LANE_COL(g % 6);
                    cv_rect(x0, y0, 116, 12, now ? mix(K_PANEL, K_ACC, 5) : K_PANEL);
                    cv_rect(x0, y0, 3, 12, gc);
                    cv_text(x0 + 6 + (s < 9 ? 5 : 0), y0 - 1, &FONT_XS, tx, now ? K_TEXT : K_DIM);
                    cv_text(x0 + 22, y0 - 1, &FONT_XS, en_genre_name(g), now ? K_TEXT : gc);
                    itoa_u((uint32_t)(e - 1) % NPIECES + 1u, tx);
                    if (tx[1] == 0)
                        tx[2] = 0, tx[1] = tx[0], tx[0] = '0';
                    cv_rect(x0 + 96, y0 + 1, 18, 10, now ? K_ACC : mix(K_BG, K_PANEL, 8));
                    text_c(x0 + 105, y0 - 1, &FONT_XS, tx, now ? K_BG : K_TEXT);
                    if (glob.sl_bars[s]) {       /* (0.17.13) its bars; playing, how far through them; a part left out: a red tick */
                        itoa_u(glob.sl_bars[s], tx);
                        cv_text(x0 + 93 - text_w(&FONT_XS, tx), y0 - 1, &FONT_XS, tx, now ? K_TEXT : K_DIM);
                        if (now && en_playing) {
                            const uint32_t done = sl_bar_now() >= sl_b0 ? sl_bar_now() - sl_b0 : 0u;
                            cv_rect(x0 + 3, y0 + 10, (int32_t)((done >= glob.sl_bars[s] ? glob.sl_bars[s] : done) * 93u / glob.sl_bars[s]), 2, K_TEXT);
                        }
                    }
                    if (glob.sl_mute[s])
                        cv_rect(x0 + 1, y0 + 9, 2, 3, K_BASS);
                } else {
                    cv_text(x0 + 6 + (s < 9 ? 5 : 0), y0 - 1, &FONT_XS, tx, K_LINE);
                    cv_rect(x0 + 22, y0 + 6, 92, 1, K_LINE);
                }
                if (cur)
                    cv_rect(x0, y0 + 12, 116, 2, K_ACC);
            }
            if (ui.btn >> B_SEL & 1u) {   /* SEL held: the chosen place's pattern and energy (SEL + KNOB2, KNOB3) */
                const int c = ui.slsl % SL_N;
                int32_t x = cv_text(4, 133, &FONT_XS, "PATTERN", K_DIM) + 4;
                if (glob.sl_pat[c])
                    itoa_u(glob.sl_pat[c], tx);
                x = cv_text(x, 133, &FONT_XS, glob.sl_pat[c] ? tx : "PIECE'S", K_TEXT) + 10;
                x = cv_text(x, 133, &FONT_XS, "ENERGY", K_DIM) + 4;
                if (glob.sl_mood[c])
                    itoa_u((uint32_t)glob.sl_mood[c] - 1u, tx);
                cv_text(x, 133, &FONT_XS, glob.sl_mood[c] ? tx : "AS IS", K_TEXT);
            } else {   /* the chosen place's parts (black keys 1-5): in, lit; out, struck */
                const int c = ui.slsl % SL_N;
                int32_t x = 4;
                int i;
                x = cv_text(x, 133, &FONT_XS, "PARTS", K_DIM) + 6;
                for (i = 0; i < SL_NPART; i++) {
                    const int out = glob.sl_mute[c] >> i & 1u;
                    const int32_t x1 = cv_text(x, 133, &FONT_XS, SL_PART[i], out ? K_LINE : K_TEXT);
                    if (out)
                        cv_rect(x, 139, x1 - x, 1, K_BASS);
                    x = x1 + 6;
                }
            }
            return;
        }
        for (s = 0; s < SL_N; s++) {
            const int32_t x0 = 4 + (s >> 3) * 117, y0 = 22 + (s & 7) * 11;
            const int e = glob.sl[s], cur = s == (int)(ui.slsl % SL_N), now = glob.sl_on && s == ui.sl_pos;
            const uint16_t fg = now ? K_ACC : e ? K_TEXT : K_LINE;
            if (cur)
                cv_rect(x0, y0, 113, 11, K_PANEL);
            itoa_u((uint32_t)s + 1u, tx);
            cv_text(x0 + 2 + (s < 9 ? 7 : 0), y0, &FONT_XS, tx, K_DIM);
            if (e) {
                cv_text(x0 + 22, y0, &FONT_XS, en_genre_name((e - 1) / NPIECES), fg);
                itoa_u((uint32_t)(e - 1) % NPIECES + 1u, tx);
                if (tx[1] == 0)
                    tx[2] = 0, tx[1] = tx[0], tx[0] = '0';
                cv_text(x0 + 110 - text_w(&FONT_XS, tx), y0, &FONT_XS, tx, fg);
            } else {
                cv_text(x0 + 22, y0, &FONT_XS, "-", K_LINE);
            }
        }
        cv_text(4, 112, &FONT_XS, glob.sl_on ? "AUTO-DJ AND PRESETS: NEXT IN THE LIST" : "TURN GENRE: THE PIECE PLAYING GOES IN", K_DIM);
        return;
    }
    if (kind == SCOPE_PROG) {                    /* the progression: eight slots, each its degree, its chord and its bars; the one playing lit, the one chosen underlined */
        uint8_t tmp[EN_PRG_LEN];
        const uint8_t *b = pg_view(tmp);
        const int mine = b == proj.prg, loop = mine && en_prg_get(b, EN_PRG_PLAY, 1) && en_prog_pos >= 0;
        int s, last = 0, n;
        char tx[24];
        for (s = 0; s < EN_PRG_N; s++)
            if (en_prg_get(b, 3 * s, 3))
                last = s + 1;
        sc_title("PROGRESSION", K_PAD);
        sc_sub(loop ? "YOURS, IN TURN" : mine ? "WALK: THE GENRE'S" : "HEARD LAST", loop ? K_ACC : K_DIM);
        if (LOOK_CONSOLE) {                      /* (0.17.12) the slots as tiles coloured by what the chord does: home (I, iii, vi: the pad's), away (ii, IV: the accent), the pull
                                                  * home (V, vii: the bass's red); its numeral, its name, its bars as pips; the one playing lit, joined to the next; the one chosen
                                                  * with a foot */
            const int sc7 = pg_scale() == 7;
            for (s = 0; s < EN_PRG_N; s++) {
                const int32_t x0 = 2 + (s & 3) * 60, y0 = 20 + (s >> 2) * 46;
                const int d = en_prg_get(b, 3 * s, 3), br = 1 << en_prg_get(b, 24 + 2 * s, 2);
                const int cur = s == (int)(ui.pgsl & 7u), now = loop && s == en_prog_pos, used = d || s < last;
                const uint16_t fc = !d ? K_LINE : !sc7 ? K_PAD : d == 2 || d == 4 ? K_ACC : d == 5 || d == 7 ? K_BASS : K_PAD;
                int i;
                cv_rect(x0, y0, 56, 42, now ? mix(K_PANEL, fc, 6) : d ? mix(K_PANEL, fc, loop ? 2 : 1) : K_PANEL);
                cv_rect(x0, y0, 56, 2, used ? fc : K_LINE);
                itoa_u((uint32_t)s + 1u, tx);
                cv_text(x0 + 3, y0 + 1, &FONT_XS, tx, K_DIM);
                if (d) {
                    cv_text(x0 + 3, y0 + 10, &FONT_B, pg_roman(d, tx), now ? K_TEXT : loop ? fc : mix(fc, K_DIM, 8));
                    cv_text(x0 + 3, y0 + 24, &FONT_XS, pg_name(d, mine ? pk_get(s) : 0, tx), now ? K_TEXT : K_DIM);
                } else if (s < last) {
                    cv_text(x0 + 3, y0 + 14, &FONT_XS, "HOLDS", K_DIM);
                }
                if (used)                        /* its bars */
                    for (i = 0; i < br && i < 8; i++)
                        cv_rect(x0 + 3 + i * 6, y0 + 36, 5, 3, now ? K_TEXT : d ? fc : K_LINE);
                if ((s & 3) < 3 && s + 1 < last)  /* on to the next */
                    cv_rect(x0 + 56, y0 + 20, 4, 2, now ? K_TEXT : K_LINE);
                if (cur)
                    cv_rect(x0, y0 + 42, 56, 2, K_ACC);
            }
            cv_text(4, 112, &FONT_XS, !mine ? "TURN A CHORD: THEY ARE YOURS" : loop ? "EDIT: NEXT SLOT  HOLD: HERE" : "PLAY: LOOP TO HEAR THEM", K_DIM);
            if (sc7)
                cv_text(4, 122, &FONT_XS, "BLACK KEYS: 1-5 CIRCLE, 6-11 CLASSICS", mix(K_DIM, K_PAD, 8));
            {
                const int pd = glob.sl_rsv[2] & 3;
                cv_text(4, 132, &FONT_XS, pd & 1 ? "W9 BASS: PEDAL" : "W9 BASS: FOLLOWS", pd & 1 ? K_ACC : K_DIM);
                cv_text(124, 132, &FONT_XS, pd & 2 ? "W10 PAD: PEDAL" : "W10 PAD: FOLLOWS", pd & 2 ? K_ACC : K_DIM);
            }
            return;
        }
        for (s = 0; s < EN_PRG_N; s++) {
            const int32_t x0 = 4 + (s & 3) * 59, y0 = 22 + (s >> 2) * 44;
            const int d = en_prg_get(b, 3 * s, 3), br = 1 << en_prg_get(b, 24 + 2 * s, 2);
            const int cur = s == (int)(ui.pgsl & 7u), now = loop && s == en_prog_pos;
            const uint16_t fg = !loop ? K_DIM : s < last ? K_TEXT : K_LINE;
            cv_rect(x0, y0, 55, 40, now ? mix(K_BG, K_PAD, 6) : K_PANEL);
            if (cur)
                cv_rect(x0, y0 + 38, 55, 2, K_ACC);
            itoa_u((uint32_t)s + 1u, tx);
            cv_text(x0 + 3, y0 + 2, &FONT_XS, tx, K_DIM);
            if (d || s < last) {
                itoa_u((uint32_t)br, tx);
                n = 0;
                while (tx[n])
                    n++;
                tx[n++] = 'B';
                tx[n] = 0;
                cv_text(x0 + 52 - text_w(&FONT_XS, tx), y0 + 2, &FONT_XS, tx, K_DIM);
            }
            if (d) {
                cv_text(x0 + 3, y0 + 11, &FONT_B, pg_roman(d, tx), now ? K_ACC : fg);
                cv_text(x0 + 3, y0 + 26, &FONT_XS, pg_name(d, mine ? pk_get(s) : 0, tx), fg);
            } else if (s < last) {
                cv_text(x0 + 3, y0 + 17, &FONT_XS, "HOLDS", K_LINE);
            }
        }
        cv_text(4, 107, &FONT_XS, !mine ? "TURN A CHORD: THEY ARE YOURS" : loop ? "EDIT: NEXT SLOT  HOLD: HERE" : "PLAY: LOOP TO HEAR THEM", K_DIM);
        if (pg_scale() == 7)
            cv_text(4, 118, &FONT_XS, "BLACK KEYS: 1-5 CIRCLE, 6-11 CLASSICS", mix(K_DIM, K_PAD, 8));
        {   /* PEDAL: white keys 9 and 10 */
            const int pd = glob.sl_rsv[2] & 3;
            cv_text(4, 129, &FONT_XS, pd & 1 ? "W9 BASS: PEDAL" : "W9 BASS: FOLLOWS", pd & 1 ? K_ACC : K_DIM);
            cv_text(124, 129, &FONT_XS, pd & 2 ? "W10 PAD: PEDAL" : "W10 PAD: FOLLOWS", pd & 2 ? K_ACC : K_DIM);
        }
        return;
    }
    if (kind == SCOPE_BLSTEP) {                  /* the bass steps (0.16.29): a bar a step (its height the note and octave), an accent bright, a glide joined to the step
                                                  * before; the step chosen framed */
        int s;
        char tx[24];
        const int on = proj.par[P_BL_MODE] == EN_BL_STEPS;
        sc_title("BASS STEPS", K_BASS);
        sc_sub(on ? "PLAYING" : "OFF: AN EDIT PLAYS THEM", on ? K_ACC : K_DIM);
        {   /* the roll: a step's height its note and octave, an accent taller, a glide drawn in from the step before (0.17.9) */
            int16_t pit[16];
            uint8_t lv[16];
            uint32_t gl = 0;
            for (s = 0; s < 16; s++) {
                const int v = en_bs[s], kd = EN_BS_KIND(v);
                pit[s] = (int16_t)(kd == EN_BS_HOLD && s ? pit[s - 1] : EN_BS_NOTE(v) + 7 * EN_BS_OCT(v));   /* (a HOLD: the note before, drawn on) */
                lv[s] = (uint8_t)(!kd || kd > EN_BS_HOLD ? 0 : kd == 2 || kd == 4 ? 3 : 2);
                if (kd == 3 || kd == 4 || kd == EN_BS_HOLD)
                    gl |= 1u << s;
            }
            step_roll(22, 86, K_BASS, pit, lv, gl, 16, ui.bstp & 15, on && en_playing ? (int)en_bs_pos : -1, 0, 0);
        }
        {
            static const char *const KN[6] = {"REST", "NOTE", "ACCENT", "GLIDE", "ACC+GLIDE", "HOLD"};
            static const char *const NN[8] = {"ROOT", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH", "OCTAVE"};
            const int v = en_bs[ui.bstp & 15];
            int n = 0;
            const char *q = "STEP ";
            while (*q)
                tx[n++] = *q++;
            itoa_u((uint32_t)(ui.bstp & 15u) + 1u, tx + n);
            cv_text(4, 112, &FONT_XS, tx, K_TEXT);
            cv_text(48, 112, &FONT_XS, KN[EN_BS_KIND(v) > EN_BS_HOLD ? 0 : EN_BS_KIND(v)], EN_BS_KIND(v) ? K_ACC : K_DIM);
            cv_text(132, 112, &FONT_XS, EN_BS_KIND(v) ? NN[EN_BS_NOTE(v)] : "", K_DIM);
        }
        return;
    }
    if (kind == SCOPE_CMSTEP) {                  /* the pluck steps: the line, a bar a step (its height the rung), the accent bright, a soft one dim; the step chosen framed */
        int s;
        char tx[24];
        sc_title("PLUCK STEPS", IMP_TEAL);
        sc_sub(proj.par[P_CM_LOCK] ? "FROZEN" : "FREE: DEJA VU'S LOOP", proj.par[P_CM_LOCK] ? K_ACC : K_DIM);
        {   /* the roll: a step's height its rung, an accent taller, a soft one dim (0.17.9) */
            int16_t pit[16];
            uint8_t lv[16];
            for (s = 0; s < 16; s++) {
                pit[s] = (int16_t)en_cm_rung[s];
                lv[s] = (uint8_t)(en_cm_lvl[s] & 3);
            }
            step_roll(22, 86, IMP_TEAL, pit, lv, 0, 16, ui.pstp & 15, en_playing ? (int)en_cm_pos : -1, 0, 0);
        }
        {
            static const char *const LN[4] = {"REST", "SOFT", "NOTE", "ACCENT"};
            int n = 0;
            const char *q = "STEP ";
            while (*q)
                tx[n++] = *q++;
            itoa_u((uint32_t)(ui.pstp & 15u) + 1u, tx + n);
            cv_text(4, 112, &FONT_XS, tx, K_TEXT);
            cv_text(48, 112, &FONT_XS, LN[en_cm_lvl[ui.pstp & 15] & 3], en_cm_lvl[ui.pstp & 15] ? K_ACC : K_DIM);
            n = 0;
            q = "NOTE ";
            while (*q)
                tx[n++] = *q++;
            itoa_u((uint32_t)en_cm_rung[ui.pstp & 15] + 1u, tx + n);
            cv_text(120, 112, &FONT_XS, tx, K_DIM);
        }
        return;
    }
    if (kind == SCOPE_MIX) {                     /* the mixer: a strip a layer, its level set and its level now, the output last */
        static const char *const NAME[6] = {"PAD", "BASS", "ACID", "PLUCK", "DRUMS", "OUT"};   /* (0.17.39: the strips' names, as home's) */
        static const uint8_t TRK[6] = {0x40, 0x10, 0x20, 0x80, 0x0F, 0};
        const uint8_t lv[5] = {(uint8_t)proj.par[P_PAD], (uint8_t)proj.par[P_BASS], (uint8_t)proj.par[P_ACID], (uint8_t)proj.par[P_CM_LVL], (uint8_t)proj.par[P_PULSE]};
        float live[6], dg = 0.0f;
        const uint16_t C[6] = {K_PAD, K_BASS, K_ACID, IMP_TEAL, RGB(255, 160, 60), K_ACC};
        int k, d;
        for (d = 0; d < EN_NLANE; d++)
            dg = fm_maxf(dg, en_drum_glow[d]);
        live[0] = en_pad_level, live[1] = en_bass_level, live[2] = en_acid_level, live[3] = en_cm_level, live[4] = dg, live[5] = en_out_peak;
        sc_title("MIXER", K_PAD);
        sc_sub("SET LEVEL, AND WHAT SOUNDS", K_DIM);
        if (LOOK_CONSOLE) {                      /* (0.17.10) a desk: a strip a layer, its fader with a cap, its meter in segments, its place under it;
                                                  * the chosen one lit, M (red) or S (amber) below */
            for (k = 0; k < 6; k++) {
                const int32_t sx = 4 + k * 39, sw = 36, top = 20, fy = 38, fh = 60;
                const int muted = k < 5 && (ui.trk_m & TRK[k]) == TRK[k], solo = k < 5 && (ui.trk_s & TRK[k]);
                const uint16_t c = C[k];
                const int n = muted ? 0 : (int)(fm_clampf(live[k], 0.0f, 1.0f) * 10.0f + 0.5f);
                int i;
                char tx[8];
                cv_panel_g(sx, top, sw, 98, k < 5 && k == ui.mix_trk ? mix(K_PANEL, c, 4) : mix(K_PANEL, K_LINE, 3), k < 5 && k == ui.mix_trk ? mix(K_PANEL, c, 1) : K_PANEL, 0);
                cv_rect(sx, top, sw, 2, muted ? K_LINE : c);
                cv_text_fit(sx + sw / 2, top + 1, &FONT_XS, NAME[k], muted ? K_DIM : c, sw - 2, 1);
                for (i = 0; i < 10; i++)         /* the meter: what sounds */
                    cv_rect(sx + 23, fy + fh - 5 - i * 6, 8, 5, i < n ? (i >= 9 ? K_BASS : i >= 7 ? K_ACC : c) : K_LINE);
                if (k < 5) {                     /* the fader: what it is set to */
                    const int32_t cy = fy + fh - 1 - (int32_t)lv[k] * (fh - 2) / 100;
                    cv_panel(sx + 9, fy - 1, 4, fh + 2, K_BG, 1);   /* (a slot in the desk) */
                    cv_rect(sx + 10, cy, 2, fy + fh - cy, muted ? K_DIM : c);
                    cv_rect(sx + 4, cy - 2, 14, 5, muted ? K_DIM : K_TEXT);
                    itoa_u((uint32_t)lv[k], tx);
                    text_c(sx + sw / 2, fy + fh + 2, &FONT_XS, tx, muted ? K_DIM : K_TEXT);
                    {                            /* its place, left to right */
                        const int32_t px = sx + 4 + (int32_t)proj.par[P_PAN + k] * (sw - 10) / 100;
                        cv_rect(sx + 4, top + 93, sw - 8, 1, K_LINE);
                        cv_rect(px, top + 91, 2, 5, proj.par[P_PAN + k] == 50 ? K_DIM : K_ACC);
                    }
                }
                if (muted || solo) {
                    cv_rect(sx + sw / 2 - 7, 121, 14, 12, muted ? K_BASS : K_ACC);
                    text_c(sx + sw / 2, 119, &FONT_XS, muted ? "M" : "S", K_BG);
                }
            }
            return;
        }
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
        sc_title("MASTER", col);
        sc_sub("COMPRESSOR, LIMITER", K_DIM);
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
        sc_foot("IN ACROSS, OUT UP: KNEE, CEILING", K_DIM);
        return;
    }
    if (kind == SCOPE_CMEL) {                    /* the counter-melody: its decay, and the line it plays over a bar (the chord's tones, low to high) */
        static const uint8_t RATE[4] = {4, 3, 2, 1};
        const int st = proj.par[P_CM_STYLE], ri = proj.par[P_CM_RATE] & 3, n = 6;
        const float T = 0.1f * fm_exp2f((float)proj.par[P_CM_DEC] * 0.045f);
        int s, k;
        col = IMP_TEAL;
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1);
            sc_y[x] = (uint8_t)(scope_expo(u * 1.2f / T, 10.0f) * (float)(SC_H - 8) * 0.45f);
        }
        sc_title("PLUCK", col);
        sc_sub(EN_CM_STYLE[(unsigned)st < EN_NCMSTYLE ? st : 0], K_DIM);
        sc_foot("A BAR: THE CHORD'S TONES, AND THE RING", K_DIM);
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
            title = "BASS ENVELOPE", sub = "ATTACK  DECAY  AMOUNT";
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
        sc_title(title, col);
        sc_foot(sub, K_DIM);
        scope_trace(col);
        scope_dot(tk, col);
        return;
    }
    if (kind == SCOPE_PLFO || kind == SCOPE_DLFO) {
        const int pl = kind == SCOPE_PLFO, shape = proj.par[pl ? P_PL_SHAPE : P_DL_SHAPE],
                  rate = proj.par[pl ? P_PL_RATE : P_DL_RATE], dep = proj.par[pl ? P_PL_DEPTH : P_DL_DEPTH];
        static const float BEATS[16] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f,
                                        2.0f / 3.0f, 0.75f, 1.5f, 3.0f};   /* (the engine's LFO_RATE_BEATS) */
        const float bt = BEATS[(unsigned)rate < 16u ? rate : 0], win = bt <= 1.0f ? 4.0f : bt <= 16.0f ? 16.0f : 2.0f * bt;   /* the screen: a bar, 4 bars, or 2 cycles */
        const float cyc = win / bt, amp = 0.06f + 0.94f * (float)dep * 0.01f;
        col = pl ? K_PAD : K_BASS;
        title = pl ? "PAD LFO" : "BASS LFO";
        sub = LFO_SHAPE_NAME_UI[(unsigned)shape < 6u ? shape : 0];
        for (x = 0; x < SC_W; x++) {
            const float u = (float)x / (float)(SC_W - 1), w = lfo_wave(shape, u * cyc + tk);
            sc_y[x] = (uint8_t)((0.5f + 0.5f * w * amp) * (float)(SC_H - 8));
        }
        sc_title(title, col);
        sc_sub(sub, K_DIM);
        scope_trace(col);
        cv_rect(SC_X, SC_Y + SC_H / 2, SC_W, 1, K_LINE);
        return;
    }
    if (kind == SCOPE_DFILT || kind == SCOPE_AFILT) {
        const int ac = kind == SCOPE_AFILT, cut = proj.par[ac ? P_AC_CUT : P_DR_CUT], res = proj.par[ac ? P_AC_RES : P_DR_RES],
                  env = ac ? proj.par[P_AC_ENV] : 0;
        int pass;
        /* (0.17.19) drawn as its FILTERS type sounds: the high-pass's corner where the engine puts it (FT_FC: a quarter of the bass's, half the acid's) */
        static const char *const FTN[6] = {"LOW", "HIGH", "BAND", "NOTCH", "PEAK", "303"};
        const int ft = ui.fxs[ac ? EN_FXS_FT_ACID : EN_FXS_FT_BASS], t = (unsigned)ft < (ac ? 6u : 5u) ? ft : 0;
        const float fcm = t == 1 ? (ac ? 0.5f : 0.25f) : 1.0f;
        char tl[24];
        int n = 0;
        const char *a = ac ? "ACID FILTER: " : "BASS FILTER: ", *b = FTN[t];
        while (*a)
            tl[n++] = *a++;
        while (*b)
            tl[n++] = *b++;
        tl[n] = 0;
        col = ac ? K_ACID : K_BASS;
        sub = ac ? "CUTOFF  RESO  ENV MOD (DIM: PEAK)" : "CUTOFF  RESO";
        sc_title(tl, col);
        sc_foot(sub, K_DIM);
        for (pass = 0; pass < (ac && env > 4 ? 2 : 1); pass++) {
            const float oct = 8.4f * (float)cut * 0.01f + (pass ? 4.0f * (float)env * 0.01f : 0.0f), fc = 60.0f * fm_exp2f(oct) * fcm,
                        q = 0.6f + (float)res * 0.09f;
            filt_curve(t, fc, q, 1, SC_H - 6);
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
        sc_title("SPACE", col);
        sc_foot("THE HIT, ITS ECHOES, THE REVERB'S TAIL", K_DIM);
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

static void draw_horizon(void)
{
    static const uint8_t HY = 104;               /* the horizon's row (the sigil's centre is at CY 74; the phrase ring reaches 131) */
    const uint16_t top = K_BG, bot = RGB(54, 8, 50), grid = RGB(255, 47, 142), haze = RGB(255, 47, 110);
    const int play = en_playing;
    const uint32_t now = plat_ms(), dage = now - sg.drop_t;
    const int flash = play && sg.drop_t && dage < 450u;
    int i, y;
    for (i = 0; i < 5; i++)                      /* the sky, five bands */
        cv_rect(0, i * HY / 5, 240, HY / 5 + 1, mix(top, bot, i * 4));
    {   /* the stars: the same forty each frame */
        uint32_t s = 0x9E3779B9u;
        for (i = 0; i < 40; i++) {
            s = s * 1664525u + 1013904223u;
            cv_pset((int32_t)(s >> 24) * 240 / 256, (int32_t)((s >> 16) & 255u) * (HY - 8) / 256, mix(top, K_TEXT, 4 + (int)((s >> 8) & 7u)));
        }
    }
    {   /* the sun: a banded disc sitting on the horizon, low in a breakdown, rising once through a build (its countdown), white on the drop */
        const int left = en_sec_bars > 8u ? 8 : (int)en_sec_bars;
        const int lift16 = !play ? 9 : en_sec == EN_SEC_BREAK ? 4 : en_sec == EN_SEC_BUILD ? 16 - left * 6 / 8 : en_sec == EN_SEC_GROOVE && dage < 4000u ? 16 : 10;
        const int r = 30, cy = HY + r * (16 - lift16) * 9 / 160;
        const uint16_t band[6] = {RGB(255, 225, 74), RGB(255, 190, 70), RGB(255, 140, 80), RGB(255, 100, 120), RGB(255, 70, 140), RGB(255, 47, 142)};
        for (y = cy - r; y < HY; y++) {
            const int d = y - cy, hw = (int)fm_sqrtf((float)(r * r - d * d)), k = (y - (cy - r)) * 6 / (2 * r);
            if (y >= cy + 4 && ((y - cy) % 5) < 1 + (y - cy) / 10)
                continue;                        /* the gaps, wider lower down */
            cv_rect(CX - hw, y, 2 * hw + 1, 1, flash ? K_TEXT : band[k > 5 ? 5 : k]);
        }
    }
    {   /* the floor: lines to the vanishing point, the near ones further apart, scrolling once a beat */
        const int tempo = proj.par[P_TEMPO] > 0 ? proj.par[P_TEMPO] : 120, beat = 60000 / tempo;
        const int ph = play ? (int)((now % (uint32_t)beat) * 64u / (uint32_t)beat) : 0;   /* 0..63 through the beat */
        cv_rect(0, HY, 240, MAIN_H - HY, mix(top, bot, 3));
        cv_rect(0, HY, 240, 1, mix(top, haze, 10));
        for (i = 0; i < 7; i++) {
            const int p = (i * 64 + ph), yy = HY + (MAIN_H - HY) * p * p / (448 * 448);
            cv_rect(0, yy, 240, 1, mix(top, grid, 4 + p * 8 / 448));
        }
        for (i = -9; i <= 9; i++)
            cv_line(CX + i * 5, HY, CX + i * 34, MAIN_H - 1, mix(top, grid, 3));
    }
}

static uint32_t chord_pcs(void)                 /* the chord playing: a bit a pitch class (0 C) */
{
    uint32_t m = 1u << (en_chord_root % 12u), i;
    for (i = 0; i < EN_NPAD; i++)
        m |= 1u << (en_chord[i] % 12u);
    return m;
}
/* ZOOM (the SCREEN page, 0.16.1, for playing at a distance or outdoors): a knob turned fills the main area with its name and its value, twice the
 * largest type, and a bar, for as long as the knob strip lights it (1.2 s) */
/* text at twice the size: a pixel two by two, each block at its pixel's coverage (0.17.40: the font's soft edge pixels stay soft, the
 * strokes stay sharp; a 9:3:3:1 blend of neighbours read fuzzy on the unit) */
static void cv_text2(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    for (; *s; s++) {
        const uint32_t gi = glyph(f, (uint8_t)*s), w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi] + 2;
        const int32_t yg = y + 2 * (int32_t)gd[-2];
        uint32_t gx, gy;
        for (gy = 0; gy < gd[-1]; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t a = gd[gy * bpr + gx / 2u];
                int32_t px, py;
                a = (gx & 1u) ? (a & 15u) : (a >> 4);
                if (!a)
                    continue;
                px = x + 2 * ((int32_t)gx - f->pad), py = yg + 2 * (int32_t)gy;
                if (a == 15u) {
                    cv_rect(px, py, 2, 2, c);
                } else {
                    a = a * 16u * 34953u >> 19;          /* (0..15 to 0..16) */
                    px_blend(px, py, c, a), px_blend(px + 1, py, c, a), px_blend(px, py + 1, c, a), px_blend(px + 1, py + 1, c, a);
                }
            }
        x += 2 * f->adv[gi];
    }
}
static uint16_t view_color(void);
static int zoom_knob(void)                       /* the knob to zoom on (0 .. 3), -1: none */
{
    return glob.sl_rsv[0] && ui.touched >= 0 && ui.touched < 4 && plat_ms() < ui.touch_until && view_knob(ui.touched) != K_NONE ? ui.touched : -1;
}
static void draw_zoom(int i)
{
    const int k = view_knob(i);
    int lo, hi, v;
    char t[16], vt[16];
    knob_range(k, &lo, &hi);
    v = knob_get(k);
    caps(t, view_label(i), sizeof t);
    text_c(120, 6, &FONT_M, t, K_DIM);
    knob_text(k, t);
    caps(vt, t, sizeof vt);
    if (2 * text_w(&FONT_L, vt) <= 236)
        cv_text2(120 - text_w(&FONT_L, vt), 38, &FONT_L, vt, K_ACC);
    else if (text_w(&FONT_L, vt) <= 236)
        text_c(120, 56, &FONT_L, vt, K_ACC);
    else
        text_c(120, 60, &FONT_M, vt, K_ACC);
    cv_rect(20, 124, 200, 6, K_LINE);
    if (hi > lo) {
        if (lo < 0) {                            /* bipolar: from the middle */
            const int32_t m = (int32_t)(v - lo) * 200 / (hi - lo);
            cv_rect(20 + (m < 100 ? m : 100), 124, m < 100 ? 100 - m : m - 100, 6, view_color());
        } else {
            cv_rect(20, 124, (int32_t)(v - lo) * 200 / (hi - lo), 6, view_color());
        }
    }
}
/* ------------------------------------------------------------ the looks --- */
/* 0.17.2, A TEST of two layouts of our own (the overhaul's options A and C, to try on the unit; Global > Theme, the last two):
 * CONSOLE: five channel strips, a voice each: its name, the bar's coming steps falling to a playhead line, a meter, MUTE when it is out;
 *   the page's voice lit. The chord and where it goes above them; the drums as a tracker (the steps down, the lanes across); faders.
 * RITUAL: the whole screen one circle: the bar's 16 steps on the rim, a voice an orbit with its notes as beads, the chord's star and the
 *   eye in the middle; the page's four knobs in the square's corners, each a quarter arc. Drawn in two halves (a canvas is 144 rows at
 *   most); the pages that draw a picture of their own keep the bands. */
enum { LK_PAD, LK_BASS, LK_ACID, LK_PLUCK, LK_DRUMS };
static const char *const LK_NAME[5] = {"PAD", "BASS", "ACID", "PLUCK", "DRUMS"};

static uint16_t lk_col(int v) { return v == LK_PAD ? K_PAD : v == LK_BASS ? K_BASS : v == LK_ACID ? K_ACID : v == LK_PLUCK ? IMP_TEAL : K_PULSE; }
static int lk_quiet(int v)                      /* the voice is out: muted, or another soloed */
{
    static const uint8_t TR[4] = {EN_TR_PAD, EN_TR_DRONE, EN_TR_ACID, EN_TR_PLUCK};
    const uint32_t bits = v == LK_DRUMS ? 15u : 1u << TR[v];
    if (ui.trk_s)
        return !(ui.trk_s & bits);
    return (ui.trk_m & bits) == bits;
}
static float lk_level(int v)
{
    int d;
    float m = 0.0f;
    if (v == LK_PAD)
        return en_pad_level;
    if (v == LK_BASS)
        return en_bass_level;
    if (v == LK_ACID)
        return en_acid_level;
    if (v == LK_PLUCK)
        return en_cm_level;
    for (d = 0; d < EN_NDRUM; d++)
        m = fm_maxf(m, en_drum_glow[d]);
    return m;
}
static int lk_voice(void)                       /* the page's voice (LK_*), -1 none */
{
    const int vo = voice_of(ui.view);
    if (vo == EN_V_PAD)
        return LK_PAD;
    if (vo == EN_V_DRONE)
        return LK_BASS;
    if (vo == EN_V_ACID)
        return LK_ACID;
    if (ui.view >= V_CMEL && ui.view <= V_CMSTEP)
        return LK_PLUCK;
    if ((ui.view >= V_DRUMS && ui.view <= V_PHRASE) || ui.view == V_SEQ)
        return LK_DRUMS;
    return -1;
}
/* the note voice v plays d steps from now: -1 a rest, else its height 0 .. 23 (the bass, the acid: scale steps; the pluck: its rung); *acc an accent */
static int lk_note(int v, int d, int *acc)
{
    *acc = 0;
    if (v == LK_BASS) {
        const int s = ((en_bs_pos >= 0 ? en_bs_pos : (int)en_drum_pos) + d) & 15;
        const uint8_t b = en_bs_pos >= 0 ? en_bs[s] : en_bs_hist[s];
        if (!EN_BS_KIND(b) || EN_BS_KIND(b) == EN_BS_HOLD)
            return -1;
        *acc = EN_BS_KIND(b) == 2 || EN_BS_KIND(b) == 4;
        return EN_BS_NOTE(b) + EN_BS_OCT(b) * 7;
    }
    if (v == LK_ACID) {
        const int len = en_acid_len && en_acid_len < 16 ? en_acid_len : 16, s = ((int)en_acid_pos + d) % len;
        if (!(en_acid_gate >> s & 1u))
            return -1;
        *acc = (int)(en_acid_acc >> s & 1u);
        return en_acid_off[s] + en_acid_oct[s] * 7;
    }
    if (v == LK_PLUCK) {
        const int s = ((int)en_cm_pos + d) & 15;
        if (!en_cm_lvl[s])
            return -1;
        *acc = en_cm_lvl[s] >= 3;
        return en_cm_rung[s];
    }
    return -1;
}
static void lk_chord(char *nm)                  /* the chord playing, named */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    en_chord_name(pcs, EN_NPAD + 1, nm);
}
static void lk_sound(int v, char *u)            /* the voice's sound, short, or MUTE */
{
    char t[16];
    if (lk_quiet(v)) {
        caps(u, "MUTE", 8);
        return;
    }
    if (v == LK_PAD || v == LK_BASS || v == LK_ACID)
        en_param_text(SOUND_PAR[v == LK_PAD ? 0 : v == LK_BASS ? 1 : 2], proj.par[SOUND_PAR[v == LK_PAD ? 0 : v == LK_BASS ? 1 : 2]], t);
    else if (v == LK_PLUCK) {
        const char *s = EN_CM_SOUND[(unsigned)proj.par[P_CM_SND] < EN_NCMSND ? proj.par[P_CM_SND] : 0];
        int j = 0;
        while (s[j] && j < 15)
            t[j] = s[j], j++;
        t[j] = 0;
    } else {
        const char *s = EN_KIT_NAME[(unsigned)proj.par[P_KIT_KICK] < EN_NKITS ? proj.par[P_KIT_KICK] : 0];
        int j = 0;
        while (s[j] && j < 15)
            t[j] = s[j], j++;
        t[j] = 0;
    }
    caps(u, t, 16);                              /* (fitted where it is drawn: 0.17.39, MARIMBA not MARIMB) */
}

/* CONSOLE: the main band's top line: the chord, where it goes (and why), the section, the way to the next chord */
static void console_top(void)
{
    char nm[16];
    int32_t x;
    lk_chord(nm);
    x = cv_text(6, 1, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
    if (en_frozen)
        cv_text(x + 8, 5, &FONT_XS, "HELD", K_FROST);
    else if (en_walk_next >= 0) {                /* where it goes: an arrow, the next root; why, when it leans or borrows */
        int j;
        cv_rect(x + 6, 10, 8, 2, K_DIM);
        for (j = 0; j < 4; j++)
            cv_rect(x + 14 + j, 7 + j, 1, 8 - 2 * j, K_DIM);
        x = cv_text(x + 21, 3, &FONT_S, EN_NOTE_NAME[en_walk_next_root % 12], K_DIM);
        if (en_chord_alt)
            cv_text(x + 6, 5, &FONT_XS, en_chord_alt == 1 ? "V OF NEXT" : "BORROWED", K_ACC);
    }
    {   /* the section, and the way to the next chord */
        const char *sn = en_sec == EN_SEC_BREAK ? "BREAK" : en_sec == EN_SEC_BUILD ? "BUILD" : "GROOVE";
        const int32_t w = 34, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_text(236 - text_w(&FONT_XS, sn), -1, &FONT_XS, sn, en_sec == EN_SEC_GROOVE ? K_DIM : K_ACC);
        cv_rect(236 - w, 15, w, 3, K_LINE);
        if (f > 0 && !en_frozen)
            cv_rect(236 - w, 15, f, 3, K_ACC);
    }
    cv_rect(0, 21, 240, 1, K_LINE);
}

/* CONSOLE: the strips (home, Global: the five voices side by side) */
static void console_home(void)
{
    const int cur = lk_voice();
    char u[16];
    int v, d, k;
    console_top();
    for (v = 0; v < 5; v++) {
        const int32_t sx = 2 + v * 47, sw = v == 4 ? 46 : 45;
        const int q = lk_quiet(v), lit = v == cur;
        const uint16_t col = lk_col(v), c = q ? mix(K_PANEL, col, 5) : col;
        cv_panel_g(sx, 24, sw, 120, lit ? mix(K_PANEL, col, 4) : mix(K_PANEL, K_LINE, 3), lit ? mix(K_PANEL, col, 1) : K_PANEL, 0);   /* (0.17.40: lit from above, falling off) */
        if (lit)
            cv_rect(sx, 24, sw, 2, col);
        cv_text(sx + 3, 25, &FONT_XS, LK_NAME[v], c);
        cv_rect(sx + 2, 118, sw - 10, 1, q ? K_LINE : K_DIM);   /* the playhead: now */
        if (v == LK_PAD) {                       /* the pad holds its chord: a column a voice, by pitch */
            const int g = q ? 4 : 6 + q16(en_pad_level) / 2;
            int lo = 127;
            for (k = 0; k < EN_NPAD; k++)
                lo = en_pad_note[k] < lo ? en_pad_note[k] : lo;
            for (k = 0; k < EN_NPAD; k++) {      /* two octaves across the strip, from the lowest voice */
                int n = (int)en_pad_note[k] - lo;
                n = n > 24 ? 24 : n;
                cv_rect(sx + 3 + n * 30 / 24, 41, 3, 76, mix(K_PANEL, col, g));
            }
        } else
            for (d = 15; d >= 0; d--) {          /* the bar's coming steps, falling: now on the line, the farthest at the top */
                const int32_t y = 114 - d * 5;
                const uint16_t nc = d == 0 && en_playing && !q ? K_TEXT : c;
                if (v == LK_DRUMS) {
                    int l;
                    for (l = 0; l < EN_NLANE && l < 6; l++)
                        if ((d == 0 && en_playing ? en_played[l] : lane_plan(l)) >> (((int)en_drum_pos + d) & 15) & 1u)
                            cv_rect(sx + 3 + l * 6, y, 4, 3, nc);
                } else {
                    int acc, n = lk_note(v, d, &acc);
                    if (n >= 0) {
                        n = n > 23 ? 23 : n;
                        cv_rect(sx + 3 + n * 28 / 23, y, acc ? 7 : 5, 3, nc);
                    }
                }
            }
        {   /* the meter: twelve segments */
            const int n = q ? 0 : (int)(fm_clampf(lk_level(v), 0.0f, 1.0f) * 12.0f + 0.5f);
            for (k = 0; k < 12; k++)
                cv_rect(sx + sw - 6, 112 - k * 6, 4, 4, k < n ? (k >= 10 ? K_BASS : k >= 8 ? K_ACC : K_ACID) : K_LINE);
        }
        lk_sound(v, u);
        cv_text_fit(sx + 3, 124, &FONT_XS, u, q ? K_BASS : K_DIM, sw - 3, 0);
    }
}

/* CONSOLE: Global (0.17.6): the scale on an octave of keys, the root marked; the keys here put notes in and out of it */
static void console_global(void)
{
    static const uint8_t WPC[7] = {0, 2, 4, 5, 7, 9, 11}, BPC[5] = {1, 3, 6, 8, 10}, BAFT[5] = {0, 1, 3, 4, 5};
    const uint32_t pool = pool_abs();
    const int root = sound_root();
    char t[24];
    int w;
    int32_t x;
    console_top();
    for (w = 0; w < 7; w++) {                    /* the white keys: lit in the scale, the root bright */
        const int pc = WPC[w], in = (int)(pool >> pc & 1u);
        const uint16_t c = pc == root ? K_TEXT : in ? K_ACC : K_PANEL;
        const int32_t kx = 8 + w * 32;
        cv_rect(kx, 26, 30, 78, c);
        text_c(kx + 15, 86, &FONT_XS, EN_NOTE_NAME[pc], in ? K_BG : K_DIM);
    }
    for (w = 0; w < 5; w++) {                    /* the black keys over them */
        const int pc = BPC[w], in = (int)(pool >> pc & 1u);
        const int32_t kx = 8 + (BAFT[w] + 1) * 32 - 11;
        cv_rect(kx - 1, 26, 22, 47, K_BG);
        cv_rect(kx, 26, 20, 46, pc == root ? K_TEXT : in ? K_ACC : mix(K_BG, K_PANEL, 8));
        text_c(kx + 10, 54, &FONT_XS, EN_NOTE_NAME[pc], in ? K_BG : K_DIM);
    }
    caps(t, EN_NOTE_NAME[root], 4);
    x = cv_text(8, 108, &FONT_B, t, K_TEXT);
    caps(t, mode_name(), sizeof t);
    cv_text(x + 6, 108, &FONT_B, t, K_ACC);
    cv_text(8, 128, &FONT_XS, "KEYS: NOTES IN / OUT   ALGO: MODE", K_DIM);
}

/* CONSOLE: the Code page (0.17.11): the nine letters on tiles (the place lit, the check letter apart), the code's bits as a strip
 * (the track's fingerprint, coloured by what each part of it holds), what it starts, and what PLAY does */
static void console_code(void)
{
    uint8_t g[CODE_N];
    tcode_t t;
    char b[28], n[8];
    int i, ok;
    int32_t x;
    if (code_typed)
        for (i = 0; i < CODE_N; i++)
            g[i] = code_in[i];
    else
        code_now(g);
    ok = !code_unpack(g, &t);
    console_top();
    for (i = 0; i < CODE_N; i++) {               /* the letters */
        const int32_t tx = 8 + i * 24 + (i >= 4 ? 8 : 0);
        const int cur = i == (int)code_pos;
        const uint16_t bg = cur ? mix(K_PANEL, K_ACC, 5) : i == CODE_N - 1 ? mix(K_BG, K_PANEL, 8) : K_PANEL;
        char c[2] = {CODE_ABC[g[i] & 31], 0};
        cv_rect(tx, 26, 21, 30, bg);
        if (cur)
            cv_rect(tx, 54, 21, 2, K_ACC);
        text_c(tx + 10, 31, &FONT_B, c, !ok ? K_BASS : code_typed ? K_ACC : K_TEXT);
    }
    cv_rect(105, 40, 6, 2, K_DIM);               /* the dash */
    if (ok) {                                    /* the fingerprint: 40 bits, each part its colour (genre, piece, seed, key, mode, tempo) */
        static const uint8_t AT[7] = {0, 5, 9, 25, 29, 33, 40};
        uint64_t v = (uint64_t)t.genre | (uint64_t)t.piece << 5 | (uint64_t)t.seed << 9 | (uint64_t)t.root << 25 |
                     (uint64_t)t.mode << 29 | (uint64_t)(t.tempo - 70) << 33;
        const uint16_t PC[6] = {K_PAD, K_ACC, K_TEXT, K_BASS, K_ACC, K_DIM};
        int f = 0;
        for (i = 0; i < 40; i++) {
            while (i >= AT[f + 1])
                f++;
            cv_rect(8 + i * 56 / 10, 60, 4, v >> i & 1u ? 10 : 3, v >> i & 1u ? PC[f] : K_LINE);
        }
        caps(b, en_genre_name(t.genre), 18);
        x = cv_text(8, 76, &FONT_B, b, K_PAD);
        itoa_u((uint32_t)t.piece + 1u, n);
        b[0] = ' ', b[1] = n[1] ? n[0] : '0', b[2] = n[1] ? n[1] : n[0], b[3] = 0;
        cv_text(x + 4, 76, &FONT_B, b, K_TEXT);
        caps(b, EN_NOTE_NAME[t.root % 12], 4);
        x = cv_text(8, 96, &FONT_S, b, K_TEXT);
        caps(b, EN_MODE_NAME[t.mode % EN_NMODES], 14);
        x = cv_text(x + 4, 96, &FONT_S, b, K_ACC);
        itoa_u(t.tempo, n);
        x = cv_text(x + 10, 96, &FONT_S, n, K_TEXT);
        cv_text(x + 3, 96, &FONT_S, "BPM", K_DIM);
    } else {
        cv_text(8, 76, &FONT_B, "NOT A CODE", K_BASS);
        cv_text(8, 96, &FONT_S, "A LETTER IS WRONG", K_DIM);
    }
    if (code_typed)
        cv_text(8, 114, &FONT_XS, ok ? "PLAY: START THIS TRACK" : "K1 THE PLACE, K2 THE LETTER", ok ? K_ACC : K_DIM);
    else if (!code_exact())
        cv_text(8, 114, &FONT_XS, "EDITED: THE CODE GIVES THE RECIPE", K_ACC);
    else
        cv_text(8, 114, &FONT_XS, en_playing ? "PLAYING: SHARE IT, PLAY IT AGAIN" : "PLAY: THIS TRACK AGAIN", K_DIM);
    cv_text(8, 128, &FONT_XS, "K1 PLACE  K2 LETTER  K3 ROLL", K_DIM);
}

/* CONSOLE: a folded strip (the other voices beside a wide one): its letter, its meter, red when it is out */
static void console_sliver(int v, int32_t x)
{
    const int q = lk_quiet(v);
    const uint16_t col = lk_col(v);
    char l[2] = {LK_NAME[v][0], 0};
    int k;
    const int n = q ? 0 : (int)(fm_clampf(lk_level(v), 0.0f, 1.0f) * 10.0f + 0.5f);
    cv_panel(x, 24, 11, 120, q ? mix(K_PANEL, K_BASS, 4) : K_PANEL, 0);
    cv_rect(x, 24, 11, 2, q ? K_LINE : col);
    cv_text(x + 6 - text_w(&FONT_XS, l) / 2, 25, &FONT_XS, l, q ? K_BASS : col);
    for (k = 0; k < 10; k++)
        cv_rect(x + 3, 134 - k * 9, 5, 6, k < n ? (k >= 8 ? K_BASS : k >= 6 ? K_ACC : col) : K_LINE);
}

/* CONSOLE: the bar of a line voice as a roll: sixteen steps across, the note's height up, accents bigger, the acid's slides joined, now lit */
static void console_roll(int v, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    const int32_t cw = w / 16;
    const int pos = v == LK_ACID ? (int)en_acid_pos : v == LK_PLUCK ? (int)en_cm_pos : en_bs_pos >= 0 ? (int)en_bs_pos : (int)en_drum_pos;
    const uint16_t col = lk_col(v);
    int s, px = -1, py = 0, any = 0;
    for (s = 0; s < 16; s++) {
        const int32_t x = x0 + s * cw;
        int acc;
        cv_rect(x, y0, 1, h, s % 4 ? mix(K_PANEL, K_LINE, 8) : K_LINE);
        any |= lk_note(v, s, &acc) >= 0;
    }
    if (!any) {                                  /* nothing on the steps: the bass a held drone (a bar across, breathing with it); the others quiet */
        if (v == LK_BASS) {
            cv_rect(x0, y0 + h - 14, w, 6, mix(K_PANEL, col, 5 + q16(fm_clampf(en_bass_level, 0.0f, 1.0f)) * 10 / 16));
            cv_text(x0 + 2, y0 + h - 32, &FONT_XS, "HELD: THE DRONE", K_DIM);
        } else
            cv_text(x0 + 2, y0 + h / 2 - 8, &FONT_XS, "NO NOTES THIS BAR", K_DIM);
        return;
    }
    if (en_playing) {                            /* the playhead's column, its edges lit (0.17.40: the focus cue) */
        const int32_t px0 = x0 + (pos & 15) * cw;
        cv_rect(px0, y0, cw, h, mix(K_PANEL, K_ACC, 3));
        cv_rect(px0, y0, 1, h, mix(K_PANEL, K_ACC, 8));
        cv_rect(px0 + cw - 1, y0, 1, h, mix(K_PANEL, K_ACC, 8));
    }
    for (s = 0; s < 16; s++) {                   /* step s of the bar: d steps from now */
        const int d = (s - (pos & 15)) & 15;
        int acc, n = lk_note(v, d, &acc);
        if (n < 0) {
            px = -1;
            continue;
        }
        n = n > 23 ? 23 : n;
        {
            const int32_t x = x0 + s * cw + 1, y = y0 + h - 6 - n * (h - 8) / 23;
            const uint16_t c = d == 0 && en_playing ? K_TEXT : acc ? col : mix(K_PANEL, col, 11);
            if (v == LK_ACID && px >= 0 && (en_acid_slide >> ((s + 15) & 15) & 1u))
                aa_seg((float)px, (float)py + 2.0f, (float)x, (float)y + 2.0f, mix(K_PANEL, col, 9), 1.0f);   /* a slide into it (smooth: 0.17.39) */
            cv_rect(x, y - (acc ? 1 : 0), cw - 2, acc ? 6 : 4, c);
            px = x + cw - 3, py = y;
        }
    }
}

/* CONSOLE: a voice's own pages: its strip opened wide (a picture of what it plays), the other four folded at the side */
static void console_voice(int v)
{
    static const uint8_t ORDER[5] = {LK_PAD, LK_BASS, LK_ACID, LK_PLUCK, LK_DRUMS};
    const uint16_t col = lk_col(v);
    const int q = lk_quiet(v);
    char u[16];
    int i, j = 0, k;
    console_top();
    cv_panel_g(2, 24, 184, 120, mix(K_PANEL, col, 3), mix(K_PANEL, col, 1), 0);
    cv_rect(2, 24, 184, 2, col);
    {
        int32_t x = cv_text(6, 25, &FONT_XS, LK_NAME[v], q ? K_BASS : col);
        lk_sound(v, u);
        cv_text(x + 6, 25, &FONT_XS, u, q ? K_BASS : K_DIM);
    }
    for (i = 0; i < 5; i++)
        if (ORDER[i] != v)
            console_sliver(ORDER[i], 188 + (j++) * 13);
    if (v == LK_PAD) {                           /* the pad: its voices on two octaves of keys, the chord's notes named */
        static const uint8_t WK[7] = {0, 2, 4, 5, 7, 9, 11};
        static const int8_t BK[12] = {-1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1};   /* a black key: after white key n of the octave */
        int lo = 127, base, o, held[4] = {0}, nh = 0;
        for (k = 0; k < EN_NPAD; k++)
            lo = en_pad_note[k] < lo ? en_pad_note[k] : lo;
        base = lo / 12 * 12;
        for (k = 0; k < EN_NPAD; k++)
            held[nh++] = en_pad_note[k] - base;
        for (o = 0; o < 14; o++) {               /* the white keys */
            const int n = o / 7 * 12 + WK[o % 7];
            int on = 0;
            for (k = 0; k < nh; k++)
                on |= held[k] == n;
            cv_rect(8 + o * 12, 44, 11, 54, on ? (q ? mix(K_PANEL, col, 6) : col) : K_TEXT);
        }
        for (o = 0; o < 24; o++)                 /* the black keys */
            if (BK[o % 12] >= 0) {
                int on = 0;
                for (k = 0; k < nh; k++)
                    on |= held[k] == o;
                cv_rect(8 + (o / 12 * 7 + BK[o % 12]) * 12 + 8, 44, 7, 32, on ? (q ? mix(K_PANEL, col, 6) : col) : K_BG);
            }
        {   /* the notes, named, under the keys */
            int32_t x = 8;
            for (k = 0; k < nh; k++)
                x = cv_text(x, 102, &FONT_S, EN_NOTE_NAME[(base + held[k]) % 12], K_TEXT) + 8;
        }
        {   /* the pad's level */
            const int32_t f = (int32_t)(fm_clampf(en_pad_level, 0.0f, 1.0f) * 172.0f);
            cv_rect(8, 132, 172, 3, K_LINE);
            if (!q)
                cv_rect(8, 132, f, 3, col);
        }
    } else if (v == LK_DRUMS) {                  /* the drums: six lanes by sixteen steps, the lane's name lit by its hit */
        int l, s;
        if (en_playing)                          /* (0.17.40) the step playing: its column lit behind the six lanes */
            cv_glow(36 + (int32_t)(en_drum_pos & 15u) * 9, 42, 8, 92, K_ACC, mix(K_PANEL, col, 2), 1);
        for (l = 0; l < EN_NLANE && l < 6; l++) {
            const int32_t y = 42 + l * 16;
            const uint16_t bits = lane_now(l);
            char ln[8];
            caps(ln, EN_LANE_NAME[l], 5);
            cv_text(6, y - 3, &FONT_XS, ln, mix(K_DIM, K_TEXT, q16(fm_clampf(en_drum_glow[l], 0.0f, 1.0f))));
            for (s = 0; s < 16; s++) {
                const int32_t x = 36 + s * 9;
                const int hit = (int)(bits >> s & 1u), now = en_playing && s == (int)en_drum_pos;
                const int lvl = en_mod[l][s] >> 6 & 3;
                cv_rect(x, y, 8, 12, hit ? (now ? K_TEXT : lvl == 3 ? K_ACC : lvl == 1 ? mix(K_PANEL, col, 8) : col) : now ? K_LINE : mix(K_PANEL, K_LINE, s % 4 ? 6 : 12));
            }
        }
    } else {                                     /* a line: the bar as a roll */
        console_roll(v, 8, 42, 176, 92);
        if ((v == LK_ACID && en_acid_ans) || (v == LK_BASS && en_bass_ans))
            cv_text(184 - text_w(&FONT_XS, "ANSWER"), 25, &FONT_XS, "ANSWER", K_ACC);
    }
}

/* CONSOLE: the sequencer as a tracker: the steps down, a lane a column; the chosen lane lit; an accent in the accent, a ghost half wide, a ratchet split */
static void console_seq(void)
{
    const int p = seq_pat();
    int l, s;
    for (l = 0; l < EN_NLANE && l < 6; l++) {
        const int32_t x = 20 + l * 36;
        char u[8];
        caps(u, EN_LANE_NAME[l], 5);             /* four letters fit a column */
        if (l == ui.lane)
            cv_rect(x, 0, 35, 144, K_PANEL);
        cv_text(x + 2, -2, &FONT_XS, u, l == ui.lane ? K_ACC : K_DIM);
    }
    for (s = 0; s < 16; s++) {
        const int32_t y = 14 + s * 8;
        if (en_playing && s == (int)en_drum_pos && p == (int)en_pat_now)
            cv_rect(0, y, 240, 8, mix(K_BG, K_ACC, 4));
        if (s % 4 == 0) {
            char t[4];
            t[0] = (char)('0' + (s + 1) / 10), t[1] = (char)('0' + (s + 1) % 10), t[2] = 0;
            cv_text(2, y - 4, &FONT_XS, t, s == (int)en_drum_pos && en_playing ? K_ACC : K_DIM);
            cv_rect(20, y, 216, 1, K_LINE);
        }
        for (l = 0; l < EN_NLANE && l < 6; l++) {
            const int32_t x = 20 + l * 36;
            if (en_pats[seq_src(l)][l] >> s & 1u) {
                const uint8_t m = en_mod[l][s];
                const int lvl = m >> 6 & 3, rat = m & 7;
                const uint16_t c = lvl == 3 ? K_ACC : K_PULSE;
                const int32_t w = lvl == 1 ? 15 : 31;
                if (rat >= 1 && rat <= 4) {      /* a ratchet: two to six hits, drawn as two to four */
                    const int n = rat + 1 > 4 ? 4 : rat + 1;
                    int j;
                    for (j = 0; j < n; j++)
                        cv_rect(x + 2 + j * (w + 1) / n, y + 2, (w + 1) / n - 1, 5, c);
                } else
                    cv_rect(x + 2, y + 2, w, 5, c);
            } else
                cv_rect(x + 17, y + 4, 2, 2, K_LINE);
        }
    }
}

static uint32_t lk_hash(uint32_t h)             /* what the strips and the tracker show besides the band's own */
{
    int i;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(view_knob(i)) + 100u);
    h = hash(h, (uint32_t)(plat_ms() < ui.touch_until) | (uint32_t)ui.touched << 1 | (uint32_t)ui.trk_m << 8 | (uint32_t)ui.trk_s << 16 | (uint32_t)ui.lane << 24);
    h = hash(h, (uint32_t)(plat_ms() < ui.msg_until) | (uint32_t)en_walk_next_root << 4 | (uint32_t)en_chord_alt << 8 | (uint32_t)ui.view << 12 | (uint32_t)en_cm_pos << 20);
    for (i = 0; i < 16; i++)
        h = hash(h, en_bs_hist[i]);
    return h;
}

/* (0.17.40) the main band sent; a new page's first frame a quarter darker, the next as it is: it settles in, not slides
 * (not in Calm, nor while the pages are being flipped through: a page lasting under a third of a second gets none) */
static void main_blit(void)
{
    if (ui.view != ui.fade_view) {
        const uint32_t now = plat_ms();
        ui.fade_view = ui.view;
        if (!UI_CALM && now - ui.fade_t > 300u && now - ui.boot_t > 2000u) {
            uint16_t *p = cv_px, *e = cv_px + cv_w * cv_h;
            for (; p < e; p++) {
                const uint32_t c = swap16(*p);
                *p = swap16(c - ((c >> 2) & 0x39E7u));
            }
            ui.sig[1] = 0;                       /* (drawn again, whole, the next frame) */
        }
        ui.fade_t = now;
    }
    cv_blit(0, MAIN_Y);
}
static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)sound_root() << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if (ui.frame % 2u)                           /* 30 frames a second is plenty */
        return;
    if (!ui.sig[1])                              /* (0.17.40) drawn again from scratch: sent whole, not only the rows that changed */
        cv_rows_reset();
    h = hash(h, ui.theme);
    if (!mix_show() && shift_show()) {           /* SEL held: what the shift does */
        h = hash(0x53484654u, (uint32_t)ui.theme | (uint32_t)(ui.view == V_SEQ) << 8 | (uint32_t)(ui.fx_menu && !ui.fx_page) << 9);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_shift();
            main_blit();
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
            main_blit();
        }
        return;
    }
    if (zoom_knob() >= 0) {                      /* ZOOM: the knob turned, large */
        const int zi = zoom_knob(), zk = view_knob(zi);
        h = hash(hash(0x5A4F4F4Du, (uint32_t)zi | (uint32_t)ui.theme << 4 | (uint32_t)ui.view << 8 | (uint32_t)ui.fx_page << 16), (uint32_t)knob_get(zk) ^ (uint32_t)zk << 20);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_zoom(zi);
            main_blit();
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
        h = hash(h, ui.flex_last | (uint32_t)ui.flex_cur << 8 | (uint32_t)ui.flex_lock << 16);
        if (ui.fx_page == FXP_SAMPLE + 1)         /* SAMPLE: the sound playing, the keys, the count-in */
            h = hash(hash(h, ui.keys), (uint32_t)(ui.smp_cur + 1) | (uint32_t)ui.smp_mode << 8 | (uint32_t)(ui.smp_t0 != 0) << 10 | (uint32_t)ui.smp_fx << 11 |
                                       (uint32_t)(ui.smp_t0 && plat_ms() - ui.smp_t0 < SMP_COUNT) << 12);
        if (ui.fx_page == FXP_MIDIIN + 1)         /* MIDI IN: what comes in, the host */
            h = hash(hash(h, ui.min_last ^ ui.min_n << 3), (uint32_t)plat_usb_host_state() | (uint32_t)en_ext << 4 | (uint32_t)plat_ms() / 500u << 5);
        if (ui.fx_page == 37 && ui.fxs[EN_FXS_LK_MODE] >= 4)   /* LINK: the cable's readout (0.17.55) */
            h = hash(h, plat_ms() / 250u);
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
            main_blit();
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
            main_blit();
        }
        return;
    }
    if (ui.view == V_JAM) {                      /* the JAM page: the keyboard, its scale, what is held */
        h = hash(hash(0x4A414D31u, ui.keys | (uint32_t)ui.jam_voice << 27 | (uint32_t)ui.theme << 29), pool_abs() | (uint32_t)ui.jam_mode << 12 | (uint32_t)ui.jam_oct << 14 |
                                                                                                        (uint32_t)proj.par[P_CM_SND] << 17 | (uint32_t)proj.par[P_PADTONE] << 22 | (uint32_t)proj.par[P_AC_WAVE] << 25 | (uint32_t)proj.par[P_DR_INST] << 28);
        h = hash(h, (en_playing ? chord_pcs() : 0u) | (uint32_t)en_chord_root << 12);   /* (the chord playing) */
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_jam();
            main_blit();
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
            main_blit();
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
        if (ui.view == V_ACSTEP) {
            int s;
            as_sync();
            for (s = 0; s < 16; s++)
                h = hash(h, (uint32_t)ui.as_off[s] | (uint32_t)ui.as_oct[s] << 4);
            h = hash(h, (uint32_t)ui.as_gate | (uint32_t)ui.as_acc << 16);
            h = hash(h, (uint32_t)ui.as_slide | (uint32_t)ui.astp << 16 | (uint32_t)proj.par[P_AC_LOCK] << 24 | (uint32_t)en_acid_pos << 25);
        }
        if (ui.view == V_SETL) {
            int s;
            for (s = 0; s < SL_N; s++)
                h = hash(h, glob.sl[s]);
            h = hash(h, (uint32_t)ui.slsl | (uint32_t)ui.sl_pos << 8 | (uint32_t)glob.sl_on << 16 | (uint32_t)proj.genre << 17 | (uint32_t)proj.piece << 24 | (uint32_t)ui.sl_q << 28);
            for (s = 0; s < SL_N; s++)
                h = hash(h, glob.sl_bars[s] | (uint32_t)glob.sl_mute[s] << 8);
            h = hash(h, glob.sl_end | (en_playing ? sl_bar_now() - sl_b0 : 0u) << 1 | (ui.btn >> B_SEL & 1u) << 31);
            for (s = 0; s < SL_N; s++)
                h = hash(h, glob.sl_pat[s] | (uint32_t)glob.sl_mood[s] << 8);
        }
        if (ui.view == V_PROG) {
            int s;
            for (s = 0; s < EN_PRG_LEN; s++)
                h = hash(h, proj.prg[s] | (uint32_t)(s < EN_PRK_LEN ? proj.prk[s] : 0) << 8);
            h = hash(h, (uint32_t)ui.pgsl | (uint32_t)(uint8_t)en_prog_pos << 8 | (uint32_t)en_chord_root << 16 | (uint32_t)en_playing << 24);
        }
        if (ui.view == V_BLSTEP) {
            int s;
            for (s = 0; s < 16; s++)
                h = hash(h, en_bs[s]);
            h = hash(h, (uint32_t)ui.bstp | (uint32_t)proj.par[P_BL_MODE] << 8 | (uint32_t)(uint8_t)en_bs_pos << 16 | (uint32_t)en_playing << 24);
        }
        if (ui.view == V_CMSTEP) {
            int s;
            for (s = 0; s < 16; s++)
                h = hash(h, (uint32_t)en_cm_rung[s] | (uint32_t)en_cm_lvl[s] << 4);
            h = hash(h, (uint32_t)ui.pstp | (uint32_t)proj.par[P_CM_LOCK] << 8 | (uint32_t)en_cm_pos << 12 | (uint32_t)en_playing << 16);
        }
        if (ui.view == V_STEPS) {
            int s;
            for (s = 0; s < 16; s++)
                h = hash(h, (uint32_t)en_mod[ui.lane % EN_NLANE][s]);
            h = hash(h, (uint32_t)ui.lane | (uint32_t)ui.stp << 4 | (uint32_t)en_pat_now << 8 | (uint32_t)en_lane_pos[ui.lane % EN_NLANE] << 12 | (uint32_t)proj.par[P_DENS + ui.lane % EN_NLANE] << 20);
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
            if (LOOK_CONSOLE)                    /* the Console layout: a picture page wears its channel's colour along the top, as a strip does */
                cv_rect(0, 0, 240, 2, view_color());
            main_blit();
        }
        return;
    }
    for (i = 0; i < EN_NLANE; i++)
        h = hash(h, (ui.view == V_SEQ ? en_pats[seq_src(i)][i] : lane_now(i)) | (uint32_t)q16(en_drum_glow[i]) << 16);
    h = hash(h, en_drum_pos | (uint32_t)en_pat_now << 8 | (uint32_t)en_pat_next << 12 | (uint32_t)en_pulse_rest << 16 |
                    (uint32_t)ui.view << 20);
    if (ui.view == V_SEQ) {
        h = hash(h, ui.lane | (uint32_t)proj.par[P_PATTERN] << 16);
        for (i = 0; i < EN_NLANE; i++)
            h = hash(h, (uint32_t)proj.par[P_PROB + i] | (uint32_t)proj.par[P_LLEN + i] << 8 | (uint32_t)proj.par[P_LSRC + i] << 16);
        {                                        /* the step tricks show too */
            int s;
            for (s = 0; s < 16; s++)
                h = hash(h, (uint32_t)en_mod[0][s] | (uint32_t)en_mod[1][s] << 8 | (uint32_t)en_mod[2][s] << 16 | (uint32_t)en_mod[3][s] << 24);
            h = lk_hash(h);
        }
        if (h == ui.sig[1])
            return;
        ui.sig[1] = h;
        cv_begin(240, MAIN_H, K_BG);
        if (LOOK_CONSOLE)
            console_seq();
        else
            draw_seq();
        main_blit();
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
    h = hash(h, (en_playing && !LOOK_CONSOLE ? plat_ms() / 66u : 0u) | (uint32_t)en_sec << 28 | (uint32_t)ui.centre_imp << 30 | (uint32_t)en_shed_level << 26);   /* the sigil moves: 15 frames a second while playing */
    h = lk_hash(h);
    h = hash(h, proj.pool | (uint32_t)sound_root() << 12 | (uint32_t)proj.par[P_MODE] << 16);   /* (Global's scale) */
    if (ui.view == V_CODE) {                     /* (the code: its letters, the place, the seed, the settings it checks) */
        for (i = 0; i < CODE_N; i++)
            h = hash(h, code_in[i] | (uint32_t)code_typed << 8 | (uint32_t)code_pos << 9);
        h = hash(h, code_seed | (uint32_t)code_clean << 16 | (uint32_t)proj.genre << 17 | (uint32_t)proj.par[P_TEMPO] << 22);
        for (i = 0; i < P_NPARAMS; i++)
            h = hash(h, (uint32_t)proj.par[i]);
    }
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (LOOK_CONSOLE) {
        if (ui.view == V_GLOBAL)
            console_global();            /* Global: the scale */
        else if (ui.view == V_CODE)
            console_code();              /* the track code */
        else if (lk_voice() >= 0 && ui.view != V_TEMPO)
            console_voice(lk_voice());   /* a voice's page: its strip opened wide */
        else
            console_home();
        main_blit();
        return;
    }
    if (LOOK_SYNTH)
        draw_horizon();
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
    main_blit();
}

/* the knob strip (X0X's): a rule with the page's name and a dot a page at its right end; under it a
 * cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The knob being
 * turned is drawn on a tint. Only redrawn when what it shows changes. */
static uint16_t view_color(void)                /* the page's voice in its colour: the pad, the drone, the acid, the drums */
{
    const int g = group_of(ui.view == V_SEQ ? V_DRUMS : ui.view);
    switch (g >= 0 ? GROUP[g].btn : -1) {
    case B_LFO: return K_BASS;
    case B_ARP: return ui.view == V_CMEL || ui.view == V_CMEL2 || ui.view == V_CMVU || ui.view == V_CMSTEP ? IMP_TEAL : K_ACID;
    case B_SEQ: return K_PULSE;
    case B_HOME: return KC_REPEAT;
    default: return ui.view == V_JAM ? KC_MOVE : perf_like() ? KC_FILTER : K_PAD;
    }
}

/* the accordion tabs (the Imp skin's knob strip): a chip a group of pages; the one you are in opens to its name and a pip a page */
static void imp_tabs(void)
{
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

/* the Console layout's page groups: ten equal keys across; the group you are in lit in its colour, a pip a page in it (the page you are on dark) */
static void console_tabs(void)
{
    const uint16_t col[10] = {K_TEXT, K_PAD, K_BASS, K_ACID, IMP_TEAL, K_PULSE, K_FROST, K_DIM, K_TEXT, KC_MOVE};
    const int v = ui.view == V_SEQ ? V_DRUMS : ui.view >= V_PERF ? V_TEMPO : (int)ui.view;
    int i, p;
    for (i = 0; i < 10; i++) {
        const int32_t x = i * 24;
        const int on = v >= TB[i].first && v < TB[i].first + TB[i].n;
        char l[2] = {TB[i].ltr, 0};
        if (on) {                                /* (0.17.40) the lit tab raised, with the focus ring; the others flat */
            cv_glow(x + 1, 11, 22, 13, col[i], K_BG, 1);
            cv_panel(x + 1, 11, 22, 13, col[i], 0);
        } else
            cv_rect(x + 1, 11, 22, 13, K_PANEL);
        cv_text(x + 12 - text_w(&FONT_XS, l) / 2, 9, &FONT_XS, l, on ? K_BG : K_DIM);
        if (!on)
            cv_rect(x + 1, 22, 22, 2, mix(K_PANEL, col[i], 7));
        else
            for (p = 0; TB[i].n > 1 && p < TB[i].n; p++)
                cv_rect(x + 12 - TB[i].n * 2 + p * 4, 21, 3, 2, p == v - TB[i].first ? K_BG : mix(col[i], K_BG, 7));
    }
}

/* (0.17.39) a knob's value as the Console strip shows it: the number, and its unit apart ("0.54" "s", "-3.0" "dB", "4" "bars"),
 * the unit as written (lower case: drawn small after the number); a word value whole, in capitals. Returns the largest font it may use */
static const felucca_font_t *const KV_F[4] = {&FONT_M, &FONT_B, &FONT_S, &FONT_XS};
static const int8_t KV_Y[4] = {34, 40, 40, 42};  /* one baseline (row 52) whatever the font */
static int kv_split(int k, char *num, char *unit)
{
    char t[16];
    int i = 0, j, u = 0;
    knob_text(k, t);
    unit[0] = 0;
    if (k == K_PGCH && knob_get(k)) {            /* (a program change's name: as written) */
        memcpy(num, t, 16);
        return 1;
    }
    while (t[i] == ' ' || t[i] == '-' || t[i] == '+')
        i++;
    if (t[i] >= '0' && t[i] <= '9') {
        while ((t[i] >= '0' && t[i] <= '9') || t[i] == '.' || t[i] == ':')
            i++;
        j = i + (t[i] == ' ');
        while ((t[j + u] >= 'a' && t[j + u] <= 'z') || (t[j + u] >= 'A' && t[j + u] <= 'Z'))
            u++;
        if (u && u <= 4 && !t[j + u]) {
            memcpy(unit, t + j, (uint32_t)u + 1u);
            t[i] = 0;
        } else
            u = 0;
    }
    caps(num, t, 16);
    return 0;
}
static int kv_num(const char *num)              /* a number (its own size class: the numbers on a strip share one size, the words another) */
{
    return (num[0] >= '0' && num[0] <= '9') || ((num[0] == '-' || num[0] == '+' || num[0] == ' ') && num[1] >= '0' && num[1] <= '9');
}
static int kv_fits(int fi, const char *num, const char *unit)
{
    return text_w(KV_F[fi], num) + (unit[0] ? 2 + text_w(&FONT_XS, unit) : 0) <= 52;
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
    int now_touch = plat_ms() < ui.touch_until, tick = now_touch && plat_ms() < ui.tick_until && !UI_CALM;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(view_knob((int)i)) + 100u);
    h = hash(h, (uint32_t)tick << 31);
    h = hash(h, (uint32_t)now_touch | (uint32_t)ui.fx_menu << 1 | (uint32_t)ui.fx_page << 2 | (uint32_t)fx_bank << 10 | (uint32_t)(ui.btn >> B_SEL & 1u) << 20);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[24];
        int k;
        const int ndots = ui.fx_menu ? (ui.fx_page ? (st_pos(ui.fx_page) >= 0 ? ST_N : FX_N) : 4) : NDIAL;   /* the dots: the pages; in the FX menu the banks, in an effect's settings the edit pages */
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
        const int32_t x0 = LOOK_CONSOLE && !ui.fx_menu ? cv_text(4, -1, &FONT_XS, t, view_color()) + 6 : 4;   /* the Console layout: the page's name first, in its voice's colour (no dots: the tabs say where) */
        int32_t xl = 0;                          /* where the left text ends: the page's name only if it clears it */
        xt = LOOK_CONSOLE && !ui.fx_menu ? 236 : xd - 5 - text_w(&FONT_XS, t);   /* (Console: the room an octave tag may use) */
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
            xs = cv_text(x0, -1, &FONT_XS, u, proj.edited >> proj.piece & 1u ? K_TEXT : K_DIM);
            if (xt - 9 > xs + 4)
                cv_rect(xs + 4, 5, xt - 9 - xs - 4, LOOK_CONSOLE ? 0 : 1, K_LINE);   /* (the Console look: no rule) */
            xl = xs;
        } else {                                 /* the voice's sound (the drums: the kit), then the rule */
            char u[16], t[12];
            int32_t xs;
            int vc = voice_of(ui.view);
            if (vc >= 0) {
                en_param_text(SOUND_PAR[vc], proj.par[SOUND_PAR[vc]], t);
                caps(u, t, sizeof u);
            } else if (ui.view == V_CMEL || ui.view == V_CMEL2 || ui.view == V_CMVU || ui.view == V_CMSTEP) {
                caps(u, EN_CM_SOUND[(unsigned)proj.par[P_CM_SND] < EN_NCMSND ? proj.par[P_CM_SND] : 0], sizeof u);
            } else if (kit_view(ui.view)) {
                int k = proj.par[P_KIT_KICK];
                int same = proj.par[P_KIT_SNARE] == k && proj.par[P_KIT_HAT] == k && proj.par[P_KIT_PERC] == k;
                caps(u, same ? EN_KIT_NAME[(unsigned)k < EN_NKITS ? k : 0] : "Mixed kit", sizeof u);
            } else {
                caps(u, LOOK_CONSOLE ? "" : "PERF-1", sizeof u);
            }
            xs = cv_text(x0, -1, &FONT_XS, u, K_DIM);
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
                cv_rect(xs + 4, 5, xt - 9 - xs - 4, LOOK_CONSOLE ? 0 : 1, K_LINE);   /* (the Console look: no rule) */
            xl = xs;
        }
        if (!(LOOK_CONSOLE && !ui.fx_menu) && xt >= xl + 6)                        /* (a long sound name, BY HAND or the store's state would run into it) */
            cv_text(xt, -1, &FONT_XS, t, K_ACC);
        if (!ui.fx_menu && (ui.btn >> B_SEL & 1u))
            cv_text(4, 14, &FONT_XS, ui.view == V_SEQ ? "SEL: KEYS=PATTERNS KNOBS=TRICKS" :
                                     voice_of(ui.view) >= 0 ? "SEL: ALGO=SOUND ENV=HAND PRE=PIECE" : "SEL: PRESETS=PIECE ALGO=SOUND", K_ACC);   /* the footer hint: what shift does here */
        else if (!ui.fx_menu)
            LOOK_CONSOLE ? console_tabs() : imp_tabs();   /* tabs: Console's ten keys, or a chip a group */
        else for (k = 0; k < ndots; k++) {
            int cur, same;
            if (ui.fx_menu) {
                cur = k == (ui.fx_page ? fx_pos(ui.fx_page) : (int)fx_bank);
                same = 0;
            } else {
                cur = k == ui.view || (ui.view == V_SEQ && k == V_DRUMS);
                same = group_of(k) >= 0 && group_of(k) == group_of(ui.view == V_SEQ ? V_DRUMS : ui.view);
            }
            cv_rect(xd + k * 5, 17, 3, 4, cur ? K_ACC : same ? K_DIM : K_LINE);
        }
    }
    int vfi[2] = {0, 0};                         /* (0.17.39) one value font for the strip's words, one for its numbers: the largest all of them fit */
    if (LOOK_CONSOLE)
        for (i = 0; i < 4; i++) {
            char vn[16], vu[8];
            int fi;
            if (view_knob((int)i) == K_NONE)
                continue;
            fi = kv_split(view_knob((int)i), vn, vu);
            while (fi < 3 && !kv_fits(fi, vn, vu))
                fi++;
            vfi[kv_num(vn)] = fi > vfi[kv_num(vn)] ? fi : vfi[kv_num(vn)];
        }
    for (i = 0; i < 4; i++) {
        int k = view_knob((int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        const uint16_t acc = hot && tick ? mix(K_ACC, K_TEXT, 6) : K_ACC;   /* (0.17.40) a step: the accent a moment brighter, then settled */
        const uint16_t fill = hot ? acc : view_color();
        if (k == K_NONE)
            continue;
        if (LOOK_CONSOLE) {                      /* the Console look: a fader a knob: its name, the value left, a track and a cap */
            char vt[16], vu[8];
            int32_t cp, xe;
            knob_range(k, &lo, &hi);
            v = knob_get(k);
            if (i)
                cv_rect(x, 27, 1, 40, K_LINE);
            if (hot)
                cv_panel(x + 2, 25, 56, 43, K_PANEL, 0);
            caps(t, view_label((int)i), sizeof t);
            cv_text(x + 5, 24, &FONT_XS, t, hot ? K_TEXT : K_DIM);
            kv_split(k, vt, vu);
            xe = cv_text_fit(x + 5, KV_Y[vfi[kv_num(vt)]], KV_F[vfi[kv_num(vt)]], vt, hot ? acc : K_TEXT, vu[0] ? 52 - 2 - text_w(&FONT_XS, vu) : 52, 0);
            if (vu[0])                           /* the unit: small, after the number, on its baseline */
                cv_text(xe + 2, 42, &FONT_XS, vu, hot ? K_TEXT : K_DIM);
            cp = hi > lo ? (int32_t)(v - lo) * 45 / (hi - lo) : 0;
            cv_panel(x + 4, 60, 52, 4, hot ? K_BG : mix(K_BG, K_LINE, 6), 1);
            if (lo < 0)                          /* bipolar: from the middle */
                cv_rect(cp < 22 ? x + 5 + cp : x + 27, 61, cp < 22 ? 22 - cp : cp - 22, 2, fill);
            else
                cv_rect(x + 5, 61, cp, 2, fill);
            if (hot)                             /* (0.17.40) the focus: the cap glows */
                cv_glow(x + 4 + cp, 57, 5, 10, K_ACC, K_PANEL, 2);
            cv_rect(x + 5 + cp - 1, 57, 5, 10, hot ? acc : K_TEXT);
            continue;
        }
        {                                        /* the label, the value large, a bar under it */
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
            {                                    /* the value in the largest type that fits its card (a long name steps down: M, B, S, then XS) */
                const int cased = k == K_PGCH && knob_get(k);   /* a chord's name as written (Am7 is not AM7): M has no small letters, so B */
                const felucca_font_t *vf = cased ? &FONT_B : &FONT_M;
                int32_t vy = cased ? 39 : 34;
                if (cased)
                    memcpy(vt, t, sizeof vt);
                else
                    caps(vt, t, sizeof vt);
                if (text_w(vf, vt) > 56)
                    vf = &FONT_B, vy = 39;
                if (text_w(vf, vt) > 56)
                    vf = &FONT_S, vy = 39;
                if (text_w(vf, vt) > 56)
                    vf = &FONT_XS, vy = 41;
                text_c(cx, vy, vf, vt, hot ? K_ACC : K_TEXT);
            }
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
        }
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
    if (en_frozen || (ui.view == V_JAM && (ph_st >= 2 || (ph_st == 1 && (ui.frame / 8u) & 1u))))
        b |= 1u << B_REC;                        /* (on JAM: the phrase writing or playing; blinking, armed) */
    if (ui.fx_lock && !ui.fx_menu && (ui.frame / 16u) & 1u)
        b |= 1u << B_FX;                         /* effects locked on, the menu closed: FX blinks */
    if (en_sec != EN_SEC_GROOVE || (en_sec_req && (ui.frame / 8u) & 1u))   /* a section; blinking: on its way */
        b |= 1u << B_SAVE;
    if ((ui.btn >> B_SEL & 1u) && !(ui.view == V_JAM && !ui.fx_menu)) {   /* shift: the pattern chosen; the one playing blinks if not (on JAM SEL is the accent) */
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
    } else if (ui.fx_menu && ui.fx_page == FXP_SAMPLE + 1) {   /* SAMPLE: by key, its ten keys (held: dark); a run, the sound's key */
        b |= 1u << B_FX;
        for (i = 0; i < SMP_N; i++)
            if (ui.smp_mode == 3 ? !(ui.keys >> WHITE_KEY[i] & 1u) : ui.smp_t0 && ui.smp_cur == (int)i)
                k |= 1u << WHITE_KEY[i];
    } else if (ui.fx_menu && ui.fx_page) {       /* another effect's page: FX lit */
        b |= 1u << B_FX;
    } else if (ui.fx_menu && fx_bank == 3) {     /* the sampler: the keys held */
        b |= 1u << B_FX;
        k |= ui.keys;
    } else if (ui.fx_menu) {                     /* FX: lit; the keys held */
        b |= 1u << B_FX;
        for (i = 0; i < NKEYS; i++)
            if (key_on((int)i))
                k |= 1u << i;
    } else if (ui.view == V_ACSTEP) {            /* the acid steps: the notes of the line lit among them, the step chosen too */
        as_sync();
        for (i = 0; i < 16; i++)
            if ((uint32_t)i == (uint32_t)(ui.astp & 15u) || (uint32_t)(ui.as_gate >> i & 1u) ^ (uint32_t)(en_playing && i == en_acid_pos))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.view == V_SETL) {              /* the set list: the slots set lit, the one chosen too, the one playing blinks against them */
        for (i = 0; i < SL_N; i++)
            if (i == (uint32_t)(ui.slsl % SL_N) || (uint32_t)(glob.sl[i] != 0) ^ (uint32_t)(glob.sl_on && i == ui.sl_pos && (ui.frame / 8u) & 1u))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.view == V_PROG) {              /* the progression: the slots set lit, the one chosen too, the one playing blinks against them */
        for (i = 0; i < EN_PRG_N; i++)
            if ((uint32_t)i == (uint32_t)(ui.pgsl & 7u) || (uint32_t)(en_prg_get(proj.prg, 3 * (int)i, 3) != 0) ^ (uint32_t)(en_playing && (int)i == en_prog_pos))
                k |= 1u << WHITE_KEY[i];
        for (i = 0; i < 2; i++)                  /* PEDAL: white keys 9 and 10 lit while the bass, the pad, hold the key */
            if (glob.sl_rsv[2] >> i & 1u)
                k |= 1u << WHITE_KEY[8 + i];
        if (pg_scale() == 7 && (ui.frame / 8u) % 6u == 0u)   /* the fills on the black keys: lit a frame in six, faint beside the slots */
            for (i = 0; i < PGF_N; i++)
                k |= 1u << BLACK_KEY[i];
    } else if (ui.view == V_BLSTEP) {            /* the bass steps: likewise */
        for (i = 0; i < 16; i++)
            if ((uint32_t)i == (uint32_t)(ui.bstp & 15u) || (uint32_t)(EN_BS_KIND(en_bs[i]) != 0) ^ (uint32_t)(en_playing && (int)i == (int)en_bs_pos))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.view == V_CMSTEP) {            /* the pluck steps: likewise */
        for (i = 0; i < 16; i++)
            if ((uint32_t)i == (uint32_t)(ui.pstp & 15u) || (uint32_t)(en_cm_lvl[i] != 0) ^ (uint32_t)(en_playing && i == en_cm_pos))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.view == V_STEPS) {             /* the steps: the step chosen lit, the lane's hits dim among them (the playhead too) */
        const int l = ui.lane % EN_NLANE;
        const uint16_t pl = en_lane_mask(l, play_src(l), proj.par[P_DENS + l]);
        for (i = 0; i < 16; i++)
            if ((i == (uint32_t)(ui.stp & 15u)) || ((pl >> i & 1u) ^ (uint32_t)(en_playing && i == en_lane_pos[l])))
                k |= 1u << WHITE_KEY[i];
    } else if (ui.view == V_SEQ) {               /* the sequencer: the lane's hits on the white keys, the playhead */
        uint16_t bits = en_pats[seq_src(ui.lane)][ui.lane];
        for (i = 0; i < 16; i++)
            if ((bits >> i & 1u) ^ (uint32_t)(en_playing && i == en_drum_pos))
                k |= 1u << WHITE_KEY[i];
    } else {                                     /* the scale; while playing, the chord playing bright and the rest of the scale dim (0.15.21, a GitHub ask) */
        const uint32_t ch = en_playing ? chord_pcs() : 0u;
        uint32_t d = 0;
        for (i = 0; i < NKEYS; i++) {
            const uint32_t pc = (5u + i) % 12u;
            if (ch >> pc & 1u)
                k |= 1u << i;
            else if (ap >> pc & 1u)
                *(ch ? &d : &k) |= 1u << i;
        }
        if (ui.view == V_JAM)
            k |= ui.keys;                        /* the JAM page: the keys held too */
        plat_keys_dim(d & ~k);
    }
    plat_glow(1);
    plat_leds(b, k);
}

/* the FX knobs glide to a scene's values (a second or so) instead of jumping; a knob turned by hand takes its own back */
/* THE GESTURE RECORDER (0.17.31; 0.17.44: it waits for you, and records the effect played too). In the FX menu, SEL + REC arms it (ARM); the
 * first move of an FX knob, or an effect played, starts it (REC), the loop's first step the start of that bar (the steps before the move: the
 * knobs as they were). Every sixteenth of the four knobs and the effect held is written, up to four bars; SEL + REC closes the loop at that
 * bar's end (the fourth bar's end closes it anyway). Then it loops: the knobs gliding from one sixteenth to the next, the effect on where it was
 * played (a hand's effect goes first). Or, with AUTO FX on and its Pick on YOURS, it is kept for AUTO FX, which plays it once on its own
 * phrase lines (Every) and puts the knobs back. A knob turned by hand (or by MIDI, or home with FX + OCT-) is the hand's again; SEL + REC in the
 * FX menu while it loops (or is kept) lets it go. Not stored, like the FX knobs. 324 B. */
static void gesture_key(void)                   /* FX menu, SEL + REC */
{
    int k;
    if (gs_st == 0) {
        if (!en_playing) {
            say("GESTURE", "PLAY FIRST");
            return;
        }
        gs_st = 1;
        gs_stop = 0;
        gs_shot = 0;
        for (k = 0; k < 4; k++)
            gs_a[k] = ui.fxk[k];
        say("GESTURE READY", "MOVE A KNOB OR PLAY AN FX");
    } else if (gs_st == 1) {
        gs_st = 0;
        say("GESTURE", "OFF");
    } else if (gs_st == 2) {
        gs_stop = 1;
        say("GESTURE", "ENDS AT THE BAR'S END");
    } else {
        gs_st = 0;
        gs_mask = 0;
        gs_shot = 0;
        gs_fxnow = 0;
        say("GESTURE", "LET GO: THE KNOBS ARE YOURS");
    }
}
/* the sixteenth a key belongs to: the nearest (Capture's and the phrase's). (0.17.48, the audit: it was rounded by the time this frame saw the
 * sixteenth begin, read after the keys: a key just after the beat was put a sixteenth late. Now the engine's own place in its sixteenth.) */
static uint32_t t16_now(void)
{
    uint32_t b;
    float f;
    do {
        b = en_bars;
        f = en_step_frac;
    } while (b != en_bars);
    return b + (f >= 0.5f ? 1u : 0u);
}
static void gs_shot_end(void)                   /* AUTO FX's play of the gesture is over: the knobs it still holds back where they were */
{
    int k;
    if (!gs_shot)
        return;
    gs_shot = 0;
    gs_fxnow = 0;
    gs_busy = 1;
    for (k = 0; k < 4; k++)
        if (gs_mask >> k & 1u)
            knob_set(K_FXF + k, gs_home[k]);
    gs_busy = 0;
    gs_mask = 0;
}
static void gesture_tick(void)
{
    const uint32_t now = plat_ms();
    int k, i, p;
    if (!gs_st)
        return;
    if (!en_playing) {                           /* stopped: armed or half written, gone; a loop or a kept one waits for the start */
        if (gs_st < 3)
            gs_st = 0;
        gs_shot_end();
        gs_pos = 0;
        gs_bar = 0;
        gs_fxnow = 0;
        return;
    }
    p = en_drum_pos % EN_NSTEP;
    if (gs_st == 1) {                            /* armed: the first move starts it, in the bar it falls in */
        int moved = gs_hand != 0;
        for (k = 0; k < 4; k++)
            if (ui.fxk[k] != gs_a[k])
                moved = 1;
        gs_pos = (uint8_t)p;
        if (!moved)
            return;
        gs_st = 2;
        gs_bar = 0;
        gs_t = now;
        for (i = 0; i < p; i++) {
            for (k = 0; k < 4; k++)
                gs_v[i][k] = gs_a[k];
            gs_fx[i] = 0;
        }
        gs_w = (uint8_t)p;
        say("GESTURE: REC", "SEL + REC ENDS IT");
    }
    if (gs_st == 4 && !gs_shot && ui.af_live && ui.af_fx == AF_GS && p == 0) {   /* AUTO FX's turn: from the top, the knobs' places kept */
        gs_shot = 1;
        gs_bar = 0;
        gs_mask = 15;
        for (k = 0; k < 4; k++)
            gs_home[k] = ui.fxk[k];
    } else if (gs_shot && !(ui.af_live && ui.af_fx == AF_GS)) {
        gs_shot_end();
    }
    if (p != gs_pos) {
        const int wrap = p < gs_pos;
        gs_pos = (uint8_t)p;
        gs_t = now;
        if (gs_st == 2 && wrap) {
            gs_bar++;
            if (gs_stop || gs_bar >= 4) {
                char t[16] = "PLAYS: 1 BAR";
                gs_len = (uint8_t)(gs_bar * EN_NSTEP);
                t[7] = (char)('0' + gs_bar);
                if (gs_bar > 1)
                    t[12] = 'S', t[13] = 0;
                gs_bar = 0;
                gs_fxnow = 0;
                if ((ui.fxs[EN_FXS_AF_AMT] & 3) && ui.fxs[EN_FXS_AF_PICK] == 2) {
                    gs_st = 4;
                    gs_mask = 0;
                    say("GESTURE KEPT", "AUTO FX PLAYS IT");
                } else {
                    gs_st = 3;
                    gs_mask = 15;
                    say("GESTURE", t);
                }
            }
        } else if ((gs_st == 3 || gs_shot) && wrap && gs_len) {
            gs_bar = (uint8_t)((gs_bar + 1) % (gs_len / EN_NSTEP));
        }
    }
    i = gs_bar * EN_NSTEP + p;
    if (gs_st == 2) {                            /* written as it is now (the steps a slow frame skipped: the same) */
        int j = gs_w;
        if (i < j)
            j = i;
        for (; j <= i; j++) {
            for (k = 0; k < 4; k++)
                gs_v[j][k] = ui.fxk[k];
            gs_fx[j] = gs_hand;
        }
        gs_w = (uint8_t)i;
    } else if (gs_len && ((gs_st == 3 && gs_mask) || gs_shot)) {   /* played: this sixteenth's value gliding to the next's, its effect */
        const int n = (i + 1) % gs_len, tempo = proj.par[P_TEMPO] > 0 ? proj.par[P_TEMPO] : 120;
        const uint32_t step = 15000u / (uint32_t)tempo, el = now - gs_t;
        const float f = el >= step ? 1.0f : (float)el / (float)step;
        gs_fxnow = gs_fx[i];
        gs_busy = 1;
        for (k = 0; k < 4; k++)
            if ((gs_mask >> k & 1u) && !(ui.glide_on >> k & 1u)) {
                const float v = (float)gs_v[i][k] + (float)(gs_v[n][k] - gs_v[i][k]) * f;
                knob_set(K_FXF + k, (int)(v + (v < 0.0f ? -0.5f : 0.5f)));
            }
        gs_busy = 0;
    } else {
        gs_fxnow = 0;
    }
}

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
static const char *const JV_NAME[4] = {"PLUCK", "PAD", "ACID", "BASS"};
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
        for (m = 48 + sound_root(); m < 128; m++)
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
static void jam_down(int i) { jam_down_v(i, ui.jam_voice & 3); }
static void jam_down_v(int i, int vc)            /* key i on voice vc (JAM's numbering) */
{
    static const uint8_t VC[4] = {EN_V_PLUCK, EN_V_PAD, EN_V_ACID, EN_V_DRONE};
    static const int8_t OFS[4] = {0, 0, 12, 24};   /* the engine plays the acid an octave and the drone two below the key */
    uint8_t nn[4];
    const int n = jam_notes(i, nn);
    vc &= 3;
    int k;
    if (vc == 0 && proj.par[P_CM_LVL] < 15)
        knob_set(P_CM_LVL, 40);                  /* the pluck has to be audible to be played */
    ui.jam_n[i] = (uint8_t)n;
    ui.jam_vc[i] = VC[vc];
    for (k = 0; k < n; k++) {
        const int key = nn[k] + OFS[vc];
        ui.jam_note[i][k] = (uint8_t)(key > 127 ? 127 : key);
        en_note_v(VC[vc], ui.jam_note[i][k], (plat_buttons() >> B_SEL & 1u) ? 127 : 100);   /* SEL held: an accent (0.15.23) */
    }
    if (en_playing && n > 0 && (ph_st == 1 || (ph_st == 2 && vc == ph_vc))) {   /* (0.17.45) the phrase: the first key starts it (from its bar), each written */
        const uint32_t t16 = t16_now();
        if (ph_st == 1) {
            if (vc == 1) {
                say("PHRASE", "THE ACID, PLUCK OR BASS");
            } else {
                ph_st = 2;
                ph_vc = (uint8_t)vc;
                ph_b0 = t16 & ~15u;
                ph_endb = 0;
                memset(en_ph_note, 0, sizeof en_ph_note);
                memset(en_ph_len, 0, sizeof en_ph_len);
                memset(en_ph_iv, 0, sizeof en_ph_iv);
                memset(ph_ks, 0, sizeof ph_ks);
                say("PHRASE: REC", "REC ENDS IT");
            }
        }
        if (ph_st == 2 && t16 >= ph_b0 && t16 - ph_b0 < EN_PH_N) {
            const uint32_t s = t16 - ph_b0;
            uint32_t iv = 0;
            en_ph_note[s] = ui.jam_note[i][0];
            en_ph_len[s] = (uint8_t)((plat_buttons() >> B_SEL & 1u) ? 0x80u : 0u);
            for (k = n - 1; vc == 0 && k >= 1; k--) {   /* (0.17.53) the pluck: the chord's other keys, as pressed (the page's chords: the third and fifth) */
                const int d = ui.jam_note[i][k] - ui.jam_note[i][0];
                if (d >= 1 && d <= 31)
                    iv = iv << 5 | (uint32_t)d;
            }
            en_ph_iv[s] = (uint16_t)iv;
            ph_ks[i] = (uint8_t)(s + 1u);
        }
    }
    if (en_playing) {                            /* Capture's memory: the note as heard, when (0.17.31: to the nearest sixteenth), over which chord, on which key and voice */
        const int w = ui.cap_w & 31;
        ui.cap_t[w] = (uint16_t)t16_now();
        ui.cap_deg[w] = en_deg;
        ui.cap_note[w] = nn[0];
        ui.cap_kv[w] = (uint8_t)(i | vc << 5 | ((plat_buttons() >> B_SEL & 1u) ? 0x80u : 0u));   /* (0.17.48: bit 7, SEL held: the phrase's accent) */
        ui.cap_len[w] = 0;
        ui.cap_w = (uint8_t)(w + 1);
    }
}
static void jam_up(int i)
{
    int k;
    for (k = 0; k < ui.jam_n[i]; k++)
        en_note(ui.jam_vc[i], ui.jam_note[i][k], 0);
    ui.jam_n[i] = 0;
    if (ph_st == 2 && ph_ks[i]) {                /* (0.17.45) the phrase: how long the key was held, to the nearest sixteenth */
        const uint32_t s = ph_ks[i] - 1u, held = t16_now() - (ph_b0 + s);
        if (en_ph_note[s] == ui.jam_note[i][0])  /* (0.17.48) the step's own key: one a later key took over leaves its length alone */
            en_ph_len[s] = (uint8_t)((en_ph_len[s] & 0x80u) | (held < 1u || held > 0x7FFFu ? 1u : held > 127u ? 127u : held));
        ph_ks[i] = 0;
    }
    for (k = 1; k <= 32; k++) {                  /* Capture's memory: how long the key was held (the latest event of this key still open) */
        const int w = (ui.cap_w - k) & 31;
        if (ui.cap_note[w] && (ui.cap_kv[w] & 31) == i && !ui.cap_len[w]) {
            const uint32_t held = (uint16_t)((uint16_t)t16_now() - ui.cap_t[w]);   /* (0.17.48, the audit: both ends rounded alike, in the 16 bits the
                                                                                       * memory keeps; a tap in a sixteenth's back half wrapped to 255,
                                                                                       * a hold to the bar's end) */
            ui.cap_len[w] = (uint8_t)(held < 1u || held > 0x7FFFu ? 1u : held > 255u ? 255u : held);
            break;
        }
    }
}
/* THE PHRASE RECORDER (0.17.45, the maker: "I want to be able to play and record a line that is typically longer than the 16 steps, with held
 * notes and spaces"). On JAM, SEL + REC arms it (header ARM); the first key on the pluck, the acid or the bass starts it, from the start of that
 * bar (REC): each key is written on its sixteenth with how long it was held (SEL held: an accent). SEL + REC ends it with that bar (four bars end it
 * anyway); it is handed to the engine on the bar's last sixteenth, so it plays from the very next one, looping, with the voice's own line off
 * (PHR). A phrase of one bar is also written into the line (Capture's way, stored with the piece). SEL + REC lets it go (on JAM or anywhere).
 * (0.17.46, "it's hard bc it's a button combo": on JAM, REC alone does all of it (FREEZE stays REC everywhere else, SEL + REC there is Capture
 * again); ended on the one or just after (the bar's first four sixteenths), it ends with the bar just gone and plays on at once, from that step.) */
static void phrase_key(void)
{
    if (ph_st == 0) {
        if (!en_playing) {
            say("PHRASE", "PLAY FIRST");
            return;
        }
        ph_st = 1;
        say("PHRASE READY", "THE FIRST KEY STARTS IT");
    } else if (ph_st == 1) {
        ph_st = 0;
        say("PHRASE", "OFF");
    } else if (ph_st == 2) {
        const uint32_t bar = en_bars >> 4;
        if (en_drum_pos % EN_NSTEP < 4u && bar > (ph_b0 >> 4)) {
            ph_close((bar - (ph_b0 >> 4)) * 16u, 1);   /* (0.17.46) on the one or just after: it ends with the bar just gone, playing on at once */
            return;
        }
        ph_endb = bar + 1u;
        say("PHRASE", "ENDS WITH THIS BAR");
    } else {
        ph_st = 0;
        en_phrase(-1, 0);
        say("PHRASE", "LET GO: THE LINE IS BACK");
    }
}

static void phrase_tick(void)
{
    uint32_t bar, nb, len;
    if (ph_st != 2)
        return;
    if (!en_playing) {                           /* stopped while writing: gone */
        ph_st = 0;
        return;
    }
    bar = en_bars >> 4;
    nb = bar - (ph_b0 >> 4);                     /* the bars written so far, this one not counted */
    if (!(nb >= 4u || (ph_endb && bar >= ph_endb) || (en_drum_pos % EN_NSTEP == 15 && (nb == 3u || (ph_endb && bar + 1u == ph_endb)))))
        return;                                  /* (on the last sixteenth of the last bar: so it plays from the very next) */
    len = (en_drum_pos % EN_NSTEP == 15 && nb < 4u && !(ph_endb && bar >= ph_endb)) ? (nb + 1u) * 16u : nb * 16u;
    ph_close(len, en_drum_pos % EN_NSTEP != 15);  /* (past the bar's end, the fallback: from where it is) */
}
static void ph_close(uint32_t len, int late)    /* the phrase handed to the engine, len sixteenths; late: from the step it is on */
{
    static const uint8_t VC[4] = {EN_V_PLUCK, EN_V_PAD, EN_V_ACID, EN_V_DRONE};
    uint32_t s;
    int i;
    if (len < 16u)
        len = 16u;
    if (len > EN_PH_N)
        len = EN_PH_N;
    for (i = 0; i < NKEYS; i++)                  /* keys still down: held to the end (one pressed after it, in the next bar: not in it) */
        if (ph_ks[i]) {
            s = ph_ks[i] - 1u;
            if (s < len && en_ph_note[s] == ui.jam_note[i][0])
                en_ph_len[s] = (uint8_t)((en_ph_len[s] & 0x80u) | (len - s > 127u ? 127u : len - s));
            ph_ks[i] = 0;
        }
    for (s = len; s < EN_PH_N; s++)              /* (anything written past it: out) */
        en_ph_note[s] = 0;
    ph_st = 3;
    en_phrase(VC[ph_vc & 3], (int)len | (late ? 0x100 : 0));
    if (len == 16u) {
        jam_capture_v(ph_vc & 3, (int)(ph_b0 & 0xFFF0u), 1);   /* (one bar: kept in the line too, with the piece: 0.17.48, the audit, its own bar, voice
                                                                * and accents, not Capture's busiest bar of the voice on the page) */
        say("PHRASE PLAYS", "AND IS KEPT IN THE LINE");
    } else {
        char t[20] = "PHRASE PLAYS: 2 BARS";
        t[14] = (char)('0' + len / 16u);
        say(t, "REC LETS GO");
    }
}
/* CAPTURE (SEL + REC): the bar you last played on Jam becomes the acid line (frozen, 16 steps: a key on a beat is an accent, a key held past the
 * next step a slide), the pluck's loop (16 slots, Deja vu at 100, the loop from the next bar) or (0.17.31) the bass steps, by the page's voice.
 * (0.17.31, "it doesn't play back as played": each note is kept with the chord it was played over, not the one at SEL + REC; a key a little early
 * counts on the step it was meant for; the acid line plays from the bar's step it was played on.) */
static void jam_capture(void) { jam_capture_v(ui.jam_voice & 3, -1, 0); }   /* the page's voice: its keys are the ones captured */
static void jam_capture_v(int vc, int bar, int selacc)   /* (0.17.48) bar >= 0: that bar (a phrase's), else the busiest of the last four; selacc: accents
                                                         * as SEL gave them (the phrase's), else a key on a beat */
{
    int k, last = -1, s;
    uint32_t bar0;
    if (vc == 1) {
        say("CAPTURE", "THE ACID, PLUCK OR BASS");
        return;
    }
    for (k = 1; k <= 32; k++) {
        const int w = (ui.cap_w - k) & 31;
        if (ui.cap_note[w] && (ui.cap_kv[w] >> 5 & 3) == vc) {
            last = w;
            break;
        }
    }
    if (last < 0 || !en_playing) {
        say("CAPTURE", "PLAY ON JAM FIRST");
        return;
    }
    bar0 = (uint32_t)ui.cap_t[last] & ~15u;       /* the bar the last key fell in, or (0.17.31) of the last four bars the one with the most keys (the latest of
                                                 * equals): a last key on the next bar's one left the phrase behind it out */
    if (bar >= 0) {
        bar0 = (uint32_t)bar;
    } else {
        int best = 0, j;
        for (k = 1; k <= 32; k++) {
            const int w = (ui.cap_w - k) & 31;
            uint32_t b;
            int n = 0;
            if (!ui.cap_note[w] || (ui.cap_kv[w] >> 5 & 3) != vc)
                continue;
            b = (uint32_t)ui.cap_t[w] & ~15u;
            if ((uint16_t)((uint16_t)ui.cap_t[last] - (uint16_t)b) > 63u)
                continue;
            for (j = 0; j < 32; j++)
                if (ui.cap_note[j] && (ui.cap_kv[j] >> 5 & 3) == vc && ((uint32_t)ui.cap_t[j] & ~15u) == b)
                    n++;
            if (n > best) {                      /* (the latest first: a later bar wins a tie) */
                best = n;
                bar0 = b;
            }
        }
    }
    {   /* (0.17.42, "it treats a held note as a single moment") a key held over several steps holds: the bass's HOLD steps, the acid's same note slid
         * into (the 303's tie); held when SEL + REC came (length 0), it holds to the bar's end */
        int hold_to = -1, hn = -1, hd = 0;
        for (s = 0; s < 16; s++) {
            int note = -1, len = 0, dg = 0, ac = 0;
            for (k = 1; k <= 32; k++) {          /* the first key of this voice on this step (the latest events first, so the earliest wins) */
                const int w = (ui.cap_w - k) & 31;
                if (ui.cap_note[w] && (ui.cap_kv[w] >> 5 & 3) == vc && (uint32_t)ui.cap_t[w] == bar0 + (uint32_t)s) {
                    note = ui.cap_note[w];
                    len = ui.cap_len[w];
                    dg = ui.cap_deg[w];
                    ac = selacc ? ui.cap_kv[w] >> 7 : (s & 3) == 0;
                }
            }
            if (note >= 0) {
                hold_to = s + (len ? len : 16) - 1;
                hn = note;
                hd = dg;
            }
            if (vc == 2) {
                if (note >= 0 || s <= hold_to)  /* the note, or the held note again; slid into the next step while the key is still down there */
                    en_acid_capture_d(s, hn, hd, note >= 0 && ac, s < hold_to);
                else
                    en_acid_step(s, 0, 0, 0, 0, 0);
            } else if (vc == 3) {
                if (note >= 0)
                    en_bass_capture_d(s, note, dg, ac, 0);
                else if (s <= hold_to)
                    en_bass_step(s, EN_BS_MAKE(EN_BS_HOLD, 0, 1));
                else
                    en_bass_step(s, 0);
            } else {
                en_pluck_capture(s, note);
            }
        }
    }
    if (vc == 2) {
        knob_set(P_AC_LEN, 16);
        knob_set(P_AC_LOCK, 1);
        say("CAPTURED", "ACID LINE (FROZEN)");
    } else if (vc == 3) {
        knob_set(P_BL_MODE, EN_BL_STEPS);
        edited();
        say("CAPTURED", "BASS STEPS");
    } else {
        knob_set(P_CM_RATE, 3);
        knob_set(P_CM_LOOP, 2);
        knob_set(P_CM_VU, 100);
        say("CAPTURED", "PLUCK LOOP");
    }
}
static void acid_keep(void)                     /* (0.17.31, a Reddit player's "learn what was answered into the track") */
{
    if (proj.par[P_AC_RESP] < 5) {
        say("NO ANSWER SET", "ACID ANSWER: ARP x4");
        return;
    }
    en_acid_keep_answer();
    knob_set(P_AC_LOCK, 1);
    edited();
    say("ANSWER KEPT", "THE ACID LINE, FROZEN");
}
static void draw_jam(void)
{
    const uint32_t pool = pool_abs();
    int w, j;
    char t[16];
    cv_text(6, 2, &FONT_B, JV_NAME[ui.jam_voice & 3], KC_MOVE);
    cv_text(6 + text_w(&FONT_B, JV_NAME[ui.jam_voice & 3]) + 8, 6, &FONT_XS, JM_NAME[ui.jam_mode % 3], K_DIM);
    const uint32_t ch = en_playing ? chord_pcs() : 0u;   /* the chord playing, in its own colour (0.15.21) */
    for (w = 0; w < 16; w++) {                   /* the keyboard: the white keys, then the black keys over them; the scale lit, the chord, the keys held bright */
        const int idx = WHITE_KEY[w], pc = (5 + idx) % 12, held = (int)(ui.keys >> idx & 1u), in = (int)(pool >> pc & 1u);
        cv_rect(8 + w * 14, 30, 13, 66, held ? KC_MOVE : ch >> pc & 1u ? KC_CHORD : in ? mix(K_PANEL, K_PAD, 9) : K_PANEL);
        if (idx == 0 || pc == 0)
            cv_rect(8 + w * 14 + 4, 90, 5, 4, K_TEXT);   /* a C marked */
    }
    for (j = 0; j < 11; j++) {
        const int idx = BLACK_KEY[j], pc = (5 + idx) % 12, held = (int)(ui.keys >> idx & 1u), in = (int)(pool >> pc & 1u);
        int lw = 0;
        for (w = 0; w < 16; w++)
            if (WHITE_KEY[w] == idx - 1)
                lw = w;
        cv_rect(8 + lw * 14 + 9, 30, 10, 42, held ? KC_MOVE : ch >> pc & 1u ? KC_CHORD : in ? K_PAD : K_BG);
        cv_frame(8 + lw * 14 + 9, 30, 10, 42, K_LINE);
    }
    if (ch) {                                    /* the chord's name, top right */
        char nm[16];
        int pcs[EN_NPAD + 1], k;
        pcs[0] = en_chord_root;
        for (k = 0; k < EN_NPAD; k++)
            pcs[k + 1] = en_chord[k] % 12;
        en_chord_name(pcs, EN_NPAD + 1, nm);
        cv_text(234 - text_w(&FONT_B, nm), 2, &FONT_B, nm, KC_CHORD);
    }
    cv_text(6, 104, &FONT_XS, "OCTAVE", K_DIM);
    cv_text(52, 104, &FONT_XS, JO_NAME[ui.jam_oct % 5], K_TEXT);
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
    uint16_t gx_pat, usb;                        /* usb (0.14.5, the field was spare): the USB audio's level, 0 the knob's, 1 fixed; older records read 0 */
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
int ui_restore_valid(uint32_t obj, const uint8_t *b, uint32_t len)
{
    if (obj == OBJ_PROJ || (obj >= OBJ_G1 && obj <= OBJ_G18) || (obj >= OBJ_G19 && obj <= OBJ_G21)) {   /* a genre's pieces (the original genre's may still be the old bank) */
        const int g = obj == OBJ_PROJ ? 0 : obj >= OBJ_G19 ? (int)(obj - OBJ_G19) + 19 : (int)(obj - OBJ_G1) + 1;
        if (obj >= OBJ_NOBJ || g >= EN_NGENRE || len < 20u)
            return 0;
        if (rd32(b) == IMG_MAGIC)
            return len >= IMG_TAIL && b[4] == IMG_VER && b[5] == (uint8_t)g && rd16(b + 10) == IMG_REC;   /* (0.15.20: a bank from before 0.15.6 ends at
                                                 * the records, without the progressions: image_load takes it; this asked for IMG_LEN and refused it) */
        return obj == OBJ_PROJ && (rd32(b) == BANK_MAGIC || rd32(b) == OLD_KIT_MAGIC);
    }
    if (obj == OBJ_GLOB)
        return len >= GLOB_OLD && len <= sizeof(glob_t) && rd32(b) == GLOB_MAGIC;   /* (0.17.61: any older record, as glob_load takes it) */
    if (obj == OBJ_KINDS)                        /* the pieces' extras (0.16.28: PCX1), or the chord types alone (KND1, 0.16.6 .. 0.16.27); fewer genres too */
        return (len >= 4u && len <= 4u + EN_NGENRE * NPIECES * EN_PRK_LEN && (len - 4u) % (NPIECES * EN_PRK_LEN) == 0u && rd32(b) == KND_MAGIC) ||
               (len >= PX_HDR && b[4] >= 4u && b[4] <= PX_LEN && len <= PX_HDR + EN_NGENRE * NPIECES * (uint32_t)b[4] &&
                (len - PX_HDR) % (NPIECES * (uint32_t)b[4]) == 0u && rd32(b) == PX_MAGIC);   /* (0.17.61: or a shorter piece's, as px_load takes it) */   /* (0.15.21: the set list after the old record) */
    if (obj == OBJ_BSTEP)                        /* the pieces' bass steps (0.16.29): this build's, or one of fewer genres */
        return len >= BS_HDR && len <= BS_SIZE && b[4] == BS_LEN && (len - BS_HDR) % (NPIECES * BS_LEN) == 0u && rd32(b) == BS_MAGIC;
    if (obj == OBJ_MMAP)                         /* the player's MIDI map (0.16.11) */
        return mm_ok(b, len);
    if (obj == OBJ_VOICES)
        return len == sizeof(vox_t) && rd32(b) == VOX_MAGIC && rd32(b + 4) <= EN_NUSER;
    if (obj == OBJ_SET) {                        /* as settings_load takes it: this build's record, or an older one with fewer effect settings (0.15.20: only the
                                                 * first was let through, so a backup's settings were refused whenever a build had added an effect setting) */
        uint32_t nf, q;
        if (len < 8u || rd32(b) != SET_MAGIC || rd16(b + 6) != 12u)
            return 0;
        nf = rd16(b + 4);
        q = 8u + nf + 12u + 4u;
        q += q & 1u;
        return (nf == EN_NFXS && len == sizeof(set_t)) || (nf <= EN_NFXS && len < sizeof(set_t) && len >= q + 4u);   /* (0.17.61: as settings_load) */
    }
    return 0;                                    /* (not the panel's calibration: it is this unit's own) */
}
void ui_param_set(int p, int v)               /* the editor (main_fm1.c px_service): a parameter as its knob sets it */
{
    if ((unsigned)p < (unsigned)P_NPARAMS) {
        if (p == P_MODE)                         /* (0.16.19: the scale's notes too, as the ALGORITHM dial sets it; the editor changed the
                                                  * mode's number and left the scale as it was) */
            set_mode(v);
        else
            knob_set(p, p == P_CM_SND ? en_cm_snd_pos(v) : v);   /* (0.17.78: the pluck's sound comes as its number, as ui_param_get gives it;
                                                  * the knob takes a place on its list: from 42 up the two differ, and the Editor and the
                                                  * link's sounds landed on another voice) */
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
    s.uiv[11] = (uint8_t)(ui.min_ch | (ui.viz == VZ_THEORY ? 1u : ui.viz == VZ_TRIP ? 2u : 0u) << 5);   /* (the spare byte: 0 in an older record, all channels; bits 5-6, 0.16.10: THEORY 1, TRIP 2, whose 7 in set_t.viz reads as AUTO in older builds) */
    s.gx_preset = ui.gx_preset;
    s.gx_tgt = ui.gx_tgt;
    s.pm_tgt = ui.pm_tgt;
    s.gx_pat = ui.gx_pat;
    s.usb = (uint16_t)(ui.usb_fixed | (ui.scan & 1) << 2 |   /* (bit 1 was the DX set chosen: written 0 and not read since 0.17.75, the DY set always) */ (ui.dj_rounds & 7) << 3 | (ui.dj_ramp & 7) << 6 | (ui.intro & 7) << 9 | (ui.midi_map & 7) << 12 | (ui.min_pads == 1 || ui.min_pads == 4) << 15);
    s.viz = (uint8_t)((ui.viz >= VZ_TRIP ? 7u : ui.viz) | (ui.min_pads == 2 || ui.min_pads == 4) << 3 | (ui.cc_mode & 3) << 4 | (ui.min_pads == 3) << 6 | ui.centre_imp << 7);   /* (bits 3-6 free before 0.15.22: older records read 0) */
    if (plat_store_save(OBJ_SET, &s, sizeof s) == 0) {
        set_disk = s;
        set_have = 1;
        ui.set_lazy = 0;
    }
}
static void settings_load(void)
{
    set_t s;
    uint8_t raw[sizeof(set_t)];
    int p, c, n = plat_store_load(OBJ_SET, raw, sizeof raw);
    if (n >= 8 && n < (int)sizeof s && raw[0] == 'S' && raw[1] == 'E' && raw[2] == 'T' && raw[3] == '1' && (raw[4] | raw[5] << 8) <= EN_NFXS && (raw[6] | raw[7] << 8) == 12) {
        /* a record from before the newest effect settings (fewer fxs, 0.14.10 on): its fields by their old places, the new settings at their defaults
         * (0.17.61, the formats locked: or as many, and a field added after usb later: shorter, that field 0) */
        const int old = raw[4] | raw[5] << 8;
        int q = 8, i;
        memset(&s, 0, sizeof s);
        s.magic = SET_MAGIC;
        s.nfxs = EN_NFXS;
        s.nui = 12;
        for (i = 0; i < EN_NFXS; i++)
            s.fxs[i] = (uint8_t)EN_FXS_DEF[i];
        for (i = 0; i < old; i++)
            s.fxs[i] = raw[q++];
        memcpy(s.uiv, raw + q, 12);
        q += 12;
        s.gx_preset = raw[q++];
        s.gx_tgt = raw[q++];
        s.pm_tgt = raw[q++];
        s.viz = raw[q++];
        if (q & 1)
            q++;
        if (q + 4 > n)
            return;
        s.gx_pat = (uint16_t)(raw[q] | raw[q + 1] << 8);
        s.usb = (uint16_t)(raw[q + 2] | raw[q + 3] << 8);
    } else if (n == (int)sizeof s) {
        memcpy(&s, raw, sizeof s);
        if (s.magic != SET_MAGIC || s.nfxs != EN_NFXS || s.nui != 12)
            return;
    } else {
        return;
    }
    set_disk = s;
    set_have = 1;
    for (p = 0; p < NFXP; p++)
        for (c = 0; c < 4; c++) {
            const int k = K_FXP + 4 * p + c, kind = FXP[p].c[c].kind;
            if (kind == KN_NONE || kind == KN_PRESET || kind == KN_PAR)   /* (KN_PAR: the project's, not a setting) */
                continue;
            if (FXP[p].bit == 63) {              /* the UI's own (not the genre: 11) */
                if (FXP[p].c[c].idx < 11)
                    fxp_set(k, s.uiv[FXP[p].c[c].idx]);
                else if (FXP[p].c[c].idx == 12)
                    fxp_set(k, s.usb & 1);
                else if (FXP[p].c[c].idx == 13)
                    fxp_set(k, !(s.usb >> 1 & 1));
                else if (FXP[p].c[c].idx == 14)
                    fxp_set(k, (s.usb >> 2 & 1) | (glob.sl_rsv[2] >> 3 & 1u) << 1);   /* (Look: the scanlines here, Calm the global record's bit 3, 0.17.40) */
                else if (FXP[p].c[c].idx >= 15 && FXP[p].c[c].idx <= 17)
                    fxp_set(k, s.usb >> (3 + 3 * (FXP[p].c[c].idx - 15)) & 7);
                else if (FXP[p].c[c].idx == 18)
                    fxp_set(k, s.usb >> 12 & 7);
                else if (FXP[p].c[c].idx == 19)
                    fxp_set(k, (s.uiv[11] & 0x1Fu) <= 16u ? s.uiv[11] & 0x1Fu : 0);
                else if (FXP[p].c[c].idx == 20)
                    fxp_set(k, (s.viz >> 6 & 1u) ? 3 : (s.viz >> 3 & 1u) && (s.usb >> 15 & 1u) ? 4 : (s.viz >> 3 & 1u) ? 2 : s.usb >> 15 & 1);   /* (CHORDS: both, 0.17.31) */   /* (M:S in set_t.viz's bit 3, 0.15.22; MPK bit 6, 0.15.23) */
                else if (FXP[p].c[c].idx == 22)
                    fxp_set(k, s.viz >> 4 & 3u);
            } else if (kind == KN_TGT) {
                fxp_set(k, s.gx_tgt);
            } else if (kind == KN_PTGT) {
                fxp_set(k, s.pm_tgt);
            } else {
                fxp_set(k, s.fxs[FXP[p].c[c].idx]);
            }
        }
    if (ui.fxs[EN_FXS_LK_MODE] >= 4)             /* (0.17.55: the link CABLE is never kept: a power-on is plain USB, so the installer always finds the unit) */
        ui.fxs[EN_FXS_LK_MODE] = 0;
    {   /* (0.17.30) the acid answer's level is on no settings page (the loop above passed it by): it came back at its default after a power-off */
        const int a = s.fxs[EN_FXS_AC_ANS] > 3 ? 3 : s.fxs[EN_FXS_AC_ANS];
        ui.fxs[EN_FXS_AC_ANS] = (uint8_t)a;
        en_fxs(EN_FXS_AC_ANS, a);
    }
    if (s.gx_preset < NGXP) {                    /* the gate's pattern, after its length (which says "the keys' own") */
        ui.gx_preset = s.gx_preset;
        ui.gx_pat = s.gx_pat;
        en_gate_pat(ui.gx_pat);
    }
    ui.viz = (uint8_t)((s.viz & 7u) == 7u ? ((s.uiv[11] >> 5 & 3u) == 1u ? VZ_THEORY : (s.uiv[11] >> 5 & 3u) == 2u ? VZ_TRIP : VZ_AUTO) : s.viz & 7u);
    ui.centre_imp = (uint8_t)(s.viz >> 7);
    ui.set_dirty = 0;
}

/* what's left waiting for flash (a store is written at once, store_now): stopped, all of it after a
 * quiet moment; playing, the player's three (theme, Tune, MIDI out), with a moment's gap */
static void autosave(void)
{
    uint32_t now = plat_ms();
    {   /* START (0.16.26): the piece playing, kept for the next power-on once it has stayed 20 s (a write of the global record: only when it costs the
         * music nothing, stopped or with the flash's seamless erase) */
        static uint8_t pl_now;
        static uint32_t pl_t;
        const int pl = proj.genre * NPIECES + proj.piece + 1;
        if (pl != pl_now) {
            pl_now = (uint8_t)pl;
            pl_t = now;
        } else if (pl != project_place() && now - pl_t >= 20000u && (!en_playing || plat_store_seamless()))
            project_place_set(pl);
    }
    if (ui.vox_dirty && !ui.btn && !ui.keys && !ui.store_wait && now - ui.act_t >= (en_playing ? 2 * GLOB_QUIET : GLOB_QUIET)) {
        ui.vox_dirty = 0;
        quiet_write(3);                          /* the player's DX7 voices: kept (playing: faded) */
        return;
    }
    if (ui.set_lazy && !en_playing)
        ui.set_dirty = 1;                        /* (stopped: a write costs the music nothing) */
    if (ui.set_dirty && !ui.btn && !ui.keys && !ui.store_wait && now - ui.act_t >= (en_playing ? 10 * GLOB_QUIET : GLOB_QUIET)) {   /* (playing: 20 s, 0.17.56: each write
                                                                                                                                 * stops the flash for 0.2-0.36 s, its slices
                                                                                                                                 * hold the audio's interrupt 1.2 ms each; the
                                                                                                                                 * maker's log: a store every 5-10 s while turning
                                                                                                                                 * the filters, heard as hitches. Stop: 2 s) */
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
 * colorway of the logo concept), the name in two colours, a ringed bar and "PRESENTS". Then (0.17.79) the maker's festival picture,
 * PERF-1 in big pixel letters under it, and the licence (GPL-3.0), with sixteen steps filling in (the white keys lighting with them) until the
 * music starts. Any button or key: straight to the music.
 * Sprite letters: K outline, H horn, h horn shade, Y eye, R body, r body shade, L highlight, T tail, . nothing. */
static const char *const MASCOT[23] = {   /* (0.17.39 re-art: lit from the top left; k its own dark outline, D r R L W the body's ramp (shade to light, hue-shifted),
                                           * g h H the horns, E a glint, A a rim of the theme's accent on the side away from the light, K the dark inside) */
    "..g............g.......",
    ".gEg..........gEg......",
    "..gHg........gHg.......",
    "..ghg.kkkkkk.ghg.......",
    "...ghkWLLRRrkhg........",
    "..kLWLRRRRRRRRAk.......",
    ".kLRRKKRRRRKKRrAk......",
    ".kLRREYRRRrEYrrAk......",
    ".kRRrKRRRRRrKrrAk......",
    ".kDrrKHKKKKHKrDAk...k..",
    "..kKKKKKKKKKKKKk...kLk.",
    "..kWLLRKKKKDDDDk..kLRrk",
    "..kDDDDLRRrKKKK...kkkkk",
    "...kKKKDDDDRRrAk....T..",
    "..kWLLRKKKKDDDDk....T..",
    "..kDDDDLRRrKKKK.....T..",
    "...kKKKDDDDRRrAk...T...",
    "..kWLLRKKKKDDDDk..T....",
    "..kDDDDLRRrKKKK...T....",
    "...kKKKDDDDRRrAkTT.....",
    "..kWLLRKKKKDDDDk.......",
    "..kDDDDLRRrKKKKk.......",
    "...kkkkkkkkkkkk........"};
static uint16_t mascot_color(char ch)
{
    if (LOOK_SYNTH)                              /* neon tube light: a magenta outline, chrome horns, cyan eyes, the body dark */
        switch (ch) {
        case 'K': case 'T': case 'k': case 'g': return RGB(255, 47, 142);
        case 'H': case 'E': return RGB(255, 255, 255);
        case 'h': return RGB(159, 220, 255);
        case 'Y': case 'A': return RGB(62, 240, 255);
        case 'R': return RGB(58, 11, 48);
        case 'r': case 'D': return RGB(36, 6, 31);
        case 'L': case 'W': return RGB(255, 122, 209);
        default: return 0;
        }
    switch (ch) {
    case 'K': return RGB(27, 16, 34);
    case 'k': return RGB(72, 16, 46);            /* (the outline: the body's own colour, darkest) */
    case 'D': return RGB(112, 24, 66);           /* (the ramp leans purple into the shade, orange into the light) */
    case 'r': return RGB(166, 32, 66);
    case 'R': return RGB(226, 56, 78);
    case 'L': return RGB(255, 118, 106);
    case 'W': return RGB(255, 192, 152);
    case 'T': return RGB(120, 26, 58);
    case 'g': return RGB(98, 72, 66);
    case 'h': return RGB(190, 160, 120);
    case 'H': return RGB(242, 230, 196);
    case 'E': return RGB(255, 252, 238);
    case 'Y': return RGB(255, 212, 71);
    case 'A': return th->acc;                    /* (the rim light) */
    default: return 0;
    }
}

static void splash_mascot(int hop, int blink)    /* the mascot, hopped up hop pixels, its eyes shut when blinking */
{
    int x, y;
    cv_begin(240, 108, K_BG);
    aa_glow(120.0f, 62.0f - (float)hop, 22.0f, 70.0f, RGB(156, 29, 58), 0.6f);   /* (0.17.39) a faint glow behind him, rising with him */
    for (y = 0; y < 23; y++)
        for (x = 0; x < 23; x++)
            if (MASCOT[y][x] != '.')
                cv_rect(120 - 23 * 2 + x * 4, 108 - 23 * 4 + y * 4 - hop, 4, 4, mascot_color(blink && y > 4 && (MASCOT[y][x] == 'Y' || MASCOT[y][x] == 'E') ? 'K' : MASCOT[y][x]));
    cv_blit(0, 0);
}

static void draw_splash(void)
{
    int i;
    lcd_fill(0, 0, 240, 240, K_BG);
    splash_mascot(0, 0);
    cv_begin(240, 132, K_BG);                    /* the name, the rings, and what's next */
    for (i = 2; i >= 0; i--) {                   /* (0.17.39) block letters: two darker copies down and right under each word */
        text_c(120 + i, i, &FONT_L, "DEAMON", i ? mix(RGB(150, 40, 80), K_BG, i * 4) : RGB(242, 230, 196));
        text_c(120 + i, 36 + i, &FONT_L, "SLINKY", i ? mix(RGB(150, 40, 80), K_BG, i * 4) : RGB(255, 143, 143));
    }
    cv_panel(40, 80, 160, 18, K_BG, 1);           /* the bar set into the card, its segments lit on top and shaded under */
    for (i = 0; i < 9; i++) {
        const uint16_t c = i & 1 ? RGB(224, 52, 79) : RGB(156, 29, 58);
        cv_rect(48 + i * 16, 86, 12, 6, c);
        cv_rect(48 + i * 16, 86, 12, 1, mix(c, RGB(255, 220, 190), 7));
        cv_rect(48 + i * 16, 91, 12, 1, mix(c, 0x0000, 7));
    }
    text_c(120, 110, &FONT_XS, "PRESENTS", K_DIM);
    cv_blit(0, 108);
}

/* the second card (0.17.79): the maker's festival picture (boot_art.h: 240 x 162, sixteen colours, run-length coded; tools/boot_art.py),
 * then under it PERF-1 in big 5 x 7 pixel letters (4 px a pixel), an orange shadow under the purple, and who made it */
#include "boot_art.h"
#define BIG_N 6
#define BIG_PX 4
static const uint8_t BIG_GLYPH[BIG_N][7] = {
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  /* P */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  /* E */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},  /* R */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  /* F */
    {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00},  /* - */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* 1 */
};
#define SPLASH_STEP_Y 228
static void draw_hyp(void)
{
    const uint8_t *p = BOOT_RLE;
    uint32_t y = 0, band;
    int g, r, c, pass;
    for (band = 0; band < 2u; band++) {          /* the picture, in two canvases (a canvas is at most 144 rows) */
        const uint32_t h = band ? BOOT_H - BOOT_H / 2u : BOOT_H / 2u;
        uint16_t *q, *e;
        cv_begin(BOOT_W, h, K_BG);
        for (q = cv_px, e = cv_px + BOOT_W * h; q < e;) {   /* a byte a run: the colour low, the length less one high */
            const uint32_t b = *p++;
            const uint16_t col = swap16(BOOT_PAL[b & 15u]);
            uint32_t n = (b >> 4) + 1u;
            while (n-- && q < e)
                *q++ = col;
        }
        cv_blit(0, y);
        y += h;
    }
    cv_begin(240, 240 - BOOT_H, K_BG);
    for (pass = 0; pass < 2; pass++)             /* the shadow, then the letters */
        for (g = 0; g < BIG_N; g++)
            for (r = 0; r < 7; r++)
                for (c = 0; c < 5; c++)
                    if (BIG_GLYPH[g][r] >> (4 - c) & 1u)
                        cv_rect(50 + (g * 6 + c) * BIG_PX + (pass ? 0 : 2), 5 + r * BIG_PX + (pass ? 0 : 2), BIG_PX, BIG_PX,
                                pass ? K_PAD : K_ACC);
    text_c(120, 38, &FONT_XS, EN_VERSION[0] == 'D' ? "FM-1 CUSTOM FIRMWARE" : "FM-1 CUSTOM FIRMWARE  //  " EN_VERSION, K_TEXT);
    text_c(120, 52, &FONT_XS, "BY DEAMONSLINKY  //  GPL-3.0", RGB(255, 143, 143));   /* (1.0: the credits are in the manual) */
    cv_blit(0, BOOT_H);
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

/* THE SAMPLE KITS YOU LOAD (0.17.38): three slots in the flash (plat.h PLAT_RAW_*), five sectors each, written from the installer page a sector at a time
 * (backup_proto.h BK_RAW0: the backup protocol's chunks), only while the music is stopped. A slot is a header (ukit_*: "SKU1", its name, its six sounds'
 * places, lengths, first ADPCM index and level) then the sounds, 4-bit IMA ADPCM at 22.05 kHz; it plays only when its CRC checks. */
#define UKIT_SECT 5u
static uint32_t ukit_crc(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static void ui_kits_load(void)
{
    int u;
    for (u = 0; u < EN_NUKIT; u++) {
        const uint8_t *b = plat_raw_ptr(PLAT_RAW_LO + (uint32_t)u * UKIT_SECT * 4096u);
        uint32_t tot = b ? (uint32_t)b[20] | (uint32_t)b[21] << 8 | (uint32_t)b[22] << 16 | (uint32_t)b[23] << 24 : 0u;
        const uint32_t crc = b ? (uint32_t)b[24] | (uint32_t)b[25] << 8 | (uint32_t)b[26] << 16 | (uint32_t)b[27] << 24 : 0u;
        const int ok = b && b[0] == 'S' && b[1] == 'K' && b[2] == 'U' && b[3] == '1' && tot > 128u && tot <= UKIT_SECT * 4096u &&
                       ukit_crc(b + 128, tot - 128u) == crc;
        en_user_kit(u, ok ? b : 0, ok ? tot : 0u);
    }
}
int ui_kit_read(uint32_t sect, uint8_t *dst)     /* a slot's sector as it is (a backup) */
{
    const uint8_t *b = plat_raw_ptr(PLAT_RAW_LO + sect * 4096u);
    uint32_t k;
    if (!b)
        return 0;
    for (k = 0; k < 4096u; k++)
        dst[k] = b[k];
    return 4096;
}
int ui_kit_write(uint32_t sect, const uint8_t *src, uint32_t len)   /* 0 written; 3 not (playing, or the flash said no) */
{
    if (en_playing) {
        say("SAMPLE KIT", "STOP FIRST");
        return 3;
    }
    en_hush(1);
    if (plat_raw_write(PLAT_RAW_LO + sect * 4096u, src, len) != 0) {
        en_hush(0);
        say("SAMPLE KIT", "NOT WRITTEN");
        return 3;
    }
    ui_kits_load();
    en_hush(0);
    if (sect % UKIT_SECT == 0u) {                /* (the header goes last: the kit is whole) */
        char t[16] = "KIT LOADED ";
        t[11] = (char)('1' + sect / UKIT_SECT);
        t[12] = 0;
        say(t, EN_KIT_NAME[EN_NKIT + EN_NSKIT_BI + (int)(sect / UKIT_SECT)]);
    }
    return 0;
}

void ui_init(void)
{
    if (rm.on)
        en_mute(0);
    memset(&rm, 0, sizeof rm);
    memset(&ui, 0, sizeof ui);
    rs_state = 0;
    ui.pat_key = -1;                             /* (no pattern key held) */
    ui.gr_all = 50;
    ui.smp_gap = 1;                              /* (the SAMPLE page: a second between sounds; none playing) */
    ui.smp_cur = -1;
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
        ui.fm_dy = 1;                          /* the DY voices (0.14.21; always since 0.17.75: only its bit in the stored settings is left) */
    }
    settings_load();                             /* the effects' and performing settings as you left them */
    voices_load();                               /* and the player's DX7 voices */
    ui_kits_load();                              /* (0.17.38) and the sample kits loaded into the flash */
    mm_load();                                   /* and their MIDI map (0.16.11) */
    en_pedal(glob.sl_rsv[2] & 3);                /* and the pedal (0.16.21: the global record's last spare byte) */
    ui_fx_apply();                               /* the piece's FILTERS and SCREAM over the settings' (0.16.28) */
    ui.start_rand = (uint8_t)project_start_random();
    ui.form = (uint8_t)(glob.sl_rsv[2] >> 2 & 1u ? 3u : (glob.sl_rsv[2] >> 4 & 3u) <= 2u ? glob.sl_rsv[2] >> 4 & 3u : 0u);   /* and Form (0.16.26: bit 2 Loop;
                                                 * 0, the records before it, is Song; 0.17.70: bits 4-5 Few rests 1, No rests 2) */
    en_song = (uint8_t)(ui.form != 3);
    en_rests = (uint8_t)(ui.form < 3 ? 2 - ui.form : 2);
    ui.sl_pos = (uint8_t)sl_find();              /* the set list: the slot of the piece playing (0xFF none) */
    ui.touched = -1;
    ui.boot_t = plat_ms();
    ui.theme = (uint8_t)proj.par[P_THEME];
    th = &THEMES[ui.theme < EN_NTHEME ? ui.theme : 0];
    ui.splash = ui_splash_ms > 0u;
    if (ui.splash) {
        draw_splash();
    } else {
        lcd_fill(0, 0, 240, 240, K_BG);
        if (!en_playing) {
            track_fresh(1, 1);
            af_fresh = 1;
            sl_start();
            en_play(1);                          /* it plays from the start */
        }
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
        if (!en_playing) {
            track_fresh(1, 1);
            af_fresh = 1;
            sl_start();
            en_play(1);
        }
    }
    if (ui.theme != (uint8_t)proj.par[P_THEME]) {    /* a new theme: everything again */
        ui.theme = (uint8_t)proj.par[P_THEME];
        th = &THEMES[ui.theme < EN_NTHEME ? ui.theme : 0];
        ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
        lcd_fill(0, 0, 240, 240, K_BG);
    }
    store_tick(0);
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
    af_tick();
    mix_tick();
    smp_tick();
    key_tick();
    glide_tick();
    gesture_tick();
    phrase_tick();
    lk_tick();
    ci_tick();
    swell_tick();
    factory_all_tick();
    saver_tick();
    {   /* the load: a half that was never rendered (a real dropout) or renders that keep running past their half (the margin is gone) make the engine drop the enhancers
         * (level 1, 0.17.61: the choruses, the resonator, the pluck's taps; the maker: "the fm does sound better with all ops active"), then hold the DX voices to four,
         * then two and give up the Echoplex and the vinyl; and it eases back a step after twenty seconds of neither. Level 1 comes early (40 late renders in ten
         * seconds: the margin thin), before a dropout; the next ones only on a dropout or a quarter of the renders late. One step at a time (four seconds
         * after the last on a dropout, ten on the other: a step needs time to show; one overrun used to climb all three at once, the peak of the last second still reading 100 %).
         * (A single late render is not a dropout: the next one starts at once. The average CPU and the peaks were the gauge before, but they sit high in the heavy pieces.) */
        static uint32_t calm_t, step_t, last_xr, last_late, win_t, win_n, eased_t, need = 20000u;   /* (0.17.26) need: the calm an ease-back waits for */
        const uint32_t now = plat_ms(), xr = plat_xruns(), lt = plat_late();
        const int lost = xr != last_xr;
        win_n += lt - last_late;
        last_xr = xr;
        last_late = lt;
        if (!en_playing) {
            calm_t = step_t = win_t = now;
            win_n = 0;
            need = 20000u;                         /* (a new start: the short wait again) */
            eased_t = 0;
        } else {
            static int last_pc = -1;
            const int pc = proj.genre * 16 + proj.piece;
            if (pc != last_pc) {                   /* (another piece: the short wait again) */
                last_pc = pc;
                need = 20000u;
                eased_t = 0;
            }
            if (now - win_t > 10000u) {             /* the late renders of the last ten seconds */
                win_t = now;
                win_n = 0;
            }
            const int warm = lost || win_n >= 40u;   /* (0.17.61) the margin thin: level 1 (the enhancers), and no easing back from it */
            if (warm)
                calm_t = now;
            if (lost || win_n >= 200u || (warm && en_shed_level == 0)) {   /* (0.17.59: 200, a quarter of the renders late: since 0.17.56 a late render that lost sound is an xrun, so this is only the backstop. The
                                                   * maker's 03:41 log: TRANCE 01 shed with the FX in use and no xrun at all. 0.17.54: 40, and 6 before that: the
                                                   * maker's TRANCE log had 62 in 14 minutes, in bursts of up to 10 in ten seconds, no dropout, and sat at level 2 (the DX at
                                                   * two voices, no chorus or resonator) 87 % of the time.) */
                calm_t = now;
#if defined(EN_DIAG) && EN_DIAG
                if (0) {                         /* (0.17.64) the diagnostic builds never ease off: their runs compare the same work */
#else
                if (en_shed_level < 3 && now - step_t > (lost ? 4000u : 10000u)) {
#endif
                    en_shed(en_shed_level + 1);
                    step_t = now;
                    win_n = 0;
                    if (eased_t && now - eased_t < 30000u && need < 160000u)   /* (0.17.26) the last ease-back did not hold: wait twice as long next time,
                                                                                   * so a piece that needs a step keeps it (it dropped out every 20-30 s) */
                        need *= 2u;
                    eased_t = 0;
                }
            } else if (now - calm_t > need && en_shed_level > 0) {
                en_shed(en_shed_level - 1);
                calm_t = eased_t = now;
            }
        }
    }
    {   /* the JAM page plays one voice by hand: its own line stops (the engine's hand switch, not the piece's setting), and comes back when you leave */
        static const uint8_t MAN[4] = {P_CM_MAN, P_PAD_MAN, P_AC_MAN, P_DR_MAN};
        const int want = (ui.view == V_JAM ? 1 << (ui.jam_voice & 3) : 0) | (ph_st == 3 ? 1 << (ph_vc & 3) : 0);   /* (0.17.45: a mask; a phrase's voice too) */
        if (want != ui.jam_man) {
            int k;
            for (k = 0; k < 4; k++)
                if ((ui.jam_man ^ want) >> k & 1)
                    en_set(MAN[k], want >> k & 1 ? 1 : proj.par[MAN[k]]);
            ui.jam_man = (uint8_t)want;
        }
    }
    autosave();
    cv_scan = ui.scan & 1u;                     /* (Look's bit 0; bit 1 is Calm) */
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
