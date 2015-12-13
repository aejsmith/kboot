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
 * @brief               Ext2 filesystem support.
 *
 * TODO:
 *  - Triple indirect block support. Not urgent though, because it's unlikely
 *    a file so large will be read during boot.
 */

#include <fs/ext2.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <endian.h>
#include <device.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** Symbolic link recursion limit. */
#define EXT2_SYMLINK_LIMIT 8

/**
 * Backwards-incompatible features supported.
 *
 * A number of these are here because they don't really have any effect on a
 * read-only driver. These are RECOVER, MMP and FLEX_BG.
 */
#define EXT2_SUPPORTED_INCOMPAT (       \
    EXT2_FEATURE_INCOMPAT_FILETYPE |    \
    EXT3_FEATURE_INCOMPAT_RECOVER |     \
    EXT4_FEATURE_INCOMPAT_EXTENTS  |    \
    EXT4_FEATURE_INCOMPAT_64BIT |       \
    EXT4_FEATURE_INCOMPAT_MMP |         \
    EXT4_FEATURE_INCOMPAT_FLEX_BG)

/** Mounted ext2 filesystem structure. */
typedef struct ext2_mount {
    fs_mount_t mount;                   /**< Mount header. */

    ext2_superblock_t sb;               /**< Superblock of the filesystem. */
    void *group_tbl;                    /**< Pointer to block group descriptor table. */
    uint32_t inodes_per_group;          /**< Inodes per group. */
    uint32_t inodes_count;              /**< Inodes count. */
    size_t block_size;                  /**< Size of a block on the filesystem. */
    size_t block_groups;                /**< Number of block groups. */
    size_t group_desc_size;             /**< Size of a group descriptor. */
    size_t inode_size;                  /**< Size of an inode. */
    size_t symlink_count;               /**< Current symbolic link recursion count. */
} ext2_mount_t;

/** Open ext2 file structure. */
typedef struct ext2_handle {
    fs_handle_t handle;                 /**< Handle header. */

    uint32_t num;                       /**< Inode number. */
    ext2_inode_t inode;                 /**< Inode the handle refers to. */
} ext2_handle_t;

/** Information about an ext2 directory entry. */
typedef struct ext2_entry {
    fs_entry_t entry;                   /**< Entry header. */
    uint32_t num;                       /**< Inode number. */
} ext2_entry_t;

/** Read a block from an ext2 filesystem.
 * @param mount         Mount to read from.
 * @param buf           Buffer to read into.
 * @param num           Block number.
 * @param offset        Offset within the block to read from.
 * @param count         Number of bytes to read (0 means whole block).
 * @return              Status code describing the result of the operation. */
static status_t read_raw_block(ext2_mount_t *mount, void *buf, uint32_t num, size_t offset, size_t count) {
    offset_t disk_offset;

    if (!count)
        count = mount->block_size;

    assert(offset + count <= mount->block_size);

    disk_offset = ((offset_t)num * mount->block_size) + offset;
    return device_read(mount->mount.device, buf, count, disk_offset);
}

/** Recurse through the extent index tree to find a leaf.
 * @param mount         Mount being read from.
 * @param header        Extent header to start at.
 * @param block         Block number to get.
 * @param buf           Temporary buffer to use.
 * @param _header       Where to store pointer to header for leaf.
 * @return              Status code describing the result of the operation. */
static status_t find_leaf_extent(
    ext2_mount_t *mount, ext4_extent_header_t *header, uint32_t block, void *buf,
    ext4_extent_header_t **_header)
{
    while (true) {
        ext4_extent_idx_t *index = (ext4_extent_idx_t *)&header[1];
        uint16_t i;
        status_t ret;

        if (le16_to_cpu(header->eh_magic) != EXT4_EXT_MAGIC) {
            return STATUS_CORRUPT_FS;
        } else if (!le16_to_cpu(header->eh_depth)) {
            *_header = header;
            return STATUS_SUCCESS;
        }

        for (i = 0; i < le16_to_cpu(header->eh_entries); i++) {
            if (block < le32_to_cpu(index[i].ei_block))
                break;
        }

        if (!i)
            return STATUS_CORRUPT_FS;

        ret = read_raw_block(mount, buf, le32_to_cpu(index[i - 1].ei_leaf), 0, 0);
        if (ret != STATUS_SUCCESS)
            return ret;

        header = (ext4_extent_header_t *)buf;
    }
}

