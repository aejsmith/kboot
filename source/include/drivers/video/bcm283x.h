/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BCM283x firmware-based video driver.
 */

#ifndef __DRIVERS_VIDEO_BCM283X_H
#define __DRIVERS_VIDEO_BCM283X_H

#include <drivers/platform/bcm283x/mbox.h>

#include <status.h>

extern status_t bcm283x_video_init(bcm283x_mbox_t *mbox);

#endif /* __DRIVERS_VIDEO_BCM283X_H */
