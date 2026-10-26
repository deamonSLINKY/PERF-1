/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The six-operator FM voices of the counter-melody (and the JAM page): a polyphonic DX7 on Melodee's core (fm6_core.inc), twenty-four
 * patches (Melodee's factory voices), played from the pluck's sequencer and by hand. The control path (envelopes, LFO, pitch, the
 * routing) follows Melodee's eng_fm6.c without its controllers, portamento, MONO and per-track settings; the voice handling is
 * small: six voices, the oldest is taken. Rendered in Dexed's 64-sample blocks and queued across chunks, by the other core, a chunk ahead of
 * the first core's use of them (amb.c: fm_job / render_fm; the notes and gates are set by the first core between chunks).
 *
 * Included by amb.c, after EN_POOL, the fast maths, par[] and mtof. */
#define MELODEE_DUAL_CORE 0
#define MELODEE_FM6_FAST 1
#define CTL FM6_N
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "fm6_tables.h"
#include "fm6_core.inc"
#include "fm6_patches.h"         /* FM6_NFACTORY, FM6_FACTORY[8][128] */

/* a Melodee factory voice: OP1..OP6 in patch order within an operator (DET 0..14, 7 = none), ALG 0..31, TRNSP 24 = none */
typedef struct {
    uint8_t op[6][FP_OP];
    uint8_t pr[4], pl[4], alg, fb, oks, lfs, lfd, lpmd, lamd, lfks, lfw, lpms, trnsp;
    char name[11];
} fm6_rom_t;
#include "fm6_rom.h"             /* FM6_ROM[]: sixteen more */
#define FM6_NPATCH (FM6_NFACTORY + FM6_NROM)
#define FM6_PACKED 128u

static uint32_t fm6_max(uint32_t i)             /* the highest value of each byte of the 155-byte voice */
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t VMAX[19] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48};
    if (i < 126u)
        return OPMAX[i % 21u];
    if (i < FP_NAME)
        return VMAX[i - 126u];
    return 126u;
}

static void fm6_sanitize(uint8_t *v)            /* every value inside its range (a name byte outside 32..126 becomes a space) */
{
    uint32_t i;
    for (i = 0; i < FP_SIZE; i++) {
        if (i >= FP_NAME)
            v[i] = v[i] < 32u || v[i] > 126u ? ' ' : v[i];
        else if (v[i] > fm6_max(i))
            v[i] = (uint8_t)fm6_max(i);
    }
    v[FP_SIZE] = 0;
}

static void fm6_unpack(const uint8_t *b, uint8_t *v)   /* 128-byte packed record (VMEM) -> 155-byte voice */
{
    uint32_t k, i;
    for (k = 0; k < 6u; k++) {
        const uint8_t *o = b + k * 17u;
        uint8_t *d = v + k * FP_OP;
        for (i = 0; i < 11u; i++)
            d[i] = o[i] & 0x7Fu;
        d[FP_LC] = o[11] & 3u;
        d[FP_RC] = (o[11] >> 2) & 3u;
        d[FP_RS] = o[12] & 7u;
        d[FP_DET] = (o[12] >> 3) & 15u;
        d[FP_AMS] = o[13] & 3u;
        d[FP_KVS] = (o[13] >> 2) & 7u;
        d[FP_OL] = o[14] & 0x7Fu;
        d[FP_MODE] = o[15] & 1u;
        d[FP_FC] = (o[15] >> 1) & 31u;
        d[FP_FF] = o[16] & 0x7Fu;
    }
    for (i = 0; i < 9u; i++)
        v[FP_PR1 + i] = b[102 + i] & 0x7Fu;              /* pitch EG, algorithm */
    v[FP_ALG] &= 31u;
    v[FP_FB] = b[111] & 7u;
    v[FP_OKS] = (b[111] >> 3) & 1u;
    for (i = 0; i < 4u; i++)
        v[FP_LFS + i] = b[112 + i] & 0x7Fu;
    v[FP_LKS] = b[116] & 1u;
    v[FP_LFW] = (b[116] >> 1) & 7u;
    v[FP_LPMS] = (b[116] >> 4) & 7u;
    v[FP_TRNSP] = b[117] & 0x7Fu;
    for (i = 0; i < 10u; i++)
        v[FP_NAME + i] = b[118 + i] & 0x7Fu;
    fm6_sanitize(v);
}

