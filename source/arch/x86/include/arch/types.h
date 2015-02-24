/*
 * Copyright (C) 2009-2015 Alex Smith
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
 * @brief               x86 type definitions.
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

/** Format character definitions for printf(). */
#ifdef CONFIG_64BIT
#   define PRIxPHYS    "lx"         /**< Format for phys_ptr_t (hexadecimal). */
#   define PRIuPHYS    "lu"         /**< Format for phys_ptr_t. */
#else
#   define PRIxPHYS    "llx"        /**< Format for phys_ptr_t (hexadecimal). */
#   define PRIuPHYS    "llu"        /**< Format for phys_ptr_t. */
#endif

/** Integer type that can represent a pointer in the loader's address space. */
typedef unsigned long ptr_t;

/**
 * Integer pointer/size types for use by OS loaders.
 *
 * On x86 we can load both 32-bit and 64-bit kernels, so these are defined to be
 * 64-bit to account for this.
*/
typedef uint64_t load_ptr_t;
typedef uint64_t load_size_t;

/** Integer types that can represent a physical address/size. */
typedef uint64_t phys_ptr_t;
typedef uint64_t phys_size_t;

#endif /* __ARCH_TYPES_H */
