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
 * @brief               16550 UART driver.
 */

#ifndef __DRIVERS_SERIAL_NS16550_H
#define __DRIVERS_SERIAL_NS16550_H

#include <drivers/console/serial.h>

/** UART port definitions. */
#define NS16550_REG_RHR         0       /**< Receive Holding Register (R). */
#define NS16550_REG_THR         0       /**< Transmit Holding Register (W). */
#define NS16550_REG_DLL         0       /**< Divisor Latches Low (R/W). */
#define NS16550_REG_DLH         1       /**< Divisor Latches High (R/W). */
#define NS16550_REG_IER         1       /**< Interrupt Enable Register (R/W). */
#define NS16550_REG_IIR         2       /**< Interrupt Identification Register (R). */
#define NS16550_REG_FCR         2       /**< FIFO Control Register (W). */
#define NS16550_REG_LCR         3       /**< Line Control Register (R/W). */
#define NS16550_REG_MCR         4       /**< Modem Control Register (R/W). */
#define NS16550_REG_LSR         5       /**< Line Status Register (R). */

/** FIFO Control Register (FCR) bits. */
#define NS16550_FCR_FIFO_EN     (1<<0)  /**< FIFO enable. */
#define NS16550_FCR_CLEAR_RX    (1<<1)  /**< Receiver soft reset. */
#define NS16550_FCR_CLEAR_TX    (1<<2)  /**< Transmitter soft reset. */
#define NS16550_FCR_DMA_EN      (1<<3)  /**< DMA enable. */

/** Line Control Register (LCR) bits. */
#define NS16550_LCR_WLS_MASK    0x03    /**< Word length select mask. */
#define NS16550_LCR_WLS_5       0x00    /**< 5 bit character length. */
#define NS16550_LCR_WLS_6       0x01    /**< 6 bit character length. */
#define NS16550_LCR_WLS_7       0x02    /**< 7 bit character length. */
#define NS16550_LCR_WLS_8       0x03    /**< 8 bit character length. */
#define NS16550_LCR_STOP        (1<<2)  /**< Stop bit length select. */
#define NS16550_LCR_PARITY      (1<<3)  /**< Parity enable. */
#define NS16550_LCR_EPAR        (1<<4)  /**< Even parity. */
#define NS16550_LCR_SPAR        (1<<5)  /**< Sticky parity. */
#define NS16550_LCR_SBRK        (1<<6)  /**< Set break. */
#define NS16550_LCR_DLAB        (1<<7)  /**< Divisor Latch Access Bit. */

/** Modem Control Register (MCR) bits. */
#define NS16550_MCR_DTR         (1<<0)  /**< DTR. */
#define NS16550_MCR_RTS         (1<<1)  /**< RTS. */

/** Line Status Register (LSR) bits. */
#define NS16550_LSR_DR          (1<<0)  /**< Data ready. */
#define NS16550_LSR_OE          (1<<1)  /**< Overrun. */
#define NS16550_LSR_PE          (1<<2)  /**< Parity error. */
#define NS16550_LSR_FE          (1<<3)  /**< Framing error. */
#define NS16550_LSR_BI          (1<<4)  /**< Break. */
#define NS16550_LSR_THRE        (1<<5)  /**< THR empty. */
#define NS16550_LSR_TEMT        (1<<6)  /**< Transmitter empty. */
#define NS16550_LSR_ERR         (1<<7)  /**< Error. */

#ifdef CONFIG_TARGET_NS16550_IO
    typedef uint16_t ns16550_base_t;
#else
    typedef ptr_t ns16550_base_t;
#endif

extern serial_port_t *ns16550_register(ns16550_base_t base, unsigned index, uint32_t clock_rate);

#endif /* __DRIVERS_SERIAL_NS16550_H */
