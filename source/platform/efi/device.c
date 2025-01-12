/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI device utility functions.
 */

#include <efi/device.h>
#include <efi/disk.h>
#include <efi/efi.h>
#include <efi/net.h>
#include <efi/services.h>

#include <lib/charset.h>
#include <lib/string.h>

#include <console.h>
#include <loader.h>
#include <memory.h>

/** Various protocol GUIDs. */
static efi_guid_t device_path_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static efi_guid_t device_path_to_text_guid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;

/** Device path to text protocol. */
static efi_device_path_to_text_protocol_t *device_path_to_text;

/** Open the device path protocol for a handle.
 * @param handle        Handle to open for.
 * @return              Pointer to device path protocol on success, NULL on
 *                      failure. */
efi_device_path_t *efi_get_device_path(efi_handle_t handle) {
    efi_device_path_t *path;
    efi_status_t ret;

    ret = efi_open_protocol(handle, &device_path_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void **)&path);
    if (ret != EFI_SUCCESS)
        return NULL;

    if (path->type == EFI_DEVICE_PATH_TYPE_END)
        return NULL;

    return path;
}

/** Helper to print a string representation of a device path.
 * @param path          Device path protocol to print.
 * @param cb            Helper function to print with.
 * @param data          Data to pass to helper function. */
void efi_print_device_path(efi_device_path_t *path, void (*cb)(void *data, char ch), void *data) {
    efi_char16_t *str;
    char *buf __cleanup_free = NULL;

    /* For now this only works on UEFI 2.0+, previous versions do not have the
     * device path to text protocol. */
    if (!device_path_to_text) {
        efi_handle_t *handles __cleanup_free = NULL;
        efi_uintn_t num_handles;
        efi_status_t ret;

        /* Get the device path to text protocol. */
        ret = efi_locate_handle(EFI_BY_PROTOCOL, &device_path_to_text_guid, NULL, &handles, &num_handles);
        if (ret == EFI_SUCCESS) {
            efi_open_protocol(
                handles[0], &device_path_to_text_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
                (void **)&device_path_to_text);
        }
    }

    /* Get the device path string. */
    str = (path && device_path_to_text)
        ? efi_call(device_path_to_text->convert_device_path_to_text, path, false, false)
        : NULL;
    if (str) {
        size_t len = 0;

        while (str[len])
            len++;

        buf = malloc((len * MAX_UTF8_PER_UTF16) + 1);
        len = utf16_to_utf8((uint8_t *)buf, str, len);
        buf[len] = 0;

        efi_free_pool(str);
    } else {
        buf = strdup("Unknown");
    }

    for (size_t i = 0; buf[i]; i++)
        cb(data, buf[i]);
}

/** Determine if a device path is a child of another.
 * @param parent        Parent device path.
 * @param child         Child device path.
 * @return              Whether child is a child of parent. */
bool efi_is_child_device_node(efi_device_path_t *parent, efi_device_path_t *child) {
    while (parent) {
        if (memcmp(child, parent, min(parent->length, child->length)) != 0)
            return false;

        parent = efi_next_device_node(parent);
        child = efi_next_device_node(child);
    }

    return child != NULL;
}

/**
 * Gets an EFI handle from a device.
 *
 * If the given device is an EFI disk, a partition on an EFI disk, or an EFI
 * network device, tries to find a handle corresponding to that device.
 *
 * @param device        Device to get handle for.
 *
 * @return              Handle to device, or NULL if not found.
 */
efi_handle_t efi_device_get_handle(device_t *device) {
    switch (device->type) {
    case DEVICE_TYPE_DISK:
        return efi_disk_get_handle((disk_device_t *)device);
    case DEVICE_TYPE_NET:
        return efi_net_get_handle((net_device_t *)device);
    default:
        return NULL;
    }
}
