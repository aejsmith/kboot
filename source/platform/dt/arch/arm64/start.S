/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               ARM64 DT startup code.
 */

#include <arch/page.h>

#include <arm64/asm.h>

/* Define to a platform-specific UART TX register address for some debug output. */
//#define UART_TX         0x09000000    /* QEMU virt machine */
//#define UART_TX         0x3f215040    /* Raspberry Pi 3B. */

/** Print a character if enabled. Clobbers x27/x28. */
.macro PUTC c
#ifdef UART_TX
    ldr     x27, =UART_TX
    mov     w28, #\c
    strb    w28, [x27]
#endif
.endm

/* Ensure that this code is linked in first. */
.section .init.text, "ax", @progbits

linux_header:
    /*
     * The very start of the image is the Linux kernel header. We begin
     * execution here so it starts with a jump over the header.
     *
     * https://www.kernel.org/doc/html/latest/arm64/booting.html
     */

    b       loader_entry            /* code0 */
    .long   0                       /* code1 */
    .quad   0                       /* text_offset - image load offset */
    .quad   __image_size            /* image_size - effective image size */
    .quad   0                       /* flags - kernel flags */
    .quad   0                       /* reserved */
    .quad   0                       /* reserved */
    .quad   0                       /* reserved */
    .long   0x644d5241              /* magic - Magic number, "ARM\x64". */
    .long   0                       /* reserved. */

/** Entry point.
 * @param x0            Physical address of DTB.
 * @param x1            0 (reserved for future use).
 * @param x2            0 (reserved for future use).
 * @param x3            0 (reserved for future use). */
FUNCTION_START(loader_entry)
    /*
     * Expected state:
     *  - MMU off.
     *  - DCache off.
     *  - ICache on or off with no stale references to the kernel image.
     *  - All interrupts disabled (PSTATE.DAIF = 1)
     *  - EL2 (recommended if virtualization is supported) or EL1.
     */

    PUTC    'S'

    /* Preserve DTB address. */
    mov     x21, x0

    /* Relocate ourself. */
    adr     x0, __start
    adr     x1, __rela_start
    adr     x2, __rela_end
    bl      dt_arch_relocate
    cmp     w0, #0
    b.ne    .Lerr_relocate

    PUTC    'R'

    /* Load the stack address. */
    adr     x0, .Lloader_stack_end
    mov     sp, x0

    /* Clear out the BSS section. */
    adr     x0, __bss_start
    mov     x1, #0
    adr     x2, __bss_end
    sub     x2, x2, x0
    bl      memset

    PUTC    'B'

    /* Call the entry point. */
    mov     fp, #0
    mov     x0, x21
    bl      dt_main

.Lerr_relocate:
    PUTC    'E'
    PUTC    'R'

.Lerr:
    b       .Lerr
FUNCTION_END(loader_entry)

.section .bss, "aw", @nobits

/** Stack. */
SYMBOL_ALIGNED(loader_stack, PAGE_SIZE)
    .fill   PAGE_SIZE
.Lloader_stack_end:
