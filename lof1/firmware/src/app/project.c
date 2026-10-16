/* SPDX-License-Identifier: GPL-3.0-only */
/* LOF-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The presets 01-10 (their factory recipes are the engine's, en_piece): each keeps its own settings in
 * memory as you leave it, but only SAVE on the Global page stores one: the preset playing goes into the
 * stored project (the others stay as they were stored). Tune and MIDI are global. */
project_t proj;
static project_t stored;                         /* what the flash holds */

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    memcpy(proj.pats, en_pats, sizeof proj.pats);
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
    en_pats_default();
    memcpy(proj.pats, en_pats, sizeof proj.pats);
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

int project_load(void)
{
    static project_t buf;
    int n;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    proj = buf;
    if (n >= 8 && project_valid()) {
        stored = proj;
        return 0;
    }
    project_defaults();
    stored = proj;
    return -1;
}

int project_save(void)                           /* the preset playing (and the drum patterns, and the globals) */
{
    int p;
    keep();
    stored.slot[proj.piece] = proj.slot[proj.piece];
    for (p = 0; p < P_NPARAMS; p++)
        stored.par[p] = proj.par[p];
    stored.piece = proj.piece;
    stored.leds = proj.leds;
    memcpy(stored.pats, proj.pats, sizeof stored.pats);
    return plat_store_save(OBJ_PROJ, &stored, sizeof stored);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    memcpy(en_pats, proj.pats, sizeof proj.pats);
}
