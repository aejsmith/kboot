/*
 * Copyright (C) 2012-2014 Alex Smith
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

#endif /* __BIOS_MEMORY_H */
