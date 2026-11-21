/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 in the browser: the host simulator (host/amb_host.c: the whole app, UI and instrument,
 * against a simulated FM-1) compiled to WebAssembly and run in an AudioWorklet (from X0X's). The worklet asks
 * for audio; producing it advances the device's clock one millisecond at a time, which runs the
 * engine every 256 samples (the I2S half buffer) and the UI every 16 ms, as on the FM-1. The page
 * sends input, and reads the screen, the lights and the saved objects back through these exports.
 *
 *   web/emu/build.sh   ->  build/emu/amb.wasm (+ the page) */
#include <stdint.h>

static void web_audio(const int32_t *blk, uint32_t n);
/* the studio's stems (its fast export): every mute the app asks for (GLO's, a song place's parts) also keeps these tracks out, so a part plays alone
 * through the whole song, place changes and all; the engine's own en_trk (amb.c) is called with them added */
void web_en_trk(uint32_t mute, uint32_t solo);
#define en_trk(m, s) web_en_trk(m, s)
#include "../../host/amb_host.c"
#undef en_trk
void en_trk(uint32_t mute, uint32_t solo);
static uint32_t st_stem_m;
void web_en_trk(uint32_t mute, uint32_t solo) { en_trk(mute | st_stem_m, solo); }

#undef __attribute__

/* (below the undef: the host defines __attribute__ away, which would leave an export out of the wasm) */
__attribute__((used, visibility("default"))) int web_st_stem(int mask)
{
    st_stem_m = (uint32_t)mask & 0xFFu;
    en_trk(ui.trk_m | st_stem_m, ui.trk_s);
    return (int)st_stem_m;
}

/* output ring, stereo float, filled by run_ms, drained by web_render */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r;
static void web_audio(const int32_t *blk, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_w++ % RING) * 2u;
        ring[i] = (float)blk[2 * k] / 8388608.0f;
        ring[i + 1] = (float)blk[2 * k + 1] / 8388608.0f;
    }
}

static float out_l[1024], out_r[1024];
__attribute__((used, visibility("default"))) float *web_out_l(void) { return out_l; }
__attribute__((used, visibility("default"))) float *web_out_r(void) { return out_r; }

/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
__attribute__((used, visibility("default"))) void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        run_ms(1);
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}

__attribute__((used, visibility("default"))) void web_boot(void)
{
    ui_splash_ms = 3200u;                        /* the boot cards, as on the FM-1 */
    host_seamless = 1;                           /* (a store goes on under the music, as the FM-1's sliced erase: no fade, the ~60 ms gap heard after each edit) */
    boot();
}
__attribute__((used, visibility("default"))) void web_seed(uint32_t s) { host_seed = s; }   /* before web_boot */
__attribute__((used, visibility("default"))) void web_buttons(uint32_t m) { held_btn = m; }
__attribute__((used, visibility("default"))) void web_keys(uint32_t m) { held_keys = m; }
__attribute__((used, visibility("default"))) void web_enc(int role, int32_t n)
{
    if (role >= 0 && role < NE)
        enc_acc[role] += n;
}
__attribute__((used, visibility("default"))) void web_master(uint32_t v) { master = v > 4096u ? 4096u : v; }
__attribute__((used, visibility("default"))) uint32_t web_lit_buttons(void) { return lit_btn; }
__attribute__((used, visibility("default"))) uint32_t web_lit_keys(void) { return lit_keys; }
__attribute__((used, visibility("default"))) uint16_t *web_fb(void) { return fb; }
__attribute__((used, visibility("default"))) uint32_t web_blits(void) { return blits; }
__attribute__((used, visibility("default"))) uint32_t web_now(void) { return now_ms; }
__attribute__((used, visibility("default"))) uint32_t web_cpu(void) { return cpu_pct; }

