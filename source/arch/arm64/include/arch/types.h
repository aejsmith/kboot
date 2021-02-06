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
 * @brief               ARM64 type definitions.
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

/** Format character definitions for printf(). */
#define PRIxPHYS    "lx"            /**< Format for phys_ptr_t (hexadecimal). */
#define PRIuPHYS    "lu"            /**< Format for phys_ptr_t. */
#define PRIxLOAD    "lx"            /**< Format for load_ptr_t (hexadecimal). */
#define PRIuLOAD    "lu"            /**< Format for load_ptr_t. */

/** Integer type that can represent a pointer in the loader's address space. */
typedef uint64_t ptr_t;

/** Integer pointer/size types for use by OS loaders. */
typedef uint64_t load_ptr_t;
typedef uint64_t load_size_t;

/** Integer types that can represent a physical address/size. */
typedef uint64_t phys_ptr_t;
typedef uint64_t phys_size_t;

#endif /* __ARCH_TYPES_H */
