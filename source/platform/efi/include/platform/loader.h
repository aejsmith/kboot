/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI platform core definitions.
 */

#ifndef __PLATFORM_LOADER_H
#define __PLATFORM_LOADER_H

/** Avoid allocating low memory as firmware tends to do funny things with it. */
#define TARGET_PHYS_MIN     0x100000

/** Properties of the platform (functions we provide etc.). */
#define TARGET_RELOCATABLE  1
#define TARGET_HAS_MM       1
#define TARGET_HAS_EXIT     1

#endif /* __PLATFORM_LOADER_H */
