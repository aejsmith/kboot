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
 * @brief		VGA console implementation.
 */

#ifndef __DRIVERS_VIDEO_VGA_H
#define __DRIVERS_VIDEO_VGA_H

#include <types.h>

/** VGA register definitions. */
#define VGA_CRTC_INDEX		0x3d4
#define VGA_CRTC_DATA		0x3d5

/** VGA memory address. */
#define VGA_MEM_BASE		0xb8000

extern void vga_init(uint16_t cols, uint16_t lines);

#endif /* __DRIVERS_VIDEO_VGA_H */