/* saved objects: the page keeps them (localStorage) and gives them back before boot */
__attribute__((used, visibility("default"))) uint8_t *web_store(uint32_t obj) { return obj < OBJ_NOBJ ? store[obj] : 0; }
__attribute__((used, visibility("default"))) int web_store_len(uint32_t obj) { return obj < OBJ_NOBJ ? store_len[obj] : -1; }
__attribute__((used, visibility("default"))) void web_store_set_len(uint32_t obj, int n)
{
    if (obj < OBJ_NOBJ)
        store_len[obj] = n < 0 ? -1 : n > (int)PLAT_STORE_MAX ? (int)PLAT_STORE_MAX : n;
}
__attribute__((used, visibility("default"))) uint32_t web_store_writes(void) { return store_writes; }
__attribute__((used, visibility("default"))) uint32_t web_nobj(void) { return OBJ_NOBJ; }
__attribute__((used, visibility("default"))) uint32_t web_store_max(void) { return PLAT_STORE_MAX; }

static float bg_out[12];                         /* for the stream view's background: what the screen shows */
static void bg_rgb(int at, uint16_t c)
{
    bg_out[at] = (float)(c >> 11) / 31.0f;
    bg_out[at + 1] = (float)((c >> 5) & 63u) / 63.0f;
    bg_out[at + 2] = (float)(c & 31u) / 31.0f;
}
/* the stream view's background: the screen theme's primary, accent and ground */
__attribute__((used, visibility("default"))) float *web_bg(void)
{
    bg_rgb(0, th->pad);
    bg_rgb(3, th->acc);
    bg_rgb(6, th->bg);
    bg_rgb(9, th->acid);
    return bg_out;
}

/* ---- the studio (web/studio, 0.17): a piece of any genre taken up as the panel takes it (GLO / PRESETS), read out in its stored form, an
 * edited one put back into play as you change it, and what the engine plays now (the bar, the chord, the lines it makes up as it goes).
 * Nothing here writes the flash: the bank in RAM is put back as it was. st_io: the record (IMG_REC) | the progression (EN_PRG_LEN) | its chord
 * types (EN_PRK_LEN) | the bass steps (16) | 1 when the FILTERS and SCREAM follow | those (PX_NFX). */
#define ST_REC 0
#define ST_PRG (ST_REC + IMG_REC)
#define ST_PRK (ST_PRG + EN_PRG_LEN)
#define ST_BS (ST_PRK + EN_PRK_LEN)
#define ST_FXON (ST_BS + 16)
#define ST_FX (ST_FXON + 1)
#define ST_LEN (ST_FX + PX_NFX)
static uint8_t st_io[ST_LEN];
__attribute__((used, visibility("default"))) uint8_t *web_st_io(void) { return st_io; }
__attribute__((used, visibility("default"))) int web_st_io_len(void) { return ST_LEN; }
/* the playing piece into st_io, as SAVE would store it */
__attribute__((used, visibility("default"))) int web_st_read(void)
{
    const int i = proj.piece;
    slot_t *s = &proj.slot[i];
    uint8_t *rec = image + IMG_HDR + (uint32_t)i * IMG_REC, *tail = image + IMG_TAIL + (uint32_t)i * EN_PRG_LEN;
    uint8_t k_rec[IMG_REC], k_tail[EN_PRG_LEN], k_px[PX_LEN];
    const uint8_t k_pend = pending_px;
    int k;
    keep();
    /* (keep() asks the engine for the bass steps and a frozen line, and a piece just taken up has only posted them to it: the engine takes them
     * at its next block. The studio puts them in through the piece, never the panel, so the piece's own are the ones: proj's, as take() set them.) */
    for (k = 0; k < 16; k++) s->bs[k] = proj.bs[k];
    for (k = 0; k < 18; k++) s->acl[k] = proj.acl[k];
    for (k = 0; k < 12; k++) s->cml[k] = proj.cml[k];
    for (k = 0; k < (int)IMG_REC; k++) k_rec[k] = rec[k];
    for (k = 0; k < EN_PRG_LEN; k++) k_tail[k] = tail[k];
    for (k = 0; k < (int)PX_LEN; k++) k_px[k] = px[i][k];
    pack_slot(s, i);
    for (k = 0; k < (int)IMG_REC; k++) st_io[ST_REC + k] = rec[k];
    for (k = 0; k < EN_PRG_LEN; k++) st_io[ST_PRG + k] = tail[k];
    for (k = 0; k < (int)IMG_REC; k++) rec[k] = k_rec[k];
    for (k = 0; k < EN_PRG_LEN; k++) tail[k] = k_tail[k];
    for (k = 0; k < (int)PX_LEN; k++) px[i][k] = k_px[k];
    pending_px = k_pend;
    for (k = 0; k < EN_PRK_LEN; k++) st_io[ST_PRK + k] = s->prk[k];
    for (k = 0; k < 16; k++) st_io[ST_BS + k] = s->bs[k];
    st_io[ST_FXON] = 1;
    project_fx_get(st_io + ST_FX);
    return 1;
}
/* piece i of genre g into play, as the panel takes it up (as left this time, else as saved, else its recipe); then read out */
__attribute__((used, visibility("default"))) int web_st_open(int g, int i)
{
    if ((unsigned)g >= EN_NGENRE || (unsigned)i >= NPIECES || ui.store_wait)
        return 0;
    if (g != proj.genre) {
        project_set_genre(g);
        en_genre(g);
        genre_fx(g);
    }
    set_piece(i);
    ui.sl_pos = (uint8_t)sl_find();             /* (the set list's place of the piece now, as at power on: PLAY plays its parts) */
    return web_st_read();
}
/* place s of the set list now (as the PRESETS knob does with the song on): its genre and piece, its parts; then st_io as web_st_open */
/* ---- the DX7 voice editor (web/studio, the pluck's SOUND): the player's voice slots read and written as the FM-1's SysEx does (main_fm1.c
 * dx_service: en_user_voice, ui_voices_changed), and the sixty built-in DX sounds to start from (fm6_dy.h, as fm6.inc plays them) */
