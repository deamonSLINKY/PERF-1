/* SPDX-License-Identifier: GPL-3.0-only */
/* HYP-1 project. Part of the unity build, after app.h.
 *
 * Ten pieces, each its settings (all but Tune, MIDI out and the theme, which are the player's) and its
 * six drum patterns. While the power is on, each piece keeps its changes as you leave it (proj.slot).
 * The bank is what is saved: one flash object (OBJ_PROJ) holding the pieces saved with Global > SAVE,
 * and the player's three. A flash write silences the audio a moment, so the bank is written while
 * stopped (project_flush, the UI's to call). */
project_t proj;

#define BANK_MAGIC 0x32505948u               /* "HYP2" */
#define BANK_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    uint16_t npar, saved;                    /* P_NPARAMS when written; bit i: piece i saved */
    int16_t tune, midi, theme, rsv0;
    slot_t slot[NPIECES];
    uint8_t rsv[16];
} bank_t;
typedef char bank_fits[sizeof(bank_t) <= PLAT_STORE_MAX ? 1 : -1];
static bank_t bank;
static bank_t disk;                          /* the bank as it is in flash */
static uint8_t pending, pending_glob;        /* the pieces; the player's three (theme, Tune, MIDI out) */

#define OLD_KIT_MAGIC 0x31505948u            /* "HYP1": the kit before the bank (its theme carried over) */
typedef struct {
    uint32_t magic, format;
    uint16_t pats[EN_NPAT][EN_NLANE];
    int16_t lanes[3 * EN_NLANE];
    uint8_t theme, rsv[15];
} old_kit_t;
#define OLD_KIT_THEME (8 + 2 * EN_NPAT * EN_NLANE + 2 * 3 * EN_NLANE)   /* its theme's byte */

const char *piece_name(int i) { return en_piece_name_g(proj.genre, i); }

static void recipe(int i, slot_t *s)
{
    en_piece_g(proj.genre, i, s->par, &s->pool);
    en_piece_pats_g(proj.genre, i, s->pats);
}

static int16_t clamp_par(int p, int v)
{
    const en_param_t *pi = en_param_info(p);
    return (int16_t)(v < pi->lo ? pi->lo : v > pi->hi ? pi->hi : v);
}

