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
 *
 * TODO:
 *  - Many fields of the on-disk structures are not correctly aligned. These
 *    will cause problems on architectures where non-aligned reads are not
 *    supported. Need unaligned access wrapper functions.
 *  - Could do with some optimization - we currently traverse the cluster chain
 *    on every read. We could potentially cache some information to help
 *    sequential reads, or perhaps read in a large chunks of the FAT at a time
 *    and store it in case the next cluster resides within the same chunk.
 */

#include <lib/charset.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <endian.h>
#include <device.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

#include "fat.h"

/** Mounted FAT filesystem. */
typedef struct fat_mount {
    fs_mount_t mount;                   /**< Mount header. */

    uint32_t cluster_size;              /**< Size of a cluster (in bytes). */
    uint32_t total_clusters;            /**< Total number of clusters. */
    offset_t fat_offset;                /**< FAT offset (in bytes). */
    offset_t root_offset;               /**< Root directory offset (in bytes). */
    offset_t data_offset;               /**< Data area offset (in bytes). */
    uint8_t fat_type;                   /**< Type of the filesystem (12, 16 or 32). */
    uint32_t end_marker;                /**< End marker for the FAT type. */
} fat_mount_t;

/** Handle to a FAT file/directory. */
typedef struct fat_handle {
    fs_handle_t handle;                 /**< Handle header. */

    uint32_t cluster;                   /**< Start cluster number. */
    uint32_t size;                      /**< Size of the file. */
} fat_handle_t;

/** Print a warning message.
 * @param h             FAT handle.
 * @param fmt           Message format.
 * @param ...           Arguments to substitute into format string. */
#define fat_warn(h, fmt, ...) \
    dprintf("fat: %s: " fmt "\n", (h)->handle.mount->device->name, ##__VA_ARGS__)

/** Read from a file.
 * @param _handle       Handle to the file.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset into the file.
 * @return              Status code describing the result of the operation. */
static status_t fat_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    fat_handle_t *handle = (fat_handle_t *)_handle;
    fat_mount_t *mount = (fat_mount_t *)_handle->mount;
    uint32_t start_logical, current_logical, current_physical;

    /* Directories have a zero size (other than the root directory, which is set
     * by fat_mount()), so do not check for these. We will return end of file if
     * we reach the end of the cluster chain. */
    if (!handle->handle.directory || !handle->cluster) {
        if (offset + count > handle->size)
            return STATUS_END_OF_FILE;
    }

    /* Special case for root directory on FAT12/16. */
    if (!handle->cluster) {
        assert(handle->handle.directory);
        return device_read(mount->mount.device, buf, count, mount->root_offset + offset);
    }

    /* Determine the start logical cluster number. */
    start_logical = offset / mount->cluster_size;

    /* Traverse the cluster chain. */
    current_logical = 0;
    current_physical = handle->cluster;
    while (count) {
        offset_t fat_offset;
        uint32_t fat_entry;
        status_t ret;

        if (current_logical >= start_logical) {
            uint32_t cluster_offset = offset % mount->cluster_size;
            uint32_t cluster_count = min(count, mount->cluster_size - cluster_offset);
            offset_t device_offset = mount->data_offset
                + ((offset_t)mount->cluster_size * (current_physical - 2))
                + cluster_offset;

            /* Read the required data from this cluster. */
            ret = device_read(mount->mount.device, buf, cluster_count, device_offset);
            if (ret != STATUS_SUCCESS)
                return ret;

            buf += cluster_count;
            offset += cluster_count;
            count -= cluster_count;

            /* Don't bother trying to find next cluster if we have nothing more
             * to read. */
            if (!count)
                break;
        }

        /* Determine the offset in the FAT of the current entry. */
        fat_offset = mount->fat_offset;
        switch (mount->fat_type) {
        case 32:
            fat_offset += current_physical << 2;
            break;
        case 16:
            fat_offset += current_physical << 1;
            break;
        case 12:
            /* FAT12 packs 2 entries across 3 bytes. This gives the required
             * entry rounded down to a byte boundary. */
            fat_offset += current_physical + (current_physical >> 1);
            break;
        }

        /* Read the FAT entry. */
        fat_entry = 0;
        ret = device_read(mount->mount.device, &fat_entry, round_up(mount->fat_type, 8) / 8, fat_offset);
        if (ret != STATUS_SUCCESS)
            return ret;

        fat_entry = le32_to_cpu(fat_entry);
        if (mount->fat_type == 12) {
            /* Handle non-byte-aligned entries. */
            if (current_physical & 1)
                fat_entry >>= 4;
            fat_entry &= 0xfff;
        } else if (mount->fat_type == 32) {
            fat_entry &= 0xfffffff;
        }

        if (fat_entry >= mount->end_marker) {
            /* End of file reached (may get here for directories, which we do
             * not know the total size for). */
            return STATUS_END_OF_FILE;
        } else if (fat_entry < 2 || fat_entry >= mount->total_clusters) {
            fat_warn(handle, "invalid cluster number 0x%" PRIx32, fat_entry);
            return STATUS_CORRUPT_FS;
        }

        current_physical = fat_entry;
        current_logical++;
    }

    return STATUS_SUCCESS;
}

