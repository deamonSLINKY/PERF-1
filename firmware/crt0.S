/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* App entry (the body's first byte, XIP 0x02001000). The boot stub (firmware/stub, 0.16.17) is what the SPL
 * enters at 0x02000120: it saved the core state the SPL left (STATUS, 0x1C07F00 + 0x20..) as this file used
 * to, checked the commit marker and the body's CRC, and jumps here. Interrupts off and our stacks. The
 * `goto +4; rts` head stays (the stub's own entry has it too; harmless here). */
	.section .text.crt0,"ax",@progbits
	.globl _start
_start:
	goto 1f
	rts
1:	cli
	sp = _ustack_top
	ssp = _sstack_top
	call fm1_cstart
	goto .

/* CPU1 entry (experimental second core, EN_CORE2), after cpu.a/startup.S.o: install separate
 * supervisor/user stacks, then return from the startup interrupt into the RAM polling loop. No
 * application interrupt is enabled on this core. */
#if EN_CORE2
	.section .text.core1,"ax",@progbits
	.globl fm1_core1_start
fm1_core1_start:
	cli
	icfg = 0
	usp = _cpu1_ustack_top
	sp = _cpu1_sstack_top
	ssp = _cpu1_sstack_top
	reti = fm1_core1_main
	rti
#endif
