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
 * @brief		BIOS CD boot sector.
 */

#include <platform/loader.h>

#include <x86/asm.h>

/** Various definitions. */
#define ISO9660_BLOCK_SHIFT	11	/**< Shift to get size of an ISO9660 block. */
#define ISO9660_BLOCK_SIZE	(1<<11)	/**< Size of an ISO9660 block. */
#define STACK_OFFSET		0x6000	/**< Address of the stack. */

.section .text
.code16

/** Entry function - jumps to the main function. */
FUNCTION_START(_start)
	jmp	boot
FUNCTION_END(_start)

/** Boot information table. */
. = _start + 8
bi_pvd:		.long 0			/**< LBA of the Primary Volume Descriptor. */
bi_file:	.long 0			/**< LBA of the boot file. */
bi_length:	.long 0			/**< Length of the boot file. */
bi_checksum:	.long 0			/**< Checksum. */
bi_reserved:	.space 40		/**< Reserved. */

/** Main function of the CD boot sector. */
PRIVATE_FUNCTION_START(boot)
	/* Set up the stack and other segment registers. */
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$STACK_OFFSET, %sp
	movw	%ax, %ds
	movw	%ax, %es

	movw	$msg_loading, %si
	call	puts

	/* Compute the block to read from, and the number of blocks to read.
	 * We are 1 block long, the rest of the file is the loader itself. */
	movl	%ds:bi_file, %eax
	inc	%eax
	movl	%eax, %ds:dap_lba_low
	movl	%ds:bi_length, %ecx
	subl	$ISO9660_BLOCK_SIZE, %ecx
	orl	%ecx, %ecx
	jz	.Linfo_invalid
	addl	$(ISO9660_BLOCK_SIZE - 1), %ecx
	shrl	$ISO9660_BLOCK_SHIFT, %ecx

	/* Read in 32 block (64KB) chunks (most BIOSen cannot read more than
	 * 64KB at a time). */
1:	cmp	$0, %ecx
	je	3f
	cmp	$32, %ecx
	jbe	2f
	movb	$32, %ds:dap_count
	movw	$dap_size, %si
	movb	$0x42, %ah
	int	$0x13
	jc	.Lread_failed
	subl	$32, %ecx
	addl	$32, %ds:dap_lba_low
	addl	$0x1000, %ds:dap_buffer_segment
	jmp	1b

2:	movb	%cl, %ds:dap_count
	movw	$dap_size, %si
	movb	$0x42, %ah
	int	$0x13
	jc	.Lread_failed

3:	movw	$msg_ok, %si
	call	puts

	/* Execute the bootloader. */
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	ljmp	$(LOADER_LOAD_ADDR >> 4), $0
.Linfo_invalid:
	movw	$msg_info_invalid, %si
	jmp	.Lfail
.Lread_failed:
	movw	$msg_read_failed, %si
	jmp	.Lfail
.Lfail:
	call	puts
.Lhalt:
	jmp	.Lhalt
FUNCTION_END(boot)

/** Display a string on the screen.
 * @param %ds:%si	String to display. */
PRIVATE_FUNCTION_START(puts)
	pushw	%ax
	cld
1:	lodsb
	test	%al, %al
	jz	2f
	pushw	%bx
	movb	$0x0E, %ah
	movw	$1, %bx
	int	$0x10
	popw	%bx
	jmp	1b
2:	popw	%ax
	ret
FUNCTION_END(puts)

/** Message strings. */
msg_loading:		.asciz "Loading... "
msg_info_invalid:	.asciz "Boot information is invalid!\r\n"
msg_read_failed:	.asciz "Read failed!\r\n"
msg_ok:			.asciz "OK\r\n"

/** Disk address packet. */
.align 4
dap_size:		.byte 0x10
dap_reserved1:		.byte 0
dap_count:		.byte 0
dap_reserved2:		.byte 0
dap_buffer_offset:	.word 0
dap_buffer_segment:	.word (LOADER_LOAD_ADDR >> 4)
dap_lba_low:		.long 0
dap_lba_high:		.long 0

/** Pad the file to 2KB. */
. = _start + 0x7FF
.byte 0
