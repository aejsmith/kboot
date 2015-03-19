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
 * @brief               Serial console implementation.
 */

#include <drivers/console/serial.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <config.h>
#include <loader.h>
#include <memory.h>

/** Colour escape sequence table. */
static struct { bool bold; uint8_t num; } colour_table[] = {
    [COLOUR_BLACK] = { false, 0 },
    [COLOUR_BLUE] = { false, 4 },
    [COLOUR_GREEN] = { false, 2 },
    [COLOUR_CYAN] = { false, 6 },
    [COLOUR_RED] = { false, 1 },
    [COLOUR_MAGENTA] = { false, 5 },
    [COLOUR_BROWN] = { false, 3 },
    [COLOUR_LIGHT_GREY] = { false, 7 },
    [COLOUR_GREY] = { true, 0 },
    [COLOUR_LIGHT_BLUE] = { true, 4 },
    [COLOUR_LIGHT_GREEN] = { true, 2 },
    [COLOUR_LIGHT_CYAN] = { true, 6 },
    [COLOUR_LIGHT_RED] = { true, 1 },
    [COLOUR_LIGHT_MAGENTA] = { true, 5 },
    [COLOUR_YELLOW] = { true, 3 },
    [COLOUR_WHITE] = { true, 7 },
};

/** Reset the console to a default state.
 * @param console       Console output device. */
static void serial_console_reset(console_out_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    /* Reset attributes. */
    console_printf(&port->console, "\033[0m");
}

/** Write a character to a serial console.
 * @param console       Console output device.
 * @param ch            Character to write. */
static void serial_console_putc(console_out_t *console, char ch) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    if (ch == '\n')
        serial_console_putc(console, '\r');

    port->ops->write(port, ch);

    while (!port->ops->tx_empty(port))
        arch_pause();
}

/** Set the current colours.
 * @param console       Console output device.
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void serial_console_set_colour(console_out_t *console, colour_t fg, colour_t bg) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    /* Reset to default to begin with. */
    console_printf(&port->console, "\033[0m");

    if (fg != COLOUR_DEFAULT)
        console_printf(&port->console, "\033[%s%um", (colour_table[fg].bold) ? "1;" : "", 30 + colour_table[fg].num);
    if (bg != COLOUR_DEFAULT)
        console_printf(&port->console, "\033[%s%um", (colour_table[bg].bold) ? "1;" : "", 40 + colour_table[bg].num);
}

/** Serial console output operations. */
static console_out_ops_t serial_console_out_ops = {
    .reset = serial_console_reset,
    .putc = serial_console_putc,
    .set_colour = serial_console_set_colour,
};

/** Check for a character from a serial console.
 * @param console       Console input device.
 * @return              Whether a character is available. */
static bool serial_console_poll(console_in_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, in);

    if (port->next_ch)
        return true;

    while (!port->ops->rx_empty(port)) {
        uint8_t ch = port->ops->read(port);

        /* Convert CR to NL, and DEL to Backspace. */
        if (ch == '\r') {
            ch = '\n';
        } else if (ch == 0x7f) {
            ch = '\b';
        }

        if (port->esc_len < 0) {
            if (ch == '\e') {
                port->esc_len = 0;
                continue;
            } else {
                port->next_ch = ch;
                return true;
            }
        } else {
            port->esc_buffer[port->esc_len++] = ch;

            /* Check for known sequences. */
            if (port->esc_len == 2) {
                if (port->esc_buffer[0] == '[') {
                    if (port->esc_buffer[1] == 'A') {
                        port->next_ch = CONSOLE_KEY_UP;
                    } else if (port->esc_buffer[1] == 'B') {
                        port->next_ch = CONSOLE_KEY_DOWN;
                    } else if (port->esc_buffer[1] == 'D') {
                        port->next_ch = CONSOLE_KEY_LEFT;
                    } else if (port->esc_buffer[1] == 'C') {
                        port->next_ch = CONSOLE_KEY_RIGHT;
                    } else if (port->esc_buffer[1] == 'H') {
                        port->next_ch = CONSOLE_KEY_HOME;
                    } else if (port->esc_buffer[1] == 'F') {
                        port->next_ch = CONSOLE_KEY_END;
                    }
                } else if (port->esc_buffer[0] == 'O') {
                    if (port->esc_buffer[1] == 'P') {
                        port->next_ch = CONSOLE_KEY_F1;
                    } else if (port->esc_buffer[1] == 'Q') {
                        port->next_ch = CONSOLE_KEY_F2;
                    } else if (port->esc_buffer[1] == 'R') {
                        port->next_ch = CONSOLE_KEY_F3;
                    } else if (port->esc_buffer[1] == 'S') {
                        port->next_ch = CONSOLE_KEY_F4;
                    }
                }
            } else if (port->esc_len == 3) {
                if (port->esc_buffer[0] == '[' && port->esc_buffer[1] == '3' && port->esc_buffer[2] == '~')
                    port->next_ch = 0x7f;
            } else if (port->esc_len == 4) {
                if (port->esc_buffer[0] == '[' && port->esc_buffer[3] == '~') {
                    if (port->esc_buffer[1] == '1') {
                        if (port->esc_buffer[2] == '5') {
                            port->next_ch = CONSOLE_KEY_F5;
                        } else if (port->esc_buffer[2] == '7') {
                            port->next_ch = CONSOLE_KEY_F6;
                        } else if (port->esc_buffer[2] == '8') {
                            port->next_ch = CONSOLE_KEY_F7;
                        } else if (port->esc_buffer[2] == '9') {
                            port->next_ch = CONSOLE_KEY_F8;
                        }
                    } else if (port->esc_buffer[1] == '2') {
                        if (port->esc_buffer[2] == '0') {
                            port->next_ch = CONSOLE_KEY_F9;
                        } else if (port->esc_buffer[2] == '1') {
                            port->next_ch = CONSOLE_KEY_F10;
                        }
                    }
                }
            }

            if (port->next_ch || port->esc_len == ESC_BUFFER_LEN)
                port->esc_len = -1;

            if (port->next_ch)
                return true;
        }
    }

    return false;
}

