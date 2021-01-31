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
 * @brief               x86 bit operations.
 */

#ifndef __ARCH_BITOPS_H
#define __ARCH_BITOPS_H

#include <types.h>

/** Find first set bit in a native-sized value.
 * @param value         Value to test.
 * @return              Position of first set bit plus 1, or 0 if value is 0. */
static inline unsigned long ffs(unsigned long value) {
    if (!value)
        return 0;

    __asm__ ("bsf %1, %0" : "=r" (value) : "rm" (value) : "cc");
    return value + 1;
}

/** Find last set bit in a native-sized value.
 * @param value     Value to test.
 * @return          Position of last set bit plus 1, or 0 if value is 0. */
static inline unsigned long fls(unsigned long value) {
    if (!value)
        return 0;

    __asm__ ("bsr %1, %0" : "=r" (value) : "rm" (value) : "cc");
    return value + 1;
}

#endif /* __ARCH_BITOPS_H */
