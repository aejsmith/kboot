/*
 * Copyright (C) 2010-2015 Alex Smith
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
 * @brief               ISO9660 filesystem support.
 */

#ifndef __FS_ISO9660_H
#define __FS_ISO9660_H

#include <types.h>

/** ISO9660 identifier. */
#define ISO9660_IDENTIFIER              "CD001"

/** Size of an ISO9660 block. */
#define ISO9660_BLOCK_SIZE              2048

/** First sector of the Data Area. */
#define ISO9660_DATA_START              16

/** Maximum file name length. */
#define ISO9660_MAX_NAME_LEN            31

/** Maximum Joliet file name length. */
#define ISO9660_JOLIET_MAX_NAME_LEN     64

/** Identifier string separators. */
#define ISO9660_SEPARATOR1              0x2e    /**< Seperator 1 (.). */
#define ISO9660_SEPARATOR2              0x3b    /**< Seperator 2 (;). */

/** Volume Descriptor type values. */
#define ISO9660_VOLUME_DESC_BOOT        0       /**< Boot Record. */
#define ISO9660_VOLUME_DESC_PRIMARY     1       /**< Primary Volume Descriptor. */
#define ISO9660_VOLUME_DESC_SUPP        2       /**< Supplementary Volume Descriptor. */
#define ISO9660_VOLUME_DESC_PARTITION   3       /**< Volume Partition Descriptor. */
#define ISO9660_VOLUME_DESC_END         255     /**< Volume Descriptor Set Terminator. */

/** Date and Time Format (ECMA-119 Page 21). */
typedef struct iso9660_timestamp {
    uint8_t year[4];                            /**< Year. */
    uint8_t month[2];                           /**< Month of year (1-12). */
    uint8_t day[2];                             /**< Day of month (1-31). */
    uint8_t hour[2];                            /**< Hour (0-23). */
    uint8_t minute[2];                          /**< Minute (0-59). */
    uint8_t second[2];                          /**< Second (0-59). */
    uint8_t centisecond[2];                     /**< Hundredths of second (0-99). */
    uint8_t offset;                             /**< Offset from GMT. */
} __packed iso9660_timestamp_t;

/** Recording Date and Time Format (ECMA-119 Page 28). */
typedef struct iso9660_dir_timestamp {
    uint8_t year;                               /**< Years since 1900. */
    uint8_t month;                              /**< Month of year (1-12). */
    uint8_t day;                                /**< Day of month (1-31). */
    uint8_t hour;                               /**< Hour (0-23). */
    uint8_t minute;                             /**< Minute (0-59). */
    uint8_t second;                             /**< Second (0-59). */
    uint8_t offset;                             /**< Offset from GMT. */
} __packed iso9660_dir_timestamp_t;

/** Header of a Volume Descriptor (ECMA-119 Page 15). */
typedef struct iso9660_volume_desc {
    uint8_t type;                               /**< Volume Descriptor Type. */
    uint8_t ident[5];                           /**< Standard Identifier. */
    uint8_t version;                            /**< Volume Descriptor Version. */
} __packed iso9660_volume_desc_t;

/**
 * Primary/Supplementary Volume Descriptor (ECMA-119 Page 17/22).
 *
 * The structure of the primary and supplementary volume descriptors are almost
 * identical, except that supplementary includes escape sequences in place of
 * an unused field in the primary descriptor. Therefore we reuse the same
 * structure for both.
 */
typedef struct iso9660_primary_volume_desc {
    iso9660_volume_desc_t header;               /**< Volume descriptor header. */

    uint8_t unused1;                            /**< Unused Field. */
    uint8_t sys_ident[32];                      /**< System Identifier. */
    uint8_t vol_ident[32];                      /**< Volume Identifier. */
    uint8_t unused2[8];                         /**< Unused Field. */
    uint32_t vol_space_size_le;                 /**< Volume Space Size (LE). */
    uint32_t vol_space_size_be;                 /**< Volume Space Size (BE). */
    uint8_t esc_sequences[32];                  /**< Escape Sequences. */
    uint16_t vol_set_size_le;                   /**< Volume Set Size (LE). */
    uint16_t vol_set_size_be;                   /**< Volume Set Size (BE). */
    uint16_t vol_seq_num_le;                    /**< Volume Sequence Number (LE). */
    uint16_t vol_seq_num_be;                    /**< Volume Sequence Number (BE). */
    uint16_t logical_block_size_le;             /**< Logical Block Size (LE). */
    uint16_t logical_block_size_be;             /**< Logical Block Size (BE). */
    uint32_t path_table_size_le;                /**< Path Table Size (LE). */
    uint32_t path_table_size_be;                /**< Path Table Size (BE). */
    uint32_t typel_path_tbl_occur;              /**< Location of Occurrence of Type L Path Table (LE). */
    uint32_t typel_path_tbl_option_occur;       /**< Location of Optional Occurrence of Type L Path Table (LE). */
    uint32_t typem_path_tbl_occur;              /**< Location of Occurrence of Type M Path Table (BE). */
    uint32_t typem_path_tbl_option_occur;       /**< Location of Optional Occurrence of Type M Path Table (BE). */
    uint8_t root_dir_record[34];                /**< Directory Record for Root Directory. */
    uint8_t vol_set_ident[128];                 /**< Volume Set Identifier. */
    uint8_t publisher_ident[128];               /**< Publisher Identifier. */
    uint8_t data_preparer_ident[128];           /**< Data Preparer Identifier. */
    uint8_t application_ident[128];             /**< Application Identifier. */
    uint8_t copyright_file_ident[37];           /**< Copyright File Identififer. */
    uint8_t abstract_file_ident[37];            /**< Abstract File Identifier. */
    uint8_t biblio_file_ident[37];              /**< Bibliographic File Identifier. */
    iso9660_timestamp_t vol_cre_time;           /**< Volume Creation Date and Time. */
    iso9660_timestamp_t vol_mod_time;           /**< Volume Modification Date and Time. */
    iso9660_timestamp_t vol_exp_time;           /**< Volume Expiration Date and Time. */
    iso9660_timestamp_t vol_eff_time;           /**< Volume Effective Date and Time. */
    uint8_t file_struct_ver;                    /**< File Structure Version. */
    uint8_t reserved1;                          /**< Reserved. */
    uint8_t application_use[512];               /**< Application Use. */
    uint8_t reserved2[653];                     /**< Reserved. */
} __packed iso9660_primary_volume_desc_t;

/** Directory Record (ECMA-119 Page 27). */
typedef struct iso9660_directory_record {
    uint8_t rec_len;                            /**< Length of Directory Record. */
    uint8_t ext_attr_rec_len;                   /**< Extended Attribute Record Length. */
    uint32_t extent_loc_le;                     /**< Location of Extent (LE). */
    uint32_t extent_loc_be;                     /**< Location of Extent (BE). */
    uint32_t data_len_le;                       /**< Data Length (LE). */
    uint32_t data_len_be;                       /**< Data Length (BE). */
    iso9660_dir_timestamp_t time;               /**< Recording Date and Time. */
    uint8_t file_flags;                         /**< File Flags. */
    uint8_t file_unit_size;                     /**< File Unit Size. */
    uint8_t interleave_gap_size;                /**< Interleave Gap Size. */
    uint16_t vol_seq_num_le;                    /**< Volume Sequence Number (LE). */
    uint16_t vol_seq_num_be;                    /**< Volume Sequence Number (BE). */
    uint8_t file_ident_len;                     /**< Length of File Identifier. */
    uint8_t file_ident[];                       /**< File Identifier. */
} __packed iso9660_directory_record_t;

#endif /* __FS_ISO9660_H */
