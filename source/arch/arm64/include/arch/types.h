/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 type definitions.
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

/** Format character definitions for printf(). */
#define PRIxPHYS    "lx"            /**< Format for phys_ptr_t (hexadecimal). */
#define PRIuPHYS    "lu"            /**< Format for phys_ptr_t. */
#define PRIxLOAD    "lx"            /**< Format for load_ptr_t (hexadecimal). */
#define PRIuLOAD    "lu"            /**< Format for load_ptr_t. */

/** Integer type that can represent a pointer in the loader's address space. */
typedef uint64_t ptr_t;

/** Integer pointer/size types for use by OS loaders. */
typedef uint64_t load_ptr_t;
typedef uint64_t load_size_t;

/** Integer types that can represent a physical address/size. */
typedef uint64_t phys_ptr_t;
typedef uint64_t phys_size_t;

#endif /* __ARCH_TYPES_H */