/** Get the raw block number from an inode block number.
 * @param handle        Handle to inode to get block number from.
 * @param block         Block number within the inode to get.
 * @param _num          Where to store raw block number.
 * @return              Status code describing the result of the operation. */
static status_t inode_block_to_raw(ext2_handle_t *handle, uint32_t block, uint32_t *_num) {
    ext2_mount_t *mount = (ext2_mount_t *)handle->handle.mount;
    ext2_inode_t *inode = &handle->inode;
    status_t ret;

    if (le32_to_cpu(inode->i_flags) & EXT4_EXTENTS_FL) {
        void *buf __cleanup_free;
        ext4_extent_header_t *header;
        ext4_extent_t *extent;
        uint16_t i;

        buf = malloc(mount->block_size);
        header = (ext4_extent_header_t *)inode->i_block;
        ret = find_leaf_extent(mount, header, block, buf, &header);
        if (ret != STATUS_SUCCESS)
            return ret;

        extent = (ext4_extent_t *)&header[1];
        for (i = 0; i < le16_to_cpu(header->eh_entries); i++) {
            if (block < le32_to_cpu(extent[i].ee_block))
                break;
        }

        if (!i)
            return STATUS_CORRUPT_FS;

        block -= le32_to_cpu(extent[i - 1].ee_block);

        *_num = (block < le16_to_cpu(extent[i - 1].ee_len))
            ? block + le32_to_cpu(extent[i - 1].ee_start)
            : 0;
        return STATUS_SUCCESS;
    } else {
        uint32_t *buf __cleanup_free = NULL;
        uint32_t inodes_per_block, num;

        /* First check if it's a direct block. This is easy to handle, just need
         * to get it straight out of the inode structure. */
        if (block < EXT2_NDIR_BLOCKS) {
            *_num = le32_to_cpu(inode->i_block[block]);
            return STATUS_SUCCESS;
        }

        buf = malloc(mount->block_size);

        block -= EXT2_NDIR_BLOCKS;

        /* Check whether the indirect block contains the block number we need.
         * The indirect block contains as many 32-bit entries as will fit in one
         * block of the filesystem. */
        inodes_per_block = mount->block_size / sizeof(uint32_t);
        if (block < inodes_per_block) {
            num = le32_to_cpu(inode->i_block[EXT2_IND_BLOCK]);
            if (!num) {
                *_num = 0;
                return STATUS_SUCCESS;
            }

            ret = read_raw_block(mount, buf, num, 0, 0);
            if (ret != STATUS_SUCCESS)
                return ret;

            *_num = le32_to_cpu(buf[block]);
            return STATUS_SUCCESS;
        }

        block -= inodes_per_block;

        /* Not in the indirect block, check the bi-indirect blocks. The bi-
         * indirect block contains as many 32-bit entries as will fit in one
         * block of the filesystem, with each entry pointing to an indirect
         * block. */
        if (block < (inodes_per_block * inodes_per_block)) {
            num = le32_to_cpu(inode->i_block[EXT2_DIND_BLOCK]);
            if (!num) {
                *_num = 0;
                return STATUS_SUCCESS;
            }

            ret = read_raw_block(mount, buf, num, 0, 0);
            if (ret != STATUS_SUCCESS)
                return ret;

            /* Get indirect block inside bi-indirect block. */
            num = le32_to_cpu(buf[block / inodes_per_block]);
            if (num == 0) {
                *_num = 0;
                return STATUS_SUCCESS;
            }

            ret = read_raw_block(mount, buf, num, 0, 0);
            if (ret != STATUS_SUCCESS)
                return ret;

            *_num = le32_to_cpu(buf[block % inodes_per_block]);
            return STATUS_SUCCESS;
        }

        /* Triple indirect block. I somewhat doubt this will be needed, aren't
         * likely to need to read files that big. */
        dprintf("ext2: tri-indirect blocks not yet supported!\n");
        return STATUS_NOT_SUPPORTED;
    }
}

