/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               Memory management.
 */

#include "test.h"

KBOOT_LOAD(0, 0, 0, VIRT_MAP_BASE, VIRT_MAP_SIZE);

#ifdef PHYS_MAP_BASE
KBOOT_MAPPING(PHYS_MAP_BASE, 0, PHYS_MAP_SIZE);
#endif

/** Physical memory allocation range. */
static phys_ptr_t phys_next;
static phys_size_t phys_size;

/** Virtual memory allocation range. */
static ptr_t virt_next;
static size_t virt_size;

/** Map physical memory.
 * @param addr          Physical address to map.
 * @param size          Size of range to map.
 * @return              Pointer to virtual mapping. */
void *phys_map(phys_ptr_t addr, size_t size) {
    assert(!(addr % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));

    #ifdef PHYS_MAP_BASE
        assert(addr + size - 1 <= PHYS_MAP_BASE + PHYS_MAP_SIZE - 1);
        return (void *)(addr + PHYS_MAP_BASE);
    #else
        ptr_t virt = virt_alloc(size);
        mmu_map(virt, addr, size);
        return (void *)virt;
    #endif
}

/** Allocate physical memory.
 * @param size          Size of range to allocate. */
phys_ptr_t phys_alloc(phys_size_t size) {
    phys_ptr_t ret;

    assert(!(size % PAGE_SIZE));

    if (size > phys_size)
        internal_error("Exhausted physical memory");

    ret = phys_next;
    phys_next += size;
    phys_size -= size;
    return ret;
}

/** Initialize the physical memory manager.
 * @param tags          Tag list. */
static void phys_init(kboot_tag_t *tags) {
    /* Look for the largest accessible memory range. */
    while (tags->type != KBOOT_TAG_NONE) {
        if (tags->type == KBOOT_TAG_MEMORY) {
            kboot_tag_memory_t *tag = (kboot_tag_memory_t *)tags;
            kboot_paddr_t end;

            end = tag->start + tag->size - 1;

            if (end <= PHYS_MAX && tag->size >= phys_size) {
                phys_next = tag->start;
                phys_size = tag->size;
            }
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }

    if (!phys_size)
        internal_error("No usable physical memory range found");

    printf("phys_next = 0x%" PRIxPHYS ", phys_size = 0x%" PRIxPHYS "\n", phys_next, phys_size);
}

/** Allocate virtual address space.
 * @param size          Size of range to allocate. */
ptr_t virt_alloc(size_t size) {
    ptr_t ret;

    assert(!(size % PAGE_SIZE));

    if (size > virt_size)
        internal_error("Exhausted virtual address space");

    ret = virt_next;
    virt_next += size;
    virt_size -= size;
    return ret;
}

/** Initialize the virtual memory manager.
 * @param tags          Tag list. */
static void virt_init(kboot_tag_t *tags) {
    /* Move the range after any KBoot allocations. */
    virt_next = VIRT_MAP_BASE;
    while (tags->type != KBOOT_TAG_NONE) {
        if (tags->type == KBOOT_TAG_VMEM) {
            kboot_tag_vmem_t *tag = (kboot_tag_vmem_t *)tags;
            ptr_t end;

            end = tag->start + tag->size;

            if (tag->start >= VIRT_MAP_BASE && end - 1 <= VIRT_MAP_BASE + VIRT_MAP_SIZE - 1) {
                if (tag->start != virt_next)
                    internal_error("Virtual ranges are non-contiguous");

                virt_next = end;
            }
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }

    virt_size = VIRT_MAP_SIZE - (virt_next - VIRT_MAP_BASE);

    if (!virt_next || !virt_size)
        internal_error("No usable virtual memory range found");

    printf("virt_next = %p, virt_size = 0x%zx\n", virt_next, virt_size);
}

/** Initialize the memory manager.
 * @param tags          Tag list. */
void mm_init(kboot_tag_t *tags) {
    phys_init(tags);
    virt_init(tags);
    mmu_init(tags);
}
