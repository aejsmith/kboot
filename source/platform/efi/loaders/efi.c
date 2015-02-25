/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               EFI executable loader.
 *
 * TODO:
 *  - Need to generate file path and set in loaded image protocol.
 */

#include <efi/console.h>
#include <efi/disk.h>
#include <efi/efi.h>
#include <efi/memory.h>
#include <efi/video.h>

#include <lib/string.h>

#include <config.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** EFI loader data. */
typedef struct efi_loader {
    fs_handle_t *handle;                /**< Handle to EFI image. */
    value_t cmdline;                    /**< Command line to the image. */
} efi_loader_t;

/** Load an EFI executable.
 * @param _loader       Pointer to loader data. */
static __noreturn void efi_loader_load(void *_loader) {
    efi_loader_t *loader = _loader;
    phys_size_t buf_size;
    void *buf;
    efi_handle_t image_handle;
    efi_loaded_image_t *image;
    device_t *device;
    efi_char16_t *str;
    efi_uintn_t str_size;
    efi_status_t status;
    status_t ret;

    /* Allocate a buffer to read the image into. */
    buf_size = round_up(loader->handle->size, PAGE_SIZE);
    buf = memory_alloc(buf_size, 0, 0, 0, MEMORY_TYPE_INTERNAL, 0, NULL);
    if (!buf)
        boot_error("Failed to allocate %" PRIu64 " bytes", loader->handle->size);

    /* Read it in. */
    ret = fs_read(loader->handle, buf, loader->handle->size, 0);
    if (ret != STATUS_SUCCESS)
        boot_error("Failed to read EFI image (%d)", ret);

    /* Ask the firmware to load the image. */
    status = efi_call(
        efi_boot_services->load_image,
        false, efi_image_handle, NULL, buf, loader->handle->size, &image_handle);
    if (status != EFI_SUCCESS)
        boot_error("Failed to load EFI image (0x%zx)", status);

    /* Get the loaded image protocol. */
    status = efi_get_loaded_image(image_handle, &image);
    if (status != EFI_SUCCESS)
        boot_error("Failed to get loaded image protocol (0x%zx)", status);

    /* Try to identify the handle of the device the image was on. */
    device = loader->handle->mount->device;
    image->device_handle = (device->type == DEVICE_TYPE_DISK)
        ? efi_disk_get_handle((disk_device_t *)device)
        : NULL;

    fs_close(loader->handle);

    /* Convert the arguments to UTF-16. FIXME: UTF-8 internally? */
    str_size = (strlen(loader->cmdline.string) + 1) * sizeof(*str);
    str = malloc(str_size);
    for (size_t i = 0; i < str_size / sizeof(*str); i++)
        str[i] = loader->cmdline.string[i];

    image->load_options = str;
    image->load_options_size = str_size;

    /* Reset everything to default state. */
    efi_video_reset();
    efi_console_reset();
    efi_memory_cleanup();

    /* Start the image. */
    status = efi_call(efi_boot_services->start_image, image_handle, &str_size, &str);
    if (status != EFI_SUCCESS)
        dprintf("efi: loaded image returned status 0x%zx\n", status);

    /* We can't do anything here - the loaded image may have done things making
     * our internal state invalid. Just pass through the error to whatever
     * loaded us. */
    efi_exit(status, str, str_size);
}

/** EFI loader operations. */
static loader_ops_t efi_loader_ops = {
    .load = efi_loader_load,
};

/** Load an EFI executable.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_efi(value_list_t *args) {
    efi_loader_t *loader;
    status_t ret;

    if ((args->count != 1 && args->count != 2)
        || args->values[0].type != VALUE_TYPE_STRING
        || (args->count == 2 && args->values[1].type != VALUE_TYPE_STRING))
    {
        config_error("efi: Invalid arguments");
        return false;
    }

    loader = malloc(sizeof(*loader));

    ret = fs_open(args->values[0].string, NULL, &loader->handle);
    if (ret != STATUS_SUCCESS) {
        config_error("efi: Error %d opening '%s'", ret, args->values[0].string);
        free(loader);
        return false;
    } else if (loader->handle->directory) {
        config_error("efi: '%s' is a directory", args->values[0].string);
        fs_close(loader->handle);
        free(loader);
        return false;
    }

    if (args->count == 2) {
        value_copy(&args->values[1], &loader->cmdline);
    } else {
        value_init(&loader->cmdline, VALUE_TYPE_STRING);
    }

    environ_set_loader(current_environ, &efi_loader_ops, loader);
    return true;
}

BUILTIN_COMMAND("efi", config_cmd_efi);
