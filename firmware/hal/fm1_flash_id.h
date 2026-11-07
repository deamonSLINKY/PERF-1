/* SPDX-License-Identifier: GPL-3.0-only
 * PERF-1 (deamonSLINKY): which flash parts the FM-1 carries (0.16.23). Its own header so the host test
 * (tests/host/flash_id_test.c) checks the same macros the firmware uses.
 *
 * The first FM-1s carry a Puya P25Q80 (JEDEC 0x856014); units sold since autumn 2026 a UCUN UC25HQ80 (0xB36014).
 * Both 1 MiB, the same commands: 06 20 02 0B 05 35 9F, erase suspend 75h and resume 7Ah (the UC25HQ80's datasheet:
 * suspend latency 45 us max, 200 us of resume before the next suspend; PERF waits up to 150 us and resumes for 1.2 ms
 * slices). Before 0.16.23 PERF turned the flash on for the Puya only: on a newer unit saves stayed in RAM and installs
 * were refused (issue #5; Melodee 1.6.0's fix, keremimo/melodee#110, is the same check). */
#ifndef FM1_FLASH_ID_H
#define FM1_FLASH_ID_H

#define FL_ID_P25Q80   0x856014u
#define FL_ID_UC25HQ80 0xB36014u

#define FL_JEDEC_OK(id) ((id) == FL_ID_P25Q80 || (id) == FL_ID_UC25HQ80)

/* for the status log: 0 no answer, 1 P25Q80, 2 UC25HQ80, 3 another part (flash off) */
#define FL_CHIP(id) ((id) == FL_ID_P25Q80 ? 1u : (id) == FL_ID_UC25HQ80 ? 2u : (id) == 0u || (id) == 0xFFFFFFu ? 0u : 3u)

#endif
