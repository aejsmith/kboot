/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 backtrace function.
 */

#include <lib/backtrace.h>

#include <x86/cpu.h>

#include <loader.h>

/** Structure containing a stack frame. */
typedef struct stack_frame {
    struct stack_frame *next;       /**< Pointer to next stack frame. */
    ptr_t addr;                     /**< Function return address. */
} stack_frame_t;

/** Print out a backtrace.
 * @param func          Print function to use. */
void backtrace(printf_t func) {
    stack_frame_t *frame;

    #if TARGET_RELOCATABLE
        func("Backtrace (base = %p):\n", __start);
    #else
        func("Backtrace:\n");
    #endif

    frame = (stack_frame_t *)x86_read_bp();
    while (frame && frame->addr) {
        #ifdef TARGET_RELOCATABLE
            func(" %p (%p)\n", frame->addr, frame->addr - (ptr_t)__start);
        #else
            func(" %p\n", frame->addr);
        #endif

        frame = frame->next;
    }
}
