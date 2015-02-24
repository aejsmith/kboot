/*
 * Copyright (C) 2014-2015 Alex Smith
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
 * @brief               EFI platform core definitions.
 */

#ifndef __EFI_EFI_H
#define __EFI_EFI_H

#include <efi/arch/efi.h>

#include <efi/api.h>

#include <status.h>

extern efi_handle_t efi_image_handle;
extern efi_loaded_image_t *efi_loaded_image;
extern efi_system_table_t *efi_system_table;
extern efi_runtime_services_t *efi_runtime_services;
extern efi_boot_services_t *efi_boot_services;

extern status_t efi_convert_status(efi_status_t status);

extern efi_status_t efi_allocate_pool(efi_memory_type_t pool_type, efi_uintn_t size, void **_buffer);
extern efi_status_t efi_free_pool(void *buffer);

extern efi_status_t efi_locate_handle(
    efi_locate_search_type_t search_type, efi_guid_t *protocol, void *search_key,
    efi_handle_t **_handles, efi_uintn_t *_num_handles);
extern efi_status_t efi_open_protocol(
    efi_handle_t handle, efi_guid_t *protocol, efi_uint32_t attributes,
    void **_interface);

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

extern void efi_console_init(void);
extern void efi_memory_init(void);
extern void efi_video_init(void);

extern efi_status_t efi_init(efi_handle_t image_handle, efi_system_table_t *system_table);

#endif /* __EFI_EFI_H */
