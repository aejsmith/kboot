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
    const char *name;                   /**< Name of the partition scheme. */

    /** Iterate over the partitions on the device.
     * @param disk          Disk to iterate over.
     * @param cb            Callback function.
     * @return              Whether the device contained a partition map of
     *                      this type. */
    bool (*iterate)(struct disk_device *disk, partition_iterate_cb_t cb);
} partition_ops_t;

/** Define a builtin partition type. */
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

    /** Check if a partition is the boot partition.
     * @param disk          Disk the partition is on.
     * @param id            ID of partition.
     * @param lba           Block that the partition starts at.
     * @return              Whether partition is a boot partition. */
    bool (*is_boot_partition)(struct disk_device *disk, uint8_t id, uint64_t lba);

    /** Get identification information for the device.
     * @param disk          Disk to identify.
     * @param type          Type of the information to get.
     * @param buf           Where to store identification string.
     * @param size          Size of the buffer. */
    void (*identify)(struct disk_device *disk, device_identify_t type, char *buf, size_t size);
} disk_ops_t;

/** Structure representing a disk device. */
typedef struct disk_device {
    device_t device;                    /**< Device header. */

    disk_type_t type;                   /**< Type of the disk. */
    const disk_ops_t *ops;              /**< Disk operations structure. */
    size_t block_size;                  /**< Size of a block on the disk. */
    uint64_t blocks;                    /**< Total number of blocks on the disk. */
    uint8_t id;                         /**< ID of the disk. */

    /** Partitioning information. */
    struct disk_device *parent;         /**< Parent disk, or NULL if this is the raw disk. */
    union {
        struct {
            list_t partitions;          /**< List of partitions. */

            /** Partitioning scheme used on the disk. */
            partition_ops_t *partition_ops;
        } raw;
        struct {
            list_t link;                /**< Link to parent's partition list. */
            uint64_t offset;            /**< LBA offset of the partition. */
        } partition;
    };
} disk_device_t;

/** Return whether a disk device is a partition.
 * @param disk          Disk to check.
 * @return              Whether the disk is a partition. */
static inline bool disk_device_is_partition(disk_device_t *disk) {
    return !!disk->parent;
}

extern void disk_device_register(disk_device_t *disk, bool boot);

#endif /* CONFIG_TARGET_HAS_DISK */
#endif /* __DISK_H */