#include "../../firmware/src/dsp/fm6_dy.h"
static uint8_t st_vox[160];
__attribute__((used, visibility("default"))) uint8_t *web_st_vox(void) { return st_vox; }
__attribute__((used, visibility("default"))) int web_st_vox_n(void) { return en_user_count(); }
/* slot's voice (128 packed bytes) into st_vox; 0: no such slot */
__attribute__((used, visibility("default"))) int web_st_vox_get(int slot)
{
    if ((unsigned)slot >= (unsigned)en_user_count())
        return 0;
    memcpy(st_vox, en_user_get(slot), 128);
    return 1;
}
/* st_vox (128 packed bytes) into slot (past the loaded ones: the next free); keep: the FM-1 writes its voices to flash a moment later, as after SysEx.
 * The slot it went to; -1 not taken (the engine has not taken the last one yet: try again a moment later) */
__attribute__((used, visibility("default"))) int web_st_vox_put(int slot, int keep)
{
    if (slot < 0 || slot > en_user_next())
        slot = en_user_next();
    ui.act_t = plat_ms();
    if (!en_user_voice(slot, st_vox))
        return -1;
    if (keep)
        ui_voices_changed(1, 3);
    return slot;
}
/* the pluck's built-in DX sound idx (0..59) into st_vox as 128 packed bytes: 1; 0 none */
__attribute__((used, visibility("default"))) int web_st_vox_dx(int idx)
{
    if ((unsigned)idx >= EN_NFMFACT + EN_NFMX)
        return 0;
    memcpy(st_vox, FM6_DY[idx], 128);            /* (as fm6.inc fm_set_patch: the DY voices, since 0.17.75 always) */
    return 1;
}

/* ---- the FX menu (web/studio's FX view): the effects latched (ui.fx_lock, as a tap on the FX menu's key latches one: the UI sends the mask every frame),
 * the menu's keys (KEY_FX), the effects' names, and their pages (ui.c FXP: a name, the effect, four settings with their ranges and the text the unit
 * shows), read and set as the FX menu's knobs do (fxp_get, fxp_set: the player's settings, kept a moment after, not a piece's) */
