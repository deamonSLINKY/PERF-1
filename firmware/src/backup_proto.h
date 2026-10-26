/* SPDX-License-Identifier: GPL-3.0-only */
/* BACKUP AND RESTORE over USB MIDI (the installer page, web/fm1backup.js): the frames the FM-1 sends and the chunks it takes, built and parsed here, away from the
 * hardware, so a host test (tests/host/dxproto_test.c) can check them against the page's. Every message is F0 7D 50 kind ... F7 with 7-bit data; a raw byte goes as
 * two nibbles (high, low). An object is one of the stores in flash (plat.h OBJ_*), at most BK_MAX bytes.
 *
 *   page -> FM-1  42 01 obj                                  read an object: the FM-1 answers with frames, in order
 *   FM-1 -> page  43 obj idx(2) len(2) n(2) [2n] chk            a frame of the object: raw bytes idx * 64 ..; len the object's length (0: none stored)
 *   page -> FM-1  42 03 obj idx(2) len(2) n(2) [2n] chk         a chunk to write: raw bytes idx * 256 ..; the first chunk (idx 0) starts the object
 *   page -> FM-1  42 04 obj len(2)                           the object is complete: check it, write it
 *   FM-1 -> page  44 obj idx status                          the answer to a chunk (idx), or to the write (idx 7F); status 0 ok, 1 bad checksum, 2 bad, 3 not written
 *   page -> FM-1  42 05                                      restart (only after a write)
 * chk: the 7-bit data bytes after the kind (obj .. the last nibble), summed, then 128 minus the sum modulo 128. */
#pragma once
#include <stdint.h>

#define BK_MAX 3840u                             /* an object (plat.h PLAT_STORE_MAX) */
#define BK_FRAME 64u                             /* raw bytes in a frame the FM-1 sends */
#define BK_CHUNK 256u                            /* raw bytes in a chunk the FM-1 takes */
#define BK_FRAME_MAX 144u                        /* the longest frame the FM-1 sends: 4 + 1 + 2 + 2 + 2 + 128 + 1 + 1 = 141 */

static inline uint32_t bk_sum(const uint8_t *p, uint32_t n)
{
    uint32_t s = 0;
    while (n--)
        s += *p++;
    return (128u - (s & 0x7Fu)) & 0x7Fu;
}

/* kind 43: a frame of object obj (n <= BK_FRAME raw bytes from d; n 0 with len 0: nothing stored) */
static inline uint32_t bk_build_frame(uint8_t *f, uint32_t obj, uint32_t idx, uint32_t len, const uint8_t *d, uint32_t n)
{
    uint32_t k, o = 0;
    f[o++] = 0xF0, f[o++] = 0x7D, f[o++] = 0x50, f[o++] = 0x43;
    f[o++] = (uint8_t)(obj & 0x7Fu);
    f[o++] = (uint8_t)((idx >> 7) & 0x7Fu), f[o++] = (uint8_t)(idx & 0x7Fu);
    f[o++] = (uint8_t)((len >> 7) & 0x7Fu), f[o++] = (uint8_t)(len & 0x7Fu);
    f[o++] = (uint8_t)((n >> 7) & 0x7Fu), f[o++] = (uint8_t)(n & 0x7Fu);
    for (k = 0; k < n; k++)
        f[o++] = (uint8_t)(d[k] >> 4), f[o++] = (uint8_t)(d[k] & 15u);
    f[o] = (uint8_t)bk_sum(f + 4, o - 4u);
    o++;
    f[o++] = 0xF7;
    return o;
}

/* kind 44: the answer to chunk idx (or, idx 0x7F, to the write) of object obj */
static inline uint32_t bk_build_ack(uint8_t *f, uint32_t obj, uint32_t idx, uint32_t status)
{
    f[0] = 0xF0, f[1] = 0x7D, f[2] = 0x50, f[3] = 0x44;
    f[4] = (uint8_t)(obj & 0x7Fu), f[5] = (uint8_t)(idx & 0x7Fu), f[6] = (uint8_t)(status & 0x7Fu), f[7] = 0xF7;
    return 8;
}

/* a chunk to write: m is the message between F0 and F7 (7D 50 42 03 obj idx(2) len(2) n(2) [2n] chk) */
typedef struct {
    uint8_t obj;
    uint16_t n, idx, len;
    uint8_t d[BK_CHUNK];
} bk_chunk_t;
static inline int bk_parse_chunk(const uint8_t *m, uint32_t mlen, bk_chunk_t *c)   /* 0 ok, 1 a bad checksum, 2 malformed */
{
    uint32_t k, n;
    if (mlen < 12u || m[0] != 0x7Du || m[1] != 0x50u || m[2] != 0x42u || m[3] != 0x03u)
        return 2;
    n = (uint32_t)(m[9] << 7 | m[10]);
    if (n == 0u || n > BK_CHUNK || mlen != 12u + 2u * n)
        return 2;
    if (bk_sum(m + 4, 7u + 2u * n) != m[11u + 2u * n])
        return 1;
    for (k = 0; k < n; k++) {
        if (m[11u + 2u * k] > 15u || m[12u + 2u * k] > 15u)
            return 2;
        c->d[k] = (uint8_t)(m[11u + 2u * k] << 4 | m[12u + 2u * k]);
    }
    c->obj = m[4];
    c->idx = (uint16_t)(m[5] << 7 | m[6]);
    c->len = (uint16_t)(m[7] << 7 | m[8]);
    c->n = (uint16_t)n;
    return 0;
}

/* the object being received: which, how long, which chunks are in (a bit each) */
typedef struct {
    uint8_t obj;
    uint16_t len, mask;
} bk_sess_t;
static inline int bk_accept(bk_sess_t *s, uint8_t *buf, const bk_chunk_t *c)   /* 0 taken, 2 not */
{
    const uint32_t off = (uint32_t)c->idx * BK_CHUNK;
    uint32_t k;
    if (c->len == 0u || c->len > BK_MAX || c->idx >= 16u || off + c->n > c->len)
        return 2;
    if (c->idx == 0u) {                          /* the first chunk starts the object */
        s->obj = c->obj;
        s->len = c->len;
        s->mask = 0;
    } else if (s->mask == 0u || s->obj != c->obj || s->len != c->len) {
        return 2;
    }
    if (c->n != (off + BK_CHUNK <= c->len ? BK_CHUNK : c->len - off))   /* every chunk full but the last */
        return 2;
    for (k = 0; k < c->n; k++)
        buf[off + k] = c->d[k];
    s->mask = (uint16_t)(s->mask | 1u << c->idx);
    return 0;
}
static inline int bk_complete(const bk_sess_t *s, uint32_t obj, uint32_t len)   /* every chunk of this object is in */
{
    const uint32_t chunks = (len + BK_CHUNK - 1u) / BK_CHUNK;
    return s->mask && s->obj == obj && s->len == len && len && len <= BK_MAX && s->mask == (uint16_t)((1u << chunks) - 1u);
}
