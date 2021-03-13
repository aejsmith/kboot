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

#include <lib/list.h>
#include <lib/utility.h>

#include <status.h>

#include <libfdt.h>

struct dt_device;

extern const void *fdt_address;

/**
 * Driver API.
 */

/** DT match table definition. */
typedef struct dt_match_table {
    const void *data;                   /**< Match table data. */
    size_t stride;                      /**< Stride between table entries. */
    size_t count;                       /**< Number of array entries. */
} dt_match_table_t;

/**
 * Defines a device tree match table. This is an array of arbitrary structures
 * where each entry starts with a compatible string pointer.
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
 *   
 *   dt_match_table_t table = DT_MATCH_TABLE(my_driver_matches);
 *
 * @param array         Array of matches (each entry must start with a string).
 */
#define DT_MATCH_TABLE(array) \
    { array, sizeof(array[0]), array_size(array) }

/** DT driver definition class. */
typedef struct dt_driver {
    dt_match_table_t matches;           /**< Match table. */
    bool ignore_status;                 /**< Ignore status of matching DT nodes and force use. */

    /** Initialize a device that matched this driver.
     * @param device        Device to initialize.
     * @return              Status code describing the result of the operation. */
    status_t (*init)(struct dt_device *device);
} dt_driver_t;

/** Define a builtin DT driver. */
#define BUILTIN_DT_DRIVER(name) \
    static dt_driver_t name; \
    DEFINE_BUILTIN(BUILTIN_TYPE_DT_DRIVER, name); \
    static dt_driver_t name

/** DT device structure. */
typedef struct dt_device {
    list_t link;

    int node_offset;                    /**< FDT node offset. */
    const char *name;                   /**< Node name. */
    const void *match;                  /**< Match table pointer. */
    dt_driver_t *driver;                /**< Driver for the device. */
    void *private;                      /**< Private data for the driver. */

    /** State of the device. */
    enum {
        DT_DEVICE_UNINIT,               /**< Uninitialized. */
        DT_DEVICE_INIT,                 /**< Initializing. */
        DT_DEVICE_READY,                /**< Ready for use. */
        DT_DEVICE_FAILED,               /**< Initialization failed. */
    } state;
} dt_device_t;

extern dt_device_t *dt_device_get_by_offset(int node_offset, dt_driver_t *driver);
extern dt_device_t *dt_device_get_by_phandle(uint32_t phandle, dt_driver_t *driver);

extern void dt_device_probe(void);

/**
 * DT access utilities.
 */

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

extern bool dt_get_prop_u32(int node_offset, const char *name, uint32_t *_value);

extern int dt_match_impl(int node_offset, const void *data, size_t stride, size_t count);

/**
 * Match a DT node against an array of compatible strings. This is like
 * dt_is_compatible() but accepts an array of arbitrary structures where each
 * entry starts with a compatible string pointer, and also returns a match
 * index rather than a simple bool.
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
