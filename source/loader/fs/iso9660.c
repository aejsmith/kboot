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

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <device.h>
#include <endian.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

#include "iso9660.h"

/** Structure containing details of an ISO9660 filesystem. */
typedef struct iso9660_mount {
    fs_mount_t mount;                   /**< Mount header. */

    int joliet_level;                   /**< Joliet level. */
} iso9660_mount_t;

/** Structure containing details of an ISO9660 handle. */
typedef struct iso9660_handle {
    fs_handle_t handle;                 /**< Handle header. */

    uint32_t data_len;                  /**< Data length. */
    uint32_t extent;                    /**< Extent block number. */
} iso9660_handle_t;

/** Convert a wide character to a multibyte sequence. */
static int utf8_wctomb(uint8_t *s, uint32_t wc, size_t max) {
    unsigned int bits = 0, j = 0, k;

    if (!s) {
        return wc >= 0x80;
    } else if (wc < 0x00000080) {
        *s = wc;
        return 1;
    }

    if (wc >= 0x04000000) {
        bits = 30;
        *s = 0xFC;
        j = 6;
    } else if (wc >= 0x00200000) {
        bits = 24;
        *s = 0xF8;
        j = 5;
    } else if (wc >= 0x00010000) {
        bits = 18;
        *s = 0xF0;
        j = 4;
    } else if (wc >= 0x00000800) {
        bits = 12;
        *s = 0xE0;
        j = 3;
    } else if (wc >= 0x00000080) {
        bits = 6;
        *s = 0xC0;
        j = 2;
    }

    if (j > max)
        return -1;

    *s |= (unsigned char)(wc >> bits);
    for (k = 1; k < j; k++) {
        bits -= 6;
        s[k] = 0x80 + ((wc >> bits) & 0x3f);
    }

    return k;
}

/** Convert big endian wide character string to UTF8. */
static int wcsntombs_be(uint8_t *s, uint8_t *pwcs, int inlen, int maxlen) {
    const uint8_t *ip;
    uint8_t *op;
    uint16_t c;
    int size;

    op = s;
    ip = pwcs;
    while ((*ip || ip[1]) && (maxlen > 0) && (inlen > 0)) {
        c = (*ip << 8) | ip[1];
        if (c > 0x7f) {
            size = utf8_wctomb(op, c, maxlen);
            if (size == -1) {
                maxlen--;
            } else {
                op += size;
                maxlen -= size;
            }
        } else {
            *op++ = (uint8_t)c;
        }

        ip += 2;
        inlen--;
    }

    return op - s;
}

/** Parse a name from a directory record.
 * @param record        Record to parse.
 * @param buf           Buffer to write into. */
static void iso9660_parse_name(iso9660_directory_record_t *record, char *buf) {
    uint32_t i, len;

    len = (record->file_ident_len < ISO9660_MAX_NAME_LEN)
        ? record->file_ident_len
        : ISO9660_MAX_NAME_LEN;

    for (i = 0; i < len; i++) {
        if (record->file_ident[i] == ISO9660_SEPARATOR2) {
            break;
        } else {
            buf[i] = tolower(record->file_ident[i]);
        }
    }

    if (i && buf[i - 1] == ISO9660_SEPARATOR1)
        i--;

    buf[i] = 0;
}

/** Parse a Joliet name from a directory record.
 * @param record        Record to parse.
 * @param buf           Buffer to write into. */
static void iso9660_parse_joliet_name(iso9660_directory_record_t *record, char *buf) {
    unsigned char len;

    len = wcsntombs_be((uint8_t *)buf, record->file_ident, record->file_ident_len >> 1, ISO9660_NAME_SIZE);
    if (len > 2 && buf[len - 2] == ';' && buf[len - 1] == '1')
        len -= 2;

    while (len >= 2 && buf[len - 1] == '.')
        len--;

    buf[len] = 0;
}

/** Generate a UUID.
 * @param pri           Primary volume descriptor.
 * @return              Pointer to allocated string for UUID. */
