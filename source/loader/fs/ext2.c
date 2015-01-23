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

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <endian.h>
#include <device.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

#include "ext2.h"

/** Symbolic link recursion limit. */
#define EXT2_SYMLINK_LIMIT 8

/** Mounted ext2 filesystem structure. */
typedef struct ext2_mount {
    fs_mount_t mount;                   /**< Mount header. */

    ext2_superblock_t sb;               /**< Superblock of the filesystem. */
    ext2_group_desc_t *group_tbl;       /**< Pointer to block group descriptor table. */
    uint32_t inodes_per_group;          /**< Inodes per group. */
    uint32_t inodes_count;              /**< Inodes count. */
    size_t block_size;                  /**< Size of a block on the filesystem. */
    size_t block_groups;                /**< Number of block groups. */
    size_t inode_size;                  /**< Size of an inode. */
    size_t symlink_count;               /**< Current symbolic link recursion count. */
} ext2_mount_t;

/** Open ext2 file structure. */
typedef struct ext2_handle {
    fs_handle_t handle;                 /**< Handle header. */

    uint32_t num;                       /**< Inode number. */
    ext2_inode_t inode;                 /**< Inode the handle refers to. */
} ext2_handle_t;

/** Read a block from an ext2 filesystem.
 * @param mount         Mount to read from.
 * @param buf           Buffer to read into.
 * @param num           Block number.
 * @return              Status code describing the result of the operation. */
static status_t ext2_read_block(ext2_mount_t *mount, void *buf, uint32_t num) {
    return device_read(mount->mount.device, buf, mount->block_size, (uint64_t)num * mount->block_size);
}

/** Recurse through the extent index tree to find a leaf.
 * @param mount         Mount being read from.
 * @param header        Extent header to start at.
 * @param block         Block number to get.
 * @param buf           Temporary buffer to use.
 * @param _header       Where to store pointer to header for leaf.
 * @return              Status code describing the result of the operation. */
static status_t ext4_find_leaf(
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

        ret = ext2_read_block(mount, buf, le32_to_cpu(index[i - 1].ei_leaf));
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
static status_t ext2_inode_block_to_raw(ext2_handle_t *handle, uint32_t block, uint32_t *_num) {
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
        ret = ext4_find_leaf(mount, header, block, buf, &header);
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

            ret = ext2_read_block(mount, buf, num);
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

            ret = ext2_read_block(mount, buf, num);
            if (ret != STATUS_SUCCESS)
                return ret;

            /* Get indirect block inside bi-indirect block. */
            num = le32_to_cpu(buf[block / inodes_per_block]);
            if (num == 0) {
                *_num = 0;
                return STATUS_SUCCESS;
            }

            ret = ext2_read_block(mount, buf, num);
            if (ret != STATUS_SUCCESS)
                return ret;

            *_num = le32_to_cpu(buf[block % inodes_per_block]);
            return STATUS_SUCCESS;
        }

        /* Triple indirect block. I somewhat doubt this will be needed,
         * aren't likely to need to read files that big. */
        dprintf("ext2: tri-indirect blocks not yet supported!\n");
        return STATUS_NOT_SUPPORTED;
    }
}

/** Read blocks from an Ext2 inode.
 * @param handle        Handle to inode to read from.
 * @param buf           Buffer to read into.
 * @param block         Starting block number.
 * @return              Status code describing the result of the operation. */
