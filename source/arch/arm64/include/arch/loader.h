/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 architecture core definitions.
 */

#ifndef __ARCH_LOADER_H
#define __ARCH_LOADER_H

#include <types.h>

/**
 * We do not support unaligned access. There is a flag to control whether
 * unaligned accesses are allowed (SCTLR_ELx.A = 1 -> fault, = 0 -> allowed),
 * however this is only applicable to Normal memory. Device memory accesses
 * cannot be unaligned.
 *
 * We run the loader with the MMU disabled, which causes all memory to be
 * treated as Device-nGnRnE, and therefore we cannot perform unaligned accesses.
 */
//#define TARGET_SUPPORTS_UNALIGNED_ACCESS 0

/** Spin loop hint. */
static inline void arch_pause(void) {
    __asm__ __volatile__("yield");
}

extern void arch_init(void);

#endif /* __ARCH_LOADER_H */
