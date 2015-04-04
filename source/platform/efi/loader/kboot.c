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
 * @brief               EFI platform KBoot loader functions.
 */

#include <efi/efi.h>

#include <lib/string.h>

#include <loader/kboot.h>

#include <assert.h>
#include <memory.h>

#ifdef __LP64__
#   define KBOOT_EFI_TYPE   KBOOT_EFI_64
#else
#   define KBOOT_EFI_TYPE   KBOOT_EFI_32
#endif

/** Perform platform-specific setup for a KBoot kernel.
 * @param loader        Loader internal data. */
void kboot_platform_setup(kboot_loader_t *loader) {
    void *memory_map __cleanup_free;
    efi_uintn_t num_entries, desc_size, size;
    efi_uint32_t desc_version;
    kboot_tag_efi_t *tag;

    /* Exit boot services mode and get the final memory map. */
    efi_exit_boot_services(&memory_map, &num_entries, &desc_size, &desc_version);

    /* Pass the memory map to the kernel. */
    size = num_entries * desc_size;
    tag = kboot_alloc_tag(loader, KBOOT_TAG_EFI, sizeof(*tag) + size);
    tag->type = KBOOT_EFI_TYPE;
    tag->system_table = virt_to_phys((ptr_t)efi_system_table);
    tag->num_memory_descs = num_entries;
    tag->memory_desc_size = desc_size;
    tag->memory_desc_version = desc_version;
    memcpy(tag->memory_map, memory_map, size);
}