/** Read a block from an ext2 inode.
 * @param handle        Handle to inode to read from.
 * @param buf           Buffer to read into.
 * @param num           Block number to read.
 * @param offset        Offset within the block to read from.
 * @param count         Number of bytes to read (0 means whole block).
 * @return              Status code describing the result of the operation. */
static status_t read_inode_block(ext2_handle_t *handle, void *buf, uint32_t num, size_t offset, size_t count) {
    ext2_mount_t *mount = (ext2_mount_t *)handle->handle.mount;
    uint32_t total, raw = 0;
    status_t ret;

    total = round_up(handle->handle.size, mount->block_size) / mount->block_size;
    if (num >= total)
        return STATUS_END_OF_FILE;

    ret = inode_block_to_raw(handle, num, &raw);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* If the block number is 0, then it's a sparse block. */
    if (raw == 0) {
        memset(buf, 0, (count) ? count : mount->block_size);
        return STATUS_SUCCESS;
    } else {
        return read_raw_block(mount, buf, raw, offset, count);
    }
}

/** Read from an ext2 inode.
 * @param _handle       Handle to the inode.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset into the file.
 * @return              Status code describing the result of the operation. */
static status_t ext2_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    ext2_handle_t *handle = (ext2_handle_t *)_handle;
    ext2_mount_t *mount = (ext2_mount_t *)_handle->mount;

    while (count) {
        uint32_t block = offset / mount->block_size;
        size_t block_offset = offset % mount->block_size;
        size_t block_count = min(count, mount->block_size - block_offset);
        status_t ret;

        ret = read_inode_block(handle, buf, block, block_offset, block_count);
        if (ret != STATUS_SUCCESS)
            return ret;

        buf += block_count;
        offset += block_count;
        count -= block_count;
    }

    return STATUS_SUCCESS;
}

/** Open an inode from the filesystem.
 * @param mount         Mount to read from.
 * @param id            ID of node. If the node is a symbolic link, the link
 *                      destination will be returned.
 * @param owner         Directory that the inode was found in.
 * @param _handle       Where to store pointer to handle.
 * @return              Status code describing the result of the operation. */
static status_t open_inode(ext2_mount_t *mount, uint32_t id, ext2_handle_t *owner, fs_handle_t **_handle) {
    size_t group, inode_size;
    ext2_group_desc_t *group_desc;
    offset_t inode_block, inode_offset, size;
    uint16_t type;
    ext2_handle_t *handle;
    status_t ret;

    /* Get the group descriptor containing the inode. */
    group = (id - 1) / mount->inodes_per_group;
    if (group >= mount->block_groups) {
        dprintf("ext2: bad inode number %" PRIu32 "\n", id);
        return STATUS_CORRUPT_FS;
    }

    group_desc = mount->group_tbl + (group * mount->group_desc_size);

    /* Get the size of the inode and its offset in the group's inode table. */
    inode_size = min(mount->inode_size, sizeof(ext2_inode_t));
    inode_block = le32_to_cpu(group_desc->bg_inode_table);
    if (mount->group_desc_size >= EXT2_MIN_GROUP_DESC_SIZE_64BIT)
        inode_block |= (offset_t)le32_to_cpu(group_desc->bg_inode_table_hi) << 32;
    inode_offset =
        (inode_block * mount->block_size) +
        ((offset_t)((id - 1) % mount->inodes_per_group) * mount->inode_size);

    handle = malloc(sizeof(*handle));
    handle->num = id;

    ret = device_read(mount->mount.device, &handle->inode, inode_size, inode_offset);
    if (ret != STATUS_SUCCESS) {
        dprintf("ext2: failed to read inode %" PRIu32 ": %pS\n", id, ret);
        free(handle);
        return false;
    }

    type = le16_to_cpu(handle->inode.i_mode) & EXT2_S_IFMT;
    size = le32_to_cpu(handle->inode.i_size);
    if (type == EXT2_S_IFREG)
        size |= (offset_t)le32_to_cpu(handle->inode.i_size_high) << 32;

    fs_handle_init(
        &handle->handle, &mount->mount,
        (type == EXT2_S_IFDIR) ? FILE_TYPE_DIR : FILE_TYPE_REGULAR, size);

    /* Check for a symbolic link. */
    if (type == EXT2_S_IFLNK) {
        char *dest __cleanup_free;

        assert(owner);

        /* Read in the link and try to open that path. */
        dest = malloc(size + 1);
        dest[size] = 0;
        if (le32_to_cpu(handle->inode.i_blocks) == 0) {
            memcpy(dest, handle->inode.i_block, size);
        } else {
            ret = ext2_read(&handle->handle, dest, size, 0);
            if (ret != STATUS_SUCCESS) {
                free(handle);
                return ret;
            }
        }

        free(handle);

        if (mount->symlink_count++ >= EXT2_SYMLINK_LIMIT)
            return STATUS_SYMLINK_LIMIT;

        ret = fs_open(dest, &owner->handle, FILE_TYPE_NONE, 0, _handle);
        mount->symlink_count--;
        return ret;
    } else if (type != EXT2_S_IFDIR && type != EXT2_S_IFREG) {
        /* Don't support reading other types here. */
        free(handle);
        return STATUS_NOT_SUPPORTED;
    }

    *_handle = &handle->handle;
    return STATUS_SUCCESS;
}

