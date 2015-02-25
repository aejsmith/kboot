/*
 * Copyright (C) 2010-2015 Alex Smith
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
#define linear_to_segoff(lin) \
    (uint32_t)(((lin & 0xfffffff0) << 12) + (lin & 0xf))

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

extern void bios_console_init(void);
extern void bios_disk_init(void);
extern void bios_video_init(void);

extern void bios_main(void);

#endif /* __BIOS_BIOS_H */
