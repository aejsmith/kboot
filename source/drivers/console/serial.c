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
 * @brief               Serial console implementation.
 */

#include <drivers/console/serial.h>

#include <drivers/serial/ns16550.h>
#include <drivers/serial/pl011.h>

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <config.h>
#include <loader.h>
#include <memory.h>
#include <time.h>

/** Colour escape sequence table. */
static struct {
    uint8_t num;
    bool bold;
} colour_table[] = {
    [COLOUR_BLACK]         = { 0, false },
    [COLOUR_BLUE]          = { 4, false },
    [COLOUR_GREEN]         = { 2, false },
    [COLOUR_CYAN]          = { 6, false },
    [COLOUR_RED]           = { 1, false },
    [COLOUR_MAGENTA]       = { 5, false },
    [COLOUR_BROWN]         = { 3, false },
    [COLOUR_LIGHT_GREY]    = { 7, false },
    [COLOUR_GREY]          = { 0, true },
    [COLOUR_LIGHT_BLUE]    = { 4, true },
    [COLOUR_LIGHT_GREEN]   = { 2, true },
    [COLOUR_LIGHT_CYAN]    = { 6, true },
    [COLOUR_LIGHT_RED]     = { 1, true },
    [COLOUR_LIGHT_MAGENTA] = { 5, true },
    [COLOUR_YELLOW]        = { 3, true },
    [COLOUR_WHITE]         = { 7, true },
};

/** Helper for printing to the serial port.
 * @param ch            Character to display.
 * @param data          Port to print to.
 * @param total         Pointer to total character count. */
static void serial_port_printf_helper(char ch, void *data, int *total) {
    serial_port_t *port = data;

    serial_port_write(port, ch);
    *total = *total + 1;
}

/** Formatted print to a serial port, used for printing escape codes.
 * @param port          Port to print to.
 * @param fmt           Format string.
 * @param ...           Arguments to substitute into format. */
static void serial_port_printf(serial_port_t *port, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    do_vprintf(serial_port_printf_helper, port, fmt, args);
    va_end(args);
}

/** Set the terminal's absolute cursor position.
 * @param port          Port to set for.
 * @param x             New X position.
 * @param y             New Y position. */
static void set_absolute_cursor(serial_port_t *port, uint16_t x, uint16_t y) {
    /* Cursor positions are 1-based. */
    serial_port_printf(port, "\e[%u;%uH", y + 1, x + 1);

    port->cursor_x = x;
    port->cursor_y = y;
}

/** Get the terminal's real cursor position.
 * @param port          Port to get for.
 * @param _x            Where to store X position.
 * @param _y            Where to store Y position. */
static void get_absolute_cursor(serial_port_t *port, uint16_t *_x, uint16_t *_y) {
    uint8_t ch;
    uint16_t x = 0, y = 0;

    serial_port_flush_rx(port);
    serial_port_puts(port, "\e[6n");

    ch = serial_port_read(port);
    if (ch != '\e')
        goto out;

    ch = serial_port_read(port);
    if (ch != '[')
        goto out;

    while (isdigit((ch = serial_port_read(port)))) {
        y *= 10;
        y += ch - '0';
    }

    if (ch != ';') {
        y = 0;
        goto out;
    }

    while (isdigit((ch = serial_port_read(port)))) {
        x *= 10;
        x += ch - '0';
    }

   if (ch != 'R') {
        x = y = 0;
        goto out;
    }

    /* Positions are 1-based. */
    x -= 1;
    y -= 1;

out:
    *_x = x;
    *_y = y;
}

/** Set the scroll region.
 * @param port          Port to set for.
 * @param y             Y position of top.
 * @param height        Height of the region.
 * @param scrollable    Whether the region should be scrollable. */