static void fm6_rom_patch(const fm6_rom_t *r, uint8_t *v)   /* a Melodee factory voice -> 155-byte voice */
{
    uint32_t n, i;
    for (n = 1; n <= 6u; n++)
        for (i = 0; i < FP_OP; i++)
            v[(6u - n) * FP_OP + i] = r->op[n - 1u][i];
    for (i = 0; i < 4u; i++) {
        v[FP_PR1 + i] = r->pr[i];
        v[FP_PL1 + i] = r->pl[i];
    }
    v[FP_ALG] = r->alg;
    v[FP_FB] = r->fb;
    v[FP_OKS] = r->oks;
    v[FP_LFS] = r->lfs;
    v[FP_LFD] = r->lfd;
    v[FP_LPMD] = r->lpmd;
    v[FP_LAMD] = r->lamd;
    v[FP_LKS] = r->lfks;
    v[FP_LFW] = r->lfw;
    v[FP_LPMS] = r->lpms;
    v[FP_TRNSP] = r->trnsp;
    for (i = 0; i < 10u; i++)
        v[FP_NAME + i] = (uint8_t)(r->name[i] ? r->name[i] : ' ');
    fm6_sanitize(v);
}

/* the LFO (Lfo::getsample, getdelay): one step a 64-sample block */
typedef struct {
    uint32_t ph, dly;                            /* LFO phase; delay ramp (DX7 two-slope) */
    int32_t val, depth;                          /* Q24, 0..1 << 24 */
    uint8_t rnd;
} fm6_lfo_t;

static void fm6_lfo_step(fm6_lfo_t *l, const uint8_t *ed)
{
    uint32_t inc = FM6_LFO_INC[ed[FP_LFS]], ph = l->ph + inc, a = 99u - (uint32_t)ed[FP_LFD], d1, d2, d;
    int32_t x;
    l->ph = ph;
    switch (ed[FP_LFW]) {
    case 0:                                      /* triangle */
        x = (int32_t)((ph >> 7) ^ (uint32_t)-(int32_t)(ph >> 31)) & ((1 << 24) - 1);
        break;
    case 1:                                      /* saw down */
        x = (int32_t)((~ph ^ (1u << 31)) >> 8);
        break;
    case 2:                                      /* saw up */
        x = (int32_t)((ph ^ (1u << 31)) >> 8);
        break;
    case 3:                                      /* square */
        x = (int32_t)(((~ph) >> 7) & (1u << 24));
        break;
    case 4:                                      /* sine */
        x = (int32_t)((0.5f + fm6_sin(ph >> 8) * 0.5f) * 16777216.0f);
        break;
    default:                                     /* sample & hold */
        if (ph < inc)
            l->rnd = (uint8_t)(l->rnd * 179u + 17u);
        x = ((l->rnd ^ 0x80) + 1) << 16;
        break;
    }
    l->val = x;
    if (a == 99u) {                              /* DELAY 0 */
        d1 = d2 = 0xFFFFFFFFu;
    } else {
        a = (16u + (a & 15u)) << (1u + (a >> 4));
        d1 = FM6_LFO_UNIT * a;
        d2 = FM6_LFO_UNIT * (a & 0xFF80u ? a & 0xFF80u : 0x80u);
    }
    d = l->dly + (l->dly < (1u << 31) ? d1 : d2);
    if (d < l->dly) {                            /* past the top */
        l->depth = 1 << 24;
        return;
    }
    l->dly = d;
    l->depth = d < (1u << 31) ? 0 : (int32_t)((d >> 7) & ((1u << 24) - 1u));
}

