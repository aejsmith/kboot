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
 * @brief               ARM64 MMU definitions.
 */

#ifndef __ARM64_MMU_H
#define __ARM64_MMU_H

#include <loader.h>

/** Check whether an address is a kernel (upper) address. */
static inline bool is_kernel_addr(uint64_t addr) {
    /* We currently only support a 48-bit address space. */
    return addr >= 0xffff000000000000;
}

/** Check whether an address range is a kernel (upper) range. */
static inline bool is_kernel_range(uint64_t start, uint64_t size) {
    uint64_t end = start + size - 1;

    return end >= start && is_kernel_addr(start) && is_kernel_addr(end);
}

#endif /* __ARM64_MMU_H */
