/*
 * Copyright (C) 2011-2014 Alex Smith
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
 * @brief		Type definitions.
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <compiler.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef CONFIG_64BIT
# define __INT_64	long
# define __PRI_64	"l"
#else
# define __INT_64	long long
# define __PRI_64	"ll"
#endif

/** Format character definitions for printf(). */
#define PRIu8		"u"		/**< Format for uint8_t. */
#define PRIu16		"u"		/**< Format for uint16_t. */
#define PRIu32		"u"		/**< Format for uint32_t. */
#define PRIu64		__PRI_64 "u"	/**< Format for uint64_t. */
#define PRId8		"d"		/**< Format for int8_t. */
#define PRId16		"d"		/**< Format for int16_t. */
#define PRId32		"d"		/**< Format for int32_t. */
#define PRId64		__PRI_64 "d"	/**< Format for int64_t. */
#define PRIx8		"x"		/**< Format for (u)int8_t (hexadecimal). */
#define PRIx16		"x"		/**< Format for (u)int16_t (hexadecimal). */
#define PRIx32		"x"		/**< Format for (u)int32_t (hexadecimal). */
#define PRIx64		__PRI_64 "x"	/**< Format for (u)int64_t (hexadecimal). */
#define PRIo8		"o"		/**< Format for (u)int8_t (octal). */
#define PRIo16		"o"		/**< Format for (u)int16_t (octal). */
#define PRIo32		"o"		/**< Format for (u)int32_t (octal). */
#define PRIo64		__PRI_64 "o"	/**< Format for (u)int64_t (octal). */

/** Unsigned data types. */
typedef unsigned char uint8_t;		/**< Unsigned 8-bit. */
typedef unsigned short uint16_t;	/**< Unsigned 16-bit. */
typedef unsigned int uint32_t;		/**< Unsigned 32-bit. */
typedef unsigned __INT_64 uint64_t;	/**< Unsigned 64-bit. */

/** Signed data types. */
typedef signed char int8_t;		/**< Signed 8-bit. */
typedef signed short int16_t;		/**< Signed 16-bit. */
typedef signed int int32_t;		/**< Signed 32-bit. */
typedef signed __INT_64 int64_t;	/**< Signed 64-bit. */

/** Extra integer types. */
typedef int64_t offset_t;		/**< Type used to store an offset into a object. */
typedef int64_t mstime_t;		/**< Type used to store a time value in milliseconds. */

/** Type limit macros. */
#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	255u
#define INT16_MIN	(-32767-1)
#define INT16_MAX	32767
#define UINT16_MAX	65535u
#define INT32_MIN	(-2147483647-1)
#define INT32_MAX	2147483647
#define UINT32_MAX	4294967295u
#define INT64_MIN	(-9223372036854775807ll-1)
#define INT64_MAX	9223372036854775807ll
#define UINT64_MAX	18446744073709551615ull

/** Number of bits in a char. */
#define CHAR_BIT	8

#undef __INT_64
#undef __PRI_64

#include <arch/types.h>

#endif /* __TYPES_H */