static int pool_ok(uint32_t p)               /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static void take(int i)                      /* piece i's slot into play (its patterns too), the globals kept */
{
    const slot_t *s = &proj.slot[i];
    int p, l;
    for (p = 0; p < P_NPARAMS; p++)
        if (!en_par_global(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            en_pats[p][l] = s->pats[p][l];
}

static void keep(void)                       /* the piece playing, into its slot */
{
    slot_t *s = &proj.slot[proj.piece];
    int p, l;
    for (p = 0; p < P_NPARAMS; p++)
        s->par[p] = proj.par[p];
    s->pool = proj.pool;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            s->pats[p][l] = en_pats[p][l];
}

static void bank_globals(void)
{
    bank.tune = proj.par[P_TUNE];
    bank.midi = proj.par[P_MIDI];
    bank.theme = proj.par[P_THEME];
}

/* the bank from flash; a fresh one if there is none. Read field by field, so a bank written with fewer
 * settings (an older firmware's) still loads: the settings it lacks take their defaults. */
static uint8_t raw[PLAT_STORE_MAX];
static uint16_t rd16(const uint8_t *b) { return (uint16_t)(b[0] | b[1] << 8); }
static uint32_t rd32(const uint8_t *b) { return (uint32_t)rd16(b) | (uint32_t)rd16(b + 2) << 16; }
static void bank_load(void)
{
    const uint32_t HDR = 20u;
    int n, i, p, l;
    uint32_t npar, stride;
    memset(&bank, 0, sizeof bank);
    n = plat_store_load(OBJ_PROJ, raw, sizeof raw);
    npar = n >= (int)HDR ? rd16(raw + 8) : 0u;
    stride = npar * 2u + 2u + 2u * EN_NPAT * EN_NLANE;
    if (n >= (int)HDR && rd32(raw) == BANK_MAGIC && rd32(raw + 4) == BANK_FORMAT && npar >= 1u && npar <= P_NPARAMS &&
        (uint32_t)n >= HDR + NPIECES * stride) {
        bank.saved = rd16(raw + 10);
        bank.tune = (int16_t)rd16(raw + 12);
        bank.midi = (int16_t)rd16(raw + 14);
        bank.theme = (int16_t)rd16(raw + 16);
        for (i = 0; i < NPIECES; i++) {
            const uint8_t *q = raw + HDR + (uint32_t)i * stride;
            slot_t *s = &bank.slot[i];
            int bad = 0;
            if (!(bank.saved >> i & 1u))
                continue;
            for (p = 0; p < P_NPARAMS; p++) {
                int v = (uint32_t)p < npar ? (int16_t)rd16(q + 2 * p) : en_param_info(p)->def;
                bad |= v != clamp_par(p, v);
                s->par[p] = (int16_t)v;
            }
            en_style_sounds(s->par);         /* stored with a style (before 1.0.27): its sounds and kit */
            q += 2u * npar;
            s->pool = rd16(q);
            q += 2;
            for (p = 0; p < EN_NPAT; p++)
                for (l = 0; l < EN_NLANE; l++)
                    s->pats[p][l] = rd16(q + 2 * (p * EN_NLANE + l));
            if (bad || !pool_ok(s->pool))
                bank.saved &= (uint16_t)~(1u << i);   /* out of range: not saved */
        }
    } else {
        int16_t theme = en_param_info(P_THEME)->def;
        if (n >= (int)sizeof(old_kit_t) && rd32(raw) == OLD_KIT_MAGIC)
            theme = clamp_par(P_THEME, raw[OLD_KIT_THEME]);
        bank.tune = en_param_info(P_TUNE)->def;
        bank.midi = en_param_info(P_MIDI)->def;
        bank.theme = theme;
    }
    bank.magic = BANK_MAGIC;
    bank.format = BANK_FORMAT;
    bank.npar = P_NPARAMS;
    disk = bank;
}

/* power-on: each piece as saved, or its recipe; a piece at random. One never saved is nudged: the
 * levels and the sound a few points either way, the tempo a little (the key, the mode and the switches
 * stay). A saved one starts as it was saved. Seed 0 (the tests'): the first piece, exact. */
void project_start(uint32_t seed)
{
    static const uint8_t NUDGE[] = {P_PAD, P_ACID, P_BASS, P_PULSE, P_NOISE, P_EVOLVE, P_DRIFT, P_MOTION, P_BRIGHT,
                                    P_DETUNE, P_NZ_TONE, P_DR_CUT, P_DR_RES, P_DE_AMT, P_DL_DEPTH, P_AC_CUT, P_AC_RES,
                                    P_AC_ENV, P_AC_DEC, P_AC_DENS, P_AC_MUT, P_ECHO, P_REVERB, P_SIZE, P_DRIVE};
    uint32_t r = seed, i;
    memset(&proj, 0, sizeof proj);
    bank_load();
    for (i = 0; i < NPIECES; i++) {
        if (bank.saved >> i & 1u)
            proj.slot[i] = bank.slot[i];
        else
            recipe((int)i, &proj.slot[i]);
    }
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info((int)i)->def;
    proj.par[P_TUNE] = clamp_par(P_TUNE, bank.tune);
    proj.par[P_MIDI] = clamp_par(P_MIDI, bank.midi);
    proj.par[P_THEME] = clamp_par(P_THEME, bank.theme);
    proj.leds = 1;
#define NEXT() (r ^= r << 13, r ^= r >> 17, r ^= r << 5, r)
    proj.piece = seed ? (uint8_t)(NEXT() % NPIECES) : 0u;
    take(proj.piece);
    if (seed && !(bank.saved >> proj.piece & 1u)) {
        for (i = 0; i < sizeof NUDGE; i++) {
            int p = NUDGE[i], v = proj.par[p];
            if (v == 0 && p <= P_NOISE)
                continue;                    /* a layer the piece leaves out stays out */
            proj.par[p] = clamp_par(p, v + (int)(NEXT() % 17u) - 8);
        }
        proj.par[P_TEMPO] = clamp_par(P_TEMPO, proj.par[P_TEMPO] + (int)(NEXT() % 7u) - 3);
        keep();
    }
#undef NEXT
    if (seed)
        en_seed(seed ^ seed >> 16);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(proj.piece);
}

/* the genres: the original bank is the one in flash. The others are factory sets (their changes last while you stay in the
 * genre; nothing of them is stored). A switch brings up the other bank with the same piece number. */
static slot_t gset0[NPIECES];                /* the original bank as it was left (its session changes) */
void project_set_genre(int g)
{
    int i;
    g = ((g % EN_NGENRE) + EN_NGENRE) % EN_NGENRE;
    if (g == proj.genre)
        return;
    keep();
    if (proj.genre == 0)
        memcpy(gset0, proj.slot, sizeof proj.slot);
    proj.genre = (uint8_t)g;
    if (g == 0) {
        memcpy(proj.slot, gset0, sizeof proj.slot);
    } else {                                 /* the others: their recipes, fresh each time (to save RAM) */
        for (i = 0; i < NPIECES; i++)
            recipe(i, &proj.slot[i]);
    }
    proj.edited = 0;
    take(proj.piece);
}

void project_piece_save(void)
{
    if (proj.genre)
        return;                              /* only the original bank is stored */
    keep();
    bank.slot[proj.piece] = proj.slot[proj.piece];
    bank.saved |= (uint16_t)(1u << proj.piece);
    bank_globals();
    proj.edited &= (uint16_t)~(1u << proj.piece);
    pending = 1;
}

void project_piece_factory(void)
{
    recipe(proj.piece, &proj.slot[proj.piece]);
    take(proj.piece);
    if (!proj.genre && (bank.saved >> proj.piece & 1u)) {
        bank.saved &= (uint16_t)~(1u << proj.piece);
        memset(&bank.slot[proj.piece], 0, sizeof bank.slot[0]);
        pending = 1;
    }
    proj.edited &= (uint16_t)~(1u << proj.piece);
}

void project_globals(void)
{
    bank_globals();
    pending_glob = 1;
}

int project_saved(int i) { return !proj.genre && (unsigned)i < NPIECES && (bank.saved >> i & 1u); }
int project_pending(void) { return pending || pending_glob; }
int project_pending_globals(void) { return pending_glob; }

int project_flush(void)
{
    if (!pending && !pending_glob)
        return 0;
    if (plat_store_save(OBJ_PROJ, &bank, sizeof bank) != 0)
        return -1;
    disk = bank;
    pending = pending_glob = 0;
    return 0;
}

/* the player's three alone, into flash as it is (a piece stored but not yet written stays waiting):
 * the music plays from power-on, so these are written without waiting for it to stop */
int project_flush_globals(void)
{
    if (!pending_glob)
        return 0;
    disk.tune = bank.tune;
    disk.midi = bank.midi;
    disk.theme = bank.theme;
    if (plat_store_save(OBJ_PROJ, &disk, sizeof disk) != 0)
        return -1;
    pending_glob = 0;
    return 0;
}
