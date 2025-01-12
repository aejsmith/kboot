/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 paging definitions.
 */

#ifndef __ARCH_PAGE_H
#define __ARCH_PAGE_H

/** Page size definitions. */
#define PAGE_WIDTH          12              /**< Width of a page in bits. */
#define PAGE_SIZE           0x1000          /**< Size of a page. */
#define LARGE_PAGE_SIZE     0x200000        /**< Size of a large page. */

#endif /* __ARCH_PAGE_H */