/** Get the size of a file.
 * @param _handle       Handle to the file.
 * @return              Size of the file. */
static offset_t fat_size(fs_handle_t *_handle) {
    fat_handle_t *handle = (fat_handle_t *)_handle;

    return handle->size;
}

/** FAT directory iteration state. */
typedef struct fat_iterate_state {
    fat_handle_t *handle;               /**< Handle being iterated. */
    fat_dir_entry_t entry;              /**< Current directory entry. */
    size_t idx;                         /**< Next directory entry index. */
    char *name;                         /**< Name buffer. */
    uint16_t *lfn_name;                 /**< Temporary unicode name buffer. */
    uint8_t lfn_seq;                    /**< Next expected LFN sequence number. */
    uint8_t lfn_checksum;               /**< LFN checksum. */
    uint8_t num_lfns;                   /**< Number of LFN entries. */
} fat_iterate_state_t;

/** Initialize directory iteration state.
 * @param state         State to initialize.
 * @param handle        Handle to directory being iterated. */
static void init_iterate_state(fat_iterate_state_t *state, fat_handle_t *handle) {
    state->handle = handle;
    state->idx = 0;
    state->name = malloc(FAT_NAME_MAX * MAX_UTF8_PER_UTF16 + 1);
    state->lfn_name = malloc(FAT_NAME_MAX * 2);
    state->lfn_seq = 0;
    state->lfn_checksum = 0;
    state->num_lfns = 0;
}

/** Destroy directory iteration state.
 * @param state         State to destroy. */
static void destroy_iterate_state(fat_iterate_state_t *state) {
    free(state->lfn_name);
    free(state->name);
}

/** Parse a long file name entry.
 * @param state         Current iteration state.
 * @return              Whether the entry was successfully parsed. */
static bool parse_long_name(fat_iterate_state_t *state) {
    fat_lfn_dir_entry_t *entry = (fat_lfn_dir_entry_t *)&state->entry;

    if (entry->id & 0x40) {
        /* This is the first entry. */
        if (state->num_lfns) {
            fat_warn(state->handle, "unexpected LFN start entry");
            return false;
        }

        state->lfn_seq = state->num_lfns = entry->id & ~0x40;
        state->lfn_checksum = entry->checksum;

        if (state->lfn_seq >= 0x20) {
            fat_warn(state->handle, "LFN start entry has invalid sequence number 0x%x", state->lfn_seq);
            return false;
        }
    } else if (!state->num_lfns) {
        fat_warn(state->handle, "missing LFN start entry");
        return false;
    } else if (entry->id != state->lfn_seq) {
        fat_warn(state->handle,
            "LFN entry has unexpected sequence number 0x%x, expected 0x%x",
            entry->id, state->lfn_seq);
        return false;
    } else if (entry->checksum != state->lfn_checksum) {
        fat_warn(state->handle,
            "LFN entry has incorrect checksum 0x%x, expected 0x%x",
            entry->checksum, state->lfn_checksum);
        return false;
    }

    /* LFN entries are in reverse order. */
    state->lfn_seq--;

    for (size_t i = 0; i < 5; i++)
        state->lfn_name[(state->lfn_seq * 13) + i] = le16_to_cpu(entry->name1[i]);
    for (size_t i = 0; i < 6; i++)
        state->lfn_name[(state->lfn_seq * 13) + 5 + i] = le16_to_cpu(entry->name2[i]);
    for (size_t i = 0; i < 2; i++)
        state->lfn_name[(state->lfn_seq * 13) + 11 + i] = le16_to_cpu(entry->name3[i]);

    /* If this is the last entry, convert to UTF-8. */
    if (!state->lfn_seq) {
        uint8_t *end = utf16_to_utf8((uint8_t *)state->name, state->lfn_name, state->num_lfns * 13);
        *end = 0;
    }

    return true;
}

