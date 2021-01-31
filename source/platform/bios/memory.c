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
 * @brief               BIOS memory detection code.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <bios/bios.h>
#include <bios/memory.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Get a copy of the BIOS memory map.
 * @param _buf          Where to store buffer address.
 * @param _num_entries  Where to store number of entries.
 * @param _entry_size   Where to store size of each entry. */
void bios_memory_get_mmap(void **_buf, size_t *_num_entries, size_t *_entry_size) {
    bios_regs_t regs;
    uint32_t num_entries = 0, entry_size = 0;

    bios_regs_init(&regs);

    /* Get the E820 memory map. */
    do {
        regs.eax = 0xe820;
        regs.edx = E820_SMAP;
        regs.ecx = 64;
        regs.edi = BIOS_MEM_BASE + (num_entries * entry_size);
        bios_call(0x15, &regs);

        if (regs.eflags & X86_FLAGS_CF)
            break;

        if (!num_entries) {
            entry_size = regs.ecx;
        } else {
            assert(entry_size == regs.ecx);
        }

        num_entries++;
    } while (regs.ebx != 0);

    /* FIXME: Should handle BIOSen that don't support this. */
    if (!num_entries)
        boot_error("BIOS does not support E820 memory map");

    *_buf = memdup((void *)BIOS_MEM_BASE, num_entries * entry_size);
    *_num_entries = num_entries;
    *_entry_size = entry_size;
}

/** Detect physical memory. */
void target_memory_probe(void) {
    void *buf __cleanup_free;
    size_t num_entries, entry_size;

    /* Add memory ranges. */
    bios_memory_get_mmap(&buf, &num_entries, &entry_size);
    for (size_t i = 0; i < num_entries; i++) {
        e820_entry_t *entry = buf + (i * entry_size);
        phys_ptr_t start, end;

        /* We only care about free ranges. */
        if (entry->type != E820_TYPE_FREE)
            continue;

        /* The E820 memory map can contain regions that aren't page-aligned.
         * However, we want to deal with page-aligned regions. Therefore, we round
         * start up and end down to the page size, to ensure that we don't resize
         * the region to include memory we shouldn't access. If this results in a
         * zero-length entry, then we ignore it. */
        start = round_up(entry->start, PAGE_SIZE);
        end = round_down(entry->start + entry->length, PAGE_SIZE);
        if (end <= start) {
            dprintf(
                "bios: broken memory map entry: [0x%" PRIx64 ",0x%" PRIx64 ") (%" PRIu32 ")\n",
                entry->start, entry->start + entry->length, entry->type);
            continue;
        }

        /* Ensure that the BIOS data area is not marked as free. BIOSes don't mark
         * it as reserved in the memory map as it can be overwritten if it is no
         * longer needed, but it may be needed by the kernel, for example to call
         * BIOS interrupts. */
        if (start == 0) {
            start = PAGE_SIZE;
            if (start >= end)
                continue;
        }

        /* Add the range to the memory manager. */
        memory_add(start, end - start, MEMORY_TYPE_FREE);
    }

    /* Mark the memory area we use for BIOS calls as internal. */
    memory_protect(BIOS_MEM_BASE, BIOS_MEM_SIZE + PAGE_SIZE);
}
