/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* M-UPGRADE update entry.
 *
 * The updater talks SysEx (F0 pack7(00 59 cmd len24 body chk) F7):
 *   cmd 0x11  handshake -> we answer our package identity (FELUCCA_ID): to the plain query its model and
 *             family only ("FM-1_700"), because M-UPGRADE 3.11 takes NAME_ and 1-3 digits and refuses
 *             the rest ("FM-1_7001608": Invalid device identity); to a query with the one-byte body 01
 *             (PERF-1's installers) the whole identity, version included
 *   F0 22 24 35 7F F7  upgrade -> ota_session(): pull parts of the package
 *     with cmd 0x30 read requests, check them, stage the package's update
 *     loader at 0xE0000 (outside Felucca's store), ask 0xE0000000 ("success"),
 *     then write the UPDATA_PARM record (flash 0xE4F00 + RAM 0x01C7FD88) and
 *     reset. The SPL runs the loader, which installs the package.
 * Nothing is committed before the host's "success"; any failure erases the
 * staging area and Felucca carries on.
 *
 * The firmware (felucca.c) and the host test supply these hooks:
 *   ota_wire_send(p, n)        one complete F0..F7 message to the host
 *   ota_frame_get(&p, &n)      next received SysEx (7-bit bytes between F0/F7), 0 if none
 *   ota_frame_done()           release it
 *   ota_now_ms(), ota_idle()   time; called while waiting (watchdog, USB)
 *   ota_erase(off) ota_prog(off, p, n) ota_fread(off, p, n)   flash, 0 = ok
 *   ota_show(step, code)       progress / result for the screen
 *   ota_commit(parm)           write RAM record + reset (never returns on hardware)
 */
#define OTA_AREA 0x000E0000u                     /* staged loader, 5 x 4 KiB */
#define OTA_AREA_LEN 0x5000u
#define OTA_RES (OTA_AREA + OTA_AREA_LEN - 0x100u)  /* 0xE4F00 = 4K boundary - 256: SPL scan */
#define OTA_LDR_HCRC 0xEBAAu                     /* official update loader (usb_hid_ota.bin) */
#define OTA_LDR_DCRC 0x5881u
#define OTA_LDR_LEN 0x4DE1u
#define OTA_FRAME_MAX 640u                       /* 512-B response = 605 wire bytes */

static int ota_wire_send(const uint8_t *p, uint32_t n);
static int ota_frame_get(const uint8_t **p, uint32_t *n);
static void ota_frame_done(void);
static uint32_t ota_now_ms(void);
static void ota_idle(void);
static int ota_erase(uint32_t off);
static int ota_prog(uint32_t off, const void *p, uint32_t n);
static int ota_fread(uint32_t off, void *p, uint32_t n);
static void ota_show(uint32_t step, int32_t code);
static void ota_commit(const uint8_t *parm);

static uint32_t ota_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t ota_rd24(const uint8_t *p) { return ota_rd16(p) | (uint32_t)p[2] << 16; }
static uint32_t ota_rd32(const uint8_t *p) { return ota_rd24(p) | (uint32_t)p[3] << 24; }
static void ota_wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void ota_wr32(uint8_t *p, uint32_t v) { ota_wr16(p, v); ota_wr16(p + 2, v >> 16); }

static uint32_t ota_crc16(const uint8_t *p, uint32_t n, uint32_t c)   /* poly 0x1021, init 0 */
{
    uint32_t i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return c;
}

static void ota_jl_enc(uint8_t *p, uint32_t n)   /* UFW header cipher, key 0xFFFF */
{
    uint32_t k = 0xFFFFu;
    while (n--) {
        *p++ ^= (uint8_t)k;
        k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
    }
}

/* 8-to-7, LSB first */
static uint32_t ota_pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8u;
        while (nb >= 7u) {
            out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        out[o++] = (uint8_t)(acc & 0x7Fu);
    return o;
}

static uint32_t ota_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)(*in++ & 0x7Fu) << nb;
        nb += 7u;
        if (nb >= 8u) {
            if (o < max)
                out[o] = (uint8_t)acc;
            o++;
            acc >>= 8;
            nb -= 8u;
        }
    }
    return o;
}

static uint8_t ota_msg[8 + 32], ota_wire[2 + 48];
static uint8_t ota_dec[OTA_FRAME_MAX];           /* decoded host message */

