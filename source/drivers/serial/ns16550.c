/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               16550 UART driver.
 */

#include <arch/io.h>

#include <drivers/serial/ns16550.h>

#include <console.h>
#include <loader.h>
#include <memory.h>

/** 16550 console data. */
typedef struct ns16550_console {
    ns16550_t base;                     /**< Base of the 16550 registers. */
} ns16550_console_t;

#ifdef CONFIG_TARGET_NS16550_IO

/** Read a UART register.
 * @param base          UART registers to read from.
 * @param reg           Register to read.
 * @return              Value read. */
static inline uint8_t ns16550_read(ns16550_t base, int reg) {
    return in8(base + reg);
}

/** Write a UART register.
 * @param base          UART registers to write to.
 * @param reg           Register to read.
 * @param value         Value to write. */
static inline void ns16550_write(ns16550_t base, int reg, uint8_t value) {
    out8(base + reg, value);
}

#else /* CONFIG_TARGET_NS16550_IO */

#ifndef CONFIG_TARGET_NS16550_REG_SHIFT
#   define CONFIG_TARGET_NS16550_REG_SHIFT  2
#endif

/** Read a UART register.
 * @param base          UART registers to read from.
 * @param reg           Register to read.
 * @return              Value read. */
static inline uint8_t ns16550_read(ns16550_t base, int reg) {
    reg = reg << CONFIG_TARGET_NS16550_REG_SHIFT;
    return read8((void *)(base + reg));
}

/** Write a UART register.
 * @param base          UART registers to write to.
 * @param reg           Register to read.
 * @param value         Value to write. */
static inline void ns16550_write(ns16550_t base, int reg, uint8_t value) {
    reg = reg << CONFIG_TARGET_NS16550_REG_SHIFT;
    write8((void *)(base + reg), value);
}

#endif /* CONFIG_TARGET_NS16550_IO */

/** Write a character to the serial console.
 * @param _console      Pointer to console.
 * @param ch            Character to write. */
static void ns16550_console_putc(void *_console, char ch) {
    ns16550_console_t *console = _console;

    if (ch == '\n')
        ns16550_console_putc(console, '\r');

    ns16550_write(console->base, NS16550_REG_THR, ch);
    while (!(ns16550_read(console->base, NS16550_REG_LSR) & NS16550_LSR_THRE))
        ;
}

/** NS16550 UART debug console output operations. */
static console_out_ops_t ns16550_console_out_ops = {
    .putc = ns16550_console_putc,
};

/**
 * Initialise the NS16550 UART as the debug console.
 *
 * Sets the NS16550 UART as the debug console. This function does not
 * reconfigure the UART, that is done by ns16550_config(). The reason for this
 * split is for platforms where we know the UART has already been configured
 * prior to the loader being executed, meaning we can keep that configuration.
 *
 * @param base          Base of UART registers.
 */
void ns16550_init(ns16550_t base) {
    ns16550_console_t *console;

    console = malloc(sizeof(*console));
    console->base = base;

    /* Wait for the transmit buffer to empty. */
    while (!(ns16550_read(base, NS16550_REG_LSR) & NS16550_LSR_THRE))
        ;

    /* Set the debug console. No input operations needed, we don't need input
     * on the debug console. */
    debug_console.out = &ns16550_console_out_ops;
    debug_console.out_private = console;
}

/** Reconfigure a NS16550 UART.
 * @param base          Base of UART registers.
 * @param clock_rate    Base clock rate.
 * @param baud_rate     Baud rate to use. */
void ns16550_config(ns16550_t base, uint32_t clock_rate, unsigned baud_rate) {
    uint16_t divisor;

    /* Disable all interrupts, disable the UART while configuring. */
    ns16550_write(base, NS16550_REG_IER, 0);
    ns16550_write(base, NS16550_REG_FCR, 0);

    /* Set DLAB to enable access to divisor registers. */
    ns16550_write(base, NS16550_REG_LCR, NS16550_LCR_DLAB);

    /* Program the divisor to set the baud rate. */
    divisor = (clock_rate / 16) / baud_rate;
    ns16550_write(base, NS16550_REG_DLL, divisor & 0xff);
    ns16550_write(base, NS16550_REG_DLH, (divisor >> 8) & 0x3f);

    /* Switch to operational mode. */
    ns16550_write(base, NS16550_REG_LCR, NS16550_LCR_WLS_8);

    /* Clear and enable FIFOs. */
    ns16550_write(base, NS16550_REG_FCR, NS16550_FCR_FIFO_EN | NS16550_FCR_CLEAR_RX | NS16550_FCR_CLEAR_TX);

    /* Enable RTS/DTR. */
    ns16550_write(base, NS16550_REG_MCR, NS16550_MCR_DTR | NS16550_MCR_RTS);
}
