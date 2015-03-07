/*
 * Copyright (C) 2011-2015 Alex Smith
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
 * @brief               x86 MMU functions.
 *
 * Notes:
 *  - Although we're using 64-bit physical addresses, we have TARGET_PHYS_MAX
 *    set to 4GB, so we will never allocate higher physical addresses. This
 *    means it is safe to truncate physical addresses when creating 32-bit
 *    page tables.
 */

#include <arch/page.h>

#include <x86/cpu.h>
#include <x86/mmu.h>

#include <lib/string.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <mmu.h>

/** Whether large pages are supported. */
static bool large_pages_supported = false;

/** Allocate a paging structure.
 * @param ctx           Context to allocate for.
 * @return              Physical address allocated. */
static phys_ptr_t allocate_structure(mmu_context_t *ctx) {
    phys_ptr_t phys;
    void *virt;

    /* Allocate high to try to avoid any fixed kernel load location. */
    virt = memory_alloc(PAGE_SIZE, PAGE_SIZE, 0, 0, ctx->phys_type, MEMORY_ALLOC_HIGH, &phys);
    memset(virt, 0, PAGE_SIZE);
    return phys;
}

/** Get a page directory from a 64-bit context.
 * @param ctx           Context to get from.
 * @param virt          Virtual address to get for (can be non-aligned).
 * @param alloc         Whether to allocate if not found.
 * @return              Address of page directory, or NULL if not found. */
static uint64_t *get_pdir_64(mmu_context_t *ctx, uint64_t virt, bool alloc) {
    uint64_t *pml4, *pdpt;
    phys_ptr_t addr;
    unsigned pml4e, pdpte;

    pml4 = (uint64_t *)phys_to_virt(ctx->cr3);

    /* Get the page directory pointer number. */
    pml4e = (virt / X86_PDPT_RANGE_64) % 512;
    if (!(pml4[pml4e] & X86_PTE_PRESENT)) {
        if (!alloc)
            return NULL;

        addr = allocate_structure(ctx);
        pml4[pml4e] = addr | X86_PTE_PRESENT | X86_PTE_WRITE;
    }

    /* Get the PDPT from the PML4. */
    pdpt = (uint64_t *)phys_to_virt((ptr_t)(pml4[pml4e] & X86_PTE_ADDR_MASK_64));

    /* Get the page directory number. */
    pdpte = (virt % X86_PDPT_RANGE_64) / X86_PDIR_RANGE_64;
    if (!(pdpt[pdpte] & X86_PTE_PRESENT)) {
        if (!alloc)
            return NULL;

        addr = allocate_structure(ctx);
        pdpt[pdpte] = addr | X86_PTE_PRESENT | X86_PTE_WRITE;
    }

    /* Return the page directory address. */
    return (uint64_t *)phys_to_virt((ptr_t)(pdpt[pdpte] & X86_PTE_ADDR_MASK_64));
}

/** Map a large page in a 64-bit context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to. */
static void map_large_64(mmu_context_t *ctx, uint64_t virt, uint64_t phys) {
    uint64_t *pdir;
    unsigned pde;

    assert(!(virt % LARGE_PAGE_SIZE_64));
    assert(!(phys % LARGE_PAGE_SIZE_64));

    pdir = get_pdir_64(ctx, virt, true);
    pde = (virt % X86_PDIR_RANGE_64) / LARGE_PAGE_SIZE_64;
    pdir[pde] = phys | X86_PTE_PRESENT | X86_PTE_WRITE | X86_PTE_LARGE;
}

/** Map a small page in a 64-bit context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to. */
static void map_small_64(mmu_context_t *ctx, uint64_t virt, uint64_t phys) {
    uint64_t *pdir, *ptbl;
    phys_ptr_t addr;
    unsigned pde, pte;

    assert(!(virt % PAGE_SIZE));
    assert(!(phys % PAGE_SIZE));

    pdir = get_pdir_64(ctx, virt, true);

    /* Get the page directory entry number. */
    pde = (virt % X86_PDIR_RANGE_64) / X86_PTBL_RANGE_64;
    if (!(pdir[pde] & X86_PTE_PRESENT)) {
        addr = allocate_structure(ctx);
        pdir[pde] = addr | X86_PTE_PRESENT | X86_PTE_WRITE;
    }

    /* Get the page table from the page directory. */
    ptbl = (uint64_t *)phys_to_virt((ptr_t)(pdir[pde] & X86_PTE_ADDR_MASK_64));

    /* Map the page. */
    pte = (virt % X86_PTBL_RANGE_64) / PAGE_SIZE;
    ptbl[pte] = phys | X86_PTE_PRESENT | X86_PTE_WRITE;
}

