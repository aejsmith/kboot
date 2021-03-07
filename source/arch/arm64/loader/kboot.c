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
 * @brief               ARM64 KBoot kernel loader.
 */

#include <arch/page.h>

#include <arm64/mmu.h>

#include <loader/kboot.h>

#include <loader.h>

/** Check whether a kernel image is supported.
 * @param loader        Loader internal data. */
void kboot_arch_check_kernel(kboot_loader_t *loader) {
    /* Nothing we need to check. */
}

/** Validate kernel load parameters.
 * @param loader        Loader internal data.
 * @param load          Load image tag. */
void kboot_arch_check_load_params(kboot_loader_t *loader, kboot_itag_load_t *load) {
    if (load->flags & KBOOT_LOAD_ARM64_EL2)
        internal_error("TODO: EL2 support");

    if (!(load->flags & KBOOT_LOAD_FIXED) && !load->alignment) {
        /* Set default alignment parameters. */
        load->alignment     = LARGE_PAGE_SIZE;
        load->min_alignment = 0x100000;
    }

    if (load->virt_map_base || load->virt_map_size) {
        if (!is_kernel_range(load->virt_map_base, load->virt_map_size))
            boot_error("Kernel specifies invalid virtual map range");
    } else {
        /* Default to the kernel (upper) address space range */
        load->virt_map_base = 0xffff000000000000;
        load->virt_map_size = 0x1000000000000ull;
    }
}

/** Perform architecture-specific setup tasks.
 * @param loader        Loader internal data. */
void kboot_arch_setup(kboot_loader_t *loader) {
    /* We require kernel to be mapped in the upper address space. */
    if (!is_kernel_range(loader->entry, 4))
        boot_error("Kernel load adddress is invalid");

    /* Find a location to recursively map the pagetables at. We'll drop this in
     * the lower half to ensure it does not conflict with the kernel virtual
     * map area. */
    uint64_t *ttl0 = (uint64_t *)phys_to_virt(loader->mmu->ttl0_lo);
    unsigned i = 512;
    while (i--) {
        if (!(ttl0[i] & ARM64_TTE_PRESENT)) {
            ttl0[i] = loader->mmu->ttl0_hi | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;

            kboot_tag_pagetables_arm64_t *tag = kboot_alloc_tag(loader, KBOOT_TAG_PAGETABLES, sizeof(*tag));

            tag->ttl0    = loader->mmu->ttl0_hi;
            tag->mapping = i * ARM64_TTL1_RANGE;

            dprintf("kboot: recursive PML4 mapping at 0x%" PRIx64 "\n", tag->mapping);
            return;
        }
    }

    boot_error("Unable to allocate page table mapping space");
}

/** Enter the kernel.
 * @param loader        Loader internal data. */
__noreturn void kboot_arch_enter(kboot_loader_t *loader) {
    internal_error("TODO: kboot_arch_enter");
}
