/*
 * Copyright (C) 2014-2015 Alex Smith
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
 * @brief               BIOS platform console functions.
 */

#include <arch/io.h>

#include <bios/bios.h>
#include <bios/console.h>
#include <bios/video.h>

#include <drivers/serial/ns16550.h>

#include <lib/utility.h>

#include <console.h>
#include <loader.h>

/** Serial port definitions. */
#define SERIAL_CLOCK        1843200

/** Array of serial port numbers. */
static uint16_t serial_ports[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };

/** Check for a character from the console.
 * @param console       Console input device.
 * @return              Whether a character is available. */
static bool bios_console_poll(console_in_t *console) {
    bios_regs_t regs;

    bios_regs_init(&regs);
    regs.eax = 0x0100;
    bios_call(0x16, &regs);
    return !(regs.eflags & X86_FLAGS_ZF);
}

/** Read a character from the console.
 * @param console       Console input device.
 * @return              Character read. */
static uint16_t bios_console_getc(console_in_t *console) {
    bios_regs_t regs;
    uint8_t ascii, scan;

    bios_regs_init(&regs);

    /* INT16 AH=00h on Apple's BIOS emulation will hang forever if there is no
     * key available, so loop trying to poll for one first. */
    do {
        regs.eax = 0x0100;
        bios_call(0x16, &regs);
    } while (regs.eflags & X86_FLAGS_ZF);

    /* Get the key code. */
    regs.eax = 0x0000;
    bios_call(0x16, &regs);

    /* Convert certain scan codes to special keys. */
    ascii = regs.eax & 0xff;
    scan = (regs.eax >> 8) & 0xff;
    switch (scan) {
    case 0x48:
        return CONSOLE_KEY_UP;
    case 0x50:
        return CONSOLE_KEY_DOWN;
    case 0x4b:
        return CONSOLE_KEY_LEFT;
    case 0x4d:
        return CONSOLE_KEY_RIGHT;
    case 0x47:
        return CONSOLE_KEY_HOME;
    case 0x4f:
        return CONSOLE_KEY_END;
    case 0x49:
        return CONSOLE_KEY_PGUP;
    case 0x51:
        return CONSOLE_KEY_PGDOWN;
    case 0x53:
        return 0x7f;
    case 0x3b ... 0x44:
        return CONSOLE_KEY_F1 + (scan - 0x3b);
    default:
        /* Convert CR to LF. */
        return (ascii == '\r') ? '\n' : ascii;
    }
}

/** BIOS console input operations. */
static console_in_ops_t bios_console_in_ops = {
    .poll = bios_console_poll,
    .getc = bios_console_getc,
};

/** BIOS console input device. */
static console_in_t bios_console_in = {
    .ops = &bios_console_in_ops,
};

/** Initialize the console. */
void target_console_init(void) {
    serial_config_t config;

    config.baud_rate = SERIAL_DEFAULT_BAUD_RATE;
    config.data_bits = SERIAL_DEFAULT_DATA_BITS;
    config.parity = SERIAL_DEFAULT_PARITY;
    config.stop_bits = SERIAL_DEFAULT_STOP_BITS;

    /* Register serial ports. */
    for (size_t i = 0; i < array_size(serial_ports); i++) {
        serial_port_t *port = ns16550_register(serial_ports[i], i, SERIAL_CLOCK);

        if (port) {
            serial_port_config(port, &config);

            /* Register the first as the debug console. */
            #ifdef CONFIG_DEBUG
                if (i == 0)
                    console_set_debug(&port->console);
            #endif
        }
    }

    /* Use BIOS for input. */
    primary_console.in = &bios_console_in;
}

/** Reset the console to original state. */
void bios_console_reset(void) {
    bios_regs_t regs;

    /* Set VGA text mode. */
    bios_regs_init(&regs);
    regs.eax = VBE_FUNCTION_SET_MODE;
    regs.ebx = 0x3;
    bios_call(0x10, &regs);

    /* Set display page to the first. */
    bios_regs_init(&regs);
    regs.ax = 0x0500;
    bios_call(0x10, &regs);

    /* Move the cursor to (0, 0). */
    bios_regs_init(&regs);
    regs.ax = 0x0200;
    bios_call(0x10, &regs);
}
