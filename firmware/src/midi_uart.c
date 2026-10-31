/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio): the ring read by content (Melodee 1.4.3) */
/* MIDI IN on the TRS jack (ported from Felucca 1.0's midi_uart.c): PH8 -> input channel 1 -> UART1
 * RX, 31250 baud, RX DMA into a 128-byte ring, polled from the TIMER5 ISR outside the render, the
 * same context as usb_poll. Channel messages and Clock / Start / Continue / Stop go into midi_in_q
 * beside USB's, as USB-MIDI packets (cable 0, CIN = status >> 4), so the engine cannot tell them
 * apart; other system messages and SysEx are ignored (no SysEx -> UBOOT from the jack).
 *
 * The ring is read by content, not by the DMA's count (Melodee 1.4.3's fix, 0.15.8 here): every slot not yet
 * read holds UM_EMPTY, and the reader takes bytes until it meets one. The count once missed a byte on an FM-1
 * while the byte was in the ring; the reader then stayed one byte behind until reboot, so each message ended
 * only with the next byte (an MPC pad sounded on its first aftertouch, and its note-off held the note until the
 * next pad). Following the content cannot drift; the count still tells an overrun. */
#include "../hal/fm1_uart.h"   /* registers; relative, so the host test finds it too */

#define UM_RING 128u
#define UM_EMPTY 0xFDu         /* an undefined realtime byte: never sent, it marks the slots not yet written */
static volatile uint8_t um_ring[UM_RING] __attribute__((aligned(16)));
static struct {
    uint32_t rd;
    uint32_t bytes, drops, msgs;
    uint8_t st, need, got, d0, sysex;
} um;

static int um_put(uint32_t pkt)                    /* into midi_in_q (usb.c): 0 = full */
{
    if (mi_w - mi_r >= MQ)
        return 0;
    midi_in_q[mi_w % MQ] = pkt;
    RING_PUBLISH();
    mi_w++;
    return 1;
}

static void uart_midi_init(void)                   /* before TIMER5 starts: PORTH read-modify-write */
{
    uint32_t i;
    for (i = 0; i < UM_RING; i++)
        um_ring[i] = UM_EMPTY;
    fm1_uart1_midi_init(um_ring, UM_RING);
}

static uint32_t um_len(uint32_t s) { return (s & 0xE0u) == 0xC0u ? 1u : 2u; }   /* Cx Dx: 1, else 2 */

static void um_byte(uint32_t b)
{
    if (b >= 0xF8u) {                              /* real time: any time, keeps running status */
        if (b == 0xF8u || b == 0xFAu || b == 0xFBu || b == 0xFCu) {
            if (um_put(0xFu | b << 8))
                um.msgs++;
            else
                um.drops++;
        }
        return;
    }
    if (b & 0x80u) {
        um.sysex = b == 0xF0u;
        um.st = b < 0xF0u ? (uint8_t)b : 0;        /* system common / SysEx end running status */
        um.need = (uint8_t)um_len(b);
        um.got = 0;
        return;
    }
    if (um.sysex || !um.st)
        return;
    if (um.need == 2u && !um.got) {
        um.d0 = (uint8_t)b;
        um.got = 1;
        return;
    }
    {
        uint32_t d1 = um.need == 2u ? um.d0 : b, d2 = um.need == 2u ? b : 0u;
        um.got = 0;
        if (um_put((um.st >> 4) | (uint32_t)um.st << 8 | d1 << 16 | d2 << 24))
            um.msgs++;
        else
            um.drops++;
    }
}

/* the bytes the DMA has written since the last call, by content; n, its count, only tells an overrun (the DMA went
 * round the ring past unread bytes: the newest UM_RING are read, the rest counted as drops). A received UM_EMPTY
 * (line noise) is passed over once the byte after it has landed. */
static void uart_midi_take(uint32_t n)
{
    uint32_t k;
    if (n > UM_RING) {                             /* the DMA lapped us: wait for a whole new status */
        um.drops += n - UM_RING;
        um.rd = (um.rd + n - UM_RING) & (UM_RING - 1u);
        um.st = um.got = um.sysex = 0;
    }
    for (k = 0; k < UM_RING; k++) {
        const uint32_t b = um_ring[um.rd];
        if (b == UM_EMPTY && um_ring[(um.rd + 1u) & (UM_RING - 1u)] == UM_EMPTY)
            break;                                 /* (an empty slot with a byte after it: noise, read and ignored) */
        um_ring[um.rd] = UM_EMPTY;
        um.rd = (um.rd + 1u) & (UM_RING - 1u);
        um.bytes++;
        um_byte(b);                                /* (UM_EMPTY is an unknown realtime byte: ignored) */
    }
}

static void uart_midi_poll(void) { uart_midi_take(fm1_uart1_rx_take()); }   /* TIMER5, outside the render */
