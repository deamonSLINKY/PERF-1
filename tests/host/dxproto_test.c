/* SPDX-License-Identifier: GPL-3.0-only */
/* The frames the FM-1 sends the editor (firmware/src/dx_proto.h): none longer than PX_FRAME_MAX (a voice reply once overran a 96-byte buffer and crashed the
 * unit), all F0 7D 50 kind ... F7 with 7-bit data, the voice's checksum right, the dump's values recoverable. */
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/dx_proto.h"
#include "../../firmware/src/backup_proto.h"

static int bad;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL " __VA_ARGS__); printf("\n"); bad++; } } while (0)

static void framed(const uint8_t *f, uint32_t n, uint8_t kind, const char *what)
{
    uint32_t i;
    CHECK(n >= 5u && f[0] == 0xF0 && f[1] == 0x7D && f[2] == 0x50 && f[3] == kind && f[n - 1] == 0xF7, "%s: not framed F0 7D 50 %02X .. F7", what, kind);
    for (i = 1; i + 1 < n; i++)
        CHECK(f[i] < 0x80, "%s: byte %u is %02X, not 7-bit", what, (unsigned)i, f[i]);
}

int main(void)
{
    /* a buffer with a guard after it: a frame that overruns PX_FRAME_MAX tramples the guard */
    struct { uint8_t f[PX_FRAME_MAX]; uint8_t guard[32]; } b;
    uint8_t voice[128];
    int32_t v[24];
    uint32_t n, k, sum;
    for (k = 0; k < 128u; k++)
        voice[k] = (uint8_t)((k * 7u + 3u) & 0x7Fu); /* a packed DX7 voice is 7-bit */
    memset(b.guard, 0xA5, sizeof b.guard);

    n = px_build_voice(b.f, 15, 16, voice);
    framed(b.f, n, 0x58, "a voice");
    CHECK(n == 136u && n <= PX_FRAME_MAX, "a voice frame is %u bytes (136 expected, at most %u)", (unsigned)n, (unsigned)PX_FRAME_MAX);
    for (sum = 0, k = 0; k < 128u; k++)
        sum += b.f[6 + k];
    CHECK(((sum + b.f[6 + 128]) & 0x7Fu) == 0u, "the voice's checksum");
    CHECK(b.f[4] == 15 && b.f[5] == 16, "slot and count");
    CHECK(!memcmp(b.f + 6, voice, 128), "the voice's bytes");

    n = px_build_voice(b.f, 20, 16, 0);
    framed(b.f, n, 0x58, "an empty slot");
    CHECK(n == 7u, "an empty slot's frame is %u bytes (7 expected)", (unsigned)n);

    for (k = 0; k < 24u; k++)
        v[k] = (int32_t)k * 9 - 100;                  /* some negative */
    v[23] = 8000;                                  /* (a value + 8192 is 14 bits: no parameter comes near the end of that) */
    n = px_build_dump(b.f, 130, v, 24);
    framed(b.f, n, 0x51, "a dump");
    CHECK(n == 7u + 48u + 1u && n <= PX_FRAME_MAX, "a dump frame is %u bytes", (unsigned)n);
    CHECK(((b.f[4] << 7) | b.f[5]) == 130 && b.f[6] == 24, "first and count");
    for (k = 0; k < 24u; k++)
        CHECK((int32_t)(((b.f[7 + 2 * k] << 7) | b.f[8 + 2 * k]) & 0x3FFF) - 8192 == v[k], "value %u comes back as %d", (unsigned)k, (int)v[k]);

    /* backup and restore (backup_proto.h): a frame is at most BK_FRAME_MAX with its nibbles and checksum right; an object sent as chunks the way the page sends them is
     * taken whole, only if every chunk is in; a bad checksum, a short chunk, a stray first chunk and a chunk of another object are refused */
    {
        static uint8_t obj[3000], got[BK_MAX], cm[BK_CHUNK * 2 + 16];
        uint8_t raw[BK_FRAME], fr[BK_FRAME_MAX];
        bk_sess_t s = {0, 0, 0};
        bk_chunk_t c;
        uint32_t i, o, nc, sm;
        for (i = 0; i < sizeof obj; i++)
            obj[i] = (uint8_t)(i * 37u + (i >> 5));
        for (i = 0; i < BK_FRAME; i++)
            raw[i] = (uint8_t)(0xF0u + i);
        n = bk_build_frame(fr, 17, 2, 3000, raw, BK_FRAME);
        framed(fr, n, 0x43, "a backup frame");
        CHECK(n == 141u && n <= BK_FRAME_MAX, "a backup frame is %u bytes (141 expected)", (unsigned)n);
        CHECK(fr[4] == 17 && fr[5] == 0 && fr[6] == 2 && ((fr[7] << 7) | fr[8]) == 3000 && ((fr[9] << 7) | fr[10]) == BK_FRAME, "obj, idx, len, n");
        for (i = 0; i < BK_FRAME; i++)
            CHECK((fr[11 + 2 * i] << 4 | fr[12 + 2 * i]) == raw[i], "frame byte %u", (unsigned)i);
        for (sm = 0, i = 4; i + 2u < n; i++)     /* (the data bytes plus the checksum add up to 0 modulo 128) */
            sm += fr[i];
        CHECK(((sm + fr[n - 2]) & 0x7Fu) == 0u, "a backup frame's checksum");
        {   /* the frame web/fm1backup.js parses in tests/web_backup_test.js */
            static const uint8_t two[2] = {0x12, 0xAB}, vec[17] = {0xF0, 0x7D, 0x50, 0x43, 0x03, 0x00, 0x01, 0x00, 0x64, 0x00, 0x02, 0x01, 0x02, 0x0A, 0x0B, 0x7E, 0xF7};
            n = bk_build_frame(fr, 3, 1, 100, two, 2);
            CHECK(n == 17u && !memcmp(fr, vec, 17), "the frame the page's test parses");
        }
        n = bk_build_frame(fr, 5, 0, 0, raw, 0);
        framed(fr, n, 0x43, "an empty object's frame");
        CHECK(n == 13u && fr[9] == 0 && fr[10] == 0, "an empty object's frame is %u bytes", (unsigned)n);
        n = bk_build_ack(fr, 3, 0x7F, 2);
        framed(fr, n, 0x44, "an answer");
        CHECK(n == 8u && fr[4] == 3 && fr[5] == 0x7F && fr[6] == 2, "an answer");
        /* the chunks, as the page builds them: 7D 50 42 03 obj idx(2) len(2) n(2) [2n] chk */
        nc = (3000u + BK_CHUNK - 1u) / BK_CHUNK;
        for (i = 0; i < nc; i++) {
            const uint32_t off = i * BK_CHUNK, cn = 3000u - off > BK_CHUNK ? BK_CHUNK : 3000u - off;
            uint32_t m = 0, q, r;
            cm[m++] = 0x7D, cm[m++] = 0x50, cm[m++] = 0x42, cm[m++] = 0x03, cm[m++] = 9;
            cm[m++] = (uint8_t)(i >> 7), cm[m++] = (uint8_t)(i & 0x7F), cm[m++] = (uint8_t)(3000u >> 7), cm[m++] = (uint8_t)(3000u & 0x7F), cm[m++] = (uint8_t)(cn >> 7), cm[m++] = (uint8_t)(cn & 0x7F);
            for (q = 0; q < cn; q++)
                cm[m++] = (uint8_t)(obj[off + q] >> 4), cm[m++] = (uint8_t)(obj[off + q] & 15u);
            cm[m] = (uint8_t)bk_sum(cm + 4, m - 4u);
            m++;
            r = (uint32_t)bk_parse_chunk(cm, m, &c);
            CHECK(r == 0u, "chunk %u parsed: %u", (unsigned)i, (unsigned)r);
            CHECK(bk_accept(&s, got, &c) == 0, "chunk %u taken", (unsigned)i);
            if (i + 1u < nc)
                CHECK(!bk_complete(&s, 9, 3000), "complete after %u of %u chunks", (unsigned)(i + 1u), (unsigned)nc);
            if (i == 1u) {                           /* a bad checksum, another object's chunk, a short chunk: refused, and the object goes on */
                bk_chunk_t d = c;
                const uint8_t sv = cm[m - 1];
                cm[m - 1] = (uint8_t)((sv + 1u) & 0x7Fu);
                CHECK(bk_parse_chunk(cm, m, &d) == 1, "a bad checksum is refused");
                cm[m - 1] = sv;
                d = c;
                d.obj = 8;
                CHECK(bk_accept(&s, got, &d) == 2, "a chunk of another object is refused");
                CHECK(bk_parse_chunk(cm, m - 1u, &d) == 2, "a short chunk is refused");
            }
        }
        CHECK(bk_complete(&s, 9, 3000), "the object is complete");
        CHECK(!bk_complete(&s, 9, 2999) && !bk_complete(&s, 8, 3000), "complete only for its own object and length");
        for (o = 0; o < 3000u && got[o] == obj[o]; o++)
            ;
        CHECK(o == 3000u, "the object came out as it went in (byte %u)", (unsigned)o);
        {   /* a first chunk that is not index 0 on an empty session */
            bk_sess_t z = {0, 0, 0};
            c.idx = 3;
            CHECK(bk_accept(&z, got, &c) == 2, "a stray chunk starts nothing");
        }
    }

    for (k = 0; k < sizeof b.guard; k++)
        CHECK(b.guard[k] == 0xA5, "the guard byte %u was overwritten: a frame is longer than PX_FRAME_MAX", (unsigned)k);
    printf(bad ? "dxproto: FAILED\n" : "dxproto: ok\n");
    return bad != 0;
}
