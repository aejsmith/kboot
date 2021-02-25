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
 * @brief               DT platform main functions.
 */

#include <dt.h>
#include <loader.h>
#include <memory.h>

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

    // TODO: initrd
}