/** Create a mapping in a 64-bit MMU context. */
static void mmu_map_64(mmu_context_t *ctx, uint64_t virt, uint64_t phys, uint64_t size) {
    /* Map using large pages where possible (always supported on 64-bit). To do
     * this, align up to a 2MB boundary using small pages, map anything possible
     * with large pages, then do the rest using small pages. If virtual and
     * physical addresses are at different offsets from a large page boundary,
     * we cannot map using large pages. */
    if ((virt % LARGE_PAGE_SIZE_64) == (phys % LARGE_PAGE_SIZE_64)) {
        while (virt % LARGE_PAGE_SIZE_64 && size) {
            map_small_64(ctx, virt, phys);
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            size -= PAGE_SIZE;
        }
        while (size / LARGE_PAGE_SIZE_64) {
            map_large_64(ctx, virt, phys);
            virt += LARGE_PAGE_SIZE_64;
            phys += LARGE_PAGE_SIZE_64;
            size -= LARGE_PAGE_SIZE_64;
        }
    }

    /* Map whatever remains. */
    while (size) {
        map_small_64(ctx, virt, phys);
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

/** Map a large page in a 32-bit context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to. */
static void map_large_32(mmu_context_t *ctx, uint32_t virt, uint32_t phys) {
    uint32_t *pdir;
    unsigned pde;

    assert(!(virt % LARGE_PAGE_SIZE_32));
    assert(!(phys % LARGE_PAGE_SIZE_32));

    pdir = (uint32_t *)phys_to_virt(ctx->cr3);
    pde = virt / X86_PTBL_RANGE_32;
    pdir[pde] = phys | X86_PTE_PRESENT | X86_PTE_WRITE | X86_PTE_LARGE;
}

/** Map a small page in a 32-bit context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to. */
static void map_small_32(mmu_context_t *ctx, uint32_t virt, uint32_t phys) {
    uint32_t *pdir, *ptbl;
    phys_ptr_t addr;
    unsigned pde, pte;

    assert(!(virt % PAGE_SIZE));
    assert(!(phys % PAGE_SIZE));

    pdir = (uint32_t *)phys_to_virt(ctx->cr3);

    /* Get the page directory entry number. */
    pde = virt / X86_PTBL_RANGE_32;
    if (!(pdir[pde] & X86_PTE_PRESENT)) {
        addr = allocate_structure(ctx);
        pdir[pde] = addr | X86_PTE_PRESENT | X86_PTE_WRITE;
    }

    /* Get the page table from the page directory. */
    ptbl = (uint32_t *)phys_to_virt((ptr_t)(pdir[pde] & X86_PTE_ADDR_MASK_32));

    /* Map the page. */
    pte = (virt % X86_PTBL_RANGE_32) / PAGE_SIZE;
    ptbl[pte] = phys | X86_PTE_PRESENT | X86_PTE_WRITE;
}

/** Create a mapping in a 32-bit MMU context. */
static void mmu_map_32(mmu_context_t *ctx, uint32_t virt, uint32_t phys, uint32_t size) {
    /* Same as mmu_map_64(). */
    if (large_pages_supported) {
        if ((virt % LARGE_PAGE_SIZE_32) == (phys % LARGE_PAGE_SIZE_32)) {
            while (virt % LARGE_PAGE_SIZE_32 && size) {
                map_small_32(ctx, virt, phys);
                virt += PAGE_SIZE;
                phys += PAGE_SIZE;
                size -= PAGE_SIZE;
            }
            while (size / LARGE_PAGE_SIZE_32) {
                map_large_32(ctx, virt, phys);
                virt += LARGE_PAGE_SIZE_32;
                phys += LARGE_PAGE_SIZE_32;
                size -= LARGE_PAGE_SIZE_32;
            }
        }
    }

    /* Map whatever remains. */
    while (size) {
        map_small_32(ctx, virt, phys);
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

/** Create a mapping in an MMU context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param size          Size of the mapping to create.
 * @return              Whether the supplied addresses were valid. */
bool mmu_map(mmu_context_t *ctx, load_ptr_t virt, phys_ptr_t phys, load_size_t size) {
    assert(!(virt % PAGE_SIZE));
    assert(!(phys % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));

    if (ctx->mode == LOAD_MODE_64BIT) {
        if (!is_canonical_range(virt, size))
            return false;

        mmu_map_64(ctx, virt, phys, size);
    } else {
        assert(virt + size <= 0x100000000ull);

        if (phys >= 0x100000000ull || phys + size > 0x100000000ull)
            return false;

        mmu_map_32(ctx, virt, phys, size);
    }

    return true;
}

/** Memory operation mode. */
enum {
    MMU_MEM_SET,
    MMU_MEM_COPY_TO,
    MMU_MEM_COPY_FROM,
};

/** Perform a memory operation. */
static void do_mem_op(phys_ptr_t page, size_t page_size, unsigned op, ptr_t *_value) {
    void *ptr = (void *)phys_to_virt(page);

    if (op == MMU_MEM_SET) {
        memset(ptr, (uint8_t)*_value, page_size);
    } else {
        if (op == MMU_MEM_COPY_TO) {
            memcpy(ptr, (const void *)*_value, page_size);
        } else {
            memcpy((void *)*_value, ptr, page_size);
        }

        *_value += page_size;
    }
}

/** Memory operation on 64-bit MMU context.
 * @param ctx           Context to operate on.
 * @param addr          Virtual address to operate on.
 * @param size          Size of range.
 * @param op            Operation to perform.
 * @param value         Value to set, or loader source/destination address.
 * @return              Whether the address range was valid. */
static bool mmu_mem_op_64(mmu_context_t *ctx, uint64_t addr, uint64_t size, unsigned op, ptr_t value) {
    uint64_t *pdir = NULL, *ptbl = NULL;

    while (size) {
        phys_ptr_t page;
        size_t page_size;

        /* If we have crossed a page directory boundary, get new directory. */
        if (!pdir || !(addr % X86_PDIR_RANGE_64)) {
            pdir = get_pdir_64(ctx, addr, false);
            if (!pdir)
                return false;
        }

        /* Same for page table. */
        if (!ptbl || !(addr % X86_PTBL_RANGE_64)) {
            unsigned pde = (addr % X86_PDIR_RANGE_64) / X86_PTBL_RANGE_64;
            if (!(pdir[pde] & X86_PTE_PRESENT))
                return false;

            if (pdir[pde] & X86_PTE_LARGE) {
                page = (pdir[pde] & X86_PTE_ADDR_MASK_64) + (addr % LARGE_PAGE_SIZE_64);
                page_size = LARGE_PAGE_SIZE_64 - (addr % LARGE_PAGE_SIZE_64);
                ptbl = NULL;
            } else {
                ptbl = (uint64_t *)phys_to_virt((ptr_t)(pdir[pde] & X86_PTE_ADDR_MASK_64));
            }
        }

        if (ptbl) {
            unsigned pte = (addr % X86_PTBL_RANGE_64) / PAGE_SIZE;
            if (!(ptbl[pte] & X86_PTE_PRESENT))
                return false;

            page = (ptbl[pte] & X86_PTE_ADDR_MASK_64) + (addr % PAGE_SIZE);
            page_size = PAGE_SIZE - (addr % PAGE_SIZE);
        }

        do_mem_op(page, page_size, op, &value);

        addr += page_size;
        size -= page_size;
    }

    return true;
}

/** Memory operation on 32-bit MMU context.
 * @param ctx           Context to operate on.
 * @param addr          Virtual address to operate on.
 * @param size          Size of range.
 * @param op            Operation to perform.
 * @param value         Value to set, or loader source/destination address.
 * @return              Whether the address range was valid. */
static bool mmu_mem_op_32(mmu_context_t *ctx, uint32_t addr, uint32_t size, unsigned op, ptr_t value) {
    uint32_t *pdir = (uint32_t *)phys_to_virt(ctx->cr3);
    uint32_t *ptbl = NULL;

    while (size) {
        phys_ptr_t page;
        size_t page_size;

        /* If we have crossed a page table boundary, get new table. */
        if (!ptbl || !(addr % X86_PTBL_RANGE_32)) {
            unsigned pde = addr / X86_PTBL_RANGE_32;
            if (!(pdir[pde] & X86_PTE_PRESENT))
                return false;

            if (pdir[pde] & X86_PTE_LARGE) {
                page = (pdir[pde] & X86_PTE_ADDR_MASK_32) + (addr % LARGE_PAGE_SIZE_32);
                page_size = LARGE_PAGE_SIZE_32 - (addr % LARGE_PAGE_SIZE_32);
                ptbl = NULL;
            } else {
                ptbl = (uint32_t *)phys_to_virt((ptr_t)(pdir[pde] & X86_PTE_ADDR_MASK_32));
            }
        }

        if (ptbl) {
            unsigned pte = (addr % X86_PTBL_RANGE_32) / PAGE_SIZE;
            if (!(ptbl[pte] & X86_PTE_PRESENT))
                return false;

            page = (ptbl[pte] & X86_PTE_ADDR_MASK_32) + (addr % PAGE_SIZE);
            page_size = PAGE_SIZE - (addr % PAGE_SIZE);
        }

        do_mem_op(page, page_size, op, &value);

        addr += page_size;
        size -= page_size;
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
    if (ctx->mode == LOAD_MODE_64BIT) {
        if (!is_canonical_range(addr, size))
            return false;

        return mmu_mem_op_64(ctx, addr, size, MMU_MEM_SET, value);
    } else {
        if (addr >= 0x100000000ull || addr + size > 0x100000000ull)
            return false;

        return mmu_mem_op_32(ctx, addr, size, MMU_MEM_SET, value);
    }
}

/** Copy to an area of virtual memory.
 * @param ctx           Context to use.
 * @param dest          Virtual address to write to, must be mapped.
 * @param src           Memory to read from.
 * @param size          Number of bytes to copy.
 * @return              Whether the range specified was valid. */
bool mmu_memcpy_to(mmu_context_t *ctx, load_ptr_t dest, const void *src, load_size_t size) {
    if (ctx->mode == LOAD_MODE_64BIT) {
        if (!is_canonical_range(dest, size))
            return false;

        return mmu_mem_op_64(ctx, dest, size, MMU_MEM_COPY_TO, (ptr_t)src);
    } else {
        if (dest >= 0x100000000ull || dest + size > 0x100000000ull)
            return false;

        return mmu_mem_op_32(ctx, dest, size, MMU_MEM_COPY_TO, (ptr_t)src);
    }
}

/** Copy from an area of virtual memory.
 * @param ctx           Context to use.
 * @param dest          Memory to write to.
 * @param src           Virtual address to read from, must be mapped.
 * @param size          Number of bytes to copy.
 * @return              Whether the range specified was valid. */
bool mmu_memcpy_from(mmu_context_t *ctx, void *dest, load_ptr_t src, load_size_t size) {
    if (ctx->mode == LOAD_MODE_64BIT) {
        if (!is_canonical_range(src, size))
            return false;

        return mmu_mem_op_64(ctx, src, size, MMU_MEM_COPY_FROM, (ptr_t)dest);
    } else {
        if (src >= 0x100000000ull || src + size > 0x100000000ull)
            return false;

        return mmu_mem_op_32(ctx, src, size, MMU_MEM_COPY_FROM, (ptr_t)dest);
    }
}

/** Create a new MMU context.
 * @param mode          Load mode for the OS.
 * @param phys_type     Physical memory type to use when allocating tables.
 * @return              Pointer to context. */
mmu_context_t *mmu_context_create(load_mode_t mode, unsigned phys_type) {
    mmu_context_t *ctx;
    x86_cpuid_t cpuid;

    if (mode == LOAD_MODE_32BIT) {
        /* Check for large page support. */
        x86_cpuid(X86_CPUID_FEATURE_INFO, &cpuid);
        large_pages_supported = cpuid.edx & X86_FEATURE_PSE;

        /* Enable it for the kernel, as we will use them if they are supported. */
        if (large_pages_supported)
            x86_write_cr4(x86_read_cr4() | X86_CR4_PSE);
    }

    ctx = malloc(sizeof(*ctx));
    ctx->mode = mode;
    ctx->phys_type = phys_type;
    ctx->cr3 = allocate_structure(ctx);
    return ctx;
}
