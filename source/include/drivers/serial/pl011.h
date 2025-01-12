/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM PL011 UART driver.
 */

#ifndef __DRIVERS_SERIAL_PL011_H
#define __DRIVERS_SERIAL_PL011_H

#include <drivers/console/serial.h>

#ifdef CONFIG_DRIVER_SERIAL_PL011

/** PL011 UART port definitions. */
#define PL011_REG_DR            0   /**< Data Register. */
#define PL011_REG_RSR           1   /**< Receive Status Register. */
#define PL011_REG_ECR           1   /**< Error Clear Register. */
#define PL011_REG_FR            6   /**< Flag Register. */
#define PL011_REG_IBRD          9   /**< Integer Baud Rate Register. */
#define PL011_REG_FBRD          10  /**< Fractional Baud Rate Register. */
#define PL011_REG_LCRH          11  /**< Line Control Register. */
#define PL011_REG_CR            12  /**< Control Register. */
#define PL011_REG_IFLS          13  /**< Interrupt FIFO Level Select Register. */
#define PL011_REG_IMSC          14  /**< Interrupt Mask Set/Clear Register. */
#define PL011_REG_RIS           15  /**< Raw Interrupt Status Register. */
#define PL011_REG_MIS           16  /**< Masked Interrupt Status Register. */
#define PL011_REG_ICR           17  /**< Interrupt Clear Register. */
#define PL011_REG_DMACR         18  /**< DMA Control Register. */

/** PL011 flag register bits. */
#define PL011_FR_TXFF           (1<<5)  /**< Transmit FIFO full. */
#define PL011_FR_RXFE           (1<<4)  /**< Receive FIFO empty. */

/** PL011 line control register bits. */
#define PL011_LCRH_PEN          (1<<1)  /**< Parity enable. */
#define PL011_LCRH_EPS          (1<<2)  /**< Even parity select. */
#define PL011_LCRH_STP2         (1<<3)  /**< 2 stop bits. */
#define PL011_LCRH_FEN          (1<<4)  /**< Enable FIFOs. */
#define PL011_LCRH_WLEN_SHIFT   5       /**< Shift for data bit count. */
#define PL011_LCRH_WLEN5        (0<<5)  /**< 5 data bits. */
#define PL011_LCRH_WLEN6        (1<<5)  /**< 6 data bits. */
#define PL011_LCRH_WLEN7        (2<<5)  /**< 7 data bits. */
#define PL011_LCRH_WLEN8        (3<<5)  /**< 8 data bits. */

/** PL011 control register bit definitions. */
#define PL011_CR_UARTEN         (1<<0)  /**< UART enable. */
#define PL011_CR_TXE            (1<<8)  /**< Transmit enable. */
#define PL011_CR_RXE            (1<<9)  /**< Receive enable. */

extern serial_port_t *pl011_register(ptr_t base, unsigned index, uint32_t clock_rate);

#ifdef CONFIG_TARGET_HAS_FDT

extern serial_port_t *dt_pl011_register(int node_offset);

#endif

#endif /* CONFIG_DRIVER_SERIAL_PL011 */
#endif /* __DRIVERS_SERIAL_PL011_H */
