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
 * @brief               FAT filesystem support.
 */

#ifndef __FS_FAT_H
#define __FS_FAT_H

#include <types.h>

/** FAT BIOS Parameter Block structure. */
typedef struct fat_bpb {
    uint8_t jump[3];                        /**< Code to jump over BPB. */
    uint8_t oem_id[8];                      /**< OEM identifier. */
    uint16_t bytes_per_sector;              /**< Number of bytes per sector. */
    uint8_t sectors_per_cluster;            /**< Number of sectors per cluster. */
    uint16_t num_reserved_sectors;          /**< Number of reserved sectors. */
    uint8_t num_fats;                       /**< Number of File Allocation Tables. */
    uint16_t num_root_entries;              /**< Number of root directory entries. */
    uint16_t total_sectors_16;              /**< Total sector count (FAT12/16). */
    uint8_t media;                          /**< Media type. */
    uint16_t sectors_per_fat_16;            /**< Number of sectors per FAT (FAT12/16). */
    uint16_t sectors_per_track;             /**< Number of sectors per track. */
    uint16_t num_heads;                     /**< Number of heads. */
    uint32_t num_hidden_sectors;            /**< Number of hidden sectors. */
    uint32_t total_sectors_32;              /**< Total sector count (32-bit). */

    /** Extended information (FAT16/32). */
    union {
        struct {
            uint8_t drive_num;              /**< BIOS drive number. */
            uint8_t _reserved1;
            uint8_t boot_sig;               /**< Boot signature. */
            uint32_t volume_serial;         /**< Volume serial number. */
            uint8_t volume_label[11];       /**< Volume label. */
            uint8_t fs_type[8];             /**< Filesystem type string. */
        } __packed fat16;

        struct {
            uint32_t sectors_per_fat_32;    /**< Number of sectors per FAT (FAT32). */
            uint16_t ext_flags;             /**< Extended flags. */
            uint16_t fs_version;            /**< Filesystem version. */
            uint32_t root_cluster;          /**< Root cluster number. */
            uint16_t fs_info;               /**< Sector number of FS info structure. */
            uint16_t backup_boot_sector;    /**< Sector number of a copy of the boot record. */
            uint8_t _reserved1[12];
            uint8_t drive_num;              /**< BIOS drive number. */
            uint8_t _reserved2;
            uint8_t boot_sig;               /**< Boot signature. */
            uint32_t volume_serial;         /**< Volume serial number. */
            uint8_t volume_label[11];       /**< Volume label. */
            uint8_t fs_type[8];             /**< Filesystem type string. */
        } __packed fat32;
    } __packed;
} __packed fat_bpb_t;

/** FAT directory entry structure. */
typedef struct fat_dir_entry {
    uint8_t name[11];                       /**< File name and extension. */
    uint8_t attributes;                     /**< File attributes. */
    uint8_t case_info;                      /**< Case information. */
    uint8_t creation_time_fine;             /**< Fine resolution creation time (10 ms units). */
    uint16_t creation_time;                 /**< Creation time. */
    uint16_t creation_date;                 /**< Creation date. */
    uint16_t access_date;                   /**< Last access date. */
    uint16_t first_cluster_high;            /**< High bytes of the first cluster number (FAT32). */
    uint16_t modified_time;                 /**< Last modified time. */
    uint16_t modified_date;                 /**< Last modified date. */
    uint16_t first_cluster_low;             /**< Low bytes of the first cluster number. */
    uint32_t file_size;                     /**< Size of the file. */
} __packed fat_dir_entry_t;

/** VFAT LFN directory entry structure. */
typedef struct fat_lfn_dir_entry {
    uint8_t id;                             /**< Sequence number. */
    uint16_t name1[5];                      /**< First 5 UCS-2 name characters. */
    uint8_t attributes;                     /**< File attributes. */
    uint8_t _reserved;
    uint8_t checksum;                       /**< Checksum of file name. */
    uint16_t name2[6];                      /**< Next 6 UCS-2 name characters. */
    uint16_t first_cluster;                 /**< First cluster (always 0). */
    uint16_t name3[2];                      /**< Final 2 UCS-2 name characters. */
} __packed fat_lfn_dir_entry_t;

/** Maximum length of a FAT filename (LFN). */
#define FAT_NAME_MAX                255

/** Value used to indicate a deleted directory entry. */
#define FAT_DIR_ENTRY_DELETED       0xe5

/** Case information in directory entry. */
#define FAT_CASE_NAME_LOWER         (1<<3)
#define FAT_CASE_EXT_LOWER          (1<<4)

/** FAT file attributes. */
#define FAT_ATTRIBUTE_READ_ONLY     (1<<0)
#define FAT_ATTRIBUTE_HIDDEN        (1<<1)
#define FAT_ATTRIBUTE_SYSTEM        (1<<2)
#define FAT_ATTRIBUTE_VOLUME_ID     (1<<3)
#define FAT_ATTRIBUTE_DIRECTORY     (1<<4)
#define FAT_ATTRIBUTE_ARCHIVE       (1<<5)

/** Valid FAT attributes. */
#define FAT_ATTRIBUTE_VALID \
    (FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM \
        | FAT_ATTRIBUTE_VOLUME_ID | FAT_ATTRIBUTE_DIRECTORY | FAT_ATTRIBUTE_ARCHIVE)

/** Attributes indicating a long file name. */
#define FAT_ATTRIBUTE_LONG_NAME \
    (FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID)

#endif /* __FS_FAT_H */
