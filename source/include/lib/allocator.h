/*
 * Copyright (C) 2012-2015 Alex Smith
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
 * @brief               Virtual memory region allocator.
 */

#ifndef __LIB_ALLOCATOR_H
#define __LIB_ALLOCATOR_H

#include <lib/list.h>

#include <loader.h>

/** Structure containing a virtual region allocator. */
typedef struct allocator {
    load_ptr_t start;               /**< Start of the region that the allocator manages. */
    load_size_t size;               /**< Size of the region that the allocator manages. */
    list_t regions;                 /**< List of regions. */
} allocator_t;

extern bool allocator_alloc(allocator_t *alloc, load_size_t size, load_size_t align, load_ptr_t *_addr);
extern bool allocator_insert(allocator_t *alloc, load_ptr_t addr, load_size_t size);
extern void allocator_reserve(allocator_t *alloc, load_ptr_t addr, load_size_t size);

extern void allocator_init(allocator_t *alloc, load_ptr_t start, load_size_t size);

#endif /* __LIB_ALLOCATOR_H */
