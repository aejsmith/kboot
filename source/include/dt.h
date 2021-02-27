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

#include <lib/utility.h>

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

extern int dt_match_impl(int node_offset, const void *data, size_t stride, size_t count);

/**
 * Match a DT node against an array of compatible strings. This is like
 * dt_is_compatible() but accepts an array of arbitrary types where each entry
 * starts with a compatible string pointer, and also returns a match index
 * rather than a simple bool.
 *
 * This allows for storing additional data along with each entry and using that
 * based on which compatible string was matched. This is useful if behaviour
 * of a driver needs to be adjusted based on which string is matched.
 *
 * For example:
 *
 *   static struct { const char *str; int flags; } my_driver_matches[] = {
 *       { "mfr,model-a", MY_DRIVER_MODEL_A },
 *       { "mfr,model-b", MY_DRIVER_MODEL_B },
 *   };
 *   ...
 *   int match = dt_match(node_offset, my_driver_matches);
 *   if (match >= 0)
 *       my_driver_init(my_driver_matches[match].flags);
 *
 * @param node_offset   Node offset.
 * @param array         Array of matches (each entry must start with a string).
 *
 * @return              Match index, or -1 if no match found.
 */
#define dt_match(node_offset, array) \
    dt_match_impl(node_offset, array, sizeof(array[0]), array_size(array))

extern bool dt_is_compatible(int node_offset, const char **strings, size_t count);
extern bool dt_is_available(int node_offset);

extern void dt_init(void *fdt);

#endif /* CONFIG_TARGET_HAS_FDT */
#endif /* __DT_H */
