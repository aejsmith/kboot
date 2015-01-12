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
 * @brief               EFI boot services wrapper functions.
 */

#include <efi/efi.h>

#include <lib/string.h>

#include <loader.h>
#include <memory.h>

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
efi_status_t efi_open_protocol(efi_handle_t handle, efi_guid_t *protocol, efi_uint32_t attributes, void **interface) {
    return efi_call(efi_system_table->boot_services->open_protocol,
        handle, protocol, interface, efi_image_handle, NULL,
        attributes);
}

/**
 * Get the current memory map.
 *
 * Gets a copy of the current memory map. This function is a wrapper for the
 * EFI GetMemoryMap boot service which handles allocation of an appropriately
 * sized buffer, and ensures that the array entries are contiguous (the
 * descriptor size returned by the firmware can change in future).
 *
 * @param _memory_map   Where to store pointer to memory map.
 * @param _num_entries  Where to store number of entries in memory map.
 * @param _map_key      Where to store the key for the current memory map.
 *
 * @return              EFI status code.
 */
efi_status_t efi_get_memory_map(
    efi_memory_descriptor_t **_memory_map, efi_uintn_t *_num_entries,
    efi_uintn_t *_map_key)
{
    efi_memory_descriptor_t *memory_map = NULL, *orig;
    efi_uintn_t size = 0, descriptor_size, num_entries, i;
    efi_uint32_t descriptor_version;
    efi_status_t ret;

    /* Call a first time to get the needed buffer size. */
    ret = efi_call(efi_system_table->boot_services->get_memory_map,
        &size, memory_map, _map_key, &descriptor_size,
        &descriptor_version);
    if (ret != EFI_SUCCESS && ret != EFI_BUFFER_TOO_SMALL)
        return ret;

    num_entries = size / descriptor_size;

    if (ret == EFI_BUFFER_TOO_SMALL) {
        memory_map = malloc(size);

        ret = efi_call(efi_system_table->boot_services->get_memory_map,
            &size, memory_map, _map_key, &descriptor_size,
            &descriptor_version);
        if (ret != EFI_SUCCESS) {
            free(memory_map);
            return ret;
        }

        if (descriptor_size != sizeof(*memory_map)) {
            orig = memory_map;
            memory_map = malloc(num_entries * sizeof(*memory_map));

            for (i = 0; i < num_entries; i++) {
                memcpy(&memory_map[i],
                    (void *)orig + (descriptor_size * i),
                    min(descriptor_size, sizeof(*memory_map)));
            }

            free(orig);
        }
    }

    *_memory_map = memory_map;
    *_num_entries = num_entries;
    return ret;
}
