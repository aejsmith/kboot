/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 KBoot kernel loader.
 */

#include <arch/page.h>

#include <arm64/cpu.h>
#include <arm64/mmu.h>

#include <lib/string.h>

#include <loader/kboot.h>

/** Entry arguments for the kernel. */
typedef struct entry_args {
    uint64_t trampoline_ttl0_hi;        /**< Trampoline address space (high). */
    uint64_t trampoline_ttl0_lo;        /**< Trampoline address space (low). */
    uint64_t trampoline_virt;           /**< Virtual location of trampoline. */
    uint64_t kernel_ttl0_hi;            /**< Kernel address space (high). */
    uint64_t kernel_ttl0_lo;            /**< Kernel address space (low). */
    uint64_t sp;                        /**< Stack pointer for the kernel. */
    uint64_t entry;                     /**< Entry point for kernel. */
    uint64_t tags;                      /**< Tag list virtual address. */

    char trampoline[];
} entry_args_t;

extern void kboot_arch_enter_64(entry_args_t *args) __noreturn;

extern char kboot_trampoline_64[];
extern uint32_t kboot_trampoline_64_size;

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
    /* TODO: Implement KBOOT_LOAD_ARM64_EL2 support. I'm just being a bit lazy
     * for now, basically the entry code needs to handle setting system
     * registers according to current EL. */
    arm64_switch_to_el1();

    /* Configure MAIR. */
    arm64_set_mair();

    entry_args_t *args = (entry_args_t *)phys_to_virt(loader->trampoline_phys);

    args->trampoline_ttl0_hi = loader->trampoline_mmu->ttl0_hi;
    args->trampoline_ttl0_lo = loader->trampoline_mmu->ttl0_lo;
    args->trampoline_virt    = loader->trampoline_virt;
    args->kernel_ttl0_hi     = loader->mmu->ttl0_hi;
    args->kernel_ttl0_lo     = loader->mmu->ttl0_lo;
    args->sp                 = loader->core->stack_base + loader->core->stack_size;
    args->entry              = loader->entry;
    args->tags               = loader->tags_virt;

    /* Copy the trampoline and call the entry code. */
    memcpy(args->trampoline, kboot_trampoline_64, kboot_trampoline_64_size);
    kboot_arch_enter_64(args);
}
