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

#include <fs/iso9660.h>

#include <lib/charset.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <device.h>
#include <endian.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** Structure containing details of an ISO9660 filesystem. */
typedef struct iso9660_mount {
    fs_mount_t mount;                   /**< Mount header. */
    int joliet_level;                   /**< Joliet level. */
} iso9660_mount_t;

/** Structure containing details of an ISO9660 handle. */
typedef struct iso9660_handle {
    fs_handle_t handle;                 /**< Handle header. */
    uint32_t extent;                    /**< Extent block number. */
} iso9660_handle_t;

/** Structure containing details of an ISO9660 entry. */
typedef struct iso9660_entry {
    fs_entry_t entry;                   /**< Entry header. */
    iso9660_directory_record_t *record; /**< Directory record for the entry. */
} iso9660_entry_t;

/** Read from an ISO9660 handle.
 * @param _handle       Handle to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset to read from.
 * @return              Status code describing the result of the operation. */
static status_t iso9660_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    iso9660_handle_t *handle = (iso9660_handle_t *)_handle;

    offset += (offset_t)handle->extent * ISO9660_BLOCK_SIZE;
    return device_read(handle->handle.mount->device, buf, count, offset);
}

/** Create a handle from a directory record.
 * @param mount         Mount the node is from.
 * @param record        Record to create from.
 * @return              Pointer to handle. */
static fs_handle_t *open_record(iso9660_mount_t *mount, iso9660_directory_record_t *record) {
    iso9660_handle_t *handle;

    handle = malloc(sizeof(*handle));

    fs_handle_init(
        &handle->handle, &mount->mount,
        (record->file_flags & (1 << 1)) ? FILE_TYPE_DIR : FILE_TYPE_REGULAR,
        le32_to_cpu(record->data_len_le));

    handle->extent = le32_to_cpu(record->extent_loc_le);

    return &handle->handle;
}

/** Open an entry on a ISO9660 filesystem.
 * @param _entry        Entry to open (obtained via iterate()).
 * @param _handle       Where to store pointer to opened handle.
 * @return              Status code describing the result of the operation. */
static status_t iso9660_open_entry(const fs_entry_t *_entry, fs_handle_t **_handle) {
    iso9660_entry_t *entry = (iso9660_entry_t *)_entry;
    iso9660_handle_t *owner = (iso9660_handle_t *)_entry->owner;
    iso9660_handle_t *root = (iso9660_handle_t *)_entry->owner->mount->root;
    iso9660_mount_t *mount = (iso9660_mount_t *)_entry->owner->mount;
    uint32_t extent = le32_to_cpu(entry->record->extent_loc_le);

    if (extent == owner->extent) {
        fs_retain(&owner->handle);
        *_handle = &owner->handle;
    } else if (extent == root->extent) {
        fs_retain(&root->handle);
        *_handle = &root->handle;
    } else {
        *_handle = open_record(mount, entry->record);
    }

    return STATUS_SUCCESS;
}

/** Parse a name from a directory record.
 * @param record        Record to parse.
 * @param buf           Buffer to write into (maximum possible size).
 * @param joliet        Joliet level. */
static void parse_name(iso9660_directory_record_t *record, char *buf, int joliet) {
    size_t len;

    if (joliet) {
        uint16_t *name = (uint16_t *)record->file_ident;

        len = min(record->file_ident_len >> 1, ISO9660_JOLIET_MAX_NAME_LEN);

        /* Name is in big-endian UCS-2, convert to native-endian. */
        for (size_t i = 0; i < len; i++)
            name[i] = be16_to_cpu(name[i]);

        /* Convert to UTF-8. */
        len = utf16_to_utf8((uint8_t *)buf, name, len);
    } else {
        len = min(record->file_ident_len, ISO9660_MAX_NAME_LEN);

        for (size_t i = 0; i < len; i++)
            buf[i] = tolower(record->file_ident[i]);
    }

    /* If file version number is 1, strip it off. Don't want to strip all
     * version numbers off, as that could leave us with duplicate file names. */
    if (len >= 2 && buf[len - 2] == ISO9660_SEPARATOR2 && buf[len - 1] == '1')
        len -= 2;

    /* Remove the '.' if there is no extension. */
    if (len && buf[len - 1] == ISO9660_SEPARATOR1)
        len--;

    buf[len] = 0;
}

