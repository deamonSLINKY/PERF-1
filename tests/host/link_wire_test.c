/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 deamonSLINKY (PERF-1) */
/* The link cable (firmware/src/link_wire.c) on the host: two units, each with its own 10 kHz tick (the second one 50 ppm
 * slow and out of phase), on two shared open-drain lines. The lead sends the link's CCs, Start and a 24 ppqn clock at 175 BPM,
 * the follower its hellos. Checked: everything arrives in order, nothing made up; with pauses (the IRQs off for a flash
 * write, 0.3 .. 1.8 ms, at either end) no packet is made up, the clock's ticks all arrive (a lost one made up), and a lost
 * CC is only one that the paused receiver could not hear. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PERF_USB_HOST 1
#define FM1_TICKS_PER_US 1u                      /* (the simulation's time: microseconds) */
#define RING_PUBLISH() ((void)0)
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;
static uint32_t sim_now;
static uint8_t pull[2][2];                       /* [unit][pin] pulled low */
static int cur;
static uint32_t fm1_ticks(void) { return sim_now; }
static uint32_t fm1_usb_pins_read(void)
{
    return (uint32_t)!(pull[0][0] | pull[1][0]) | (uint32_t)!(pull[0][1] | pull[1][1]) << 1;
}
static void fm1_usb_pin_low(uint32_t pin, uint32_t low) { pull[cur][pin & 1u] = (uint8_t)(low != 0); }
#include "link_wire.c"

typedef struct {
    __typeof__(lw) lw;
    uint32_t q[MQ], w, r;
} unit_t;
static unit_t u[2];
static void load(int k)
{
    cur = k;
    memcpy((void *)&lw, &u[k].lw, sizeof lw);
    memcpy(midi_in_q, u[k].q, sizeof midi_in_q);
    mi_w = u[k].w;
    mi_r = u[k].r;
}
static void save(int k)
{
    memcpy(&u[k].lw, (void *)&lw, sizeof lw);
    memcpy(u[k].q, midi_in_q, sizeof midi_in_q);
    u[k].w = mi_w;
    u[k].r = mi_r;
}

#define NS 4096
static uint32_t sent[2][NS], nsent[2], ticks_sent;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void send(int k, uint32_t st, uint32_t d1, uint32_t d2)   /* (the engine's MIDI out, through midi_out_event) */
{
    const uint32_t pkt = st >> 4 | st << 8 | d1 << 16 | d2 << 24;
    load(k);
    if (st == 0xF8u)
        ticks_sent++;
    else if ((uint8_t)(lw.qw - lw.qr) < LW_Q && nsent[k] < NS)
        sent[k][nsent[k]++] = pkt;
    lw_out(pkt);
    save(k);
}

