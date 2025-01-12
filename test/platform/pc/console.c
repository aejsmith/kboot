/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               PC platform console functions.
 */

#include <arch/io.h>

#include <drivers/serial/ns16550.h>

#include <console.h>
#include <test.h>

KBOOT_VIDEO(KBOOT_VIDEO_VGA | KBOOT_VIDEO_LFB, 0, 0, 0);

/** Serial port definitions. */
#define SERIAL_PORT         0x3f8
#define SERIAL_CLOCK        1843200

/** Initialize the fallback debug console. */
void platform_debug_console_init(void) {
    serial_port_t *port = ns16550_register(SERIAL_PORT, NS16550_TYPE_STANDARD, 0, SERIAL_CLOCK);

    if (port)
        debug_console = &port->console;
}
