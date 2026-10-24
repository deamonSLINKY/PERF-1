/* SPDX-License-Identifier: GPL-3.0-only
 * Boot, audio ISR and main loop adapted from Felucca's main.c / audio.c, Copyright (C) 2026
 * Leo Kuroshita (@kurogedelic), Hugelton Instruments. */
/* AMB-1 on the FM-1 (from X0X's): boot (WDT first, boot-loop guard, guards), LCD, input (TIMER5
 * 10 kHz), audio (ALNK0), USB; then the main loop: UI at ~60 frames/s, input polled in between. */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[], _dsp_start[], _dsp_end[], _dsp_load[];

/* --------------------------------------------------------------- audio --- */
/* Felucca's 256-frame halves (5.8 ms): AMB-1's render is light, and a strum wants the short latency */
#ifndef EN_HALF
#define EN_HALF 512u                        /* two renders per half: a slow chunk borrows from a quick one (the telemetry showed the xruns were
                                             * single renders of 6.0 .. 6.2 ms against the 5.8 ms a half of 256 allows, at 70% load on average);
                                             * the price is 5.8 ms more delay. 256: the old, one render per half */
#endif
#define HALF_FRAMES EN_HALF                 /* I2S half buffer: 11.6 ms at 44.1 kHz (256: 5.8 ms) */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define FS 44100u
#define REND_FRAMES 256u                    /* en_render's frames per call */
#if FELUCCA_UAC
_Static_assert(REND_FRAMES == UA_HALF, "usb.c sizes the USB audio ring band for this block");
#endif
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

#define DBG_MAGIC 0x44424731u                       /* "DBG1": read with `fm1t memr` */
struct en_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots, stage;
    uint32_t crash_seen;                            /* the crash count already reported at a boot */
} en_dbg __attribute__((section(".noinit")));
static uint8_t safe_mode;                           /* two failed boots: no audio, USB on (safe_main) */
static volatile uint32_t tel_isr_in, tel_isr_out;   /* telemetry: ticks in the timer interrupt while a render ran (it nests into it) and outside it */
static volatile uint32_t tel_max_us;                /* telemetry: the longest render (a half block) since the last frame */

static void render_block(int32_t *o)                /* ALNK0: one half, measured */
{
    uint32_t t0 = fm1_ticks(), us, budget = HALF_FRAMES * 1000000u / FS;
    en_dbg.in_audio = 1;
#if FELUCCA_UAC
    uac_render_start();
#endif
    fm1_core1_check();                              /* nothing outstanding: retire a CPU1 that faulted or misread */
    {
        uint32_t k;
        for (k = 0; k < HALF_FRAMES; k += REND_FRAMES) {
            en_render(o + 2u * k, REND_FRAMES, master_q12);
#if FELUCCA_UAC
            uac_tap(o + 2u * k, REND_FRAMES);       /* the USB audio input: the same master output */
#endif
        }
    }
    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    en_dbg.cpu_q8 = (en_dbg.cpu_q8 * 15u + (us * 256u) / budget) / 16u;
    audio_cpu_pct = (en_dbg.cpu_q8 * 100u) >> 8;
    en_dbg.halves++;
    en_dbg.last_us = us;
    if (us > en_dbg.max_us)
        en_dbg.max_us = us;
    if (us > tel_max_us)
        tel_max_us = us;
    {
        const uint32_t pc = us * 100u / budget;
        if (pc > pk_cur_pct)
            pk_cur_pct = pc;
        if (++pk_n >= FS / HALF_FRAMES) {           /* a second: the window slides */
            pk_n = 0;
            pk_prev_pct = pk_cur_pct;
            pk_cur_pct = 0;
        }
    }
    en_dbg.in_audio = 0;
}

void fm1_alnk0_irq(void)                            /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        render_block(&abuf[half * HALF_WORDS]);
        fm1_audio_ack_half();
        if (fm1_audio_free_half() != half) {
            en_dbg.late++;                         /* the DMA moved on while we rendered */
            audio_xruns++;
        }
    }
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}

static void audio_silence(void)                     /* IRQs off (flash erase): the DMA would loop stale audio */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}

