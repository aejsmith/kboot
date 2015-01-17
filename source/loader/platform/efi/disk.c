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
 * @brief               EFI disk device support.
 */

#include <lib/string.h>

#include <efi/efi.h>

#include <disk.h>
#include <loader.h>
#include <memory.h>

/** Structure containing EFI disk information. */
typedef struct efi_disk {
    disk_device_t disk;                 /**< Disk device header. */

    efi_device_path_t *path;            /**< Device path. */
    efi_block_io_protocol_t *block;     /**< Block I/O protocol. */
    efi_uint32_t media_id;              /**< Media ID. */
} efi_disk_t;

/** Block I/O protocol GUID. */
static efi_guid_t block_io_guid = EFI_BLOCK_IO_PROTOCOL_GUID;

/** Read blocks from an EFI disk.
 * @param _disk         Disk device being read from.
 * @param buf           Buffer to read into.
 * @param count         Number of blocks to read.
 * @param lba           Block number to start reading from.
 * @return              Status code describing the result of the operation. */
static status_t efi_disk_read_blocks(disk_device_t *_disk, void *buf, size_t count, uint64_t lba) {
    efi_disk_t *disk = (efi_disk_t *)_disk;
    efi_status_t ret;

    ret = efi_call(disk->block->read_blocks, disk->block, disk->media_id, lba, count * disk->disk.block_size, buf);
    if (ret != EFI_SUCCESS) {
        dprintf("efi: read from %s failed with status 0x%" PRIx32 "\n", disk->disk.device.name, ret);
        return efi_convert_status(ret);
    }

    return STATUS_SUCCESS;
}

/** EFI disk operations structure. */
static disk_ops_t efi_disk_ops = {
    .read_blocks = efi_disk_read_blocks,
};

/** Detect and register all disk devices. */
void efi_disk_init(void) {
    efi_handle_t *handles;
    efi_uintn_t num_handles, num_raw_devices = 0;
    list_t raw_devices, child_devices;
    efi_status_t ret;

    list_init(&raw_devices);
    list_init(&child_devices);

    /* Get a list of all handles supporting the block I/O protocol. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &block_io_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS) {
        dprintf("efi: no block devices available\n");
        return;
    }

    /*
     * EFI gives us both the raw devices, and any partitions as child devices.
     * We are only interested in the raw devices, as we handle partition maps
     * internally. We want to pick out the raw devices, and identify the type of
     * these devices.
     *
     * It seems like there should be a better way to identify the type, but raw
     * devices don't appear to get flagged with the type of device they are:
     * their device path nodes are just typed as ATA/SCSI/whatever (except for
     * floppies, which can be identified by their ACPI HID). Child devices do
     * get flagged with a device type.
     *
     * What we do then is make a first pass over all devices to get their block
     * protocol. If a device is a raw device (media->logical_partition == 0,
     * then we do some guesswork:
     *
     *  1. If device path node is ACPI, check HID, mark as floppy if matches.
     *  3. Otherwise, if removable, read only, and block size is 2048, mark as CD.
     *  4. Otherwise, mark as HD.
     *
     * We then do a pass over the child devices, and if they identify the type
     * of their parent, then that overrides the type guessed for the raw device.
     */
    for (efi_uintn_t i = 0; i < num_handles; i++) {
        efi_disk_t *disk;
        efi_block_io_media_t *media;

        disk = malloc(sizeof(*disk));
        memset(disk, 0, sizeof(*disk));
        list_init(&disk->disk.device.header);

        disk->path = efi_get_device_path(handles[i]);
        if (!disk->path) {
            free(disk);
            continue;
        }

        ret = efi_open_protocol(handles[i], &block_io_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void **)&disk->block);
        if (ret != EFI_SUCCESS) {
            dprintf("efi: warning: failed to open block I/O for %pE\n", disk->path);
            free(disk);
            continue;
        }

        media = disk->block->media;

        disk->media_id = media->media_id;
        disk->disk.ops = &efi_disk_ops;
        disk->disk.block_size = media->block_size;
        disk->disk.blocks = (media->media_present) ? media->last_block + 1 : 0;

        if (media->logical_partition) {
            list_append(&child_devices, &disk->disk.device.header);
        } else {
            efi_device_path_t *last = efi_last_device_node(disk->path);

            disk->disk.type = DISK_TYPE_HD;
            if (last->type == EFI_DEVICE_PATH_TYPE_ACPI) {
                efi_device_path_acpi_t *acpi = (efi_device_path_acpi_t *)last;

                /* Check EISA ID for a floppy. */
                if (acpi->hid == 0x060441d0)
                    disk->disk.type = DISK_TYPE_FLOPPY;
            } else if (media->removable_media && media->read_only && media->block_size == 2048) {
                disk->disk.type = DISK_TYPE_CDROM;
            }

            list_append(&raw_devices, &disk->disk.device.header);
            num_raw_devices++;
        }
    }

    free(handles);

    /* Pass over child devices to identify their types. */
    list_foreach_safe(&child_devices, iter) {
        efi_disk_t *child = list_entry(iter, efi_disk_t, disk.device.header);
        efi_device_path_t *last = efi_last_device_node(child->path);

        if (last->type == EFI_DEVICE_PATH_TYPE_MEDIA) {
            /* Identify the parent device. */
            list_foreach_safe(&raw_devices, piter) {
                efi_disk_t *parent = list_entry(piter, efi_disk_t, disk.device.header);

                if (efi_is_child_device_node(parent->path, child->path)) {
                    switch (last->subtype) {
                    case EFI_DEVICE_PATH_MEDIA_SUBTYPE_HD:
                        parent->disk.type = DISK_TYPE_HD;
                        break;
                    case EFI_DEVICE_PATH_MEDIA_SUBTYPE_CDROM:
                        parent->disk.type = DISK_TYPE_CDROM;
                        break;
                    }
                }
            }
        }

        list_remove(&child->disk.device.header);
        free(child);
    }

    /* Finally add the raw devices. */
    list_foreach_safe(&raw_devices, iter) {
        efi_disk_t *disk = list_entry(iter, efi_disk_t, disk.device.header);

        list_remove(&disk->disk.device.header);
        disk_device_register(&disk->disk);

        dprintf("efi: disk %s at %pE (block_size: %zu, blocks: %" PRIu64 ")\n",
            disk->disk.device.name, disk->path, disk->disk.block_size,
            disk->disk.blocks);

        disk_device_probe(&disk->disk);
    }
}