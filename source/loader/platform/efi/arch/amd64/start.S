/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief		AMD64 EFI startup code.
 */

#include <arch/page.h>

#include <x86/asm.h>

.section .text, "ax", @progbits

/** EFI entry point. */
FUNCTION_START(loader_entry)
	/* We are entered with interrupts enabled. We don't want them. */
	cli

	/* EFI uses the Microsoft x86_64 ABI. Arguments are passed in RCX/RDX. */
	pushq	%rcx
	pushq	%rdx

	/* Relocate the loader. */
	leaq	__start(%rip), %rdi
	leaq	_DYNAMIC(%rip), %rsi
	call	efi_arch_relocate
	popq	%rsi
	popq	%rdi
	testq	%rax, %rax
	jz	1f
	ret
1:

	mov	$0x3f8, %dx
	mov	$'H', %al
	out	%al, (%dx)
	mov	$'e', %al
	out	%al, (%dx)
	mov	$'l', %al
	out	%al, (%dx)
	mov	$'l', %al
	out	%al, (%dx)
	mov	$'o', %al
	out	%al, (%dx)
	mov	$'\r', %al
	out	%al, (%dx)
	mov	$'\n', %al
	out	%al, (%dx)
1:	jmp	1b
FUNCTION_END(loader_entry)

/**
 * Dummy PE relocation so that the EFI loader recognizes us as relocatable.
 */

.section .data, "aw", @progbits

__dummy:
	.long   0

.section .reloc, "aw", @progbits

__dummy_reloc:
        .long   __dummy - __dummy_reloc
        .long   10
        .word   0