/* ------------------------------------------------------------ timer --- */
static uint8_t usb_due;
void fm1_timer5_irq(void)
{
    static uint32_t sub;
    const uint32_t tel_t0 = fm1_ticks(), tel_nest = en_dbg.in_audio;
    fm1_timer5_ack();
    en_dbg.timer_irqs++;
    if (en_dbg.in_audio)
        en_dbg.nested++;
    fm1_input_tick();
    /* 2 kHz: all USB SIE traffic lives here. This ISR now nests into the audio render (so the key
     * matrix keeps its rhythm, below); USB shares the MIDI queues with the audio ISR, so a poll that
     * falls inside the render waits for the first tick outside it, as it did before the nesting. */
    if (sub % 5u == 0u) {
        usb_due = 1;
#if FELUCCA_UAC
        /* the USB audio stream cannot wait for the render (one packet per 1 ms frame, a render takes
         * up to ~11.6 ms): it runs nested too. It touches only EP4 (INDEX is set on every access) and
         * the consumer side of its ring, and usb_poll never runs nested, so the two never interleave */
        if (en_dbg.in_audio)
            uac_service();
#endif
    }
    if (usb_due && !en_dbg.in_audio) {
        usb_due = 0;
        usb_poll();
#if EN_TRS
        uart_midi_poll();                           /* the TRS jack: same context, same queue */
#endif
    }
    if (++sub == 10u)
        sub = 0;
    if (tel_nest)
        tel_isr_in += fm1_ticks() - tel_t0;
    else
        tel_isr_out += fm1_ticks() - tel_t0;
    {
        static uint32_t last, acc;
        uint32_t now = fm1_ticks();
        acc += now - last;
        last = now;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
}
extern void isr_timer5(void);

/* ------------------------------------------------------------- boot --- */
#define BOOTGUARD_MAGIC 0x42475244u
struct bootguard_s bootguard __attribute__((section(".noinit")));   /* declared in main.c (ota_commit) */

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    if (fm1_core1_online())
        fm1_core1_bar_set();                        /* CPU1's data can go bad too: this boot and the next run on one core */
    fm1_core1_stop();
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 8, 240, &FONT_S, "PERF-1 CRASH", C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(10, 40, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->pc);
    draw_text_box(10, 60, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->emu);
    draw_text_box(10, 84, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->dbg);
    draw_text_box(10, 102, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->rets);
    draw_text_box(10, 120, 220, &FONT_S, b, C_WHITE, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

static void enter_uboot(const char *why)
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 110, 240, &FONT_S, why, C_WHITE, 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                          /* intentional reset: not a failed boot */
    fm1_enter_uboot();
}

/* SAFE MODE: AMB-1 crashed (or hung) twice while starting. Nothing that makes sound runs: no audio,
 * no engine, no UI; USB is on, so the web installer (AMB-1, or the stock firmware) and M-UPGRADE can
 * reach it. PLAY tries AMB-1 again. The chip's own update mode stays the last resort (fm1_cstart). */
static void safe_main(void)
{
    char b[24];
    uint32_t t0, play;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 18, 240, &FONT_B, "PERF-1 SAFE MODE", C_HI, 1);
    draw_text_box(0, 52, 240, &FONT_S, "IT CRASHED TWICE", C_WHITE, 1);
    draw_text_box(0, 72, 240, &FONT_S, "WHILE STARTING.", C_WHITE, 1);
    draw_text_box(0, 102, 240, &FONT_S, "NO SOUND. USB IS ON:", C_WHITE, 1);
    draw_text_box(0, 122, 240, &FONT_S, "REINSTALL FROM THE", C_WHITE, 1);
    draw_text_box(0, 142, 240, &FONT_S, "WEB INSTALLER.", C_WHITE, 1);
    draw_text_box(0, 172, 240, &FONT_S, "PLAY: TRY AGAIN", C_HI, 1);
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 208, 240, &FONT_S, b, C_GRAY, 1);
    }
    fm1_input_init();
    panel_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 1);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    play = 1u << panel.btn[B_PLAY];
    t0 = fm1_ms;
    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (bootguard.pending && fm1_ms - t0 > 10000u)
            bootguard.pending = 0;                  /* safe mode itself is up: not another failed boot */
        if ((fm1_in.buttons & play) && fm1_ms - t0 > 500u) {
            bootguard.failed = 0;                   /* try AMB-1 again, from scratch */
            bootguard.pending = 0;
            draw_text_box(0, 172, 240, &FONT_S, "STARTING PERF-1...", C_HI, 1);
            usb_detach();
            fm1_delay_ms(30);
            fm1_reboot();
        }
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (flash_ok)
                ota_session();                      /* an install commits and resets; else back here */
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
    }
}

