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
 * @brief               x86 exception definitions.
 */

#ifndef __X86_EXCEPTION_H
#define __X86_EXCEPTION_H

/** Definitions for hardware exception numbers. */
#define X86_EXCEPTION_DE    0       /**< Divide Error. */
#define X86_EXCEPTION_DB    1       /**< Debug. */
#define X86_EXCEPTION_NMI   2       /**< Non-Maskable Interrupt. */
#define X86_EXCEPTION_BP    3       /**< Breakpoint. */
#define X86_EXCEPTION_OF    4       /**< Overflow. */
#define X86_EXCEPTION_BR    5       /**< BOUND Range Exceeded. */
#define X86_EXCEPTION_UD    6       /**< Invalid Opcode. */
#define X86_EXCEPTION_NM    7       /**< Device Not Available. */
#define X86_EXCEPTION_DF    8       /**< Double Fault. */
#define X86_EXCEPTION_TS    10      /**< Invalid TSS. */
#define X86_EXCEPTION_NP    11      /**< Segment Not Present. */
#define X86_EXCEPTION_SS    12      /**< Stack Fault. */
#define X86_EXCEPTION_GP    13      /**< General Protection Fault. */
#define X86_EXCEPTION_PF    14      /**< Page Fault. */
#define X86_EXCEPTION_MF    16      /**< x87 FPU Floating-Point Error. */
#define X86_EXCEPTION_AC    17      /**< Alignment Check. */
#define X86_EXCEPTION_MC    18      /**< Machine Check. */
#define X86_EXCEPTION_XM    19      /**< SIMD Floating-Point. */

#ifndef __ASM__

#include <types.h>

#ifdef __LP64__

/** Structure defining an exception stack frame. */
typedef struct exception_frame {
    unsigned long r15;              /**< R15. */
    unsigned long r14;              /**< R14. */
    unsigned long r13;              /**< R13. */
    unsigned long r12;              /**< R12. */
    unsigned long r11;              /**< R11. */
    unsigned long r10;              /**< R10. */
    unsigned long r9;               /**< R9. */
    unsigned long r8;               /**< R8. */
    unsigned long bp;               /**< RBP. */
    unsigned long si;               /**< RSI. */
    unsigned long di;               /**< RDI. */
    unsigned long dx;               /**< RDX. */
    unsigned long cx;               /**< RCX. */
    unsigned long bx;               /**< RBX. */
    unsigned long ax;               /**< RAX. */
    unsigned long num;              /**< Interrupt number. */
    unsigned long err_code;         /**< Error code (if applicable). */
    unsigned long ip;               /**< IP. */
    unsigned long cs;               /**< CS. */
    unsigned long flags;            /**< FLAGS. */
    unsigned long sp;               /**< SP. */
    unsigned long ss;               /**< SS. */
} __packed exception_frame_t;

#else /* __LP64__ */

/** Structure defining an interrupt stack frame. */
typedef struct exception_frame {
    unsigned long gs;               /**< GS. */
    unsigned long fs;               /**< FS. */
    unsigned long es;               /**< ES. */
    unsigned long ds;               /**< DS. */
    unsigned long di;               /**< EDI. */
    unsigned long si;               /**< ESI. */
    unsigned long bp;               /**< EBP. */
    unsigned long sp;               /**< ESP (kernel). */
    unsigned long bx;               /**< EBX. */
    unsigned long dx;               /**< EDX. */
    unsigned long cx;               /**< ECX. */
    unsigned long ax;               /**< EAX. */
    unsigned long num;              /**< Interrupt number. */
    unsigned long err_code;         /**< Error code (if applicable). */
    unsigned long ip;               /**< IP. */
    unsigned long cs;               /**< CS. */
    unsigned long flags;            /**< FLAGS. */
    unsigned long usp;              /**< ESP (user). */
    unsigned long ss;               /**< SS. */
} __packed exception_frame_t;

#endif /* __LP64__ */

extern void x86_exception_handler(exception_frame_t *frame) __noreturn;

#endif /* __ASM__ */
#endif /* __X86_EXCEPTION_H */
