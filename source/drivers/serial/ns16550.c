/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               16550 UART driver.
 */

#include <arch/io.h>

#include <drivers/serial/ns16550.h>

#include <lib/utility.h>

#include <console.h>
#include <kboot.h>
#include <loader.h>
#include <memory.h>

#ifndef CONFIG_TARGET_NS16550_IO
#   ifndef CONFIG_TARGET_NS16550_REG_SHIFT
#       define CONFIG_TARGET_NS16550_REG_SHIFT  2
#   endif
#endif

/** 16550 serial port structure. */
typedef struct ns16550_port {
    serial_port_t port;                 /**< Serial port header. */

    ns16550_base_t base;                /**< Base of the 16550 registers. */
    ns16550_type_t type;                /**< Type of the UART. */
    uint32_t clock_rate;                /**< Clock rate. */
} ns16550_port_t;

/** Read a UART register.
 * @param port          Port to read from.
 * @param reg           Register to read.
 * @return              Value read. */
static inline uint8_t ns16550_read(ns16550_port_t *port, int reg) {
    #ifdef CONFIG_TARGET_NS16550_IO
        return in8(port->base + reg);
    #else
        reg = reg << CONFIG_TARGET_NS16550_REG_SHIFT;
        return read8((void *)(port->base + reg));
    #endif
}

/** Write a UART register.
 * @param port          Port to write to.
 * @param reg           Register to read.
 * @param value         Value to write. */
static inline void ns16550_write(ns16550_port_t *port, int reg, uint8_t value) {
    #ifdef CONFIG_TARGET_NS16550_IO
        out8(port->base + reg, value);
    #else
        reg = reg << CONFIG_TARGET_NS16550_REG_SHIFT;
        write8((void *)(port->base + reg), value);
    #endif
}

/** Set the port configuration.
 * @param _port         Port to configure.
 * @param config        Port configuration.
 * @return              Status code describing the result of the operation. */
static status_t ns16550_port_config(serial_port_t *_port, const serial_config_t *config) {
    ns16550_port_t *port = (ns16550_port_t *)_port;
    uint16_t divisor;
    uint8_t lcr;

    if (port->clock_rate == 0)
        return STATUS_NOT_SUPPORTED;

    /* Disable all interrupts, disable the UART while configuring. */
    ns16550_write(port, NS16550_REG_IER, 0);
    ns16550_write(port, NS16550_REG_FCR, 0);

    /* Set DLAB to enable access to divisor registers. */
    ns16550_write(port, NS16550_REG_LCR, NS16550_LCR_DLAB);

    /* Program the divisor to set the baud rate. */
    divisor = (port->clock_rate / 16) / config->baud_rate;
    ns16550_write(port, NS16550_REG_DLL, divisor & 0xff);
    ns16550_write(port, NS16550_REG_DLH, (divisor >> 8) & 0x3f);

    /* Determine the LCR value. */
    lcr = NS16550_LCR_WLS_5 + config->data_bits - 5;
    if (config->stop_bits == 2)
        lcr |= NS16550_LCR_STOP;
    if (config->parity != SERIAL_PARITY_NONE) {
        lcr |= NS16550_LCR_PARITY;
        if (config->parity == SERIAL_PARITY_EVEN)
            lcr |= NS16550_LCR_EPAR;
    }

    /* Switch to operational mode. */
    ns16550_write(port, NS16550_REG_LCR, lcr);

    /* Clear and enable FIFOs. */
    ns16550_write(port, NS16550_REG_FCR, NS16550_FCR_FIFO_EN | NS16550_FCR_CLEAR_RX | NS16550_FCR_CLEAR_TX);

    /* Enable RTS/DTR. */
    ns16550_write(port, NS16550_REG_MCR, NS16550_MCR_DTR | NS16550_MCR_RTS);

    return STATUS_SUCCESS;
}

/** Check whether the RX buffer is empty.
 * @param _port         Port to check.
 * @return              Whether the RX buffer is empty. */
static bool ns16550_port_rx_empty(serial_port_t *_port) {
    ns16550_port_t *port = (ns16550_port_t *)_port;

    return !(ns16550_read(port, NS16550_REG_LSR) & NS16550_LSR_DR);
}

/** Read from a port (RX will be non-empty).
 * @param _port         Port to write to.
 * @return              Value read from the port. */
static uint8_t ns16550_port_read(serial_port_t *_port) {
    ns16550_port_t *port = (ns16550_port_t *)_port;

    return ns16550_read(port, NS16550_REG_RHR);
}

/** Check whether the TX buffer is empty.
 * @param _port         Port to check.
 * @return              Whether the TX buffer is empty. */
