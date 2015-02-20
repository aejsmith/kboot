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
 * @brief               Filesystem support.
 */

#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <device.h>
#include <fs.h>
#include <memory.h>

/**
 * Allocate a new FS handle structure.
 *
 * Allocates a new FS handle structure. Filesystem implementations are expected
 * to embed the fs_handle_t structure at the beginning of a structure containing
 * any private data required by them. This function allocates a structure of
 * the specified size, and initialises the generic header (aside from the
 * directory flag, which must be initialized by the FS).
 *
 * @param size          Size of the structure.
 * @param mount         Mount that the handle belongs to.
 *
 * @return              Pointer to allocated structure.
 */
void *fs_handle_alloc(size_t size, fs_mount_t *mount) {
    fs_handle_t *handle;

    assert(size >= sizeof(fs_handle_t));

    handle = malloc(size);
    handle->mount = mount;
    handle->count = 1;
    return handle;
}

/** Increase the reference count of a FS handle.
 * @param handle        Handle to increase count of. */
void fs_handle_retain(fs_handle_t *handle) {
    handle->count++;
}

/** Decrease the reference count of a FS handle and free it if it reaches 0.
 * @param handle        Handle to decrease count of. */
void fs_handle_release(fs_handle_t *handle) {
    assert(handle->count);

    if (--handle->count == 0) {
        if (handle->mount->ops->close)
            handle->mount->ops->close(handle);

        free(handle);
    }
}

/** Probe a device for filesystems.
 * @param device        Device to probe.
 * @return              Pointer to mount if found, NULL if not. */
fs_mount_t *fs_probe(device_t *device) {
    builtin_foreach(BUILTIN_TYPE_FS, fs_ops_t, ops) {
        fs_mount_t *mount;
        status_t ret;

        ret = ops->mount(device, &mount);
        switch (ret) {
        case STATUS_SUCCESS:
            dprintf("fs: mounted %s on %s ('%s') (uuid: %s)\n", ops->name, device->name, mount->label, mount->uuid);

            mount->ops = ops;
            mount->device = device;
            return mount;
        case STATUS_UNKNOWN_FS:
        case STATUS_END_OF_FILE:
            /* End of file usually means no media. */
            break;
        default:
            dprintf("fs: error %d while probing device %s\n", ret, device->name);
            return NULL;
        }
    }

    return NULL;
}

/** Structure containing data for fs_open(). */
typedef struct fs_open_data {
    const char *name;                   /**< Name of entry being searched for. */
    fs_handle_t *handle;                /**< Handle to found entry. */
} fs_open_data_t;

/** Directory iteration callback for fs_open().
 * @param name          Name of entry.
 * @param handle        Handle to entry.
 * @param _data         Pointer to data structure.
 * @return              Whether to continue iteration. */
static bool fs_open_cb(const char *name, fs_handle_t *handle, void *_data) {
    fs_open_data_t *data = _data;
    int result;

    result = (handle->mount->case_insensitive)
        ? strcasecmp(name, data->name)
        : strcmp(name, data->name);

    if (!result) {
        fs_handle_retain(handle);
        data->handle = handle;
        return false;
    } else {
        return true;
    }
}

/**
 * Open a handle to a file/directory.
 *
 * Looks up a path and returns a handle to it. If a source handle is given and
 * the path is a relative path, the path will be looked up relative to that
 * directory. If no source is given then relative paths will not be allowed.
 *
 * An absolute path either begins with a '/' character, or a device specifier
 * in the form "(<device name>)" followed by a '/'. If no device specifier is
 * included, the lookup will take place from the root of the current device.
 *
 * @param path          Path to entry to open.
 * @param from          If not NULL, a directory to look up relative to.
 *
 * @return              Pointer to handle on success, NULL on failure.
 */
status_t fs_open(const char *path, fs_handle_t *from, fs_handle_t **_handle) {
    char *orig __cleanup_free;
    char *dup, *tok;
    device_t *device;
    fs_mount_t *mount;
    fs_handle_t *handle;
    status_t ret;

    /* Duplicate the path string so we can modify it. */
    dup = orig = strdup(path);

    if (dup[0] == '(') {
        dup++;
        tok = strsep(&dup, ")");
        if (!tok || !tok[0] || dup[0] != '/')
            return STATUS_INVALID_ARG;

        device = device_lookup(tok);
        if (!device || !device->mount)
            return STATUS_NOT_FOUND;

        mount = device->mount;
    } else if (from) {
        mount = from->mount;
    } else {
        device = (current_environ) ? current_environ->device : boot_device;
        if (!device || !device->mount)
            return STATUS_NOT_FOUND;

        mount = device->mount;
    }

    if (dup[0] == '/') {
        from = mount->root;

        /* Strip leading / characters from the path. */
        while (*dup == '/')
            dup++;
    } else if (!from) {
        return STATUS_INVALID_ARG;
    }

    if (mount->ops->open) {
        /* Use the provided open() implementation. */
        ret = mount->ops->open(mount, dup, from, &handle);
        if (ret != STATUS_SUCCESS)
            return ret;
    } else {
        assert(mount->ops->iterate);

        handle = from;
        fs_handle_retain(handle);

        /* Loop through each element of the path string. */
        while (true) {
            fs_open_data_t data;

            tok = strsep(&dup, "/");
            if (!tok) {
                /* The last token was the last element of the path string,
                 * return the handle we're currently on. */
                break;
            } else if (!handle->directory) {
                /* The previous node was not a directory: this means the path
                 * string is trying to treat a non-directory as a directory.
                 * Reject this. */
                fs_handle_release(handle);
                return STATUS_NOT_DIR;
            } else if (!tok[0]) {
                /* Zero-length path component, do nothing. */
                continue;
            }

            /* Search the directory for the entry. */
            data.name = tok;
            data.handle = NULL;
            ret = mount->ops->iterate(handle, fs_open_cb, &data);
            fs_handle_release(handle);
            if (ret != STATUS_SUCCESS) {
                return ret;
            } else if (!data.handle) {
                return STATUS_NOT_FOUND;
            }

            handle = data.handle;
        }
    }

    *_handle = handle;
    return STATUS_SUCCESS;
}

/** Read from a file.
 * @param handle        Handle to the file.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset into the file.
 * @return              Status code describing the result of the operation. */
status_t file_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset) {
    if (handle->directory)
        return STATUS_NOT_FILE;

    if (!count)
        return STATUS_SUCCESS;

    return handle->mount->ops->read(handle, buf, count, offset);
}

/** Get the size of a file.
 * @param handle        Handle to the file.
 * @return              Size of the file. */
offset_t file_size(fs_handle_t *handle) {
    return (!handle->directory) ? handle->mount->ops->size(handle) : 0;
}

/** Iterate over entries in a directory.
 * @param handle        Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
status_t dir_iterate(fs_handle_t *handle, dir_iterate_cb_t cb, void *arg) {
    if (!handle->directory)
        return STATUS_NOT_DIR;

    return handle->mount->ops->iterate(handle, cb, arg);
}
