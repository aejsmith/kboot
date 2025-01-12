/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               RAM disk driver.
 */

#ifndef __DRIVERS_DISK_RAMDISK_H
#define __DRIVERS_DISK_RAMDISK_H

#include <status.h>
#include <types.h>

#ifdef CONFIG_DRIVER_DISK_RAMDISK

extern void ramdisk_create(const char *name, void *data, size_t size, bool boot);

#endif /* CONFIG_DRIVER_DISK_RAMDISK */
#endif /* __DRIVERS_DISK_RAMDISK_H */