static char *iso9660_make_uuid(iso9660_primary_volume_desc_t *pri) {
    iso9660_timestamp_t *time;
    char *uuid;
    size_t i;

    /* If the modification time is set, then base the UUID off that, else use
     * the creation time. The ISO9660 says that a date is unset if all the
     * fields are '0' and the offset is 0. */
    time = &pri->vol_mod_time;
    for (i = 0; i < 16; i++) {
        if (((uint8_t *)time)[i] != 0)
            break;
    }
    if (i == 16 && time->offset == 0)
        time = &pri->vol_cre_time;

    /* Create the UUID string. */
    uuid = malloc(23);
    snprintf(uuid, 23, "%c%c%c%c-%c%c-%c%c-%c%c-%c%c-%c%c-%c%c",
        time->year[0], time->year[1], time->year[2], time->year[3],
        time->month[0], time->month[1], time->day[0], time->day[1],
        time->hour[0], time->hour[1], time->minute[0], time->minute[1],
        time->second[0], time->second[1], time->centisecond[0],
        time->centisecond[1]);
    return uuid;
}

/** Create a handle from a directory record.
 * @param mount         Mount the node is from.
 * @param record        Record to create from.
 * @return              Pointer to handle. */
static fs_handle_t *iso9660_handle_create(iso9660_mount_t *mount, iso9660_directory_record_t *record) {
    iso9660_handle_t *handle;

    handle = fs_handle_alloc(sizeof(*handle), &mount->mount);
    handle->handle.directory = record->file_flags & (1<<1);
    handle->data_len = le32_to_cpu(record->data_len_le);
    handle->extent = le32_to_cpu(record->extent_loc_le);
    return &handle->handle;
}

/** Mount an ISO9660 filesystem.
 * @param device        Device to mount.
 * @param _mount        Where to store pointer to mount structure.
 * @return              Status code describing the result of the operation. */
static status_t iso9660_mount(device_t *device, fs_mount_t **_mount) {
    iso9660_primary_volume_desc_t *desc __cleanup_free = NULL;
    iso9660_primary_volume_desc_t *primary __cleanup_free = NULL;
    iso9660_primary_volume_desc_t *supp __cleanup_free = NULL;
    iso9660_mount_t *mount;
    int joliet = 0;

    desc = malloc(ISO9660_BLOCK_SIZE);

    /* Read in volume descriptors until we find the primary descriptor. I don't
     * know whether there's a limit on the number of descriptors - I just put
     * in a sane one so we don't loop for ages. */
    for (size_t i = ISO9660_DATA_START; i < 128; i++) {
        status_t ret;

        ret = device_read(device, desc, ISO9660_BLOCK_SIZE, i * ISO9660_BLOCK_SIZE);
        if (ret != STATUS_SUCCESS)
            return ret;

        /* Check that the identifier is valid. */
        if (strncmp((char *)desc->header.ident, "CD001", 5) != 0)
            return STATUS_UNKNOWN_FS;

        if (desc->header.type == ISO9660_VOLUME_DESC_PRIMARY) {
            if (primary)
                return STATUS_CORRUPT_FS;

            primary = memdup(desc, sizeof(*primary));
        } else if (desc->header.type == ISO9660_VOLUME_DESC_SUPP) {
            if (supp)
                return STATUS_CORRUPT_FS;

            /* Determine whether Joliet is supported. */
            if (desc->esc_sequences[0] == 0x25 && desc->esc_sequences[1] == 0x2f) {
                if (desc->esc_sequences[2] == 0x40) {
                    joliet = 1;
                } else if (desc->esc_sequences[2] == 0x43) {
                    joliet = 2;
                } else if (desc->esc_sequences[2] == 0x45) {
                    joliet = 3;
                } else {
                    continue;
                }

                supp = memdup(desc, sizeof(*supp));
            }
        } else if (desc->header.type == ISO9660_VOLUME_DESC_END) {
            break;
        }
    }

    /* Check whether a descriptor was found. */
    if (!primary)
        return STATUS_UNKNOWN_FS;

    mount = malloc(sizeof(*mount));
    mount->joliet_level = joliet;

    /* Store the filesystem label and UUID. */
    primary->vol_ident[31] = 0;
    primary->sys_ident[31] = 0;
    mount->mount.uuid = iso9660_make_uuid(primary);
    mount->mount.label = strdup(strstrip((char *)primary->vol_ident));

    /* Retreive the root node. */
    mount->mount.root = iso9660_handle_create(mount, (supp)
        ? (iso9660_directory_record_t *)&supp->root_dir_record
        : (iso9660_directory_record_t *)&primary->root_dir_record);

    dprintf("iso9660: mounted %s ('%s') (uuid: %s)\n", device->name, mount->mount.label, mount->mount.uuid);
    *_mount = &mount->mount;
    return STATUS_SUCCESS;
}

