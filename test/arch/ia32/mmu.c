/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IA32 MMU code.
 */

#include <x86/mmu.h>

#include <lib/string.h>

#include <test.h>

/** Address of recursive mapping. */
static uint32_t *recursive_mapping;

/** Map physical memory.
 * @param virt          Virtual address to map at.
 * @param phys          Physical address to map to.
 * @param size          Size of range to map. */
void mmu_map(ptr_t virt, phys_ptr_t phys, size_t size) {
    ptr_t rmap_addr = (ptr_t)recursive_mapping;

    assert(!(size % PAGE_SIZE));

    while (size) {
        unsigned pde, pte;

        pde = (rmap_addr / PAGE_SIZE) + (virt / X86_PTBL_RANGE_32);
        pte = virt / PAGE_SIZE;

        if (!(recursive_mapping[pde] & X86_PTE_PRESENT)) {
            phys_ptr_t phys = phys_alloc(PAGE_SIZE);
            recursive_mapping[pde] = phys | X86_PTE_PRESENT | X86_PTE_WRITE;
            memset(&recursive_mapping[pte & ~1023], 0, PAGE_SIZE);
        }

        recursive_mapping[pte] = phys | X86_PTE_PRESENT | X86_PTE_WRITE;

        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

/** Initialize the MMU code.
 * @param tags          Tag list. */
void mmu_init(kboot_tag_t *tags) {
    while (tags->type != KBOOT_TAG_NONE) {
        if (tags->type == KBOOT_TAG_PAGETABLES) {
            kboot_tag_pagetables_t *tag = (kboot_tag_pagetables_t *)tags;

            recursive_mapping = (uint32_t *)((ptr_t)tag->mapping);
            return;
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }

    internal_error("No pagetables tag found");
}