/* the operators' pitch, level and rate scaling for the voice's note and velocity (Dx7Note::init / update) */
static void fm6_keyed(fm6_voice_t *s, const uint8_t *ed, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = &ed[k * FP_OP];
        s->ol[k] = (int16_t)fm6_outlevel(op, note, s->vel);
        s->rs[k] = (int8_t)fm6_rscale(op, note);
        s->base[k] = fm6_logfreq(op, note);
    }
}

/* ------------------------------------------------------------- the voices --- */
#define FM_NV 6                                  /* six voices: each is up to 6 operators a sample (the other core renders them, a chunk ahead) */
typedef struct {
    fm6_voice_t s;
    uint8_t used, gate;
    uint32_t age;                                /* the order the notes came in: the oldest is taken */
    int32_t gate_left;                           /* samples until the key goes up */
    int32_t kill_left;                           /* after the key is up: samples the note may ring on, then it is faded out (the heavy patches ring for seconds) */
    float fade;                                  /* 1, falling to 0 when it is being faded out */
} fm_slot_t;
static fm_slot_t fmx[FM_NV];
static void fm_zero(void *p, uint32_t n)         /* (the unit has no libc) */
{
    uint8_t *b = (uint8_t *)p;
    while (n--)
        *b++ = 0;
}
static uint8_t fm_ed[FP_SIZE + 1u];              /* the patch all voices play */
static int fm_cur = -1;
static fm6_lfo_t fm_lfo;
static uint32_t fm_age;
static uint32_t fm_ams_amd, fm_ams_pt[4];
static uint8_t fm_ams_ok;
static float fm_blk[FM6_N] EN_POOL;              /* a block of the voices' sum, and the place in it */
static uint32_t fm_pos = FM6_N;
#ifndef FM_ENG
#define FM_ENG FM6_MARK1
#endif
static uint32_t fm_engine = FM_ENG;           /* the DX7's own log-sine and exponent tables (MODERN: plain fp32; OPL) */
static int32_t fm_tune;                          /* the global tune in cents, as a Q24 log */
static int fm_inited;

static void fm_init(void)                        /* the tables, once, before anything renders */
{
    if (!fm_inited) {
        fm6_tables_init();
        fm_inited = 1;
    }
}

static void fm_reset(void)                       /* a new engine: no voices, no patch, the block queue empty */
{
    int k;
    for (k = 0; k < FM_NV; k++) {
        fmx[k].used = 0;
        fm_zero(&fmx[k].s, sizeof fmx[k].s);
    }
    fm_zero(&fm_lfo, sizeof fm_lfo);
    fm_cur = -1;
    fm_pos = FM6_N;
    fm_age = 0;
}

/* the 24 voices of Melodee's sets, less the two that were the same sound again (a second Tine EP and a second Drawbars): the patch numbers the pluck
 * knows (0 ..) and where they are in those sets */
