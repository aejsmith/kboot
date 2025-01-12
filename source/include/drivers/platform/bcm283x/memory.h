/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BCM283x memory definitions.
 */

#ifndef __DRIVERS_PLATFORM_BCM283X_MEMORY_H
#define __DRIVERS_PLATFORM_BCM283X_MEMORY_H

#include <types.h>

/** Convert a BCM283x bus address to a CPU physical address. */
static phys_ptr_t bcm283x_bus_to_phys(uint32_t bus) {
    /* This is good for SDRAM on all RPi versions, but not for I/O. It's enough
     * for the current use case (converting framebuffer address from firmware)
     * but if we need to handle I/O addresses here in future then we'll have to
     * do some platform detection from DT. */
    return bus & ~0xc0000000;
}

#endif /* __DRIVERS_PLATFORM_BCM283X_MEMORY_H */
