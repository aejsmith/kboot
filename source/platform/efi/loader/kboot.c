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
    efi_status_t ret;

    /* Try multiple times to call ExitBootServices, it can change the memory map
     * the first time. This should not happen more than once however, so only
     * do it twice. */
    for (unsigned i = 0; i < 2; i++) {
        efi_uintn_t size, map_key, desc_size;
        efi_uint32_t desc_version;
        void *buf __cleanup_free = NULL;

        /* Call a first time to get the needed buffer size. */
        size = 0;
        ret = efi_call(efi_boot_services->get_memory_map, &size, NULL, &map_key, &desc_size, &desc_version);
        if (ret != EFI_BUFFER_TOO_SMALL)
            internal_error("Failed to get memory map size (0x%zx)", ret);

        buf = malloc(size);

        ret = efi_call(efi_boot_services->get_memory_map, &size, buf, &map_key, &desc_size, &desc_version);
        if (ret != EFI_SUCCESS)
            internal_error("Failed to get memory map (0x%zx)", ret);

        /* Try to exit boot services. */
        ret = efi_call(efi_boot_services->exit_boot_services, efi_image_handle, map_key);
        if (ret == EFI_SUCCESS) {
            kboot_tag_efi_t *tag;

            /* Disable the debug console, it could now be invalid. */
            debug_console.out = NULL;

            tag = kboot_alloc_tag(loader, KBOOT_TAG_EFI, sizeof(*tag) + size);
            tag->type = KBOOT_EFI_TYPE;
            tag->system_table = virt_to_phys((ptr_t)efi_system_table);
            tag->num_memory_descs = size / desc_size;
            tag->memory_desc_size = desc_size;
            tag->memory_desc_version = desc_version;
            memcpy(tag->memory_map, buf, size);

            return;
        }
    }

    internal_error("Failed to exit boot services (0x%zx)", ret);
}
