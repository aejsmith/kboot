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
 * @brief               EFI platform main functions.
 */

#include <efi/disk.h>
#include <efi/efi.h>

#include <device.h>
#include <loader.h>

/** Loaded image protocol GUID. */
static efi_guid_t loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

/** Handle to the loader image. */
efi_handle_t efi_image_handle;

/** Loaded image structure for the loader image. */
efi_loaded_image_t *efi_loaded_image;

/** Pointer to the EFI system table and boot/runtime services. */
efi_system_table_t *efi_system_table;
efi_runtime_services_t *efi_runtime_services;
efi_boot_services_t *efi_boot_services;

/** Main function of the EFI loader.
 * @param image_handle  Handle to the loader image.
 * @param system_table  Pointer to EFI system table.
 * @return              EFI status code. */
efi_status_t efi_init(efi_handle_t image_handle, efi_system_table_t *system_table) {
    efi_status_t ret;

    efi_image_handle = image_handle;
    efi_system_table = system_table;
    efi_runtime_services = system_table->runtime_services;
    efi_boot_services = system_table->boot_services;

    arch_init();

    /* Firmware is required to set a 5 minute watchdog timer before running an
     * image. Disable it. */
    efi_call(efi_boot_services->set_watchdog_timer, 0, 0, 0, NULL);

    efi_console_init();
    efi_memory_init();
    efi_video_init();

    /* Get the loaded image protocol. */
    ret = efi_open_protocol(
        image_handle, &loaded_image_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
        (void **)&efi_loaded_image);
    if (ret != EFI_SUCCESS)
        internal_error("Failed to get loaded image protocol (0x%x)", ret);

    loader_main();
    return EFI_SUCCESS;
}

/** Detect and register all devices. */
void target_device_probe(void) {
    efi_disk_init();
}

/** Reboot the system. */
void target_reboot(void) {
    efi_call(efi_runtime_services->reset_system, EFI_RESET_WARM, EFI_SUCCESS, 0, NULL);
    internal_error("EFI reset failed");
}
