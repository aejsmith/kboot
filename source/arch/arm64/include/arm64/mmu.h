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
 * @brief               ARM64 MMU definitions.
 */

#ifndef __ARM64_MMU_H
#define __ARM64_MMU_H

#include <loader.h>

/** Definitions of paging structure bits. */
#define ARM64_TTE_PRESENT               (1<<0)  /**< Entry is present. */
#define ARM64_TTE_TABLE                 (1<<1)  /**< Entry is a table. */
#define ARM64_TTE_PAGE                  (1<<1)  /**< Entry is a page. */
#define ARM64_TTE_AF                    (1<<10) /**< Entry has been accessed. */
#define ARM64_TTE_AP_P_RW_U_NA          (0<<6)  /**< Protected RW, user not accessible. */
#define ARM64_TTE_AP_P_RW_U_RW          (1<<6)  /**< Protected RW, user RW. */
#define ARM64_TTE_AP_P_RO_U_NA          (2<<6)  /**< Protected RO, user not accessible. */
#define ARM64_TTE_AP_P_RO_U_RO          (3<<6)  /**< Protected RO, user RO. */
#define ARM64_TTE_AP_MASK               (3<<6)
#define ARM64_TTE_SH_NON_SHAREABLE      (0<<8)
#define ARM64_TTE_SH_OUTER_SHAREABLE    (2<<8)
#define ARM64_TTE_SH_INNER_SHAREABLE    (3<<8)
#define ARM64_TTE_SH_MASK               (3<<8)
#define ARM64_TTE_ATTR_INDEX(value)     ((value)<<2)
#define ARM64_TTE_ATTR_INDEX_MASK       0x000000000000001Cull

/** Masks to get physical address from a page table entry. */
#define ARM64_TTE_ADDR_MASK             0x00007ffffffff000ull

/** Ranges covered by paging structures. */
#define ARM64_TTL1_RANGE                0x8000000000ull
#define ARM64_TTL2_RANGE                0x40000000
#define ARM64_TTL3_RANGE                0x200000

/** MAIR attribute indices. */
#define ARM64_MAIR_NORMAL_WB            0
#define ARM64_MAIR_NORMAL_WT            1
#define ARM64_MAIR_DEVICE               2

/** MAIR value. */
#define ARM64_MAIR                      0x00aaff

/** ARM64 MMU context structure. */
struct mmu_context {
    phys_ptr_t ttl0_lo;             /**< TTL0 for lower half. */
    phys_ptr_t ttl0_hi;             /**< TTL0 for upper half. */
    unsigned phys_type;             /**< Physical memory type for page tables. */
};

/** Check whether an address is a kernel (upper) address. */
static inline bool is_kernel_addr(uint64_t addr) {
    /* We currently only support a 48-bit address space. */
    return addr >= 0xffff000000000000ull;
}

/** Check whether an address range is a kernel (upper) range. */
static inline bool is_kernel_range(uint64_t start, uint64_t size) {
    uint64_t end = start + size - 1;

    return end >= start && is_kernel_addr(start) && is_kernel_addr(end);
}

/** Check whether an address is valid. */
static inline bool is_valid_addr(uint64_t addr) {
    return addr < 0x1000000000000ull || addr >= 0xffff000000000000ull;
}

/** Check whether an address range is valid. */
static inline bool is_valid_range(uint64_t start, uint64_t size) {
    uint64_t end = start + size - 1;

    return end >= start && is_valid_addr(start) && is_valid_addr(end);
}

#endif /* __ARM64_MMU_H */