static void set_scroll_region(serial_port_t *port, uint16_t y, uint16_t height, bool scrollable) {
    /* Multiple slight hacks here. I couldn't find a way to prevent scrolling,
     * so if we've been asked to make the region not scroll, what we do is set
     * the scroll region one more than asked, and handle it in putc(). Second,
     * we're actually scrolling the whole width, not just the part specified by
     * the region. Due to the way the UI is laid out, this is actually OK - the
     * horizontal constraints are only used to introduce padding around the
     * edges. */
    if (y || y + height < port->height) {
        uint16_t top = y + 1;
        uint16_t bottom = y + height;

        if (!scrollable)
            bottom++;

        serial_port_printf(port, "\e[%u;%ur", top, bottom);
    } else {
        serial_port_puts(port, "\e[r");
    }
}

/** Write a character to a serial console.
 * @param console       Console output device.
 * @param ch            Character to write. */
static void serial_console_putc(console_out_t *console, char ch) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    if (ch == '\n')
        serial_console_putc(console, '\r');

    if (console->in_ui) {
        bool update = false;

        /* We have to try to track cursor position here so that we can implement
         * the draw region constraints. */
        switch (ch) {
        case '\b':
            if (port->cursor_x > port->region.x) {
                port->cursor_x--;
            } else if (port->cursor_y > port->region.y) {
                port->cursor_x = port->region.x + port->region.width - 1;
                port->cursor_y--;
                update = true;
            } else {
                /* Do nothing. */
                return;
            }

            break;
        case '\r':
            port->cursor_x = port->region.x;
            if (port->region.x)
                update = true;
            break;
        case '\n':
            port->cursor_y++;
            if (port->region.x)
                update = true;
            break;
        case '\t':
            port->cursor_x += 8 - (port->cursor_x % 8);
            break;
        default:
            /* If it is a non-printing character, ignore it. */
            if (ch < ' ')
                return;

            port->cursor_x++;
            break;
        }

        /* Send the character. */
        serial_port_write(port, ch);

        /* If we have reached the edge of the draw region insert a new line. */
        if (port->cursor_x >= port->region.x + port->region.width) {
            port->cursor_x = port->region.x;
            port->cursor_y++;

            if (port->region.x || port->region.x + port->region.width < port->width) {
                /* Newline is to make sure we scroll if necessary. */
                serial_port_write(port, '\n');
                update = true;
            }
        }

        /* If we have reached the bottom, bring the cursor back. */
        if (port->cursor_y >= port->region.y + port->region.height) {
            port->cursor_y = port->region.y + port->region.height - 1;
            update = true;
        }

        if (update)
            set_absolute_cursor(port, port->cursor_x, port->cursor_y);
    } else {
        serial_port_write(port, ch);
    }
}

/** Set the current colours.
 * @param console       Console output device.
 * @param fg            Foreground colour.
 * @param bg            Background colour. */
static void serial_console_set_colour(console_out_t *console, colour_t fg, colour_t bg) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    /* Reset to default to begin with. */
    serial_port_puts(port, "\e[0m");

    if (fg != COLOUR_DEFAULT) {
        serial_port_printf(
            port,"\033[%s%um",
            (colour_table[fg].bold) ? "1;" : "", 30 + colour_table[fg].num);
    }

    if (bg != COLOUR_DEFAULT) {
        serial_port_printf(
            port, "\033[%s%um",
            (colour_table[bg].bold) ? "1;" : "", 40 + colour_table[bg].num);
    }
}

/** Set whether the cursor is visible.
 * @param console       Console output device.
 * @param visible       Whether the cursor should be visible. */
static void serial_console_set_cursor_visible(console_out_t *console, bool visible) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    if (visible != port->cursor_visible) {
        serial_port_puts(port, (visible) ? "\e[?25h" : "\e[?25l");
        port->cursor_visible = visible;
    }
}

/** Set the cursor position.
 * @param console       Console output device.
 * @param x             New X position.
 * @param y             New Y position. */
static void serial_console_set_cursor_pos(console_out_t *console, int16_t x, int16_t y) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    assert(abs(x) < port->region.width);
    assert(abs(y) < port->region.height);

    set_absolute_cursor(
        port,
        (x < 0) ? port->region.x + port->region.width + x : port->region.x + x,
        (y < 0) ? port->region.y + port->region.height + y : port->region.y + y);
}

