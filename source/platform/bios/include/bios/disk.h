/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BIOS disk interface definitions.
 */

#ifndef __BIOS_DISK_H
#define __BIOS_DISK_H

/** INT13 function definitions. */
#define INT13_GET_DRIVE_PARAMETERS      0x800   /**< Get Drive Parameters. */
#define INT13_EXT_INSTALL_CHECK         0x4100  /**< INT13 Extensions - Installation Check. */
#define INT13_EXT_READ                  0x4200  /**< INT13 Extensions - Extended Read. */
#define INT13_EXT_GET_DRIVE_PARAMETERS  0x4800  /**< INT13 Extensions - Get Drive Parameters. */
#define INT13_CDROM_GET_STATUS          0x4b01  /**< Bootable CD-ROM - Get Status. */

#ifndef __ASM__

#include <disk.h>

/** Drive parameters structure. We only care about the EDD 1.x fields. */
typedef struct drive_parameters {
    uint16_t size;
    uint16_t flags;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t spt;
    uint64_t sector_count;
    uint16_t sector_size;
} __packed drive_parameters_t;

/** Disk address packet structure. */
typedef struct disk_address_packet {
    uint8_t size;
    uint8_t reserved1;
    uint16_t block_count;
    uint16_t buffer_offset;
    uint16_t buffer_segment;
    uint64_t start_lba;
} __packed disk_address_packet_t;

/** Bootable CD-ROM Specification Packet. */
typedef struct specification_packet {
    uint8_t size;
    uint8_t media_type;
    uint8_t drive_number;
    uint8_t controller_num;
    uint32_t image_lba;
    uint16_t device_spec;
} __packed specification_packet_t;

extern uint8_t bios_boot_device;
extern uint64_t bios_boot_partition;
extern char bios_boot_path[];

extern uint8_t bios_disk_get_id(disk_device_t *disk);

extern void bios_disk_init(void);

#endif /* __ASM__ */
#endif /* __BIOS_DISK_H */
