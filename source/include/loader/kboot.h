/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               KBoot loader internal definitions.
 */

#ifndef __LOADER_KBOOT_H
#define __LOADER_KBOOT_H

#include <lib/allocator.h>
#include <lib/list.h>

#include <config.h>
#include <fs.h>
#include <kboot.h>
#include <mmu.h>

/** Image tag header structure. */
typedef struct kboot_itag {
    list_t header;                      /**< Link to image tag list. */

    uint32_t type;                      /**< Type of the tag. */
    uint64_t data[];                    /**< Tag data. */
} kboot_itag_t;

/** Description of a module to load. */
typedef struct kboot_module {
    list_t header;                      /**< Link to module list. */

    fs_handle_t *handle;                /**< Handle to module. */
    char *name;                         /**< Base name of module. */
} kboot_module_t;

/** Structure describing a virtual memory mapping. */
typedef struct kboot_mapping {
    list_t header;                      /**< Link to virtual mapping list. */

    kboot_vaddr_t start;                /**< Start of the virtual memory range. */
    kboot_vaddr_t size;                 /**< Size of the virtual memory range. */
    kboot_paddr_t phys;                 /**< Physical address that this range maps to. */
    uint32_t cache;                     /**< Cacheability flag. */
} kboot_mapping_t;

/** Structure containing KBoot loader data. */
typedef struct kboot_loader {
    /** Details obtained by configuration command. */
    fs_handle_t *handle;                /**< Handle to kernel image. */
    void *ehdr;                         /**< ELF header. */
    void *phdrs;                        /**< ELF program headers. */
    load_mode_t mode;                   /**< Whether the kernel is 32- or 64-bit. */
    list_t itags;                       /**< Image tags. */
    kboot_itag_image_t *image;          /**< Main image tag. */
    list_t modules;                     /**< Modules to load. */
    const char *path;                   /**< Path to kernel image (only valid during command). */
    bool success;                       /**< Success flag used during iteration functions. */

    /** State used by the main loader. */
    kboot_tag_core_t *core;             /**< Core image tag (also head of the tag list). */
    kboot_itag_load_t *load;            /**< Load image tag. */
    mmu_context_t *mmu;                 /**< MMU context for the kernel. */
    allocator_t allocator;              /**< Virtual address space allocator. */
    list_t mappings;                    /**< Virtual mapping information. */
    load_ptr_t entry;                   /**< Kernel entry point address. */
    load_ptr_t tags_virt;               /**< Virtual address of tag list. */
    mmu_context_t *trampoline_mmu;      /**< Kernel trampoline address space. */
    phys_ptr_t trampoline_phys;         /**< Page containing kernel entry trampoline. */
    load_ptr_t trampoline_virt;         /**< Virtual address of trampoline page. */
} kboot_loader_t;

extern void *kboot_find_itag(kboot_loader_t *loader, uint32_t type);
extern void *kboot_next_itag(kboot_loader_t *loader, void *data);

extern void *kboot_alloc_tag(kboot_loader_t *loader, uint32_t type, size_t size);

extern kboot_vaddr_t kboot_alloc_virtual(
    kboot_loader_t *loader, kboot_paddr_t phys, kboot_vaddr_t size,
    uint32_t cache);
extern void kboot_map_virtual(
    kboot_loader_t *loader, kboot_vaddr_t addr, kboot_paddr_t phys,
    kboot_vaddr_t size, uint32_t cache);

/** Iterate over all tags of a certain type in the image tag list. */
#define kboot_itag_foreach(_loader, _type, _vtype, _vname) \
    for (_vtype *_vname = kboot_find_itag(_loader, _type); _vname; _vname = kboot_next_itag(_loader, _vname))

extern void kboot_arch_check_kernel(kboot_loader_t *loader);
extern void kboot_arch_check_load_params(kboot_loader_t *loader, kboot_itag_load_t *load);
extern void kboot_arch_setup(kboot_loader_t *loader);
extern void kboot_arch_enter(kboot_loader_t *loader) __noreturn;

extern void kboot_platform_setup(kboot_loader_t *loader);

#endif /* __LOADER_KBOOT_H */
