/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief               BIOS disk device support.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <bios/bios.h>
#include <bios/disk.h>
#include <bios/multiboot.h>

#include <disk.h>
#include <loader.h>
#include <memory.h>

/** BIOS disk device structure. */
typedef struct bios_disk {
    disk_device_t disk;                 /**< Disk device header. */
    uint8_t id;                         /**< BIOS device ID. */
} bios_disk_t;

/** Maximum number of blocks per transfer. */
#define blocks_per_transfer(disk) ((BIOS_MEM_SIZE / disk->disk.block_size) - 1)

/** Read blocks from a BIOS disk device.
 * @param _disk         Disk device being read from.
 * @param buf           Buffer to read into.
 * @param count         Number of blocks to read.
 * @param lba           Block number to start reading from.
 * @return              Status code describing the result of the operation. */
static status_t bios_disk_read_blocks(disk_device_t *_disk, void *buf, size_t count, uint64_t lba) {
    bios_disk_t *disk = (bios_disk_t *)_disk;
    size_t transfers;

    /* Split large transfers up as we have limited space to transfer to. */
    transfers = round_up(count, blocks_per_transfer(disk)) / blocks_per_transfer(disk);

    for (size_t i = 0; i < transfers; i++) {
        disk_address_packet_t *dap = (disk_address_packet_t *)BIOS_MEM_BASE;
        void *dest = (void *)(BIOS_MEM_BASE + disk->disk.block_size);
        size_t num = min(count, blocks_per_transfer(disk));
        bios_regs_t regs;

        /* Fill in a disk address packet for the transfer. */
        dap->size = sizeof(*dap);
        dap->reserved1 = 0;
        dap->block_count = num;
        dap->buffer_offset = (ptr_t)dest;
        dap->buffer_segment = 0;
        dap->start_lba = lba + (i * blocks_per_transfer(disk));

        /* Perform the transfer. */
        bios_regs_init(&regs);
        regs.eax = INT13_EXT_READ;
        regs.edx = disk->id;
        regs.esi = BIOS_MEM_BASE;
        bios_call(0x13, &regs);
        if (regs.eflags & X86_FLAGS_CF) {
            dprintf("bios: read from %s failed with status 0x%x\n", disk->disk.device.name, regs.ax >> 8);
            return STATUS_DEVICE_ERROR;
        }

        /* Copy the transferred blocks to the buffer. */
        memcpy(buf, dest, disk->disk.block_size * num);

        buf += disk->disk.block_size * num;
        count -= num;
    }

    return STATUS_SUCCESS;
}

/** Operations for a BIOS disk device. */
static disk_ops_t bios_disk_ops = {
    .read_blocks = bios_disk_read_blocks,
};

/** Add the disk with the specified ID.
 * @param id            ID of the device. */
