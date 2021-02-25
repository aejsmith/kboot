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
 *
 * This provides some common functionality for platforms which use a Device
 * Tree. It is in generic code rather than the DT platform, since other
 * platforms might make use of a DT in some way (e.g. EFI on ARM).
 */

#ifndef __DT_H
#define __DT_H

#ifdef CONFIG_TARGET_HAS_FDT

#include <libfdt.h>

extern const void *fdt_address;

/** Get the number of entries in a property.
 * @param len           Byte length of the property.
 * @param num_cells     Number of cells per entry. */
static inline uint32_t dt_get_num_entries(int len, uint32_t num_cells) {
    return len / 4 / num_cells;
}

extern uint32_t dt_get_address_cells(int node_offset);
extern uint32_t dt_get_size_cells(int node_offset);
extern uint64_t dt_get_value(const uint32_t *ptr, uint32_t num_cells);
extern bool dt_get_reg(int node_offset, int index, phys_ptr_t *_address, phys_size_t *_size);

extern bool dt_is_compatible(int node_offset, const char **strings, size_t count);

extern void dt_init(void *fdt);

#endif /* CONFIG_TARGET_HAS_FDT */
#endif /* __DT_H */
