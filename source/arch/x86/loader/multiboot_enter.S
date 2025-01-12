/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Multiboot kernel entry functions.
 */

#include <x86/asm.h>
#include <x86/cpu.h>
#include <x86/descriptor.h>
#include <x86/multiboot.h>

.section .text, "ax", @progbits

#ifdef __LP64__

/** Enter a Multiboot kernel.
 * @param entry         Entry point address.
 * @param info          Information structure address. */
FUNCTION_START(multiboot_loader_enter)
    /* Switch to the 32-bit code segment. */
    pushq   $SEGMENT_CS32
    leaq    .Lprotected_mode(%rip), %rax
    push    %rax
    lretq
.align 8
.code32
.Lprotected_mode:
    /* Set data segments. */
    mov     $SEGMENT_DS32, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Disable paging. */
    movl    %cr0, %ecx
    andl    $~X86_CR0_PG, %ecx
    movl    %ecx, %cr0

    /* Disable long mode. */
    movl    $X86_MSR_EFER, %ecx
    rdmsr
    andl    $~X86_EFER_LME, %eax
    wrmsr

    /* Disable PAE. */
    movl    %cr4, %eax
    andl    $~X86_CR4_PAE, %eax
    movl    %eax, %cr4

    /* Prepare arguments. */
    movl    $MULTIBOOT_LOADER_MAGIC, %eax
    movl    %esi, %ebx

    /* Clear EFLAGS. */
    push    $0
    popf

    /* Jump to the kernel. */
    jmp     *%edi
FUNCTION_END(multiboot_loader_enter)

#else /* __LP64__ */

/** Enter a Multiboot kernel.
 * @param entry         Entry point address.
 * @param info          Information structure address. */
FUNCTION_START(multiboot_loader_enter)
    /* Prepare arguments. */
    movl    $MULTIBOOT_LOADER_MAGIC, %eax
    movl    8(%esp), %ebx

    /* Clear EFLAGS. */
    push    $0
    popf

    /* Jump to the kernel. */
    movl    4(%esp), %edx
    jmp     *%edx
FUNCTION_END(multiboot_loader_enter)

#endif /* __LP64__ */
