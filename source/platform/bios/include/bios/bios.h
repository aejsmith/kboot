/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BIOS platform main definitions.
 */

#ifndef __BIOS_BIOS_H
#define __BIOS_BIOS_H

#include <lib/string.h>

#include <x86/cpu.h>

/** Memory area to use when passing data to BIOS interrupts (56KB).
 * @note                Area is actually 60KB, but the last 4KB are used for
 *                      the stack. */
#define BIOS_MEM_BASE       0x1000
#define BIOS_MEM_SIZE       0xe000

/** Convert a segment + offset pair to a linear address. */
#define segoff_to_linear(segoff) \
    (ptr_t)((((segoff) & 0xffff0000) >> 12) + ((segoff) & 0xffff))

/** Convert a linear address to a segment + offset pair. */
#define linear_to_segoff(linear) \
    (uint32_t)((((linear) & 0xfffffff0) << 12) + ((linear) & 0xf))

/** Structure describing registers to pass to a BIOS interrupt. */
typedef struct bios_regs {
    union {
        struct {
            uint32_t eflags;
            uint32_t eax;
            uint32_t ebx;
            uint32_t ecx;
            uint32_t edx;
            uint32_t edi;
            uint32_t esi;
            uint32_t ebp;
            uint32_t _es;
        };
        struct {
            uint16_t flags, _hflags;
            uint16_t ax, _hax;
            uint16_t bx, _hbx;
            uint16_t cx, _hcx;
            uint16_t dx, _hdx;
            uint16_t di, _hdi;
            uint16_t si, _hsi;
            uint16_t bp, _hbp;
            uint16_t es, _hes;
        };
    };
} bios_regs_t;

/** Initialise a BIOS registers structure.
 * @param regs          Structure to initialise. */
static inline void bios_regs_init(bios_regs_t *regs) {
    memset(regs, 0, sizeof(bios_regs_t));
}

extern void bios_call(uint8_t num, bios_regs_t *regs);
extern uint16_t bios_pxe_call(uint16_t func, uint32_t segoff);

extern void bios_main(void) __noreturn;

#endif /* __BIOS_BIOS_H */
