/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Shell interface.
 */

#ifndef __SHELL_H
#define __SHELL_H

#include <types.h>

extern bool shell_enabled;

extern void shell_main(void) __noreturn;

#endif /* __SHELL_H */
