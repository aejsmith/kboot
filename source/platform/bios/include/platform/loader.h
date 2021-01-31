/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               BIOS platform core definitions.
 */

#ifndef __PLATFORM_LOADER_H
#define __PLATFORM_LOADER_H

/** Avoid allocating low memory as firmware tends to do funny things with it. */
#define TARGET_PHYS_MIN         0x100000

/** Load address of the boot loader. */
#define LOADER_LOAD_ADDR        0x10000

/** Load segment of the boot loader. */
#define LOADER_LOAD_SEGMENT     (LOADER_LOAD_ADDR >> 4)

/**
 * Multiboot load address.
 *
 * When we are loaded via Multiboot, we cannot load to our real load address
 * as the boot loader that loads us is probably there. Therefore, we load
 * higher up, and the entry code will relocate us to the correct place.
 */
#define MULTIBOOT_LOAD_ADDR     0x100000

/** Load offset for Multiboot. */
#define MULTIBOOT_LOAD_OFFSET   (MULTIBOOT_LOAD_ADDR - LOADER_LOAD_ADDR)

#endif /* __PLATFORM_LOADER_H */
