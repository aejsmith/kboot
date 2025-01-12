/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
