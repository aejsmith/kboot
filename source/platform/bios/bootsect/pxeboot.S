/*
 * Copyright (C) 2010-2015 Alex Smith
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
 * @brief               BIOS PXE boot sector.
 */

#include <platform/loader.h>

#include <x86/asm.h>

.section .text
.code16

/** Main function of the PXE boot sector. */
FUNCTION_START(_start)
    /* Set the boot drive ID to 0x7f, which indicates to the loader we booted
     * via PXE. */
    movb    $0x7f, %dl
    xorl    %ebx, %ebx
    xorl    %ecx, %ecx

    /* Jump to the loader entry point. */
    ljmp    $(LOADER_LOAD_ADDR >> 4), $0
FUNCTION_END(_start)

/** Pad the file to the loader load address. */
. = _start + LOADER_LOAD_ADDR - 0x7c00 - 1;
.byte 0
