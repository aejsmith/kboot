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
 * @brief               Compiler-specific macros/definitions.
 */

#ifndef __COMPILER_H
#define __COMPILER_H

#ifdef __GNUC__
#   define __unused         __attribute__((unused))
#   define __used           __attribute__((used))
#   define __packed         __attribute__((packed))
#   define __aligned(a)     __attribute__((aligned(a)))
#   define __noreturn       __attribute__((noreturn))
#   define __malloc         __attribute__((malloc))
#   define __printf(a, b)   __attribute__((format(printf, a, b)))
#   define __deprecated     __attribute__((deprecated))
#   define __section(s)     __attribute__((section(s)))
#   define likely(x)        __builtin_expect(!!(x), 1)
#   define unlikely(x)      __builtin_expect(!!(x), 0)
#else
#   error "KBoot does not currently support non-GCC compatible compilers"
#endif

#define STRINGIFY(val)      #val
#define XSTRINGIFY(val)     STRINGIFY(val)

#define static_assert(cond) \
    do { \
        struct __static_assert { \
            char static_assert_failed[(cond) ? 1 : -1]; \
        }; \
    } while (0)

#endif /* __COMPILER_H */
