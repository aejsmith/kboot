/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

extern char __text_start[], __data_start[], __bss_start[];

extern efi_handle_t efi_image_handle;
extern efi_loaded_image_t *efi_loaded_image;
extern efi_system_table_t *efi_system_table;
extern efi_runtime_services_t *efi_runtime_services;
extern efi_boot_services_t *efi_boot_services;

extern void efi_main(efi_handle_t image_handle, efi_system_table_t *system_table) __noreturn;

#endif /* __EFI_EFI_H */