/* TELEMETRY. A host (tools/fm1log.py) sends F0 7D 50 46 01 F7 and every few seconds again; while it does, a status frame goes back four
 * times a second: F0 7D 50 46 (version 1) then 34 little-endian words packed 8-to-7, F7. Nothing is sent otherwise, and a frame
 * that would have to wait for the host is dropped. The words (tel_service): sequence, ms, CPU %, CPU1 %, core2 state, xruns, late,
 * the longest render since the last frame (us), the last render, nested ticks, shed level, playing, flash writes, the last write's
 * us, crash count, renders, UI frames, the render budget (us), the longest gap between main-loop turns and the longest UI frame (us), the genre and the piece, then the first core's stages (render_chunk PT: clear, bass+acid, pluck+DX+snips+noise, wait for the pad,
 * merge+sidechain, drums, echoes, wait for the reverb, master; us) and the chunks they cover, then the timer interrupt's time (nested in renders, outside). */
static uint32_t tel_gap_us, tel_ui_us;              /* the longest since the last frame */
/* BENCH. On request (F0 7D 50 46 02 F7, tools/fm1log.py --bench) the chip times a handful of loops with the interrupts off (a glitch in
 * the music, once): float multiply-add, the same from RAM, divide, int <-> float, a table's lookup from flash and from RAM, a rational
 * soft clip, integer adds and a RAM copy; each BN turns, in ticks. Back: F0 7D 50 47 then the words (BN, ticks per us, the nine results) F7. */
#define BN 1500u
#define BENCH_BODY_F     float a = 1.0f, b = 0.5f;     uint32_t i;     for (i = 0; i < BN; i++) {         a = a * 0.9999f + b;         b = b * 0.9998f + a * 0.0001f;         a = a * 1.0001f - b * 0.0001f;         b = b + a * 0.00002f;     }     return a + b;
static float bench_sink;
static __attribute__((noinline)) float bench_f_flash(void) { BENCH_BODY_F }
static __attribute__((section(".ram_text.bench"), noinline, used)) float bench_f_ram(void) { BENCH_BODY_F }
static __attribute__((noinline)) float bench_div(void)
{
    float a = 1.0f;
    uint32_t i;
    for (i = 0; i < BN; i++) {
        a = 1.0f / (a + 1.5f);
        a = 1.0f / (a + 1.5f);
        a = 1.0f / (a + 1.5f);
        a = 1.0f / (a + 1.5f);
    }
    return a;
}
static __attribute__((noinline)) float bench_cvt(void)
{
    float a = 0.37f;
    uint32_t i;
    for (i = 0; i < BN; i++) {
        a = (float)(int32_t)(a * 1000.0f) * 0.001f + 0.37f;
        a = (float)(int32_t)(a * 1000.0f) * 0.001f + 0.21f;
        a = (float)(int32_t)(a * 1000.0f) * 0.001f + 0.13f;
        a = (float)(int32_t)(a * 1000.0f) * 0.001f + 0.43f;
    }
    return a;
}
static const float bench_tab_f[130] = {0.0f, 0.049f, 0.098f, 0.146f, 0.195f, 0.243f, 0.290f, 0.336f, 0.382f, 0.426f, 0.471f, 0.514f, 0.556f, 0.596f, 0.634f, 0.673f, 0.707f};
static float bench_tab_r[130] = {0.0f, 0.049f, 0.098f, 0.146f, 0.195f, 0.243f, 0.290f, 0.336f, 0.382f, 0.426f, 0.471f, 0.514f, 0.556f, 0.596f, 0.634f, 0.673f, 0.707f};
static __attribute__((noinline)) float bench_tab(const float *tb)
{
    float a = 0.0f, ph = 0.1f;
    uint32_t i;
    for (i = 0; i < BN; i++) {
        float x, f;
        int k;
        ph += 0.0371f;
        ph -= (float)(int32_t)ph;
        x = ph * 64.0f;
        k = (int)x;
        f = x - (float)k;
        a += tb[k] + (tb[k + 1] - tb[k]) * f;
        a *= 0.5f;
    }
    return a;
}
static __attribute__((noinline)) float bench_sat(void)
{
    float a = 0.3f;
    uint32_t i;
    for (i = 0; i < BN; i++) {
        float x = a * 1.7f;
        x = x > 3.0f ? 3.0f : x < -3.0f ? -3.0f : x;
        a = x * (27.0f + x * x) / (27.0f + 9.0f * x * x) + 0.3f;
    }
    return a;
}
static __attribute__((noinline)) uint32_t bench_int(void)
{
    uint32_t a = 1u, b = 7u, i;
    for (i = 0; i < BN; i++) {
        a = a * 3u + b;
        b ^= a >> 3;
        a += b;
        b += a & 0xFFu;
    }
    return a ^ b;
}
static uint32_t bench_buf[256], bench_buf2[256];
static __attribute__((noinline)) uint32_t bench_copy(void)
{
    uint32_t i, j, s = 0;
    for (j = 0; j < BN / 64u; j++)
        for (i = 0; i < 256u; i++) {
            bench_buf2[i] = bench_buf[i] + s;
            s += bench_buf2[i];
        }
    return s;
}
static void bench_run(uint32_t *w)
{
    uint32_t t, f = irq_save();
#define BT(slot, call) t = fm1_ticks(); bench_sink += (float)(call); w[slot] = fm1_ticks() - t
    BT(2, bench_f_flash());
    BT(3, FL_FAR(bench_f_ram)());
    BT(4, bench_div());
    BT(5, bench_cvt());
    BT(6, bench_tab(bench_tab_f));
    BT(7, bench_tab(bench_tab_r));
    BT(8, bench_sat());
    BT(9, bench_int());
    BT(10, bench_copy());
#undef BT
    irq_restore(f);
    w[0] = BN;
    w[1] = FM1_TICKS_PER_US;
    fm1_probe_read(w + 11);                         /* the cache control as the boot left it (stock writes 0x1EEE008, 0x1EEE00C = 0x808080, 0x1EEE010 = 0xFFFF), the PLL, the CPU1 enable */
}