static int ota_send_msg(uint32_t cmd, const uint8_t *body, uint32_t n)   /* n <= 32 */
{
    uint32_t i, s = 0, w;
    ota_msg[0] = 0;
    ota_msg[1] = 0x59;
    ota_msg[2] = (uint8_t)cmd;
    ota_msg[3] = (uint8_t)n;
    ota_msg[4] = 0;
    ota_msg[5] = 0;
    for (i = 0; i < n; i++) {
        ota_msg[6 + i] = body[i];
        s += body[i];
    }
    ota_msg[6 + n] = (uint8_t)~s;
    ota_wire[0] = 0xF0;
    w = 1u + ota_pack7(ota_msg, 7u + n, ota_wire + 1);
    ota_wire[w++] = 0xF7;
    return ota_wire_send(ota_wire, w);
}

#ifdef FELUCCA_LOADER                            /* the update loader, exactly as before ("ota-FM-1_900" passes M-UPGRADE 3.11) */
static void ota_reply_identity(void)
{
    static const char ID[] = FELUCCA_ID;         /* == the package marker string */
    uint8_t id[27];
    uint32_t i;
    for (i = 0; i < sizeof id; i++)
        id[i] = i < sizeof ID - 1u ? (uint8_t)ID[i] : 0;
    ota_send_msg(0x11, id, sizeof id);
}
#define OTA_HS(d) ((d) == 7u && ota_dec[2] == 0x11)
#define OTA_HS_REPLY(d) ota_reply_identity()
#else
static void ota_reply_identity(int full)
{
    static const char ID[] = FELUCCA_ID;         /* == the package marker string */
    uint8_t id[27];
    uint32_t i, n = sizeof ID - 1u;
    if (!full) {                                 /* the model, '_' and three digits: "FM-1_7001612" -> "FM-1_700" */
        for (i = 0; i < n && ID[i] != '_'; i++)
            ;
        if (i + 4u < n)
            n = i + 4u;
    }
    for (i = 0; i < sizeof id; i++)
        id[i] = i < n ? (uint8_t)ID[i] : 0;
    ota_send_msg(0x11, id, sizeof id);
}

/* a decoded handshake: 1 the plain query (M-UPGRADE's), 2 the one asking for the whole identity, 0 neither */
static int ota_is_hs(uint32_t d)
{
    if (ota_dec[2] != 0x11)
        return 0;
    return d == 7u ? 1 : d == 8u && ota_dec[6] == 1 ? 2 : 0;
}
#define OTA_HS(d) ota_is_hs(d)
#define OTA_HS_REPLY(d) ota_reply_identity(ota_is_hs(d) == 2)
#endif

/* decode the pending frame; returns its decoded length (0 = not ours) */
static uint32_t ota_take(void)
{
    const uint8_t *p;
    uint32_t n, d, s = 0, i;
    if (!ota_frame_get(&p, &n))
        return 0;
    d = ota_unpack7(p, n, ota_dec, sizeof ota_dec);
    ota_frame_done();
    if (d < 7u || d > sizeof ota_dec || ota_dec[0] != 0 || ota_dec[1] != 0x59 || ota_rd24(ota_dec + 3) + 7u != d)
        return 0;
    for (i = 6; i + 1u < d; i++)
        s += ota_dec[i];
    if ((uint8_t)~s != ota_dec[d - 1u])
        return 0;
    return d;
}

/* main loop, outside a session: answer handshakes */
static void ota_service(void)
{
    uint32_t d = ota_take();
    if (OTA_HS(d))
        OTA_HS_REPLY(d);
}

/* one read request (3 tries x 5 s); data -> dst */
static uint32_t ota_deadline;                    /* 0 = none; else ota_now_ms() limit of the session */

static int ota_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    uint8_t q[8];
    uint32_t tries, t, d, i;
    if (ota_deadline && (int32_t)(ota_now_ms() - ota_deadline) > 0)
        return -1;                                  /* the whole session took too long: give up */
    q[0] = 0;
    ota_wr32(q + 1, addr);
    q[5] = (uint8_t)len;
    q[6] = (uint8_t)(len >> 8);
    q[7] = 0;
    for (tries = 0; tries < 3u; tries++) {
        if (ota_send_msg(0x30, q, 8))
            return -1;
        for (t = ota_now_ms(); ota_now_ms() - t < 5000u;) {
            ota_idle();
            d = ota_take();
            if (!d)
                continue;
            if (OTA_HS(d)) {
                OTA_HS_REPLY(d);
                continue;
            }
            if (d == len + 15u && ota_dec[2] == 0x30 && ota_rd24(ota_dec + 3) == len + 8u &&
                ota_rd32(ota_dec + 7) == addr && ota_rd24(ota_dec + 11) == len) {
                for (i = 0; i < len; i++)
                    dst[i] = ota_dec[14 + i];
                return 0;
            }
        }
    }
    return -1;
}

