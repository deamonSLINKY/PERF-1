/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 on the host: the whole app (UI, project, engine) against a simulated FM-1 driven by a script.
 * Audio goes to a WAV, the screen to PNGs, the lights to text, MIDI out to a log. The device-only
 * parts (HAL, USB, OTA, flash driver) are replaced by plat.h implemented here. (From X0X's x0x_host.)
 *
 *   amb_host SCRIPT [OUTDIR]
 *
 * Script, one command per line (# comments):
 *   wait MS                      run the device for MS milliseconds
 *   press BTN | release BTN | tap BTN     (BTN: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   key K down|up | tapkey K     (K: 0..26 = F3..G5, or w0..w15 white keys, b0..b10 black keys)
 *   turn ENC N [MS]              (ENC: SELECT ALGO PRESET K1 K2 K3 K4; N detents, MS apart, default 80)
 *   spin ENC N                   N detents at once, exactly N (no acceleration)
 *   master N                     MASTER pot 0..4096
 *   midi B0 B1 B2                incoming USB MIDI message (hex bytes)
 *   wav FILE | wavstop           start / stop recording the output
 *   peakreset                    restart the output peak (expect peak_db_min / peak_db_max)
 *   shot FILE.png                save the screen
 *   leds                         print the lit buttons and keys
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *   reboot                       re-run boot (power off and on)
 *   seed N                       the seed the next boot uses (0: the first piece, exact)
 *   restore OBJ [bad|old|shared] write an object back as a restored backup's (checked first); expect rsvalid, rsstate, storelock
 *   seamless 0/1                 a store without the fade (the sliced erase)
 *   battery N                    the battery divider's reading, 0..1023 (default: none)
 *   storefuzz SEED               fill the project object with random damage (for the sanitizer runs)
 *   bank76 P N                   write a bank as the firmware before styles did (76 settings a piece),
 *                                piece P stored with its Pad at N
 */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#define __attribute__(x)

/* ------------------------------------------------------------ screen --- */
static uint16_t fb[240 * 240];          /* RGB565, byte-swapped as the panel takes it (gfx.c) */
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = sc;
}
static uint32_t blits;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    blits++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = px[j * w + i];
}
#include "../firmware/src/gfx.c"


/* ---------------------------------------------------------- platform --- */
#include "../firmware/src/app/plat.h"

static uint32_t now_ms, held_btn, held_keys, master = 2048;
static int32_t enc_acc[NE];
static uint32_t lit_btn, lit_keys;

uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return held_btn; }
uint32_t plat_keys(void) { return held_keys; }
int32_t plat_enc(int role)
{
    int32_t v = enc_acc[role];
    enc_acc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return master; }
static uint32_t host_batt = 0xFFFFFFFFu;
uint32_t plat_battery(void) { return host_batt; }
static int host_usb;
int plat_usb_power(void) { return host_usb; }
static uint32_t keys_dim, dim_keys;
void plat_keys_dim(uint32_t k) { keys_dim = k; }
void plat_leds(uint32_t b, uint32_t k)
{
    lit_btn = b;
    lit_keys = k;
    dim_keys = keys_dim & ~k;            /* the keys lit dim (plat_keys_dim, for this call only) */
    keys_dim = 0;
}
static int glow_on = 1;                 /* the device's button glow (plat_glow); the lights printed are the bright ones */
void plat_glow(int on) { glow_on = on; }
void plat_play_red(int red) { (void)red; }      /* the device's PLAY colour: green, red while recording */

#define MQ 256
static uint32_t min_q[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t last_out;                        /* the last packet out, not clock: status << 16 | d1 << 8 | d2 */
static uint32_t midi_out_count, clock_out_count, noteon_out_count, start_out_count, cm_pcs_out;
static int cm_now_out, cm_max_out;               /* (0.17.53) the pluck's note-ons in the block being rendered, the most in one */
static uint32_t nh, nh_keep, nh_t0, nh_win, nh_arm, nh_n, nh_blk;   /* (0.17.11) the notes out as a hash: armed, from the next start (0xFA) for nh_win ms */
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r++ % MQ];
    return 1;
}
static uint32_t cc_out_n[128];                   /* control changes out, by number (0.15.22) */
static int last_vel;                             /* the last note-on's velocity (0.15.23) */
void plat_midi_out(uint32_t pkt)
{
    midi_out_count++;
    if (((pkt >> 8) & 0xF0u) == 0x90u && (pkt >> 24))
        last_vel = (int)(pkt >> 24), noteon_out_count++;
    if (((pkt >> 8) & 0xFFu) == 0x91u && (pkt >> 24)) {
        cm_pcs_out |= 1u << (pkt >> 16 & 0x7Fu) % 12u;
        if (++cm_now_out > cm_max_out)
            cm_max_out = cm_now_out;
    }
    if (((pkt >> 8) & 0xF0u) == 0xB0u)
        cc_out_n[pkt >> 16 & 0x7Fu]++;
    if (((pkt >> 8) & 0xFFu) == 0xFAu)
        start_out_count++;
    if (((pkt >> 8) & 0xFF) != 0xF8)
        last_out = (pkt >> 8 & 0xFFu) << 16 | (pkt >> 16 & 0xFFu) << 8 | pkt >> 24;
    if (((pkt >> 8) & 0xFF) == 0xF8) {
        clock_out_count++;
        return;
    }
    if (((pkt >> 8) & 0xFF) == 0xFA && nh_arm) {
        nh_arm = 0;
        nh = 2166136261u;
        nh_t0 = nh_blk + 1u;
        nh_n = 0;
    }
    if (!nh_arm && nh_t0 && (nh_blk + 1u - nh_t0) * 256u / 44u < nh_win && ((pkt >> 8) & 0xF0u) == 0x90u && (pkt >> 8 & 15u) && (pkt >> 24)) {   /* the notes on (not the pad's: its breaths go on from the sound before), and the block each is in */
        uint32_t v = (pkt >> 8 & 0xFFFFFFu) ^ (nh_blk + 1u - nh_t0) << 24 ^ (nh_blk + 1u - nh_t0) >> 8 << 4;
        int k;
        for (k = 0; k < 4; k++)
            nh = (nh ^ (v >> (8 * k) & 0xFFu)) * 16777619u;
        nh ^= (nh_blk + 1u - nh_t0) * 2654435761u;
        nh_n++;
    }
    if (midi_log)
        fprintf(midi_log, "%u ms: %02X %02X %02X\n", now_ms, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, (pkt >> 24) & 0xFF);
}
/* the engine's MIDI out, as the device's glue sends it (USB-MIDI packet, cable 0) */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

/* flash: one buffer per object, kept across a simulated reboot */
static uint8_t store[OBJ_NOBJ][PLAT_STORE_MAX];
static int store_len[OBJ_NOBJ] = {-1, -1, -1, -1};
static uint32_t store_writes;
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    if (obj >= OBJ_NOBJ || (n = store_len[obj]) < 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, store[obj], (size_t)n);
    return n;
}
static int store_lock, rs_valid;                 /* a restored object was written (nothing else is until the restart); the last restore's check */
static int host_seamless;
int plat_store_seamless(void) { return host_seamless; }
static int host_uh_want, host_uh_msg;            /* the USB host: asked for, and what it says (the simulator has no USB: it only answers on and off) */
void plat_usb_host(int on)
{
    if (on != host_uh_want)
        host_uh_msg = on ? PLAT_UH_ON : PLAT_UH_OFF;
    host_uh_want = on;
}
void plat_usb_host_diag(char *b) { b[0] = 0; }
int plat_usb_host_state(void) { return host_uh_want; }
int plat_wire(int mode) { (void)mode; return 0; }   /* (the link cable: no wires on the host, nothing comes in) */
void plat_wire_diag(char *b) { static const char S[] = "WIRE OUT 0 IN 0 BAD 0"; memcpy(b, S, sizeof S); }
int plat_usb_host_msg(void)
{
    const int m = host_uh_msg;
    host_uh_msg = 0;
    return m;
}
int plat_store_locked(void) { return store_lock; }
static uint8_t raw_flash[PLAT_RAW_HI - PLAT_RAW_LO];   /* (0.17.38) the sample kits' flash */
int plat_raw_write(uint32_t off, const void *src, uint32_t len)
{
    if (off < PLAT_RAW_LO || off >= PLAT_RAW_HI || (off & 0xFFFu) || len > 4096u)
        return -2;
    memset(raw_flash + (off - PLAT_RAW_LO), 0xFF, 4096u);
    memcpy(raw_flash + (off - PLAT_RAW_LO), src, len);
    return 0;
}
const uint8_t *plat_raw_ptr(uint32_t off) { return off >= PLAT_RAW_LO && off < PLAT_RAW_HI ? raw_flash + (off - PLAT_RAW_LO) : 0; }
static int sf_obj = -1, sf_n;                   /* (0.17.48) storefail OBJ N: the next N writes of object OBJ fail */
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    if (store_lock)
        return -8;
    if ((int)obj == sf_obj && sf_n > 0) {
        sf_n--;
        return -3;
    }
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}
int plat_store_restore(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    store_lock = 1;
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}

