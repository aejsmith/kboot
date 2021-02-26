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

#ifndef __ARM64_EXCEPTION_H
#define __ARM64_EXCEPTION_H

#include <types.h>

/** Structure defining an exception stack frame. */
typedef struct exception_frame {
    unsigned long elr;
    unsigned long spsr;
    unsigned long x30;
    unsigned long sp;
    unsigned long x28;
    unsigned long x29;
    unsigned long x26;
    unsigned long x27;
    unsigned long x24;
    unsigned long x25;
    unsigned long x22;
    unsigned long x23;
    unsigned long x20;
    unsigned long x21;
    unsigned long x18;
    unsigned long x19;
    unsigned long x16;
    unsigned long x17;
    unsigned long x14;
    unsigned long x15;
    unsigned long x12;
    unsigned long x13;
    unsigned long x10;
    unsigned long x11;
    unsigned long x8;
    unsigned long x9;
    unsigned long x6;
    unsigned long x7;
    unsigned long x4;
    unsigned long x5;
    unsigned long x2;
    unsigned long x3;
    unsigned long x0;
    unsigned long x1;
} exception_frame_t;

extern void arm64_sync_exception_handler(exception_frame_t *frame) __noreturn;
extern void arm64_exception_init(void);

#endif /* __ARM64_EXCEPTION_H */
