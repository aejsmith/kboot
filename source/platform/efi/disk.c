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

#include <lib/charset.h>
#include <lib/string.h>

#include <efi/device.h>
#include <efi/disk.h>
#include <efi/efi.h>
#include <efi/net.h>
#include <efi/services.h>

#include <loader.h>
#include <memory.h>

/** Structure containing EFI disk information. */
typedef struct efi_disk {
    disk_device_t disk;                 /**< Disk device header. */

    efi_handle_t handle;                /**< Handle to disk. */
    efi_device_path_t *path;            /**< Device path. */
    efi_block_io_protocol_t *block;     /**< Block I/O protocol. */
    efi_uint32_t media_id;              /**< Media ID. */
    bool boot;                          /**< Whether the device is the boot device. */
    uint64_t boot_partition_lba;        /**< LBA of the boot partition. */
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
        dprintf("efi: read from %s failed: 0x%zx\n", disk->disk.device.name, ret);
        return efi_convert_status(ret);
    }

    return STATUS_SUCCESS;
}

/** Check if a partition is the boot partition.
 * @param _disk         Disk the partition resides on.
 * @param id            ID of partition.
 * @param lba           Block that the partition starts at.
 * @return              Whether partition is a boot partition. */
static bool efi_disk_is_boot_partition(disk_device_t *_disk, uint8_t id, uint64_t lba) {
    efi_disk_t *disk = (efi_disk_t *)_disk;

    return disk->boot && lba == disk->boot_partition_lba;
}

/** Get a string to identify an EFI disk.
 * @param _disk         Disk to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void efi_disk_identify(disk_device_t *_disk, device_identify_t type, char *buf, size_t size) {
    efi_disk_t *disk = (efi_disk_t *)_disk;

    if (type == DEVICE_IDENTIFY_SHORT)
        snprintf(buf, size, "EFI disk %pE", disk->path);
}

/** EFI disk operations structure. */
static disk_ops_t efi_disk_ops = {
    .read_blocks = efi_disk_read_blocks,
    .is_boot_partition = efi_disk_is_boot_partition,
    .identify = efi_disk_identify,
};

/**
 * Gets an EFI handle from a disk device.
 *
 * If the given disk is an EFI disk, or a partition on an EFI disk, tries to
 * find a handle corresponding to that device.
 *
 * @param _disk         Disk to get handle for.
 *
 * @return              Handle to disk, or NULL if not found.
 */
efi_handle_t efi_disk_get_handle(disk_device_t *_disk) {
    efi_disk_t *disk;
    disk_device_t *partition = NULL;

    if (disk_device_is_partition(_disk)) {
        partition = _disk;
        _disk = _disk->parent;
    }

    if (_disk->ops != &efi_disk_ops)
        return NULL;

    disk = (efi_disk_t *)_disk;

    if (partition) {
        efi_handle_t *handles __cleanup_free = NULL;
        efi_uintn_t num_handles;
        efi_status_t ret;

        /* We need to try to locate the partition device node. */
        ret = efi_locate_handle(EFI_BY_PROTOCOL, &block_io_guid, NULL, &handles, &num_handles);
        if (ret != EFI_SUCCESS) {
            dprintf("efi: failed to get handles while identifying partition: 0x%zx\n", ret);
            return NULL;
        }

        for (efi_uintn_t i = 0; i < num_handles; i++) {
            efi_device_path_t *path = efi_get_device_path(handles[i]);

            if (!path)
                continue;

            if (efi_is_child_device_node(disk->path, path)) {
                efi_device_path_t *last = efi_last_device_node(path);

                if (last->type == EFI_DEVICE_PATH_TYPE_MEDIA) {
                    if (last->subtype == EFI_DEVICE_PATH_MEDIA_SUBTYPE_HD) {
                        efi_device_path_hd_t *hd = (efi_device_path_hd_t *)last;

                        if (partition->offset == hd->partition_start)
                            return handles[i];
                    }
                }
            }
        }

        return NULL;
    } else {
        /* Simple, we've got the handle already. */
        return disk->handle;
    }
}

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
     * protocol. If a device is a raw device (media->logical_partition == 0),
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

        /* Some iPXE developer decided it would be a great idea to put a dummy
         * block I/O protocol on network handles that just returns EFI_NO_MEDIA
         * for any function. Skip devices that support SNP to work around this. */
        if (efi_net_is_net_device(handles[i]))
            continue;

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

        disk->handle = handles[i];
        disk->media_id = media->media_id;
        disk->boot = handles[i] == efi_loaded_image->device_handle;
        disk->disk.ops = &efi_disk_ops;
        disk->disk.block_size = media->block_size;
        disk->disk.blocks = (media->media_present) ? media->last_block + 1 : 0;

        if (disk->boot)
            dprintf("efi: boot device is %pE\n", disk->path);

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

        /* Identify the parent device. */
        list_foreach_safe(&raw_devices, piter) {
            efi_disk_t *parent = list_entry(piter, efi_disk_t, disk.device.header);

            if (efi_is_child_device_node(parent->path, child->path)) {
                /* Mark the parent as the boot device if the partition is the
                 * boot partition. */
                if (child->boot)
                    parent->boot = true;

                if (last->type == EFI_DEVICE_PATH_TYPE_MEDIA) {
                    switch (last->subtype) {
                    case EFI_DEVICE_PATH_MEDIA_SUBTYPE_HD:
                        parent->disk.type = DISK_TYPE_HD;

                        /* If this is the boot partition, get its start LBA. */
                        if (child->boot) {
                            efi_device_path_hd_t *hd = (efi_device_path_hd_t *)last;
                            parent->boot_partition_lba = hd->partition_start;
                        }

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

        /* Find the boot directory. For a CD the boot path is invalid as it
         * refers to the embedded EFI system partition, which is not used for
         * anything by us except to store the boot file. */
        if (disk->boot && disk->disk.type != DISK_TYPE_CDROM) {
            efi_device_path_file_t *efi_path = (efi_device_path_file_t *)efi_loaded_image->file_path;
            size_t len;
            uint8_t *path __cleanup_free = NULL;

            if (efi_path->header.type == EFI_DEVICE_PATH_TYPE_MEDIA &&
                efi_path->header.subtype == EFI_DEVICE_PATH_MEDIA_SUBTYPE_FILE)
            {
                len = (efi_path->header.length - sizeof(efi_device_path_file_t)) / sizeof(efi_char16_t);
                path = malloc(len * MAX_UTF8_PER_UTF16);

                len = utf16_to_utf8(path, efi_path->path, len);
                path[len] = 0;

                for (size_t i = 0; i < len; i++) {
                    if (path[i] == '\\')
                        path[i] = '/';
                }

                boot_directory = dirname((char *)path);
            } else {
                dprintf("efi: image path is not a file path, cannot determine boot directory\n");
            }
        }

        disk_device_register(&disk->disk, disk->boot);
    }
}