static const uint8_t FM_MAP[EN_NFMFACT] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
/* the voices a patch may use at once: the heaviest (six operators ringing, bells, strings, pads) three; the middle four or five; the rest all six */
static const uint8_t FM_CAP[EN_NCMFM] = {4, 3, 6, 3, 3, 6, 6, 6, 4, 6, 3, 6, 6, 3, 3, 4, 6, 6, 4, 6, 5, 3, 6, 6, 6, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};   /* (a loaded voice: four) */
/* the player's voices: 128-byte packed records. The main loop fills a staging slot and posts C_USERVOICE; the engine copies it in between chunks. */
static uint8_t fm_user[EN_NUSER][128] EN_POOL, fm_stage[EN_NUSER][128] EN_POOL;
static volatile uint16_t fm_stage_pend;
static volatile uint8_t fm_user_n;
static char fm_uname[EN_NUSER][16];
static void fm_user_name(int slot)               /* "DX " and the voice's ten characters, trailing spaces gone */
{
    int k, n = 10;
    char *d = fm_uname[slot];
    d[0] = 'D', d[1] = 'X', d[2] = ' ';
    while (n > 0 && (fm_user[slot][118 + n - 1] & 0x7Fu) <= 32u)
        n--;
    for (k = 0; k < n; k++) {
        const uint8_t c = fm_user[slot][118 + k] & 0x7Fu;
        d[3 + k] = c < 32u || c > 126u ? ' ' : (char)c;
    }
    if (!n)
        d[3] = '?', n = 1;
    d[3 + n] = 0;
    EN_CM_SOUND[12 + EN_NFMFACT + slot] = d;
}
static void fm_user_take(int slot)               /* C_USERVOICE, in the engine's own time */
{
    int k;
    if ((unsigned)slot >= EN_NUSER || !(fm_stage_pend >> slot & 1u))
        return;
    for (k = 0; k < 128; k++)
        fm_user[slot][k] = fm_stage[slot][k] & 0x7Fu;
    fm_stage_pend &= (uint16_t)~(1u << slot);
    if (slot >= (int)fm_user_n)
        fm_user_n = (uint8_t)(slot + 1);
    fm_user_name(slot);
    if (fm_cur == EN_NFMFACT + slot)
        fm_cur = -1;                             /* playing: load it again */
}
static void fm_user_wipe(void)
{
    int s;
    fm_user_n = 0;
    for (s = 0; s < EN_NUSER; s++)
        EN_CM_SOUND[12 + EN_NFMFACT + s] = "(empty)";
    if (fm_cur >= EN_NFMFACT)
        fm_cur = -1;
}
static uint8_t fm_req_n;                         /* the main loop's view of how many there will be (the engine takes the voices a chunk later) */
int en_user_voice(int slot, const uint8_t *p)
{
    int k;
    if ((unsigned)slot >= EN_NUSER || slot > (int)fm_req_n || (fm_stage_pend >> slot & 1u))
        return 0;
    if (slot == (int)fm_req_n)
        fm_req_n++;
    for (k = 0; k < 128; k++)
        fm_stage[slot][k] = p[k] & 0x7Fu;
    fm_stage_pend |= (uint16_t)(1u << slot);
    post(C_USERVOICE, slot, 0);
    return 1;
}
void en_user_clear(void)
{
    fm_req_n = 0;
    post(C_USERCLEAR, 0, 0);
}
int en_user_count(void) { return fm_user_n; }
int en_ncm_snd(void) { return 12 + EN_NFMFACT + fm_user_n; }
int en_user_next(void) { return fm_req_n < EN_NUSER ? fm_req_n : EN_NUSER - 1; }   /* where the next single voice goes: appended, or replacing the last */
const uint8_t *en_user_get(int slot) { return (unsigned)slot < EN_NUSER ? fm_user[slot] : fm_user[0]; }
static void fm_set_patch(int idx)                /* between chunks: a new patch cuts the notes */
{
    int k;
    fm_init();
    if (idx < 0 || idx >= EN_NCMFM)
        idx = 0;
    if (idx >= EN_NFMFACT && idx - EN_NFMFACT >= (int)fm_user_n)
        idx = 0;                                 /* a loaded voice that is not there: the first */
    if (idx == fm_cur)
        return;
    if (idx >= EN_NFMFACT) {
        fm6_unpack(fm_user[idx - EN_NFMFACT], fm_ed);
    } else {
        const int src = FM_MAP[idx];
        if (src < (int)FM6_NFACTORY)
            fm6_unpack(FM6_FACTORY[src], fm_ed);
        else
            fm6_rom_patch(&FM6_ROM[src - (int)FM6_NFACTORY], fm_ed);
    }
    fm_cur = idx;
    for (k = 0; k < FM_NV; k++) {
        fmx[k].used = 0;
        fm_zero(&fmx[k].s, sizeof fmx[k].s);
    }
    fm_zero(&fm_lfo, sizeof fm_lfo);
    fm_pos = FM6_N;
}