__attribute__((used, visibility("default"))) int web_st_fx_lock(int set, int lo, int hi)
{
    if (set) {
        ui.fx_lock = (uint64_t)(uint32_t)hi << 32 | (uint32_t)lo;
        ui.act_t = plat_ms();
    }
    return (int)(uint32_t)ui.fx_lock;
}
__attribute__((used, visibility("default"))) int web_st_fx_lock_hi(void) { return (int)(uint32_t)(ui.fx_lock >> 32); }
__attribute__((used, visibility("default"))) int web_st_fx_nkeys(void) { return NKEYS; }
__attribute__((used, visibility("default"))) int web_st_fx_key(int bank, int key) { return (unsigned)bank < 4u && (unsigned)key < (unsigned)NKEYS ? KEY_FX[bank][key] : FX_NONE; }
__attribute__((used, visibility("default"))) const char *web_st_fx_name(int bit) { return (unsigned)bit < EN_NFX && EN_FX_NAME[bit] ? EN_FX_NAME[bit] : ""; }
__attribute__((used, visibility("default"))) int web_st_fx_page(int bit) { return fx_page_of(bit) - 1; }
__attribute__((used, visibility("default"))) int web_st_fxp_n(void) { return NFXP; }
/* page p's: what 0 its effect (63: none, the UI's settings), 1 heard only while held; control i's: 2 its kind (-1 none), 3 lowest, 4 highest, 5 value */
__attribute__((used, visibility("default"))) int web_st_fxp_num(int p, int i, int what)
{
    int lo, hi;
    const int k = K_FXP + p * 4 + (i & 3);
    if ((unsigned)p >= NFXP)
        return 0;
    if (what == 0)
        return FXP[p].bit;
    if (what == 1)
        return FXP[p].hold;
    if (FXP[p].c[i & 3].kind == KN_NONE)
        return -1;
    if (what == 2)
        return FXP[p].c[i & 3].kind;
    fxp_range(k, &lo, &hi);
    return what == 3 ? lo : what == 4 ? hi : fxp_get(k);
}
static char st_fxt[32];
/* page p's name (what 0), control i's label (1) or its value as the unit shows it (2) */
__attribute__((used, visibility("default"))) const char *web_st_fxp_str(int p, int i, int what)
{
    if ((unsigned)p >= NFXP)
        return "";
    if (what == 0)
        return FXP[p].name;
    if (what == 1)
        return FXP[p].c[i & 3].label;
    st_fxt[0] = 0;
    if (FXP[p].c[i & 3].kind != KN_NONE)
        fxp_text(K_FXP + p * 4 + (i & 3), st_fxt);
    return st_fxt;
}
__attribute__((used, visibility("default"))) int web_st_fxp_set(int p, int i, int v)
{
    if ((unsigned)p >= NFXP || FXP[p].c[i & 3].kind == KN_NONE)
        return 0;
    ui.act_t = plat_ms();                        /* (as a knob turned: the settings' store waits until it is left alone) */
    fxp_set(K_FXP + p * 4 + (i & 3), v);
    return fxp_get(K_FXP + p * 4 + (i & 3));
}
/* a parameter set as the FM-1's knob sets it (the Editor's px_service way), without the studio's copy of the piece: a fade's steps */
__attribute__((used, visibility("default"))) int web_st_par(int p, int v)
{
    ui_param_set(p, v);
    return ui_param_get(p);
}

/* ---- the studio's fast export (a Web Worker runs this wasm as fast as it can, web/studio): PLAY as the button starts it (a fresh track, the song's
 * place from the top: ui.c B_PLAY) and stops it, whether it plays, and a parameter read (a fade's level, the tempo) */
__attribute__((used, visibility("default"))) int web_st_play(int on)
{
    if (on && !en_playing) {
        track_fresh(1, 0);
        af_fresh = 1;
        sl_start();
        en_play(1);
    } else if (!on && en_playing) {
        en_play(0);
    }
    return en_playing;
}
__attribute__((used, visibility("default"))) int web_st_playing(void) { return en_playing; }
/* the FM-1's MIDI clock, start, continue and stop (EN_CLK_*), as its MIDI in gives them to the engine (ui.c midi_in): the studio follows the unit's Play */
__attribute__((used, visibility("default"))) void web_st_clock(int a) { en_clock(a); }
__attribute__((used, visibility("default"))) int web_st_par_get(int p) { return ui_param_get(p); }

/* the tracks muted and soloed (a bit a track, EN_TR_*), as the FM-1's GLO and keys: heard now, not stored; a song place's parts set them again */
__attribute__((used, visibility("default"))) int web_st_trk(int mute, int solo)
{
    ui.trk_m = (uint8_t)mute;
    ui.trk_s = (uint8_t)solo;
    en_trk((uint32_t)mute, (uint32_t)solo);
    return 1;
}

