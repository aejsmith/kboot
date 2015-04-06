/*
 * Copyright (C) 2009-2015 Alex Smith
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
 * @brief               BIOS platform video support.
 */

#ifndef __BIOS_VIDEO_H
#define __BIOS_VIDEO_H

#include <bios/vbe.h>

#include <video.h>

extern bool bios_video_get_controller_info(vbe_info_t *info);
extern bool bios_video_get_mode_info(video_mode_t *mode, vbe_mode_info_t *info);
extern uint16_t bios_video_get_mode_num(video_mode_t *mode);

extern void bios_video_init(void);

#endif /* __BIOS_VIDEO_H */