/** Get the cursor position.
 * @param console       Console output device.
 * @param _x            Where to store X position (relative to draw region).
 * @param _y            Where to store Y position (relative to draw region). */
static void serial_console_get_cursor_pos(console_out_t *console, uint16_t *_x, uint16_t *_y) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    if (_x)
        *_x = port->cursor_x - port->region.x;
    if (_y)
        *_y = port->cursor_y - port->region.y;
}

/** Set the draw region of the console.
 * @param console       Console output device.
 * @param region        New draw region, or NULL to restore to whole console. */
static void serial_console_set_region(console_out_t *console, const draw_region_t *region) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    if (region) {
        assert(region->width && region->height);
        assert(region->x + region->width <= port->width);
        assert(region->y + region->height <= port->height);

        memcpy(&port->region, region, sizeof(port->region));
    } else {
        port->region.x = port->region.y = 0;
        port->region.width = port->width;
        port->region.height = port->height;
        port->region.scrollable = true;
    }

    /* Set the scroll region. */
    set_scroll_region(port, port->region.y, port->region.height, port->region.scrollable);

    /* Move cursor to top of region. */
    set_absolute_cursor(port, port->region.x, port->region.y);
}

/** Get the current draw region.
 * @param console       Console output device.
 * @param region        Where to store details of the current draw region. */
static void serial_console_get_region(console_out_t *console, draw_region_t *region) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    memcpy(region, &port->region, sizeof(*region));
}

/** Clear an area to the current background colour.
 * @param console       Console output device.
 * @param x             Start X position (relative to draw region).
 * @param y             Start Y position (relative to draw region).
 * @param width         Width of the area (if 0, whole width is cleared).
 * @param height        Height of the area (if 0, whole height is cleared). */
static void serial_console_clear(console_out_t *console, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    assert(x + width <= port->region.width);
    assert(y + height <= port->region.height);

    if (!width)
        width = port->region.width - x;
    if (!height)
        height = port->region.height - y;

    if (!x && !y && width == port->width && height == port->height) {
        serial_port_puts(port, "\e[2J");
    } else {
        uint16_t prev_x, prev_y;

        /* I'm being a bit lazy here. We could make use of the other partial
         * clear commands, but this function is mostly used for small clears so
         * it's not really worth the effort to try and handle all the cases
         * where we could use commands. Instead just print spaces all over then
         * restore the previous cursor position. */
        prev_x = port->cursor_x;
        prev_y = port->cursor_y;

        for (uint16_t i = 0; i < height; i++) {
            set_absolute_cursor(port, x + port->region.x, y + port->region.y + i);

            for (uint16_t j = 0; j < width; j++)
                serial_port_write(port, ' ');
        }

        set_absolute_cursor(port, prev_x, prev_y);
    }
}

/** Scroll the draw region up (move contents down).
 * @param console       Console output device. */
static void serial_console_scroll_up(console_out_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, out);
    uint16_t prev_x, prev_y;

    /* Due to what we do to disable scrolling (see set_scroll_region()), we need
     * to change the scroll region to the actual region here so we don't scroll
     * to outside the region. */
    set_scroll_region(port, port->region.y, port->region.height, true);

    /* Doesn't seem to work if the cursor is not on the top row of the region. */
    prev_x = port->cursor_x;
    prev_y = port->cursor_y;
    set_absolute_cursor(port, 0, port->region.y);

    serial_port_puts(port, "\eM");

    set_scroll_region(port, port->region.y, port->region.height, port->region.scrollable);
    set_absolute_cursor(port, prev_x, prev_y);
}

/** Scroll the draw region down (move contents up).
 * @param console       Console output device. */
