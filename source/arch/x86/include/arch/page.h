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
#define PAGE_WIDTH      12              /**< Width of a page in bits. */
#define PAGE_SIZE       0x1000          /**< Size of a page (4KB). */

/** Mask to clear page offset and unsupported bits from a virtual address. */
#ifdef CONFIG_64BIT
#   define PAGE_MASK    0x000ffffffffff000l
#else
#   define PAGE_MASK    0xfffff000l
#endif

/** Mask to clear page offset and unsupported bits from a physical address. */
#define PHYS_PAGE_MASK  0x000000fffffff000l

#endif /* __ARCH_PAGE_H */
