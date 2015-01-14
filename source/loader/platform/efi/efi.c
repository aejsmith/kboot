/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               EFI boot services utility functions.
 */

#include <efi/efi.h>

#include <lib/string.h>

#include <loader.h>
#include <memory.h>

/** Device path protocol GUID. */
static efi_guid_t device_path_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static efi_guid_t device_path_to_text_guid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;

/** Device path to text protocol. */
static efi_device_path_to_text_protocol_t *device_path_to_text;

/** Allocate EFI pool memory.
 * @param pool_type     Pool memory type to allocate.
 * @param size          Size of memory to allocate.
 * @param _buffer       Where to store pointer to allocated memory.
 * @return              EFI status code. */
efi_status_t efi_allocate_pool(efi_memory_type_t pool_type, efi_uintn_t size, void **_buffer) {
    return efi_call(efi_system_table->boot_services->allocate_pool, pool_type, size, _buffer);
}

/** Free EFI pool memory.
 * @param buffer        Pointer to memory to free.
 * @return              EFI status code. */
efi_status_t efi_free_pool(void *buffer) {
    return efi_call(efi_system_table->boot_services->free_pool, buffer);
}

/**
 * Return an array of handles that support a protocol.
 *
 * Returns an array of handles that support a specified protocol. This is a
 * wrapper for the EFI LocateHandle boot service that handles the allocation
 * of a sufficiently sized buffer. The returned buffer should be freed with
 * free() once it is no longer needed.
 *
 * @param search_type   Specifies which handles are to be returned.
 * @param protocol      The protocol to search for.
 * @param search_key    Search key.
 * @param _handles      Where to store pointer to handle array.
 * @param _num_handles  Where to store the number of handles returned.
 *
 * @return              EFI status code.
 */
efi_status_t efi_locate_handle(
    efi_locate_search_type_t search_type, efi_guid_t *protocol, void *search_key,
    efi_handle_t **_handles, efi_uintn_t *_num_handles)
{
    efi_handle_t *handles = NULL;
    efi_uintn_t size = 0;
    efi_status_t ret;

    /* Call a first time to get the needed buffer size. */
    ret = efi_call(efi_system_table->boot_services->locate_handle,
        search_type, protocol, search_key, &size, handles);
    if (ret == EFI_BUFFER_TOO_SMALL) {
        handles = malloc(size);

        ret = efi_call(efi_system_table->boot_services->locate_handle,
            search_type, protocol, search_key, &size, handles);
        if (ret != EFI_SUCCESS)
            free(handles);
    }

    *_handles = handles;
    *_num_handles = size / sizeof(efi_handle_t);
    return ret;
}

/**
 * Open a protocol supported by a handle.
 *
 * This function is a wrapper for the EFI OpenProtocol boot service which
 * passes the correct values for certain arguments.
 *
 * @param handle        Handle to open on.
 * @param protocol      Protocol to open.
 * @param attributes    Open mode of the protocol interface.
 * @param _interface    Where to store pointer to opened interface.
 *
 * @return              EFI status code.
 */
efi_status_t efi_open_protocol(
    efi_handle_t handle, efi_guid_t *protocol, efi_uint32_t attributes,
    void **interface)
{
    return efi_call(efi_system_table->boot_services->open_protocol,
        handle, protocol, interface, efi_image_handle, NULL,
        attributes);
}

/** Open the device path protocol for a handle.
 * @param handle        Handle to open for.
 * @return              Pointer to device path protocol on success, NULL on
 *                      failure. */
efi_device_path_protocol_t *efi_get_device_path(efi_handle_t handle) {
    void *interface;
    efi_status_t ret;

    ret = efi_open_protocol(handle, &device_path_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, &interface);
    if (ret != EFI_SUCCESS)
        return NULL;

    return interface;
}

/** Helper to print a string representation of a device path.
 * @param path          Device path protocol to print.
 * @param cb            Helper function to print with.
 * @param data          Data to pass to helper function. */
void efi_print_device_path(efi_device_path_protocol_t *path, void (*cb)(void *data, char ch), void *data) {
    static efi_char16_t unknown[] = { 'U', 'n', 'k', 'n', 'o', 'w', 'n' };

    efi_char16_t *str;

    /* For now this only works on UEFI 2.0+, previous versions do not have the
     * device path to text protocol. */
    if (!device_path_to_text) {
        efi_handle_t *handles;
        efi_uintn_t num_handles;
        efi_status_t ret;

        /* Get the device path to text protocol. */
        ret = efi_locate_handle(EFI_BY_PROTOCOL, &device_path_to_text_guid, NULL, &handles, &num_handles);
        if (ret == EFI_SUCCESS) {
            efi_open_protocol(
                handles[0], &device_path_to_text_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
                (void **)&device_path_to_text);
            free(handles);
        }
    }

    /* Get the device path string. */
    str = (device_path_to_text)
        ? efi_call(device_path_to_text->convert_device_path_to_text, path, false, false)
        : NULL;
    if (!str)
        str = unknown;

    for (size_t i = 0; str[i]; i++) {
        /* FIXME: Unicode. */
        if (str[i] & 0x7f)
            cb(data, str[i] & 0x7f);
    }

    if (str != unknown)
        efi_free_pool(str);
}
