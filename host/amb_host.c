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
 *   restore OBJ [bad]            write an object back as a restored backup's (checked first); expect rsvalid, rsstate, storelock
 *   storefuzz SEED               fill the project object with random damage (for the sanitizer runs)
 *   bank76 P N                   write a bank as the firmware before styles did (76 settings a piece),
 *                                piece P stored with its Pad at N
 */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
void plat_leds(uint32_t b, uint32_t k)
{
    lit_btn = b;
    lit_keys = k;
}
static int glow_on = 1;                 /* the device's button glow (plat_glow); the lights printed are the bright ones */
void plat_glow(int on) { glow_on = on; }
void plat_play_red(int red) { (void)red; }      /* the device's PLAY colour: green, red while recording */

#define MQ 256
static uint32_t min_q[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t last_out;                        /* the last packet out, not clock: status << 16 | d1 << 8 | d2 */
static uint32_t midi_out_count, clock_out_count;
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r++ % MQ];
    return 1;
}
void plat_midi_out(uint32_t pkt)
{
    midi_out_count++;
    if (((pkt >> 8) & 0xFF) != 0xF8)
        last_out = (pkt >> 8 & 0xFFu) << 16 | (pkt >> 16 & 0xFFu) << 8 | pkt >> 24;
    if (((pkt >> 8) & 0xFF) == 0xF8) {
        clock_out_count++;
        return;
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
int plat_store_locked(void) { return store_lock; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    if (store_lock)
        return -8;
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
uint32_t plat_peak_pct(void) { return cpu_pct; }
uint32_t plat_cpu1_pct(void) { return 0; }
uint32_t plat_core2(void) { return 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

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
            en_render(blk, 256, master);
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

static int expect(const char *what, const char *val)
{
    int got;
    if (!strcmp(what, "playing"))
        got = en_playing;
    else if (!strcmp(what, "frozen"))
        got = en_frozen;
    else if (!strcmp(what, "view"))
        got = ui.view;
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
    } else if (!strcmp(what, "edited"))             /* the pieces changed since taken up (bit i: piece i) */
        got = proj.edited;
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "clocks_out"))
        got = (int)clock_out_count;
    else if (!strcmp(what, "sixteenths"))
        got = (int)en_bars;
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
    else if (!strcmp(what, "fxmask"))             /* the effects held (FX menu) */
        got = (int)(uint32_t)ui.fx_mask;
    else if (!strcmp(what, "trkmute"))            /* the tracks muted (GLO held, the keys): a bit a track */
        got = ui.trk_m;
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
    else if (!strcmp(what, "jamv"))               /* the JAM page: voice, keys, the answer's memory, whether it is playing */
        got = ui.jam_voice;
    else if (!strcmp(what, "jamm"))
        got = ui.jam_mode;
    else if (!strcmp(what, "anscnt"))
        got = ui.ans_cnt;
    else if (!strcmp(what, "ansplay"))
        got = ui.ans_playing;
    else if (!strcmp(what, "smp"))                /* the sampler's voices sounding */
        got = en_smp_active;
    else if (!strcmp(what, "smpmode"))            /* the sampler page's Keys setting */
        got = ui.fxs[EN_FXS_SM_MODE];
    else if (!strcmp(what, "saver"))              /* the STAGE page: the screensaver setting, it is on, the lights */
        got = ui.saver;
    else if (!strcmp(what, "saveron"))
        got = ui.saver_on;
    else if (!strcmp(what, "lights"))
        got = ui.lights;
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
    else if (!strcmp(what, "lastout"))            /* the last MIDI out: status << 16 | d1 << 8 | d2 */
        got = (int)last_out;
    else if (!strcmp(what, "litbtn"))             /* the buttons lit (bit b: B_*) */
        got = (int)lit_btn;
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
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
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
        } else if (!strcmp(cmd, "restore")) {          /* restore OBJ [bad]: the object as it is in flash, checked and written back as a backup's (bad: one byte changed) */
            static uint8_t tmp[PLAT_STORE_MAX];
            const int o = atoi(a), n = plat_store_load((uint32_t)o, tmp, sizeof tmp);
            if (b[0] == 'b' && n > 0)
                tmp[0] ^= 0xFFu;
            rs_valid = n > 0 && ui_restore_valid((uint32_t)o, tmp, (uint32_t)n);
            if (rs_valid)
                ui_restore_start((uint32_t)o, tmp, (uint32_t)n);
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