static void add_disk(uint8_t id) {
    drive_parameters_t *params = (drive_parameters_t *)BIOS_MEM_BASE;
    bios_regs_t regs;
    bios_disk_t *disk;

    /* Create a data structure for the device. */
    disk = malloc(sizeof(bios_disk_t));
    disk->disk.ops = &bios_disk_ops;
    disk->id = id;

    /* If this is the boot device, check if it is a CD drive. */
    if (id == bios_boot_device) {
        specification_packet_t *packet = (specification_packet_t *)BIOS_MEM_BASE;

        /* Use the bootable CD-ROM status function. */
        bios_regs_init(&regs);
        regs.eax = INT13_CDROM_GET_STATUS;
        regs.edx = id;
        regs.esi = (ptr_t)packet;
        bios_call(0x13, &regs);

        if (!(regs.eflags & X86_FLAGS_CF) && packet->drive_number == id) {
            /* Should be no emulation. */
            if (packet->media_type & 0xf) {
                dprintf("bios: boot CD should be no emulation\n");
                return;
            }

            /* Add the drive. We do not bother checking whether extensions are
             * supported here, as some BIOSes (Intel/AMI) return error from the
             * installation check call for CDs even though they are supported.
             * Additionally, there appears to be no way to get the size of a
             * CD - get drive parameters returns -1 for sector count on a CD. */
            disk->disk.type = DISK_TYPE_CDROM;
            disk->disk.block_size = 2048;
            disk->disk.blocks = ~0ULL;
            disk_device_register(&disk->disk);

            dprintf("bios: disk %-6s at 0x%x (block_size: %zu)\n",
                disk->disk.device.name, disk->id, disk->disk.block_size);
            return;
        }
    }

    /* Check for INT13 extensions support. */
    bios_regs_init(&regs);
    regs.eax = INT13_EXT_INSTALL_CHECK;
    regs.ebx = 0x55AA;
    regs.edx = id;
    bios_call(0x13, &regs);
    if (regs.eflags & X86_FLAGS_CF || (regs.ebx & 0xFFFF) != 0xAA55 || !(regs.ecx & (1<<0))) {
        dprintf("bios: device 0x%x does not support extensions, ignoring\n", id);
        return;
    }

    /* Get drive parameters. According to RBIL, some Phoenix BIOSes fail to
     * correctly handle the function if the flags word is not 0. Clear the
     * entire structure to be on the safe side. */
    memset(params, 0, sizeof(drive_parameters_t));
    params->size = sizeof(drive_parameters_t);
    bios_regs_init(&regs);
    regs.eax = INT13_EXT_GET_DRIVE_PARAMETERS;
    regs.edx = id;
    regs.esi = (ptr_t)params;
    bios_call(0x13, &regs);
    if (regs.eflags & X86_FLAGS_CF || !params->sector_count || !params->sector_size) {
        dprintf("bios: failed to obtain drive parameters for device 0x%x\n", id);
        return;
    }

    /* Add the drive. */
    disk->disk.type = DISK_TYPE_HD;
    disk->disk.block_size = params->sector_size;
    disk->disk.blocks = params->sector_count;
    disk_device_register(&disk->disk);

    dprintf("bios: disk %-6s at 0x%x (block_size: %zu, blocks: %" PRIu64 ")\n",
        disk->disk.device.name, disk->id, disk->disk.block_size,
        disk->disk.blocks);
}

/** Detect and register all disk devices. */
void bios_disk_init(void) {
    bios_regs_t regs;
    uint8_t count;
    bool separate_boot;

    /* If booted from Multiboot, retrieve boot device ID from there. */
    if (multiboot_magic == MULTIBOOT_LOADER_MAGIC) {
        bios_boot_device = (multiboot_info.boot_device & 0xFF000000) >> 24;

        dprintf("bios: boot device ID is 0x%x, partition ID is 0x%x\n",
            bios_boot_device,
            (multiboot_info.boot_device & 0x00FF0000) >> 16);
    } else {
        dprintf("bios: boot device ID is 0x%x, partition offset is 0x%" PRIx64 "\n",
            bios_boot_device, bios_boot_partition);
    }

    /* Use the Get Drive Parameters call to get the number of drives. */
    bios_regs_init(&regs);
    regs.eax = INT13_GET_DRIVE_PARAMETERS;
    regs.edx = 0x80;
    bios_call(0x13, &regs);
    count = (regs.eflags & X86_FLAGS_CF) ? 0 : (regs.edx & 0xFF);

    /* Boot device may not be included in this count if it is a CD drive. */
    separate_boot = bios_boot_device < 0x80 || bios_boot_device >= count + 0x80;

    dprintf("bios: detected %u disks:\n", (separate_boot) ? count + 1 : count);

    /* Probe all drives. */
    for (uint8_t id = 0x80; id < count + 0x80; id++)
        add_disk(id);

    /* Add the boot device if it is separate. */
    if (separate_boot)
        add_disk(bios_boot_device);
}
