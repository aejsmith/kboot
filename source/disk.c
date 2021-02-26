/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Disk device management.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <config.h>
#include <disk.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

static void probe_disk(disk_device_t *disk);

/** Next disk IDs. */
static uint8_t next_disk_ids[DISK_TYPE_FLOPPY + 1];

/** Disk type names. */
static const char *const disk_type_names[] = {
    [DISK_TYPE_HD] = "hd",
    [DISK_TYPE_CDROM] = "cdrom",
    [DISK_TYPE_FLOPPY] = "floppy",
};

/** Read from a disk.
 * @param device        Device to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset in the disk to read from.
 * @return              Whether the read was successful. */
static status_t disk_device_read(device_t *device, void *buf, size_t count, offset_t offset) {
    disk_device_t *disk = (disk_device_t *)device;
    void *tmp __cleanup_free = NULL;
    uint64_t start, end;
    size_t size;
    status_t ret;

    if (offset + count > disk->blocks * disk->block_size)
        return STATUS_END_OF_FILE;

    /* Now work out the start block and the end block. Subtract one from count
     * to prevent end from going onto the next block when the offset plus the
     * count is an exact multiple of the block size. */
    start = offset / disk->block_size;
    end = (offset + (count - 1)) / disk->block_size;

    /* If we're not starting on a block boundary, we need to do a partial
     * transfer on the initial block to get up to a block boundary. If the
     * transfer only goes across one block, this will handle it. */
    if (offset % disk->block_size) {
        tmp = malloc(disk->block_size);

        /* Read the block into the temporary buffer. */
        ret = disk->ops->read_blocks(disk, tmp, 1, start);
        if (ret != STATUS_SUCCESS)
            return ret;

        size = (start == end) ? count : disk->block_size - (size_t)(offset % disk->block_size);
        memcpy(buf, tmp + (offset % disk->block_size), size);
        buf += size;
        count -= size;
        start++;
    }

    /* Handle any full blocks. */
    while (count / disk->block_size) {
        void *dest;

        /* Some disk backends (e.g. EFI) cannot handle unaligned buffers. */
        if ((ptr_t)buf % 8) {
            if (!tmp)
                tmp = malloc(disk->block_size);

            dest = tmp;
            size = 1;
        } else {
            dest = buf;
            size = count / disk->block_size;
        }

        ret = disk->ops->read_blocks(disk, dest, size, start);
        if (ret != STATUS_SUCCESS)
            return ret;

        if (dest != buf)
            memcpy(buf, dest, disk->block_size);

        buf += size * disk->block_size;
        count -= size * disk->block_size;
        start += size;
    }

    /* Handle anything that's left. */
    if (count) {
        if (!tmp)
            tmp = malloc(disk->block_size);

        ret = disk->ops->read_blocks(disk, tmp, 1, start);
        if (ret != STATUS_SUCCESS)
            return ret;

        memcpy(buf, tmp, count);
    }

    return STATUS_SUCCESS;
}

/** Get disk device identification information.
 * @param device        Device to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void disk_device_identify(device_t *device, device_identify_t type, char *buf, size_t size) {
    disk_device_t *disk = (disk_device_t *)device;

    if (type == DEVICE_IDENTIFY_LONG) {
        size_t ret = snprintf(buf, size,
            "block size = %zu\n"
            "blocks     = %" PRIu64 "\n",
            disk->block_size, disk->blocks);
        buf += ret;
        size -= ret;
    }

    if (disk->ops->identify)
        disk->ops->identify(disk, type, buf, size);
}

/** Disk device operations. */
static device_ops_t disk_device_ops = {
    .read = disk_device_read,
    .identify = disk_device_identify,
};

/** Read blocks from a partition.
 * @param disk          Disk being read from.
 * @param buf           Buffer to read into.
 * @param count         Number of blocks to read.
 * @param lba           Block number to start reading from.
 * @return              Status code describing the result of the operation. */
static status_t partition_read_blocks(disk_device_t *disk, void *buf, size_t count, uint64_t lba) {
    return disk->parent->ops->read_blocks(disk->parent, buf, count, lba + disk->offset);
}

/** Get partition identification information.
 * @param disk          Disk to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void partition_identify(disk_device_t *disk, device_identify_t type, char *buf, size_t size) {
    if (type == DEVICE_IDENTIFY_SHORT) {
        snprintf(buf, size,
            "%s partition %" PRIu8 " @ %" PRIu64,
            disk->parent->partition_ops->name, disk->id, disk->offset);
    }
}

/** Operations for a partition. */
static disk_ops_t partition_disk_ops = {
    .read_blocks = partition_read_blocks,
    .identify = partition_identify,
};

/** Add a partition to a disk device.
 * @param parent        Parent of the partition.
 * @param id            ID of the partition.
 * @param lba           Start LBA.
 * @param blocks        Size in blocks. */
static void add_partition(disk_device_t *parent, uint8_t id, uint64_t lba, uint64_t blocks) {
    disk_device_t *partition;
    char *name;

    partition = malloc(sizeof(*partition));
    partition->device.type = DEVICE_TYPE_DISK;
    partition->device.ops = &disk_device_ops;
    partition->type = parent->type;
    partition->ops = &partition_disk_ops;
    partition->blocks = blocks;
    partition->block_size = parent->block_size;
    partition->id = id;
    partition->parent = parent;
    partition->offset = lba;

    name = malloc(16);
    snprintf(name, 16, "%s,%u", parent->device.name, id);
    partition->device.name = name;

    list_init(&partition->link);
    list_append(&parent->partitions, &partition->link);

    device_register(&partition->device);

    /* Check if this is the boot partition. */
    if (boot_device == &parent->device && parent->ops->is_boot_partition) {
        if (parent->ops->is_boot_partition(parent, id, lba))
            boot_device = &partition->device;
    }

    probe_disk(partition);
}

