/*
 * Copyright (C) 2010-2014 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		BIOS interrupt functions.
 */

#include <x86/asm.h>
#include <x86/descriptor.h>

#include <platform/loader.h>

/** Size of the registers structure. */
#define REGS_SIZE	0x24

/** Stack to use in real mode. */
#define REAL_MODE_STACK	0xfffc
#define REAL_MODE_SP	(REAL_MODE_STACK - REGS_SIZE)

.section .text, "ax", @progbits

/** Macro to switch to real mode.
 * @note		Trashes EAX. */
#define PROT_TO_REAL	\
	/* Save the stack pointer at the top of the real mode stack and switch \
	 * to it. */ \
	pushfl; \
	cli; \
	movl	%esp, (REAL_MODE_STACK); \
	movl	$REAL_MODE_SP, %esp; \
	\
	/* Switch to the BIOS IVT. */ \
	lidt	(bios_idtp); \
	\
	/* Set data segments. */ \
	movw	$SEGMENT_DS16, %ax; \
	movw	%ax, %ds; \
	movw	%ax, %es; \
	movw	%ax, %fs; \
	movw	%ax, %gs; \
	movw	%ax, %ss; \
	\
	/* Jump into the real mode code segment. */ \
	ljmp	$SEGMENT_CS16, $1f - LOADER_LOAD_ADDR; \
	\
	/* Clear the PE (Protection Enable) bit of CR0. */ \
1:	movl	%cr0, %eax; \
	andl	$~(1<<0), %eax; \
	movl	%eax, %cr0; \
	\
	/* Set correct segments. */ \
	.byte 0x66; \
	ljmp	$(LOADER_LOAD_ADDR >> 4), $2f - LOADER_LOAD_ADDR; \
.code16 \
2:	xorw	%ax, %ax; \
	mov	%ax, %ds; \
	mov	%ax, %es; \
	mov	%ax, %fs; \
	mov	%ax, %gs; \
	mov	%ax, %ss

/** Macro to switch to protected mode.
 * @note		Trashes EAX. */
#define REAL_TO_PROT	\
	/* Ensure interrupts are disabled. */ \
	cli; \
	\
	/* Return to protected mode. */ \
	movl	%cr0, %eax; \
	orl	$(1<<0), %eax; \
	movl	%eax, %cr0; \
	data32 ljmp $SEGMENT_CS, $3f; \
.code32 \
3: \
	mov	$SEGMENT_DS, %ax; \
	mov	%ax, %ds; \
	mov	%ax, %es; \
	mov	%ax, %fs; \
	mov	%ax, %gs; \
	mov	%ax, %ss; \
	\
	/* Switch back to the loader IDT. */ \
	lidt	(loader_idtp); \
	\
	/* Restore stack pointer. */ \
	mov	(REAL_MODE_STACK), %esp; \
	popfl

/** Call a BIOS interrupt.
 * @param num		Interrupt number.
 * @param regs		Registers to use for the interrupt. */
FUNCTION_START(bios_call)
	/* Save callee-save registers. */
	push	%ebp
	push	%ebx
	push	%edi
	push	%esi

	/* Write the interrupt number. */
	movl	20(%esp), %eax
	movb	%al, .Lint

	/* Copy the registers structure to the real mode stack. */
	movl	24(%esp), %esi
	movl	$REAL_MODE_SP, %edi
	movl	$(REGS_SIZE >> 2), %ecx
	rep movsl

	/* Switch to real mode. */
	PROT_TO_REAL

	/* Get the registers to use. */
	pop	%eax
	movl	28(%esp), %eax
	mov	%ax, %es
	movl	0(%esp), %eax
	movl	4(%esp), %ebx
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	movl	16(%esp), %edi
	movl	20(%esp), %esi
	movl	24(%esp), %ebp

	/* Enable interrupts across the interrupt. */
	sti

	/* Hard-code in an INT instruction that gets modified above to contain
	 * the correct interrupt number. */
	.byte 0xcd
.Lint:	.byte 0x0

	/* Save the new register/flags state. */
	mov	%eax, 0(%esp)
	mov	%ebx, 4(%esp)
	mov	%ecx, 8(%esp)
	mov	%edx, 12(%esp)
	mov	%edi, 16(%esp)
	mov	%esi, 20(%esp)
	mov	%ebp, 24(%esp)
	mov	%es, %ax
	movl	%eax, 28(%esp)
	pushfl

	/* Switch back to protected mode. */
	REAL_TO_PROT

	/* Copy the registers structure back from the real mode stack. */
	movl	$REAL_MODE_SP, %esi
	movl	24(%esp), %edi
	movl	$(REGS_SIZE >> 2), %ecx
	rep movsl

	/* Pop callee-save registers and return. */
	pop	%esi
	pop	%edi
	pop	%ebx
	pop	%ebp
	ret
FUNCTION_END(bios_call)

/** IDT pointer for the BIOS IVT. */
SYMBOL(bios_idtp)
	.word 0x7ff
	.long 0