/** Read a character from a serial console.
 * @param console       Console input device.
 * @return              Character read. */
static uint16_t serial_console_getc(console_in_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, in);
    uint16_t ret;

    if (!port->next_ch) {
        while (!serial_console_poll(console))
            ;
    }

    ret = port->next_ch;
    port->next_ch = 0;
    return ret;
}

/** Serial console input operations. */
static console_in_ops_t serial_console_in_ops = {
    .poll = serial_console_poll,
    .getc = serial_console_getc,
};

/** Register a serial port.
 * @param port          Port to register.
 * @return              Status code describing result of the operation. */
status_t serial_port_register(serial_port_t *port) {
    char *name;
    size_t count;

    name = malloc(8);
    snprintf(name, 8, "serial%u", port->index);

    port->console.name = name;
    port->console.out = &port->out;
    port->console.in = &port->in;
    port->out.ops = &serial_console_out_ops;
    port->in.ops = &serial_console_in_ops;
    port->next_ch = 0;
    port->esc_len = -1;

    /* Ensure the transmit esc_buffer is empty. Assume port is unusable if it never
     * empties. */
    count = 0;
    while (!port->ops->tx_empty(port)) {
        if (++count == 100000) {
            free(name);
            return STATUS_DEVICE_ERROR;
        }
    }

    console_register(&port->console);
    return STATUS_SUCCESS;
}

/**
 * Configuration commands.
 */

#ifndef __TEST

/** Configure a serial port.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_serial(value_list_t *args) {
    console_t *console;
    serial_port_t *port;
    serial_config_t config;
    status_t ret;

    if (args->count < 1 || args->values[0].type != VALUE_TYPE_STRING)
        goto err_inval;

    console = console_lookup(args->values[0].string);
    if (!console) {
        config_error("Console '%s' not found", args->values[0].string);
        return false;
    } else if (!console->out || console->out->ops != &serial_console_out_ops) {
        config_error("Console '%s' is not a serial port", args->values[0].string);
        return false;
    }

    port = container_of(console, serial_port_t, console);

    if (args->count >= 2) {
        if (args->values[1].type != VALUE_TYPE_INTEGER)
            goto err_inval;

        switch (args->values[1].integer) {
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
            break;
        default:
            config_error("Baud rate %" PRIu64 " is invalid", args->values[1].integer);
            return false;
        }

        config.baud_rate = args->values[1].integer;
    } else {
        config.baud_rate = SERIAL_DEFAULT_BAUD_RATE;
    }

    if (args->count >= 3) {
        if (args->values[2].type != VALUE_TYPE_INTEGER)
            goto err_inval;

        if (args->values[2].integer < 5 || args->values[2].integer > 8) {
            config_error("Data bits value %" PRIu64 " is invalid", args->values[2].integer);
            return false;
        }

        config.data_bits = args->values[2].integer;
    } else {
        config.data_bits = SERIAL_DEFAULT_DATA_BITS;
    }

    if (args->count >= 4) {
        if (args->values[3].type == VALUE_TYPE_INTEGER) {
            if (args->values[3].integer > 2) {
                config_error("Parity type %" PRIu64 " is invalid", args->values[3].integer);
                return false;
            }

            config.parity = args->values[3].integer;
        } else if (args->values[3].type == VALUE_TYPE_STRING) {
            if (strcmp(args->values[3].string, "none") == 0) {
                config.parity = SERIAL_PARITY_NONE;
            } else if (strcmp(args->values[3].string, "odd") == 0) {
                config.parity = SERIAL_PARITY_ODD;
            } else if (strcmp(args->values[3].string, "even") == 0) {
                config.parity = SERIAL_PARITY_EVEN;
            } else {
                config_error("Parity type '%s' is invalid", args->values[3].string);
                return false;
            }
        } else {
            goto err_inval;
        }
    } else {
        config.parity = SERIAL_DEFAULT_PARITY;
    }

    if (args->count >= 5) {
        if (args->values[4].type != VALUE_TYPE_INTEGER)
            goto err_inval;

        if (args->values[4].integer < 1 || args->values[4].integer > 2) {
            config_error("Stop bits value %" PRIu64 " is invalid", args->values[4].integer);
            return false;
        }

        config.stop_bits = args->values[4].integer;
    } else {
        config.stop_bits = SERIAL_DEFAULT_STOP_BITS;
    }

    ret = serial_port_config(port, &config);
    if (ret != STATUS_SUCCESS) {
        config_error("Failed to set port configuration: %pE", ret);
        return false;
    }

    return true;

err_inval:
    config_error("Invalid arguments");
    return false;
}

BUILTIN_COMMAND("serial", "Configure a serial port", config_cmd_serial);

#endif /* __TEST */
