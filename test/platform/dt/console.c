/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               DT platform console functions.
 */

#include <console.h>
#include <test.h>

KBOOT_VIDEO(KBOOT_VIDEO_LFB, 0, 0, 0);

/** Initialize the fallback debug console. */
void platform_debug_console_init(void) {
    /* Assume on DT that we have been supplied a KBOOT_TAG_SERIAL for a
     * serial port from DT. */
}
