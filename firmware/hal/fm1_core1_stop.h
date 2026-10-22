/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Safe CPU1 hold, from Melodee's fm1_core1_stop.h (itself adapted from Felucca 1.6.1 fm1_cpu1_halt).
 * Holding CPU1 can abort an outstanding bus access and raise a delayed bus-invalid flag on CPU0:
 * mask those traps, hold, drain and clear the flags, then restore the debug enables and the
 * write-lock state. Registers only: also used before application RAM is initialised.
 * Nothing at all unless the experimental second-core build (EN_CORE2). */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#ifndef EN_CORE2
#define EN_CORE2 0
#endif

FM1_INLINE void fm1_core1_stop(void)
{
#if EN_CORE2
    uint32_t locked = !(*(volatile uint32_t *)0x1EEE240u & 1u), en, bus = 0x003F0030u;
    volatile uint32_t n;
    __asm__("" : "+r"(bus));                  /* do not fold ~bus into a stock-ROM-looking address (build.py check) */
    if (locked)
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    en = *(volatile uint32_t *)0x1EEE340u;
    *(volatile uint32_t *)0x1EEE340u = en & ~bus;
    __asm__ volatile("csync" ::: "memory");
    *(volatile uint32_t *)0x1EEE004u = *(volatile uint32_t *)0x1EEE004u | 2u;
    *(volatile uint32_t *)0x1EEE004u = *(volatile uint32_t *)0x1EEE004u & ~8u;
    __asm__ volatile("csync" ::: "memory");
    for (n = 0; n < 256u; n++)                /* let the delayed bus flag arrive */
        ;
    *(volatile uint32_t *)0x1EEE248u = bus;
    __asm__ volatile("csync" ::: "memory");
    *(volatile uint32_t *)0x1EEE340u = en;
    if (locked && (*(volatile uint32_t *)0x1EEE240u & 1u))
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    __asm__ volatile("csync" ::: "memory");
#endif
}