/** Parse a short file name.
 * @param state         Current iteration state. */
static void parse_short_name(fat_iterate_state_t *state) {
    fat_dir_entry_t *entry = &state->entry;
    char *name = state->name;
    size_t pos = 0, dot = 0;
    bool volume_id = entry->attributes & FAT_ATTRIBUTE_VOLUME_ID;

    /* Spaces may exist within the name, can't break off at the first space. */
    for (size_t i = 0; i < 8 && entry->name[i]; i++) {
        name[pos++] = (entry->case_info & FAT_CASE_NAME_LOWER && !volume_id)
            ? tolower(entry->name[i])
            : entry->name[i];
    }

    /* Trim trailing spaces and add a '.'. Does not apply to volume label. */
    if (!(entry->attributes & FAT_ATTRIBUTE_VOLUME_ID)) {
        while (pos && name[pos - 1] == ' ')
            pos--;

        dot = pos;
        name[pos++] = '.';
    }

    for (size_t i = 8; i < 11 && entry->name[i]; i++) {
        name[pos++] = (entry->case_info & FAT_CASE_EXT_LOWER && !volume_id)
            ? tolower(entry->name[i])
            : entry->name[i];
    }

    /* Trim trailing spaces again. */
    while (pos && name[pos - 1] == ' ')
        pos--;

    /* Remove the '.' we added if the extension was blank. */
    if (dot && pos == dot + 1)
        pos--;

    name[pos] = 0;
}

/** Get the next directory entry.
 * @param state         Current iteration state.
 * @return              Status code describing the result of the operation.
 *                      When the end of the directory is reached, will return
 *                      STATUS_END_OF_FILE. */
static status_t next_dir_entry(fat_iterate_state_t *state) {
    fat_dir_entry_t *entry = &state->entry;

    state->num_lfns = 0;

    /* We don't know the total directory size, so just iterate until a read
     * returns end of file (from the end of the cluster chain). */
    while (true) {
        offset_t offset;
        status_t ret;

        /* Read the next entry. */
        offset = state->idx * sizeof(*entry);
        ret = fat_read(&state->handle->handle, entry, sizeof(*entry), offset);
        if (ret != STATUS_SUCCESS) {
            if (ret != STATUS_END_OF_FILE)
                fat_warn(state->handle, "failed to read directory with status %d", ret);

            return ret;
        }

        state->idx++;

        /* A zero byte here indicates we've reached the end. */
        if (!entry->name[0])
            return STATUS_END_OF_FILE;

        /* Ignore deleted entries and entries that have unknown attributes. */
        if (entry->attributes & ~FAT_ATTRIBUTE_VALID) {
            continue;
        } else if (entry->name[0] == FAT_DIR_ENTRY_DELETED || entry->name[0] == '.') {
            continue;
        }

        /* LFNs are implemented in special directory entries preceding the entry
         * they are for. They are marked with the low 4 attribute bits set.   */
        if (entry->attributes == FAT_ATTRIBUTE_LONG_NAME) {
            if (!parse_long_name(state)) {
                /* Discard LFN state. Don't completely fail, things may have
                 * been messed up by something that doesn't understand LFNs so
                 * fall back on the short name. */
                state->num_lfns = 0;
            }

            /* Continue getting the file name. */
            continue;
        } else if (state->num_lfns) {
            if (state->lfn_seq) {
                /* Still expecting more LFN entries, fall back on short name. */
                fat_warn(state->handle, "unexpected end of LFN entry list");
                state->num_lfns = 0;
            } else {
                uint8_t checksum = 0;

                /* Check whether the checksum matches (may get mismatches if
                 * entries are modified by a system which does not support
                 * LFNs). */
                for (size_t i = 0; i < sizeof(entry->name); i++)
                    checksum = ((checksum & 1) << 7) + (checksum >> 1) + entry->name[i];

                if (checksum != state->lfn_checksum) {
                    fat_warn(state->handle, "LFN checksum mismatch");
                    state->num_lfns = 0;
                }
            }
        }

        /* If we don't have a valid long name, calculate short name. */
        if (!state->num_lfns)
            parse_short_name(state);

        /* Return this entry. */
        return STATUS_SUCCESS;
    }
}

