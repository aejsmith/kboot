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
#include <efi/memory.h>
#include <efi/net.h>
#include <efi/services.h>
#include <efi/video.h>

#include <console.h>
#include <device.h>
#include <loader.h>
#include <memory.h>

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
 * @param system_table  Pointer to EFI system table. */
__noreturn void efi_main(efi_handle_t image_handle, efi_system_table_t *system_table) {
    efi_status_t ret;

    efi_image_handle = image_handle;
    efi_system_table = system_table;
    efi_runtime_services = system_table->runtime_services;
    efi_boot_services = system_table->boot_services;

    arch_init();

    /* Firmware is required to set a 5 minute watchdog timer before running an
     * image. Disable it. */
    efi_call(efi_boot_services->set_watchdog_timer, 0, 0, 0, NULL);

    console_init();

    /* Print out section information, useful for debugging. */
    dprintf(
        "efi: base @ %p, text @ %p, data @ %p, bss @ %p\n",
        __start, __text_start, __data_start, __bss_start);

    efi_memory_init();
    efi_video_init();

    /* Get the loaded image protocol. */
    ret = efi_get_loaded_image(image_handle, &efi_loaded_image);
    if (ret != EFI_SUCCESS)
        internal_error("Failed to get loaded image protocol (0x%zx)", ret);

    loader_main();
}

/** Detect and register all devices. */
void target_device_probe(void) {
    efi_disk_init();
    efi_net_init();
}

/** Reboot the system. */
void target_reboot(void) {
    efi_call(efi_runtime_services->reset_system, EFI_RESET_WARM, EFI_SUCCESS, 0, NULL);
    internal_error("EFI reset failed");
}

/** Exit the loader. */
void target_exit(void) {
    efi_exit(EFI_SUCCESS, NULL, 0);
}
