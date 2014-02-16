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
 * @brief		EFI platform core definitions.
 */

#ifndef __EFI_EFI_H
#define __EFI_EFI_H

#include <efi/arch/efi.h>

#include <efi/api.h>

extern efi_handle_t efi_image_handle;
extern efi_system_table_t *efi_system_table;

extern efi_status_t efi_locate_handle(efi_locate_search_type_t search_type,
	efi_guid_t *protocol, void *search_key, efi_handle_t **_handles,
	efi_uintn_t *_num_handles);
extern efi_status_t efi_open_protocol(efi_handle_t handle, efi_guid_t *protocol,
	efi_uint32_t attributes, void **_interface);
extern efi_status_t efi_get_memory_map(efi_memory_descriptor_t **_memory_map,
	efi_uintn_t *_num_entries, efi_uintn_t *_map_key);

extern void efi_console_init(void);
extern void efi_memory_init(void);

efi_status_t platform_init(efi_handle_t image, efi_system_table_t *systab);

#endif /* __EFI_EFI_H */
