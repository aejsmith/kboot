/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               IA32 EFI startup code.
 */

#include <arch/page.h>

#include <x86/asm.h>
#include <x86/descriptor.h>

.section .text, "ax", @progbits

/** EFI entry point.
 * @param 4(%esp)       Handle to the loader image.
 * @param 8(%esp)       Pointer to EFI system table. */
FUNCTION_START(loader_entry)
    /* Figure out our load address, and the address of the dynamic section. */
    call    1f
1:  popl    %eax
    movl    %eax, %ecx
    addl    $(__start - 1b), %eax
    addl    $(_DYNAMIC - 1b), %ecx

    /* Relocate the loader. */
    pushl   %ecx
    pushl   %eax
    call    efi_arch_relocate
    addl    $8, %esp
    testl   %eax, %eax
    jz      2f
    ret
2:
    /* Clear EFLAGS. This will disable interrupts. */
    push    $0
    popf

    /* Zero the BSS section. */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorb    %al, %al
    rep stosb

    /* Save the EFI GDT and IDT pointers which we must restore before calling
     * any EFI functions. */
    sgdt    efi_gdtp
    sidt    efi_idtp
    movw    %cs, efi_cs

    /* Load the GDT. */
    lgdt    loader_gdtp
    ljmp    $SEGMENT_CS, $3f
3:
    /* Clear the stack frame pointer. */
    xorl    %ebp, %ebp

    /* Call the main function. Arguments are on the stack. */
    jmp     efi_main
FUNCTION_END(loader_entry)

/** EFI call wrapper. */
FUNCTION_START(__efi_call)
    /* Get the function address to call. */
    movl    __efi_call_func, %edx

    /* We need to maintain 16 byte stack alignment. We will have been called
     * with that satisfied, but then a call to the function will break that.
     * Can't just lower ESP because of the arguments, so swap out the return
     * address on the stack to point to .Lreturn below, and jump to the function
     * instead. Then we jump back to that saved return address later. */
    movl    (%esp), %eax
    movl    %eax, __efi_call_func
    movl    $.Lreturn, (%esp)

    /* Switch to the EFI GDT/IDT. */
    lgdt    efi_gdtp
    movzxw  efi_cs, %eax
    pushl   %eax
    movl    $1f, %eax
    pushl   %eax
    lretl
1:  lidt    efi_idtp

    /* Enable interrupts. It's possible there's some EFI implementations that
     * break if you call them with interrupts off... */
    sti

    /* Call the function. */
    jmp     *%edx

.Lreturn:
    /* Disable interrupts again. */
    cli

    /* Load our GDT/IDT. */
    lgdt    loader_gdtp
    ljmp    $SEGMENT_CS, $2f
2:  lidt    loader_idtp

    /* Jump back the correct return address. */
    movl    __efi_call_func, %edx
    jmp     *%edx
FUNCTION_END(__efi_call)

/** Enter a Linux kernel using the handover entry point.
 * @param handle        Handle to the loader image.
 * @param table         Pointer to EFI system table.
 * @param params        Kernel parameters structure.
 * @param entry         Entry point address. */
FUNCTION_START(linux_platform_enter)
    /* Switch to the EFI GDT/IDT. */
    lgdt    efi_gdtp
    movzxw  efi_cs, %eax
    pushl   %eax
    movl    $1f, %eax
    pushl   %eax
    lretl
1:  lidt    efi_idtp

    /* Jump to the entry point. */
    movl    16(%esp), %eax
    jmp     *%eax
FUNCTION_END(linux_platform_enter)

.section .data, "aw", @progbits

/** EFI function call address. */
SYMBOL(__efi_call_func)
    .long   0

/** Saved EFI code segment. */
efi_cs:
    .word   0

/** Saved EFI GDT pointer. */
efi_gdtp:
    .word   0
    .long   0

/** Saved EFI IDT pointer. */
efi_idtp:
    .word   0
    .long   0

__dummy:
    .long   0

/**
 * Dummy PE relocation so that the EFI loader recognizes us as relocatable.
 */

.section .reloc

__dummy_reloc:
    .long   __dummy - __dummy_reloc
    .long   10
    .word   0
