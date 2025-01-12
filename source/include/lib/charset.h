/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Character set conversion functions.
 */

#ifndef __LIB_CHARSET_H
#define __LIB_CHARSET_H

#include <types.h>

/** Maximum number of UTF-8 characters per UTF-16 character. */
#define MAX_UTF8_PER_UTF16      4

extern size_t utf16_to_utf8(uint8_t *dest, const uint16_t *src, size_t src_len);

#endif /* __LIB_CHARSET_H */
