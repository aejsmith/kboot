/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BIOS platform Multiboot support.
 */

#ifndef __BIOS_MULTIBOOT_H
#define __BIOS_MULTIBOOT_H

#include <x86/multiboot.h>

extern uint32_t multiboot_magic;
extern multiboot_info_t multiboot_info;

/** @return             Whether booted via Multiboot. */
static inline bool multiboot_valid(void) {
    return multiboot_magic == MULTIBOOT_LOADER_MAGIC;
}

extern void multiboot_init(void);

#endif /* __BIOS_MULTIBOOT_H */
