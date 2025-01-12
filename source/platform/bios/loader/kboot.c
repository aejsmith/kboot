/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BIOS platform KBoot loader functions.
 */

#include <bios/bios.h>
#include <bios/memory.h>

#include <loader/kboot.h>

#include <assert.h>
#include <memory.h>

/** Perform platform-specific setup for a KBoot kernel.
 * @param loader        Loader internal data. */
void kboot_platform_setup(kboot_loader_t *loader) {
    void *buf __cleanup_free;
    size_t num_entries, entry_size, size;
    kboot_tag_bios_e820_t *tag;

    /* Get a copy of the E820 memory map. */
    bios_memory_get_mmap(&buf, &num_entries, &entry_size);
    size = num_entries * entry_size;
    tag = kboot_alloc_tag(loader, KBOOT_TAG_BIOS_E820, sizeof(*tag) + size);
    tag->num_entries = num_entries;
    tag->entry_size = entry_size;
    memcpy(tag->entries, buf, size);
}
