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
 * @brief               ARM64 KBoot kernel entry functions.
 */

#include <arch/page.h>

#include <arm64/asm.h>
#include <arm64/cpu.h>
#include <arm64/mmu.h>

#include <platform/loader.h>

#include <kboot.h>

.section .text, "ax", @progbits

/** Offsets in entry_args_t. */
#define ENTRY_ARGS_TRAMPOLINE_TTL0_HI   0
#define ENTRY_ARGS_TRAMPOLINE_TTL0_LO   8
#define ENTRY_ARGS_TRAMPOLINE_VIRT      16
#define ENTRY_ARGS_KERNEL_TTL0_HI       24
#define ENTRY_ARGS_KERNEL_TTL0_LO       32
#define ENTRY_ARGS_SP                   40
#define ENTRY_ARGS_ENTRY                48
#define ENTRY_ARGS_TAGS                 56
#define ENTRY_ARGS_TRAMPOLINE           64

/** 64-bit kernel trampoline. */
FUNCTION_START(kboot_trampoline_64)
    /* Switch to the real kernel translation tables. */
    ldr     x9, [x0, #ENTRY_ARGS_KERNEL_TTL0_LO]
    ldr     x10, [x0, #ENTRY_ARGS_KERNEL_TTL0_HI]
    msr     ttbr0_el1, x9
    msr     ttbr1_el1, x10
    isb

    /* Invalidate the TLB again to flush out the trampoline address space. */
    dsb     sy
    tlbi    vmalle1
    dsb     sy
    isb

    /* Invalidate instruction cache. */
    ic      ialluis

    /* Set up stack/frame pointer. */
    ldr     x9, [x0, #ENTRY_ARGS_SP]
    mov     sp, x9
    mov     fp, #0

    /* Retrieve tag list address and entry point. */
    ldr     x1, [x0, #ENTRY_ARGS_TAGS]
    ldr     x9, [x0, #ENTRY_ARGS_ENTRY]

    /* Call the kernel. */
    ldr     x0, =KBOOT_MAGIC
    blr     x9
    b       .
.pool
SYMBOL(kboot_trampoline_64_size)
    .long   . - kboot_trampoline_64
FUNCTION_END(kboot_trampoline_64)

/** Enter a 64-bit KBoot kernel.
 * @param x0            Entry arguments structure. */
FUNCTION_START(kboot_arch_enter_64)
    /*
     * Initialize TCR:
     *  - 48-bit virtual address.
     *  - 48-bit intermediate physical address.
     *  - Write-back/write-allocate, inner shareable translation tables.
     *  - 4KB granule.
     */
    ldr     x9, =((16 << ARM64_TCR_T0SZ_SHIFT) | \
                  ARM64_TCR_IRGN0_WB_WA | \
                  ARM64_TCR_ORGN0_WB_WA | \
                  ARM64_TCR_SH0_INNER | \
                  ARM64_TCR_TG0_4 | \
                  (16 << ARM64_TCR_T1SZ_SHIFT) | \
                  ARM64_TCR_IRGN1_WB_WA | \
                  ARM64_TCR_ORGN1_WB_WA | \
                  ARM64_TCR_SH1_INNER | \
                  ARM64_TCR_TG1_4 | \
                  ARM64_TCR_IPS_48 | \
                  ARM64_TCR_TBI0 | \
                  ARM64_TCR_TBI1)
    msr     tcr_el1, x9
    isb

    /* Set TTBR0/1 to point to the trampoline translation tables. */
    ldr     x9, [x0, #ENTRY_ARGS_TRAMPOLINE_TTL0_LO]
    ldr     x10, [x0, #ENTRY_ARGS_TRAMPOLINE_TTL0_HI]
    msr     ttbr0_el1, x9
    msr     ttbr1_el1, x10
    isb

    /* Invalidate TLBs. */
    dsb     sy
    tlbi    vmalle1
    dsb     sy
    isb

    /* Save the virtual address for the trampoline. */
    ldr     x0, [x0, #ENTRY_ARGS_TRAMPOLINE_VIRT]

    /*
     * Initialize SCTLR to enable the MMU.
     *  - MMU on.
     *  - Data and instruction caches enabled.
     *  - Alignment checking disabled.
     */
    ldr     x9, =(ARM64_SCTLR_M | \
                  ARM64_SCTLR_C | \
                  ARM64_SCTLR_I | \
                  ARM64_SCTLR_EL1_RES1)
    msr     sctlr_el1, x9
    isb

    /* Jump to the trampoline. */
    add     x9, x0, #ENTRY_ARGS_TRAMPOLINE
    br      x9
FUNCTION_END(kboot_arch_enter_64)
