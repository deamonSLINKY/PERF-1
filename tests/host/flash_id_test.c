/* SPDX-License-Identifier: GPL-3.0-only
 * PERF-1: the flash parts the firmware turns its flash on for (firmware/hal/fm1_flash_id.h, 0.16.23). Both FM-1
 * parts must count, nothing else may: a wrong "yes" would write a chip whose commands PERF doesn't know. */
#include <stdio.h>
#include <stdint.h>
#include "fm1_flash_id.h"

static int fails;
static void ok(int c, const char *what)
{
    printf("%s %s\n", c ? "ok  " : "FAIL", what);
    if (!c)
        fails++;
}

int main(void)
{
    ok(FL_JEDEC_OK(0x856014u), "Puya P25Q80 (the first FM-1s): flash on");
    ok(FL_JEDEC_OK(0xB36014u), "UCUN UC25HQ80 (FM-1s since autumn 2026): flash on");
    ok(!FL_JEDEC_OK(0x000000u), "no answer (0): flash off");
    ok(!FL_JEDEC_OK(0xFFFFFFu), "floating bus (FFFFFF): flash off");
    ok(!FL_JEDEC_OK(0xEB6014u), "the SMK-37's part on an FM-1 build: flash off");
    ok(!FL_JEDEC_OK(0x856015u) && !FL_JEDEC_OK(0xB36013u), "a neighbouring ID: flash off");
    ok(FL_CHIP(0x856014u) == 1u && FL_CHIP(0xB36014u) == 2u, "status log: P25Q80 = 1, UC25HQ80 = 2");
    ok(FL_CHIP(0u) == 0u && FL_CHIP(0xFFFFFFu) == 0u && FL_CHIP(0xEB6014u) == 3u, "status log: none = 0, another part = 3");
    {
        uint32_t id, n = 0;                      /* exactly two of the 16M IDs */
        for (id = 0; id < 0x1000000u; id++)
            n += FL_JEDEC_OK(id) ? 1u : 0u;
        ok(n == 2u, "exactly two IDs accepted");
    }
    if (fails)
        return 1;
    printf("flash id: all passed\n");
    return 0;
}
