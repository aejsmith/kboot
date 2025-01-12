/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               KBoot ELF loading functions.
 */

#include <elf.h>

/** KBoot ELF note iteration callback.
 * @param loader        Loader internal data.
 * @param note          Note header.
 * @param desc          Note data.
 * @return              Whether to continue iteration. */
typedef bool (*kboot_note_cb_t)(kboot_loader_t *loader, elf_note_t *note, void *desc);

/** Allocate and map memory for the kernel image.
 * @param loader        Loader internal data.
 * @param virt_base     Virtual base address.
 * @param virt_end      Virtual end address.
 * @return              Loader mapping of allocated memory. */
static void *allocate_kernel(kboot_loader_t *loader, load_ptr_t virt_base, load_ptr_t virt_end) {
    load_size_t size;
    void *dest;
    phys_ptr_t phys;
    size_t align;

    if (virt_base % PAGE_SIZE)
        boot_error("Kernel load address is not page aligned");

    size = round_up(virt_end - virt_base, PAGE_SIZE);

    /* Iterate down in powers of 2 until we reach the minimum allowed alignment. */
    align = (loader->load->alignment) ? loader->load->alignment : PAGE_SIZE;
    dest = NULL;
    while (align >= loader->load->min_alignment) {
        dest = memory_alloc(
            size, align, 0, 0, MEMORY_TYPE_ALLOCATED,
            MEMORY_ALLOC_HIGH | MEMORY_ALLOC_CAN_FAIL, &phys);
        if (dest)
            break;

        align >>= 1;
    }

    if (!dest)
        boot_error("Insufficient memory available (allocating %" PRIuLOAD " bytes)", size);

    dprintf(
        "kboot: loading kernel to 0x%" PRIxPHYS " (alignment: 0x%" PRIx64 ", "
            "min_alignment: 0x%" PRIx64 ", base: 0x%" PRIxLOAD ", size: 0x%" PRIxLOAD ")\n",
        phys, loader->load->alignment, loader->load->min_alignment, virt_base, size);

    kboot_map_virtual(loader, virt_base, phys, size, KBOOT_CACHE_DEFAULT);

    loader->core->kernel_phys = phys;
    return dest;
}

/** Allocate memory for a single segment at a fixed address.
 * @param loader        Loader internal data.
 * @param virt          Virtual load address.
 * @param phys          Physical load address.
 * @param size          Total load size.
 * @param idx           Segment index.
 * @return              Loader mapping of allocated memory. */
static void *allocate_segment(kboot_loader_t *loader, load_ptr_t virt, phys_ptr_t phys, load_size_t size, size_t idx) {
    void *dest;

    if (virt % PAGE_SIZE || phys % PAGE_SIZE)
        boot_error("Segment %zu load address is not page aligned", idx);

    size = round_up(size, PAGE_SIZE);
    dest = memory_alloc(size, 0, phys, phys + size, MEMORY_TYPE_ALLOCATED, 0, NULL);

    dprintf(
        "kboot: loading segment %zu to 0x%" PRIxPHYS " (size: 0x%" PRIxLOAD ", virt: 0x%" PRIxLOAD ")\n",
        idx, phys, size, virt);

    kboot_map_virtual(loader, virt, phys, size, KBOOT_CACHE_DEFAULT);

    return dest;
}

#if CONFIG_TARGET_HAS_KBOOT32
#   define KBOOT_LOAD_ELF32
#   include "kboot_elfxx.h"
#   undef KBOOT_LOAD_ELF32
#endif

#if CONFIG_TARGET_HAS_KBOOT64
#   define KBOOT_LOAD_ELF64
#   include "kboot_elfxx.h"
#   undef KBOOT_LOAD_ELF64
#endif

/** Identify a KBoot kernel image.
 * @param loader        Loader internal data.
 * @return              Status code describing the result of the operation. */
static status_t kboot_elf_identify(kboot_loader_t *loader) {
    size_t size;
    status_t ret;

    /* ELF32 header is smaller, but if the file is less than the size of the
     * ELF64 header then it's probably invalid, so just use the maximum of the
     * two sizes. */
    size = max(sizeof(Elf32_Ehdr), sizeof(Elf64_Ehdr));
    loader->ehdr = malloc(size);

    ret = fs_read(loader->handle, loader->ehdr, size, 0);
    if (ret == STATUS_SUCCESS) {
        #if CONFIG_TARGET_HAS_KBOOT32
            if (elf_check(loader->ehdr, ELFCLASS32, ELF_ENDIAN, ELF_MACHINE_32, ELF_ET_EXEC)) {
                loader->mode = LOAD_MODE_32BIT;
                return kboot_elf32_identify(loader);
            }
        #endif
        #if CONFIG_TARGET_HAS_KBOOT64
            if (elf_check(loader->ehdr, ELFCLASS64, ELF_ENDIAN, ELF_MACHINE_64, ELF_ET_EXEC)) {
                loader->mode = LOAD_MODE_64BIT;
                return kboot_elf64_identify(loader);
            }
        #endif

        ret = STATUS_UNKNOWN_IMAGE;
    }

    free(loader->ehdr);
    return ret;
}

/** Iterate over KBoot ELF notes.
 * @param loader        Loader internal data.
 * @param cb            Callback function.
 * @return              Status code describing result of the operation. */
static status_t kboot_elf_iterate_notes(kboot_loader_t *loader, kboot_note_cb_t cb) {
    #if CONFIG_TARGET_HAS_KBOOT32
        if (loader->mode == LOAD_MODE_32BIT)
            return kboot_elf32_iterate_notes(loader, cb);
    #endif
    #if CONFIG_TARGET_HAS_KBOOT64
        if (loader->mode == LOAD_MODE_64BIT)
            return kboot_elf64_iterate_notes(loader, cb);
    #endif

    unreachable();
}

/** Load the kernel image.
 * @param loader        Loader internal data. */
static void kboot_elf_load_kernel(kboot_loader_t *loader) {
    #if CONFIG_TARGET_HAS_KBOOT32
        if (loader->mode == LOAD_MODE_32BIT) {
            kboot_elf32_load_kernel(loader);
            return;
        }
    #endif
    #if CONFIG_TARGET_HAS_KBOOT64
        if (loader->mode == LOAD_MODE_64BIT) {
            kboot_elf64_load_kernel(loader);
            return;
        }
    #endif

    unreachable();
}

/** Load additional ELF sections.
 * @param loader        Loader internal data. */
static void kboot_elf_load_sections(kboot_loader_t *loader) {
    #if CONFIG_TARGET_HAS_KBOOT32
        if (loader->mode == LOAD_MODE_32BIT) {
            kboot_elf32_load_sections(loader);
            return;
        }
    #endif
    #if CONFIG_TARGET_HAS_KBOOT64
        if (loader->mode == LOAD_MODE_64BIT) {
            kboot_elf64_load_sections(loader);
            return;
        }
    #endif

    unreachable();
}
