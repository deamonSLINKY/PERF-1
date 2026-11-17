/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 deamonSLINKY (PERF-1) */
/* THE LINK CABLE (2026-11-16): two units on one plain USB-C cable, with no USB at all. The FM-1's USB host never registered
 * the SMK (no 5 V on the cable), so the link (amb.c lk_send, ui.c lk_in: channel 16, CC 20 .. 31, and the lead's clock)
 * runs over D+ and D- as two plain wires, one each way (the pin test proved them, 2026-11-16): the lead sends on D+ and
 * listens on D-, the follower the other way. Open-drain by hand, both lines pulled up (hal/fm1_usb.h), so two units set
 * the same way never fight: they only hear nothing.
 *
 * A UART in software, 3333 baud (one bit = 3 ticks of the 10 kHz TIMER5), 8N1, LSB first; lw_tick runs on every tick,
 * lw_poll with usb_poll (outside the render) hands what came in to midi_in_q, as USB-MIDI packets, so ui.c's midi_in
 * takes them as it takes USB's. Only the link's own messages travel (the rest is dropped while the wire is on):
 *   a clock tick:     F8 n             n: a running count (4 bits) and a check of it (3). A lost tick is made up at the next one,
 *                                      a repeat dropped.
 *   anything else:    st d1 d2 c       Start / Continue / Stop (d1 d2 0), the link's CCs (BF 20..31 v); c a CRC-7 of the three.
 * A tick waits for no queued CC (it goes next). The IRQs off for a flash write stop the tick: a gap seen at either end
 * drops the byte coming in, and a packet going out is sent again from its start after a frame of idle line. */
#if PERF_USB_HOST

#define LW_TK 3u                                 /* ticks a bit */
#define LW_Q 16u                                 /* packets waiting (a power of two; the lead's four-bar refresh is 10 at once) */
#define LW_RQ 16u                                /* bytes in (a power of two) */
#define LW_ERR 0xFDu                             /* (in the bytes in: something was lost here, the packet with it; never sent) */
static struct {
    uint32_t t;                                  /* the last tick's time (fm1_ticks) */
    uint32_t q[LW_Q];                            /* the packets waiting (st | d1 << 8 | d2 << 16 | c << 24) */
    volatile uint8_t qw, qr, tkw, tkr;           /* their ring; clock ticks asked for, sent */
    uint8_t me, rx;                              /* the pin this one pulls low (0 D+, 1 D-), the one it listens on */
    uint8_t tx_ph, tx_n, tx_i, tx_len, tx_idle, low;   /* the bit's ticks left, its frame's bits left, the packet's byte, its length, idle bits owed, the line pulled */
    uint16_t tx_sh;                              /* the frame going out: start, 8 bits, stop */
    uint8_t tx_pk[4];
    uint8_t rx_ph, rx_n, rx_sh;                  /* the same coming in */
    uint8_t rb[LW_RQ];
    volatile uint8_t rbw, rbr;
    uint8_t pk[4], pn, tk_last, tk_any;          /* the packet being read; the last tick's count */
    uint16_t sent, got, bad, gaps;               /* (the LINK page: packets out, in, refused; sent again after a pause) */
} lw;

static uint32_t lw_crc7(uint32_t a, uint32_t b, uint32_t c)   /* CRC-7 (x^7 + x^3 + 1, the SD card's) of three bytes */
{
    const uint32_t v = a << 16 | b << 8 | c;
    uint32_t crc = 0, i;
    for (i = 0; i < 24u; i++) {
        const uint32_t fb = (crc >> 6 ^ v >> (23u - i)) & 1u;
        crc = (crc << 1) & 0x7Fu;
        if (fb)
            crc ^= 0x09u;
    }
    return crc;
}

static uint32_t lw_tkb(uint32_t n) { return (n & 15u) | (lw_crc7(0xF8u, n & 15u, 0) & 7u) << 4; }   /* a tick's byte: its count, checked */

static void lw_out(uint32_t pkt)                 /* (midi_out_event, IRQs off or the render): a link message to the wire, the rest dropped */
{
    const uint32_t st = pkt >> 8 & 0xFFu, d1 = pkt >> 16 & 0x7Fu, d2 = pkt >> 24 & 0x7Fu;
    if (st == 0xF8u) {
        lw.tkw++;
        return;
    }
    if (!(st == 0xFAu || st == 0xFBu || st == 0xFCu || (st == 0xBFu && d1 >= 20u && d1 <= 31u)))
        return;
    if ((uint8_t)(lw.qw - lw.qr) >= LW_Q)
        return;
    lw.q[lw.qw % LW_Q] = st | d1 << 8 | d2 << 16 | lw_crc7(st, d1, d2) << 24;
    RING_PUBLISH();
    lw.qw++;
}

static void lw_pull(uint32_t low)
{
    if (low != lw.low) {
        lw.low = (uint8_t)low;
        fm1_usb_pin_low(lw.me, low);
    }
}

static void lw_rb(uint32_t b)                   /* (lw_tick) a byte in, or LW_ERR */
{
    if ((uint8_t)(lw.rbw - lw.rbr) < LW_RQ) {
        lw.rb[lw.rbw % LW_RQ] = (uint8_t)b;
        lw.rbw++;
    }
}