/* a key goes down (Dx7Note::init); the key comes up after gate samples */
static void fm_note_on(int note, float vel, int32_t gate)
{
    int k, best = 0, steal = 1;
    uint32_t oldest = 0xFFFFFFFFu;
    fm_slot_t *x;
    fm6_voice_t *s;
    const uint8_t *ed = fm_ed;
    if (fm_cur < 0)
        return;
    {   /* a free voice, unless the load has capped them (shed: 4, then 2): then the oldest is taken */
        int cap = en_shed_level == 0 ? FM_NV : en_shed_level == 1 ? 4 : 2;
        if (fm_cur >= 0 && FM_CAP[fm_cur] < cap)
            cap = FM_CAP[fm_cur];                /* (the patch's own: the heavy ones fewer) */
        int used = 0;
        for (k = 0; k < FM_NV; k++)
            used += fmx[k].used;
        for (k = 0; k < FM_NV; k++) {
            if (!fmx[k].used && used < cap) {
                best = k;
                steal = 0;
                break;
            }
            if (fmx[k].used && fmx[k].age < oldest) {
                oldest = fmx[k].age;
                best = k;
            }
        }
    }
    x = &fmx[best];
    s = &x->s;
    for (k = 0; k < FM_NV; k++)                  /* the first key down: the LFO restarts */
        if (fmx[k].used && fmx[k].gate)
            break;
    if (k == FM_NV) {
        if (ed[FP_LKS])
            fm_lfo.ph = (1u << 31) - 1u;
        fm_lfo.dly = 0;
    }
    s->vel = (uint8_t)clamp((int32_t)(vel * 127.0f), 1, 127);
    s->note = (uint8_t)note;
    fm6_keyed(s, ed, (uint32_t)clamp(note + ed[FP_TRNSP] - 24, 0, 127));
    for (k = 0; k < 6; k++) {
        s->eg[k].level = 0;
        fm6_eg_go(&s->eg[k], &ed[k * FP_OP], s->ol[k], s->rs[k], 0);
        s->porta[k] = s->base[k];
    }
    fm6_peg_set(&s->pe, ed);
    s->down = 1;
    if (ed[FP_OKS] && !steal)                    /* KEY SYNC (not on a stolen voice: no click) */
        for (k = 0; k < 6; k++)
            s->ph[k] = 0, s->gout[k] = 0;
    s->played = 1;
    s->sub = 0;
    s->quiet = 0;
    s->frozen = 0;
    s->still = 0;
    x->used = 1;
    x->gate = 1;
    x->gate_left = gate;
    x->kill_left = 0x7FFFFFFF;
    x->fade = 1.0f;
    x->age = ++fm_age;
}

static void fm_gates(uint32_t n, int32_t tail)  /* once a chunk, on the first core: the keys whose time is up; a note that has been let go for tail samples is faded out */
{
    int k;
    for (k = 0; k < FM_NV; k++) {
        if (!fmx[k].used)
            continue;
        if (fmx[k].gate) {
            if ((fmx[k].gate_left -= (int32_t)n) <= 0) {
                fmx[k].gate = 0;
                fmx[k].kill_left = tail;
            }
        } else if (fmx[k].kill_left > 0x40000000) {
            fmx[k].kill_left = tail;
        } else if ((fmx[k].kill_left -= (int32_t)n) <= 0) {
            fmx[k].fade = fmx[k].fade < 1.0f ? fmx[k].fade : 0.9999f;      /* (the fade starts: see fm_block) */
        }
    }
}