static int ota_memeq(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    while (n--)
        if (*a++ != *b++)
            return 0;
    return 1;
}

/* JLFS head "LOADER.BIN" in front of the staged loader (vm_update_defrag), written last */
static int ota_stage_head(uint32_t len)
{
    static const char NAME[16] = "LOADER.BIN";
    uint8_t h[32];
    uint32_t i;
    for (i = 0; i < 32u; i++)
        h[i] = 0;
    ota_wr16(h + 2, 0xFFFFu);
    ota_wr32(h + 4, OTA_AREA);
    ota_wr32(h + 8, len + 32u);
    h[12] = 0x41u;
    h[13] = 0xFFu;
    ota_wr16(h + 14, 0xFFFFu);
    for (i = 0; i < 16u; i++)
        h[16 + i] = (uint8_t)NAME[i];
    ota_wr16(h, ota_crc16(h + 2, 30, 0));
    return ota_prog(OTA_AREA, h, 32);
}

/* UFW header + entry list (hdr: 1 KiB) from the host -> offset / length of
 * flash.bin (entry type 0) and ota.bin (type 100), 0 if absent; -1 read, -2 CRC */
static int ota_ufw(uint8_t *hdr, uint32_t *fl_off, uint32_t *fl_len, uint32_t *ota_off, uint32_t *ota_len)
{
    uint32_t i, nent;
    *fl_off = *fl_len = *ota_off = *ota_len = 0;
    if (ota_read(0, hdr, 512) || ota_read(512, hdr + 512, 512))
        return -1;
    ota_jl_enc(hdr, 0x40);
    nent = ota_rd16(hdr + 8);
    if (ota_crc16(hdr + 2, 0x3E, 0) != ota_rd16(hdr) || nent == 0 || nent > 11u ||
        ota_crc16(hdr + 0x40, nent * 0x50u, 0) != ota_rd16(hdr + 2))
        return -2;
    for (i = 0; i < nent; i++) {
        uint8_t *e = hdr + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) {                             /* flash.bin */
            *fl_off = ota_rd32(e + 8);
            *fl_len = ota_rd32(e + 12);
        }
        if (ota_rd16(e) == 100) {                           /* ota.bin */
            *ota_off = ota_rd32(e + 8);
            *ota_len = ota_rd32(e + 12);
        }
    }
    return 0;
}

#ifndef FELUCCA_LOADER
/* M-VAVE's own boot heads (flash.bin's first 16 KiB, CRC-32 as zlib's): V15 (FM-1_015) and V16 (FM-1_016, offered by
 * M-UPGRADE 3.11). The official loader may rewrite the head only from one of these to the other, as M-VAVE's own
 * V15 -> V16 update does; any other head that differs is refused (-5), as before (0.16.15) */
