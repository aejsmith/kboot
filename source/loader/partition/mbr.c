/*
 * Copyright (C) 2009-2012 Alex Smith
 * Copyright (C) 2012 Daniel Collins
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
 * @brief               MBR partition support.
 */

#include <lib/utility.h>

#include <disk.h>
#include <endian.h>
#include <loader.h>
#include <memory.h>

#include "mbr.h"

/** Read in an MBR and convert endianness.
 * @param disk          Disk to read from.
 * @param mbr           MBR to read into.
 * @param lba           LBA to read from.
 * @return              Whether read successfully. */
static bool read_mbr(disk_device_t *disk, mbr_t *mbr, uint32_t lba) {
    if (device_read(&disk->device, mbr, sizeof(*mbr), (uint64_t)lba * disk->block_size) != STATUS_SUCCESS)
        return false;

    for (size_t i = 0; i < array_size(mbr->partitions); i++) {
        mbr->partitions[i].start_lba = le32_to_cpu(mbr->partitions[i].start_lba);
        mbr->partitions[i].num_sectors = le32_to_cpu(mbr->partitions[i].num_sectors);
    }

    return true;
}

/** Check whether a partition is valid.
 * @param disk          Disk containing partition record.
 * @param partition     Partition record.
 * @return              Whether the partition is valid. */
static bool is_valid(disk_device_t *disk, mbr_partition_t *partition) {
    return (partition->type != 0)
        && (partition->bootable == 0 || partition->bootable == 0x80)
        && (partition->start_lba < disk->blocks)
        && (partition->start_lba + partition->num_sectors <= disk->blocks);
}

/** Check whether a partition is an extended partition.
 * @param partition     Partition record.
 * @return              Whether the partition is extended. */
static bool is_extended(mbr_partition_t *partition) {
    switch (partition->type) {
    case 0x5:
    case 0xf:
    case 0x85:
        /* These are different types of extended partition, 0x5 is supposedly
         * with CHS addressing, while 0xF is LBA. However, Linux treats them the
         * exact same way. */
        return true;
    default:
        return false;
    }
}

/** Iterate over an extended partition.
 * @param disk      Disk that the partition is on.
 * @param lba       LBA of the extended partition.
 * @param cb        Callback function. */
static void handle_extended(disk_device_t *disk, uint32_t lba, partition_iterate_cb_t cb) {
    mbr_t *ebr;
    size_t i = 4;

    ebr = malloc(sizeof(*ebr));

    for (uint32_t curr_ebr = lba, next_ebr = 0; curr_ebr; curr_ebr = next_ebr) {
        mbr_partition_t *partition, *next;

        if (!read_mbr(disk, ebr, curr_ebr)) {
            dprintf("disk: failed to read EBR at %" PRIu32 "\n", curr_ebr);
            break;
        } else if (ebr->signature != MBR_SIGNATURE) {
            dprintf("disk: warning: invalid EBR, corrupt partition table\n");
            break;
        }

        /* First entry contains the logical partition, second entry refers to
         * the next EBR, forming a linked list of EBRs. */
        partition = &ebr->partitions[0];
        next = &ebr->partitions[1];

        /* Calculate the location of the next EBR. The start sector is relative
         * to the start of the extended partition. Set to 0 if the second
         * partition doesn't refer to another EBR entry, causes the loop to end. */
        next->start_lba += lba;
        next_ebr = (is_valid(disk, next) && is_extended(next) && next->start_lba > curr_ebr)
            ? next->start_lba
            : 0;

        /* Get the partition. Here the start sector is relative to the current
         * EBR's location. */
        partition->start_lba += curr_ebr;
        if (!is_valid(disk, partition))
            continue;

        dprintf("disk: logical MBR partition %zu (type: 0x%" PRIu8 ", lba: %" PRIu32 ", count: %" PRIu32 ")\n",
            i, partition->type, partition->start_lba, partition->num_sectors);

        cb(disk, i++, partition->start_lba, partition->num_sectors);
    }

    free(ebr);
}

/** Iterate over the partitions on a device.
 * @param disk          Disk to iterate over.
 * @param cb            Callback function.
 * @return              Whether the device contained an MBR partition table. */
static bool mbr_partition_iterate(disk_device_t *disk, partition_iterate_cb_t cb) {
    mbr_t *mbr;
    bool seen_extended;

    /* Read in the MBR, which is in the first block on the device. */
    mbr = malloc(sizeof(*mbr));
    if (!read_mbr(disk, mbr, 0) || mbr->signature != MBR_SIGNATURE) {
        free(mbr);
        return false;
    }

    /* Check if this is a GPT partition table (technically we should not get
     * here if this is a GPT disk as the GPT code should be reached first). This
     * is just a safeguard. */
    if (mbr->partitions[0].type == MBR_PARTITION_TYPE_GPT) {
        free(mbr);
        return false;
    }

    /* Loop through all partitions in the table. */
    seen_extended = false;
    for (size_t i = 0; i < array_size(mbr->partitions); i++) {
        mbr_partition_t *partition = &mbr->partitions[i];

        if (!is_valid(disk, partition))
            continue;

        if (is_extended(partition)) {
            if (seen_extended) {
                dprintf("disk: warning: ignoring multiple extended partitions\n");
                continue;
            }

            handle_extended(disk, partition->start_lba, cb);
            seen_extended = true;
        } else {
            dprintf("disk: primary MBR partition %zu (type: 0x%" PRIu8 ", lba: %" PRIu32 ", count: %" PRIu32 ")\n",
                i, partition->type, partition->start_lba, partition->num_sectors);

            cb(disk, i, partition->start_lba, partition->num_sectors);
        }
    }

    free(mbr);
    return true;
}

/** MBR partition map type. */
BUILTIN_PARTITION_OPS(mbr_partition_ops) = {
    .iterate = mbr_partition_iterate,
};
