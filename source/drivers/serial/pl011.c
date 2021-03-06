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
 * @brief               ARM PL011 UART driver.
 *
 * Reference:
 *  - PrimeCell UART (PL011) Technical Reference Manual
 *    https://developer.arm.com/documentation/ddi0183/g/
 */

#include <arch/io.h>

#include <drivers/serial/pl011.h>

#include <lib/utility.h>

#include <console.h>
#include <kboot.h>
#include <loader.h>
#include <memory.h>

/** PL011 serial port structure. */
typedef struct pl011_port {
    serial_port_t port;                 /**< Serial port header. */

    uint32_t *base;                     /**< Base of the registers. */
    uint32_t clock_rate;                /**< Clock rate. */
} pl011_port_t;

/** Read a UART register. */
static inline uint32_t pl011_read(pl011_port_t *port, int reg) {
    return read32(&port->base[reg]);
}

/** Write a UART register. */
static inline void pl011_write(pl011_port_t *port, int reg, uint32_t value) {
    write32(&port->base[reg], value);
}

/** Set the port configuration. */
static status_t pl011_port_config(serial_port_t *_port, const serial_config_t *config) {
    pl011_port_t *port = (pl011_port_t *)_port;
    uint32_t divider, fraction, lcr;

    if (port->clock_rate == 0)
        return STATUS_NOT_SUPPORTED;

    /* Disable the UART while we configure it. */
    pl011_write(port, PL011_REG_CR, 0);

    /* Calculate the baud rate divisor registers. See PL011 Reference
     * Manual, page 3-10.
     *  Baud Rate Divisor = UARTCLK / (16 * Baud Rate)
     * This is split into an integer and a fractional part.
     *  FBRD = Round((64 * (UARTCLK % (16 * Baud Rate))) / (16 * Baud Rate))
     */
    divider  = port->clock_rate / (16 * config->baud_rate);
    fraction = (8 * (port->clock_rate % (16 * config->baud_rate))) / config->baud_rate;
    fraction = (fraction >> 1) + (fraction & 1);

    /* A write to LCR is required for a change to these to take effect. */
    pl011_write(port, PL011_REG_IBRD, divider);
    pl011_write(port, PL011_REG_FBRD, fraction);

    /* Determine the LCR value. */
    lcr  = PL011_LCRH_FEN;
    lcr |= (config->data_bits - 5) << PL011_LCRH_WLEN_SHIFT;
    if (config->stop_bits == 2)
        lcr |= PL011_LCRH_STP2;
    if (config->parity != SERIAL_PARITY_NONE) {
        lcr |= PL011_LCRH_PEN;
        if (config->parity == SERIAL_PARITY_EVEN)
            lcr |= PL011_LCRH_EPS;
    }

    pl011_write(port, PL011_REG_LCRH, lcr);

    /* Enable the UART. */
    pl011_write(port, PL011_REG_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);

    return STATUS_SUCCESS;
}

/** Check whether the RX buffer is empty.
 * @param _port         Port to check.
 * @return              Whether the RX buffer is empty. */
static bool pl011_port_rx_empty(serial_port_t *_port) {
    pl011_port_t *port = (pl011_port_t *)_port;

    return pl011_read(port, PL011_REG_FR) & PL011_FR_RXFE;
}

/** Read from a port (RX will be non-empty).
 * @param _port         Port to write to.
 * @return              Value read from the port. */
static uint8_t pl011_port_read(serial_port_t *_port) {
    pl011_port_t *port = (pl011_port_t *)_port;

    return pl011_read(port, PL011_REG_DR);
}

/** Check whether the TX buffer is empty.
 * @param _port         Port to check.
 * @return              Whether the TX buffer is empty. */
static bool pl011_port_tx_empty(serial_port_t *_port) {
    pl011_port_t *port = (pl011_port_t *)_port;

    return !(pl011_read(port, PL011_REG_FR) & PL011_FR_TXFF);
}

/** Write to a port (TX will be empty).
 * @param _port         Port to write to.
 * @param val           Value to write. */
static void pl011_port_write(serial_port_t *_port, uint8_t val) {
    pl011_port_t *port = (pl011_port_t *)_port;

    pl011_write(port, PL011_REG_DR, val);
}

#if CONFIG_TARGET_HAS_KBOOT32 || CONFIG_TARGET_HAS_KBOOT64

/** Get KBoot parameters for the port.
 * @param port          Port to get parameters for.
 * @param tag           KBoot tag to fill in (type, addr, io_type). */
static void pl011_port_get_kboot_params(serial_port_t *_port, kboot_tag_serial_t *tag) {
    pl011_port_t *port = (pl011_port_t *)_port;

    tag->addr    = (ptr_t)port->base;
    tag->io_type = KBOOT_IO_TYPE_MMIO;
    tag->type    = KBOOT_SERIAL_TYPE_PL011;
}

#endif

/** pl011 serial port operations. */
static serial_port_ops_t pl011_port_ops = {
    .config = pl011_port_config,
    .rx_empty = pl011_port_rx_empty,
    .read = pl011_port_read,
    .tx_empty = pl011_port_tx_empty,
    .write = pl011_port_write,
    #if CONFIG_TARGET_HAS_KBOOT32 || CONFIG_TARGET_HAS_KBOOT64
    .get_kboot_params = pl011_port_get_kboot_params,
    #endif
};

/**
 * Register a PL011 UART.
 *
 * Registers a PL011 UART as a console. This function does not reconfigure the
 * UART, to do so use serial_port_config(). If no reconfiguration is done, the
 * UART will continue to use whichever parameters are currently set (e.g. ones
 * set by platform firmware).
 *
 * This assumes that a PL011 is at the specified location and does not check
 * that it is valid.
 *
 * @param base          Base of UART registers.
 * @param index         Index of the UART, used to name the console.
 * @param clock_rate    UART base clock rate (0 will forbid reconfiguration).
 *
 * @return              Created port.
 */
serial_port_t *pl011_register(ptr_t base, unsigned index, uint32_t clock_rate) {
    pl011_port_t *port = malloc(sizeof(*port));

    port->port.ops = &pl011_port_ops;
    port->port.index = index;
    port->base = (uint32_t *)base;
    port->clock_rate = clock_rate;

    if (serial_port_register(&port->port) != STATUS_SUCCESS) {
        free(port);
        return NULL;
    }

    return &port->port;
}

#if defined(CONFIG_TARGET_HAS_FDT) && !defined(__TEST)

static const char *dt_pl011_compatible[] = {
    "arm,pl011",
};

/** Register a PL011 from a device tree node if compatible.
 * @param node_offset   Offset of DT node.
 * @return              Registered port, or null if not supported. */
serial_port_t *dt_pl011_register(int node_offset) {
    if (!dt_is_compatible(node_offset, dt_pl011_compatible, array_size(dt_pl011_compatible)))
        return NULL;

    phys_ptr_t base;
    phys_ptr_t size;
    if (!dt_get_reg(node_offset, 0, &base, &size))
        return NULL;

    /* TODO: Get clock rate. For now we just don't allow reconfiguration. */
    return pl011_register(phys_to_virt(base), 0, 0);
}

#endif