/** Open an entry on an ext2 filesystem.
 * @param _entry        Entry to open (obtained via iterate()).
 * @param _handle       Where to store pointer to opened handle.
 * @return              Status code describing the result of the operation. */
static status_t ext2_open_entry(const fs_entry_t *_entry, fs_handle_t **_handle) {
    ext2_entry_t *entry = (ext2_entry_t *)_entry;
    ext2_handle_t *owner = (ext2_handle_t *)_entry->owner;
    ext2_mount_t *mount = (ext2_mount_t *)_entry->owner->mount;

    if (entry->num == owner->num) {
        fs_retain(&owner->handle);
        *_handle = &owner->handle;
        return STATUS_SUCCESS;
    } else if (entry->num == EXT2_ROOT_INO) {
        fs_retain(mount->mount.root);
        *_handle = mount->mount.root;
        return STATUS_SUCCESS;
    } else {
        return open_inode(mount, entry->num, owner, _handle);
    }
}

/** Iterate over ext2 directory entries.
 * @param _handle       Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
static status_t ext2_iterate(fs_handle_t *_handle, fs_iterate_cb_t cb, void *arg) {
    ext2_handle_t *handle = (ext2_handle_t *)_handle;
    char *buf __cleanup_free_large;
    char *name __cleanup_free;
    bool cont;
    offset_t offset;
    status_t ret;

    /* Allocate buffers to read the data into. */
    buf = malloc_large(handle->handle.size);
    name = malloc(EXT2_NAME_MAX + 1);

    /* Read in all the directory entries. */
    ret = ext2_read(_handle, buf, handle->handle.size, 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    cont = true;
    offset = 0;
    while (cont && offset < handle->handle.size) {
        ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(buf + offset);

        if (entry->inode && entry->file_type != EXT2_FT_UNKNOWN && entry->name_len != 0) {
            ext2_entry_t child;

            strncpy(name, entry->name, entry->name_len);
            name[entry->name_len] = 0;

            child.entry.owner = &handle->handle;
            child.entry.name = name;
            child.num = le32_to_cpu(entry->inode);

            cont = cb(&child.entry, arg);
        } else if (!le16_to_cpu(entry->rec_len)) {
            cont = false;
        }

        offset += le16_to_cpu(entry->rec_len);
    }

    return STATUS_SUCCESS;
}

/** Mount an ext2 filesystem.
 * @param device        Device to mount.
 * @param _mount        Where to store pointer to mount structure.
 * @return              Status code describing the result of the operation. */