/** Iterate over directory entries.
 * @param _handle       Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
static status_t iso9660_iterate(fs_handle_t *_handle, fs_iterate_cb_t cb, void *arg) {
    iso9660_handle_t *handle = (iso9660_handle_t *)_handle;
    iso9660_mount_t *mount = (iso9660_mount_t *)_handle->mount;
    size_t name_len;
    char *name __cleanup_free;
    char *buf __cleanup_free;
    uint32_t offset;
    bool cont;
    status_t ret;

    /* Allocate a temporary buffer for names. */
    name_len = 1 + (mount->joliet_level)
        ? ISO9660_JOLIET_MAX_NAME_LEN * MAX_UTF8_PER_UTF16
        : ISO9660_MAX_NAME_LEN;
    name = malloc(name_len);

    /* Read in all the directory data. */
    buf = malloc(handle->handle.size);
    ret = iso9660_read(_handle, buf, handle->handle.size, 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Iterate through each entry. */
    cont = true;
    offset = 0;
    while (cont && offset < handle->handle.size) {
        iso9660_directory_record_t *record = (iso9660_directory_record_t *)(buf + offset);
        iso9660_entry_t entry;

        if (record->rec_len) {
            offset += record->rec_len;
        } else {
            /* A zero record length means we should move on to the next block.
             * If this is the end, this will cause us to break out of the while
             * loop if offset becomes >= size. */
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
                snprintf(name, name_len, ".");
            } else if (record->file_ident[0] == 1) {
                snprintf(name, name_len, "..");
            }
        }

        if (!name[0])
            parse_name(record, name, mount->joliet_level);

        entry.entry.owner = &handle->handle;
        entry.entry.name = name;
        entry.record = record;

        cont = cb(&entry.entry, arg);
    }

    return STATUS_SUCCESS;
}

/** Generate a UUID.
 * @param pri           Primary volume descriptor.
 * @return              Pointer to allocated string for UUID. */
static char *make_uuid(iso9660_primary_volume_desc_t *pri) {
    iso9660_timestamp_t *time;
    char *uuid;
    size_t i;

    /* If the modification time is set, then base the UUID off that, else use
     * the creation time. The ISO9660 says that a date is unset if all the
     * fields are '0' and the offset is 0. */
    time = &pri->vol_mod_time;
    for (i = 0; i < 16; i++) {
        if (((uint8_t *)time)[i] != '0')
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
        if (strncmp((char *)desc->header.ident, ISO9660_IDENTIFIER, 5) != 0)
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

    /* If we don't have Joliet, names should not be case sensitive. */
    mount->mount.case_insensitive = !joliet;
    mount->joliet_level = joliet;

    /* Store the filesystem label and UUID. */
    primary->vol_ident[31] = 0;
    primary->sys_ident[31] = 0;
    mount->mount.uuid = make_uuid(primary);
    mount->mount.label = strdup(strstrip((char *)primary->vol_ident));

    /* Retreive the root node. */
    mount->mount.root = open_record(mount, (supp)
        ? (iso9660_directory_record_t *)&supp->root_dir_record
        : (iso9660_directory_record_t *)&primary->root_dir_record);

    *_mount = &mount->mount;
    return STATUS_SUCCESS;
}

/** ISO9660 filesystem operations structure. */
BUILTIN_FS_OPS(iso9660_fs_ops) = {
    .name = "ISO9660",
    .read = iso9660_read,
    .open_entry = iso9660_open_entry,
    .iterate = iso9660_iterate,
    .mount = iso9660_mount,
};