/** Iterate over directory entries.
 * @param _handle       Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
static status_t fat_iterate(fs_handle_t *_handle, dir_iterate_cb_t cb, void *arg) {
    fat_handle_t *handle = (fat_handle_t *)_handle;
    fat_iterate_state_t state;
    bool cont;
    status_t ret;

    init_iterate_state(&state, handle);

    cont = true;
    while (cont) {
        fat_handle_t *child;

        ret = next_dir_entry(&state);
        if (ret == STATUS_END_OF_FILE) {
            ret = STATUS_SUCCESS;
            break;
        } else if (ret != STATUS_SUCCESS) {
            break;
        }

        /* Don't care about volume labels here. */
        if (state.entry.attributes & FAT_ATTRIBUTE_VOLUME_ID)
            continue;

        /* Allocate a handle for it. */
        child = fs_handle_alloc(sizeof(*child), handle->handle.mount);
        child->handle.directory = state.entry.attributes & FAT_ATTRIBUTE_DIRECTORY;
        child->cluster = (le16_to_cpu(state.entry.first_cluster_high) << 16)
            | le16_to_cpu(state.entry.first_cluster_low);
        child->size = le32_to_cpu(state.entry.file_size);

        cont = cb(state.name, &child->handle, arg);
        fs_handle_release(&child->handle);
    }

    destroy_iterate_state(&state);
    return ret;
}

/** Get the label for a FAT filesystem.
 * @param handle        Handle to root directory.
 * @param _label        Where to store label string.
 * @return              Status code describing the result of the operation. */
static status_t get_volume_label(fat_handle_t *handle, char **_label) {
    fat_iterate_state_t state;
    status_t ret;

    init_iterate_state(&state, handle);

    while (true) {
        ret = next_dir_entry(&state);
        if (ret == STATUS_END_OF_FILE) {
            *_label = strdup("");
            ret = STATUS_SUCCESS;
            break;
        } else if (ret != STATUS_SUCCESS) {
            return ret;
        } else if (state.entry.attributes & FAT_ATTRIBUTE_VOLUME_ID) {
            /* Shrink the string down to an appropriate length. */
            *_label = strdup(state.name);
            break;
        }
    }

    destroy_iterate_state(&state);
    return ret;
}

/** Mount a FAT filesystem.
 * @param device        Device to mount.
 * @param _mount        Where to store pointer to mount structure.
 * @return              Status code describing the result of the operation. */
