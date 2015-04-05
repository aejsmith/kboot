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
 * @brief               Multiboot support.
 *
 * The loader is capable of being loaded by a Multiboot-compliant boot loader.
 * When loaded via Multiboot, if no modules are loaded, the boot device will
 * be set to the device set in the Multiboot information. If modules are loaded,
 * then the root device will be a virtual file system containing these modules.
 * If no module named kboot.cfg is loaded, then one will be automatically
 * generated that will load the first module as a KBoot kernel, and all
 * remaining modules as modules for the kernel. In effect, this allows the
 * loader to function as a secondary loader for KBoot kernels.
 */

#include <bios/multiboot.h>

#include <lib/list.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <config.h>
#include <device.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** Multiboot device information. */
typedef struct multiboot_device {
    device_t device;                    /**< Device header. */
    fs_mount_t mount;                   /**< Mount header. */

    list_t files;                       /**< List of files. */
} multiboot_device_t;

/** Multiboot file information. */
typedef struct multiboot_file {
    fs_handle_t handle;                 /**< Handle header. */
    fs_entry_t entry;                   /**< Entry header. */

    list_t header;                      /**< Link to the file list. */
    void *addr;                         /**< Module data address. */
    char *cmdline;                      /**< Command line (following file name). */
} multiboot_file_t;

/** Parse command line arguments. */
static void parse_arguments(void) {
    char *cmdline, *tok;

    cmdline = (char *)multiboot_info.cmdline;
    while ((tok = strsep(&cmdline, " "))) {
        if (strncmp(tok, "config=", 7) == 0)
            config_file_override = tok + 7;
    }
}

/** Get Multiboot device identification information.
 * @param device        Device to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void multiboot_device_identify(device_t *device, device_identify_t type, char *buf, size_t size) {
    if (type == DEVICE_IDENTIFY_SHORT)
        snprintf(buf, size, "Multiboot modules");
}

/** Multiboot device operations. */
static device_ops_t multiboot_device_ops = {
    .identify = multiboot_device_identify,
};

/** Read from a file.
 * @param handle        Handle to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset to read from.
 * @return              Status code describing the result of the operation. */
static status_t multiboot_fs_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset) {
    multiboot_file_t *file = container_of(handle, multiboot_file_t, handle);

    memcpy(buf, file->addr + offset, count);
    return STATUS_SUCCESS;
}

/** Open an entry.
 * @param entry         Entry to open (obtained via iterate()).
 * @param _handle       Where to store pointer to opened handle.
 * @return              Status code describing the result of the operation. */
static status_t multiboot_fs_open_entry(const fs_entry_t *entry, fs_handle_t **_handle) {
    multiboot_file_t *file = container_of(entry, multiboot_file_t, entry);

    fs_retain(&file->handle);

    *_handle = &file->handle;
    return STATUS_SUCCESS;
}

/** Iterate over directory entries.
 * @param handle        Handle to directory.
 * @param cb            Callback to call on each entry.
 * @param arg           Data to pass to callback.
 * @return              Status code describing the result of the operation. */
static status_t multiboot_fs_iterate(fs_handle_t *handle, fs_iterate_cb_t cb, void *arg) {
    multiboot_device_t *multiboot = container_of(handle->mount, multiboot_device_t, mount);

    assert(handle == handle->mount->root);

    list_foreach(&multiboot->files, iter) {
        multiboot_file_t *file = list_entry(iter, multiboot_file_t, header);

        if (!cb(&file->entry, arg))
            break;
    }

    return STATUS_SUCCESS;
}

/** Multiboot filesystem operations structure. */
static fs_ops_t multiboot_fs_ops = {
    .name = "Multiboot",
    .read = multiboot_fs_read,
    .open_entry = multiboot_fs_open_entry,
    .iterate = multiboot_fs_iterate,
};

/** Generate a configuration file.
 * @param multiboot     Multiboot device. */
