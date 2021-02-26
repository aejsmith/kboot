/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               TAR filesystem support.
 *
 * This allows for a TAR file to be accessed as a filesystem by mounting it as
 * a disk image.
 */

#include <fs/tar.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <device.h>
#include <fs.h>
#include <memory.h>

typedef struct tar_handle {
    fs_handle_t handle;                 /**< Handle header. */
    offset_t offset;                    /**< Data offset (for files). */
    char *name;                         /**< Entry name. */
    struct tar_handle *parent;          /**< Parent. */
    list_t link;                        /**< Link to parent. */
    list_t children;                    /**< List of children. */
} tar_handle_t;

typedef struct tar_entry {
    fs_entry_t entry;
    tar_handle_t *handle;
} tar_entry_t;

static fs_ops_t tar_fs_ops;

/** Read from a file. */
static status_t tar_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    tar_handle_t *handle = (tar_handle_t *)_handle;

    return device_read(handle->handle.mount->device, buf, count, handle->offset + offset);
}

/** Open an entry on the filesystem. */
static status_t tar_open_entry(const fs_entry_t *_entry, fs_handle_t **_handle) {
    tar_entry_t *entry = (tar_entry_t *)_entry;

    fs_retain(&entry->handle->handle);

    *_handle = &entry->handle->handle;
    return STATUS_SUCCESS;
}

/** Iterate over directory entries. */
static status_t tar_iterate(fs_handle_t *_handle, fs_iterate_cb_t cb, void *arg) {
    tar_handle_t *handle = (tar_handle_t *)_handle;

    tar_entry_t entry;
    entry.entry.owner = &handle->handle;

    entry.entry.name = ".";
    entry.handle     = handle;
    cb(&entry.entry, arg);

    entry.entry.name = "..";
    entry.handle     = (handle->parent) ? handle->parent : handle;
    cb(&entry.entry, arg);

    list_foreach(&handle->children, iter) {
        tar_handle_t *child = list_entry(iter, tar_handle_t, link);

        entry.entry.name = child->name;
        entry.handle     = child;
        cb(&entry.entry, arg);
    }

    return STATUS_SUCCESS;
}

static void tar_handle_init(tar_handle_t *handle, fs_mount_t *mount, file_type_t type, offset_t size) {
    fs_handle_init(&handle->handle, mount, type, size);

    list_init(&handle->link);
    list_init(&handle->children);

    handle->offset = 0;
    handle->name   = NULL;
    handle->parent = NULL;
}

/** Mount an instance of this filesystem. */
static status_t tar_mount(device_t *device, fs_mount_t **_mount) {
    status_t ret;

    tar_header_t *header __cleanup_free = malloc(sizeof(*header));

    /* Read in the first header. */
    ret = device_read(device, header, sizeof(*header), 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (strncmp(header->magic, "ustar", 5) != 0)
        return STATUS_UNKNOWN_FS;

    fs_mount_t *mount = malloc(sizeof(*mount));
    mount->ops              = &tar_fs_ops; /* Needed now for fs_open() to work. */
    mount->device           = device;
    mount->case_insensitive = false;
    mount->label            = (char *)"";
    mount->uuid             = (char *)"";

    /* Create the root. */
    tar_handle_t *root = malloc(sizeof(*root));
    tar_handle_init(root, mount, FILE_TYPE_DIR, 0);
    mount->root = &root->handle;

    offset_t offset = 0;
    while (true) {
        if (offset != 0) {
            /* Read in the next header. */
            ret = device_read(device, header, sizeof(*header), offset);
            if (ret != STATUS_SUCCESS)
                goto err_cleanup;
        }

        /* Two NULL bytes in the name field indicates EOF. */
        if (!header->name[0] && !header->name[1])
            break;

        if (strncmp(header->magic, "ustar", 5) != 0) {
            ret = STATUS_CORRUPT_FS;
            goto err_cleanup;
        }

        /* All fields in the header are stored as ASCII - convert the size to
         * an integer (base 8). */
        offset_t data_offset = offset + 512;
        size_t data_size     = strtoul(header->size, NULL, 8);

        /* 512 for the header, plus the file size if necessary. */
        offset += 512;
        if (data_size > 0)
            offset += round_up(data_size, 512);

        /* Skip root directory if it's there. */
        if (strcmp(header->name, "./") == 0)
            continue;

        file_type_t type = FILE_TYPE_NONE;

        switch (header->typeflag) {
        case REGTYPE:
        case AREGTYPE:
            type = FILE_TYPE_REGULAR;
            break;
        case DIRTYPE:
            type = FILE_TYPE_DIR;
            break;
        case 'x':
            /* PAX extended header. Ignore for now. */
            break;
        default:
            dprintf("tar: warning: unhandled type flag '%c' for '%s'\n", header->typeflag, header->name);
            break;
        }

        if (type == FILE_TYPE_NONE)
            continue;

        char *dir __cleanup_free  = dirname(header->name);
        char *name __cleanup_free = basename(header->name);

        dprintf("'%s' -> '%s' '%s'\n", header->name, dir, name);

        fs_handle_t *_parent __cleanup_close = NULL;
        ret = fs_open(dir, &root->handle, FILE_TYPE_DIR, 0, &_parent);
        if (ret != STATUS_SUCCESS) {
            dprintf(
                "tar: failed to open parent '%s' for '%s' (%d), missing directory in file?\n",
                dir, header->name, ret);
            goto err_cleanup;
        }

        tar_handle_t *parent = (tar_handle_t *)_parent;

        tar_handle_t *handle = malloc(sizeof(*handle));
        tar_handle_init(handle, mount, type, (type == FILE_TYPE_REGULAR) ? data_size : 0);

        handle->offset = data_offset;
        handle->name   = name;
        handle->parent = parent;

        list_append(&parent->children, &handle->link);

        name = NULL;
    }

    *_mount = mount;
    return STATUS_SUCCESS;

err_cleanup:
    ;
    tar_handle_t *handle = root;

    while (handle) {
        if (!list_empty(&handle->children)) {
            handle = list_first(&handle->children, tar_handle_t, link);
            list_remove(&handle->link);
        } else {
            tar_handle_t *parent = handle->parent;
            free(handle);
            handle = parent;
        }
    }

    free(mount);
    return ret;
}

BUILTIN_FS_OPS(tar_fs_ops) = {
    .name       = "TAR",
    .read       = tar_read,
    .open_entry = tar_open_entry,
    .iterate    = tar_iterate,
    .mount      = tar_mount,
};
