/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI disk functions.
 */

#ifndef __EFI_DISK_H
#define __EFI_DISK_H

#include <efi/api.h>

#include <disk.h>

extern efi_handle_t efi_disk_get_handle(disk_device_t *disk);

extern void efi_disk_init(void);

#endif /* __EFI_DISK_H */
