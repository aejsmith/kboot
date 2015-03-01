/*
 * Copyright (C) 2012-2015 Alex Smith
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
 * @brief               KBoot test kernel definitions.
 */

#ifndef __TEST_H
#define __TEST_H

#include <arch/loader.h>

#include <platform/loader.h>

#ifdef __LP64__
#   define PHYS_MAP_BASE        0xffffffff00000000
#   define PHYS_MAP_SIZE        0x80000000
#   define VIRT_MAP_BASE        0xffffffff80000000
#   define VIRT_MAP_SIZE        0x80000000
#else
#   define PHYS_MAP_BASE        0x40000000
#   define PHYS_MAP_SIZE        0x80000000
#   define VIRT_MAP_BASE        0xc0000000
#   define VIRT_MAP_SIZE        0x40000000
#endif

/* Platform-dependent. */
#define PHYS_MAP_OFFSET         0

/* Make use of the virt/phys conversion functions in loader.h, but override the
 * arch/platform definitions for the loader and set them to be correct for our
 * address space. */
#undef TARGET_VIRT_OFFSET
#define TARGET_VIRT_OFFSET      PHYS_MAP_BASE - PHYS_MAP_OFFSET

#include <elf.h>
#include <kboot.h>
#include <loader.h>

#undef vprintf
#undef printf
#undef dvprintf
#undef dprintf

extern int vprintf(const char *fmt, va_list args);
extern int printf(const char *fmt, ...) __printf(1, 2);

extern void console_init(kboot_tag_t *tags);
extern void log_init(kboot_tag_t *tags);

extern void kmain(uint32_t magic, kboot_tag_t *tags);

#endif /* __TEST_H */
