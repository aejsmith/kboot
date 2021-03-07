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
 * @brief               ARM64 MMU functions.
 */

#include <arch/page.h>

#include <arm64/mmu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <mmu.h>

/** Allocate a paging structure. */
static phys_ptr_t allocate_structure(mmu_context_t *ctx) {
    phys_ptr_t phys;
    void *virt;

    /* Allocate high to try to avoid any fixed kernel load location. */
    virt = memory_alloc(PAGE_SIZE, PAGE_SIZE, 0, 0, ctx->phys_type, MEMORY_ALLOC_HIGH, &phys);
    memset(virt, 0, PAGE_SIZE);
    return phys;
}

static uint64_t *get_ttl2(mmu_context_t *ctx, uint64_t virt, bool alloc) {
    uint64_t *ttl0 = (virt & (1ull << 63))
        ? (uint64_t *)phys_to_virt(ctx->ttl0_hi)
        : (uint64_t *)phys_to_virt(ctx->ttl0_lo);

    unsigned ttl0e = (virt / ARM64_TTL1_RANGE) % 512;
    if (!(ttl0[ttl0e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = allocate_structure(ctx);
        ttl0[ttl0e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    uint64_t *ttl1 = (uint64_t *)phys_to_virt((ptr_t)(ttl0[ttl0e] & ARM64_TTE_ADDR_MASK));

    unsigned ttl1e = (virt % ARM64_TTL1_RANGE) / ARM64_TTL2_RANGE;
    if (!(ttl1[ttl1e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = allocate_structure(ctx);
        ttl1[ttl1e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    return (uint64_t *)phys_to_virt((ptr_t)(ttl1[ttl1e] & ARM64_TTE_ADDR_MASK));
}

static void map_large(mmu_context_t *ctx, uint64_t virt, uint64_t phys, uint64_t tte_flags) {
    assert(!(virt % LARGE_PAGE_SIZE));
    assert(!(phys % LARGE_PAGE_SIZE));

    uint64_t *ttl2 = get_ttl2(ctx, virt, true);

    unsigned ttl2e = (virt % ARM64_TTL2_RANGE) / ARM64_TTL3_RANGE;
    ttl2[ttl2e] = phys | tte_flags;
}

static void map_small(mmu_context_t *ctx, uint64_t virt, uint64_t phys, uint64_t tte_flags) {
    assert(!(virt % PAGE_SIZE));
    assert(!(phys % PAGE_SIZE));

    uint64_t *ttl2 = get_ttl2(ctx, virt, true);

    unsigned ttl2e = (virt % ARM64_TTL2_RANGE) / ARM64_TTL3_RANGE;
    if (!(ttl2[ttl2e] & ARM64_TTE_PRESENT)) {
        phys_ptr_t addr = allocate_structure(ctx);
        ttl2[ttl2e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    assert(ttl2[ttl2e] & ARM64_TTE_TABLE);

    uint64_t *ttl3 = (uint64_t *)phys_to_virt((ptr_t)(ttl2[ttl2e] & ARM64_TTE_ADDR_MASK));

    unsigned ttl3e = (virt % ARM64_TTL3_RANGE) / PAGE_SIZE;
    ttl3[ttl3e] = phys | ARM64_TTE_PAGE | tte_flags;
}

/** Create a mapping in an MMU context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param size          Size of the mapping to create.
 * @param flags         Mapping flags.
 * @return              Whether the supplied addresses were valid. */
bool mmu_map(mmu_context_t *ctx, load_ptr_t virt, phys_ptr_t phys, load_size_t size, uint32_t flags) {
    assert(!(virt % PAGE_SIZE));
    assert(!(phys % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));

    if (!is_valid_range(virt, size))
        return false;

    uint32_t cache_flag = flags & MMU_MAP_CACHE_MASK;

    uint32_t mair_index;
    switch (cache_flag) {
    case MMU_MAP_CACHE_UC:
        mair_index = ARM64_MAIR_DEVICE;
        break;
    case MMU_MAP_CACHE_WT:
        mair_index = ARM64_MAIR_NORMAL_WT;
        break;
    default:
        mair_index = ARM64_MAIR_NORMAL_WB;
        break;
    }

    uint64_t tte_flags =
        ARM64_TTE_PRESENT |
        ARM64_TTE_AF |
        ((flags & MMU_MAP_RO) ? ARM64_TTE_AP_P_RO_U_NA : ARM64_TTE_AP_P_RW_U_NA) |
        ((cache_flag == MMU_MAP_CACHE_UC) ? ARM64_TTE_SH_OUTER_SHAREABLE : ARM64_TTE_SH_INNER_SHAREABLE) |
        ARM64_TTE_ATTR_INDEX(mair_index);

    /* Map using large pages where possible. To do this, align up to a 2MB
     * boundary using small pages, map anything possible with large pages, then
     * do the rest using small pages. If virtual and physical addresses are at
     * different offsets from a large page boundary, we cannot map using large
     * pages. */
    if ((virt % LARGE_PAGE_SIZE) == (phys % LARGE_PAGE_SIZE)) {
        while (virt % LARGE_PAGE_SIZE && size) {
            map_small(ctx, virt, phys, tte_flags);
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            size -= PAGE_SIZE;
        }
        while (size / LARGE_PAGE_SIZE) {
            map_large(ctx, virt, phys, tte_flags);
            virt += LARGE_PAGE_SIZE;
            phys += LARGE_PAGE_SIZE;
            size -= LARGE_PAGE_SIZE;
        }
    }

    /* Map whatever remains. */
    while (size) {
        map_small(ctx, virt, phys, tte_flags);
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    return true;
}

/** Set bytes in an area of virtual memory.
 * @param ctx           Context to use.
 * @param addr          Virtual address to write to, must be mapped.
 * @param value         Value to write.
 * @param size          Number of bytes to write.
 * @return              Whether the range specified was valid. */
bool mmu_memset(mmu_context_t *ctx, load_ptr_t addr, uint8_t value, load_size_t size) {
    internal_error("TODO: mmu_memset");
}

/** Copy to an area of virtual memory.
 * @param ctx           Context to use.
 * @param dest          Virtual address to write to, must be mapped.
 * @param src           Memory to read from.
 * @param size          Number of bytes to copy.
 * @return              Whether the range specified was valid. */
bool mmu_memcpy_to(mmu_context_t *ctx, load_ptr_t dest, const void *src, load_size_t size) {
    internal_error("TODO: mmu_memcpy_to");
}

/** Copy from an area of virtual memory.
 * @param ctx           Context to use.
 * @param dest          Memory to write to.
 * @param src           Virtual address to read from, must be mapped.
 * @param size          Number of bytes to copy.
 * @return              Whether the range specified was valid. */
bool mmu_memcpy_from(mmu_context_t *ctx, void *dest, load_ptr_t src, load_size_t size) {
    internal_error("TODO: mmu_memcpy_from");
}

/** Create a new MMU context.
 * @param mode          Load mode for the OS.
 * @param phys_type     Physical memory type to use when allocating tables.
 * @return              Pointer to context. */
mmu_context_t *mmu_context_create(load_mode_t mode, unsigned phys_type) {
    assert(mode == LOAD_MODE_64BIT);

    mmu_context_t *ctx = malloc(sizeof(*ctx));

    ctx->phys_type = phys_type;
    ctx->ttl0_lo   = allocate_structure(ctx);
    ctx->ttl0_hi   = allocate_structure(ctx);

    return ctx;
}
