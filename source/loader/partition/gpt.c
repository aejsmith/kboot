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
 * @brief               GPT partition table support.
 */

#include <lib/string.h>

#include <disk.h>
#include <endian.h>
#include <loader.h>
#include <memory.h>

#include "gpt.h"
#include "mbr.h"

/** Zero GUID (for easy comparison). */
static gpt_guid_t zero_guid;

/** Iterate over the partitions on a device.
 * @param disk          Disk to iterate over.
 * @param cb            Callback function.
 * @return              Whether the device contained a GPT partition table. */
static bool gpt_partition_iterate(disk_device_t *disk, partition_iterate_cb_t cb) {
    void *buf __cleanup_free = NULL;
    mbr_t *mbr;
    gpt_header_t *header;
    uint64_t offset;
    uint32_t num_entries, entry_size;

    /* Allocate a temporary buffer. */
    buf = malloc(disk->block_size);

    /* GPT requires a protective MBR in the first block. Read this in first and
     * check that it contains a protective GPT partition. If we have a legacy
     * MBR then let it be handled through the MBR code. Note that on some
     * systems (e.g. Macs) we can have a "hybrid MBR" where we have both a
     * valid (non-protective) MBR and a GPT. In this case we will use the MBR,
     * since the two should be in sync. */
    mbr = buf;
    if (device_read(&disk->device, mbr, disk->block_size, 0) != STATUS_SUCCESS) {
        return false;
    } else if (mbr->signature != MBR_SIGNATURE || mbr->partitions[0].type != MBR_PARTITION_TYPE_GPT) {
        return false;
    }

    /* Read in the GPT header (second block). At most one block in size. */
    mbr = NULL;
    header = buf;
    if (device_read(&disk->device, header, disk->block_size, disk->block_size) != STATUS_SUCCESS) {
        return false;
    } else if (le64_to_cpu(header->signature) != GPT_HEADER_SIGNATURE) {
        return false;
    }

    /* Pull needed information out of the header. */
    offset = le64_to_cpu(header->partition_entry_lba) * disk->block_size;
    num_entries = le32_to_cpu(header->num_partition_entries);
    entry_size = le32_to_cpu(header->partition_entry_size);
    header = NULL;

    /* Iterate over partition entries. */
    for (uint32_t i = 0; i < num_entries; i++, offset += entry_size) {
        gpt_partition_entry_t *entry = buf;
        uint64_t lba, count;

        if (device_read(&disk->device, buf, entry_size, offset) != STATUS_SUCCESS) {
            dprintf("disk: failed to read GPT partition entry at %" PRIu64 "\n", offset);
            return false;
        }

        /* Ignore unused entries. */
        if (memcmp(&entry->type_guid, &zero_guid, sizeof(entry->type_guid)) == 0)
            continue;

        lba = le64_to_cpu(entry->start_lba);
        count = (le64_to_cpu(entry->last_lba) - lba) + 1;

        if (lba >= disk->blocks || lba + count > disk->blocks) {
            dprintf("disk: warning: GPT partition %" PRIu32 " outside range of device", i);
            continue;
        }

        dprintf("disk: GPT partition %" PRIu32 " (type: %pu, lba: %" PRIu64 ", count: %" PRIu64 ")\n",
            i, &entry->type_guid, lba, count);

        cb(disk, i, lba, count);
    }

    return true;
}

/** GPT partition map type. */
BUILTIN_PARTITION_OPS(gpt_partition_ops) = {
    .iterate = gpt_partition_iterate,
};
