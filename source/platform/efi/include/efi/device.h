/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
