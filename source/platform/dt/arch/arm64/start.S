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

#include <arm64/asm.h>

/* Define to a platform-specific UART TX register address for some debug output. */
//#define UART_TX         0x09000000    /* QEMU virt machine */
//#define UART_TX         0x3f215040    /* Raspberry Pi 3B. */

.macro PUTC c
#ifdef UART_TX
    ldr     x29, =UART_TX
    ldr     x30, =\c
    str     x30, [x29]
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

FUNCTION_START(loader_entry)
    PUTC    'K'
    PUTC    'B'
    PUTC    'o'
    PUTC    'o'
    PUTC    't'

1:  b       1b

FUNCTION_END(loader_entry)
