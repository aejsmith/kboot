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

#ifndef __PARTITION_GPT_H
#define __PARTITION_GPT_H

#include <types.h>

/** GPT/EFI GUID structure. */
typedef struct gpt_guid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4;
    uint8_t data5;
    uint8_t data6;
    uint8_t data7;
    uint8_t data8;
    uint8_t data9;
    uint8_t data10;
    uint8_t data11;
} __aligned(8) gpt_guid_t;

/** GPT header. */
typedef struct gpt_header {
    uint64_t signature;                 /**< Signature (ASCII "EFI PART"). */
    uint32_t revision;                  /**< Revision number. */
    uint32_t header_size;               /**< Size of the GPT header. */
    uint32_t header_crc32;              /**< CRC32 checksum of the GPT header. */
    uint32_t reserved;
    uint64_t my_lba;                    /**< LBA of this GPT header. */
    uint64_t alternate_lba;             /**< LBA of the alternate GPT header. */
    uint64_t first_usable_lba;          /**< First usable LBA for a partition. */
    uint64_t last_usable_lba;           /**< Last usable LBA for a partition. */
    gpt_guid_t disk_guid;               /**< GUID of the disk. */
    uint64_t partition_entry_lba;       /**< Starting LBA of the partition entry array. */
    uint32_t num_partition_entries;     /**< Number of entries in the partition entry array. */
    uint32_t partition_entry_size;      /**< Size of each partition entry array entry. */
    uint32_t partition_entry_crc32;     /**< CRC32 of the partition entry array. */
} gpt_header_t;

/** GPT signature. */
#define GPT_HEADER_SIGNATURE    0x5452415020494645ull

/** GPT partition entry. */
typedef struct gpt_partition_entry {
    gpt_guid_t type_guid;               /**< Partition type GUID. */
    gpt_guid_t partition_guid;          /**< Unique partition GUID. */
    uint64_t start_lba;                 /**< Start LBA. */
    uint64_t last_lba;                  /**< Last LBA. */
    uint64_t attributes;                /**< Partition attributes. */
    uint16_t partition_name[72 / 2];    /**< Partition name (null-terminated). */
} gpt_partition_entry_t;

#endif /* __PARTITION_GPT_H */
