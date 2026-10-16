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
#include "../../host/amb_host.c"

#undef __attribute__

/* output ring, eight channels of float, filled by run_ms, drained by web_render: the mix (L, R), then
 * the voices apart when asked for (web_split): the star, planets 1-3, planets 4-6, the drums, the weather
 * and the radio, the echo and the reverb */
#define RING 4096u
#define NCH 8u
static float ring[RING * NCH];
static uint32_t ring_w, ring_r;
static void web_audio(const int32_t *blk, uint32_t n)
{
    uint32_t k, c;
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_w++ % RING) * NCH;
        ring[i] = (float)blk[2 * k] / 8388608.0f;
        ring[i + 1] = (float)blk[2 * k + 1] / 8388608.0f;
        for (c = 0; c < EN_NBUS; c++)
            ring[i + 2u + c] = en_bus_on ? en_bus[k * EN_NBUS + c] : 0.0f;
    }
}

static float out_ch[NCH][1024];
__attribute__((used, visibility("default"))) float *web_out(uint32_t ch) { return out_ch[ch < NCH ? ch : 0]; }
__attribute__((used, visibility("default"))) float *web_out_l(void) { return out_ch[0]; }
__attribute__((used, visibility("default"))) float *web_out_r(void) { return out_ch[1]; }
__attribute__((used, visibility("default"))) void web_split(uint32_t on) { en_bus_on = (uint8_t)(on != 0); }

/* n frames (<= 1024) into out_ch: the device runs until they exist */
__attribute__((used, visibility("default"))) void web_render(uint32_t n)
{
    uint32_t k, c;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        run_ms(1);
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * NCH;
        for (c = 0; c < NCH; c++)
            out_ch[c][k] = ring[i + c];
    }
}

/* MIDI: what the device sends (USB-MIDI packets, as plat_midi_out has them) waits here for the page to
 * forward it; what the page receives (a clock, notes) goes in as the device's MIDI in */
#define MO_N 1024u
static uint32_t mo_ring[MO_N], mo_wr;
static void web_midi_push(uint32_t pkt) { mo_ring[mo_wr++ % MO_N] = pkt; }
__attribute__((used, visibility("default"))) uint32_t *web_midi_ring(void) { return mo_ring; }
__attribute__((used, visibility("default"))) uint32_t web_midi_written(void) { return mo_wr; }
__attribute__((used, visibility("default"))) void web_midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t cin = st >= 0xF0u ? 0x0Fu : st >> 4;
    min_q[mi_w++ % MQ] = cin | st << 8 | d1 << 16 | d2 << 24;
}

__attribute__((used, visibility("default"))) void web_boot(void)
{
    ui_splash_ms = 3200u;                        /* the boot cards, as on the FM-1 */
    boot();
}
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
