/* SPDX-License-Identifier: GPL-3.0-only */
/* The frames the FM-1 sends the editor (firmware/src/dx_proto.h): none longer than PX_FRAME_MAX (a voice reply once overran a 96-byte buffer and crashed the
 * unit), all F0 7D 50 kind ... F7 with 7-bit data, the voice's checksum right, the dump's values recoverable. */
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/dx_proto.h"

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

    for (k = 0; k < sizeof b.guard; k++)
        CHECK(b.guard[k] == 0xA5, "the guard byte %u was overwritten: a frame is longer than PX_FRAME_MAX", (unsigned)k);
    printf(bad ? "dxproto: FAILED\n" : "dxproto: ok\n");
    return bad != 0;
}
