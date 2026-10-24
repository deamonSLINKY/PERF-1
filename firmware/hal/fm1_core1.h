/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The AC79's second core (CPU1) as a one-job-at-a-time worker, for the experimental reverb
 * offload (EN_CORE2). Bare-metal launch after Melodee's fm1_multicore.h (itself after the AC79 SDK's
 * EnableOtherCpu); the mailbox protocol after its audio_worker.h:
 *
 *   one producer (CPU0, the audio ISR), one consumer (CPU1, polling in internal RAM), one job
 *   outstanding. The job's function and context are published with a check word; CPU1 runs a
 *   request only when the check matches (some units' CPU1 now and then misreads shared SRAM),
 *   a misread is counted and never run. CPU0 joins before it touches anything the job owns
 *   (every render chunk does), before flash IO (IRQs are off then: no job is outstanding), and
 *   a CPU1 that faults, misreads or stalls for 10 ms is held for good ("retired"): CPU0 renders
 *   alone again, and the barred flag (kept in .noinit across soft resets, lost at power-off) keeps
 *   every boot until power-off on one core. Nothing here is built without EN_CORE2.
 *
 *   fm1_core1_boot()          start CPU1 (after RAM is initialised, before the PC guard)
 *   fm1_core1_submit(fn, p)   1: CPU1 runs fn(p); 0: not online or still busy (run it yourself)
 *   fm1_core1_join()          1: the job is done; 0: CPU1 was lost (retired), drop the job's results
 *   fm1_core1_check()         between jobs: retire a worker that faulted or misread
 *   fm1_core1_online()        CPU1 is running the worker */
#pragma once
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_irq.h"                         /* fm1_core1_fault: what the exception handler saw of CPU1 */
#include "fm1_core1_stop.h"

/* Carried across soft resets (a crash, the watchdog, an update), lost at power-off. */
#define FM1_CORE1_BAR_MAGIC 0x52414231u              /* "1BAR" */
typedef struct { uint32_t magic, barred; } fm1_core1_bar_t;
/* Not static, as fm1_crash: the compiler must not assume a zero start. */
fm1_core1_bar_t fm1_core1_bar __attribute__((section(".noinit")));
static inline int fm1_core1_barred(void)
{
    return fm1_core1_bar.magic == FM1_CORE1_BAR_MAGIC && fm1_core1_bar.barred == 1u;
}
static inline void fm1_core1_bar_set(void)
{
    fm1_core1_bar.magic = FM1_CORE1_BAR_MAGIC;
    fm1_core1_bar.barred = 1;
}

typedef void (*fm1_core1_fn)(void *);

#if EN_CORE2

#define AW_CHECK(fn, ctx, req) \
    ((uint32_t)(uintptr_t)(fn) ^ (uint32_t)(uintptr_t)(ctx) ^ (uint32_t)(req) * 0x9E3779B1u)
#define AW_TIMEOUT_TICKS (10000u * FM1_TICKS_PER_US)

static inline __attribute__((always_inline)) uint32_t aw_load(volatile uint32_t *p)
{
    uint32_t v = *p;
    __asm__ volatile("csync" ::: "memory");
    return v;
}
static inline __attribute__((always_inline)) uint32_t aw_ticks(void) { return FM1_T4_CNT; }   /* inline: the CPU1 loop is RAM code, no calls into XIP */
static inline __attribute__((always_inline)) void aw_store(volatile uint32_t *p, uint32_t v)
{
    __asm__ volatile("csync" ::: "memory");
    *p = v;
    __asm__ volatile("csync" ::: "memory");
}

/* CPU0's words, then CPU1's on a cache line of their own */
static struct {
    volatile uint32_t request;
    fm1_core1_fn fn;
    void *context;
    uint32_t check, max_wait_ticks, timeouts;
    volatile uint32_t ready __attribute__((aligned(32)));
    volatile uint32_t complete, rejected;
    uint32_t jobs, max_job_ticks, misread;
    int32_t avg_ticks;                           /* the job's time, smoothed (CPU1 writes it) */
} aw __attribute__((aligned(32)));
static int aw_online;

static int fm1_core1_online(void) { return aw_online; }
/* how much of a chunk (256 frames at 44.1 kHz = 139320 ticks of the 24 MHz timer) the job takes, in percent: the other core's load */
static uint32_t fm1_core1_load(void) { return aw_online && aw.avg_ticks > 0 ? (uint32_t)aw.avg_ticks / 1393u : 0u; }
static uint32_t aw_t0;                      /* when the outstanding job was submitted */
static void fm1_core1_fence(void) { __asm__ volatile("csync" ::: "memory"); }   /* publish memory between the cores */
/* 1 while the outstanding job can still finish: no CPU1 fault, under the 10 ms the join allows */
static int fm1_core1_alive(void)
{
    return !fm1_core1_fault.count && (uint32_t)(fm1_ticks() - aw_t0) < AW_TIMEOUT_TICKS;
}

static int fm1_core1_submit(fm1_core1_fn fn, void *context)
{
    uint32_t request;
    if (!aw_online || (request = aw_load(&aw.request)) != aw_load(&aw.complete))
        return 0;
    request++;
    aw.fn = fn;
    aw.context = context;
    aw.check = AW_CHECK(fn, context, request);
    aw_t0 = fm1_ticks();
    aw_store(&aw.request, request);
    return 1;
}

/* CPU0: hold CPU1 for good and render alone. A job it may have been running is abandoned, never
 * rerun (it partly advanced its state). */
static void fm1_core1_retire(void)
{
    fm1_core1_stop();
    fm1_core1_bar_set();
    aw_online = 0;
    aw_store(&aw.complete, aw_load(&aw.request));
}

/* CPU0 between jobs (nothing outstanding): retire a worker that faulted or misread */
static void fm1_core1_check(void)
{
    if (aw_online && (fm1_core1_fault.count || aw_load(&aw.rejected)))
        fm1_core1_retire();
}

/* 1: the job's results are in. 0: CPU1 faulted or stalled for 10 ms; it is retired and the job's
 * results must be dropped. */
static int fm1_core1_join(void)
{
    uint32_t start = fm1_ticks(), elapsed;
    while (aw_load(&aw.complete) != aw_load(&aw.request)) {
        if (fm1_core1_fault.count || (uint32_t)(fm1_ticks() - start) >= AW_TIMEOUT_TICKS) {
            if (!fm1_core1_fault.count)
                aw.timeouts++;
            fm1_core1_retire();
            return 0;
        }
    }
    elapsed = fm1_ticks() - start;
    if (elapsed > aw.max_wait_ticks)
        aw.max_wait_ticks = elapsed;
    return 1;
}

/* The CPU1 side: one look at the mailbox. The count of finished requests stays in a register:
 * only request, fn, context and check come from SRAM, and a misread of any of them fails the
 * check. Must stay in internal RAM: with no job outstanding CPU0 may turn XIP off for a flash
 * write. Publishes completion only after a callback has returned from XIP. No idle: CPU1 has no
 * interrupt to wake it. */
__attribute__((section(".dsp_text"), noinline, noreturn, used)) static void fm1_core1_loop(void)
{
    uint32_t done = aw_load(&aw.complete);
    aw_store(&aw.ready, 1u);
    for (;;) {
        uint32_t request = aw_load(&aw.request);
        if (request == done) {                   /* nothing yet: a few cycles of rest between looks (a flat-out poll, a memory barrier each time, may take the bus from the first core) */
            __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop");
            continue;
        }
        if (request == done + 1u) {
            fm1_core1_fn fn = aw.fn;
            void *context = aw.context;
            if (aw.check == AW_CHECK(fn, context, request)) {
                uint32_t start = aw_ticks(), took;
                fn(context);
                took = aw_ticks() - start;
                aw.jobs++;
                if (took > aw.max_job_ticks)
                    aw.max_job_ticks = took;
                aw.avg_ticks += ((int32_t)took - aw.avg_ticks) >> 4;
                done = request;
                aw_store(&aw.complete, request);
                continue;
            }
        }
        aw.misread = request;
        aw_store(&aw.rejected, aw_load(&aw.rejected) + 1u);
    }
}

/* two 4 KB stacks (user, supervisor): the pad's voices call a few levels deep */
static uint32_t cpu1_stacks[2048] __attribute__((section(".cpu1_stacks"), aligned(32), used));
extern void fm1_core1_start(void);
extern uint8_t _cpu1_stacks_lo[], _cpu1_stacks_hi[];
/* Assembly (crt0.S) names this symbol. */
void __attribute__((section(".dsp_text"), noreturn, used)) fm1_core1_main(void)
{
    /* The stack limit CPU0 uses, at CPU1's EMU bank; the bottom 256 bytes are kept so a descending
     * stack traps before it reaches the other core's data. */
    uint32_t lo = (uint32_t)(uintptr_t)_cpu1_stacks_lo + 256u;
    uint32_t hi = (uint32_t)(uintptr_t)_cpu1_stacks_hi - 1u;
    *(volatile uint32_t *)0x1EEF2D8u = hi;
    *(volatile uint32_t *)0x1EEF2DCu = lo;
    *(volatile uint32_t *)0x1EEF2E0u = hi;
    *(volatile uint32_t *)0x1EEF2E4u = lo;
    *(volatile uint32_t *)0x1EEF2D0u = (*(volatile uint32_t *)0x1EEF2D0u & ~((1u << 2) | (0x1Fu << 16))) | (1u << 3);
    fm1_core1_loop();
}

/* Start CPU1 and wait (10 ms at most) for its ready word. Called after bss/pool/data are set up and
 * before the PC-limit guard: CPU1 starts through mask ROM before it reaches our RAM worker. */
static int fm1_core1_boot(void)
{
    uint32_t start, div;
    fm1_core1_stop();
    aw_online = 0;
    aw_store(&aw.ready, 0);
    aw_store(&aw.request, 0);
    aw_store(&aw.complete, 0);
    aw_store(&aw.rejected, 0);
    fm1_core1_fault.count = fm1_core1_fault.echoes = 0;
    for (start = 0; start < 32u; start++)     /* CPU1's IRQ bank (CPU0 is at 0x1EEF100): worker stays IRQ-disabled */
        *(volatile uint32_t *)(0x1EEF300u + 4u * start) = 0;
    *(volatile uint32_t *)0x01C7FFF8u = (uint32_t)(uintptr_t)fm1_core1_start;
    div = *(volatile uint32_t *)0x10008u;
    *(volatile uint32_t *)0x10008u = div | 8u;
    __asm__ volatile("csync" ::: "memory");
    *(volatile uint32_t *)0x1EEE004u |= 8u;
    *(volatile uint32_t *)0x1EEE004u &= ~2u;
    start = fm1_ticks();
    while (!aw_load(&aw.ready) && (uint32_t)(fm1_ticks() - start) < 10000u * FM1_TICKS_PER_US)
        ;
    aw_online = aw_load(&aw.ready) != 0;
    if (!aw_online)
        fm1_core1_stop();
    *(volatile uint32_t *)0x10008u = div;
    return aw_online;
}

#else  /* single core: the same names, nothing behind them */

static inline int fm1_core1_online(void) { return 0; }
static inline uint32_t fm1_core1_load(void) { return 0; }
static inline int fm1_core1_submit(fm1_core1_fn fn, void *context) { (void)fn; (void)context; return 0; }
static inline void fm1_core1_fence(void) {}
static inline int fm1_core1_alive(void) { return 1; }
static inline int fm1_core1_join(void) { return 1; }
static inline void fm1_core1_check(void) {}
static inline int fm1_core1_boot(void) { return 0; }

#endif
