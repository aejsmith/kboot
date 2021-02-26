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
 * @brief               RAM disk driver.
 */

#include <drivers/disk/ramdisk.h>

#include <fs/decompress.h>

#include <lib/string.h>

#include <disk.h>
#include <fs.h>
#include <memory.h>

typedef struct ramdisk_handle {
    fs_handle_t handle;
    uint8_t *data;
    size_t size;
} ramdisk_handle_t;

static status_t ramdisk_fs_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    ramdisk_handle_t *handle = (ramdisk_handle_t *)_handle;

    memcpy(buf, handle->data + offset, count);
    return STATUS_SUCCESS;
}

static fs_ops_t ramdisk_fs_ops = {
    .read = ramdisk_fs_read,
};

static fs_mount_t ramdisk_mount = {
    .ops = &ramdisk_fs_ops,
};

/**
 * Creates a ramdisk. This is a disk device that is backed by memory. The memory
 * must remain allocated for the whole time the RAM disk might be used. The
 * memory can also be compressed and it will be transparently decompressed.
 *
 * @param name          Name for the device.
 * @param data          Data for the disk.
 * @param size          Size of the data.
 * @param boot          Whether the disk is the boot device.
 */
void ramdisk_create(const char *name, void *data, size_t size, bool boot) {
    ramdisk_handle_t *handle = malloc(sizeof(*handle));

    handle->data = data;
    handle->size = size;

    fs_handle_init(&handle->handle, &ramdisk_mount, FILE_TYPE_REGULAR, size);

    fs_handle_t *source;
    if (!decompress_open(&handle->handle, &source)) {
        source = &handle->handle;
        dprintf("ramdisk: %zu byte uncompressed RAM disk '%s' at %p\n", size, name, data);
    } else {
        dprintf("ramdisk: %zu byte compressed RAM disk '%s' at %p\n", source->size, name, data);
    }

    disk_image_register(name, source, boot);
}