static void serial_console_scroll_down(console_out_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, out);
    uint16_t prev_x, prev_y;

    set_scroll_region(port, port->region.y, port->region.height, true);

    prev_x = port->cursor_x;
    prev_y = port->cursor_y;
    set_absolute_cursor(port, 0, port->region.y + port->region.height - 1);

    serial_port_puts(port, "\eD");

    set_scroll_region(port, port->region.y, port->region.height, port->region.scrollable);
    set_absolute_cursor(port, prev_x, prev_y);
}

/** Begin UI mode.
 * @param console       Console output device. */
static void serial_console_begin_ui(console_out_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, out);

    port->cursor_visible = true;

    /* Try to figure out dimensions. Set an oversized cursor position, will be
     * clamped to the dimensions of the console. */
    serial_port_puts(port, "\e[10000;10000H");
    get_absolute_cursor(port, &port->width, &port->height);
    port->width++;
    port->height++;

    /* Initialize the draw region and move the cursor to the start of it. */
    serial_console_set_region(console, NULL);
}

/** Serial console output operations. */
static console_out_ops_t serial_console_out_ops = {
    .putc = serial_console_putc,
    .set_colour = serial_console_set_colour,
    .set_cursor_visible = serial_console_set_cursor_visible,
    .set_cursor_pos = serial_console_set_cursor_pos,
    .get_cursor_pos = serial_console_get_cursor_pos,
    .set_region = serial_console_set_region,
    .get_region = serial_console_get_region,
    .clear = serial_console_clear,
    .scroll_up = serial_console_scroll_up,
    .scroll_down = serial_console_scroll_down,
    .begin_ui = serial_console_begin_ui,
};

/** Check for a character from a serial console.
 * @param console       Console input device.
 * @return              Whether a character is available. */
static bool serial_console_poll(console_in_t *console) {
    serial_port_t *port = container_of(console, serial_port_t, in);

    if (port->next_ch)
        return true;

    /* Have a maximum time between receiving the escape and the rest of a
     * sequence. This allows us to distinguish between an escape sequence or
     * actually pressing the escape key on its own. */
    if (!port->esc_len && current_time() - port->esc_time >= 100) {
        port->next_ch = '\e';
        port->esc_len = -1;
        return true;
    }

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
                port->esc_time = current_time();
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

/** Flush any received data on a serial port.
 * @param port          Port to flush. */
void serial_port_flush_rx(serial_port_t *port) {
    while (!port->ops->rx_empty(port))
        port->ops->read(port);
}

/** Read a byte from a serial port.
 * @param port          Port to read from.
 * @return              Value read. */
uint8_t serial_port_read(serial_port_t *port) {
    while (port->ops->rx_empty(port))
        arch_pause();

    return port->ops->read(port);
}

/** Write a byte to a serial port.
 * @param port          Port to write to.
 * @param val           Value to write. */
void serial_port_write(serial_port_t *port, uint8_t val) {
    port->ops->write(port, val);

    while (!port->ops->tx_empty(port))
        arch_pause();
}

/** Write a string to a serial port.
 * @param port          Port to write to.
 * @param str           String to write. */
void serial_port_puts(serial_port_t *port, const char *str) {
    while (*str) {
        serial_port_write(port, *str);
        str++;
    }
}

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
 * Device Tree support.
 */

#if defined(CONFIG_TARGET_HAS_FDT) && !defined(__TEST)

typedef serial_port_t *(*dt_serial_driver_t)(int);

static dt_serial_driver_t dt_serial_drivers[] = {
    #ifdef CONFIG_DRIVER_SERIAL_NS16550
    dt_ns16550_register,
    #endif
    #ifdef CONFIG_DRIVER_SERIAL_PL011
    dt_pl011_register,
    #endif
};

/** Register a serial port from a device tree node.
 * @param node_offset   Offset of DT node.
 * @return              Registered port, or null if not supported. */
serial_port_t *dt_serial_port_register(int node_offset) {
    for (size_t i = 0; i < array_size(dt_serial_drivers); i++) {
        serial_port_t *port = dt_serial_drivers[i](node_offset);
        if (port)
            return port;
    }

    return NULL;
}

#endif

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
