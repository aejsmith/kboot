/*
 * Copyright (C) 2010-2015 Alex Smith
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
 * @brief               BIOS chain loader.
 */

#include <bios/bios.h>
#include <bios/disk.h>

#include <partition/mbr.h>

#include <config.h>
#include <fs.h>
#include <loader.h>

/** Where to load the boot sector to. */
#define CHAIN_LOAD_ADDR         0x7c00

/** Where to load partition table entry to. */
#define PARTITION_TABLE_ADDR    0x7be

extern void chain_loader_enter(uint8_t id, ptr_t partition_addr) __noreturn;

/** Chain load a device.
 * @param _handle       Pointer to file handle. */
static __noreturn void chain_loader_load(void *_handle) {
    fs_handle_t *handle = _handle;
    mbr_t *mbr = (mbr_t *)CHAIN_LOAD_ADDR;
    disk_device_t *disk;
    uint8_t disk_id;
    ptr_t partition_addr;

    status_t ret;

    if (handle) {
        disk = (disk_device_t *)handle->mount->device;
        ret = fs_read(handle, mbr, sizeof(*mbr), 0);
        fs_close(handle);
    } else {
        disk = (disk_device_t *)current_environ->device;
        ret = device_read(&disk->device, mbr, sizeof(*mbr), 0);
    }

    if (ret != STATUS_SUCCESS) {
        boot_error("Error reading boot sector: %pS", ret);
    } else if (mbr->signature != MBR_SIGNATURE) {
        boot_error("Boot sector has invalid signature");
    }

    disk_id = bios_disk_get_id(disk);
    partition_addr = 0;

    /* If this is an MBR partition, we should make available the partition table
     * entry corresponding to the partition. */
    if (disk_device_is_partition(disk)) {
        disk_device_t *parent = disk->parent;

        if (strcmp(parent->raw.partition_ops->name, "MBR") == 0) {
            ret = device_read(
                &parent->device, (void *)PARTITION_TABLE_ADDR,
                sizeof(mbr->partitions), offsetof(mbr_t, partitions));
            if (ret != STATUS_SUCCESS)
                boot_error("Error reading partition table: %pS", ret);

            partition_addr = PARTITION_TABLE_ADDR + (disk->id * sizeof(mbr->partitions[0]));
        }
    }

    dprintf("chain: chainloading device %s (id: 0x%x)\n", disk->device.name, disk_id);
    console_reset(current_console);
    chain_loader_enter(disk_id, partition_addr);
}

/** Chain loader operations. */
static loader_ops_t chain_loader_ops = {
    .load = chain_loader_load,
};

/** Chain load from a device or file.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_chain(value_list_t *args) {
    fs_handle_t *handle = NULL;
    device_t *device;

    if (args->count > 1 || (args->count == 1 && args->values[0].type != VALUE_TYPE_STRING)) {
        config_error("Invalid arguments");
        return false;
    }

    if (args->count == 1) {
        status_t ret = fs_open(args->values[0].string, NULL, FILE_TYPE_REGULAR, &handle);
        if (ret != STATUS_SUCCESS) {
            config_error("Error opening '%s': %pS", args->values[0].string, ret);
            return false;
        }

        device = handle->mount->device;
    } else {
        if (!current_environ->device) {
            config_error("No current device");
            return false;
        }

        device = current_environ->device;
    }

    if (device->type != DEVICE_TYPE_DISK) {
        config_error("Device '%s' is not a disk", device->name);

        if (handle)
            fs_close(handle);

        return false;
    }

    environ_set_loader(current_environ, &chain_loader_ops, handle);
    return true;
}

BUILTIN_COMMAND("chain", "Load another boot sector", config_cmd_chain);
