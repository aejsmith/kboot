/*
 * Copyright (C) 2011-2014 Alex Smith
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
 * @brief               x86 paging definitions.
 */

#ifndef __ARCH_PAGE_H
#define __ARCH_PAGE_H

/** Page size definitions. */
#define PAGE_WIDTH          12              /**< Width of a page in bits. */
#define PAGE_SIZE           0x1000          /**< Size of a page. */
#define LARGE_PAGE_SIZE_32  0x400000        /**< 32-bit large page size. */
#define LARGE_PAGE_SIZE_64  0x200000        /**< 64-bit large page size. */

/** Masks to clear page offset and unsupported bits from a virtual address. */
#define PAGE_MASK_32        0xfffff000ul
#define PAGE_MASK_64        0x000ffffffffff000ull

/** Native paging definitions. */
#ifdef __LP64__
#   define LARGE_PAGE_SIZE  LARGE_PAGE_SIZE_64
#   define PAGE_MASK        PAGE_MASK_64
#else
#   define LARGE_PAGE_SIZE  LARGE_PAGE_SIZE_32
#   define PAGE_MASK        PAGE_MASK_32
#endif

/** Mask to clear page offset and unsupported bits from a physical address. */
#define PHYS_PAGE_MASK_64   0x000000fffffff000ull
#define PHYS_PAGE_MASK_32   0xfffff000

#endif /* __ARCH_PAGE_H */
