/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               Character set conversion functions.
 */

#include <lib/charset.h>

/**
 * Convert a UTF-16 string to UTF-8.
 *
 * Converts a UTF-16 or UCS-2 string in native endian to a UTF-8 string. The
 * supplied destination buffer must be at least (src_len * MAX_UTF8_PER_UTF16)
 * bytes long. The converted string will NOT be NULL-terminated.
 *
 * @param dest          Destination buffer.
 * @param src           Source string.
 * @param src_len       Maximum source length, will return early if a zero
 *                      character is encountered.
 *
 * @return              End of converted string.
 */
uint8_t *utf16_to_utf8(uint8_t *dest, const uint16_t *src, size_t src_len) {
    uint32_t surrogate = 0;

    while (src_len--) {
        uint32_t code = *src++;

        if (surrogate) {
            /* Have a high surrogate, this is the low surrogate. */
            if (code < 0xdc00 || code > 0xdfff) {
                *dest++ = '?';
            } else {
                code = 0x10000 + (((surrogate - 0xd800) << 10) | (code - 0xdc00));

                *dest++ = 0xf0 | (code >> 18);
                *dest++ = 0x80 | ((code >> 12) & 0x3f);
                *dest++ = 0x80 | ((code >> 6) & 0x3f);
                *dest++ = 0x80 | (code & 0x3f);
            }

            surrogate = 0;
        } else if (!code) {
            break;
        } else if (code <= 0x7f) {
            *dest++ = code;
        } else if (code <= 0x7ff) {
            *dest++ = 0xc0 | (code >> 6);
            *dest++ = 0x80 | (code & 0x3f);
        } else if (code >= 0xd800 && code <= 0xdbff) {
            /* High surrogate. */
            surrogate = code;
        } else if (code >= 0xdc00 && code <= 0xdfff) {
            /* This is a low surrogate code but we haven't had a high. */
            *dest++ = '?';
        } else {
            *dest++ = 0xe0 | (code >> 12);
            *dest++ = 0x80 | ((code >> 6) & 0x3f);
            *dest++ = 0x80 | (code & 0x3f);
        }
    }

    return dest;
}
