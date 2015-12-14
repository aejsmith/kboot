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
 * @brief               BIOS Multiboot loader functions.
 *
 * TODO:
 *  - APM/config table/drive info.
 */

#include <bios/bios.h>
#include <bios/disk.h>
#include <bios/memory.h>
#include <bios/video.h>

#include <lib/utility.h>

#include <x86/multiboot.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Get platform-specific Multiboot information.
 * @param loader        Loader internal data. */
void multiboot_platform_load(multiboot_loader_t *loader) {
    void *buf __cleanup_free, *dest;
    size_t num_entries, entry_size;

    /* Get a memory map. */
    bios_memory_get_mmap(&buf, &num_entries, &entry_size);
    loader->info->flags |= MULTIBOOT_INFO_MEMORY | MULTIBOOT_INFO_MEM_MAP;
    loader->info->mmap_length = num_entries * (entry_size + 4);
    dest = multiboot_alloc_info(loader, loader->info->mmap_length, &loader->info->mmap_addr);
    for (size_t i = 0; i < num_entries; i++) {
        e820_entry_t *entry = buf + (i * entry_size);

        /* Get upper/lower memory information. */
        if (entry->type == E820_TYPE_FREE) {
            if (entry->start <= 0x100000 && entry->start + entry->length > 0x100000) {
                loader->info->mem_upper = (entry->start + entry->length - 0x100000) / 1024;
            } else if (entry->start == 0) {
                loader->info->mem_lower = min(entry->length, 0x100000) / 1024;
            }
        }

        /* Add a new entry, with the size field beforehand. Copy the whole size
         * returned, in case the BIOS has returned some fields that we don't
         * know about. */
        *(uint32_t *)dest = entry_size;
        memcpy(dest + 4, entry, entry_size);
        dest += entry_size + 4;
    }

    /* Try to get the boot device ID. */
    if (current_environ->device->type == DEVICE_TYPE_DISK) {
        disk_device_t *disk = (disk_device_t *)current_environ->device;

        loader->info->flags |= MULTIBOOT_INFO_BOOTDEV;
        loader->info->boot_device = (uint32_t)bios_disk_get_id(disk) << 24;
        if (disk_device_is_partition(disk))
            loader->info->boot_device |= (uint32_t)disk->id << 16;
    }

    /* Pass video mode information if required. */
    if (loader->mode) {
        vbe_info_t *control;
        vbe_mode_info_t *mode;
        uint32_t phys;

        loader->info->flags |= MULTIBOOT_INFO_VIDEO_INFO;

        control = multiboot_alloc_info(loader, sizeof(*control), &phys);
        if (bios_video_get_controller_info(control))
            loader->info->vbe_control_info = phys;

        mode = multiboot_alloc_info(loader, sizeof(*mode), &phys);
        if (bios_video_get_mode_info(loader->mode, mode))
            loader->info->vbe_mode_info = phys;

        loader->info->vbe_mode = bios_video_get_mode_num(loader->mode);
    }
}