static int run(const char *name, uint32_t seconds, int pauses, uint32_t seed)
{
    uint64_t next[2] = {0, 37000};               /* (ns) each unit's next tick: the second 37 us later */
    const uint64_t per[2] = {100000, 100005};    /* (ns) 10 kHz, the second 50 ppm slow */
    uint64_t pause_end[2] = {0, 0}, t_end = (uint64_t)seconds * 1000000000u, next_clk = 0, next_cc = 0, next_hello = 0;
    uint32_t sub[2] = {0, 0}, got[2] = {0, 0}, ticks_got = 0, bogus = 0, cc_i = 0, n_pause[2] = {0, 0};
    uint32_t rd[2] = {0, 0};
    const uint64_t clk_per = 60000000000ull / 175u / 24u;
    srand(seed);
    memset(u, 0, sizeof u);
    memset(pull, 0, sizeof pull);
    memset(nsent, 0, sizeof nsent);
    ticks_sent = 0;
    sim_now = 0;
    load(0); lw_start(0); save(0);               /* the lead: D+ */
    load(1); lw_start(1); save(1);               /* the follower: D- */
    for (;;) {
        const int k = next[0] <= next[1] ? 0 : 1;
        const uint64_t t = next[k];
        if (t >= t_end)
            break;
        /* the engines, between ticks */
        while (next_clk <= t) {
            send(0, 0xF8u, 0, 0);
            next_clk += clk_per;
        }
        if (next_cc <= t) {                      /* the lead's: Start once, then its CCs, a few a second, sometimes in a burst */
            if (!cc_i)
                send(0, 0xFAu, 0, 0);
            else {
                const int burst = cc_i % 7u == 0 ? 6 : 1, b0 = burst;
                int b;
                for (b = 0; b < b0; b++)
                    send(0, 0xBFu, 23u + (cc_i + (uint32_t)b) % 9u, (cc_i * 5u + (uint32_t)b) & 0x7Fu);
            }
            cc_i++;
            next_cc = t + 90000000u + (uint64_t)(rand() % 60) * 1000000u;
        }
        if (next_hello <= t) {                   /* both: a hello a second */
            send(0, 0xBFu, 20, 2 | 64);
            send(1, 0xBFu, 20, 1);
            next_hello = t + 1000000000u;
        }
        next[k] += per[k];
        if (t < pause_end[k])                    /* IRQs off: this tick never runs */
            continue;
        if (pauses && rand() % 20000 == 0) {     /* about one a 2 s: 0.3 .. 1.8 ms */
            pause_end[k] = t + 300000u + (uint64_t)(rand() % 1500) * 1000u;
            n_pause[k]++;
            continue;
        }
        sim_now = (uint32_t)(t / 1000u);
        load(k);
        lw_tick(sim_now);
        if (++sub[k] % 5u == 0u)
            lw_poll();
        while (mi_r != mi_w) {                   /* what the UI's midi_in would take */
            const uint32_t p = midi_in_q[mi_r % MQ];
            mi_r++;
            if ((p >> 8 & 0xFFu) == 0xF8u) {
                ticks_got += k == 1;
                bogus += k == 0;
                continue;
            }
            {   /* in order, perhaps one sent again (a pause mid-packet), perhaps some lost (a pause at the receiver): never one not sent */
                const int from = !k;
                uint32_t j = rd[k] > 0 ? rd[k] - 1u : 0u;
                while (j < nsent[from] && sent[from][j] != p)
                    j++;
                if (j >= nsent[from]) {
                    bogus++;
                    if (bogus < 5)
                        printf("  bogus at unit %d: %08X\n", k, p);
                } else {
                    rd[k] = j + 1u;
                    got[k]++;
                }
            }
        }
        save(k);
    }
    load(0);
    {
        const uint32_t s0 = lw.sent, b0 = lw.bad, p0 = lw.gaps;
        load(1);
        printf("%-12s %us: lead sent %u packets (%u CC/Start, %u ticks), follower got %u + %u ticks; follower sent %u, lead got %u; "
               "bad %u/%u, resent %u/%u, pauses %u/%u, bogus %u\n",
               name, seconds, s0, nsent[0], ticks_sent, got[1], ticks_got, nsent[1], got[0], b0, lw.bad, p0, lw.gaps, n_pause[0], n_pause[1], bogus);
        CHECK(bogus == 0, "%s: %u packets made up", name, bogus);
        CHECK(ticks_got + 2u >= ticks_sent && ticks_got <= ticks_sent, "%s: ticks %u of %u", name, ticks_got, ticks_sent);
        if (!pauses) {
            CHECK(got[1] == nsent[0] && got[0] == nsent[1], "%s: lost packets with no pause (%u/%u, %u/%u)", name, got[1], nsent[0], got[0], nsent[1]);
            CHECK(b0 == 0 && lw.bad == 0, "%s: bad packets with no pause", name);
        } else {
            CHECK(got[1] + 3u * (n_pause[1] + 1u) >= nsent[0], "%s: lost more than the pauses explain (%u of %u)", name, got[1], nsent[0]);
        }
    }
    return 0;
}

int main(void)
{
    uint32_t s;
    printf("CRC-7 of BF 14 42: %02X\n", lw_crc7(0xBF, 20, 66));
    run("clean", 60, 0, 1);
    for (s = 1; s <= 4; s++)
        run("pauses", 120, 1, s * 7919u);
    if (fails) {
        printf("link_wire_test: %d FAILED\n", fails);
        return 1;
    }
    printf("link_wire_test: ok\n");
    return 0;
}
