/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 ELF definitions.
 */

#ifndef __ARCH_ELF_H
#define __ARCH_ELF_H

/** Definitions of platform ELF properties. */
#define ELF_MACHINE_32  ELF_EM_ARM      /**< 32-bit ELF machine type. */
#define ELF_MACHINE_64  ELF_EM_AARCH64  /**< 64-bit ELF machine type. */
#define ELF_ENDIAN      ELFDATA2LSB     /**< ELF endianness (little-endian). */

#endif /* __ARCH_ELF_H */
