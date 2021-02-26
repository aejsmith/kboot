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
 * @brief               DT platform memory functions.
 */

#include <dt/memory.h>

#include <lib/utility.h>

#include <dt.h>
#include <loader.h>
#include <memory.h>

phys_ptr_t dt_initrd_address = 0;
phys_size_t dt_initrd_size = 0;

static void find_initrd(void) {
    int chosen_offset = fdt_path_offset(fdt_address, "/chosen");
    if (chosen_offset < 0)
        return;

    uint64_t start = 0;
    uint64_t end = 0;

    int len;
    const void *prop = fdt_getprop(fdt_address, chosen_offset, "linux,initrd-start", &len);
    if (prop) {
        if (len == 4) {
            start = fdt32_to_cpu(*(const uint32_t *)prop);
        } else if (len == 8) {
            start = fdt64_to_cpu(*(const uint64_t *)prop);
        }
    }

    prop = fdt_getprop(fdt_address, chosen_offset, "linux,initrd-end", &len);
    if (prop) {
        if (len == 4) {
            end = fdt32_to_cpu(*(const uint32_t *)prop);
        } else if (len == 8) {
            end = fdt64_to_cpu(*(const uint64_t *)prop);
        }
    }

    dt_initrd_address = start;
    dt_initrd_size    = end - start;
}

/** Detect physical memory. */
void target_memory_probe(void) {
    /* Find the /memory node */
    int memory_offset = fdt_path_offset(fdt_address, "/memory");
    if (memory_offset < 0)
        internal_error("Missing '/memory' FDT node");

    /* reg property contains address/size pairs. */
    int len;
    const uint32_t *prop = fdt_getprop(fdt_address, memory_offset, "reg", &len);
    if (!prop)
        internal_error("Missing '/memory/reg' FDT property");

    uint32_t address_cells = dt_get_address_cells(memory_offset);
    uint32_t size_cells    = dt_get_size_cells(memory_offset);
    uint32_t total_cells   = address_cells + size_cells;
    uint32_t num_entries   = dt_get_num_entries(len, total_cells);

    dprintf(
        "memory: DT contains %" PRIu32 " entries (%" PRIu32 " address cells, %" PRIu32 " size cells)\n",
        num_entries, address_cells, size_cells);

    for (uint32_t i = 0; i < num_entries; i++) {
        uint64_t address = dt_get_value(prop, address_cells);
        prop += address_cells;
        uint64_t size    = dt_get_value(prop, size_cells);
        prop += size_cells;

        memory_add(address, size, MEMORY_TYPE_FREE);
    }

    /* Protect the FDT. */
    memory_protect((phys_ptr_t)fdt_address, fdt_totalsize(fdt_address));

    /* Protect memory reservations from the DT. */
    for (uint32_t i = 0; ; i++) {
        uint64_t address, size;
        fdt_get_mem_rsv(fdt_address, i, &address, &size);
        if (!size)
            break;

        /* No guarantee that these are page-aligned. */
        phys_ptr_t start = round_down(address, PAGE_SIZE);
        phys_ptr_t end   = round_up(address + size, PAGE_SIZE);

        dprintf("memory: DT reservation @ 0x%" PRIxPHYS "-0x%" PRIxPHYS "\n", start, end);

        memory_remove(start, end - start);
    }

    find_initrd();
    if (dt_initrd_size != 0) {
        /* Protect the initrd. */
        memory_protect(dt_initrd_address, dt_initrd_size);
    }
}
