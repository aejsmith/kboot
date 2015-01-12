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
 * @brief               x86 optimized string handling functions.
 */

#include <lib/string.h>

/**
 * Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @param dest          The memory area to copy to.
 * @param src           The memory area to copy from.
 * @param count         The number of bytes to copy.
 *
 * @return              Destination location.
 */
void *memcpy(void *__restrict dest, const void *__restrict src, size_t count) {
    register unsigned long out;

    asm volatile("rep movsb"
        : "=&D"(out), "=&S"(src), "=&c"(count)
        : "0"(dest), "1"(src), "2"(count)
        : "memory");

    return dest;
}

/** Fill a memory area.
 * @param dest          The memory area to fill.
 * @param val           The value to fill with (converted to an unsigned char).
 * @param count         The number of bytes to fill.
 * @return              Destination location. */
void *memset(void *dest, int val, size_t count) {
    register unsigned long out;

    asm volatile("rep stosb"
        : "=&D"(out), "=&c"(count)
        : "a"(val), "0"(dest), "1"(count)
        : "memory");

    return dest;
}
