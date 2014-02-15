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
#include <x86/descriptor.h>

.section .text, "ax", @progbits

/** EFI entry point.
 * @param %rcx		Handle to the loader image.
 * @param %rdx		Pointer to EFI system table. */
FUNCTION_START(loader_entry)
	/* We are entered with interrupts enabled. We don't want them. */
	cli

	/* Clear the stack frame pointer/EFLAGS. */
	xorl	%ebp, %ebp
	push	$0
	popf

	/* EFI uses the Microsoft x86_64 ABI. Arguments are passed in RCX/RDX. */
	pushq	%rcx
	pushq	%rdx

	/* Relocate the loader. */
	leaq	__start(%rip), %rdi
	leaq	_DYNAMIC(%rip), %rsi
	call	efi_arch_relocate
	testq	%rax, %rax
	jz	1f
	addq	$16, %rsp
	ret
1:
	/* Zero the BSS section. */
	leaq	__bss_start(%rip), %rdi
	leaq	__bss_end(%rip), %rcx
	subq	%rdi, %rcx
	xorb	%al, %al
	rep stosb

	/* Save the EFI GDT and IDT pointers which we must restore before
	 * calling any EFI functions. */
	leaq	efi_gdtp(%rip), %rax
	sgdt	(%rax)
	leaq	efi_idtp(%rip), %rax
	sidt	(%rax)
	leaq	efi_cs(%rip), %rax
	movw	%cs, (%rax)

	/* Load the GDT. */
	leaq	loader_gdtp(%rip), %rax
	lgdt	(%rax)
	push	$SEGMENT_CS
	leaq	2f(%rip), %rax
	push	%rax
	lretq
2:
	/* Call the main function. */
	popq	%rsi
	popq	%rdi
	call	platform_init
	ret
FUNCTION_END(loader_entry)

/** EFI call wrapper. */
FUNCTION_START(__efi_call)
	/* Switch to the EFI GDT/IDT. */
	leaq	efi_gdtp(%rip), %rax
	lgdt	(%rax)
	leaq	efi_cs(%rip), %rax
	movzxw	(%rax), %rax
	push	%rax
	leaq	1f(%rip), %rax
	push	%rax
	lretq
1:	leaq	efi_idtp(%rip), %rax
	lidt	(%rax)

	/* Get the function address to call. */
	leaq	__efi_call_func(%rip), %r10
	movq	(%r10), %rax

	/* We need to return back here, so swap out the return address on the
	 * stack to point to the return point below. Save the old address in
	 * __efi_call_func. */
	movq	(%rsp), %r11
	movq	%r11, (%r10)
	leaq	2f(%rip), %r11
	movq	%r11, (%rsp)

	/* Enable interrupts. It's possible there's some EFI implementations
	 * that break if you call them with interrupts off... */
	sti

	/* Call the function. */
	jmp	*%rax
2:
	/* Disable interrupts again. */
	cli

	/* Put back the real return address. */
	leaq	__efi_call_func(%rip), %r10
	movq	(%r10), %r10
	push	%r10

	/* Load our GDT/IDT. */
	leaq	loader_gdtp(%rip), %rdx
	lgdt	(%rdx)
	push	$SEGMENT_CS
	leaq	3f(%rip), %rdx
	push	%rdx
	lretq
3:	leaq	loader_idtp(%rip), %rdx
	lidt	(%rdx)

	/* And we're done! */
	ret
FUNCTION_END(__efi_call)

.section .data, "aw", @progbits

/** EFI function call address. */
SYMBOL(__efi_call_func)
	.quad	0

/** Saved EFI code segment. */
efi_cs:
	.word	0

/** Saved EFI GDT pointer. */
efi_gdtp:
	.word	0
	.quad	0

/** Saved EFI IDT pointer. */
efi_idtp:
	.word	0
	.quad	0

__dummy:
	.long	0

/**
 * Dummy PE relocation so that the EFI loader recognizes us as relocatable.
 */

.section .reloc, "aw", @progbits

__dummy_reloc:
	.long	__dummy - __dummy_reloc
	.long	10
	.word	0
