/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 CPU register definitions.
 */

#ifndef __ARM64_CPU_H
#define __ARM64_CPU_H

#include <types.h>

/** Current Exception Level values, as contained in CurrentEL */
#define ARM64_CURRENTEL_EL0     (0<<2)
#define ARM64_CURRENTEL_EL1     (1<<2)
#define ARM64_CURRENTEL_EL2     (2<<2)
#define ARM64_CURRENTEL_EL3     (3<<2)

/** Exception Syndrome Register (ESR_ELx). */
#define ARM64_ESR_EC_SHIFT      26
#define ARM64_ESR_EC_MASK       (0x3ful << ARM64_ESR_EC_SHIFT)
#define ARM64_ESR_EC(esr)       (((esr) & ARM64_ESR_EC_MASK) >> ARM64_ESR_EC_SHIFT)

/** Hypervisor Control Register (HCR_EL2). */
#define ARM64_HCR_RW            (UL(1)<<31)

/** Saved Program Status Register (SPSR_ELx). */
#define ARM64_SPSR_MODE_EL0T    (UL(0)<<0)
#define ARM64_SPSR_MODE_EL1T    (UL(4)<<0)
#define ARM64_SPSR_MODE_EL1H    (UL(5)<<0)
#define ARM64_SPSR_MODE_EL2T    (UL(8)<<0)
#define ARM64_SPSR_MODE_EL2H    (UL(9)<<0)
#define ARM64_SPSR_F            (UL(1)<<6)
#define ARM64_SPSR_I            (UL(1)<<7)
#define ARM64_SPSR_A            (UL(1)<<8)
#define ARM64_SPSR_D            (UL(1)<<9)

/** System Control Register (SCTLR_ELx). */
#define ARM64_SCTLR_M           (UL(1)<<0)
#define ARM64_SCTLR_A           (UL(1)<<1)
#define ARM64_SCTLR_C           (UL(1)<<2)
#define ARM64_SCTLR_I           (UL(1)<<12)
#define ARM64_SCTLR_EL1_RES1    ((UL(1)<<11) | (UL(1)<<20) | (UL(1)<<22) | (UL(1)<<28) | (UL(1)<<29))

/** Translation Control Register (TCR_ELx). */
#define ARM64_TCR_T0SZ_SHIFT    0
#define ARM64_TCR_IRGN0_WB_WA   (UL(1)<<8)
#define ARM64_TCR_ORGN0_WB_WA   (UL(1)<<10)
#define ARM64_TCR_SH0_INNER     (UL(3)<<12)
#define ARM64_TCR_TG0_4         (UL(0)<<14)
#define ARM64_TCR_T1SZ_SHIFT    16
#define ARM64_TCR_IRGN1_WB_WA   (UL(1)<<24)
#define ARM64_TCR_ORGN1_WB_WA   (UL(1)<<26)
#define ARM64_TCR_SH1_INNER     (UL(3)<<28)
#define ARM64_TCR_TG1_4         (UL(2)<<30)
#define ARM64_TCR_IPS_48        (UL(5)<<32)
#define ARM64_TCR_TBI0          (UL(1)<<37)
#define ARM64_TCR_TBI1          (UL(1)<<38)

#ifndef __ASM__

extern int arm64_loader_el;

/** Check whether the loader is running in EL2. */
static inline bool arm64_is_el2(void) {
    return arm64_loader_el == 2;
}

extern void arm64_switch_to_el1(void);

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
