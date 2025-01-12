/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               VGA console implementation.
 */

#ifndef __DRIVERS_CONSOLE_VGA_H
#define __DRIVERS_CONSOLE_VGA_H

#include <console.h>

/** VGA register definitions. */
#define VGA_CRTC_INDEX      0x3d4
#define VGA_CRTC_DATA       0x3d5

extern console_out_t *vga_console_create(void);

#endif /* __DRIVERS_CONSOLE_VGA_H */
