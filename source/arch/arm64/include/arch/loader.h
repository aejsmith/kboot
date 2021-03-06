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
