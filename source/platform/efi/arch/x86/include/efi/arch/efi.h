/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 EFI core definitions.
 */

#ifndef __EFI_ARCH_EFI_H
#define __EFI_ARCH_EFI_H

#include <efi/api.h>

#include <elf.h>

extern efi_status_t efi_arch_relocate(ptr_t load_base, elf_dyn_t *dyn);

#endif /* __EFI_ARCH_EFI_H */
