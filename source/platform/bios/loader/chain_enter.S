/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               BIOS chain loader entry function.
 */

#include <platform/loader.h>

#include <x86/asm.h>
#include <x86/cpu.h>
#include <x86/descriptor.h>

/** Where to load the boot sector to. */
#define CHAIN_LOAD_ADDR         0x7c00

.section .text, "ax", @progbits

/** Drop to real mode and jump to a new boot sector.
 * @param id            ID of boot disk.
 * @param partition     Address of partition table entry. */
FUNCTION_START(chain_loader_enter)
    /* Save arguments. */
    movl    4(%esp), %edx
    movl    8(%esp), %esi

    /* Switch to the real-mode stack. */
    movl    $CHAIN_LOAD_ADDR, %esp

    /* Switch to the BIOS IVT. */
    lidt    (bios_idtp)

    /* Set data segments. */
    movw    $SEGMENT_DS16, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    /* Jump into the real mode code segment. */
    ljmp    $SEGMENT_CS16, $1f - LOADER_LOAD_ADDR

    /* Clear the PE (Protection Enable) bit of CR0. */
1:  movl    %cr0, %eax
    andl    $~X86_CR0_PE, %eax
    movl    %eax, %cr0

    /* Set correct segments. */
    .byte 0x66
    ljmp    $LOADER_LOAD_SEGMENT, $2f - LOADER_LOAD_ADDR
.code16
2:  xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    sti

    push    %edx
    push    %esi

    /* Try to disable the A20 line. */
    call    disable_a20

    /* Jump to the loader. */
    xorl    %eax, %eax
    xorl    %ebx, %ebx
    xorl    %ecx, %ecx
    xorl    %edi, %edi
    xorl    %ebp, %ebp
    pop     %esi
    pop     %edx
    ljmp    $0, $CHAIN_LOAD_ADDR
FUNCTION_END(chain_loader_enter)

/** Disable the A20 line. */
PRIVATE_FUNCTION_START(disable_a20)
    mov     $0x100, %cx
.Lbegin:
    /* First try using a BIOS interrupt. */
    mov     $0x2400, %ax
    int     $0x15
    call    test_a20
    test    %ax, %ax
    jnz     .Ldisable_fast
    ret
.Ldisable_fast:
    /* Try the fast A20 mechanism. */
    inb     $0x92, %al
    andb    $~((1<<0) | (1<<1)), %al
    outb    %al, $0x92
    call    test_a20
    test    %ax, %ax
    jnz     .Lfail
    ret
.Lfail:
    loop    .Lbegin

    /* Don't error on failure, on some machines A20 is always enabled. */
    ret
FUNCTION_END(disable_a20)
