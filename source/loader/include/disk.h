/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               Disk device management.
 */

#ifndef __DISK_H
#define __DISK_H

#include <device.h>

#ifdef CONFIG_TARGET_HAS_DISK

struct disk_device;

/** Partition map iteration callback function type.
 * @param disk          Disk containing the partition.
 * @param id            ID of the partition.
 * @param lba           Start LBA.
 * @param blocks        Size in blocks. */
typedef void (*partition_iterate_cb_t)(struct disk_device *disk, uint8_t id, uint64_t lba, uint64_t blocks);

/** Partition operations. */
typedef struct partition_ops {
    /** Iterate over the partitions on the device.
     * @param disk          Disk to iterate over.
     * @param cb            Callback function.
     * @return              Whether the device contained a partition map of
     *                      this type. */
    bool (*iterate)(struct disk_device *disk, partition_iterate_cb_t cb);
} partition_ops_t;

/** Define a builtin partition map type. */
#define BUILTIN_PARTITION_OPS(name) \
    static partition_ops_t name; \
    DEFINE_BUILTIN(BUILTIN_TYPE_PARTITION, name); \
    static partition_ops_t name

/** Types of disk devices (primarily used for naming purposes). */
typedef enum disk_type {
    DISK_TYPE_HD,                       /**< Hard drive/other. */
    DISK_TYPE_CDROM,                    /**< CDROM. */
    DISK_TYPE_FLOPPY,                   /**< Floppy drive. */
} disk_type_t;

/** Structure containing operations for a disk. */
typedef struct disk_ops {
    /** Read blocks from a disk.
     * @param disk          Disk device being read from.
     * @param buf           Buffer to read into.
     * @param count         Number of blocks to read.
     * @param lba           Block number to start reading from.
     * @return              Status code describing the result of the operation. */
    status_t (*read_blocks)(struct disk_device *disk, void *buf, size_t count, uint64_t lba);
} disk_ops_t;

/** Structure representing a disk device. */
typedef struct disk_device {
    device_t device;                    /**< Device header. */

    disk_type_t type;                   /**< Type of the disk. */
    const disk_ops_t *ops;              /**< Disk operations structure. */
    size_t block_size;                  /**< Size of a block on the disk. */
    uint64_t blocks;                    /**< Total number of blocks on the disk. */
    uint8_t id;                         /**< ID of the disk. */
} disk_device_t;

extern void disk_device_register(disk_device_t *disk);
extern void disk_device_probe(disk_device_t *disk);

#endif /* CONFIG_TARGET_HAS_DISK */
#endif /* __DISK_H */
