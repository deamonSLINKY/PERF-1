/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* IRQ entry wrappers: save what a C call may clobber, call the C body,
 * restore, csync, rti. clang does not emit the csync needed before rti,
 * so ISRs are not written in C directly. */
	.macro FM1_ISR name, body, sect=.text
	.section \sect\().\name,"ax",@progbits
	.globl \name
\name:
	[--sp] = {psr, rets, reti}
	[--sp] = {r3-r0}
	call \body
	{r3-r0} = [sp++]
	{psr, rets, reti} = [sp++]
	csync
	rti
	.endm

	FM1_ISR isr_timer5, fm1_timer5_irq, .ram_hot	/* PERF-1 0.16.22: in RAM with its body (main_fm1.c) */
	FM1_ISR isr_alnk0, fm1_alnk0_irq
