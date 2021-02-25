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
 * @brief               Memory management functions.
 */

#ifndef __MEMORY_H
#define __MEMORY_H

#include <arch/page.h>

#include <lib/list.h>

/** Physical memory range descriptor. */
typedef struct memory_range {
    list_t header;                      /**< Link to memory range list. */

    phys_ptr_t start;                   /**< Start of range. */
    phys_size_t size;                   /**< Size of range. */
    uint8_t type;                       /**< Type of the range. */
} memory_range_t;

/**
 * Memory type definitions.
 *
 * Memory types to be used with the memory allocation functions. These match
 * the types specified by the KBoot spec, with some additions.
 */
#define MEMORY_TYPE_FREE        0       /**< Free, usable memory. */
#define MEMORY_TYPE_ALLOCATED   1       /**< Kernel image and other non-reclaimable data. */
#define MEMORY_TYPE_RECLAIMABLE 2       /**< Memory reclaimable when boot information is no longer needed. */
#define MEMORY_TYPE_PAGETABLES  3       /**< Temporary page tables for the kernel. */
#define MEMORY_TYPE_STACK       4       /**< Stack set up for the kernel. */
#define MEMORY_TYPE_MODULES     5       /**< Module data. */
#define MEMORY_TYPE_INTERNAL    6       /**< Freed before the OS is entered. */

/** Memory allocation behaviour flags. */
#define MEMORY_ALLOC_HIGH       (1<<0)  /**< Allocate highest possible address. */
#define MEMORY_ALLOC_CAN_FAIL   (1<<1)  /**< Allocation is allowed to fail. */

extern void *malloc(size_t size);
extern void *realloc(void *addr, size_t size);
extern void free(void *addr);

extern void *malloc_large(size_t size);
extern void free_large(void *addr);

/** Helper for __cleanup_free. */
static inline void freep(void *p) {
    free(*(void **)p);
}

/** Helper for __cleanup_free_large. */
static inline void free_largep(void *p) {
    free_large(*(void **)p);
}

/** Attribute to free a pointer (with free) when it goes out of scope. */
#define __cleanup_free          __cleanup(freep)

/** Attribute to free a pointer (with free_large) when it goes out of scope. */
#define __cleanup_free_large    __cleanup(free_largep)

extern void memory_map_insert(list_t *map, phys_ptr_t start, phys_size_t size, uint8_t type);
extern void memory_map_remove(list_t *map, phys_ptr_t start, phys_size_t size);
extern void memory_map_dump(list_t *map);
extern void memory_map_free(list_t *map);

extern void *memory_alloc(
    phys_size_t size, phys_size_t align, phys_ptr_t min_addr, phys_ptr_t max_addr,
    uint8_t type, unsigned flags, phys_ptr_t *_phys);
extern void memory_free(void *addr, phys_size_t size);

extern void memory_snapshot(list_t *map);
extern void memory_finalize(list_t *map);

#ifndef TARGET_HAS_MM

extern void target_memory_probe(void);

extern void memory_add(phys_ptr_t start, phys_size_t size, uint8_t type);
extern void memory_protect(phys_ptr_t start, phys_size_t size);
extern void memory_remove(phys_ptr_t start, phys_size_t size);
extern void memory_init(void);

#else

static inline void memory_init(void) {}

#endif /* !TARGET_HAS_MM */

#endif /* __MEMORY_H */
