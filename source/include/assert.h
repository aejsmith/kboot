/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Assertion macro.
 */

#ifndef __ASSERT_H
#define __ASSERT_H

#include <loader.h>

#ifdef CONFIG_DEBUG

/** Ensure that a condition is true, and raise an error if not.
 * @todo                Should be able to disable this in non-debug builds. */
#define assert(cond) \
    if (unlikely(!(cond))) \
        internal_error("Assertion failure: %s\nat %s:%d", #cond, __FILE__, __LINE__);

#else

#define assert(cond)

#endif /* CONFIG_DEBUG */
#endif /* __ASSERT_H */
