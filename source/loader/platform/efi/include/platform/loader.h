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
 * @brief		EFI platform core definitions.
 */

#ifndef __PLATFORM_LOADER_H
#define __PLATFORM_LOADER_H

/** Segment defintions. */
#define SEGMENT_CS		0x08		/**< Code segment. */
#define SEGMENT_DS		0x10		/**< Data segment. */
#define SEGMENT_CS16		0x18		/**< 16-bit code segment. */
#define SEGMENT_DS16		0x20		/**< 16-bit code segment. */
#define SEGMENT_CS64		0x28		/**< 64-bit code segment. */
#define SEGMENT_DS64		0x30		/**< 64-bit data segment. */

#ifndef __ASM__

extern void platform_init(void);

#endif /* __ASM__ */
#endif /* __PLATFORM_LOADER_H */
