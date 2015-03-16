/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               PC platform console functions.
 */

#include <arch/io.h>

#include <drivers/serial/ns16550.h>

#include <test.h>

/** Serial port definitions. */
#define SERIAL_PORT         0x3f8
#define SERIAL_CLOCK        1843200
#define SERIAL_BAUD_RATE    115200

/** Initialize the debug console. */
void debug_console_init(void) {
    uint8_t status;

    /* Initialize the serial port as the debug console. TODO: Disable for
     * non-debug builds? */
    status = in8(SERIAL_PORT + 6);
    if ((status & ((1 << 4) | (1 << 5))) && status != 0xff) {
        ns16550_init(SERIAL_PORT);
        ns16550_config(SERIAL_PORT, SERIAL_CLOCK, SERIAL_BAUD_RATE);
    }
}