static uint32_t ota_crc32(const uint8_t *p, uint32_t n, uint32_t c)
{
    uint32_t k;
    c = ~c;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static int ota_known_head(uint32_t crc) { return crc == 0x3A04AD23u || crc == 0x08316064u; }
#endif

static uint32_t ota_touched;                     /* ota_stage began erasing the staging area */
static int ota_stage(void)                       /* steps 1..6; 0 = host said success */
{
    static uint8_t hdr[0x400], b[512], mine[512];
    uint32_t i, n, ota_off, ota_len, fl_off, fl_len, c, len, own, official;
    int rc;
    /* 1. UFW header + entry list */
    ota_show(1, 0);
    if ((rc = ota_ufw(hdr, &fl_off, &fl_len, &ota_off, &ota_len)) != 0)
        return rc;
    if (!fl_off || !ota_off)
        return -3;
    /* 2. which loader: the official one (known CRCs; it rewrites the whole flash.bin
     *    including the head) or Felucca's own (app area only, checks the chip key) */
    if (ota_read(ota_off, b, 512))
        return -6;
    len = ota_rd32(b + 8);
    if (ota_crc16(b + 2, 30, 0) != ota_rd16(b) || ota_rd32(b + 4) != 0x20u || b[12] != 0x41u ||
        ota_len != len + 0x20u || len > OTA_AREA_LEN - 0x200u || ota_crc16(b + 0x22, 30, 0) != ota_rd16(b + 0x20) ||
        ota_rd32(b + 0x20 + 8) != 0x01C0A800u)
        return -7;
    official = ota_rd16(b) == OTA_LDR_HCRC && ota_rd16(b + 2) == OTA_LDR_DCRC && len == OTA_LDR_LEN;
    for (i = 0x48, own = 0; i + 16u <= 0x48u + 64u && !own; i++)    /* first LZ4 literals: crt0_ldr.S */
        own = ota_memeq(b + i, (const uint8_t *)"FELUCCA-LOADER-1", 16) && ota_rd32(b + 0x20 + 4) <= 0x14000u;
    if (!official && !own)
        return -7;
#ifdef FELUCCA_LOADER
    if (official) {                    /* the official loader writes the head: it must equal ours */
        ota_show(2, 0);
        for (i = 0; i < 0x4000u; i += 512u) {
            if (ota_read(fl_off + i, b, 512) || ota_fread(i, mine, 512))
                return -4;
            if (!ota_memeq(b, mine, 512))
                return -5;
        }
    }
#else
    if (official) {                    /* the official loader writes the head: it must equal ours, or both be M-VAVE's (V15, V16) */
        uint32_t cp = 0, cm = 0, same = 1;
        ota_show(2, 0);
        for (i = 0; i < 0x4000u; i += 512u) {
            if (ota_read(fl_off + i, b, 512) || ota_fread(i, mine, 512))
                return -4;
            same &= (uint32_t)ota_memeq(b, mine, 512);
            cp = ota_crc32(b, 512, cp);
            cm = ota_crc32(mine, 512, cm);
        }
        if (!same && !(ota_known_head(cp) && ota_known_head(cm)))
            return -5;
    }
#endif
    /* 4. stage the package's loader: erase, body, verify, head last */
    ota_show(3, 0);
    if (ota_read(ota_off, b, 512))
        return -6;
    c = ota_rd16(b + 2);                                    /* outer dcrc of the body */
    ota_touched = 1;                                        /* from here a failure erases the area (ota_session) */
    for (i = 0; i < OTA_AREA_LEN; i += 0x1000u)
        if (ota_erase(OTA_AREA + i))
            return -8;
    {
        uint32_t crc = 0;
        for (i = 0; i < len; i += n) {
            n = len - i > 512u ? 512u : len - i;
            if (ota_read(ota_off + 0x20u + i, b, n))
                return -9;
            crc = ota_crc16(b, n, crc);
            if (ota_prog(OTA_AREA + 32u + i, b, n))
                return -10;
        }
        if (crc != c)
            return -11;
        for (i = 0, crc = 0; i < len; i += n) {             /* read back from flash */
            n = len - i > 512u ? 512u : len - i;
            if (ota_fread(OTA_AREA + 32u + i, b, n))
                return -11;
            crc = ota_crc16(b, n, crc);
        }
        if (crc != c)
            return -11;
    }
    if (ota_stage_head(len))
        return -12;
    /* 6. the host confirms: nothing is committed without it */
    ota_show(4, 0);
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xE0000000u, b, 8) && ota_memeq(b, (const uint8_t *)"success", 8))
            return 0;
    return -13;
}

static void ota_unstage(void)
{
    uint32_t i;
    for (i = 0; i < OTA_AREA_LEN; i += 0x1000u)
        ota_erase(OTA_AREA + i);
}

/* returns only when nothing was committed (refused, failed, or dry run) */
static int ota_session(void)
{
    static uint8_t parm[112];
    uint8_t back[112];
    uint32_t i;
    int rc;
    ota_deadline = ota_now_ms() + 120000u;          /* step 1 normally takes ~5 s; a stalled host gives up after 2 min */
    ota_touched = 0;
    rc = ota_stage();
    if (rc) {
        if (ota_touched)                            /* refused before staging (a head, a package, the host): the
                                                     * staged rescue loader stays (0.16.17) */
            ota_unstage();
        ota_show(9, rc);
        return rc;
    }
    /* 5./7. UPDATA_PARM (update_mode_api_v2 layout), flash copy first */
    for (i = 0; i < sizeof parm; i++)
        parm[i] = 0;
    ota_wr16(parm + 2, 0x5A0Du);
    ota_wr16(parm + 4, 0x5A01u);
    ota_wr16(parm + 6, 0x5441u);
    for (i = 0; i < 12u; i++)
        parm[8 + i] = (uint8_t)"ota-FM-1_015"[i];    /* the loader's own USB identity */
    ota_wr32(parm + 72, OTA_AREA);
    ota_wr16(parm, ota_crc16(parm + 2, 78, 0));
#if FELUCCA_OTA_DRYRUN
    ota_unstage();
    ota_show(9, 1);                                 /* 1 = dry run complete */
    return 1;
#endif
#ifndef FELUCCA_OTA_RAMONLY
#define FELUCCA_OTA_RAMONLY 0                       /* 1: RAM record only, no power-loss resume (a power
                                                     * cycle leaves a broken loader behind) */