/* a new Dexed block for a voice: key-up, the envelopes, pitch and gains (Dx7Note::compute), the routing */
static void fm_control(fm_slot_t *x)
{
    fm6_voice_t *s = &x->s;
    const uint8_t *ed = fm_ed;
    uint32_t alg = (uint32_t)ed[FP_ALG] & 31u, k, dead = 1;
    int32_t lv[6], lfo = fm_lfo.val, dly = fm_lfo.depth, pm, sens, amd, a1, fbv;
    const uint32_t pmd = ((uint32_t)ed[FP_LPMD] * 165u) >> 6;
    if (!x->gate && s->down) {                   /* key up: every envelope to its fourth segment */
        s->down = 0;
        for (k = 0; k < 6u; k++)
            fm6_eg_go(&s->eg[k], &ed[k * FP_OP], s->ol[k], s->rs[k], 3);
        fm6_peg_go(&s->pe, 3);
    }
    /* pitch: the LFO (PMD x PMS, after the delay), the pitch envelope, the tune */
    sens = FM6_PMS[ed[FP_LPMS]] * (lfo - (1 << 23));
    pm = (int32_t)((float)pmd * (float)dly * (float)sens * (1.0f / 549755813888.0f));
    pm = pm < 0 ? -pm : pm;
    pm = sens < 0 ? -pm : pm;
    pm += fm_tune;
    /* amplitude: the LFO (AMD, after the delay) */
    lfo = (1 << 24) - lfo;
    a1 = (int32_t)((float)(((uint32_t)ed[FP_LAMD] * 165u) >> 6) * (float)dly * (float)lfo * (1.0f / 4294967296.0f));
    amd = a1 > 0 ? a1 : 0;
    pm = fm6_peg_step(&s->pe, s->down) + pm;
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = &ed[k * FP_OP];
        int32_t freq, level;
        if (op[FP_MODE])
            freq = s->base[k] + fm_tune;
        else
            freq = s->base[k] + pm;
        if (!((s->freq_valid >> k) & 1u) || s->freq_log[k] != freq) {
            s->freq_log[k] = freq;
            s->fq[k] = fm6_freq(freq);
            s->freq_valid |= (uint8_t)(1u << k);
        }
        level = fm6_eg_step(&s->eg[k], op, s->ol[k], s->rs[k], s->down);
        if (FM6_AMS[op[FP_AMS]]) {               /* AMS: the modulation takes a part of the level */
            uint32_t a = (uint32_t)op[FP_AMS], pt;
            if (fm_ams_amd != (uint32_t)amd) {
                fm_ams_amd = (uint32_t)amd;
                fm_ams_ok = 0;
            }
            if (!((fm_ams_ok >> a) & 1u)) {
                uint32_t sa = (uint32_t)((float)amd * ((float)FM6_AMS[a] * (1.0f / 16777216.0f)));
                fm_ams_pt[a] = sa ? fm6_ams_pt(sa) : 198789u;   /* exp(12.2) */
                fm_ams_ok |= (uint8_t)(1u << a);
            }
            pt = fm_ams_pt[a];
            level -= (int32_t)((float)level * ((float)pt * (1.0f / 16777216.0f)));
        }
        lv[k] = level;
    }
    for (k = 0; k < 6u; k++) {                   /* the output operators all gone for good? (and the feedback one) */
        const fm6_eg_t *e = &s->eg[k];
        if ((fm6_carrier(alg, 6u - k) || ((FM6_ALG[alg][k] & 0xc0u) == 0xc0u && ed[FP_FB])) &&
            (s->down || ed[k * FP_OP + FP_L1 + 3] > 0 || e->level >= FM6_SILENT || e->ix < 3u || (e->ix == 3u && e->rising)))
            dead = 0;
    }
    s->quiet = dead ? (uint8_t)(s->quiet + (s->quiet < 2u)) : 0;
    fbv = ed[FP_FB];
    fm6_plan(s, lv, alg, fm_engine, fbv ? 8u - (uint32_t)fbv : 16u);
}

