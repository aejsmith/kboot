/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
    if (prop)
        start = dt_get_value((const uint32_t *)prop, (len == 8) ? 2 : 1);

    prop = fdt_getprop(fdt_address, chosen_offset, "linux,initrd-end", &len);
    if (prop)
        end = dt_get_value((const uint32_t *)prop, (len == 8) ? 2 : 1);

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
