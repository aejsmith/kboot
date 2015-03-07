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

#include <lib/allocator.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Structure of a region. */
typedef struct allocator_region {
    list_t header;                  /**< List header. */

    load_ptr_t start;               /**< Start of the region. */
    load_size_t size;               /**< Size of the region. */
    bool allocated;                 /**< Whether the region is allocated. */
} allocator_region_t;

/** Insert a region into the allocator.
 * @param alloc         Allocator to insert into.
 * @param start         Start of the region.
 * @param size          Size of the region.
 * @param allocated     Whether the region is allocated.
 * @param location      Where to insert before, or NULL to find insertion point. */
static void insert_region(
    allocator_t *alloc, load_ptr_t start, load_size_t size, bool allocated,
    allocator_region_t *location)
{
    allocator_region_t *region, *exist, *split;
    load_ptr_t end, exist_end;

    region = malloc(sizeof(*region));
    region->start = start;
    region->size = size;
    region->allocated = allocated;

    /* We need to deal with the case where start + size wraps to 0, i.e. if
     * we are allocating from the whole address space. */
    end = region->start + region->size - 1;

    if (!location) {
        /* Try to find where to insert the region in the list. */
        list_foreach(&alloc->regions, iter) {
            exist = list_entry(iter, allocator_region_t, header);

            if (region->start <= exist->start) {
                location = exist;
                break;
            }
        }
    }

    /* Insert the region. */
    list_init(&region->header);
    if (location) {
        if (region->start <= location->start) {
            list_add_before(&location->header, &region->header);
        } else {
            /* May go after if we've been passed a location in the middle of
             * the region. */
            list_add_after(&location->header, &region->header);
        }
    } else {
        /* Must go at the end of the list if there is still no location. */
        list_append(&alloc->regions, &region->header);
    }

    /* Check if the new region has overlapped part of the previous. */
    if (region != list_first(&alloc->regions, allocator_region_t, header)) {
        exist = list_prev(region, header);
        exist_end = exist->start + exist->size - 1;

        if (region->start <= exist_end) {
            if (exist_end > end) {
                /* Must split the region. */
                split = malloc(sizeof(*split));
                split->start = end + 1;
                split->size = exist_end - end;
                split->allocated = exist->allocated;

                list_init(&split->header);
                list_add_after(&region->header, &split->header);
            }

            exist->size = region->start - exist->start;
        }
    }

    /* Swallow up any following ranges that the new range overlaps. */
    list_foreach_safe(&region->header, iter) {
        if (iter == &alloc->regions)
            break;

        exist = list_entry(iter, allocator_region_t, header);
        exist_end = exist->start + exist->size - 1;

        if (exist->start > end) {
            break;
        } else if (exist_end > end) {
            /* Resize the range and finish. */
            exist->start = end + 1;
            exist->size = exist_end - end;
            break;
        } else {
            /* Completely remove the range. */
            list_remove(&exist->header);
            free(exist);
        }
    }
}

/** Allocate a region from an allocator.
 * @param alloc         Allocator to allocate from.
 * @param size          Size of the region to allocate.
 * @param align         Alignment of the region.
 * @param _addr         Where to store address of allocated region.
 * @return              Whether there was enough space for the region. */
bool allocator_alloc(allocator_t *alloc, load_size_t size, load_size_t align, load_ptr_t *_addr) {
    assert(!(size % PAGE_SIZE));
    assert(!(align % PAGE_SIZE));
    assert(size);

    if (!align)
        align = PAGE_SIZE;

    list_foreach(&alloc->regions, iter) {
        allocator_region_t *region = list_entry(iter, allocator_region_t, header);

        if (!region->allocated) {
            load_ptr_t start = round_up(region->start, align);

            if (start + size - 1 <= region->start + region->size - 1) {
                /* Create a new allocated region and insert over this space. */
                insert_region(alloc, start, size, true, region);

                *_addr = start;
                return true;
            }
        }
    }

    return false;
}

/**
 * Mark a region as allocated.
 *
 * Tries to mark a region of the address space as allocated, ensuring that no
 * other regions are already allocated within it.
 *
 * @param alloc         Allocator to insert into.
 * @param addr          Start of region to insert.
 * @param size          Size of region to insert.
 *
 * @return              Whether successfully inserted.
 */
bool allocator_insert(allocator_t *alloc, load_ptr_t addr, load_size_t size) {
    load_ptr_t region_end;

    assert(!(addr % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));
    assert(size);

    region_end = addr + size - 1;

    /* Check for conflicts with other allocated regions. */
    list_foreach(&alloc->regions, iter) {
        allocator_region_t *exist = list_entry(iter, allocator_region_t, header);
        load_ptr_t exist_end;

        if (!exist->allocated)
            continue;

        exist_end = exist->start + exist->size - 1;

        if (max(addr, exist->start) <= min(region_end, exist_end))
            return false;
    }

    allocator_reserve(alloc, addr, size);
    return true;
}

/**
 * Block a region from being allocated.
 *
 * Prevents any future allocations from returning any address within the given
 * region. The specified range will be overwritten.
 *
 * @param alloc         Allocator to reserve in.
 * @param addr          Address of region to reserve.
 * @param size          Size of region to reserve.
 */
void allocator_reserve(allocator_t *alloc, load_ptr_t addr, load_size_t size) {
    load_ptr_t region_end, alloc_end, end;

    assert(!(addr % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));
    assert(size);

    /* Trim given range to be within the allocator. */
    region_end = addr + size - 1;
    alloc_end = alloc->start + alloc->size - 1;

    addr = max(addr, alloc->start);
    end = min(region_end, alloc_end);
    if (end < addr)
        return;

    size = min(region_end, alloc_end) - addr + 1;

    insert_region(alloc, addr, size, true, NULL);
}

/** Initialize an allocator.
 * @param alloc         Allocator to initialize.
 * @param start         Start of range that the allocator manages.
 * @param size          Size of range that the allocator manages. A size of 0
 *                      can be used in conjunction with 0 start to mean the
 *                      entire address space. */
void allocator_init(allocator_t *alloc, load_ptr_t start, load_size_t size) {
    assert(!(start % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));
    assert(start + size > start || start + size == 0);

    list_init(&alloc->regions);

    alloc->start = start;
    alloc->size = size;

    /* Add a free region covering the entire space. */
    insert_region(alloc, start, size, false, NULL);
}
