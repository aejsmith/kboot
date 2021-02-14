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

/* Early console configuration. */
#if CONFIG_DEBUG && CONFIG_DT_SINGLE_PLATFORM
    #if CONFIG_DT_PLATFORM_BCM2837
        /* Raspberry Pi 3 - mini UART is the primary UART by default. It's best
         * if we don't mess around with the configuration on this before we've
         * parsed the DT - the clock rate is linked to the VPU core clock which,
         * although fixed when the UART is enabled, can vary with the
         * configuration file. */
        #define EARLY_CONSOLE_NS16550       1
        #define EARLY_CONSOLE_ADDR          0x3f215040
        #define EARLY_CONSOLE_CLOCK         0
    #elif CONFIG_DT_PLATFORM_VIRT_ARM64
        /* QEMU virt machine. According to the docs we shouldn't rely on any
         * fixed address for it, but *shrug*, it's hardcoded to this in the code
         * right now. */
        #define EARLY_CONSOLE_PL011         1
        #define EARLY_CONSOLE_ADDR          0x9000000
        #define EARLY_CONSOLE_CLOCK         24000000
    #endif
#endif

/** Initialize an early debug console. */
void dt_early_console_init(void) {
    #ifdef EARLY_CONSOLE_ADDR
        serial_port_t *port = NULL;

        #if EARLY_CONSOLE_NS16550
            port = ns16550_register(EARLY_CONSOLE_ADDR, 0, EARLY_CONSOLE_CLOCK);
        #elif EARLY_CONSOLE_PL011
            port = pl011_register(EARLY_CONSOLE_ADDR, 0, EARLY_CONSOLE_CLOCK);
        #endif

        if (port) {
            #if EARLY_CONSOLE_CLOCK
                serial_config_t config;

                config.baud_rate = SERIAL_DEFAULT_BAUD_RATE;
                config.data_bits = SERIAL_DEFAULT_DATA_BITS;
                config.parity = SERIAL_DEFAULT_PARITY;
                config.stop_bits = SERIAL_DEFAULT_STOP_BITS;

                serial_port_config(port, &config);
            #endif

            console_set_debug(&port->console);
        }
    #endif
}

/** Initialize the console. */
void target_console_init(void) {
    /* TODO: Get UART from DT. */
}
