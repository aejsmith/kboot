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
 * @brief               DT platform console functions.
 */

#include <drivers/serial/ns16550.h>
#include <drivers/serial/pl011.h>

#include <dt/console.h>

#include <console.h>
#include <dt.h>
#include <loader.h>

/* Early console configuration. */
#if CONFIG_DEBUG && CONFIG_DT_SINGLE_PLATFORM
    #if CONFIG_DT_PLATFORM_BCM2837
        /* Raspberry Pi 3 - mini UART is the primary UART by default. */
        #define EARLY_CONSOLE_NS16550       1
        #define EARLY_CONSOLE_ADDR          0x3f215040
        #define EARLY_CONSOLE_TYPE          NS16550_TYPE_BCM2835_AUX
    #elif CONFIG_DT_PLATFORM_BCM2711
        /* Raspberry Pi 4 - mini UART is the primary UART by default. */
        #define EARLY_CONSOLE_NS16550       1
        #define EARLY_CONSOLE_ADDR          0xfe215040
        #define EARLY_CONSOLE_TYPE          NS16550_TYPE_BCM2835_AUX
    #elif CONFIG_DT_PLATFORM_BCM2712
        /* Raspberry Pi 5 - PL011 on the debug port. */
        #define EARLY_CONSOLE_PL011         1
        #define EARLY_CONSOLE_ADDR          0x107d001000ul
    #elif CONFIG_DT_PLATFORM_VIRT_ARM64
        /* QEMU virt machine. According to the docs we shouldn't rely on any
         * fixed address for it, but *shrug*, it's hardcoded to this in the code
         * right now. */
        #define EARLY_CONSOLE_PL011         1
        #define EARLY_CONSOLE_ADDR          0x9000000
    #endif
#endif

#ifdef EARLY_CONSOLE_ADDR
    static serial_port_t *early_console;
#endif

/** Initialize an early debug console. */
void dt_early_console_init(void) {
    #ifdef EARLY_CONSOLE_ADDR
        serial_port_t *port = NULL;

        #if EARLY_CONSOLE_NS16550
            port = ns16550_register(EARLY_CONSOLE_ADDR, EARLY_CONSOLE_TYPE, 999, 0);
        #elif EARLY_CONSOLE_PL011
            port = pl011_register(EARLY_CONSOLE_ADDR, 999, 0);
        #endif

        if (port) {
            console_set_debug(&port->console);
            early_console = port;
        }
    #endif
}

/** Initialize the console. */
void target_console_init(void) {
    const char *path = NULL;
    int len, dev = -1;

    int offset = fdt_path_offset(fdt_address, "/chosen");
    if (offset >= 0) {
        path = fdt_getprop(fdt_address, offset, "stdout-path", &len);
        if (path && len > 0) {
            len--;

            const char *marker = strchr(path, ':');
            if (marker)
                len = marker - path;

            dev = fdt_path_offset_namelen(fdt_address, path, len);
        }
    }

    if (dev < 0) {
        /* Raspberry Pi doesn't set stdout-path, try this instead. */
        offset = fdt_path_offset(fdt_address, "/aliases");
        if (offset >= 0) {
            path = fdt_getprop(fdt_address, offset, "serial0", &len);
            if (path && len > 0) {
                len--;
                dev = fdt_path_offset_namelen(fdt_address, path, len);
            }
        }
    }

    dprintf("dt: console path is '%s'\n", path);

    if (dev < 0 || !dt_is_available(dev))
        return;

    serial_port_t *port = dt_serial_port_register(dev);
    if (!port)
        return;

    #ifdef EARLY_CONSOLE_ADDR
        console_unregister(&early_console->console);
        early_console = NULL;
    #endif

    console_set_debug(&port->console);
}