/* THE PLAYER'S DX7 VOICES. usb.c collects a DX7 SysEx (a 32-voice bulk dump: the first EN_NUSER voices replace the loaded ones; a single voice, appended; or
 * ours, F0 7D 50 56 slot 128-bytes checksum F7: one slot, or 7F alone to clear), here it is checked and handed to the engine, and the UI keeps them. */
static void dx_service(void)
{
    uint32_t i, k, sum = 0;
    uint8_t b[128], ack[7];
    int kind, r;                                     /* r: the voices taken; -1 a bad checksum; -2 not taken */
    if (dx_state != 2u)
        return;
    RING_PUBLISH();
    kind = dx_kind;
    if (kind == 1) {                                 /* bulk: 5 header bytes, 4096 data, the checksum */
        for (i = 0; i < 4096u; i++)
            sum += dx_buf[5u + i];
        if (((sum + dx_buf[5u + 4096u]) & 0x7Fu) != 0u) {
            r = -1;
        } else {
            en_user_clear();
            for (r = 0, k = 0; k < EN_NUSER; k++)
                r += en_user_voice((int)k, dx_buf + 5u + k * 128u);
        }
    } else if (kind == 2) {                          /* a single voice: 155 bytes, packed, appended */
        for (i = 0; i < 155u; i++)
            sum += dx_buf[5u + i];
        if (((sum + dx_buf[5u + 155u]) & 0x7Fu) != 0u) {
            r = -1;
        } else {
            en_voice_pack(dx_buf + 5u, b);
            r = en_user_voice(en_user_next(), b) ? 1 : -2;
        }
    } else if (kind == 3) {                          /* ours: one slot */
        for (i = 0; i < 128u; i++)
            sum += dx_buf[4u + i];
        if (((sum + dx_buf[4u + 128u]) & 0x7Fu) != 0u)
            r = -1;
        else
            r = en_user_voice(dx_buf[3], dx_buf + 4u) ? 1 : -2;
    } else {
        en_user_clear();
        r = 0;
    }
    ui_voices_changed(r, kind);
    ack[0] = 0xF0, ack[1] = 0x7D, ack[2] = 0x50, ack[3] = 0x57, ack[4] = (uint8_t)kind, ack[5] = r < 0 ? 0x7Fu : (uint8_t)r, ack[6] = 0xF7;
    usb_sysex_try(ack, sizeof ack);                  /* the answer: the page waits for it before the next voice */
    dx_state = 0;
}

