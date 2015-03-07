/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               x86 ELF definitions.
 */

#ifndef __ARCH_ELF_H
#define __ARCH_ELF_H

/** Definitions of platform ELF properties. */
#define ELF_MACHINE_32  ELF_EM_386      /**< 32-bit ELF machine type. */
#define ELF_MACHINE_64  ELF_EM_X86_64   /**< 64-bit ELF machine type. */
#define ELF_ENDIAN      ELFDATA2LSB     /**< ELF endianness (little-endian). */

#endif /* __ARCH_ELF_H */
