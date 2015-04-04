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
 * @brief               EFI services utility functions.
 */

#ifndef __EFI_SERVICES_H
#define __EFI_SERVICES_H

#include <efi/api.h>

#include <status.h>

extern char __text_start[], __data_start[], __bss_start[];

extern status_t efi_convert_status(efi_status_t status);

extern efi_status_t efi_allocate_pool(efi_memory_type_t pool_type, efi_uintn_t size, void **_buffer);
extern efi_status_t efi_free_pool(void *buffer);
extern efi_status_t efi_get_memory_map(
    efi_memory_descriptor_t **_memory_map, efi_uintn_t *_num_entries,
    efi_uintn_t *_map_key);

extern efi_status_t efi_locate_handle(
    efi_locate_search_type_t search_type, efi_guid_t *protocol, void *search_key,
    efi_handle_t **_handles, efi_uintn_t *_num_handles);
extern efi_status_t efi_open_protocol(
    efi_handle_t handle, efi_guid_t *protocol, efi_uint32_t attributes,
    void **_interface);

extern void efi_exit(efi_status_t status, efi_char16_t *data, efi_uintn_t data_size) __noreturn;
extern void efi_exit_boot_services(
    void **_memory_map, efi_uintn_t *_num_entries, efi_uintn_t *_desc_size,
    efi_uint32_t *_desc_version);
extern efi_status_t efi_get_loaded_image(efi_handle_t handle, efi_loaded_image_t **_image);

#endif /* __EFI_SERVICES_H */