static void fm_block(void)                       /* the next 64 samples of all the voices */
{
    int k;
    uint32_t j;
    for (j = 0; j < FM6_N; j++)
        fm_blk[j] = 0.0f;
    if (fm_cur < 0)
        return;
    fm6_lfo_step(&fm_lfo, fm_ed);
    for (k = 0; k < FM_NV; k++) {
        fm_slot_t *x = &fmx[k];
        if (!x->used)
            continue;
        fm_control(x);
        if (x->s.quiet >= 2u && !x->gate) {      /* over: released, and every output operator gone */
            x->used = 0;
            continue;
        }
        fm6_run(&x->s, FM6_N);
        if (x->fade < 1.0f) {                    /* faded out over 0.15 s: the ring-on is over */
            const float g0 = x->fade, g1 = g0 - 1.0f / 103.0f;
            for (j = 0; j < FM6_N; j++)
                fm_blk[j] += fm6_sum[j] * (g0 + (g1 - g0) * (float)j * (1.0f / (float)FM6_N));
            x->fade = g1;
            if (g1 <= 0.0f)
                x->used = 0;
            continue;
        }
        for (j = 0; j < FM6_N; j++)
            fm_blk[j] += fm6_sum[j];
    }
}

static inline float fm_next(void)                /* one sample, clipped as the DX7 would (+-16 is far past full scale) */
{
    float y;
    if (fm_pos >= FM6_N) {
        fm_block();
        fm_pos = 0;
    }
    y = fm_blk[fm_pos++];
    return y < -16.0f ? -16.0f : y > 16.0f ? 16.0f : y;
}

/* tests: MARK I on the float buses (1) or the integer ones (0): the output must be the same bits */
void en_fm_float_path(int v) { fm6_float_path = v; }

/* the DX7's other format: the 155-byte single-voice record -> the 128-byte packed one (the inverse of fm6_unpack) */
static void fm6_pack(const uint8_t *v, uint8_t *b)
{
    uint32_t k, i;
    for (i = 0; i < 128u; i++)
        b[i] = 0;
    for (k = 0; k < 6u; k++) {
        uint8_t *o = b + k * 17u;
        const uint8_t *d = v + k * FP_OP;
        for (i = 0; i < 11u; i++)
            o[i] = d[i] & 0x7Fu;
        o[11] = (uint8_t)((d[FP_LC] & 3u) | (d[FP_RC] & 3u) << 2);
        o[12] = (uint8_t)((d[FP_RS] & 7u) | (d[FP_DET] & 15u) << 3);
        o[13] = (uint8_t)((d[FP_AMS] & 3u) | (d[FP_KVS] & 7u) << 2);
        o[14] = d[FP_OL] & 0x7Fu;
        o[15] = (uint8_t)((d[FP_MODE] & 1u) | (d[FP_FC] & 31u) << 1);
        o[16] = d[FP_FF] & 0x7Fu;
    }
    for (i = 0; i < 9u; i++)
        b[102 + i] = v[FP_PR1 + i] & 0x7Fu;
    b[111] = (uint8_t)((v[FP_FB] & 7u) | (v[FP_OKS] & 1u) << 3);
    for (i = 0; i < 4u; i++)
        b[112 + i] = v[FP_LFS + i] & 0x7Fu;
    b[116] = (uint8_t)((v[FP_LKS] & 1u) | (v[FP_LFW] & 7u) << 1 | (v[FP_LPMS] & 7u) << 4);
    b[117] = v[FP_TRNSP] & 0x7Fu;
    for (i = 0; i < 10u; i++)
        b[118 + i] = v[FP_NAME + i] & 0x7Fu;
}
void en_voice_unpack(const uint8_t *b, uint8_t *v) { fm6_unpack(b, v); }
void en_voice_pack(const uint8_t *v, uint8_t *b) { fm6_pack(v, b); }
