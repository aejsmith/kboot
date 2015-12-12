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
 * @brief               EFI Multiboot loader functions.
 */

#include <efi/services.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <x86/multiboot.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

#include "../../../../bios/include/bios/vbe.h"

/** Get platform-specific Multiboot information.
 * @param loader        Loader internal data. */
void multiboot_platform_load(multiboot_loader_t *loader) {
    void *efi_mmap __cleanup_free;
    efi_uintn_t efi_entries, desc_size;
    efi_uint32_t desc_version;
    multiboot_mmap_entry_t *mmap __cleanup_free = NULL;
    size_t offset;
    void *dest;

    /* Multiboot requires an E820-style memory map. Exit boot services mode to
     * get the final memory map and then we convert it into a E820 format. */
    efi_exit_boot_services(&efi_mmap, &efi_entries, &desc_size, &desc_version);

    offset = 0;
    for (efi_uintn_t i = 0; i < efi_entries; i++) {
        efi_memory_descriptor_t *desc = efi_mmap + (i * desc_size);
        uint32_t type;

        switch (desc->type) {
        case EFI_LOADER_CODE:
        case EFI_LOADER_DATA:
        case EFI_BOOT_SERVICES_CODE:
        case EFI_BOOT_SERVICES_DATA:
        case EFI_CONVENTIONAL_MEMORY:
            type = MULTIBOOT_MMAP_FREE;
            break;
        case EFI_UNUSABLE_MEMORY:
            type = MULTIBOOT_MMAP_BAD;
            break;
        case EFI_ACPI_RECLAIM_MEMORY:
            type = MULTIBOOT_MMAP_ACPI_RECLAIM;
            break;
        case EFI_ACPI_MEMORY_NVS:
            type = MULTIBOOT_MMAP_ACPI_NVS;
            break;
        default:
            type = MULTIBOOT_MMAP_RESERVED;
            break;
        }

        /* Coalesce adjacent entries of same type. */
        if (mmap && mmap[offset].type == type && desc->physical_start == mmap[offset].addr + mmap[offset].len) {
            mmap[offset].len += desc->num_pages * EFI_PAGE_SIZE;
        } else {
            if (mmap)
                offset++;

            mmap = realloc(mmap, sizeof(*mmap) * (offset + 1));
            mmap[offset].size = sizeof(*mmap) - 4;
            mmap[offset].addr = desc->physical_start;
            mmap[offset].len = desc->num_pages * EFI_PAGE_SIZE;
            mmap[offset].type = type;
        }
    }

    /* Copy the final memory map into the info area. */
    assert(mmap);
    loader->info->flags |= MULTIBOOT_INFO_MEMORY | MULTIBOOT_INFO_MEM_MAP;
    loader->info->mmap_length = sizeof(*mmap) * (offset + 1);
    dest = multiboot_alloc_info(loader, loader->info->mmap_length, &loader->info->mmap_addr);
    memcpy(dest, mmap, loader->info->mmap_length);

    /* Get upper/lower memory information. */
    for (size_t i = 0; i <= offset; i++) {
        if (mmap[i].type == MULTIBOOT_MMAP_FREE) {
            if (mmap[i].addr <= 0x100000 && mmap[i].addr + mmap[i].len > 0x100000) {
                loader->info->mem_upper = (mmap[i].addr + mmap[i].len - 0x100000) / 1024;
            } else if (mmap[i].addr == 0) {
                loader->info->mem_lower = min(mmap[i].len, 0x100000) / 1024;
            }
        }
    }

    /* Pass video mode information if required. */
    if (loader->mode) {
        vbe_info_t *control;
        vbe_mode_info_t *mode;

        loader->info->flags |= MULTIBOOT_INFO_VIDEO_INFO;

        /* Try to fudge together something that looks vaguely VBE-ish... */
        control = multiboot_alloc_info(loader, sizeof(*control), &loader->info->vbe_control_info);
        memcpy(control->vbe_signature, "VESA", sizeof(control->vbe_signature));
        control->vbe_version_major = 2;
        control->vbe_version_minor = 0;
        control->video_mode_ptr = loader->mode->mem_phys;
        control->total_memory = loader->mode->mem_size;

        mode = multiboot_alloc_info(loader, sizeof(*mode), &loader->info->vbe_mode_info);
        mode->mode_attributes = 0x9b;
        mode->bytes_per_scan_line = loader->mode->pitch;
        mode->x_resolution = loader->mode->width;
        mode->y_resolution = loader->mode->height;
        mode->bits_per_pixel = loader->mode->format.bpp;
        mode->memory_model = VBE_MEMORY_MODEL_DIRECT_COLOUR;
        mode->reserved1 = 1;
        mode->red_mask_size = loader->mode->format.red_size;
        mode->red_field_position = loader->mode->format.red_pos;
        mode->green_mask_size = loader->mode->format.green_size;
        mode->green_field_position = loader->mode->format.green_pos;
        mode->blue_mask_size = loader->mode->format.blue_size;
        mode->blue_field_position = loader->mode->format.blue_pos;
        mode->phys_base_ptr = loader->mode->mem_phys;

        loader->info->vbe_mode = 0x100 | VBE_MODE_LFB;
    }
}
