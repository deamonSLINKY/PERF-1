/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB host's reading of an attached device (usb_host_desc.h, from Melodee 1.5.0): a class-compliant USB-MIDI configuration gives
 * its streaming interface's IN and OUT endpoints, an audio-only one gives none; the packets let through are channel messages and
 * clock / transport, never SysEx (the update and editor commands are the computer's, not a controller's). */
#include <stdio.h>
#include <stdint.h>
#include "usb_host_desc.h"

static int fails;
static void check(int ok, const char *what)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

int main(void)
{
    /* a pad box as most are: configuration 1, audio control (interface 0), MIDI streaming (interface 1) with bulk OUT 0x01 and
     * IN 0x81 of 64 bytes (the class-specific descriptors between them are skipped) */
    static const uint8_t pads[] = {
        9, 2, 101, 0, 2, 1, 0, 0x80, 50,
        9, 4, 0, 0, 0, 1, 1, 0, 0,
        9, 0x24, 1, 0, 1, 9, 0, 1, 1,
        9, 4, 1, 0, 2, 1, 3, 0, 0,
        7, 0x24, 1, 0, 1, 25, 0,
        6, 0x24, 2, 1, 1, 0,
        9, 5, 0x01, 2, 64, 0, 0, 0, 0,
        5, 0x25, 1, 1, 1,
        9, 5, 0x81, 2, 64, 0, 0, 0, 0,
        5, 0x25, 1, 1, 3};
    /* an audio-only device: streaming interface subclass 2, an isochronous endpoint */
    static const uint8_t audio[] = {
        9, 2, 34, 0, 1, 1, 0, 0x80, 50,
        9, 4, 0, 1, 1, 1, 2, 0, 0,
        9, 5, 0x82, 1, 192, 0, 1, 0, 0};
    uh_midi_t m;
    check(uh_parse_midi(pads, sizeof pads, &m) && m.cfg == 1 && m.iface == 1 && m.alt == 0, "a USB-MIDI pad box: its streaming interface found");
    check(m.in_ep == 1 && m.in_type == 2 && m.in_mps == 64 && m.out_ep == 1 && m.out_mps == 64, "its IN and OUT endpoints (bulk, 64 bytes)");
    check(!uh_parse_midi(audio, sizeof audio, &m), "an audio-only device: no MIDI");
    check(!uh_parse_midi(pads, 40, &m), "a configuration cut short before the endpoints: nothing");
    /* packets, low byte first: cable << 4 | CIN, status, data, data */
    check(uh_midi_ok(0x643C9909u) == 0x643C9909u, "a note on passes");
    check(uh_midi_ok(0x643C9919u) == 0x643C9909u, "a note on from cable 1 passes as cable 0");
    check(uh_midi_ok(0x0000F80Fu) != 0u && uh_midi_ok(0x0000FA0Fu) != 0u && uh_midi_ok(0x0000FC0Fu) != 0u, "clock, start and stop pass");
    check(uh_midi_ok(0x0000FE0Fu) == 0u, "active sensing does not");
    check(uh_midi_ok(0x7D00F004u) == 0u && uh_midi_ok(0x0000F705u) == 0u, "SysEx does not");
    check(uh_midi_ok(0x643C8909u) == 0u, "a status that does not match its CIN does not");
    printf(fails ? "usbhost: %d FAILED\n" : "usbhost: ok\n", fails);
    return fails != 0;
}
