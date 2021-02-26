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
 * @brief               Device Tree (DT) support.
 */

#include <dt.h>
#include <loader.h>

/** Address of Flattened Device Tree (FDT) blob. */
const void *fdt_address;

static uint32_t get_num_cells(int node_offset, const char *name, uint32_t def) {
    while (true) {
        int len;
        const uint32_t *prop = fdt_getprop(fdt_address, node_offset, name, &len);
        if (prop)
            return fdt32_to_cpu(*prop);

        if (node_offset == 0)
            return def;

        node_offset = fdt_parent_offset(fdt_address, node_offset);
        if (node_offset < 0)
            return def;
    }
}

/** Get the number of address cells for a node. */
uint32_t dt_get_address_cells(int node_offset) {
    return get_num_cells(node_offset, "#address-cells", 2);
}

/** Get the number of size cells for a node. */
uint32_t dt_get_size_cells(int node_offset) {
    return get_num_cells(node_offset, "#size-cells", 1);
}

/** Get a value from a property.
 * @param ptr           Pointer to value.
 * @param num_cells     Number of cells per value.
 * @return              Value read. */
uint64_t dt_get_value(const uint32_t *ptr, uint32_t num_cells) {
    uint64_t value = 0;

    for (uint32_t i = 0; i < num_cells; i++) {
        value = value << 32;
        value |= fdt32_to_cpu(*(ptr));
        ptr++;
    }

    return value;
}

/** Translate an address according to ranges. */
static phys_ptr_t translate_address(int node_offset, phys_ptr_t address) {
    int parent_offset = node_offset;
    uint32_t parent_address_cells = 0;
    uint32_t parent_size_cells = 0;
    bool first = true;

    while (node_offset > 0) {
        node_offset = parent_offset;
        uint32_t node_address_cells = parent_address_cells;
        uint32_t node_size_cells    = parent_size_cells;

        if (node_offset > 0) {
            parent_offset = fdt_parent_offset(fdt_address, node_offset);
            if (parent_offset < 0)
                break;

            parent_address_cells = dt_get_address_cells(parent_offset);
            parent_size_cells    = dt_get_size_cells(parent_offset);
        } else {
            parent_address_cells = 2;
            parent_size_cells    = 1;
        }

        if (first) {
            /* Just need some details from the parent to start, but we start
             * searching for ranges from the parent. */
            first = false;
            continue;
        }

        int len;
        const uint32_t *prop = fdt_getprop(fdt_address, node_offset, "ranges", &len);
        if (prop) {
            /* Each entry is a (child-address, parent-address, child-length) triplet. */
            uint32_t entry_cells = node_address_cells + parent_address_cells + node_size_cells;
            uint32_t entries     = dt_get_num_entries(len, entry_cells);

            for (uint32_t i = 0; i < entries; i++) {
                uint64_t node_base   = dt_get_value(prop, node_address_cells);
                prop += node_address_cells;
                uint64_t parent_base = dt_get_value(prop, parent_address_cells);
                prop += parent_address_cells;
                uint64_t length      = dt_get_value(prop, node_size_cells);
                prop += node_size_cells;

                /* Translate if within the range. */
                if (address >= node_base && address < (node_base + length)) {
                    address = (address - node_base) + parent_base;
                    break;
                }
            }
        }
    }

    return address;
}

/** Get a register address for a DT node.
 * @param node_offset   Node offset.
 * @param index         Register index.
 * @param _address      Where to store register adddress.
 * @param _size         Where to store register size.
 * @return              Whether found. */
bool dt_get_reg(int node_offset, int index, phys_ptr_t *_address, phys_size_t *_size) {
    uint32_t address_cells = dt_get_address_cells(node_offset);
    uint32_t size_cells    = dt_get_size_cells(node_offset);
    uint32_t total_cells   = address_cells + size_cells;

    int len;
    const uint32_t *prop = fdt_getprop(fdt_address, node_offset, "reg", &len);
    if (!prop)
        return false;

    int entries = dt_get_num_entries(len, total_cells);
    if (index >= entries)
        return false;

    phys_ptr_t address = dt_get_value(&prop[index * total_cells], address_cells);
    phys_size_t size   = dt_get_value(&prop[(index * total_cells) + address_cells], size_cells);

    *_address = translate_address(node_offset, address);
    *_size    = size;
    return true;
}

/** Check if a DT node is compatible with one of an array of strings.
 * @param node_offset   Node offset.
 * @param strings       String array.
 * @param count         Size of array.
 * @return              Whether compatible. */
bool dt_is_compatible(int node_offset, const char **strings, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (fdt_node_check_compatible(fdt_address, node_offset, strings[i]) == 0)
            return true;
    }

    return false;
}

/**
 * Checks the status of a DT node. A device should not be used if this returns
 * false.
 *
 * @param node_offset   Node offset.
 *
 * @return              True if the status is OK or not present.
 */
bool dt_is_available(int node_offset) {
    int len;
    const char *prop = fdt_getprop(fdt_address, node_offset, "status", &len);
    if (!prop)
        return true;

    if (len == 0) {
        /* Present but invalid. */
        return false;
    }

    return strcmp(prop, "ok") == 0 || strcmp(prop, "okay") == 0;
}

/** Validate the FDT and set fdt_address.
 * @param fdt           Platform-supplied FDT blob. */
void dt_init(void *fdt) {
    if (fdt_check_header(fdt) != 0)
        internal_error("Flattened Device Tree (FDT) is invalid");

    fdt_address = fdt;
}
