/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
    xorw    %si, %si

    /* Jump to the loader entry point. */
    ljmp    $LOADER_LOAD_SEGMENT, $0
FUNCTION_END(_start)

/** Pad the file to the loader load address. */
. = _start + LOADER_LOAD_ADDR - 0x7c00 - 1;
.byte 0
