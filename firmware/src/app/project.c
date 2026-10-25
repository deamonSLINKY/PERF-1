/* SPDX-License-Identifier: GPL-3.0-only */
/* HYP-1 project. Part of the unity build, after app.h.
 *
 * Ten pieces, each its settings (all but Tune, MIDI out and the theme, which are the player's) and its
 * six drum patterns. While the power is on, each piece keeps its changes as you leave it (proj.slot).
 * The bank is what is saved: one flash object (OBJ_PROJ) holding the pieces saved with Global > SAVE,
 * and the player's three. A flash write silences the audio a moment, so the bank is written while
 * stopped (project_flush, the UI's to call). */
project_t proj;

/* THE STORED FORM. Every genre is a bank of ten pieces and has its own flash object (the original's is OBJ_PROJ, the others OBJ_G1 ..): the
 * pieces saved with Global > SAVE, in a packed form (a byte a setting: its value less its lowest, the pool, the six patterns), each in a
 * fixed-size record of IMG_REC bytes, so settings (and, later, a piece's step data) can be added without moving anything: a record
 * written with fewer settings loads with the piece's recipe for the missing ones. A save writes the genre's object (22 ms of silence: the
 * UI fades around it). The player's three (Tune, MIDI out, the theme) are their own small object (OBJ_GLOB). A bank written by an
 * older firmware (the original genre's, 16-bit settings, in OBJ_PROJ) is read as it was and written in this form by the next save. */
#define IMG_MAGIC 0x33505948u                /* "HYP3" */
#define IMG_VER 2u
#define IMG_HDR 16u                          /* magic 4, ver 1, genre 1, npar 2, saved 2, record size 2, 4 spare */
#define IMG_REC 376u
#define IMG_LEN (IMG_HDR + NPIECES * IMG_REC)
#define REC_PAR (2u + 2u * EN_NPAT * EN_NLANE)   /* the settings follow the pool and the patterns */
typedef char img_fits[IMG_LEN <= PLAT_STORE_MAX ? 1 : -1];
typedef char rec_fits[REC_PAR + P_NPARAMS <= IMG_REC ? 1 : -1];
static uint8_t image[PLAT_STORE_MAX];        /* the playing genre's object, as it is in flash (and as the next save writes it: its first IMG_LEN bytes; the old bank is read here whole first) */
static uint16_t saved;                       /* bit i: piece i is in the image */
static uint8_t pending, pending_glob;        /* the pieces; the player's three (theme, Tune, MIDI out) */
typedef struct {
    uint32_t magic;
    int16_t tune, midi, theme, rsv;
} glob_t;
#define GLOB_MAGIC 0x4C474D50u               /* "PMGL" */
static glob_t glob;
static int glob_from_old;                    /* the old bank's header had them */

#define BANK_MAGIC 0x32505948u               /* "HYP2": the old bank, one object, ten pieces of 16-bit settings */
#define BANK_FORMAT 1u
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
        if (!en_par_sticky(p))
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

static uint16_t rd16(const uint8_t *b) { return (uint16_t)(b[0] | b[1] << 8); }
static uint32_t rd32(const uint8_t *b) { return (uint32_t)rd16(b) | (uint32_t)rd16(b + 2) << 16; }
static void wr16(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *b, uint32_t v) { wr16(b, v); wr16(b + 2, v >> 16); }
static uint32_t obj_of(int g) { return g <= 0 ? (uint32_t)OBJ_PROJ : (uint32_t)OBJ_G1 + (uint32_t)g - 1u; }

static void glob_sync(void)                  /* the player's three, from play */
{
    glob.tune = proj.par[P_TUNE];
    glob.midi = proj.par[P_MIDI];
    glob.theme = proj.par[P_THEME];
}

