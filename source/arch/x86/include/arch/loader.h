/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 architecture core definitions.
 */

#ifndef __ARCH_LOADER_H
#define __ARCH_LOADER_H

#include <types.h>

/** We support unaligned memory accesses. */
#define TARGET_SUPPORTS_UNALIGNED_ACCESS  1

/** Spin loop hint. */
static inline void arch_pause(void) {
    __asm__ __volatile__("pause");
}

extern void arch_init(void);

#endif /* __ARCH_LOADER_H */