static status_t fat_mount(device_t *device, fs_mount_t **_mount) {
    fat_bpb_t bpb;
    fat_mount_t *mount;
    uint32_t sector_size;
    uint32_t total_sectors, reserved_sectors, fat_sectors, root_sectors, data_sectors;
    uint32_t fat_start_sector, root_start_sector, data_start_sector;
    fat_handle_t *root;
    uint32_t serial;
    status_t ret;

    /* Read in the BPB. */
    ret = device_read(device, &bpb, sizeof(bpb), 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    mount = malloc(sizeof(*mount));
    mount->mount.device = device;
    mount->mount.case_insensitive = true;

    /* There is no easy check for whether a filesystem is FAT. Just assume that
     * it is not if any of the following checks fail. */
    ret = STATUS_UNKNOWN_FS;

    if (!bpb.num_fats || (bpb.media < 0xf8 && bpb.media != 0xf0))
        goto err;

    sector_size = le16_to_cpu(bpb.bytes_per_sector);
    if (!is_pow2(sector_size) || sector_size < 512 || sector_size > 4096)
        goto err;

    mount->cluster_size = sector_size * bpb.sectors_per_cluster;
    if (!is_pow2(mount->cluster_size))
        goto err;

    total_sectors = (bpb.total_sectors_16)
        ? le16_to_cpu(bpb.total_sectors_16)
        : le32_to_cpu(bpb.total_sectors_32);
    reserved_sectors = le16_to_cpu(bpb.num_reserved_sectors);
    if (!reserved_sectors)
        goto err;

    /* Calculate the sector offset and size of the FATs. */
    fat_start_sector = reserved_sectors;
    fat_sectors = (bpb.sectors_per_fat_16)
        ? le16_to_cpu(bpb.sectors_per_fat_16)
        : le32_to_cpu(bpb.fat32.sectors_per_fat_32);

    /* Calculate number of root directory sectors. Each directory entry is 32
     * bytes. For FAT32 this will be 0 (num_root_entries == 0), as it does not
     * have a fixed root directory location. */
    root_start_sector = fat_start_sector + (fat_sectors * bpb.num_fats);
    root_sectors = round_up(le16_to_cpu(bpb.num_root_entries) * 32, sector_size) / sector_size;

    /* Calculate the sector offset and size of the data area. */
    data_start_sector = root_start_sector + root_sectors;
    data_sectors = total_sectors - data_start_sector;

    /* Calculate total cluster count, and from this the FAT type. */
    mount->total_clusters = data_sectors / bpb.sectors_per_cluster;
    if (mount->total_clusters < 4085) {
        mount->fat_type = 12;
        mount->end_marker = 0xff8;
    } else if (mount->total_clusters < 65525) {
        mount->fat_type = 16;
        mount->end_marker = 0xfff8;
    } else {
        /* FAT32 is really FAT28! */
        mount->fat_type = 32;
        mount->end_marker = 0xffffff8;
    }

    /* Save byte offsets of FAT, root and data areas. */
    mount->fat_offset = fat_start_sector * sector_size;
    mount->root_offset = root_start_sector * sector_size;
    mount->data_offset = data_start_sector * sector_size;

    /* Create a handle to the root directory. For FAT32 the root directory does
     * not have a fixed region, so use the specified cluster number, else set
     * it to 0 which fat_read() takes to refer to the root directory. */
    root = fs_handle_alloc(sizeof(*root), &mount->mount);
    root->handle.directory = true;
    root->cluster = (mount->fat_type == 32) ? le32_to_cpu(bpb.fat32.root_cluster) : 0;
    root->size = root_sectors * sector_size;
    mount->mount.root = &root->handle;

    /* Get the volume label, stored in the root directory. */
    ret = get_volume_label(root, &mount->mount.label);
    if (ret != STATUS_SUCCESS)
        goto err;

    /* Generate the UUID string from the serial number. */
    serial = le32_to_cpu((mount->fat_type == 32) ? bpb.fat32.volume_serial : bpb.fat16.volume_serial);
    mount->mount.uuid = malloc(10);
    snprintf(mount->mount.uuid, 10, "%04X-%04X", serial >> 16, serial & 0xffff);

    *_mount = &mount->mount;
    return STATUS_SUCCESS;

err:
    free(mount);
    return ret;
}

/** FAT filesystem operations structure. */
BUILTIN_FS_OPS(fat_fs_ops) = {
    .name = "FAT",
    .mount = fat_mount,
    .read = fat_read,
    .size = fat_size,
    .iterate = fat_iterate,
};