static uint32_t prof_clk(void) { return fm1_ticks(); }
static void tel_service(void)
{
    static uint8_t on;
    static uint32_t until, next, seq;
    uint8_t r = usb.tel_req;
    if (r == 3u) {                                  /* the bench, once */
        uint32_t w[16], i;
        uint8_t f[4 + 16 * 5 + 2], b[16 * 4];
        usb.tel_req = 0;
        bench_run(w);
        for (i = 0; i < 16u; i++)
            b[4 * i] = (uint8_t)w[i], b[4 * i + 1] = (uint8_t)(w[i] >> 8), b[4 * i + 2] = (uint8_t)(w[i] >> 16), b[4 * i + 3] = (uint8_t)(w[i] >> 24);
        f[0] = 0xF0, f[1] = 0x7D, f[2] = 0x50, f[3] = 0x47;
        i = 4u + ota_pack7(b, sizeof b, f + 4);
        f[i++] = 0xF7;
        usb_sysex_try(f, i);
        return;
    }
    if (r) {
        usb.tel_req = 0;
        on = r == 2u;
        until = fm1_ms + 10000u;
        if (on)
            next = fm1_ms;
    }
    if (!on)
        return;
    if ((int32_t)(fm1_ms - until) > 0) {            /* the host went quiet: stop */
        on = 0;
        return;
    }
    if ((int32_t)(fm1_ms - next) < 0)
        return;
    {
        uint32_t w[34], i;
        uint8_t f[4 + 34 * 5 + 2];
        uint32_t n = 4;
        w[0] = seq;
        w[1] = fm1_ms;
        w[2] = audio_cpu_pct;
        w[3] = fm1_core1_load();
        w[4] = plat_core2();
        w[5] = audio_xruns;
        w[6] = en_dbg.late;
        w[7] = tel_max_us;
        w[8] = en_dbg.last_us;
        w[9] = en_dbg.nested;
        w[10] = (uint32_t)en_shed_level | (uint32_t)en_chorus_c0 << 8 | (uint32_t)en_bass_c1 << 9;   /* (bit 8: the chorus on the first core) */
        w[11] = (uint32_t)en_playing;
        w[12] = tel_stores;
        w[13] = tel_store_us;
        w[14] = fm1_crash.magic == FM1_CRASH_MAGIC ? fm1_crash.count : 0u;
        w[15] = en_dbg.halves;
        w[16] = en_dbg.ui_frames;
        w[17] = HALF_FRAMES * 1000000u / FS;
        w[18] = tel_gap_us;
        w[19] = tel_ui_us;
        w[20] = (uint32_t)proj.genre;
        w[21] = (uint32_t)proj.piece;
        for (i = 0; i < 9u; i++) {                  /* the first core's time by stage since the last frame (us), then the chunks */
            w[22 + i] = en_prof[i] / FM1_TICKS_PER_US;
            en_prof[i] = 0;
        }
        w[31] = en_prof[9];
        en_prof[9] = 0;
        w[32] = tel_isr_in / FM1_TICKS_PER_US;      /* the timer interrupt: us nested into renders, and outside them */
        w[33] = tel_isr_out / FM1_TICKS_PER_US;
        tel_isr_in = tel_isr_out = 0;
        f[0] = 0xF0, f[1] = 0x7D, f[2] = 0x50, f[3] = 0x46;
        {
            uint8_t b[34 * 4];
            for (i = 0; i < 34u; i++)
                b[4 * i] = (uint8_t)w[i], b[4 * i + 1] = (uint8_t)(w[i] >> 8), b[4 * i + 2] = (uint8_t)(w[i] >> 16), b[4 * i + 3] = (uint8_t)(w[i] >> 24);
            n += ota_pack7(b, sizeof b, f + n);
        }
        f[n++] = 0xF7;
        if (usb_sysex_try(f, n) == 0) {
            seq++;
            tel_max_us = 0;
            tel_gap_us = 0;
            tel_ui_us = 0;
            next += 250u;
            if ((int32_t)(fm1_ms - next) > 0)
                next = fm1_ms + 250u;
        } else {
            next = fm1_ms + 250u;                   /* no room (no host listening): try again later */
        }
    }
}

static void splash(void)
{
    lcd_fill(0, 0, 240, 240, RGB(22, 21, 27));       /* charcoal, until the UI's RedtailWorks card (ui_init) */
    lcd_sync();
}

