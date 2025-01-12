/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 EFI platform Linux loader.
 */

#include <x86/linux.h>

#include <efi/console.h>
#include <efi/efi.h>

#include <loader.h>

#ifdef __LP64__
#   define HANDOVER_BITS    64
#   define HANDOVER_XLOAD   LINUX_XLOAD_EFI_HANDOVER_64
#   define HANDOVER_OFFSET  512
#else
#   define HANDOVER_BITS    32
#   define HANDOVER_XLOAD   LINUX_XLOAD_EFI_HANDOVER_32
#   define HANDOVER_OFFSET  0
#endif

extern void linux_platform_enter(
    efi_handle_t handle, efi_system_table_t *table, linux_params_t *params,
    ptr_t entry) __noreturn;

/** Check for platform-specific requirements.
 * @param loader        Loader internal data.
 * @param header        Kernel image header.
 * @return              Whether the kernel image is valid. */
bool linux_platform_check(linux_loader_t *loader, linux_header_t *header) {
    if (header->version < 0x20b || !header->handover_offset) {
        config_error("'%s' does not support EFI handover", loader->path);
        return false;
    } else if (header->version >= 0x20c && !(header->xloadflags & HANDOVER_XLOAD)) {
        config_error("'%s' does not support %u-bit EFI handover", loader->path, HANDOVER_BITS);
        return false;
    }

    return true;
}

/** Enter a Linux kernel.
 * @param loader        Loader internal data.
 * @param params        Kernel parameters structure. */
void linux_platform_load(linux_loader_t *loader, linux_params_t *params) {
    ptr_t entry;

    /* 64-bit entry point is 512 bytes after the 32-bit one. */
    entry = params->hdr.code32_start + params->hdr.handover_offset + HANDOVER_OFFSET;

    /* Start the kernel. */
    dprintf("linux: kernel EFI handover entry at %p, params at %p\n", entry, params);
    linux_platform_enter(efi_image_handle, efi_system_table, params, entry);
}
