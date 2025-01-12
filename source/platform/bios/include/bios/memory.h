/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               INT 15h interface definitions.
 */

#ifndef __BIOS_MEMORY_H
#define __BIOS_MEMORY_H

#include <types.h>

/** Value of EAX for the E820 call ('SMAP'). */
#define E820_SMAP               0x534d4150

/** Memory map type values. */
#define E820_TYPE_FREE          1       /**< Usable memory. */
#define E820_TYPE_RESERVED      2       /**< Reserved memory. */
#define E820_TYPE_ACPI_RECLAIM  3       /**< ACPI reclaimable. */
#define E820_TYPE_ACPI_NVS      4       /**< ACPI NVS. */
#define E820_TYPE_BAD           5       /**< Bad memory. New in ACPI 3.0. */
#define E820_TYPE_DISABLED      6       /**< Address range is disabled. New in ACPI 4.0. */

/** Attribute bit values. */
#define E820_ATTR_ENABLED       (1<<0)  /**< Address range is enabled. New in 3.0. In ACPI 4.0+, must be 1. */
#define E820_ATTR_NVS           (1<<1)  /**< Address range is nonvolatile. */
#define E820_ATTR_SLOW          (1<<2)  /**< Access to this memory may be very slow. New in ACPI 4.0. */
#define E820_ATTR_ERRORLOG      (1<<3)  /**< Memory is used to log hardware errors. New in ACPI 4.0. */

/** E820 memory map entry structure. */
typedef struct e820_entry {
    uint64_t start;                     /**< Start of range. */
    uint64_t length;                    /**< Length of range. */
    uint32_t type;                      /**< Type of range. */
    uint32_t attr;                      /**< Attributes of range. */
} __packed e820_entry_t;

extern void bios_memory_get_mmap(void **_buf, size_t *_num_entries, size_t *_entry_size);

#endif /* __BIOS_MEMORY_H */
