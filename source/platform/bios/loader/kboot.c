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
