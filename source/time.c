/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Timing functions.
 */

#include <loader.h>
#include <time.h>

/** Delay for a number of milliseconds.
 * @param msecs         Milliseconds to delay for. */
void delay(mstime_t msecs) {
    mstime_t target = current_time() + msecs;

    while (current_time() < target)
        arch_pause();
}