static bool ns16550_port_tx_empty(serial_port_t *_port) {
    ns16550_port_t *port = (ns16550_port_t *)_port;

    return ns16550_read(port, NS16550_REG_LSR) & NS16550_LSR_THRE;
}

/** Write to a port (TX will be empty).
 * @param _port         Port to write to.
 * @param val           Value to write. */
static void ns16550_port_write(serial_port_t *_port, uint8_t val) {
    ns16550_port_t *port = (ns16550_port_t *)_port;

    ns16550_write(port, NS16550_REG_THR, val);
}

#if CONFIG_TARGET_HAS_KBOOT32 || CONFIG_TARGET_HAS_KBOOT64

/** Get KBoot parameters for the port.
 * @param port          Port to get parameters for.
 * @param tag           KBoot tag to fill in (type, addr, io_type). */
static void ns16550_port_get_kboot_params(serial_port_t *_port, kboot_tag_serial_t *tag) {
    ns16550_port_t *port = (ns16550_port_t *)_port;

    tag->addr = port->base;

    #ifdef CONFIG_TARGET_NS16550_IO
        tag->io_type = KBOOT_IO_TYPE_PIO;
    #else
        tag->io_type = KBOOT_IO_TYPE_MMIO;
    #endif

    switch (port->type) {
    case NS16550_TYPE_STANDARD:    tag->type = KBOOT_SERIAL_TYPE_NS16550; break;
    case NS16550_TYPE_BCM2835_AUX: tag->type = KBOOT_SERIAL_TYPE_BCM2835_AUX; break;
    default:
        internal_error("Unhandled NS16550 type");
        break;
    }
}

#endif

/** NS16550 serial port operations. */
static serial_port_ops_t ns16550_port_ops = {
    .config = ns16550_port_config,
    .rx_empty = ns16550_port_rx_empty,
    .read = ns16550_port_read,
    .tx_empty = ns16550_port_tx_empty,
    .write = ns16550_port_write,
    #if CONFIG_TARGET_HAS_KBOOT32 || CONFIG_TARGET_HAS_KBOOT64
    .get_kboot_params = ns16550_port_get_kboot_params,
    #endif
};

/**
 * Register a NS16550 UART.
 *
 * Registers a NS16550 UART as a console. This function does not reconfigure
 * the UART, to do so use serial_port_config(). If no reconfiguration is done,
 * the UART will continue to use whichever parameters are currently set (e.g.
 * ones set by platform firmware).
 *
 * @param base          Base of UART registers.
 * @param type          Type of the UART.
 * @param index         Index of the UART, used to name the console.
 * @param clock_rate    UART base clock rate (0 will forbid reconfiguration).
 *
 * @return              Created port, or NULL if port does not exist.
 */
serial_port_t *ns16550_register(
    ns16550_base_t base, ns16550_type_t type, unsigned index,
    uint32_t clock_rate)
{
    ns16550_port_t *port = malloc(sizeof(*port));

    port->port.ops = &ns16550_port_ops;
    port->port.index = index;
    port->base = base;
    port->type = type;
    port->clock_rate = clock_rate;

    /* See if this looks like a 16550. Check for registers that are known 0. */
    if (ns16550_read(port, NS16550_REG_IIR) & 0x30 || ns16550_read(port, NS16550_REG_MCR) & 0xe0) {
        free(port);
        return NULL;
    }

    if (serial_port_register(&port->port) != STATUS_SUCCESS) {
        free(port);
        return NULL;
    }

    return &port->port;
}

#if defined(CONFIG_TARGET_HAS_FDT) && !defined(__TEST)

typedef struct dt_ns16550_match {
    const char *compatible;
    ns16550_type_t type;
} dt_ns16550_match_t;

static dt_ns16550_match_t dt_ns16550_match[] = {
    { "ns8250",                NS16550_TYPE_STANDARD },
    { "ns16550",               NS16550_TYPE_STANDARD },
    { "ns16550a",              NS16550_TYPE_STANDARD },
    { "brcm,bcm2835-aux-uart", NS16550_TYPE_BCM2835_AUX },
};

/** Register a NS16550 from a device tree node if compatible.
 * @param node_offset   Offset of DT node.
 * @return              Registered port, or null if not supported. */
serial_port_t *dt_ns16550_register(int node_offset) {
    int match = dt_match(node_offset, dt_ns16550_match);
    if (match < 0)
        return NULL;

    phys_ptr_t base;
    phys_ptr_t size;
    if (!dt_get_reg(node_offset, 0, &base, &size))
        return NULL;

    /* TODO: Get clock rate. For now we just don't allow reconfiguration. */
    return ns16550_register(phys_to_virt(base), dt_ns16550_match[match].type, 0, 0);
}

#endif