static uint32_t cpu_pct;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_xruns(void) { return 0; }
uint32_t plat_late(void) { return 0; }
uint32_t plat_peak_pct(void) { return cpu_pct; }
uint32_t plat_cpu1_pct(void) { return 0; }
uint32_t plat_core2(void) { return 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

#ifndef EN_WEB
/* (0.17.61) THE SAVE FORMATS, LOCKED FOR 1.x. fmt_dump: what this build makes of the saved objects, a "key value" a line: every genre's
 * saved pieces as they load (only the settings the record has: those after it are the recipe's, which may change), the set list, the
 * settings (the effect settings the record has), the MIDI map, the voices. tests/format/<release>/ keeps the objects a public release
 * saved and this dump of them: every build must load them the same (fmtcheck). fmt_lock: the layout, for tools/format_lock.py. */
static void fmt_dump(FILE *f)
{
    static uint8_t raw[PLAT_STORE_MAX];
    const int g0 = proj.genre, p0 = proj.piece;
    int g, i, k, l, n;
    for (g = 0; g < EN_NGENRE; g++) {
        uint32_t npar = 0;
        n = plat_store_load(obj_of(g), raw, sizeof raw);
        if (n <= 0)
            continue;
        if (n >= (int)IMG_HDR && rd32(raw) == IMG_MAGIC)
            npar = rd16(raw + 6);
        else if (n >= 10 && rd32(raw) == BANK_MAGIC)
            npar = rd16(raw + 8);
        proj.genre = (uint8_t)g;
        image_load(g);
        fprintf(f, "g%d.saved %u\n", g, (unsigned)saved);
        for (i = 0; i < NPIECES; i++) {
            const slot_t *s = &proj.slot[i];
            if (!(saved >> i & 1u))
                continue;
            for (k = 0; k < P_NPARAMS && (uint32_t)k < npar; k++)
                if (!(k >= P_DD_CHANGE && k <= P_DD_MUTATE && !(px[i][PX_FLAGS] & 1u)))   /* (before 0.16.28: the defaults, image_load) */
                    fprintf(f, "g%d.p%d.par%d %d\n", g, i, k, s->par[k]);
            fprintf(f, "g%d.p%d.pool %u\n", g, i, (unsigned)s->pool);
            for (k = 0; k < EN_NPAT; k++) {
                fprintf(f, "g%d.p%d.pat%d", g, i, k);
                for (l = 0; l < EN_NLANE; l++)
                    fprintf(f, " %u", (unsigned)s->pats[k][l]);
                fputc('\n', f);
            }
            for (l = 0; l < EN_NLANE; l++) {
                fprintf(f, "g%d.p%d.mod%d", g, i, l);
                for (k = 0; k < 16; k++)
                    fprintf(f, " %u", (unsigned)s->mod[l][k]);
                fputc('\n', f);
            }
#define FMT_BYTES(nm, arr, len) do { fprintf(f, "g%d.p%d." nm, g, i); for (k = 0; k < (int)(len); k++) fprintf(f, " %u", (unsigned)(arr)[k]); fputc('\n', f); } while (0)
            FMT_BYTES("acl", s->acl, 18);
            FMT_BYTES("cml", s->cml, 12);
            FMT_BYTES("prg", s->prg, EN_PRG_LEN);
            FMT_BYTES("prk", s->prk, EN_PRK_LEN);
            FMT_BYTES("bs", s->bs, 16);
#undef FMT_BYTES
            for (k = EN_PRK_LEN; k < (int)PX_LEN; k++)   /* the extras past the chord types, a byte a line (PX_LEN may grow) */
                fprintf(f, "g%d.p%d.px%d %u\n", g, i, k, (unsigned)px[i][k]);
        }
    }
    proj.genre = (uint8_t)g0;
    image_load(g0);
    proj.piece = (uint8_t)p0;
    take(proj.piece);
    n = plat_store_load(OBJ_GLOB, raw, sizeof raw);
    if (n > 0) {
        fprintf(f, "glob.tune %d\nglob.midi %d\nglob.theme %d\nglob.rsv %d\nglob.sl_on %u\nglob.sl_end %u\n", glob.tune, glob.midi, glob.theme,
                glob.rsv, (unsigned)glob.sl_on, (unsigned)glob.sl_end);
        for (k = 0; k < 3; k++)
            fprintf(f, "glob.sl_rsv%d %u\nglob.sl_x%d %u\n", k, (unsigned)glob.sl_rsv[k], k, (unsigned)glob.sl_x[k]);
        for (k = 0; k < SL_N; k++)
            fprintf(f, "glob.place%d %u %u %u %u %u\n", k, (unsigned)glob.sl[k], (unsigned)glob.sl_bars[k], (unsigned)glob.sl_mute[k],
                    (unsigned)glob.sl_pat[k], (unsigned)glob.sl_mood[k]);
    }
    n = plat_store_load(OBJ_SET, raw, sizeof raw);
    if (n >= 8 && rd32(raw) == SET_MAGIC && set_have) {
        const int nf = rd16(raw + 4) < EN_NFXS ? rd16(raw + 4) : EN_NFXS;
        for (k = 0; k < nf; k++)
            fprintf(f, "set.fxs%d %u\n", k, (unsigned)set_disk.fxs[k]);
        for (k = 0; k < 12; k++)
            fprintf(f, "set.uiv%d %u\n", k, (unsigned)set_disk.uiv[k]);
        fprintf(f, "set.gx_preset %u\nset.gx_tgt %u\nset.pm_tgt %u\nset.viz %u\nset.gx_pat %u\nset.usb %u\n", (unsigned)set_disk.gx_preset,
                (unsigned)set_disk.gx_tgt, (unsigned)set_disk.pm_tgt, (unsigned)set_disk.viz, (unsigned)set_disk.gx_pat, (unsigned)set_disk.usb);
    }
    if (plat_store_load(OBJ_MMAP, raw, sizeof raw) > 0) {
        fprintf(f, "mm.n %u\n", (unsigned)mm.n);
        for (k = 0; k < mm.n; k++)
            fprintf(f, "mm.e%d %u %u %u\n", k, (unsigned)mm.e[k][0], (unsigned)mm.e[k][1], (unsigned)mm.e[k][2]);
    }
    if (plat_store_load(OBJ_VOICES, raw, sizeof raw) > 0) {
        fprintf(f, "vox.n %d\n", en_user_count());
        for (k = 0; k < en_user_count(); k++) {
            uint32_t h = 2166136261u;
            const uint8_t *v = en_user_get(k);
            for (l = 0; l < 128; l++)
                h = (h ^ v[l]) * 16777619u;
            fprintf(f, "vox.v%d %08x\n", k, (unsigned)h);
        }
    }
}
static void fmt_lock(FILE *f)
{
    char t[16];
    int i, v;
    fprintf(f, "img.layout ver %u hdr %u rec %u par %u cml1 %u acl %u cml2 %u mod %u tail %u len %u\n", IMG_VER, IMG_HDR, IMG_REC,
            (unsigned)REC_PAR, REC_CML1, (unsigned)REC_ACL, (unsigned)REC_CML2, (unsigned)REC_MOD, (unsigned)IMG_TAIL, (unsigned)IMG_LEN);
    fprintf(f, "img.magic %08x\nnpieces %d\nnpat %d\nnlane %d\nprg.len %d\nprk.len %d\n", IMG_MAGIC, NPIECES, EN_NPAT, EN_NLANE, EN_PRG_LEN, EN_PRK_LEN);
    fprintf(f, "room.piece_settings %d\n", (int)REC_CML1 - (int)(REC_PAR + P_NPARAMS));
    fprintf(f, "room.piece_extras %d\n", (int)PX_LEN - (PX_FXAT + PX_NFX));
    fprintf(f, "room.genre_object %d\n", (int)PLAT_STORE_MAX - (int)IMG_LEN);
    fprintf(f, "room.extras_object %d\n", (int)PLAT_STORE_MAX - (int)PX_SIZE);
    fprintf(f, "px.layout magic %08x hdr %u len %u flags %d fxat %d\n", PX_MAGIC, PX_HDR, PX_LEN, PX_FLAGS, PX_FXAT);
    for (i = 0; i < PX_NFX; i++)
        fprintf(f, "pxfx%d %u\n", i, (unsigned)PX_FXS[i]);
    fprintf(f, "bs.layout magic %08x hdr %u len %u\n", BS_MAGIC, BS_HDR, BS_LEN);
    fprintf(f, "glob.magic %08x\nglob.size %u\nglob.layout tune %u midi %u theme %u rsv %u sl %u sl_on %u sl_rsv %u sl_bars %u sl_mute %u sl_end %u "
               "sl_x %u sl_pat %u sl_mood %u\n", GLOB_MAGIC, (unsigned)sizeof(glob_t), (unsigned)offsetof(glob_t, tune),
            (unsigned)offsetof(glob_t, midi), (unsigned)offsetof(glob_t, theme), (unsigned)offsetof(glob_t, rsv), (unsigned)offsetof(glob_t, sl),
            (unsigned)offsetof(glob_t, sl_on), (unsigned)offsetof(glob_t, sl_rsv), (unsigned)offsetof(glob_t, sl_bars),
            (unsigned)offsetof(glob_t, sl_mute), (unsigned)offsetof(glob_t, sl_end), (unsigned)offsetof(glob_t, sl_x),
            (unsigned)offsetof(glob_t, sl_pat), (unsigned)offsetof(glob_t, sl_mood));
    fprintf(f, "set.magic %08x\nset.size %u\nset.uiv 12\n", SET_MAGIC, (unsigned)sizeof(set_t));
    fprintf(f, "vox.layout magic %08x size %u\nmm.layout magic %08x %08x size %u\n", VOX_MAGIC, (unsigned)sizeof(vox_t), MM_MAGIC, MM_MAGIC2,
            (unsigned)sizeof(mmap_t));
    for (i = 0; i < EN_NGENRE; i++)
        fprintf(f, "genre%d %s\n", i, en_genre_name(i));
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        int named = 0;
        fprintf(f, "par%d %s|%d|%d|%d", i, p->name, p->lo, p->hi, p->def);
        if (p->hi - p->lo <= 80)                 /* a list (a sound, a kit, a kind): its values' names, which a saved number means */
            for (v = p->lo; v <= p->hi && !named; v++) {
                const char *c;
                en_param_text(i, v, t);
                for (c = t; *c; c++)
                    named |= (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z');
            }
        if (named)
            for (v = p->lo; v <= p->hi; v++) {
                en_param_text(i, v, t);
                fprintf(f, "|%s", t);
            }
        fputc('\n', f);
    }
}
/* fmtcheck: each line of the kept dump must be in this build's, the same */
static int fmt_check(const char *path)
{
    static char now[1 << 20], want[512];
    FILE *m = fmemopen(now, sizeof now - 1, "w"), *e = fopen(path, "r");
    int bad = 0, lines = 0;
    if (!m || !e) {
        printf("FAIL fmtcheck: cannot open %s\n", path);
        if (m)
            fclose(m);
        if (e)
            fclose(e);
        return 1;
    }
    fmt_dump(m);
    fclose(m);
    while (fgets(want, sizeof want, e)) {
        char key[64], *sp, *hit;
        size_t kl;
        want[strcspn(want, "\r\n")] = 0;
        if (!want[0] || want[0] == '#')
            continue;
        lines++;
        sp = strchr(want, ' ');
        kl = sp ? (size_t)(sp - want) : strlen(want);
        if (kl >= sizeof key)
            kl = sizeof key - 1;
        memcpy(key, want, kl);
        key[kl] = 0;
        hit = now;
        while ((hit = strstr(hit, want)) != 0 && !((hit == now || hit[-1] == '\n') && (hit[strlen(want)] == '\n' || !hit[strlen(want)])))
            hit++;
        if (!hit) {
            char *k2 = now, got[200] = "(none)";
            while ((k2 = strstr(k2, key)) != 0 && !((k2 == now || k2[-1] == '\n') && k2[kl] == ' '))
                k2++;
            if (k2) {
                size_t gl = strcspn(k2, "\n");
                if (gl >= sizeof got)
                    gl = sizeof got - 1;
                memcpy(got, k2, gl);
                got[gl] = 0;
            }
            if (bad < 12)
                printf("FAIL fmtcheck %s: want \"%s\", this build: \"%s\"\n", path, want, got);
            bad++;
        }
    }
    fclose(e);
    printf("  fmtcheck %s: %d lines, %d differ\n", path, lines, bad);
    return bad != 0 || lines == 0;
}
#endif

/* Kick Runner's autopilot (videos only): SELECT turned a notch at a time, as a player would, toward the
 * nearest gate and out of a wall's way */
static int autopilot;
static void kr_autopilot(void)
{
    static uint32_t t;
    int k, l, target = -1, danger = 0, dir = 0;
    float best = 99.0f;
    if (now_ms - t < 110u || kr.over)
        return;
    for (k = 0; k < KR_NROW; k++) {
        float z = kr.row[k].beat - kr.beat;
        if (kr.row[k].done || z < 0.0f || z > 4.0f)
            continue;
        if ((kr.row[k].wall >> kr.lane & 1u) && z < 1.8f)
            danger = 1;
        for (l = 0; l < KR_LANES; l++)
            if ((kr.row[k].gate >> l & 1u) && z < best && z > 0.4f) {
                best = z;
                target = l;
            }
    }
    if (danger) {                                /* the nearest lane with no wall coming soon */
        int d;
        for (d = 1; d < 4 && !dir; d++) {
            int s;
            for (s = 1; s >= -1; s -= 2) {
                int ln = (kr.lane + s * d + KR_LANES) % KR_LANES, ok = 1;
                for (k = 0; k < KR_NROW; k++) {
                    float z = kr.row[k].beat - kr.beat;
                    if (!kr.row[k].done && z >= 0.0f && z < 2.2f && (kr.row[k].wall >> ln & 1u))
                        ok = 0;
                }
                if (ok) {
                    dir = s;
                    break;
                }
            }
        }
    } else if (target >= 0 && target != kr.lane) {
        int d = (target - kr.lane + KR_LANES) % KR_LANES;
        dir = d <= KR_LANES / 2 ? 1 : -1;
    }
    if (dir) {
        enc_acc[EN_SELECT] += dir;
        t = now_ms;
    }
}

/* ---------------------------------------------------------- outputs --- */
static void png_write(const char *path)
{
    /* RGB PNG with stored (uncompressed) deflate blocks: no zlib needed */
    static uint8_t raw[240 * (1 + 240 * 3)];
    FILE *f = fopen(path, "wb");
    uint32_t crc_t[256], i, j, k, n = 0, a = 1, b = 0, pos, len = sizeof raw;
    uint8_t hdr[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_t[i] = c;
    }
    for (j = 0; j < 240; j++) {
        raw[n++] = 0;
        for (i = 0; i < 240; i++) {
            uint16_t p = fb[j * 240 + i];
            p = (uint16_t)((p >> 8) | (p << 8));
            raw[n++] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            raw[n++] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            raw[n++] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    for (i = 0; i < len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
#define PUT32(v) do { uint8_t q_[4] = {(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}; fwrite(q_, 1, 4, f); } while (0)
    fwrite(hdr, 1, 8, f);
    {
        uint8_t ih[17] = {'I', 'H', 'D', 'R', 0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(13u);
        fwrite(ih, 1, 17, f);
        for (i = 0; i < 17; i++)
            c = crc_t[(c ^ ih[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    {
        uint32_t nblk = (len + 65534) / 65535, zlen = 2 + len + nblk * 5 + 4, c = 0xFFFFFFFFu;
        uint8_t *z = malloc(zlen + 4), *q = z;
        memcpy(q, "IDAT", 4);
        q += 4;
        *q++ = 0x78;
        *q++ = 0x01;
        for (pos = 0; pos < len;) {
            uint32_t m = len - pos > 65535 ? 65535 : len - pos;
            *q++ = pos + m >= len ? 1 : 0;
            *q++ = (uint8_t)m;
            *q++ = (uint8_t)(m >> 8);
            *q++ = (uint8_t)~m;
            *q++ = (uint8_t)(~m >> 8);
            memcpy(q, raw + pos, m);
            q += m;
            pos += m;
        }
        *q++ = (uint8_t)(b >> 8);
        *q++ = (uint8_t)b;
        *q++ = (uint8_t)(a >> 8);
        *q++ = (uint8_t)a;
        PUT32(zlen);
        fwrite(z, 1, zlen + 4, f);
        for (i = 0; i < zlen + 4; i++)
            c = crc_t[(c ^ z[i]) & 255] ^ (c >> 8);
        PUT32(~c);
        free(z);
    }
    {
        uint8_t ie[4] = {'I', 'E', 'N', 'D'};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(0u);
        fwrite(ie, 1, 4, f);
        for (i = 0; i < 4; i++)
            c = crc_t[(c ^ ie[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    fclose(f);
}

static FILE *wav;
static uint32_t wav_frames;
static float peak_out;
static void wav_open(const char *path)
{
    static const uint8_t h[44] = {0};
    wav = fopen(path, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_frames = 0;
}
static void wav_close(void)
{
    uint32_t bytes = wav_frames * 4, v;
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36 + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    {
        uint16_t fmt[2] = {1, 2};
        uint32_t r[2] = {44100, 44100 * 4};
        uint16_t al[2] = {4, 16};
        fwrite(fmt, 2, 2, wav);
        fwrite(r, 4, 2, wav);
        fwrite(al, 2, 2, wav);
    }
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

/* --------------------------------------------------------- the device --- */
static double audio_due_ms, render_ns_total, render_budget_ns_total;
static uint32_t ui_due;
static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static uint32_t host_seed;                     /* 0: power-on is the first piece, exact (the tests) */
uint32_t plat_seed(void) { return host_seed; }

static void boot(void)
{
    en_init();
    project_start(plat_seed());
    project_apply();
    lcd_fill(0, 0, 240, 240, 0);
    ui_init();
}

static void run_ms(uint32_t ms)
{
    static int32_t blk[512];
    uint32_t i, k;
    for (i = 0; i < ms; i++) {
        now_ms++;
        while (audio_due_ms <= (double)now_ms) {      /* 256-frame blocks, as the device renders them */
            double t0 = now_ns(), dt;
            cm_now_out = 0;
            en_render(blk, 256, master);
            nh_blk++;
#ifdef EN_WEB
            web_audio(blk, 256);
#endif
            dt = now_ns() - t0;
            render_ns_total += dt;
            render_budget_ns_total += 256.0 * 1e9 / 44100.0;
            cpu_pct = (uint32_t)(100.0 * dt / (256.0 * 1e9 / 44100.0));
            for (k = 0; k < 256; k++) {
                float l = (float)blk[2 * k] / 8388608.0f, r = (float)blk[2 * k + 1] / 8388608.0f;
                int16_t s[2];
                if (fabsf(l) > peak_out)
                    peak_out = fabsf(l);
                if (fabsf(r) > peak_out)
                    peak_out = fabsf(r);
                s[0] = (int16_t)(l * 32767.0f);
                s[1] = (int16_t)(r * 32767.0f);
                if (wav) {
                    fwrite(s, 2, 2, wav);
                    wav_frames++;
                }
            }
            audio_due_ms += 256.0 * 1000.0 / 44100.0;
        }
        if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            if (autopilot && kr.on)
                kr_autopilot();
            ui_frame();
        } else {
            ui_input_only();
        }
    }
}

/* ---------------------------------------------------------- script --- */
static const char *const BTN_N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                      "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_N[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

static int find_name(const char *const *t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}

static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int key_of(const char *s)
{
    if (s[0] == 'w')
        return WHITE_K[atoi(s + 1) & 15];
    if (s[0] == 'b')
        return BLACK_K[atoi(s + 1) % 11];
    return atoi(s);
}

static int expect_quiet;                         /* (waitfor: the tries before the last say nothing) */
static int expect(const char *what, const char *val)
{
    int got;
    if (!strcmp(what, "playing"))
        got = en_playing;
    else if (!strcmp(what, "frozen"))
        got = en_frozen;
    else if (!strcmp(what, "view"))
        got = ui.view;
    else if (!strcmp(what, "viz"))                  /* the visualizer chosen (VZ_*) */
        got = ui.viz;
    else if (!strcmp(what, "dd_changes"))          /* drum drift: patterns changed, mutations, bars left */
        got = (int)en_dd_changes;
    else if (!strcmp(what, "dd_mutations"))
        got = (int)en_dd_mutations;
    else if (!strcmp(what, "dd_left"))
        got = en_dd_left;
    else if (!strcmp(what, "pattern_synced"))
        got = proj.par[P_PATTERN] == en_pat_next;
    else if (!strcmp(what, "runner"))              /* the secret game (SEL + EDIT): on, lane, rows, gates, ... */
        got = kr.on;
    else if (!strcmp(what, "kr_lane"))
        got = kr.lane;
    else if (!strcmp(what, "kr_rows"))
        got = (int)kr.rows_born;
    else if (!strcmp(what, "kr_gates"))
        got = (int)kr.gates;
    else if (!strcmp(what, "kr_crashes"))
        got = (int)kr.crashes;
    else if (!strcmp(what, "kr_layer"))
        got = kr.layer;
    else if (!strcmp(what, "kr_over"))
        got = kr.over;
    else if (!strcmp(what, "piece"))
        got = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        got = proj.pool;
    else if (!strcmp(what, "touched"))            /* (0.16.18) the page's knob lit by the last turn (or a learned setting), -1 none; a message on screen */
        got = (int32_t)(ui.touch_until - plat_ms()) > 0 ? ui.touched : -1;
    else if (!strcmp(what, "msgon"))
        got = (int32_t)(ui.msg_until - plat_ms()) > 0;
    else if (!strcmp(what, "acidgate"))           /* the acid line's steps that play (a 16-bit mask) */
        got = en_acid_gate;
    else if (!strncmp(what, "acidoff", 7))        /* the acid line's step N: scale steps above the chord */
        got = en_acid_off[atoi(what + 7) & 15];
    else if (!strncmp(what, "acidoct", 7))        /* its octave */
        got = en_acid_oct[atoi(what + 7) & 15];
    else if (!strncmp(what, "acidkind", 8)) {     /* the acid line's step N as the page says: 0 rest, 1 note, 2 accent, 3 slide, 4 accent and slide */
        const int s = atoi(what + 8) & 15, g = en_acid_gate >> s & 1, ac = en_acid_acc >> s & 1, sl = en_acid_slide >> s & 1;
        got = !g ? 0 : ac ? (sl ? 4 : 2) : (sl ? 3 : 1);
    }
    else if (!strcmp(what, "acidacc"))
        got = en_acid_acc;
    else if (!strcmp(what, "acidslide"))
        got = en_acid_slide;
    else if (!strcmp(what, "hushed"))             /* the output is faded out (a store waiting to write) */
        got = en_hushed;
    else if (!strcmp(what, "storewait"))          /* a store is waiting for the fade */
        got = ui.store_wait;
    else if (!strcmp(what, "dirty"))              /* the bank has changes not yet in flash */
        got = project_pending();
    else if (!strcmp(what, "saved"))              /* the pieces in the bank (bit i: piece i) */
        got = (int)(project_saved(0) | project_saved(1) << 1 | project_saved(2) << 2 | project_saved(3) << 3 |
                    project_saved(4) << 4 | project_saved(5) << 5 | project_saved(6) << 6 | project_saved(7) << 7 |
                    project_saved(8) << 8 | project_saved(9) << 9);
    else if (!strcmp(what, "parsbad")) {          /* settings out of range: the playing piece's and every slot's */
        int p, i;
        got = 0;
        for (p = 0; p < P_NPARAMS; p++) {
            const en_param_t *pi = en_param_info(p);
            got += proj.par[p] < pi->lo || proj.par[p] > pi->hi;
            for (i = 0; i < NPIECES; i++)
                got += proj.slot[i].par[p] < pi->lo || proj.slot[i].par[p] > pi->hi;
        }
        for (i = 0; i < NPIECES; i++)
            got += !(proj.slot[i].pool & 1u) || (proj.slot[i].pool >> 12);
    } else if (!strcmp(what, "parwide")) {        /* settings spanning 255 steps or more (none may: a stored 0xFF means "the recipe's", project.c REC_RECIPE) */
        int p;
        got = 0;
        for (p = 0; p < P_NPARAMS; p++)
            got += en_param_info(p)->hi - en_param_info(p)->lo >= 255;
    } else if (!strcmp(what, "uservoices"))         /* the player's DX7 voices loaded */
        got = en_user_count();
    else if (!strcmp(what, "edited"))             /* the pieces changed since taken up (bit i: piece i) */
        got = proj.edited;
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "clocks_out"))
        got = (int)clock_out_count;
    else if (!strcmp(what, "noteons_out"))       /* (0.17.47) note-ons sent since the start (or midireset) */
        got = (int)noteon_out_count;
    else if (!strcmp(what, "cmpcs"))             /* (0.17.53) the pluck's notes out (channel 2) since midireset: their pitch classes (bit 0 C) */
        got = (int)cm_pcs_out;
    else if (!strcmp(what, "cmmax"))             /* and the most at once (in one block) */
        got = cm_max_out;
    else if (!strcmp(what, "phchords"))          /* the phrase's sixteenths holding a chord */
    {
        int s2;
        got = 0;
        for (s2 = 0; s2 < EN_PH_N; s2++)
            got += en_ph_note[s2] && en_ph_iv[s2];
    }
    else if (!strcmp(what, "starts_out"))        /* (0.17.47) MIDI Starts sent (not reset by midireset) */
        got = (int)start_out_count;
    else if (!strcmp(what, "lkrole"))            /* (0.17.47) the link: 0 alone, 1 leading, 2 following, 3 no lead */
        got = lk_role;
    else if (!strncmp(what, "lkq", 3) || !strncmp(what, "lkmute", 6)) {   /* the engine's link bits (EN_LK_*), the tracks it leaves to the partner */
        int en_test_lk(int w);
        got = en_test_lk(what[2] == 'q' ? 0 : 1);
    }
    else if (!strcmp(what, "capt"))              /* (0.17.48) Capture's last key: its sixteenth (16 bits), how long it was held */
        got = ui.cap_t[(ui.cap_w - 1) & 31];
    else if (!strcmp(what, "caplen"))
        got = ui.cap_len[(ui.cap_w - 1) & 31];
    else if (!strcmp(what, "capstep"))           /* its place in its bar (0..15) */
        got = ui.cap_t[(ui.cap_w - 1) & 31] & 15;
    else if (!strcmp(what, "capdelta"))          /* (0.17.48) Capture's last key: its sixteenth less the one playing now (0 on it, 1 the next) */
        got = (int16_t)(uint16_t)(ui.cap_t[(ui.cap_w - 1) & 31] - (uint16_t)en_bars);
    else if (!strcmp(what, "barstep"))           /* the sixteenth in its bar now (0..15) */
        got = (int)(en_bars & 15u);
    else if (!strcmp(what, "bsaccents")) {       /* (0.17.48) the bass steps that are accents (kinds 2 and 4) */
        int s3;
        got = 0;
        for (s3 = 0; s3 < 16; s3++)
            got += EN_BS_KIND(en_bs[s3]) == 2 || EN_BS_KIND(en_bs[s3]) == 4;
    }
    else if (!strcmp(what, "stepphase"))         /* (0.17.48) how far into its sixteenth the engine is, in hundredths */
        got = (int)(en_step_frac * 100.0f);
    else if (!strcmp(what, "qdrops"))            /* (0.17.48) the engine's commands dropped */
        got = (int)en_q_drops;
    else if (!strcmp(what, "sixteenths"))
        got = (int)en_bars;
    else if (!strcmp(what, "fillq")) {           /* (0.17.16) fills asked of the engine (the song place's last bar) */
        extern int en_host_fillq;
        got = en_host_fillq;
    } else if (!strcmp(what, "arrange"))         /* (0.17.16) Song's arrangement on (a set-list place with bars: off) */
        got = en_arrange;
    else if (!strcmp(what, "arrm")) {            /* the tracks it rests now */
        extern int en_host_arrm(void);
        got = en_host_arrm();
    }
    else if (!strcmp(what, "hashsame"))           /* (0.17.11) the notes since the armed start as those kept (and some were played) */
        got = nh == nh_keep && nh_n > 20u;
    else if (!strcmp(what, "notes"))              /* the notes hashed */
        got = (int)nh_n;
    else if (!strcmp(what, "codeexact")) {        /* the code playing gives the piece as it is */
        extern int ui_code_now(char *b);
        char b[16];
        got = ui_code_now(b);
    } else if (!strcmp(what, "lfo16")) {            /* the pad LFO's place in its cycle, 0 .. 15 (0.17.10) */
        extern int en_test_lfo16(void);
        got = en_test_lfo16();
    } else if (!strcmp(what, "lfobar")) {         /* the pad LFO (at 1 bar) against the bar: its place less the bar's step, 0 when locked, either way of a step */
        extern int en_test_lfo16(void), en_test_step16(void);
        const int d = (en_test_lfo16() - en_test_step16() + 16) % 16;
        got = d == 15 ? 0 : d;
    }
    else if (!strcmp(what, "ext"))
        got = en_ext;
    else if (!strcmp(what, "midi_out"))
        got = (int)midi_out_count;
    else if (!strcmp(what, "pad_level"))
        got = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "bass_level"))
        got = (int)(en_bass_level * 100.0f);
    else if (!strcmp(what, "chordroot"))
        got = en_chord_root;
    else if (!strcmp(what, "bspos"))              /* the bass step playing (0.16.29; -1 not playing its steps) */
        got = en_bs_pos;
    else if (!strncmp(what, "bskind", 6))         /* bskindN, bsnoteN, bsoctN: bass step N (0.16.29) */
        got = EN_BS_KIND(en_bs[atoi(what + 6) & 15]);
    else if (!strncmp(what, "bsnote", 6))
        got = EN_BS_NOTE(en_bs[atoi(what + 6) & 15]);
    else if (!strncmp(what, "bsoct", 5))
        got = EN_BS_OCT(en_bs[atoi(what + 5) & 15]);
    else if (!strcmp(what, "bsany"))              /* any bass step set (not a rest) */
        got = (en_bs[0] | en_bs[1] | en_bs[2] | en_bs[3] | en_bs[4] | en_bs[5] | en_bs[6] | en_bs[7] | en_bs[8] | en_bs[9] | en_bs[10] | en_bs[11] | en_bs[12] | en_bs[13] | en_bs[14] | en_bs[15]) != 0;
    else if (!strcmp(what, "walknext"))           /* the walk's next degree, a chord ahead (0.16.27; -1 none) */
        got = en_walk_next;
    else if (!strcmp(what, "chordalt"))           /* the chord playing: 0 plain, 1 a secondary dominant, 2 borrowed */
        got = en_chord_alt;
    else if (!strcmp(what, "midimap"))            /* the MIDI out map (USB & MIDI page): 0 GM, 1 Model:Samples, 2 MPC Sample, 3 SP-404 */
        got = ui.midi_map;
    else if (!strcmp(what, "minpads"))            /* MIDI in (MIDI IN page): pads on, the channel (0 all); a melodic pad's note sounding; a drum lit by a hit (its glow over half) */
        got = ui.min_pads;
    else if (!strcmp(what, "minch"))
        got = ui.min_ch;
    else if (!strcmp(what, "usbhost"))            /* the USB host asked for (MIDI IN page; never kept) */
        got = host_uh_want;
    else if (!strncmp(what, "padnote", 7))
        got = ui.pad_note[atoi(what + 7) % 48];
    else if (!strncmp(what, "drumglow", 8))
        got = en_drum_glow[atoi(what + 8) % EN_NDRUM] > 0.5f;
    else if (!strcmp(what, "progpos"))            /* the progression: the slot playing (-1 the walk), Play, Drift's unit, a slot's chord (0 none, 1.. a degree) and bars (1, 2, 4, 8) */
        got = en_prog_pos;
    else if (!strcmp(what, "prgplay"))
        got = en_prg_get(proj.prg, EN_PRG_PLAY, 1);
    else if (!strcmp(what, "prgunit"))
        got = en_prg_get(proj.prg, EN_PRG_UNIT, 2);
    else if (!strncmp(what, "prgslot", 7))
        got = en_prg_get(proj.prg, 3 * (atoi(what + 7) & 7), 3);
    else if (!strncmp(what, "prgtype", 7))       /* a slot's chord type (0.16.6: 0 the piece's, else EN_CH_*) */
        got = (int)(((uint32_t)proj.prk[0] | (uint32_t)proj.prk[1] << 8 | (uint32_t)proj.prk[2] << 16) >> 3 * (atoi(what + 7) & 7) & 7u);
    else if (!strncmp(what, "prgbars", 7))
        got = 1 << en_prg_get(proj.prg, 24 + 2 * (atoi(what + 7) & 7), 2);
    else if (!strcmp(what, "chordiv")) {          /* the pad's notes as intervals over the chord's root (bits) */
        int k;
        got = 0;
        for (k = 0; k < EN_NPAD; k++)
            got |= 1 << ((en_chord[k] - en_chord_root + 120) % 12);
    }
    else if (!strncmp(what, "chord", 5))          /* chordN: pad voice N's note */
        got = en_chord[atoi(what + 5) & 3];
    else if (!strcmp(what, "sec"))                /* the section: 0 groove, 1 breakdown, 2 build */
        got = en_sec;
    else if (!strcmp(what, "aflive"))             /* AUTO FX (0.16.8): one sounding now; one planned (or sounding); its effect; its end on a phrase line (1), off it (0), none (-1) */
        got = ui.af_live;
    else if (!strcmp(what, "afplanned"))
        got = ui.af_end != 0;
    else if (!strcmp(what, "affx"))
        got = ui.af_end ? ui.af_fx : -1;
    else if (!strcmp(what, "afline"))
        got = ui.af_end ? (int)(ui.af_end % (64u << (ui.fxs[EN_FXS_AF_EVERY] & 3)) == 0) : -1;
    else if (!strcmp(what, "fxmask"))             /* the effects held (FX menu) */
        got = (int)(uint32_t)ui.fx_mask;
    else if (!strncmp(what, "fxbit", 5))          /* fxbitN: effect N (FX_*) held (the mask's upper half too) */
        got = (int)(ui.fx_mask >> (atoi(what + 5) & 63) & 1u);
    else if (!strcmp(what, "bassheld"))           /* the bass's held note, its pitch class (0.16.21: PEDAL) */
        got = en_bass_held() % 12;
    else if (!strncmp(what, "padheld", 7))        /* padheldN: pad voice N's note, its pitch class */
        got = en_pad_held(atoi(what + 7)) % 12;
    else if (!strcmp(what, "entrkmute"))          /* the mutes the engine has (0.16.12: a MIDI pad changed the screen only) */
        got = (int)en_trk_heard();
    else if (!strcmp(what, "trkmute"))            /* the tracks muted (GLO held, the keys): a bit a track */
        got = ui.trk_m;
    else if (!strcmp(what, "acidsteps")) {       /* (0.17.31) how many of the acid line's steps play */
        int s2;
        got = 0;
        for (s2 = 0; s2 < 16; s2++)
            got += en_acid_gate >> s2 & 1;
    } else if (!strcmp(what, "bssteps")) {       /* and of the bass steps (not rests) */
        int s2;
        got = 0;
        for (s2 = 0; s2 < 16; s2++)
            got += EN_BS_KIND(en_bs[s2]) >= 1 && EN_BS_KIND(en_bs[s2]) <= 4;
    }
    else if (!strncmp(what, "kitnamelen", 10))     /* (0.17.38) a loaded kit slot's name, its length (an empty slot: "Empty N", 7) */
        got = (int)strlen(EN_KIT_NAME[EN_NKIT + EN_NSKIT_BI + (atoi(what + 10) % EN_NUKIT)]);
    else if (!strncmp(what, "kitempty", 8))       /* 1: that slot holds no kit */
        got = !strncmp(EN_KIT_NAME[EN_NKIT + EN_NSKIT_BI + (atoi(what + 8) % EN_NUKIT)], "Empty", 5);
    else if (!strcmp(what, "extch"))             /* (0.17.31) a chord from outside on (MIDI in CHORDS) */
        got = en_extch;
    else if (!strcmp(what, "gesture"))           /* (0.17.31) the gesture recorder: 0 off, 1 armed, 2 writing, 3 playing, 4 kept for AUTO FX (0.17.44) */
        got = gs_st;
    else if (!strcmp(what, "gsshot"))            /* (0.17.44) AUTO FX is playing the kept gesture */
        got = gs_shot;
    else if (!strcmp(what, "phrase"))            /* (0.17.45) the phrase recorder: 0 off, 1 armed, 2 writing, 3 playing */
        got = ph_st;
    else if (!strcmp(what, "phnotes") || !strcmp(what, "phmax")) {   /* the notes written; the longest held (sixteenths) */
        int s2, c = 0, mx = 0;
        for (s2 = 0; s2 < EN_PH_N; s2++)
            if (en_ph_note[s2]) {
                c++;
                if ((en_ph_len[s2] & 0x7F) > mx)
                    mx = en_ph_len[s2] & 0x7F;
            }
        got = what[2] == 'n' ? c : mx;
    }
    else if (!strncmp(what, "ph_", 3)) {         /* the engine's: ph_v (its voice, 255 none), ph_n (length), ph_cur (the note sounding) */
        int en_test_ph(int w);
        got = en_test_ph(what[3] == 'v' ? 0 : what[3] == 'n' ? 1 : 2);
    }
    else if (!strcmp(what, "gsfx"))              /* the effects written into the gesture: how many sixteenths hold one */
    {
        int s2;
        got = 0;
        for (s2 = 0; s2 < 64; s2++)
            got += gs_fx[s2] != 0;
    }
    else if (!strcmp(what, "gsmask"))            /* the FX knobs it plays */
        got = gs_mask;
    else if (!strcmp(what, "gslen"))             /* its loop in sixteenths */
        got = gs_len;
    else if (!strncmp(what, "fxk", 3))           /* fxkN: the FX knobs' values (filter, crush, throw, wash) */
        got = ui.fxk[atoi(what + 3) & 3];
    else if (!strcmp(what, "fxpage"))             /* the FX menu's page: 0 the keys, 1.. an effect's edit page */
        got = ui.fx_menu ? ui.fx_page : -1;
    else if (!strcmp(what, "fxmaskhi"))           /* the effects held: bits 32 and up */
        got = (int)(uint32_t)(ui.fx_mask >> 32);
    else if (!strcmp(what, "scenequant"))         /* scenes on the next bar (1) or at once (0) */
        got = ui.scene_quant;
    else if (!strcmp(what, "djlen"))              /* the auto-DJ's groove length: 0 .. 3 (8, 16, 32, 64 bars) */
        got = ui.dj_len;
    else if (!strcmp(what, "djbreak"))
        got = ui.dj_break;
    else if (!strcmp(what, "sceneq"))             /* the scene waiting for the bar: slot + 1 */
        got = ui.scene_q;
    else if (!strcmp(what, "mood"))               /* the mood macro, 0 .. 100 */
        got = ui.mood;
    else if (!strcmp(what, "flexq"))              /* Flex Beat: quantize, mode, state (0 idle, 1 armed, 2 playing), one shots done, the last pad */
        got = ui.fxs[EN_FXS_FX_QUANT];
    else if (!strcmp(what, "flexmode"))
        got = ui.fxs[EN_FXS_FX_MODE];
    else if (!strcmp(what, "flexstate"))
        got = en_flx_state;
    else if (!strcmp(what, "flexdone"))
        got = (int)en_flx_done;
    else if (!strcmp(what, "flexlast"))
        got = ui.flex_last;
    else if (!strcmp(what, "jamv"))               /* the JAM page: voice, keys */
        got = ui.jam_voice;
    else if (!strcmp(what, "jamm"))
        got = ui.jam_mode;
    else if (!strcmp(what, "smp"))                /* the sampler's voices sounding */
        got = en_smp_active;
    else if (!strcmp(what, "smpmode"))            /* the sampler page's Keys setting */
        got = ui.fxs[EN_FXS_SM_MODE];
    else if (!strcmp(what, "saver"))              /* the STAGE page: the screensaver setting, it is on, the lights */
        got = ui.saver;
    else if (!strcmp(what, "saveron"))
        got = ui.saver_on;
    else if (!strcmp(what, "dyvoices"))           /* the DY set in use: always 1 since 0.17.75 */
        got = en_fm_dy;
    else if (!strcmp(what, "lights"))
        got = ui.lights;
    else if (!strcmp(what, "zoom"))               /* (0.16.1) the SCREEN page's Zoom, and the knob it shows now (-1: none) */
        got = glob.sl_rsv[0];
    else if (!strcmp(what, "zoomknob"))
        got = zoom_knob();
    else if (!strcmp(what, "acidans"))            /* (0.17.22) the acid answers this bar */
        got = en_acid_ans;
    else if (!strcmp(what, "bassans"))            /* (0.17.30) the bass answers this bar */
        got = en_bass_ans;
    else if (!strcmp(what, "pedal"))              /* (0.17.22) Pedal: bit 0 the bass, bit 1 the pad */
        got = glob.sl_rsv[2] & 3u;
    else if (!strcmp(what, "mix"))                /* the SET page's Mix, and the glide's bars (0: none) */
        got = glob.sl_rsv[1];
    else if (!strcmp(what, "mixbars"))
        got = ui.mx_bars;
    else if (!strcmp(what, "samplecur"))          /* the SAMPLE page: the sound playing (-1 none), a run going, dry */
        got = ui.smp_cur;
    else if (!strcmp(what, "samplerun"))
        got = ui.smp_t0 != 0;
    else if (!strcmp(what, "sampledry"))
        got = ui.smp_dry;
    else if (!strcmp(what, "glide"))              /* an FX knob still gliding to a scene's value (bits) */
        got = ui.glide_on;
    else if (!strcmp(what, "keyon"))              /* the drop's key change is on */
        got = ui.key_on;
    else if (!strcmp(what, "keyset"))
        got = ui.key_set;
    else if (!strcmp(what, "moodacid"))           /* the acid density as the engine is given it */
        got = mood_val(P_AC_DENS, proj.par[P_AC_DENS]);
    else if (!strcmp(what, "genre"))              /* the bank of pieces: 0 the original, 1 lofi */
        got = proj.genre;
    else if (!strcmp(what, "fxbank"))             /* the FX keys' bank (the dial in the FX menu) */
        got = fx_bank;
    else if (!strcmp(what, "undook"))
        got = ui.undo_ok;
    else if (!strcmp(what, "gxpat"))              /* the gate's pattern (bit s: step s open) */
        got = ui.gx_pat;
    else if (!strcmp(what, "gxpreset"))
        got = ui.gx_preset;
    else if (!strncmp(what, "fxs", 3))            /* fxsN: the effects' setting N (EN_FXS_*) */
        got = ui.fxs[atoi(what + 3) % EN_NFXS];
    else if (!strcmp(what, "scenes"))             /* the scenes stored (a bit a slot) */
        got = ui.scene_set;
    else if (!strncmp(what, "setsl", 5))          /* the set list's slot N (0.15.21): genre * 10 + piece + 1, 0 empty */
        got = glob.sl[atoi(what + 5) % SL_N];
    else if (!strcmp(what, "setpos"))             /* the slot playing (255 none) */
        got = ui.sl_pos;
    else if (!strcmp(what, "seton"))              /* the set list's Play */
        got = glob.sl_on;
    else if (!strncmp(what, "setbars", 7))        /* (0.17.13, song mode) place N's bars, its parts left out, Once */
        got = glob.sl_bars[atoi(what + 7) % SL_N];
    else if (!strncmp(what, "setmute", 7))
        got = glob.sl_mute[atoi(what + 7) % SL_N];
    else if (!strcmp(what, "setend"))
        got = glob.sl_end;
    else if (!strncmp(what, "setpat", 6))         /* (0.17.14) place N's pattern (0 the piece's), its energy (0 as is, else mood + 1) */
        got = glob.sl_pat[atoi(what + 6) % SL_N];
    else if (!strncmp(what, "setmood", 7))
        got = glob.sl_mood[atoi(what + 7) % SL_N];
    else if (!strcmp(what, "setbar"))             /* the bar of the place playing (from 0) */
        got = en_playing ? (int)(sl_bar_now() - sl_b0) : -1;
    else if (!strcmp(what, "djon"))               /* the auto-DJ: on */
        got = ui.dj_on;
    else if (!strcmp(what, "djphase"))            /* groove 0, breakdown 1, build 2 */
        got = ui.dj_phase;
    else if (!strcmp(what, "djn"))                /* bars in the phase */
        got = ui.dj_n;
    else if (!strcmp(what, "trksolo"))
        got = ui.trk_s;
    else if (!strcmp(what, "tempo"))
        got = proj.par[P_TEMPO];
    else if (!strcmp(what, "pat"))                /* the drum pattern playing (0..5) */
        got = en_pat_now;
    else if (!strncmp(what, "lanesrc", 7) && what[7] >= '0' && what[7] <= '5')   /* the pattern lane N plays (0..5) */
        got = en_lane_src[what[7] - '0'];
    else if (!strncmp(what, "lanemask", 8) && what[8] >= '0' && what[8] <= '5')  /* lane N of the pattern playing, at its density, as it would sound (a 16-bit mask) */
        got = en_lane_mask(what[8] - '0', en_lane_src[what[8] - '0'], proj.par[P_DENS + (what[8] - '0')]);
    else if (!strncmp(what, "lanewrit", 8) && what[8] >= '0' && what[8] <= '5')  /* the same, as written */
        got = en_pats[en_lane_src[what[8] - '0']][what[8] - '0'];
    else if (!strncmp(what, "mod", 3) && what[3] >= '0' && what[3] <= '5' && what[4] == '_')   /* a lane's step trick byte: modL_S (lane L, step S, 0 .. 15) */
        got = en_mod[what[3] - '0'][atoi(what + 5) & 15];
    else if (!strcmp(what, "rsvalid"))            /* the last restore command's check: 1 valid */
        got = rs_valid;
    else if (!strcmp(what, "rsstate"))            /* 0 none, 1 waiting, 2 written, 3 not written */
        got = ui_restore_state();
    else if (!strcmp(what, "storelock"))
        got = store_lock;
    else if (!strcmp(what, "patkey"))             /* the held pattern's state: 1 a pattern key is down, 2 one let go and waiting to go back */
        got = ui.pat_key >= 0 ? 1 : ui.pat_up ? 2 : 0;
    else if (!strcmp(what, "patnext"))            /* the one waiting for the bar */
        got = en_pat_next;
    else if (!strncmp(what, "lane", 4))           /* laneL: lane L's steps in the pattern chosen (bits) */
        got = (int)en_pats[proj.par[P_PATTERN]][atoi(what + 4) % EN_NLANE];
    else if (!strncmp(what, "pats", 4))           /* patsP_L: pattern P's lane L (bits) */
        got = (int)en_pats[atoi(what + 4) % EN_NPAT][atoi(strchr(what, '_') ? strchr(what, '_') + 1 : "0") % EN_NLANE];
    else if (!strcmp(what, "fxlock"))             /* the effects locked on (SAVE with their keys held) */
        got = (int)ui.fx_lock;
    else if (!strcmp(what, "fxon"))               /* the FX menu is on */
        got = ui.fx_menu;
    else if (!strcmp(what, "held"))               /* keys held playing a voice */
        got = en_held;
    else if (!strcmp(what, "remote"))             /* remote (SEL + GLO) on; the browser answering */
        got = rm.on;
    else if (!strcmp(what, "remotelink"))
        got = rm.link;
    else if (!strncmp(what, "ccout", 5))          /* ccoutN: control changes N sent so far */
        got = (int)cc_out_n[atoi(what + 5) & 127];
    else if (!strcmp(what, "lastvel"))            /* the last note-on out's velocity */
        got = last_vel;
    else if (!strcmp(what, "ccmode"))             /* MIDI IN's Knob CCs: bit 0 in, bit 1 out */
        got = ui.cc_mode;
    else if (!strcmp(what, "lastout"))            /* the last MIDI out: status << 16 | d1 << 8 | d2 */
        got = (int)last_out;
    else if (!strcmp(what, "litbtn"))             /* the buttons lit (bit b: B_*) */
        got = (int)lit_btn;
    else if (!strcmp(what, "keychord")) {         /* the keys show the chord playing (0.15.21): lit = its tones, dim = the rest of the scale, none both, some dim */
        const uint32_t ch = chord_pcs(), ap = pool_abs();
        uint32_t i, ok = dim_keys != 0 && !(lit_keys & dim_keys);
        for (i = 0; i < NKEYS; i++) {
            const uint32_t pc = (5u + i) % 12u, l = lit_keys >> i & 1u, d = dim_keys >> i & 1u;
            if (l != (ch >> pc & 1u) || d != ((ap >> pc & 1u) && !(ch >> pc & 1u)))
                ok = 0;
        }
        got = (int)ok;
    } else if (!strcmp(what, "dimkeys"))            /* the keys lit dim (the scale while a chord plays, 0.15.21) */
        got = (int)dim_keys;
    else if (!strcmp(what, "litkeys"))            /* the keys lit (bit k: key k) */
        got = (int)lit_keys;
    else if (!strcmp(what, "seqlane"))            /* the sequencer's lane (OCT- / OCT+) */
        got = ui.lane;
    else if (!strcmp(what, "acidnotes"))          /* the acid loop's steps that play */
        got = __builtin_popcount(en_acid_gate);
    else if (!strcmp(what, "theme"))
        got = proj.par[P_THEME];
    else if (!strncmp(what, "enpar", 5))        /* enparN: the engine's own value (the mood's, the JAM page's hand switch) */
        got = en_par_now(atoi(what + 5));
    else if (!strncmp(what, "par", 3))          /* parN: parameter N (amb.h P_*) */
        got = proj.par[atoi(what + 3)];
    else if (!strcmp(what, "peak_db_max") || !strcmp(what, "peak_db_min")) {
        double db = 20.0 * log10(peak_out > 1e-9f ? peak_out : 1e-9f);
        int bad = what[8] == 'm' && what[9] == 'a' ? db > atof(val) : db < atof(val);
        printf("%s peak %.1f dBFS (%s %s)\n", bad ? "FAIL" : "  ok", db, what + 8, val);
        return bad;
    } else {
        printf("FAIL unknown expect %s\n", what);
        return 1;
    }
    if (val[0] == '<' || val[0] == '>') {
        int lim = (int)strtol(val + 1, 0, 0);
        if (val[0] == '<' ? got >= lim : got <= lim) {
            if (!expect_quiet)
                printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        if (!expect_quiet)
            printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        if (!expect_quiet)
            printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
    if (!expect_quiet)
        printf("  ok %s == %d\n", what, got);
    return 0;
}

#ifndef EN_WEB
int main(int argc, char **argv)
{
    FILE *sc;
    char line[512], out[400];
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: amb_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    sc = fopen(argv[1], "r");
    if (!sc) {
        perror(argv[1]);
        return 2;
    }
    snprintf(out, sizeof out, "%s/midi_out.txt", dir);
    midi_log = fopen(out, "w");
    ui_splash_ms = 0u;                           /* the tests start at the UI (script: splash MS) */
    boot();
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run_ms((uint32_t)atoi(a));
        else if (!strcmp(cmd, "press") || !strcmp(cmd, "release") || !strcmp(cmd, "tap")) {
            int i = find_name(BTN_N, NB, a);
            if (i < 0) {
                printf("line %d: no button %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] != 'r')
                held_btn |= 1u << i;
            if (cmd[0] == 't')
                run_ms(40);
            if (cmd[0] != 'p')
                held_btn &= ~(1u << i);
            run_ms(cmd[0] == 't' ? 40 : 2);
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            int k = key_of(a);
            if (cmd[0] == 't') {
                held_keys |= 1u << k;
                run_ms(60);
                held_keys &= ~(1u << k);
                run_ms(30);
            } else {
                if (!strcmp(b, "down"))
                    held_keys |= 1u << k;
                else
                    held_keys &= ~(1u << k);
                run_ms(2);
            }
        } else if (!strcmp(cmd, "strum")) {          /* strum A B [MS]: a finger across the white keys */
            int x = atoi(a), y = atoi(b), gap = c[0] ? atoi(c) : 25, s = y >= x ? 1 : -1;
            for (;; x += s) {
                held_keys |= 1u << WHITE_K[x & 15];
                run_ms((uint32_t)gap);
                held_keys &= ~(1u << WHITE_K[x & 15]);
                if (x == y)
                    break;
            }
            run_ms(2);
        } else if (!strcmp(cmd, "turn")) {
            int e = find_name(ENC_N, NE, a), n = atoi(b), s = n > 0 ? 1 : -1;
            uint32_t gap = c[0] ? (uint32_t)atoi(c) : 80u;
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            for (; n; n -= s) {
                enc_acc[e] += s;
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
            accel_off = 1;
            run_ms(1);
            accel_off = 0;
        } else if (!strcmp(cmd, "master"))
            master = (uint32_t)atoi(a);
        else if (!strcmp(cmd, "midi")) {
            uint32_t s = (uint32_t)strtoul(a, 0, 16), d1 = (uint32_t)strtoul(b, 0, 16), d2 = (uint32_t)strtoul(c, 0, 16);
            uint32_t cin = s >= 0xF0 ? 0x0F : s >> 4;
            min_q[mi_w++ % MQ] = cin | s << 8 | d1 << 16 | d2 << 24;
        } else if (!strcmp(cmd, "wav")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            wav_open(out);
        } else if (!strcmp(cmd, "wavstop"))
            wav_close();
        else if (!strcmp(cmd, "peakreset"))
            peak_out = 0.0f;
        else if (!strcmp(cmd, "frames")) {           /* frames N [FPS]: a video's frames, at their exact times
                                                         * from the first: frame_NNNNN.png, and frames.txt
                                                         * (frame, lit buttons, lit keys, held buttons, held keys) */
            static FILE *flog;
            static uint32_t fn, ft0, fps = 30;
            int k, n = atoi(a);
            if (b[0])
                fps = (uint32_t)atoi(b);
            if (!flog) {
                snprintf(out, sizeof out, "%s/frames.txt", dir);
                flog = fopen(out, "w");
                ft0 = now_ms;
            }
            for (k = 0; k < n; k++, fn++) {
                uint32_t t = ft0 + (uint32_t)((uint64_t)fn * 1000u / fps);
                if (t > now_ms)
                    run_ms(t - now_ms);
                snprintf(out, sizeof out, "%s/frame_%05u.png", dir, fn);
                png_write(out);
                fprintf(flog, "%u %u %u %u %u\n", fn, lit_btn, lit_keys, held_btn, held_keys);
            }
            fflush(flog);
        } else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            png_write(out);
        } else if (!strcmp(cmd, "leds")) {
            int i;
            printf("  leds: buttons");
            for (i = 0; i < NB; i++)
                if (lit_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | white ");
            for (i = 0; i < 16; i++)
                printf("%c", (lit_keys >> WHITE_K[i] & 1u) ? '#' : '.');
            printf(" black ");
            for (i = 0; i < 11; i++)
                printf("%c", (lit_keys >> BLACK_K[i] & 1u) ? '#' : '.');
            printf("\n");
        } else if (!strcmp(cmd, "expect"))
            fails += expect(a, b);
        else if (!strcmp(cmd, "waitfor")) {         /* waitfor WHAT VALUE MS (0.16.8): the simulation runs on until the expectation holds or MS have passed,
                                                         * then it is checked (for what comes at a moment the script cannot know: AUTO FX's dice) */
            uint32_t t;
            expect_quiet = 1;
            for (t = 0; t < (uint32_t)atoi(c) && expect(a, b); t += 10u)
                run_ms(10u);
            expect_quiet = 0;
            fails += expect(a, b);
        }
        else if (!strcmp(cmd, "storefuzz")) {         /* storefuzz SEED: the project object filled with damage:
                                                         * random bytes behind a header that is now valid, now not */
            static uint8_t bk[PLAT_STORE_MAX];
            uint32_t r = (uint32_t)strtoul(a, 0, 10) * 2654435761u + 1u, k, n;
#define FZ() (r ^= r << 13, r ^= r >> 17, r ^= r << 5, r)
            n = FZ() % (PLAT_STORE_MAX + 1u);
            for (k = 0; k < n; k++)
                bk[k] = (uint8_t)FZ();
            if (n >= 20u && (FZ() & 3u)) {             /* mostly a real header, so the parser goes deep */
                bk[0] = 'H', bk[1] = 'Y', bk[2] = 'P', bk[3] = '2';
                bk[4] = 1, bk[5] = bk[6] = bk[7] = 0;
                if (FZ() & 1u) {
                    uint32_t np = FZ() % 100u;
                    bk[8] = (uint8_t)np, bk[9] = 0;
                }
                if (FZ() & 1u) {                       /* and values mostly in range */
                    for (k = 20; k + 1 < n; k += 2)
                        bk[k + 1] = (FZ() & 7u) ? 0 : bk[k + 1];
                }
            } else if (n >= 4u && (FZ() & 1u)) {
                bk[0] = 'H', bk[1] = 'Y', bk[2] = 'P', bk[3] = '1';   /* the old kit's magic */
            }
#undef FZ
            plat_store_save(OBJ_PROJ, bk, n);
        } else if (!strcmp(cmd, "dumpobj") || !strcmp(cmd, "loadobj")) {   /* dumpobj OBJ FILE: a flash object to a file; loadobj OBJ FILE: a file into one, as is
                                                 * (tests/web_piece_test.js: a bank the pieces page builds, loaded at the next boot) */
            static uint8_t tmp[PLAT_STORE_MAX];
            FILE *fo = fopen(b, cmd[0] == 'd' ? "wb" : "rb");
            if (!fo) {
                fprintf(stderr, "%s: cannot open %s\n", cmd, b);
                fails++;
            } else if (cmd[0] == 'd') {
                const int n = plat_store_load((uint32_t)atoi(a), tmp, sizeof tmp);
                if (n > 0)
                    fwrite(tmp, 1, (size_t)n, fo);
                fclose(fo);
            } else {
                const size_t n = fread(tmp, 1, sizeof tmp, fo);
                fclose(fo);
                plat_store_save((uint32_t)atoi(a), tmp, (uint32_t)n);
            }
        } else if (!strcmp(cmd, "restore")) {          /* restore OBJ [bad]: the object as it is in flash, checked and written back as a backup's (bad: one byte changed) */
            static uint8_t tmp[PLAT_STORE_MAX];
            const int o = atoi(a), n = plat_store_load((uint32_t)o, tmp, sizeof tmp);
            int m = n;
            if (b[0] == 'b' && n > 0)
                tmp[0] ^= 0xFFu;
            if (b[0] == 's' && n > 0 && rd32(tmp) == IMG_MAGIC) {      /* shared: the first saved piece's Tempo as the pieces page writes a setting the piece predates */
                const uint32_t sv = rd16(tmp + 8);
                int i = 0;
                while (i < NPIECES && !(sv >> i & 1u))
                    i++;
                if (i < NPIECES)
                    tmp[IMG_HDR + (uint32_t)i * IMG_REC + REC_PAR + P_TEMPO] = REC_RECIPE;
            }
            if (b[0] == 'o' && n > 0 && rd32(tmp) == IMG_MAGIC) {      /* old: a bank as before 0.15.6, the records only */
                tmp[14] = 0;
                m = (int)IMG_TAIL;
            } else if (b[0] == 'o' && o == OBJ_GLOB && n == (int)sizeof(glob_t)) {   /* old: the player's three without the set list (0.15.20 and older) */
                m = (int)GLOB_OLD;
            } else if (b[0] == 'o' && n == (int)sizeof(set_t)) {       /* old: settings with three effect settings fewer, packed as an older build wrote them */
                static uint8_t o2[sizeof(set_t)];
                const int old = EN_NFXS - 3;
                int q = 8 + old;
                memcpy(o2, tmp, 8u + (uint32_t)old);
                o2[4] = (uint8_t)old, o2[5] = (uint8_t)(old >> 8);
                memcpy(o2 + q, tmp + offsetof(set_t, uiv), 16), q += 16;
                q += q & 1;
                memcpy(o2 + q, tmp + offsetof(set_t, gx_pat), 4), q += 4;
                memcpy(tmp, o2, (uint32_t)q);
                m = q;
            }

            rs_valid = m > 0 && ui_restore_valid((uint32_t)o, tmp, (uint32_t)m);
            if (rs_valid)
                ui_restore_start((uint32_t)o, tmp, (uint32_t)m);
        } else if (!strcmp(cmd, "bank76")) {
            static uint8_t bk[PLAT_STORE_MAX];
            static int16_t par[P_NPARAMS];
            static uint16_t pats[EN_NPAT][EN_NLANE];
            uint16_t pool;
            int pc = atoi(a), k, o = 20, i, PADV = atoi(b);
            memset(bk, 0, sizeof bk);
            bk[0] = 'H', bk[1] = 'Y', bk[2] = 'P', bk[3] = '2';
            bk[4] = 1;
            bk[8] = 76;
            bk[10] = (uint8_t)(1u << pc), bk[11] = (uint8_t)((1u << pc) >> 8);
            bk[13] = 0, bk[14] = 1, bk[16] = 2;      /* MIDI on, theme Ice */
            for (i = 0; i < NPIECES; i++) {
                en_piece(i, par, &pool);
                en_piece_pats(i, pats);
                if (i == pc)
                    par[P_PAD] = (int16_t)PADV;
                for (k = 0; k < 76; k++, o += 2)
                    bk[o] = (uint8_t)par[k], bk[o + 1] = (uint8_t)((uint16_t)par[k] >> 8);
                bk[o] = (uint8_t)pool, bk[o + 1] = (uint8_t)(pool >> 8), o += 2;
                for (k = 0; k < EN_NPAT * EN_NLANE; k++, o += 2)
                    bk[o] = (uint8_t)pats[k / EN_NLANE][k % EN_NLANE], bk[o + 1] = (uint8_t)(pats[k / EN_NLANE][k % EN_NLANE] >> 8);
            }
            plat_store_save(OBJ_PROJ, bk, (uint32_t)o + 16u);
        } else if (!strcmp(cmd, "autopilot")) {      /* autopilot 0/1: Kick Runner steers itself (for videos) */
            autopilot = atoi(a);
        } else if (!strcmp(cmd, "splash")) {         /* splash MS: the next boot's RedtailWorks card */
            ui_splash_ms = (uint32_t)strtoul(a, 0, 10);
        } else if (!strcmp(cmd, "fxpageset")) {      /* (0.17.19) fxpageset N: the effects' page N up (as the walk reaches it), for pictures */
            ui.fx_page = (uint8_t)atoi(a);
        } else if (!strcmp(cmd, "storefail")) {      /* (0.17.48) storefail OBJ N: the next N writes of object OBJ fail (plat.h's OBJ_*) */
            sf_obj = atoi(a);
            sf_n = atoi(b);
        } else if (!strcmp(cmd, "flood")) {          /* (0.17.48) flood N: N commands posted at once, no render between (the queue's 1024) */
            int k2;
            for (k2 = 0; k2 < atoi(a); k2++)
                en_set(P_TEMPO, proj.par[P_TEMPO]);
        } else if (!strcmp(cmd, "midireset")) {      /* (0.17.47) the MIDI out counts from zero */
            midi_out_count = clock_out_count = noteon_out_count = cm_pcs_out = 0;
            cm_max_out = 0;
            memset(cc_out_n, 0, sizeof cc_out_n);
        } else if (!strcmp(cmd, "setfxs")) {         /* (0.17.19) setfxs N V: the effects' setting N (EN_FXS_*), as its page sets it */
            ui.fxs[atoi(a)] = (uint8_t)atoi(b);
            en_fxs(atoi(a), atoi(b));
        } else if (!strcmp(cmd, "kitload")) {        /* (0.17.38) kitload SLOT FILE: a kit's image written into the slot as the installer does, its sectors
                                                         * through the firmware's own writer (ui_kit_write), the header's last */
            static uint8_t kb[5 * 4096];
            FILE *kf = fopen(b, "rb");
            const int slot = atoi(a);
            size_t kn = kf ? fread(kb, 1, sizeof kb, kf) : 0;
            int ks;
            if (kf)
                fclose(kf);
            for (ks = 1; ks <= 5; ks++) {
                const int k = ks % 5;
                if ((size_t)k * 4096u < kn)
                    ui_kit_write((uint32_t)(slot * 5 + k), kb + k * 4096, (uint32_t)(kn - (size_t)k * 4096u > 4096u ? 4096u : kn - (size_t)k * 4096u));
            }
        } else if (!strcmp(cmd, "setpar")) {         /* setpar N V: parameter N set as its knob sets it */
            ui_param_set(atoi(a), atoi(b));
        } else if (!strcmp(cmd, "oldimage")) {       /* the first genre's stored bank marked as written before the pluck's DX voices were thirty (the sounds renumber on the next load) */
            if (store_len[OBJ_PROJ] > 13)
                store[OBJ_PROJ][13] = 0;
        } else if (!strcmp(cmd, "usbpower")) {       /* usbpower 0/1: a USB host powers the unit (a bolt in the battery) */
            host_usb = atoi(a);
        } else if (!strcmp(cmd, "seamless")) {       /* seamless 0/1: a flash write does not stop the audio (as the unit's sliced erase): no fade around a store */
            host_seamless = atoi(a);
        } else if (!strcmp(cmd, "battery")) {        /* battery N: the divider's ADC reading (0..1023; the indicator shows from about 500) */
            host_batt = (uint32_t)strtoul(a, 0, 10);
        } else if (!strcmp(cmd, "hasharm")) {        /* hasharm MS: the notes hashed from the next start for MS */
            nh_arm = 1;
            nh_win = (uint32_t)atoi(a);
            nh_t0 = 0;
        } else if (!strcmp(cmd, "hashkeep")) {
            nh_keep = nh;
            printf("  notes %u hash %08X\n", nh_n, nh);
        } else if (!strcmp(cmd, "codekeep") || !strcmp(cmd, "code")) {   /* codekeep: the code playing kept; code [CODE]: one started (the kept one) */
            extern int ui_code_now(char *b), ui_code_play(const char *s);
            static char kept[16];
            if (cmd[4]) {
                ui_code_now(kept);
                printf("  code %s\n", kept);
            } else if (ui_code_play(*a ? a : kept)) {
                printf("FAIL line %d: not a code: %s\n", lineno, *a ? a : kept);
                fails++;
            }
        } else if (!strcmp(cmd, "fmtdump") || !strcmp(cmd, "fmtlock")) {   /* (0.17.61) fmtdump FILE: the saved objects as this build loads them;
                                                 * fmtlock FILE: the formats' layout (tools/format_lock.py) */
            FILE *fo = fopen(a, "w");
            if (!fo) {
                printf("FAIL line %d: cannot write %s\n", lineno, a);
                fails++;
            } else {
                if (cmd[3] == 'd')
                    fmt_dump(fo);
                else
                    fmt_lock(fo);
                fclose(fo);
            }
        } else if (!strcmp(cmd, "fmtcheck")) {       /* (0.17.61) fmtcheck FILE: a dump kept in tests/format/ (each of its lines must be in this build's) */
            fails += fmt_check(a);
        } else if (!strcmp(cmd, "seed")) {           /* seed N: the next power-on's (reboot) */
            host_seed = (uint32_t)strtoul(a, 0, 10);
        } else if (!strcmp(cmd, "reboot")) {
            store_lock = 0;
            memset(&proj, 0, sizeof proj);
            boot();
        } else if (!strcmp(cmd, "echo"))
            printf("%s", line + 5);
        else {
            printf("line %d: unknown command %s\n", lineno, cmd);
            return 2;
        }
    }
    wav_close();
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, render %.1f%% of real time (host CPU)%s\n", now_ms, blits,
           render_budget_ns_total > 0 ? 100.0 * render_ns_total / render_budget_ns_total : 0.0,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* EN_WEB */
