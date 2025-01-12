/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 I/O functions.
 */

#ifndef __ARCH_IO_H
#define __ARCH_IO_H

#include <types.h>

/**
 * Port I/O functions.
 */

/** Read an 8 bit value from a port.
 * @param port          Port to read from.
 * @return              Value read. */
static inline uint8_t in8(uint16_t port) {
    uint8_t ret;

    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/** Write an 8 bit value to a port.
 * @param port          Port to write to.
 * @param val           Value to write. */
static inline void out8(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %1, %0" :: "dN"(port), "a"(val));
}

/** Read a 16 bit value from a port.
 * @param port          Port to read from.
 * @return              Value read. */
static inline uint16_t in16(uint16_t port) {
    uint16_t ret;

    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/** Write a 16 bit value to a port.
 * @param port          Port to write to.
 * @param val           Value to write. */
static inline void out16(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %1, %0" :: "dN"(port), "a"(val));
}

/** Read a 32 bit value from a port.
 * @param port          Port to read from.
 * @return              Value read. */
static inline uint32_t in32(uint16_t port) {
    uint32_t ret;

    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/** Write a 32 bit value to a port.
 * @param port          Port to write to.
 * @param val           Value to write. */
static inline void out32(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %1, %0" :: "dN"(port), "a"(val));
}

/**
 * Memory mapped I/O functions.
 */

/** Read an 8 bit value from a memory mapped register.
 * @param addr          Address to read from.
 * @return              Value read. */
static inline uint8_t read8(const volatile uint8_t *addr) {
    uint8_t ret;

    __asm__ __volatile__("movb %1, %0" : "=q"(ret) : "m"(*addr) : "memory");
    return ret;
}

/** Write an 8 bit value to a memory mapped register.
 * @param addr          Address to write to.
 * @param val           Value to write. */
static inline void write8(volatile uint8_t *addr, uint8_t val) {
    __asm__ __volatile__("movb %0, %1" :: "q"(val), "m"(*addr) : "memory");
}

/** Read a 16 bit value from a memory mapped register.
 * @param addr          Address to read from.
 * @return              Value read. */
static inline uint16_t read16(const volatile uint16_t *addr) {
    uint16_t ret;

    __asm__ __volatile__("movw %1, %0" : "=r"(ret) : "m"(*addr) : "memory");
    return ret;
}

/** Write a 16 bit value to a memory mapped register.
 * @param addr          Address to write to.
 * @param val           Value to write. */
static inline void write16(volatile uint16_t *addr, uint16_t val) {
    __asm__ __volatile__("movw %0, %1" :: "r"(val), "m"(*addr) : "memory");
}

/** Read a 32 bit value from a memory mapped register.
 * @param addr          Address to read from.
 * @return              Value read. */
static inline uint32_t read32(const volatile uint32_t *addr) {
    uint32_t ret;

    __asm__ __volatile__("movl %1, %0" : "=r"(ret) : "m"(*addr) : "memory");
    return ret;
}

/** Write a 32 bit value to a memory mapped register.
 * @param addr          Address to write to.
 * @param val           Value to write. */
static inline void write32(volatile uint32_t *addr, uint32_t val) {
    __asm__ __volatile__("movl %0, %1" :: "r"(val), "m"(*addr) : "memory");
}

#endif /* __ARCH_IO_H */
