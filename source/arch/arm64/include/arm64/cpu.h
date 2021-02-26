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
 * @brief               ARM64 CPU register definitions.
 */

#ifndef __ARM64_CPU_H
#define __ARM64_CPU_H

/** Current Exception Level values, as contained in CurrentEL */
#define ARM64_CURRENTEL_EL0     (0<<2)
#define ARM64_CURRENTEL_EL1     (1<<2)
#define ARM64_CURRENTEL_EL2     (2<<2)
#define ARM64_CURRENTEL_EL3     (3<<2)

/** Exception Syndrome Register (ESR_ELx). */
#define ARM64_ESR_ELx_EC_SHIFT  26
#define ARM64_ESR_ELx_EC_MASK   (0x3ful << ARM64_ESR_ELx_EC_SHIFT)
#define ARM64_ESR_ELx_EC(esr)   (((esr) & ARM64_ESR_ELx_EC_MASK) >> ARM64_ESR_ELx_EC_SHIFT)

#ifndef __ASM__

#include <types.h>

extern int arm64_loader_el;

/** Check whether the loader is running in EL2. */
static inline bool arm64_is_el2(void) {
    return arm64_loader_el == 2;
}

/** Read from a system register.
 * @param r             Register name.
 * @return              Register value. */
#define arm64_read_sysreg(r) \
    __extension__ \
    ({ \
        unsigned long __v; \
        __asm__ __volatile__("mrs %0, " STRINGIFY(r) : "=r"(__v)); \
        __v; \
    })

/** Read from a system register for the loader's current EL.
 * @param r             Register name (will have appropriate _el suffix added).
 * @return              Register value. */
#define arm64_read_sysreg_el(r) \
    __extension__ \
    ({ \
        unsigned long __v; \
        if (arm64_is_el2()) { \
            __asm__ __volatile__("mrs %0, " STRINGIFY(r) "_el2" : "=r"(__v)); \
        } else { \
            __asm__ __volatile__("mrs %0, " STRINGIFY(r) "_el1" : "=r"(__v)); \
        } \
        __v; \
    })

/** Write to a system register.
 * @param r             Register name.
 * @param v             Value to write. */
#define arm64_write_sysreg(r, v) \
    do { \
        unsigned long __v = (unsigned long)(v); \
        __asm__ __volatile__("msr " STRINGIFY(r) ", %x0" :: "rZ"(__v)); \
    } while (0)

/** Write to a system register for the loader's current EL.
 * @param r             Register name (will have appropriate _el suffix added).
 * @param v             Value to write. */
#define arm64_write_sysreg_el(r, v) \
    do { \
        unsigned long __v = (unsigned long)(v); \
        if (arm64_is_el2()) { \
            __asm__ __volatile__("msr " STRINGIFY(r) "_el2" ", %x0" :: "rZ"(__v)); \
        } else { \
            __asm__ __volatile__("msr " STRINGIFY(r) "_el1" ", %x0" :: "rZ"(__v)); \
        } \
    } while (0)

#endif /* __ASM__ */
#endif /* __ARM64_CPU_H */