/** Read from an ISO9660 handle.
 * @param _handle       Handle to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset to read from.
 * @return              Status code describing the result of the operation. */
static status_t iso9660_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    iso9660_handle_t *handle = (iso9660_handle_t *)_handle;

    if ((uint64_t)offset + count > (uint64_t)handle->data_len)
        return STATUS_END_OF_FILE;

    offset += (offset_t)handle->extent * ISO9660_BLOCK_SIZE;
    return device_read(handle->handle.mount->device, buf, count, offset);
}

/** Get the size of an ISO9660 file.
 * @param _handle       Handle to the file.
 * @return              Size of the file. */
static offset_t iso9660_size(fs_handle_t *_handle) {
    iso9660_handle_t *handle = (iso9660_handle_t *)_handle;
    return handle->data_len;
}

/** Iterate over directory entries.
 * @param _handle       Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
static status_t iso9660_iterate(fs_handle_t *_handle, dir_iterate_cb_t cb, void *arg) {
    iso9660_handle_t *handle = (iso9660_handle_t *)_handle;
    iso9660_mount_t *mount = (iso9660_mount_t *)_handle->mount;
    char *buf __cleanup_free;
    uint32_t offset;
    status_t ret;

    /* Read in all the directory data. */
    buf = malloc(handle->data_len);
    ret = iso9660_read(_handle, buf, handle->data_len, 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Iterate through each entry. */
    offset = 0;
    while (offset < handle->data_len) {
        iso9660_directory_record_t *record = (iso9660_directory_record_t *)(buf + offset);
        char name[ISO9660_NAME_SIZE];
        fs_handle_t *child;
        bool done;

        if (record->rec_len) {
            offset += record->rec_len;
        } else {
            /* A zero record length means we should move on to the next block.
             * If this is the end, this will cause us to break out of the while
             * loop if offset becomes >= data_len. */
            offset = round_up(offset, ISO9660_BLOCK_SIZE);
            continue;
        }

        /* Bit 0 indicates that this is not a user-visible record. */
        if (record->file_flags & (1<<0))
            continue;

        /* If this is a directory, check for '.' and '..'. */
        name[0] = 0;
        if (record->file_flags & (1<<1) && record->file_ident_len == 1) {
            if (record->file_ident[0] == 0) {
                snprintf(name, sizeof(name), ".");
            } else if (record->file_ident[0] == 1) {
                snprintf(name, sizeof(name), "..");
            }
        }

        if (!name[0]) {
            /* Parse the name based on the Joliet level. */
            if (mount->joliet_level) {
                iso9660_parse_joliet_name(record, name);
            } else {
                iso9660_parse_name(record, name);
            }
        }

        child = iso9660_handle_create(mount, record);
        done = !cb(name, child, arg);
        fs_handle_release(child);
        if (done)
            break;
    }

    return STATUS_SUCCESS;
}

/** ISO9660 filesystem operations structure. */
BUILTIN_FS_OPS(iso9660_fs_ops) = {
    .mount = iso9660_mount,
    .read = iso9660_read,
    .size = iso9660_size,
    .iterate = iso9660_iterate,
};
