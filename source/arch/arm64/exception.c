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
 * @brief               ARM64 exception handling.
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>

#include <loader.h>

extern uint8_t arm64_exception_vectors[];

/** Synchronous exception handler. */
void arm64_sync_exception_handler(exception_frame_t *frame) {
    internal_error("Unhandled synchronous exception\n"
        "x0:   0x%016lx  x1:  0x%016lx  x2:  0x%016lx\n"
        "x3:   0x%016lx  x4:  0x%016lx  x5:  0x%016lx\n"
        "x6:   0x%016lx  x7:  0x%016lx  x8:  0x%016lx\n"
        "x9:   0x%016lx  x10: 0x%016lx  x11: 0x%016lx\n"
        "x12:  0x%016lx  x13: 0x%016lx  x14: 0x%016lx\n"
        "x15:  0x%016lx  x16: 0x%016lx  x17: 0x%016lx\n"
        "x18:  0x%016lx  x19: 0x%016lx  x20: 0x%016lx\n"
        "x21:  0x%016lx  x22: 0x%016lx  x23: 0x%016lx\n"
        "x24:  0x%016lx  x25: 0x%016lx  x26: 0x%016lx\n"
        "x27:  0x%016lx  x28: 0x%016lx  x29: 0x%016lx\n"
        "x30:  0x%016lx  sp:  0x%016lx  elr: 0x%016lx\n"
        "spsr: 0x%016lx",
        frame->x0,  frame->x1,  frame->x2,
        frame->x3,  frame->x4,  frame->x5,
        frame->x6,  frame->x7,  frame->x8,
        frame->x9,  frame->x10, frame->x11,
        frame->x12, frame->x13, frame->x14,
        frame->x15, frame->x16, frame->x17,
        frame->x18, frame->x19, frame->x20,
        frame->x21, frame->x22, frame->x23,
        frame->x24, frame->x25, frame->x26,
        frame->x27, frame->x28, frame->x29,
        frame->x30, frame->sp,  frame->elr,
        frame->spsr);
}

void arm64_exception_init(void) {
    /* Ensure we run exceptions with current EL SP. */
    arm64_write_sysreg(spsel, 1);

    /* Install exception vectors. */
    arm64_write_sysreg_el(vbar, (ptr_t)arm64_exception_vectors);
}