static status_t ext2_mount(device_t *device, fs_mount_t **_mount) {
    ext2_mount_t *mount;
    uint32_t incompat_features;
    offset_t offset;
    size_t size;
    status_t ret;

    mount = malloc(sizeof(*mount));
    mount->mount.device = device;
    mount->mount.case_insensitive = false;
    mount->symlink_count = 0;

    /* Read in the superblock. */
    ret = device_read(device, &mount->sb, sizeof(mount->sb), 1024);
    if (ret != STATUS_SUCCESS)
        goto err_free_mount;

    /* Check if it is supported. */
    if (le16_to_cpu(mount->sb.s_magic) != EXT2_MAGIC) {
        ret = STATUS_UNKNOWN_FS;
        goto err_free_mount;
    } else if (le32_to_cpu(mount->sb.s_rev_level) != EXT2_DYNAMIC_REV) {
        /* Reject this because GOOD_OLD_REV does not have a UUID or label. */
        dprintf("ext2: device %s is not EXT2_DYNAMIC_REV, unsupported\n", device->name);
        ret = STATUS_NOT_SUPPORTED;
        goto err_free_mount;
    }

    incompat_features = le32_to_cpu(mount->sb.s_feature_incompat);
    if (incompat_features & ~EXT2_SUPPORTED_INCOMPAT) {
        dprintf(
            "ext2: device %s has unsupported filesystem features: 0x%x\n",
            device->name, incompat_features);
        ret = STATUS_NOT_SUPPORTED;
        goto err_free_mount;
    }

    /* Get useful information out of the superblock. */
    mount->inodes_per_group = le32_to_cpu(mount->sb.s_inodes_per_group);
    mount->inodes_count = le32_to_cpu(mount->sb.s_inodes_count);
    mount->block_size = 1024 << le32_to_cpu(mount->sb.s_log_block_size);
    mount->block_groups = mount->inodes_count / mount->inodes_per_group;
    mount->inode_size = le16_to_cpu(mount->sb.s_inode_size);

    /* Determine group descriptor size (changes with 64-bit feature). */
    if (incompat_features & EXT4_FEATURE_INCOMPAT_64BIT) {
        mount->group_desc_size = le16_to_cpu(mount->sb.s_desc_size);

        if (mount->group_desc_size < EXT2_MIN_GROUP_DESC_SIZE_64BIT ||
            mount->group_desc_size > EXT2_MAX_GROUP_DESC_SIZE ||
            !is_pow2(mount->group_desc_size))
        {
            dprintf(
                "ext2: device %s has unsupported group descriptor size %zu\n",
                device->name, mount->group_desc_size);
            ret = STATUS_CORRUPT_FS;
            goto err_free_mount;
        }
    } else {
        mount->group_desc_size = EXT2_MIN_GROUP_DESC_SIZE;
    }

    /* Read in the group descriptor table. */
    offset = mount->block_size * (le32_to_cpu(mount->sb.s_first_data_block) + 1);
    size = round_up(mount->block_groups * mount->group_desc_size, mount->block_size);
    mount->group_tbl = malloc_large(size);
    ret = device_read(device, mount->group_tbl, size, offset);
    if (ret != STATUS_SUCCESS)
        goto err_free_tbl;

    /* Get a handle to the root inode. */
    ret = open_inode(mount, EXT2_ROOT_INO, NULL, &mount->mount.root);
    if (ret != STATUS_SUCCESS)
        goto err_free_tbl;

    /* Store label and UUID. */
    mount->mount.label = strdup(mount->sb.s_volume_name);
    mount->mount.uuid = malloc(UUID_STR_LEN);
    snprintf(mount->mount.uuid, UUID_STR_LEN, "%pU", mount->sb.s_uuid);

    *_mount = &mount->mount;
    return STATUS_SUCCESS;

err_free_tbl:
    free_large(mount->group_tbl);

err_free_mount:
    free(mount);
    return ret;
}

/** Ext2 filesystem operations structure. */
BUILTIN_FS_OPS(ext2_fs_ops) = {
    .name = "ext2",
    .read = ext2_read,
    .open_entry = ext2_open_entry,
    .iterate = ext2_iterate,
    .mount = ext2_mount,
};
