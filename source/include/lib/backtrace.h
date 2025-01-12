/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Backtrace function.
 */

#ifndef __LIB_BACKTRACE_H
#define __LIB_BACKTRACE_H

#include <lib/printf.h>

extern void backtrace(printf_t func);

#endif /* __LIB_BACKTRACE_H */