static void lw_tick(uint32_t now)                /* TIMER5, every tick (10 kHz), while the wire is on */
{
    const int gap = now - lw.t > 150u * FM1_TICKS_PER_US;   /* (a tick or more missed: the IRQs were off) */
    const uint32_t in = fm1_usb_pins_read() >> lw.rx & 1u;
    lw.t = now;
    /* in: a start bit's edge seen on tick k, its middle sampled on k + 1, bit j's on k + 4 + 3j, the stop's on k + 28 */
    if (gap) {                                   /* whatever was coming in is lost: its packet goes */
        lw.rx_n = 0;
        lw_rb(LW_ERR);
    }
    if (!lw.rx_n) {
        if (!in) {
            lw.rx_n = 10;
            lw.rx_ph = 1;
        }
    } else if (!--lw.rx_ph) {
        lw.rx_ph = LW_TK;
        if (lw.rx_n == 10u) {
            lw.rx_n = in ? 0 : 9;                /* (a glitch, not a start) */
        } else if (lw.rx_n > 1u) {
            lw.rx_sh = (uint8_t)(lw.rx_sh >> 1 | in << 7);
            lw.rx_n--;
        } else {
            lw_rb(in ? lw.rx_sh : LW_ERR);       /* (no stop bit: not a byte) */
            lw.rx_n = 0;
        }
    }
    /* out: a packet's bytes back to back, the line let go (high) between packets */
    if (gap && (lw.tx_n || lw.tx_i < lw.tx_len)) {   /* a pause mid-packet: a frame of idle line, then the packet again */
        lw.gaps++;
        lw.tx_n = 0;
        lw.tx_i = 0;
        lw.tx_idle = 10;
        lw.tx_ph = 0;
    }
    if (lw.tx_ph && --lw.tx_ph)
        return;
    lw.tx_ph = LW_TK;
    if (!lw.tx_n) {
        if (lw.tx_idle) {
            lw.tx_idle--;
            lw_pull(0);
            return;
        }
        if (lw.tx_i >= lw.tx_len) {              /* the next packet: a tick first */
            lw.tx_i = 0;
            lw.tx_len = 0;
            if (lw.tkr != lw.tkw) {
                lw.tx_pk[0] = 0xF8u;
                lw.tx_pk[1] = (uint8_t)lw_tkb(lw.tkr);
                lw.tkr++;
                lw.tx_len = 2;
            } else if (lw.qr != lw.qw) {
                const uint32_t p = lw.q[lw.qr % LW_Q];
                lw.qr++;
                lw.tx_pk[0] = (uint8_t)p;
                lw.tx_pk[1] = (uint8_t)(p >> 8);
                lw.tx_pk[2] = (uint8_t)(p >> 16);
                lw.tx_pk[3] = (uint8_t)(p >> 24);
                lw.tx_len = 4;
            }
            if (!lw.tx_len) {
                lw_pull(0);
                return;
            }
            lw.sent++;
        }
        lw.tx_sh = (uint16_t)((uint32_t)lw.tx_pk[lw.tx_i++] << 1 | 0x200u);
        lw.tx_n = 10;
    }
    lw_pull(!(lw.tx_sh & 1u));
    lw.tx_sh >>= 1;
    lw.tx_n--;
}

static void lw_in(uint32_t pkt)                  /* into midi_in_q (usb.c), as USB's */
{
    if (mi_w - mi_r >= MQ)
        return;
    midi_in_q[mi_w % MQ] = pkt;
    RING_PUBLISH();
    mi_w++;
}

static void lw_poll(void)                        /* TIMER5 outside the render (usb_poll's place): the bytes in, to packets */
{
    while (lw.rbr != lw.rbw) {
        const uint32_t b = lw.rb[lw.rbr % LW_RQ];
        lw.rbr++;
        if (b == LW_ERR) {
            lw.bad += lw.pn != 0;
            lw.pn = 0;
            continue;
        }
        if (b & 0x80u) {
            lw.pk[0] = (uint8_t)b;
            lw.pn = 1;
            continue;
        }
        if (!lw.pn)
            continue;
        lw.pk[lw.pn++] = (uint8_t)b;
        if (lw.pk[0] == 0xF8u) {                 /* a tick: as many as its count moved on (a lost one made up, a repeat dropped) */
            uint32_t d = lw.tk_any ? (b - lw.tk_last) & 15u : 1u;
            if (b != lw_tkb(b)) {                /* (its check failed: not a tick) */
                lw.bad++;
                lw.pn = 0;
                continue;
            }
            if (d > 4u)
                d = 0;                           /* (far off: counting again from it) */
            lw.tk_last = (uint8_t)b;
            lw.tk_any = 1;
            lw.got++;
            while (d--)
                lw_in(0xFu | 0xF8u << 8);
            lw.pn = 0;
        } else if (lw.pn == 4u) {
            const uint32_t st = lw.pk[0], d1 = lw.pk[1], d2 = lw.pk[2];
            if (lw_crc7(st, d1, d2) == lw.pk[3] && (st >= 0xF0u || st == 0xBFu)) {
                lw.got++;
                lw_in((st >> 4) | st << 8 | d1 << 16 | d2 << 24);
            } else {
                lw.bad++;
            }
            lw.pn = 0;
        }
    }
}

static void lw_start(uint32_t me)                /* (the UI, IRQs off) the wire on: this one pulls pin me (0 D+, 1 D-) */
{
    memset(&lw, 0, sizeof lw);
    lw.me = (uint8_t)(me & 1u);
    lw.rx = (uint8_t)(lw.me ^ 1u);
    lw.t = fm1_ticks();
}
#endif /* PERF_USB_HOST */
