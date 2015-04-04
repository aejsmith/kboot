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
 * @brief               EFI device utility functions.
 */

#ifndef __EFI_DEVICE_H
#define __EFI_DEVICE_H

#include <efi/api.h>

struct device;

extern efi_device_path_t *efi_get_device_path(efi_handle_t handle);
extern void efi_print_device_path(efi_device_path_t *path, void (*cb)(void *data, char ch), void *data);
extern bool efi_is_child_device_node(efi_device_path_t *parent, efi_device_path_t *child);

/** Get the next device path node in a device path.
 * @param path          Current path node.
 * @return              Next path node, or NULL if end of list. */
static inline efi_device_path_t *efi_next_device_node(efi_device_path_t *path) {
    path = (efi_device_path_t *)((char *)path + path->length);
    return (path->type != EFI_DEVICE_PATH_TYPE_END) ? path : NULL;
}

/** Get the last device path node in a device path.
 * @param path          Current path node.
 * @return              Last path node. */
static inline efi_device_path_t *efi_last_device_node(efi_device_path_t *path) {
    for (efi_device_path_t *next = path; next; path = next, next = efi_next_device_node(path))
        ;

    return path;
}

extern efi_handle_t efi_device_get_handle(struct device *device);

#endif /* __EFI_DEVICE_H */
