/* SPDX-License-Identifier: GPL-3.0-only */
/* The frames the FM-1 sends the editor (web/editor): built here, away from the hardware, so a host test (tests/host/dxproto_test.c) can check their sizes and
 * bytes. Every frame is F0 7D 50 kind ... F7 with 7-bit data, and none is longer than PX_FRAME_MAX: the buffer the caller builds it in. */
#pragma once
#include <stdint.h>

#define PX_FRAME_MAX 144u                        /* the longest: a voice, 4 + 2 + 128 + 1 + 1 = 136 */

/* kind 51: parameters first .. first + n - 1 (n <= 24): their values + 8192, each as two 7-bit bytes */
static inline uint32_t px_build_dump(uint8_t *f, uint32_t first, const int32_t *v, uint32_t n)
{
    uint32_t k, o = 0;
    f[o++] = 0xF0, f[o++] = 0x7D, f[o++] = 0x50, f[o++] = 0x51;
    f[o++] = (uint8_t)((first >> 7) & 0x7Fu), f[o++] = (uint8_t)(first & 0x7Fu), f[o++] = (uint8_t)n;
    for (k = 0; k < n; k++) {
        const uint32_t u = (uint32_t)(v[k] + 8192) & 0x3FFFu;
        f[o++] = (uint8_t)(u >> 7), f[o++] = (uint8_t)(u & 0x7Fu);
    }
    f[o++] = 0xF7;
    return o;
}

/* kind 58: voice `slot`, `loaded` voices held; when d128 is not 0 the voice's 128 bytes and a checksum follow */
static inline uint32_t px_build_voice(uint8_t *f, uint32_t slot, uint32_t loaded, const uint8_t *d128)
{
    uint32_t k, sum = 0, o = 0;
    f[o++] = 0xF0, f[o++] = 0x7D, f[o++] = 0x50, f[o++] = 0x58;
    f[o++] = (uint8_t)(slot & 0x7Fu), f[o++] = (uint8_t)(loaded & 0x7Fu);
    if (d128) {
        for (k = 0; k < 128u; k++) {
            f[o++] = d128[k] & 0x7Fu;
            sum += d128[k] & 0x7Fu;
        }
        f[o++] = (uint8_t)((128u - (sum & 0x7Fu)) & 0x7Fu);
    }
    f[o++] = 0xF7;
    return o;
}