#endif
    if (!FELUCCA_OTA_RAMONLY && (ota_prog(OTA_RES, parm, sizeof parm) || ota_fread(OTA_RES, back, sizeof back) ||
        !ota_memeq(back, parm, sizeof parm))) {
        ota_unstage();
        ota_show(9, -14);
        return -14;
    }
    ota_show(5, 0);
    ota_commit(parm);
    return 0;
}

/* The staged loader is Felucca's own and whole: the boot stub's check (firmware/stub/stub_core.h stub_loader_check,
 * which reads the area through XIP; here through ota_fread in small pieces). Returns the staged length from OTA_AREA
 * (the head, the inner head, the blocks), 0 if the stub would refuse it. (0.16.17, from Melodee 1.3.5's ota.c,
 * Kerem Kilic; the check is ChoralRoot FM-1's, GPL-3.0-only.) */
static uint32_t ota_staged(void)
{
    uint8_t h[0x88], q[8];
    uint32_t len, isize, p, end, total = 0, i, own = 0;
    if (ota_fread(OTA_AREA, h, sizeof h))
        return 0;
    if (ota_crc16(h + 2, 30, 0) != ota_rd16(h) || ota_rd32(h + 4) != OTA_AREA || h[12] != 0x41u ||
        !ota_memeq(h + 16, (const uint8_t *)"LOADER.BIN", 11))
        return 0;
    len = ota_rd32(h + 8) - 32u;
    if (len < 32u + 8u || len > OTA_AREA_LEN - 0x200u)
        return 0;
    isize = ota_rd32(h + 0x20 + 4);
    if (ota_crc16(h + 0x22, 30, 0) != ota_rd16(h + 0x20) || ota_rd32(h + 0x20 + 8) != 0x01C0A800u ||
        !isize || isize > 0x14000u)
        return 0;
    for (i = 0x48; i + 16u <= 0x48u + 64u && !own; i++)
        own = (uint32_t)ota_memeq(h + i, (const uint8_t *)"FELUCCA-LOADER-1", 16);
    if (!own)
        return 0;
    for (p = 0x40, end = 0x20u + len; p + 8u <= end;) {
        uint32_t clen, dlen;
        if (ota_fread(OTA_AREA + p, q, 8))
            return 0;
        clen = ota_rd32(q);
        dlen = ota_rd32(q + 4);
        if (!clen || clen > 4096u || !dlen || dlen > 4096u || clen > end - p - 8u)
            return 0;
        p += 8u + clen;
        total += dlen;
        if (dlen != 4096u && p != end)
            return 0;
    }
    return p == end && total == isize ? end : 0;
}

/* boot (we are running, so an update finished or never started). 0.16.17: a whole staged loader of our own STAYS, for
 * the boot stub to hand an interrupted install to on the next power-on; only the record's sector (the last of the
 * area, OTA_RES) is cleared, so the SPL does not re-enter the loader. Anything else staged (the official loader, a
 * damaged or half-erased area) is erased whole, as before. 0 nothing to do, 1 erased, 2 the record cleared, -1 error */
static int ota_boot_cleanup(void)
{
    uint8_t b[128];
    uint32_t i, off, used = 0, end = ota_staged();
    if (end && end <= OTA_AREA_LEN - 0x1000u) {
        for (off = OTA_AREA_LEN - 0x1000u; off < OTA_AREA_LEN; off += sizeof b) {
            if (ota_fread(OTA_AREA + off, b, sizeof b))
                return -1;
            for (i = 0; i < sizeof b; i++)
                used |= b[i] != 0xFFu;
        }
        if (!used)
            return 0;
        return ota_erase(OTA_AREA + OTA_AREA_LEN - 0x1000u) ? -1 : 2;
    }
    for (off = 0; off < OTA_AREA_LEN && !used; off += sizeof b) {
        if (ota_fread(OTA_AREA + off, b, sizeof b))
            return -1;
        for (i = 0; i < sizeof b; i++)
            used |= b[i] != 0xFFu;
    }
    if (used)
        ota_unstage();
    return (int)used;
}
