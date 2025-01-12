/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BIOS platform Linux entry function.
 */

#include <platform/loader.h>

#include <x86/asm.h>
#include <x86/cpu.h>
#include <x86/descriptor.h>

.section .text, "ax", @progbits

/** Enter a Linux kernel.
 * @param entry         32-bit entry point address.
 * @param params        Pointer to kernel parameters structure. */
FUNCTION_START(linux_platform_enter)
    cli

    /* Load the GDT and new segments. */
    lgdt    (linux_gdtp)
    ljmp    $0x10, $2f
2:
    mov     $0x18, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Get the arguments. */
    movl    4(%esp), %eax
    movl    8(%esp), %esi

    /* Clear out EFLAGS and other registers required to be 0. */
    xorl    %ebp, %ebp
    xorl    %edi, %edi
    xorl    %ebx, %ebx
    push    $0
    popf

    /* Jump to the kernel entry point. */
    call    *%eax
1:  jmp     1b
FUNCTION_END(linux_platform_enter)

.section .data, "aw", @progbits

/** GDT pointer. */
linux_gdtp:
    .word .L__gdt_end - __linux_gdt - 1
    .long __linux_gdt

/** Global descriptor table. */
__linux_gdt:
    .quad 0x0000000000000000        /**< NULL descriptor (0x00). */
    .quad 0x0000000000000000        /**< NULL descriptor (0x08). */
    .quad 0x00cf9a000000ffff        /**< 32-bit code     (0x10). */
    .quad 0x00cf92000000ffff        /**< 32-bit data     (0x18). */
.L__gdt_end:
