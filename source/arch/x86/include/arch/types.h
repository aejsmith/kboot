/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 type definitions.
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

/** Format character definitions for printf(). */
#ifdef __LP64__
#   define PRIxPHYS    "lx"         /**< Format for phys_ptr_t (hexadecimal). */
#   define PRIuPHYS    "lu"         /**< Format for phys_ptr_t. */
#   define PRIxLOAD    "lx"         /**< Format for load_ptr_t (hexadecimal). */
#   define PRIuLOAD    "lu"         /**< Format for load_ptr_t. */
#else
#   define PRIxPHYS    "llx"        /**< Format for phys_ptr_t (hexadecimal). */
#   define PRIuPHYS    "llu"        /**< Format for phys_ptr_t. */
#   define PRIxLOAD    "llx"        /**< Format for load_ptr_t (hexadecimal). */
#   define PRIuLOAD    "llu"        /**< Format for load_ptr_t. */
#endif

/** Integer type that can represent a pointer in the loader's address space. */
typedef unsigned long ptr_t;

/**
 * Integer pointer/size types for use by OS loaders.
 *
 * On x86 we can load both 32-bit and 64-bit kernels, so these are defined to be
 * 64-bit to account for this.
*/
typedef uint64_t load_ptr_t;
typedef uint64_t load_size_t;

/** Integer types that can represent a physical address/size. */
typedef uint64_t phys_ptr_t;
typedef uint64_t phys_size_t;

#endif /* __ARCH_TYPES_H */