/* a piece in the stored form, and back (the slot should hold the recipe first: what the record lacks keeps it). 1: a good piece */
static void pack_slot(const slot_t *s, uint8_t *rec)
{
    int p, l;
    memset(rec, 0, IMG_REC);
    wr16(rec, s->pool);
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            wr16(rec + 2 + 2 * (p * EN_NLANE + l), s->pats[p][l]);
    for (p = 0; p < P_NPARAMS; p++) {
        const int v = s->par[p] - en_param_info(p)->lo;
        rec[REC_PAR + p] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
}
static int unpack_slot(const uint8_t *rec, uint32_t npar, slot_t *s)
{
    int p, l, bad = 0;
    for (p = 0; p < EN_NPAT; p++)
        for (l = 0; l < EN_NLANE; l++)
            s->pats[p][l] = rd16(rec + 2 + 2 * (p * EN_NLANE + l));
    s->pool = rd16(rec);
    for (p = 0; (uint32_t)p < npar && p < P_NPARAMS; p++) {
        const int v = en_param_info(p)->lo + rec[REC_PAR + p];
        if (p == P_AC_CHANCE && v > 1) {         /* (a development build kept a chance here, 100: now the sync, bars) */
            s->par[p] = 0;
            continue;
        }
        bad |= v > en_param_info(p)->hi;
        s->par[p] = clamp_par(p, v);
    }
    return !bad && pool_ok(s->pool);
}
static void ans_old(slot_t *s)                /* the acid answer in the development builds before 0.6.0: 1 octave up, 2 fifth up, 3 mirror, 4 sparse, 5 octave down, 6 fifth down; and the old style's place (now its sync: bars) */
{
    static const uint8_t M[7] = {0, 8, 13, 22, 37, 6, 11};   /* kind * 5 + direction (3 up, 1 down, 2 the same) */
    const int v = s->par[P_AC_RESP];
    s->par[P_AC_RESP] = (int16_t)(v >= 0 && v < 7 ? M[v] : 0);
    s->par[P_AC_CHANCE] = 0;
}
static void image_header(int g)
{
    memset(image, 0, IMG_HDR);
    wr32(image, IMG_MAGIC);
    image[4] = IMG_VER;
    image[5] = (uint8_t)g;
    wr16(image + 6, P_NPARAMS);
    wr16(image + 8, saved);
    wr16(image + 10, IMG_REC);
    image[12] = 1;                           /* 1: the acid answer in its present form (kind * 5 + direction) */
}

/* the old bank (OBJ_PROJ, 16-bit settings): its pieces into the slots, as before: read field by field, so a bank written with fewer
 * settings still loads; the settings it lacks take their defaults. n: its length, in image. */
static void old_bank_load(int n)
{
    const uint32_t HDR = 20u;
    int i, p, l;
    uint32_t npar, stride;
    int old_rate, old_theme, old_tone;
    const uint8_t *raw = image;
    old_tone = n >= (int)HDR && rd16(raw + 18) < 3;                    /* P_AC_EVERY was the old unused tone (50) */
    old_theme = n >= (int)HDR && rd16(raw + 18) < 2;                 /* the themes were in the order Synth, Acid, Ice, Ember, Phosphor, Imp */
    old_rate = n >= (int)HDR && rd16(raw + 18) == 0;   /* a bank from before the pluck's Rate was sorted slow to fast: 1/8, 1/16, 1/4, 3/16 */
    npar = n >= (int)HDR ? rd16(raw + 8) : 0u;
    stride = npar * 2u + 2u + 2u * EN_NPAT * EN_NLANE;
    if (n >= (int)HDR && rd32(raw) == BANK_MAGIC && rd32(raw + 4) == BANK_FORMAT && npar >= 1u && npar <= P_NPARAMS &&
        (uint32_t)n >= HDR + NPIECES * stride) {
        uint16_t sv = rd16(raw + 10);
        glob.tune = (int16_t)rd16(raw + 12);
        glob.midi = (int16_t)rd16(raw + 14);
        glob.theme = (int16_t)rd16(raw + 16);
        glob_from_old = 1;
        if (old_theme)
            glob.theme = (int16_t)(glob.theme >= 5 ? 0 : glob.theme + 1);   /* (Imp is the first now, the others one on) */
        for (i = 0; i < NPIECES; i++) {
            const uint8_t *q = raw + HDR + (uint32_t)i * stride;
            slot_t *s = &proj.slot[i];
            int bad = 0;
            if (!(sv >> i & 1u))
                continue;
            for (p = 0; p < P_NPARAMS; p++) {
                int v = (uint32_t)p < npar ? (int16_t)rd16(q + 2 * p) : en_param_info(p)->def;
                if (p == P_AC_EVERY && old_tone)
                    v = 0;
                if (p == P_CM_RATE && old_rate)
                    v = (v + 2) & 3;      /* (old 0 1 2 3 = 1/8 1/16 1/4 3/16 is now 2 3 0 1) */
                bad |= p != P_STYLE && v != clamp_par(p, v);   /* (the old style is converted below) */
                s->par[p] = (int16_t)v;
            }
            en_style_sounds(s->par);         /* stored with a style (before 1.0.27): its sounds and kit */
            ans_old(s);
            q += 2u * npar;
            s->pool = rd16(q);
            q += 2;
            for (p = 0; p < EN_NPAT; p++)
                for (l = 0; l < EN_NLANE; l++)
                    s->pats[p][l] = rd16(q + 2 * (p * EN_NLANE + l));
            if (bad || !pool_ok(s->pool))
                sv &= (uint16_t)~(1u << i);   /* out of range: not saved */
        }
        saved = sv;
    } else if (n >= (int)sizeof(old_kit_t) && rd32(raw) == OLD_KIT_MAGIC) {
        glob.theme = clamp_par(P_THEME, raw[OLD_KIT_THEME]);
        glob_from_old = 1;
    }
}

/* genre g's pieces (proj.genre is g): each its recipe, or as saved. The original genre may still have the old form in flash: read it,
 * and make the image from it (flash is left as it is until the next save). */
static void image_load(int g)
{
    int i, n;
    saved = 0;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    n = plat_store_load(obj_of(g), image, sizeof image);
    if (n >= (int)IMG_HDR && rd32(image) == IMG_MAGIC && image[4] == IMG_VER && image[5] == (uint8_t)g && rd16(image + 10) == IMG_REC &&
        (uint32_t)n >= IMG_LEN) {
        const uint32_t npar = rd16(image + 6);
        const uint16_t sv = rd16(image + 8);
        const int enc = image[12];
        for (i = 0; i < NPIECES; i++)
            if (sv >> i & 1u) {
                if (unpack_slot(image + IMG_HDR + (uint32_t)i * IMG_REC, npar, &proj.slot[i])) {
                    saved |= (uint16_t)(1u << i);
                    if (!enc)
                        ans_old(&proj.slot[i]);
                } else {
                    recipe(i, &proj.slot[i]);       /* out of range: not saved */
                }
            }
    } else {
        if (g == 0 && n > 0)
            old_bank_load(n);
        memset(image, 0, sizeof image);
    }
    for (i = 0; i < NPIECES; i++)       /* the image is what the slots saved hold */
        if (saved >> i & 1u)
            pack_slot(&proj.slot[i], image + IMG_HDR + (uint32_t)i * IMG_REC);
    image_header(g);
}

static void glob_load(void)                  /* the player's three: their object, else the old bank's header, else the defaults */
{
    glob_t g;
    if (plat_store_load(OBJ_GLOB, &g, sizeof g) == (int)sizeof g && g.magic == GLOB_MAGIC) {
        glob = g;
    } else if (!glob_from_old) {
        glob.tune = en_param_info(P_TUNE)->def;
        glob.midi = en_param_info(P_MIDI)->def;
        glob.theme = en_param_info(P_THEME)->def;
    }
    glob.magic = GLOB_MAGIC;
}

/* power-on: each piece as saved, or its recipe; a piece at random. One never saved is nudged: the
 * levels and the sound a few points either way, the tempo a little (the key, the mode and the switches
 * stay). A saved one starts as it was saved. Seed 0 (the tests'): the first piece, exact. */
void project_start(uint32_t seed)
{
    static const uint8_t NUDGE[] = {P_PAD, P_ACID, P_BASS, P_PULSE, P_NOISE, P_EVOLVE, P_DRIFT, P_MOTION, P_BRIGHT,
                                    P_DETUNE, P_DR_CUT, P_DR_RES, P_DE_AMT, P_DL_DEPTH, P_AC_CUT, P_AC_RES,
                                    P_AC_ENV, P_AC_DEC, P_AC_DENS, P_AC_MUT, P_ECHO, P_REVERB, P_SIZE, P_DRIVE};
    uint32_t r = seed, i;
    memset(&proj, 0, sizeof proj);
    image_load(0);
    glob_load();
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info((int)i)->def;
    proj.par[P_TUNE] = clamp_par(P_TUNE, glob.tune);
    proj.par[P_MIDI] = clamp_par(P_MIDI, glob.midi);
    proj.par[P_THEME] = clamp_par(P_THEME, glob.theme);
    proj.leds = 1;
#define NEXT() (r ^= r << 13, r ^= r >> 17, r ^= r << 5, r)
    proj.piece = seed ? (uint8_t)(NEXT() % NPIECES) : 0u;
    take(proj.piece);
    if (seed && !(saved >> proj.piece & 1u)) {
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

/* the genres: each is its own object in flash (the pieces saved in it, the rest its recipes). A switch brings up the other bank with the
 * same piece number; the changes not saved last while you stay in a genre (the original genre's, as you left it, come back with it). */
static slot_t gset0[NPIECES];                /* the original bank as it was left (its session changes) */
void project_set_genre(int g)
{
    g = ((g % EN_NGENRE) + EN_NGENRE) % EN_NGENRE;
    if (g == proj.genre)
        return;
    keep();
    if (pending)
        project_flush();                     /* (a piece saved but not yet written goes first) */
    if (proj.genre == 0)
        memcpy(gset0, proj.slot, sizeof proj.slot);
    proj.genre = (uint8_t)g;
    image_load(g);
    if (g == 0)
        memcpy(proj.slot, gset0, sizeof proj.slot);
    proj.edited = 0;
    take(proj.piece);
}

void project_piece_save(void)
{
    keep();
    pack_slot(&proj.slot[proj.piece], image + IMG_HDR + (uint32_t)proj.piece * IMG_REC);
    saved |= (uint16_t)(1u << proj.piece);
    proj.edited &= (uint16_t)~(1u << proj.piece);
    pending = 1;
}

void project_piece_factory(void)
{
    recipe(proj.piece, &proj.slot[proj.piece]);
    take(proj.piece);
    if (saved >> proj.piece & 1u) {
        saved &= (uint16_t)~(1u << proj.piece);
        memset(image + IMG_HDR + (uint32_t)proj.piece * IMG_REC, 0, IMG_REC);
        pending = 1;
    }
    proj.edited &= (uint16_t)~(1u << proj.piece);
}

void project_globals(void)
{
    glob_sync();
    pending_glob = 1;
}

int project_saved(int i) { return (unsigned)i < NPIECES && (saved >> i & 1u); }
int project_pending(void) { return pending || pending_glob; }
int project_pending_globals(void) { return pending_glob; }

int project_flush(void)
{
    if (!pending && !pending_glob)
        return 0;
    if (pending) {
        image_header(proj.genre);
        if (plat_store_save(obj_of(proj.genre), image, IMG_LEN) != 0)
            return -1;
        pending = 0;
    }
    return project_flush_globals();
}

/* the player's three alone, into flash as they are (a piece saved but not yet written stays waiting):
 * the music plays from power-on, so these are written without waiting for it to stop */
int project_flush_globals(void)
{
    if (!pending_glob)
        return 0;
    glob.magic = GLOB_MAGIC;
    if (plat_store_save(OBJ_GLOB, &glob, sizeof glob) != 0)
        return -1;
    pending_glob = 0;
    return 0;
}
