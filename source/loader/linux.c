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
 * @brief               Linux kernel loader.
 *
 * This file just implements the frontend 'linux' command for loading a Linux
 * kernel. The actual loading work is deferred to the architecture, as each
 * architecture has its own boot protocol.
 */

#include <lib/string.h>

#include <loader/linux.h>

#include <loader.h>
#include <memory.h>
#include <ui.h>

/** Video mode types to support (will only get VGA if platform supports). */
#define LINUX_VIDEO_TYPES   (VIDEO_MODE_VGA | VIDEO_MODE_LFB)

/** Load a Linux kernel.
 * @param _loader       Pointer to loader internal data. */
static __noreturn void linux_loader_load(void *_loader) {
    linux_loader_t *loader = _loader;
    size_t size;

    /* Combine the path string and arguments back into a single string. */
    size = strlen("BOOT_IMAGE=") + strlen(loader->path) + strlen(loader->args.string) + 1;
    loader->cmdline = malloc(size);
    strcpy(loader->cmdline, "BOOT_IMAGE=");
    strcat(loader->cmdline, loader->path);
    strcat(loader->cmdline, " ");
    strcat(loader->cmdline, loader->args.string);

    /* Architecture code does all the work. */
    linux_arch_load(loader);
}

#ifdef CONFIG_TARGET_HAS_UI

/** Get a configuration window.
 * @param _loader       Pointer to loader internal data.
 * @param title         Title to give the window.
 * @return              Configuration window. */
static ui_window_t *linux_loader_configure(void *_loader, const char *title) {
    linux_loader_t *loader = _loader;
    ui_window_t *window;
    ui_entry_t *entry;

    window = ui_list_create(title, true);
    entry = ui_entry_create("Command line", &loader->args);
    ui_list_insert(window, entry, false);

    #ifdef CONFIG_TARGET_HAS_VIDEO
        entry = video_env_chooser(current_environ, "video_mode", LINUX_VIDEO_TYPES);
        ui_list_insert(window, entry, false);
    #endif

    return window;
}

#endif

/** Linux loader operations. */
static loader_ops_t linux_loader_ops = {
    .load = linux_loader_load,
    #ifdef CONFIG_TARGET_HAS_UI
    .configure = linux_loader_configure,
    #endif
};

/** Split a command line string into path and arguments.
 * @param str           String to split.
 * @param _path         Where to store malloc()'d path string.
 * @param _cmdline      Where to store malloc()'d arguments string. */
static void split_cmdline(const char *str, char **_path, char **_cmdline) {
    size_t len = 0;
    bool escaped = false;
    char *path;

    for (size_t i = 0; str[i]; i++) {
        if (!escaped && str[i] == '\\') {
            escaped = true;
        } else if (!escaped && str[i] == ' ') {
            break;
        } else {
            len++;
            escaped = false;
        }
    }

    path = malloc(len + 1);
    path[len] = 0;

    escaped = false;
    for (size_t i = 0; i < len; str++) {
        if (!escaped && *str == '\\') {
            escaped = true;
        } else if (!escaped && *str == ' ') {
            break;
        } else {
            path[i++] = *str;
            escaped = false;
        }
    }

    /* Skip a space. */
    if (*str)
        str++;

    *_path = path;
    *_cmdline = strdup(str);
}

/** Load Linux kernel initrd data.
 * @param loader        Loader internal data.
 * @param addr          Allocated address to load to. */
void linux_initrd_load(linux_loader_t *loader, void *addr) {
    list_foreach(&loader->initrds, iter) {
        linux_initrd_t *initrd = list_entry(iter, linux_initrd_t, header);
        status_t ret;

        ret = fs_read(initrd->handle, addr, initrd->handle->size, 0);
        if (ret != STATUS_SUCCESS)
            boot_error("Error loading initrd: %pS", ret);

        addr += initrd->handle->size;
    }
}

/** Add an initrd file.
 * @param loader        Linux loader internal data.
 * @param path          Path to initrd.
 * @return              Whether successful. */
static bool add_initrd(linux_loader_t *loader, const char *path) {
    linux_initrd_t *initrd;
    status_t ret;

    initrd = malloc(sizeof(*initrd));
    list_init(&initrd->header);

    ret = fs_open(path, NULL, FILE_TYPE_REGULAR, &initrd->handle);
    if (ret != STATUS_SUCCESS) {
        config_error("Error opening '%s': %pS", path, ret);
        free(initrd);
        return false;
    }

    loader->initrd_size += initrd->handle->size;
    list_append(&loader->initrds, &initrd->header);
    return true;
}

/** Load a Linux kernel.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_linux(value_list_t *args) {
    linux_loader_t *loader;
    status_t ret;

    if (args->count < 1 || args->count > 2 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    loader = malloc(sizeof(*loader));
    list_init(&loader->initrds);
    loader->initrd_size = 0;

    loader->args.type = VALUE_TYPE_STRING;
    split_cmdline(args->values[0].string, &loader->path, &loader->args.string);

    ret = fs_open(loader->path, NULL, FILE_TYPE_REGULAR, &loader->kernel);
    if (ret != STATUS_SUCCESS) {
        config_error("Error opening '%s': %pS", loader->path, ret);
        goto err_free;
    }

    if (args->count == 2) {
        if (args->values[1].type == VALUE_TYPE_STRING) {
            if (!add_initrd(loader, args->values[1].string))
                goto err_close;
        } else if (args->values[1].type == VALUE_TYPE_LIST) {
            value_list_t *list = args->values[1].list;

            for (size_t i = 0; i < list->count; i++) {
                if (list->values[i].type != VALUE_TYPE_STRING) {
                    config_error("Invalid arguments");
                    goto err_initrd;
                } else if (!add_initrd(loader, list->values[i].string)) {
                    goto err_initrd;
                }
            }
        } else {
            config_error("Invalid arguments");
            goto err_close;
        }
    }

    /* Check whether the kernel image is valid. */
    if (!linux_arch_check(loader))
        goto err_initrd;

    #ifdef CONFIG_TARGET_HAS_VIDEO
        video_env_init(current_environ, "video_mode", LINUX_VIDEO_TYPES, NULL);
    #endif

    environ_set_loader(current_environ, &linux_loader_ops, loader);
    return true;

err_initrd:
    while (!list_empty(&loader->initrds)) {
        linux_initrd_t *initrd = list_first(&loader->initrds, linux_initrd_t, header);

        list_remove(&initrd->header);
        fs_close(initrd->handle);
        free(initrd);
    }

err_close:
    fs_close(loader->kernel);

err_free:
    value_destroy(&loader->args);
    free(loader->path);
    free(loader);
    return false;
}

BUILTIN_COMMAND("linux", "Load a Linux kernel", config_cmd_linux);
