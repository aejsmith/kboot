/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 KBoot kernel entry functions.
 */

#include <arch/page.h>

#include <platform/loader.h>

#include <x86/asm.h>
#include <x86/cpu.h>
#include <x86/descriptor.h>

#include <kboot.h>

.section .text, "ax", @progbits

/** Offsets in entry_args_t. */
#define ENTRY_ARGS_TRAMPOLINE_CR3   0
#define ENTRY_ARGS_TRAMPOLINE_VIRT  8
#define ENTRY_ARGS_KERNEL_CR3       16
#define ENTRY_ARGS_SP               24
#define ENTRY_ARGS_ENTRY            32
#define ENTRY_ARGS_TAGS             40
#define ENTRY_ARGS_TRAMPOLINE       48

/** 32-bit kernel trampoline. */
FUNCTION_START(kboot_trampoline_32)
.code32
    /* Switch to the real kernel page directory. */
    movl    ENTRY_ARGS_KERNEL_CR3(%edi), %eax
    movl    %eax, %cr3

    /* Set the stack pointer. */
    movl    ENTRY_ARGS_SP(%edi), %esp

    /* Clear the stack frame/EFLAGS. */
    xorl    %ebp, %ebp
    push    $0
    popf

    /* Retrieve tag list address and entry point. */
    movl    ENTRY_ARGS_TAGS(%edi), %edx
    movl    ENTRY_ARGS_ENTRY(%edi), %eax

    /* Call the kernel. */
    push    %edx
    push    $KBOOT_MAGIC
    call    *%eax
1:  jmp     1b

SYMBOL(kboot_trampoline_32_size)
    .long   . - kboot_trampoline_32
FUNCTION_END(kboot_trampoline_32)

/** 64-bit kernel trampoline. */
FUNCTION_START(kboot_trampoline_64)
.code64
    /* Switch to the real kernel page directory. */
    movq    ENTRY_ARGS_KERNEL_CR3(%rdi), %rax
    movq    %rax, %cr3

    /* Set the stack pointer. */
    movq    ENTRY_ARGS_SP(%rdi), %rsp

    /* Clear the stack frame/RFLAGS. */
    xorq    %rbp, %rbp
    push    $0
    popf

    /* Retrieve tag list address and entry point. */
    movq    ENTRY_ARGS_TAGS(%rdi), %rsi
    movq    ENTRY_ARGS_ENTRY(%rdi), %rax

    /* Call the kernel. */
    movq    $KBOOT_MAGIC, %rdi
    call    *%rax
1:  jmp     1b

SYMBOL(kboot_trampoline_64_size)
    .long   . - kboot_trampoline_64
FUNCTION_END(kboot_trampoline_64)

#ifdef __LP64__

/** Enter a 32-bit KBoot kernel.
 * @param args          Entry arguments structure. */
FUNCTION_START(kboot_arch_enter_32)
.code64
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

    /* Point CR3 to the trampoline page directory. */
    movl    ENTRY_ARGS_TRAMPOLINE_CR3(%edi), %eax
    movl    %eax, %cr3

    /* Get the correct virtual address for the trampoline. */
    movl    ENTRY_ARGS_TRAMPOLINE_VIRT(%edi), %edi

    /* Enable paging. */
    movl    %cr0, %ecx
    orl     $X86_CR0_PG, %ecx
    movl    %ecx, %cr0

    /* According to section 9.8.5.4 in the Intel manuals volume 3, a branch must
     * immediately follow the move to CR0 to enable paging after switching out
     * of long mode. Not sure how necessary this actually is, but do it just to
     * be safe. */
    jmp     1f
1:
    /* Jump to the trampoline. */
    leal    ENTRY_ARGS_TRAMPOLINE(%edi), %eax
    jmp     *%eax
FUNCTION_END(kboot_arch_enter_32)

/** Enter a 64-bit KBoot kernel.
 * @param args          Entry arguments structure. */
FUNCTION_START(kboot_arch_enter_64)
.code64
    /* Get the address of the trampoline PML4. */
    movq    ENTRY_ARGS_TRAMPOLINE_CR3(%rdi), %rax

    /* Save the correct virtual address for the trampoline (64-bit). */
    movq    ENTRY_ARGS_TRAMPOLINE_VIRT(%rdi), %rdi

    /* Now switch to trampoline PML4. */
    movq    %rax, %cr3

    /* Jump to the trampoline. */
    leaq    ENTRY_ARGS_TRAMPOLINE(%rdi), %rax
    jmp     *%rax
FUNCTION_END(kboot_arch_enter_64)

#else /* __LP64__ */

/** Enter a 32-bit KBoot kernel.
 * @param args          Entry arguments structure. */
FUNCTION_START(kboot_arch_enter_32)
.code32
    /* Store arguments address in EDI. */
    movl    4(%esp), %edi

    /* Point CR3 to the trampoline page directory. */
    movl    ENTRY_ARGS_TRAMPOLINE_CR3(%edi), %eax
    movl    %eax, %cr3

    /* Get the correct virtual address for the trampoline. */
    movl    ENTRY_ARGS_TRAMPOLINE_VIRT(%edi), %edi

    /* Enable paging. */
    movl    %cr0, %ecx
    orl     $X86_CR0_PG, %ecx
    movl    %ecx, %cr0

    /* Jump to the trampoline. */
    leal    ENTRY_ARGS_TRAMPOLINE(%edi), %eax
    jmp     *%eax
FUNCTION_END(kboot_arch_enter_32)

/** Enter a 64-bit KBoot kernel.
 * @param args          Entry arguments structure. */
FUNCTION_START(kboot_arch_enter_64)
.code32
    /* Store arguments address in EDI. */
    movl    4(%esp), %edi

    /* Enable PAE. */
    movl    %cr4, %eax
    orl     $X86_CR4_PAE, %eax
    movl    %eax, %cr4

    /* Point CR3 to the trampoline PML4. */
    movl    ENTRY_ARGS_TRAMPOLINE_CR3(%edi), %eax
    movl    %eax, %cr3

    /* Get the correct virtual address for the trampoline (64-bit). */
    movl    ENTRY_ARGS_TRAMPOLINE_VIRT + 4(%edi), %esi
    movl    ENTRY_ARGS_TRAMPOLINE_VIRT(%edi), %edi

    /* Enable long mode by setting EFER.LME. */
    movl    $X86_MSR_EFER, %ecx
    rdmsr
    orl     $X86_EFER_LME, %eax
    wrmsr

    /* Enable paging to put us in compatibility mode. */
    movl    %cr0, %ecx
    orl     $X86_CR0_PG, %ecx
    movl    %ecx, %cr0

    /* Jump into the 64-bit code segment. */
    ljmp    $SEGMENT_CS64, $.Llong_mode
.align 8
.code64
.Llong_mode:
    /* Set data segments. */
    xorl    %eax, %eax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Get the correct trampoline virtual address. */
    movl    %edi, %edi
    shlq    $32, %rsi
    orq     %rsi, %rdi

    /* Jump to the trampoline. */
    leaq    ENTRY_ARGS_TRAMPOLINE(%rdi), %rax
    jmp     *%rax
FUNCTION_END(kboot_arch_enter_64)

#endif /* __LP64__ */
