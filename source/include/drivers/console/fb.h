/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Framebuffer console implementation.
 *
 * Note: the framebuffer console must be initialized after memory detection is
 * done as it uses the physical memory manager to allocate a backbuffer.
 */

#ifndef __DRIVERS_CONSOLE_FB_H
#define __DRIVERS_CONSOLE_FB_H

#include <console.h>

extern console_out_t *fb_console_create(void);

#endif /* __DRIVERS_CONSOLE_FB_H */
