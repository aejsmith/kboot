/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Timing functions.
 */

#ifndef __TIME_H
#define __TIME_H

#include <lib/utility.h>

extern mstime_t current_time(void);

extern void delay(mstime_t time);

/** Convert seconds to milliseconds.
 * @param secs          Seconds value to convert.
 * @return              Equivalent time in milliseconds. */
static inline mstime_t secs_to_msecs(unsigned secs) {
    return (mstime_t)secs * 1000;
}

/** Convert milliseconds to seconds.
 * @param msecs         Milliseconds value to convert.
 * @return              Equivalent time in seconds (rounded up). */
static inline unsigned msecs_to_secs(mstime_t msecs) {
    return round_up(msecs, 1000) / 1000;
}

#endif /* __TIME_H */
