/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               Device management.
 */

#ifndef __DEVICE_H
#define __DEVICE_H

#include <lib/list.h>

#include <status.h>

struct device;

/** Types of devices. */
typedef enum device_type {
    DEVICE_TYPE_DISK,                   /**< Local disk. */
    DEVICE_TYPE_NET,                    /**< Network device. */
    DEVICE_TYPE_VIRTUAL,                /**< Virtual device (e.g. boot image). */
} device_type_t;

/** Device identification information to get. */
typedef enum device_identify {
    /** Short identification string shown in device list. */
    DEVICE_IDENTIFY_SHORT,

    /** Long (multi-line) information given by lsdev for a specific device. */
    DEVICE_IDENTIFY_LONG,
} device_identify_t;

/** Device operations structure. */
typedef struct device_ops {
    /** Read from a device.
     * @param device        Device to read from.
     * @param buf           Buffer to read into.
     * @param count         Number of bytes to read.
     * @param offset        Offset in the device to read from.
     * @return              Status code describing the result of the read. */
    status_t (*read)(struct device *device, void *buf, size_t count, offset_t offset);

    /** Get identification information for the device.
     * @param device        Device to identify.
     * @param type          Type of the information to get.
     * @param buf           Where to store identification string.
     * @param size          Size of the buffer. */
    void (*identify)(struct device *device, device_identify_t type, char *buf, size_t size);
} device_ops_t;

/** Base device structure (embedded by device type structures). */
typedef struct device {
    list_t header;                      /**< Link to devices list. */

    device_type_t type;                 /**< Type of the device. */
    const device_ops_t *ops;            /**< Operations for the device (can be NULL). */

    const char *name;                   /**< Name of the device. */
    struct fs_mount *mount;             /**< Filesystem on the device. */
} device_t;

extern device_t *boot_device;

extern status_t device_read(device_t *device, void *buf, size_t count, offset_t offset);

extern device_t *device_lookup(const char *name);
extern void device_register(device_t *device);

extern void target_device_probe(void);

extern void device_init(void);

#endif /* __DEVICE_H */
