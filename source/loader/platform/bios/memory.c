/*
 * Copyright (C) 2010-2014 Alex Smith
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

#include <loader.h>
#include <memory.h>

/** Detect physical memory. */
void bios_memory_init(void) {
    bios_regs_t regs;
    size_t count = 0, i;
    e820_entry_t *mmap;
    phys_ptr_t start, end;

    bios_regs_init(&regs);

    /* Obtain a memory map using interrupt 15h, function E820h. */
    do {
        regs.eax = 0xe820;
        regs.edx = E820_SMAP;
        regs.ecx = 64;
        regs.edi = BIOS_MEM_BASE + (count * sizeof(e820_entry_t));
        bios_call(0x15, &regs);

        /* If CF is set, the call was not successful. BIOSes are allowed to
         * return a non-zero continuation value in EBX and return an error on
         * next call to indicate that the end of the list has been reached. */
        if (regs.eflags & X86_FLAGS_CF)
            break;

        count++;
    } while (regs.ebx != 0);

    /* FIXME: Should handle BIOSen that don't support this. */
    if (count == 0)
        boot_error("BIOS does not support E820 memory map");

    /* Iterate over the obtained memory map and add the entries. */
    mmap = (e820_entry_t *)BIOS_MEM_BASE;
    for (i = 0; i < count; i++) {
        /* We only care about free ranges. */
        if (mmap[i].type != E820_TYPE_FREE)
            continue;

        /* The E820 memory map can contain regions that aren't page-aligned.
         * However, we want to deal with page-aligned regions. Therefore, we
         * round start up and end down to the page size, to ensure that we don't
         * resize the region to include memory we shouldn't access. If this
         * results in a zero-length entry, then we ignore it. */
        start = round_up(mmap[i].start, PAGE_SIZE);
        end = round_down(mmap[i].start + mmap[i].length, PAGE_SIZE);

        /* What we did above may have made the region too small, warn
         * and ignore it if this is the case. */
        if (end <= start) {
            dprintf("memory: broken memory map entry: [0x%" PRIx64 ",0x%" PRIx64 ") (%" PRIu32 ")\n",
                mmap[i].start, mmap[i].start + mmap[i].length, mmap[i].type);
            continue;
        }

        /* Ensure that the BIOS data area is not marked as free. BIOSes don't
         * mark it as reserved in the memory map as it can be overwritten if it
         * is no longer needed, but it may be needed by the kernel, for example
         * to call BIOS interrupts. */
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

    /* Initialize the memory manager. */
    memory_init();
}
