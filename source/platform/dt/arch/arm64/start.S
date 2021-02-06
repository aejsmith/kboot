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

/* Ensure that this code is linked in first. */
.section .init.text, "ax", @progbits

FUNCTION_START(loader_entry)
    /*
     * The very start of the image is the Linux kernel header. We begin
     * execution here so it starts with a jump over the header.
     *
     * https://www.kernel.org/doc/html/latest/arm64/booting.html
     */

    b       _start                  /* code0 */
    .long   0                       /* code1 */
    .quad   0                       /* text_offset - image load offset */
    .quad   0                       /* image_size - effective image size */
    .quad   0                       /* flags - kernel flags */
    .quad   0                       /* reserved */
    .quad   0                       /* reserved */
    .quad   0                       /* reserved */
    .long   0x644d5241              /* magic - Magic number, "ARM\x64". */
    .long   0                       /* reserved. */

_start:
    ldr     x1, =0x3f215040
    ldr     x2, ='K'
    str     x2, [x1]
    ldr     x2, ='B'
    str     x2, [x1]
    ldr     x2, ='o'
    str     x2, [x1]
    ldr     x2, ='o'
    str     x2, [x1]
    ldr     x2, ='t'
    str     x2, [x1]


1:  b       1b

FUNCTION_END(loader_entry)