/** Probe a disk device's contents.
 * @param disk          Disk device to probe. */
static void probe_disk(disk_device_t *disk) {
    if (!disk->blocks)
        return;

    /* Probe for filesystems. */
    disk->device.mount = fs_probe(&disk->device);

    if (!disk->device.mount) {
        /* Check for a partition table on the device. */
        builtin_foreach(BUILTIN_TYPE_PARTITION, partition_ops_t, ops) {
            if (ops->iterate(disk, add_partition)) {
                disk->partition_ops = ops;
                break;
            }
        }
    }
}

/** Register a disk device.
 * @param disk          Disk device to register (fields marked in structure
 *                      should be initialized).
 * @param boot          Whether the device is the boot device or contains the
 *                      boot partition. */
void disk_device_register(disk_device_t *disk, bool boot) {
    char *name;

    list_init(&disk->partitions);
    disk->parent = NULL;
    disk->partition_ops = NULL;

    /* Assign an ID for the disk and name it. */
    disk->id = next_disk_ids[disk->type]++;
    name = malloc(16);
    snprintf(name, 16, "%s%u", disk_type_names[disk->type], disk->id);

    /* Add the device. */
    disk->device.type = DEVICE_TYPE_DISK;
    disk->device.ops = &disk_device_ops;
    disk->device.name = name;
    device_register(&disk->device);

    if (boot)
        boot_device = &disk->device;

    probe_disk(disk);
}

/*
 * Disk image support.
 */

typedef struct disk_image {
    disk_device_t disk;                 /**< Disk device header. */

    fs_handle_t *source;                /**< Source file handle. */
} disk_image_t;

static status_t disk_image_read_blocks(disk_device_t *_disk, void *buf, size_t count, uint64_t block) {
    disk_image_t *disk = (disk_image_t *)_disk;

    size_t size     = count * disk->disk.block_size;
    uint64_t offset = block * disk->disk.block_size;

    /* File size may not be block-aligned. */
    size_t padding = (offset + size > disk->source->size) ? disk->source->size - offset : 0;
    size -= padding;

    status_t ret = fs_read(disk->source, buf, size, offset);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (padding > 0)
        memset((uint8_t *)buf + size, 0, padding);

    return STATUS_SUCCESS;
}

static bool disk_image_is_boot_partition(disk_device_t *_disk, uint8_t id, uint64_t lba) {
    /* Assume partition 0 is the boot partition if the image has been set as the
     * boot device. */
    return id == 1;
}

static void disk_image_identify(disk_device_t *_disk, device_identify_t type, char *buf, size_t size) {
    if (type == DEVICE_IDENTIFY_SHORT)
        snprintf(buf, size, "Disk image");
}

static disk_ops_t disk_image_ops = {
    .read_blocks       = disk_image_read_blocks,
    .is_boot_partition = disk_image_is_boot_partition,
    .identify          = disk_image_identify,
};

/**
 * Register a disk image. This creates a new disk device that is backed by a
 * file.
 *
 * @param name          Name for the device.
 * @param handle        File handle.
 * @param boot          Whether the disk is the boot device.
 */
void disk_image_register(const char *name, fs_handle_t *handle, bool boot) {
    disk_image_t *image = malloc(sizeof(*image));

    list_init(&image->disk.partitions);

    image->disk.device.type   = DEVICE_TYPE_DISK;
    image->disk.device.ops    = &disk_device_ops;
    image->disk.device.name   = strdup(name);
    image->disk.type          = DISK_TYPE_HD;
    image->disk.ops           = &disk_image_ops;
    image->disk.parent        = NULL;
    image->disk.partition_ops = NULL;
    image->disk.id            = 0;
    image->source             = handle;

    /* If the image is on a disk, use its block size so we match I/O size there. */
    image->disk.block_size = 512;
    if (handle->mount->device && handle->mount->device->type == DEVICE_TYPE_DISK) {
        disk_device_t *disk = (disk_device_t *)handle->mount->device;
        image->disk.block_size = disk->block_size;
    }

    /* Round up to block size, when reading we'll pad with 0 if not a multiple. */
    image->disk.blocks = round_up(handle->size, image->disk.block_size) / image->disk.block_size;

    device_register(&image->disk.device);

    if (boot)
        boot_device = &image->disk.device;

    probe_disk(&image->disk);
}

/** Mount a disk image.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_diskimage(value_list_t *args) {
    // command should open compressed, check existance of devic.

    if (args->count != 2 ||
        args->values[0].type != VALUE_TYPE_STRING ||
        args->values[1].type != VALUE_TYPE_STRING)
    {
        config_error("Invalid arguments");
        return false;
    }

    const char *name = args->values[0].string;
    const char *path = args->values[1].string;

    if (strchr(name, '(') || strchr(name, ')') || strchr(name, ',') || strchr(name, '/')) {
        config_error("Device name '%s' is invalid", name);
        return false;
    }

    if (device_lookup(name)) {
        config_error("Device '%s' already exists", name);
        return false;
    }

    fs_handle_t *handle;
    status_t ret = fs_open(path, NULL, FILE_TYPE_REGULAR, FS_OPEN_DECOMPRESS, &handle);
    if (ret != STATUS_SUCCESS) {
        config_error("Error opening '%s': %pS", path, ret);
        return false;
    }

    disk_image_register(name, handle, false);
    return true;
}

BUILTIN_COMMAND("diskimage", "Mount a disk image", config_cmd_diskimage);
