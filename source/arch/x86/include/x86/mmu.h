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
 * @brief               x86 MMU definitions.
 */

#ifndef __X86_MMU_H
#define __X86_MMU_H

#include <loader.h>

/** Definitions of paging structure bits. */
#define X86_PTE_PRESENT         (1<<0)  /**< Page is present. */
#define X86_PTE_WRITE           (1<<1)  /**< Page is writable. */
#define X86_PTE_USER            (1<<2)  /**< Page is accessible in CPL3. */
#define X86_PTE_PWT             (1<<3)  /**< Page has write-through caching. */
#define X86_PTE_PCD             (1<<4)  /**< Page has caching disabled. */
#define X86_PTE_ACCESSED        (1<<5)  /**< Page has been accessed. */
#define X86_PTE_DIRTY           (1<<6)  /**< Page has been written to. */
#define X86_PTE_LARGE           (1<<7)  /**< Page is a large page. */
#define X86_PTE_GLOBAL          (1<<8)  /**< Page won't be cleared in TLB. */

/** Masks to get physical address from a page table entry. */
#define X86_PTE_ADDR_MASK_64    0x000000fffffff000ull
#define X86_PTE_ADDR_MASK_32    0xfffff000

/** Ranges covered by paging structures. */
#define X86_PDPT_RANGE_64       0x8000000000ull
#define X86_PDIR_RANGE_64       0x40000000
#define X86_PTBL_RANGE_64       0x200000
#define X86_PTBL_RANGE_32       0x400000

/** x86 MMU context structure. */
struct mmu_context {
    uint32_t cr3;                   /**< Value loaded into CR3. */
    load_mode_t mode;               /**< Load mode for the context. */
    unsigned phys_type;             /**< Physical memory type for page tables. */
};

/** Check whether an address is canonical.
 * @param addr          Address to check.
 * @return              Result of check. */
static inline bool is_canonical_addr(uint64_t addr) {
    return ((uint64_t)((int64_t)addr >> 47) + 1) <= 1;
}

/** Check whether an address range is canonical.
 * @param start         Start of range to check.
 * @param size          Size of address range.
 * @return              Result of check. */
static inline bool is_canonical_range(uint64_t start, uint64_t size) {
    uint64_t end = start + size - 1;

    return is_canonical_addr(start)
        && is_canonical_addr(end)
        && (start & (1ull << 47)) == (end & (1ull << 47));
}

#endif /* __X86_MMU_H */
