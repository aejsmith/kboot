/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               EFI video functions.
 */

#ifndef __EFI_VIDEO_H
#define __EFI_VIDEO_H

#include <video.h>

extern void efi_video_init(void);
extern void efi_video_reset(void);

#endif /* __EFI_VIDEO_H */