static void generate_config(multiboot_device_t *multiboot) {
    size_t count, offset;
    char *buf;
    multiboot_file_t *file;

    /* Take a wild guess at the maximum file size for now. */
    count = 1024;
    offset = 0;
    buf = malloc(count);

    /* Get the kernel image. */
    file = list_first(&multiboot->files, multiboot_file_t, header);

    /* Turn command line options into environment variables. */
    if (file->cmdline) {
        char *pos = file->cmdline;

        while (*pos) {
            if (*pos != ' ') {
                const char *name = pos;
                const char *value;

                while (*pos && *pos != '=' && *pos != ' ')
                    pos++;

                if (*pos == '=') {
                    *pos++ = 0;
                    value = pos;

                    /* Don't break at spaces inside quotes. */
                    if (*pos == '"') {
                        pos++;
                        while (*pos && *pos != '"')
                            pos++;
                    }

                    while (*pos && *pos != ' ')
                        pos++;
                } else {
                    /* No =, treat as a boolean option. */
                    value = "true";
                }

                if (*pos)
                    *pos++ = 0;

                offset += snprintf(buf + offset, count - offset, "set \"%s\" %s\n", name, value);
            } else {
                pos++;
            }
        }
    }

    /* Add the kernel load command. */
    offset += snprintf(buf + offset, count - offset, "kboot \"%s\"", file->entry.name);

    /* Add all modules. */
    if (file != list_last(&multiboot->files, multiboot_file_t, header)) {
        offset += snprintf(buf + offset, count - offset, " [\n");

        while (file != list_last(&multiboot->files, multiboot_file_t, header)) {
            file = list_next(file, header);
            offset += snprintf(buf + offset, count - offset, "    \"%s\"\n", file->entry.name);
        }

        offset += snprintf(buf + offset, count - offset, "]\n");
    } else {
        offset += snprintf(buf + offset, count - offset, "\n");
    }

    /* Create a file for the entry. */
    file = malloc(sizeof(*file));
    fs_handle_init(&file->handle, &multiboot->mount, FILE_TYPE_REGULAR, offset);
    file->entry.owner = multiboot->mount.root;
    file->entry.name = "kboot.cfg";
    file->addr = buf;
    file->cmdline = NULL;

    list_init(&file->header);
    list_append(&multiboot->files, &file->header);
}

/** Load Multiboot modules. */
static void load_modules(void) {
    multiboot_device_t *multiboot;
    multiboot_module_info_t *modules;
    bool found_config;

    if (!multiboot_info.mods_count)
        return;

    dprintf("multiboot: using modules for boot filesystem\n");

    multiboot = malloc(sizeof(*multiboot));
    multiboot->device.type = DEVICE_TYPE_VIRTUAL;
    multiboot->device.ops = &multiboot_device_ops;
    multiboot->device.name = "mb";
    multiboot->mount.device = &multiboot->device;
    multiboot->mount.case_insensitive = false;
    multiboot->mount.ops = &multiboot_fs_ops;
    multiboot->mount.label = NULL;
    multiboot->mount.uuid = NULL;
    list_init(&multiboot->files);

    /* Create the root directory. */
    multiboot->mount.root = malloc(sizeof(*multiboot->mount.root));
    multiboot->mount.root->mount = &multiboot->mount;
    multiboot->mount.root->type = FILE_TYPE_DIR;
    multiboot->mount.root->count = 1;

    modules = (multiboot_module_info_t *)((ptr_t)multiboot_info.mods_addr);
    found_config = false;
    for (uint32_t i = 0; i < multiboot_info.mods_count; i++) {
        multiboot_file_t *file;
        char *name;
        size_t size;

        if (!modules[i].cmdline)
            continue;

        file = malloc(sizeof(*file));

        fs_handle_init(
            &file->handle, &multiboot->mount, FILE_TYPE_REGULAR,
            modules[i].mod_end - modules[i].mod_start);

        file->entry.owner = multiboot->mount.root;

        /* Get the name and command line. Strip off any path prefix. */
        file->cmdline = (char *)modules[i].cmdline;
        name = strsep(&file->cmdline, " ");
        while (name)
            file->entry.name = strsep(&name, "/");

        /* Check if we have a config file. */
        if (strcmp(file->entry.name, "kboot.cfg") == 0)
            found_config = true;

        /* We re-allocate the module data as high as possible to make it
         * unlikely that we will conflict with any fixed load addresses for a
         * kernel. */
        size = round_up(file->handle.size, PAGE_SIZE);
        file->addr = memory_alloc(size, 0, 0, 0, MEMORY_TYPE_INTERNAL, MEMORY_ALLOC_HIGH, NULL);
        memcpy(file->addr, (void *)phys_to_virt(modules[i].mod_start), file->handle.size);

        list_init(&file->header);
        list_append(&multiboot->files, &file->header);
    }

    /* May have had empty command lines above. */
    if (list_empty(&multiboot->files)) {
        free(multiboot);
        return;
    }

    /* If we do not have a configuration file, generate one. */
    if (!found_config)
        generate_config(multiboot);

    device_register(&multiboot->device);
    multiboot->device.mount = &multiboot->mount;
    boot_device = &multiboot->device;
}

/** Handle Multiboot modules/arguments. */
void multiboot_init(void) {
    if (!multiboot_valid())
        return;

    parse_arguments();
    load_modules();
}
