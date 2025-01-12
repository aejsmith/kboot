/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 backtrace function.
 */

#include <arm64/exception.h>

#include <lib/backtrace.h>

#include <loader.h>

/** Structure containing a stack frame. */
typedef struct stack_frame {
    struct stack_frame *next;       /**< Pointer to next stack frame. */
    ptr_t addr;                     /**< Function return address. */
} stack_frame_t;

/** Print out a backtrace.
 * @param func          Print function to use. */
void backtrace(printf_t func) {
    #if TARGET_RELOCATABLE
        func("Backtrace (base = %p):\n", __start);
    #else
        func("Backtrace:\n");
    #endif

    stack_frame_t *frame;
    asm volatile("mov %0, x29" : "=r"(frame));

    while (frame && frame->addr) {
        ptr_t addr = frame->addr;

        /* Subtract 4 unless this the exception address so that the address
         * points at the call site rather than after it - this can give more
         * useful backtraces for tail calls. */
        if (!arm64_exception_frame || addr != arm64_exception_frame->elr)
            addr -= 4;

        #ifdef TARGET_RELOCATABLE
            func(" %p (%p)\n", addr, addr - (ptr_t)__start);
        #else
            func(" %p\n", addr);
        #endif

        frame = frame->next;
    }
}
