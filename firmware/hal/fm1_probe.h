/* SPDX-License-Identifier: GPL-3.0-only */
/* Read-only looks at chip registers the firmware does not otherwise use, for the telemetry bench (what the boot left: the cache control the
 * stock firmware writes, the PLL's control, the CPU1 enable). Reads only. */
#pragma once
#include <stdint.h>
#define FM1_PROBE_CACHE0 (*(volatile uint32_t *)0x1EEE008u)
#define FM1_PROBE_CACHE1 (*(volatile uint32_t *)0x1EEE00Cu)
#define FM1_PROBE_CACHE2 (*(volatile uint32_t *)0x1EEE010u)
#define FM1_PROBE_PLL (*(volatile uint32_t *)0x16A00u)
#define FM1_PROBE_CPU1EN (*(volatile uint32_t *)0x10008u)

static inline void fm1_probe_read(uint32_t *w)       /* five words: cache 0, 1, 2, the PLL's control, the CPU1 enable */
{
    w[0] = FM1_PROBE_CACHE0;
    w[1] = FM1_PROBE_CACHE1;
    w[2] = FM1_PROBE_CACHE2;
    w[3] = FM1_PROBE_PLL;
    w[4] = FM1_PROBE_CPU1EN;
}
