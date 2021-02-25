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

#ifndef __DRIVERS_CONSOLE_SERIAL_H
#define __DRIVERS_CONSOLE_SERIAL_H

#include <console.h>
#include <dt.h>
#include <status.h>

struct serial_port;

/** Serial port parity enumeration. */
typedef enum serial_parity {
    SERIAL_PARITY_NONE,                 /**< No parity. */
    SERIAL_PARITY_ODD,                  /**< Odd parity. */
    SERIAL_PARITY_EVEN,                 /**< Even parity. */
} serial_parity_t;

/** Serial port configuration. */
typedef struct serial_config {
    uint32_t baud_rate;                 /**< Baud rate. */
    uint8_t data_bits;                  /**< Number of data bits. */
    serial_parity_t parity;             /**< Parity mode. */
    uint8_t stop_bits;                  /**< Number of stop bits. */
} serial_config_t;

/** Serial port defaults. */
#define SERIAL_DEFAULT_BAUD_RATE    115200
#define SERIAL_DEFAULT_DATA_BITS    8
#define SERIAL_DEFAULT_PARITY       SERIAL_PARITY_NONE
#define SERIAL_DEFAULT_STOP_BITS    1

/** Serial port operations. */
typedef struct serial_port_ops {
    /** Set the port configuration.
     * @param port          Port to configure.
     * @param config        Port configuration.
     * @return              Status code describing the result of the operation. */
    status_t (*config)(struct serial_port *port, const serial_config_t *config);

    /** Check whether the RX buffer is empty.
     * @param port          Port to check.
     * @return              Whether the RX buffer is empty. */
    bool (*rx_empty)(struct serial_port *port);

    /** Read from a port (RX will be non-empty).
     * @param port          Port to write to.
     * @return              Value read from the port. */
    uint8_t (*read)(struct serial_port *port);

    /** Check whether the TX buffer is empty.
     * @param port          Port to check.
     * @return              Whether the TX buffer is empty. */
    bool (*tx_empty)(struct serial_port *port);

    /** Write to a port (TX will be empty).
     * @param port          Port to write to.
     * @param val           Value to write. */
    void (*write)(struct serial_port *port, uint8_t val);
} serial_port_ops_t;

/** Buffer length for escape code parser. */
#define ESC_BUFFER_LEN              4

/** Serial port structure (embedded in implementation-specific structure). */
typedef struct serial_port {
    console_t console;                  /**< Console structure. */
    console_out_t out;                  /**< Console output device header. */
    console_in_t in;                    /**< Console input device header. */

    const serial_port_ops_t *ops;       /**< Port operations. */
    unsigned index;                     /**< Port index. */

    uint16_t next_ch;                   /**< Next character to read. */
    char esc_buffer[ESC_BUFFER_LEN];    /**< Buffer containing collected escape sequence. */
    int esc_len;                        /**< Escape buffer length. */
    mstime_t esc_time;                  /**< Time that an escape character was received. */

    uint16_t width;                     /**< Width of the console. */
    uint16_t height;                    /**< Height of the console. */
    draw_region_t region;               /**< Current draw region. */
    uint16_t cursor_x;                  /**< X position of the cursor. */
    uint16_t cursor_y;                  /**< Y position of the cursor. */
    bool cursor_visible;                /**< Whether the cursor is visible. */
} serial_port_t;

extern void serial_port_flush_rx(serial_port_t *port);
extern uint8_t serial_port_read(serial_port_t *port);
extern void serial_port_write(serial_port_t *port, uint8_t val);
extern void serial_port_puts(serial_port_t *port, const char *str);

/** Reconfigure a serial port.
 * @param port          Port to reconfigure.
 * @param config        Configuration to set (must be valid).
 * @return              Status code describing the result of the operation. */
static inline status_t serial_port_config(serial_port_t *port, const serial_config_t *config) {
    return port->ops->config(port, config);
}

extern status_t serial_port_register(serial_port_t *port);

#ifdef CONFIG_TARGET_HAS_FDT

extern serial_port_t *dt_serial_port_register(int node_offset);

#endif /* CONFIG_TARGET_HAS_FDT */
#endif /* __DRIVERS_CONSOLE_SERIAL_H */