__attribute__((used, visibility("default"))) int web_st_go(int s)
{
    if ((unsigned)s >= SL_N || !glob.sl[s] || ui.store_wait)
        return 0;
    ui.sl_q = 0;
    sl_go(s);
    if (glob.sl_on)
        sl_parts(s);
    return web_st_read();
}
/* st_io into the playing piece, as if its settings, steps and lines had been changed on the panel (the track goes on) */
__attribute__((used, visibility("default"))) int web_st_put(void)
{
    const int i = proj.piece;
    slot_t *s = &proj.slot[i];
    uint8_t *rec = image + IMG_HDR + (uint32_t)i * IMG_REC, *tail = image + IMG_TAIL + (uint32_t)i * EN_PRG_LEN;
    uint8_t k_rec[IMG_REC], k_tail[EN_PRG_LEN], k_px[PX_LEN];
    int k, ok;
    keep();
    for (k = 0; k < (int)IMG_REC; k++) { k_rec[k] = rec[k]; rec[k] = st_io[ST_REC + k]; }
    for (k = 0; k < EN_PRG_LEN; k++) { k_tail[k] = tail[k]; tail[k] = st_io[ST_PRG + k]; }
    for (k = 0; k < (int)PX_LEN; k++) k_px[k] = px[i][k];
    for (k = 0; k < EN_PRK_LEN; k++) px[i][k] = st_io[ST_PRK + k];
    ok = unpack_slot(i, P_NPARAMS, 1, s);
    for (k = 0; k < (int)IMG_REC; k++) rec[k] = k_rec[k];
    for (k = 0; k < EN_PRG_LEN; k++) tail[k] = k_tail[k];
    for (k = 0; k < (int)PX_LEN; k++) px[i][k] = k_px[k];
    for (k = 0; k < 16; k++) s->bs[k] = st_io[ST_BS + k];
    if (st_io[ST_FXON]) {
        pxl[i][0] = 1;
        for (k = 0; k < PX_NFX; k++) pxl[i][1 + k] = st_io[ST_FX + k];
    }
    take(i);
    ui.key_on = 0;
    project_apply();
    ui.act_t = plat_ms();
    proj.edited |= (uint16_t)(1u << i);
    return ok;
}
/* what plays now: the sixteenth since start (4), the step, the pattern, the progression's slot (-1 the walk), the chord's root (pitch class),
 * its notes (4), the pad's (4), the bass's last bar (16) and step, the acid's line (length, step, gate, accents, slides: 2 each; offsets, octaves:
 * 16 each), the pluck's loop (rungs, levels: 16 each; its slot), the pattern each lane plays (6), playing, the genre, the piece, the chord's tones,
 * each lane's step (6), the pool (2), the root (P_ROOT), the bass mode */