static status_t ext2_inode_read_block(ext2_handle_t *handle, void *buf, uint32_t block) {
    ext2_mount_t *mount = (ext2_mount_t *)handle->handle.mount;
    ext2_inode_t *inode = &handle->inode;
    uint32_t raw;
    status_t ret;

    if (block >= (round_up(le32_to_cpu(inode->i_size), mount->block_size) / mount->block_size))
        return STATUS_END_OF_FILE;

    ret = ext2_inode_block_to_raw(handle, block, &raw);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* If the block number is 0, then it's a sparse block. */
    if (raw == 0) {
        memset(buf, 0, mount->block_size);
        return STATUS_SUCCESS;
    } else {
        return ext2_read_block(mount, buf, raw);
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
    void *block __cleanup_free = NULL;
    uint32_t start, end;
    status_t ret;

    if (offset + count > le32_to_cpu(handle->inode.i_size))
        return STATUS_END_OF_FILE;

    /* Allocate a temporary buffer for partial transfers if required. */
    if (offset % mount->block_size || count % mount->block_size)
        block = malloc(mount->block_size);

    /* Now work out the start block and the end block. Subtract one from
     * count to prevent end from going onto the next block when the offset
     * plus the count is an exact multiple of the block size. */
    start = offset / mount->block_size;
    end = (offset + (count - 1)) / mount->block_size;

    /* If we're not starting on a block boundary, we need to do a partial
     * transfer on the initial block to get up to a block boundary. If the
     * transfer only goes across one block, this will handle it. */
    if (offset % mount->block_size) {
        size_t size;

        /* Read the block into the temporary buffer. */
        ret = ext2_inode_read_block(handle, block, start);
        if (ret != STATUS_SUCCESS)
            return ret;

        size = (start == end) ? count : mount->block_size - (size_t)(offset % mount->block_size);
        memcpy(buf, block + (offset % mount->block_size), size);
        buf += size;
        count -= size;
        start++;
    }

    /* Handle any full blocks. */
    while (count >= mount->block_size) {
        /* Read directly into the destination buffer. */
        ret = ext2_inode_read_block(handle, buf, start);
        if (ret != STATUS_SUCCESS)
            return ret;

        buf += mount->block_size;
        count -= mount->block_size;
        start++;
    }

    /* Handle anything that's left. */
    if (count > 0) {
        ret = ext2_inode_read_block(handle, block, start);
        if (ret != STATUS_SUCCESS)
            return ret;

        memcpy(buf, block, count);
    }

    return STATUS_SUCCESS;
}

/** Get the size of a file.
 * @param _handle       Handle to the file.
 * @return              Size of the file. */
static offset_t ext2_size(fs_handle_t *_handle) {
    ext2_handle_t *handle = (ext2_handle_t *)_handle;

    return le32_to_cpu(handle->inode.i_size);
}

/** Open an inode from the filesystem.
 * @param mount         Mount to read from.
 * @param id            ID of node. If the node is a symbolic link, the link
 *                      destination will be returned.
 * @param from          Directory that the inode was found in.
 * @param _handle       Where to store pointer to handle.
 * @return              Status code describing the result of the operation. */
static status_t ext2_inode_open(ext2_mount_t *mount, uint32_t id, ext2_handle_t *from, fs_handle_t **_handle) {
    size_t group, size;
    offset_t offset;
    ext2_handle_t *handle;
    ext2_inode_t *inode;
    uint16_t type;
    status_t ret;

    /* Get the group descriptor table containing the inode. */
    group = (id - 1) / mount->inodes_per_group;
    if (group >= mount->block_groups) {
        dprintf("ext2: bad inode number %" PRIu32 "\n", id);
        return STATUS_CORRUPT_FS;
    }

    handle = fs_handle_alloc(sizeof(*handle), &mount->mount);
    handle->num = id;

    /* Get the size of the inode and its offset in the group's inode table. */
    size = min(mount->inode_size, sizeof(ext2_inode_t));
    offset =
        ((offset_t)le32_to_cpu(mount->group_tbl[group].bg_inode_table) * mount->block_size) +
        ((offset_t)((id - 1) % mount->inodes_per_group) * mount->inode_size);

    /* Read the inode into memory. */
    inode = &handle->inode;
    ret = device_read(mount->mount.device, inode, size, offset);
    if (ret != STATUS_SUCCESS) {
        dprintf("ext2: failed to read inode %" PRIu32 ": %d\n", id, ret);
        free(handle);
        return false;
    }

    type = le16_to_cpu(inode->i_mode) & EXT2_S_IFMT;
    handle->handle.directory = type == EXT2_S_IFDIR;

    /* Check for a symbolic link. */
    if (type == EXT2_S_IFLNK) {
        char *dest __cleanup_free;

        assert(from);

        /* Read in the link and try to open that path. */
        size = le32_to_cpu(inode->i_size);
        dest = malloc(size + 1);
        dest[size] = 0;
        if (le32_to_cpu(inode->i_blocks) == 0) {
            memcpy(dest, inode->i_block, size);
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

        ret = fs_open(dest, &from->handle, _handle);
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

/** Iterate over ext2 directory entries.
 * @param _handle       Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
static status_t ext2_iterate(fs_handle_t *_handle, dir_iterate_cb_t cb, void *arg) {
    ext2_handle_t *handle = (ext2_handle_t *)_handle;
    ext2_mount_t *mount = (ext2_mount_t *)_handle->mount;
    char *buf __cleanup_free;
    char *name __cleanup_free;
    bool done;
    uint32_t offset;
    status_t ret;

    /* Allocate buffers to read the data into. */
    buf = malloc(le32_to_cpu(handle->inode.i_size));
    name = malloc(EXT2_NAME_MAX + 1);

    /* Read in all the directory entries required. */
    ret = ext2_read(_handle, buf, le32_to_cpu(handle->inode.i_size), 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    done = false;
    offset = 0;
    while (!done && offset < le32_to_cpu(handle->inode.i_size)) {
        ext2_dirent_t *dirent = (ext2_dirent_t *)(buf + offset);

        if (dirent->file_type != EXT2_FT_UNKNOWN && dirent->name_len != 0) {
            fs_handle_t *child;

            strncpy(name, dirent->name, dirent->name_len);
            name[dirent->name_len] = 0;

            if (le32_to_cpu(dirent->inode) == handle->num) {
                child = _handle;
                fs_handle_retain(child);
            } else {
                /* Create a handle to the child. */
                ret = ext2_inode_open(mount, le32_to_cpu(dirent->inode), handle, &child);
                if (ret != STATUS_SUCCESS)
                    return ret;
            }

            done = !cb(name, child, arg);
            fs_handle_release(child);
        } else if (!le16_to_cpu(dirent->rec_len)) {
            done = true;
        }

        offset += le16_to_cpu(dirent->rec_len);
    }

    return STATUS_SUCCESS;
}

/** Mount an ext2 filesystem.
 * @param device        Device to mount.
 * @param _mount        Where to store pointer to mount structure.
 * @return              Status code describing the result of the operation. */
static status_t ext2_mount(device_t *device, fs_mount_t **_mount) {
    ext2_mount_t *mount;
    offset_t offset;
    size_t size;
    status_t ret;

    mount = malloc(sizeof(*mount));
    mount->mount.device = device;
    mount->group_tbl = NULL;
    mount->symlink_count = 0;

    /* Read in the superblock. */
    ret = device_read(device, &mount->sb, sizeof(mount->sb), 1024);
    if (ret != STATUS_SUCCESS)
        goto err;

    /* Check if it is supported. */
    if (le16_to_cpu(mount->sb.s_magic) != EXT2_MAGIC) {
        ret = STATUS_UNKNOWN_FS;
        goto err;
    } else if (le32_to_cpu(mount->sb.s_rev_level) != EXT2_DYNAMIC_REV) {
        /* Reject this because GOOD_OLD_REV does not have a UUID or label. */
        dprintf("ext2: device %s is not EXT2_DYNAMIC_REV, unsupported\n", device->name);
        ret = STATUS_UNKNOWN_FS;
        goto err;
    }

    /* Get useful information out of the superblock. */
    mount->inodes_per_group = le32_to_cpu(mount->sb.s_inodes_per_group);
    mount->inodes_count = le32_to_cpu(mount->sb.s_inodes_count);
    mount->block_size = 1024 << le32_to_cpu(mount->sb.s_log_block_size);
    mount->block_groups = mount->inodes_count / mount->inodes_per_group;
    mount->inode_size = le16_to_cpu(mount->sb.s_inode_size);

    /* Read in the group descriptor table. */
    offset = mount->block_size * (le32_to_cpu(mount->sb.s_first_data_block) + 1);
    size = round_up(mount->block_groups * sizeof(ext2_group_desc_t), mount->block_size);
    mount->group_tbl = malloc(size);
    ret = device_read(device, mount->group_tbl, size, offset);
    if (ret != STATUS_SUCCESS)
        goto err;

    /* Get a handle to the root inode. */
    ret = ext2_inode_open(mount, EXT2_ROOT_INO, NULL, &mount->mount.root);
    if (ret != STATUS_SUCCESS)
        goto err;

    /* Store label and UUID. */
    mount->mount.label = strdup(mount->sb.s_volume_name);
    mount->mount.uuid = malloc(UUID_STR_LEN);
    snprintf(mount->mount.uuid, UUID_STR_LEN, "%pU", mount->sb.s_uuid);

    dprintf("ext2: mounted %s ('%s') (uuid: %s)\n", device->name, mount->mount.label, mount->mount.uuid);
    *_mount = &mount->mount;
    return STATUS_SUCCESS;

err:
    free(mount->group_tbl);
    free(mount);
    return ret;
}

/** Ext2 filesystem operations structure. */
BUILTIN_FS_OPS(ext2_fs_ops) = {
    .mount = ext2_mount,
    .read = ext2_read,
    .size = ext2_size,
    .iterate = ext2_iterate,
};
