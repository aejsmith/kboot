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
 * @brief               ARM64 exception entry functions.
 */

#include <arm64/asm.h>
#include <arm64/cpu.h>

.section .text, "ax", @progbits

/*
 * The loader is running in either EL1 or EL2, SP = 1.
 * We must provide the following vectors:
 *   ELct   - Current EL using EL0 SP.
 *   ELch   - Current EL using current EL SP.
 *   ELl_64 - Lower EL in AArch64.
 *   ELl_32 - Lower EL in AArch32.
 * We only actually care about ELch.
 */

.p2align 12
SYMBOL(arm64_exception_vectors)
sync_ELct_handler:
    b       .

.p2align 7
irq_ELct_handler:
    b       .

.p2align 7
fiq_ELct_handler:
    b       .

.p2align 7
serror_ELct_handler:
    b       .

.p2align 7
sync_ELch_handler:
    stp     x0, x1, [sp,#-16]!
    stp     x2, x3, [sp,#-16]!
    stp     x4, x5, [sp,#-16]!
    stp     x6, x7, [sp,#-16]!
    stp     x8, x9, [sp,#-16]!
    stp     x10, x11, [sp,#-16]!
    stp     x12, x13, [sp,#-16]!
    stp     x14, x15, [sp,#-16]!
    stp     x16, x17, [sp,#-16]!
    stp     x18, x19, [sp,#-16]!
    stp     x20, x21, [sp,#-16]!
    stp     x22, x23, [sp,#-16]!
    stp     x24, x25, [sp,#-16]!
    stp     x26, x27, [sp,#-16]!
    stp     x28, x29, [sp,#-16]!
    add     x0, sp, #(30 * 8)
    stp     x30, x0, [sp,#-16]!

    mrs     x0, currentel
    cmp     x0, #ARM64_CURRENTEL_EL2
    b.eq    .Lel2

.Lel1:
    mrs     x0, spsr_el1
    mrs     x1, elr_el1
    b       .Lhandle

.Lel2:
    mrs     x0, spsr_el2
    mrs     x1, elr_el2
    b       .Lhandle

.Lhandle:
    stp     x1, x0, [sp,#-16]!
    mov     x0, sp

    /* Stack frame with ELR as return address. */
    stp     x29, x1, [sp, #-16]!
    mov     x29, sp
    bl      arm64_sync_exception_handler

.p2align 7
irq_ELch_handler:
    b       .

.p2align 7
fiq_ELch_handler:
    b       .

.p2align 7
serror_ELch_handler:
    b       .

.p2align 7
sync_ELl_64_handler:
    b       .

.p2align 7
irq_ELl_64_handler:
    b       .

.p2align 7
fiq_ELl_64_handler:
    b       .

.p2align 7
serror_ELl_64_handler:
    b       .

.p2align 7
sync_ELl_32_handler:
    b       .

.p2align 7
irq_ELl_32_handler:
    b       .

.p2align 7
fiq_ELl_32_handler:
    b       .

.p2align 7
serror_ELl_32_handler:
    b       .