static uint8_t st_lv[144];
__attribute__((used, visibility("default"))) uint8_t *web_st_live(void)
{
    uint8_t *b = st_lv;
    const uint32_t t = en_bars;
    int k;
    b[0] = (uint8_t)t; b[1] = (uint8_t)(t >> 8); b[2] = (uint8_t)(t >> 16); b[3] = (uint8_t)(t >> 24);
    b[4] = en_drum_pos; b[5] = en_pat_now; b[6] = (uint8_t)en_prog_pos; b[7] = en_chord_root;
    for (k = 0; k < 4; k++) { b[8 + k] = en_chord[k]; b[12 + k] = en_pad_note[k]; }
    for (k = 0; k < 16; k++) b[16 + k] = en_bs_hist[k];
    b[32] = (uint8_t)en_bs_pos; b[33] = en_acid_len; b[34] = en_acid_pos;
    b[35] = (uint8_t)en_acid_gate; b[36] = (uint8_t)(en_acid_gate >> 8);
    b[37] = (uint8_t)en_acid_acc; b[38] = (uint8_t)(en_acid_acc >> 8);
    b[39] = (uint8_t)en_acid_slide; b[40] = (uint8_t)(en_acid_slide >> 8);
    for (k = 0; k < 16; k++) { b[41 + k] = en_acid_off[k]; b[57 + k] = en_acid_oct[k]; b[73 + k] = en_cm_rung[k]; b[89 + k] = en_cm_lvl[k]; }
    b[105] = en_cm_pos;
    for (k = 0; k < EN_NLANE; k++) { b[106 + k] = en_lane_src[k]; b[116 + k] = en_lane_pos[k]; }
    b[112] = en_playing; b[113] = proj.genre; b[114] = proj.piece; b[115] = en_chord_n;
    b[122] = (uint8_t)proj.pool; b[123] = (uint8_t)(proj.pool >> 8);
    b[124] = (uint8_t)proj.par[P_ROOT]; b[125] = (uint8_t)proj.par[P_BL_MODE];
    b[126] = (uint8_t)proj.par[P_AC_LOCK]; b[127] = (uint8_t)proj.par[P_CM_LOCK];
    {   /* (128 ..) the parts' levels now, 0..255 (the desk's meters): pad, bass, acid, pluck, the loudest drum; the compressor's reduction (dB x 8), the output's peak */
        float d = 0.0f;
        const float lv[5] = {en_pad_level, en_bass_level, en_acid_level, en_cm_level, 0.0f};
        for (k = 0; k < EN_NDRUM; k++)
            if (en_drum_glow[k] > d)
                d = en_drum_glow[k];
        for (k = 0; k < 5; k++) {
            const float v = (k == 4 ? d : lv[k]) * 255.0f;
            b[128 + k] = (uint8_t)(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v);
        }
        b[133] = (uint8_t)(en_comp_gr * 8.0f < 0.0f ? 0.0f : en_comp_gr * 8.0f > 255.0f ? 255.0f : en_comp_gr * 8.0f);
        b[134] = (uint8_t)(en_out_peak * 255.0f < 0.0f ? 0.0f : en_out_peak * 255.0f > 255.0f ? 255.0f : en_out_peak * 255.0f);
    }
    {   /* (135 ..) the song (0.17.13): the set list's place playing (255 none) and its bar from 0 (255 stopped) */
        const uint32_t bb = sl_bar_now() - sl_b0;
        b[135] = (uint8_t)ui.sl_pos;
        b[136] = (uint8_t)(en_playing ? (bb > 254u ? 254u : bb) : 255u);
        b[137] = (uint8_t)en_trk_heard();       /* the tracks muted now (a place's parts out, or GLO's mutes) */
        {
            extern int en_host_arrm(void);
            b[138] = (uint8_t)en_host_arrm();   /* (0.17.16) the tracks Form Song's arrangement rests now (not mutes: they come back by themselves) */
            b[139] = (uint8_t)en_pad_filtered();   /* (0.17.18) the pad's sound has a filter (its saw sounds) */
            b[140] = (uint8_t)(en_lim_gr * 8.0f < 0.0f ? 0.0f : en_lim_gr * 8.0f > 255.0f ? 255.0f : en_lim_gr * 8.0f);   /* (0.17.73) the limiter's reduction (dB x 8): the desk's master strip */
        }
    }
    return b;
}
__attribute__((used, visibility("default"))) void web_st_boot(void)
{
    ui_splash_ms = 600u;                         /* the studio: the boot card short */
    host_seamless = 1;                           /* (as web_boot: a store without the fade) */
    boot();
}
/* track codes (0.17.11, ui.c): a code's text in st_txt started as the panel's Code page starts it (its genre, its piece's recipe, key, mode,
 * tempo, every generator from its seed), then read out into st_io (-1: not a code); and the code of the track playing into st_txt (1: it gives
 * the piece exactly as it is) */
