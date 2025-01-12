/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
