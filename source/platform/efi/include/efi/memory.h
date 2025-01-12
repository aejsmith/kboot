/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI memory functions.
 */

#ifndef __EFI_MEMORY_H
#define __EFI_MEMORY_H

#include <efi/api.h>

extern void efi_memory_init(void);
extern void efi_memory_cleanup(void);

#endif /* __EFI_MEMORY_H */