static void fm1_main(void)
{
    int32_t knob = 512 * 16;
    uint32_t last_frame = 0;
    {
        uint32_t f = irq_save();
        flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;   /* the expected 1 MiB part, else RAM only */
        irq_restore(f);
        if (flash_ok)
            ota_boot_cleanup();
    }
    lcd_init();
    palette_set(1);
    splash();
    if (en_dbg.magic != DBG_MAGIC) {
        memset(&en_dbg, 0, sizeof en_dbg);
        en_dbg.magic = DBG_MAGIC;
    }
    if (safe_mode)
        safe_main();                                /* never returns */
    if (fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != en_dbg.crash_seen) {
        char b[16];                                 /* a crash since the last boot: say so, a moment */
        en_dbg.crash_seen = fm1_crash.count;
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 176, 240, &FONT_S, "RESTARTED AFTER A CRASH", RGB(255, 80, 60), 1);
        draw_text_box(0, 196, 240, &FONT_S, b, C_GRAY, 1);
        fm1_wdt_feed();
        fm1_delay_ms(2500);
        fm1_wdt_feed();
    }
    en_dbg.boots++;
    en_dbg.max_us = 0;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    led_pos_init();
    en_init();
    en_prof_clock = prof_clk;
#if EN_CORE2
    en_core2_attach(fm1_core1_submit, fm1_core1_join, fm1_core1_fence, fm1_core1_alive);   /* the reverb runs on CPU1 while it is online */
#endif
    project_start(plat_seed());                     /* a piece at random, a little changed */
    project_apply();
    audio_init();
    usb_start();
#if EN_TRS
    uart_midi_init();                               /* before TIMER5: it read-modify-writes port H too */
#endif
    /* above ALNK0 (3): the key / encoder / LED matrix is scanned one column per tick and must keep
     * its rhythm through a render (X0X's finding: below the audio, turns lost steps and LEDs flickered) */
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
    if ((fm1_in.buttons & 3u) == 3u)
        panel_setup();                              /* OCT- + OCT+ held at power-on */
    fm1_delay_ms(400);
    ui_init();

    for (;;) {
        {   /* telemetry: the longest gap between turns of this loop (a flash write, a long frame: what the audio felt) */
            static uint32_t last_turn;
            const uint32_t t = fm1_ticks(), g = (t - last_turn) / FM1_TICKS_PER_US;
            if (last_turn && g > tel_gap_us)
                tel_gap_us = g;
            last_turn = t;
        }
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {     /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        {
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                uint32_t k10;
                knob += (a * 16 - knob) / 8;
                k10 = (uint32_t)(knob / 16);
                master_q12 = (k10 * k10) >> 8;           /* 0 .. ~4096 */
            }
        }
        {   /* OCT- + OCT+ held 5 s: update mode (Felucca's, unchanged) */
            static uint32_t t0, shown;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both) {
                if (shown)
                    ui_say("UPDATE MODE", "CANCELLED");
                shown = 0;
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != shown) {
                    char d[4] = {(char)('0' + left), '.', '.', 0};
                    ui_say("UPDATE MODE IN", d);
                    shown = left;
                }
            } else if (fm1_ms - t0 > 5000u) {
                enter_uboot("UBOOT");
            }
        }
        ota_service();                              /* M-UPGRADE handshake (other SysEx is dropped) */
        tel_service();
        dx_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            en_play(0);
            en_panic();
            if (flash_ok)
                ota_session();                      /* returns only if nothing was committed */
            ui_init();
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
#if EN_CDC
        cdc_task();
#endif
        if (fm1_ms - last_frame >= 16u) {           /* ~60 frames/s at most; input in between */
            last_frame = fm1_ms;
            en_dbg.ui_frames++;
            {
                const uint32_t t = fm1_ticks();
                uint32_t u;
                ui_frame();
                u = (fm1_ticks() - t) / FM1_TICKS_PER_US;
                if (u > tel_ui_us)
                    tel_ui_us = u;
            }
        } else {
            ui_input_only();
        }
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    fm1_core1_stop();                               /* a CPU1 left running by the last boot: hold it before RAM is cleared */
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 4u) {                   /* safe mode failed too: the chip's own update mode */
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    p3 |= (bootguard.failed >= 2u) << 8;            /* two failed boots: safe mode (kept past the .bss clear) */
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    for (s = _dsp_load, d = _dsp_start; d < _dsp_end; s++, d++)
        *d = *s;                                    /* CPU1's worker loop (EN_CORE2) */
    fm1_mailbox_clear();
#if EN_CORE2
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS);
    /* One core after a crash (CPU1's data can go bad where no check reaches), in safe mode, or after CPU1
     * was retired: the bar stays until power-off. CPU1 starts through mask ROM before it reaches our RAM
     * worker, so the instruction-fetch limits come after it. */
    if (!fm1_core1_barred() && bootguard.failed == 0u)
        fm1_core1_boot();
    fm1_guard_enable(FM1_GUARD_PC);
#else
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
#endif
    safe_mode = (uint8_t)(p3 >> 8);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}