static char st_txt[16];
__attribute__((used, visibility("default"))) char *web_st_txt(void) { return st_txt; }
__attribute__((used, visibility("default"))) int web_st_code_play(void)
{
    st_txt[15] = 0;
    if (ui_code_play(st_txt) < 0)
        return -1;
    return web_st_read();
}
__attribute__((used, visibility("default"))) int web_st_code_now(void) { return ui_code_now(st_txt); }
/* a new take: the track from its start with a new seed, the piece as it is (the panel's roll); the engine's own parts come out anew */
__attribute__((used, visibility("default"))) void web_st_roll(void)
{
    track_fresh(1, 1);
}
/* a note heard as the studio's editors place it: a voice (EN_V_*) and its note and velocity (0: off), or a drum (voice 9: lane, velocity) */
__attribute__((used, visibility("default"))) void web_st_note(int voice, int note, int vel)
{
    if (voice == 9) {
        if (vel > 0)
            en_drum_tap(note, vel);
    } else
        en_note_v(voice, note, vel);
}
/* the playing piece saved into its genre's bank, as SAVE on the Global page does, and written to the (emulator's) flash at once */
__attribute__((used, visibility("default"))) int web_st_save(void)
{
    if (ui.store_wait)
        return 0;
    project_piece_save();
    {   /* (its bass steps and frozen lines as the piece has them, not as the engine may not have taken them yet: see web_st_read) */
        slot_t *s = &proj.slot[proj.piece];
        int k;
        for (k = 0; k < 16; k++) s->bs[k] = proj.bs[k];
        for (k = 0; k < 18; k++) s->acl[k] = proj.acl[k];
        for (k = 0; k < 12; k++) s->cml[k] = proj.cml[k];
        pack_slot(s, proj.piece);
    }
    project_flush();
    return 1;
}
/* the set list (glob): sixteen places (genre * 10 + piece + 1, 0 empty) and Play; (0.17.13, song mode) each place's bars (0 Auto) and the parts it leaves
 * out, and the end (0 Loop, 1 Once); (0.17.14) each place's pattern (0 the piece's, 1..6) and energy (0 as is, else mood + 1). st_sl: places 0..15,
 * Play 16, bars 17..32, parts 33..48, end 49, patterns 50..65, energy 66..81; write 1: into glob (and the flash), then read back */
#define ST_SLN (SL_N * 5 + 2)
static uint8_t st_sl[ST_SLN];
__attribute__((used, visibility("default"))) uint8_t *web_st_sl(void) { return st_sl; }
__attribute__((used, visibility("default"))) int web_st_sl_len(void) { return ST_SLN; }
__attribute__((used, visibility("default"))) void web_st_set(int write)
{
    int k;
    if (write) {
        const int was = ui.sl_pos < SL_N && glob.sl_on ? glob.sl[ui.sl_pos] : 0;   /* (the place playing: an edit to another place, or a move, leaves it and its bars going) */
        for (k = 0; k < SL_N; k++) {
            glob.sl[k] = st_sl[k] <= EN_NGENRE * NPIECES ? st_sl[k] : 0;
            glob.sl_bars[k] = st_sl[SL_N + 1 + k];
            glob.sl_mute[k] = (uint8_t)(st_sl[2 * SL_N + 1 + k] & 31u);
            glob.sl_pat[k] = st_sl[3 * SL_N + 2 + k] <= EN_NPAT ? st_sl[3 * SL_N + 2 + k] : 0;
            glob.sl_mood[k] = st_sl[4 * SL_N + 2 + k] <= 101 ? st_sl[4 * SL_N + 2 + k] : 0;
        }
        glob.sl_on = st_sl[SL_N] ? 1 : 0;
        glob.sl_end = st_sl[3 * SL_N + 1] ? 1 : 0;
        project_globals();
        project_flush_globals();
        ui.sl_pos = (uint8_t)sl_find();         /* (as the unit's Play knob: the place of the piece playing, its bars from here; its parts heard at once) */
        if (!(was && glob.sl_on && ui.sl_pos < SL_N && glob.sl[ui.sl_pos] == was)) {   /* (the same place still in the list: its bar count goes on, a jump is st_go's) */
            ui.sl_q = 0;
            sl_b0 = sl_bar_now();
        }
        if (sl_play() && ui.sl_pos < SL_N)
            sl_parts(ui.sl_pos);
        else if (!glob.sl_on) {
            ui.trk_m = 0;                       /* (the song off: every part back) */
            en_trk(0, 0);
        }
    }
    for (k = 0; k < SL_N; k++) {
        st_sl[k] = glob.sl[k];
        st_sl[SL_N + 1 + k] = glob.sl_bars[k];
        st_sl[2 * SL_N + 1 + k] = glob.sl_mute[k];
        st_sl[3 * SL_N + 2 + k] = glob.sl_pat[k];
        st_sl[4 * SL_N + 2 + k] = glob.sl_mood[k];
    }
    st_sl[SL_N] = glob.sl_on;
    st_sl[3 * SL_N + 1] = glob.sl_end;
}
