/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI executable loader.
 */

#include <efi/console.h>
#include <efi/device.h>
#include <efi/efi.h>
#include <efi/memory.h>
#include <efi/services.h>
#include <efi/video.h>

#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>
#include <ui.h>

/** EFI loader data. */
typedef struct efi_loader {
    fs_handle_t *handle;                /**< Handle to EFI image. */
    char *path;                         /**< Path to image. */
    value_t args;                       /**< Arguments to the image. */
    efi_device_path_t *efi_path;        /**< EFI file path. */
} efi_loader_t;

/** Load an EFI executable.
 * @param _loader       Pointer to loader data. */
static __noreturn void efi_loader_load(void *_loader) {
    efi_loader_t *loader = _loader;
    void *buf;
    efi_handle_t image_handle;
    efi_loaded_image_t *image;
    efi_char16_t *str;
    efi_uintn_t str_size;
    efi_status_t status;
    status_t ret;

    /* Allocate a buffer to read the image into. */
    buf = malloc_large(loader->handle->size);

    /* Read it in. */
    ret = fs_read(loader->handle, buf, loader->handle->size, 0);
    if (ret != STATUS_SUCCESS)
        boot_error("Error reading EFI image: %pS", ret);

    /* Ask the firmware to load the image. */
    status = efi_call(
        efi_boot_services->load_image,
        false, efi_image_handle, NULL, buf, loader->handle->size, &image_handle);
    if (status != EFI_SUCCESS)
        boot_error("Error loading EFI image (0x%zx)", status);

    /* Get the loaded image protocol. */
    status = efi_get_loaded_image(image_handle, &image);
    if (status != EFI_SUCCESS)
        boot_error("Error getting loaded image protocol (0x%zx)", status);

    /* Try to identify the handle of the device the image was on. */
    image->device_handle = efi_device_get_handle(loader->handle->mount->device);
    image->file_path = loader->efi_path;

    fs_close(loader->handle);

    /* Convert the arguments to UTF-16. FIXME: UTF-8 internally? */
    str_size = (strlen(loader->args.string) + 1) * sizeof(*str);
    str = malloc(str_size);
    for (size_t i = 0; i < str_size / sizeof(*str); i++)
        str[i] = loader->args.string[i];

    image->load_options = str;
    image->load_options_size = str_size;

    loader_preboot();

    /* Free up extra memory we've allocated. */
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

/** Get a configuration window.
 * @param _loader       Pointer to loader data.
 * @param title         Title to give the window.
 * @return              Configuration window. */
static ui_window_t *efi_loader_configure(void *_loader, const char *title) {
    efi_loader_t *loader = _loader;
    ui_window_t *window;
    ui_entry_t *entry;

    window = ui_list_create(title, true);
    entry = ui_entry_create("Command line", &loader->args);
    ui_list_insert(window, entry, false);
    return window;
}

/** EFI loader operations. */
static loader_ops_t efi_loader_ops = {
    .load = efi_loader_load,
    .configure = efi_loader_configure,
};

/** Create an EFI file path for a file path.
 * @param handle        Handle to file.
 * @param path          Path to file.
 * @return              Pointer to generated EFI path, NULL on failure. */
static efi_device_path_t *convert_file_path(fs_handle_t *handle, const char *path) {
    efi_device_path_file_t *efi_path;
    efi_device_path_t *end;
    size_t len;

    /* We need to generate an EFI path from the file path. Since we can't get
     * the full path from a relative path, only allow absolute ones (or relative
     * ones from the root). */
    if (*path == '(') {
        while (*path && *path++ != ')')
            ;

        /* This should be true, fs_open() succeeded. */
        assert(*path == '/');
    } else if (*path != '/') {
        if (current_environ->directory && current_environ->directory != handle->mount->root) {
            config_error("File path must be absolute or relative to root");
            return NULL;
        }
    }

    while (*path == '/')
        path++;

    /* This allocation may overestimate, due to duplicate '/'s, but we will put
     * the correct size in the structure. Add 2 to length for leading '\' and
     * NULL terminator. */
    efi_path = malloc(sizeof(efi_device_path_file_t)
        + ((strlen(path) + 2) * sizeof(efi_char16_t))
        + sizeof(efi_device_path_t));
    efi_path->header.type = EFI_DEVICE_PATH_TYPE_MEDIA;
    efi_path->header.subtype = EFI_DEVICE_PATH_MEDIA_SUBTYPE_FILE;

    /* Always need a leading '\'. */
    len = 0;
    efi_path->path[len++] = '\\';

    /* Convert the path. FIXME: UTF-8 conversion. */
    while (*path) {
        if (*path == '/') {
            efi_path->path[len++] = '\\';
            while (*path == '/')
                path++;
        } else {
            efi_path->path[len++] = *path++;
        }
    }

    /* Add the null terminator. */
    efi_path->path[len++] = 0;

    efi_path->header.length = sizeof(efi_device_path_file_t) + (len * sizeof(efi_char16_t));

    /* Add a terminator entry. */
    end = (efi_device_path_t *)((char *)efi_path + efi_path->header.length);
    end->length = sizeof(efi_device_path_t);
    end->type = EFI_DEVICE_PATH_TYPE_END;
    end->subtype = EFI_DEVICE_PATH_END_SUBTYPE_WHOLE;
    return &efi_path->header;
}

/** Load an EFI application.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_efi(value_list_t *args) {
    efi_loader_t *loader;
    status_t ret;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    loader = malloc(sizeof(*loader));

    loader->args.type = VALUE_TYPE_STRING;
    split_cmdline(args->values[0].string, &loader->path, &loader->args.string);

    ret = fs_open(loader->path, NULL, FILE_TYPE_REGULAR, 0, &loader->handle);
    if (ret != STATUS_SUCCESS) {
        config_error("Error opening '%s': %pS", loader->path, ret);
        goto err_free;
    }

    loader->efi_path = convert_file_path(loader->handle, loader->path);
    if (!loader->efi_path)
        goto err_close;

    environ_set_loader(current_environ, &efi_loader_ops, loader);
    return true;

err_close:
    fs_close(loader->handle);

err_free:
    value_destroy(&loader->args);
    free(loader->path);
    free(loader);
    return false;
}

BUILTIN_COMMAND("efi", "Load an EFI application", config_cmd_efi);
