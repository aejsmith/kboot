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
 * @brief               AMD64 EFI startup code.
 */

#include <arch/page.h>

#include <x86/asm.h>
#include <x86/descriptor.h>

.section .text, "ax", @progbits

/** EFI entry point.
 * @param %rcx          Handle to the loader image.
 * @param %rdx          Pointer to EFI system table. */
FUNCTION_START(loader_entry)
    /* EFI uses the Microsoft x86_64 ABI. Arguments are passed in RCX/RDX. */
    push    %rcx
    push    %rdx

    /* Relocate the loader. Preserve RDI/RSI to return to caller if needed. */
    push    %rdi
    push    %rsi
    subq    $8, %rsp
    leaq    __start(%rip), %rdi
    leaq    _DYNAMIC(%rip), %rsi
    call    efi_arch_relocate
    testq   %rax, %rax
    jz      1f
    addq    $8, %rsp
    pop     %rsi
    pop     %rdi
    ret
1:  addq    $24, %rsp

    /* Clear RFLAGS. This will disable interrupts. */
    push    $0
    popf

    /* Zero the BSS section. */
    leaq    __bss_start(%rip), %rdi
    leaq    __bss_end(%rip), %rcx
    subq    %rdi, %rcx
    xorb    %al, %al
    rep stosb

    /* Save the EFI GDT and IDT pointers which we must restore before calling
     * any EFI functions. */
    leaq    efi_gdtp(%rip), %rax
    sgdt    (%rax)
    leaq    efi_idtp(%rip), %rax
    sidt    (%rax)
    leaq    efi_cs(%rip), %rax
    movw    %cs, (%rax)

    /* Load the GDT. */
    leaq    loader_gdtp(%rip), %rax
    lgdt    (%rax)
    push    $SEGMENT_CS
    leaq    2f(%rip), %rax
    push    %rax
    lretq
2:
    /* Clear the stack frame pointer. */
    xorq    %rbp, %rbp

    /* Call the main function. */
    popq    %rsi
    popq    %rdi
    jmp     efi_main
FUNCTION_END(loader_entry)

/*
 * EFI call wrappers. These convert between the SysV and MS calling conventions.
 * MS calling convention summary:
 *  - Parameters use RCX, RDX, R8 and R9, then the stack.
 *  - Must always reserve stack space for the 4 register arguments (32 bytes),
 *    this space is owned by callee.
 *  - RSP + 8 (after the return address) is 16-byte aligned. The same applies
 *    for SysV.
 */

FUNCTION_START(__efi_call0)
    push    %rbp
    movq    %rsp, %rbp

    subq    $32, %rsp
    jmp     __efi_call
FUNCTION_END(__efi_call0)

FUNCTION_START(__efi_call1)
    push    %rbp
    movq    %rsp, %rbp

    subq    $32, %rsp
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call1)

FUNCTION_START(__efi_call2)
    push    %rbp
    movq    %rsp, %rbp

    subq    $32, %rsp
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call2)

FUNCTION_START(__efi_call3)
    push    %rbp
    movq    %rsp, %rbp

    subq    $32, %rsp
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call3)

FUNCTION_START(__efi_call4)
    push    %rbp
    movq    %rsp, %rbp

    subq    $32, %rsp
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call4)

FUNCTION_START(__efi_call5)
    push    %rbp
    movq    %rsp, %rbp

    subq    $48, %rsp
    movq    %r8, 32(%rsp)
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call5)

FUNCTION_START(__efi_call6)
    push    %rbp
    movq    %rsp, %rbp

    subq    $48, %rsp
    movq    %r9, 40(%rsp)
    movq    %r8, 32(%rsp)
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call6)

FUNCTION_START(__efi_call7)
    push    %rbp
    movq    %rsp, %rbp

    subq    $64, %rsp
    movq    80(%rsp), %rax
    movq    %rax, 48(%rsp)
    movq    %r9, 40(%rsp)
    movq    %r8, 32(%rsp)
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call7)

FUNCTION_START(__efi_call8)
    push    %rbp
    movq    %rsp, %rbp

    subq    $64, %rsp
    movq    88(%rsp), %rax
    movq    %rax, 56(%rsp)
    movq    80(%rsp), %rax
    movq    %rax, 48(%rsp)
    movq    %r9, 40(%rsp)
    movq    %r8, 32(%rsp)
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call8)

FUNCTION_START(__efi_call9)
    push    %rbp
    movq    %rsp, %rbp

    subq    $80, %rsp
    movq    112(%rsp), %rax
    movq    %rax, 64(%rsp)
    movq    104(%rsp), %rax
    movq    %rax, 56(%rsp)
    movq    96(%rsp), %rax
    movq    %rax, 48(%rsp)
    movq    %r9, 40(%rsp)
    movq    %r8, 32(%rsp)
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call9)

FUNCTION_START(__efi_call10)
    push    %rbp
    movq    %rsp, %rbp

    subq    $80, %rsp
    movq    120(%rsp), %rax
    movq    %rax, 72(%rsp)
    movq    112(%rsp), %rax
    movq    %rax, 64(%rsp)
    movq    104(%rsp), %rax
    movq    %rax, 56(%rsp)
    movq    96(%rsp), %rax
    movq    %rax, 48(%rsp)
    movq    %r9, 40(%rsp)
    movq    %r8, 32(%rsp)
    movq    %rcx, %r9
    movq    %rdx, %r8
    movq    %rsi, %rdx
    movq    %rdi, %rcx
    jmp     __efi_call
FUNCTION_END(__efi_call10)

/** EFI call wrapper. */
PRIVATE_FUNCTION_START(__efi_call)
    /* Switch to the EFI GDT/IDT. */
    leaq    efi_gdtp(%rip), %rax
    lgdt    (%rax)
    leaq    efi_cs(%rip), %rax
    movzxw  (%rax), %rax
    push    %rax
    leaq    1f(%rip), %rax
    push    %rax
    lretq
1:  leaq    efi_idtp(%rip), %rax
    lidt    (%rax)

    /* Get the function address to call. */
    leaq    __efi_call_func(%rip), %rax
    movq    (%rax), %rax

    /* Enable interrupts. It's possible there's some EFI implementations that
     * break if you call them with interrupts off... */
    sti

    /* Call the function. */
    callq   *%rax

    /* Disable interrupts again. */
    cli

    /* Load our GDT/IDT. */
    leaq    loader_gdtp(%rip), %rdx
    lgdt    (%rdx)
    push    $SEGMENT_CS
    leaq    2f(%rip), %rdx
    push    %rdx
    lretq
2:  leaq    loader_idtp(%rip), %rdx
    lidt    (%rdx)

    /* Restore old stack pointer and return. */
    movq    %rbp, %rsp
    pop     %rbp
    ret
FUNCTION_END(__efi_call)

/** Enter a Linux kernel using the handover entry point.
 * @param handle        Handle to the loader image.
 * @param table         Pointer to EFI system table.
 * @param params        Kernel parameters structure.
 * @param entry         Entry point address. */
FUNCTION_START(linux_platform_enter)
    /* Switch to the EFI GDT/IDT. */
    leaq    efi_gdtp(%rip), %rax
    lgdt    (%rax)
    leaq    efi_cs(%rip), %rax
    movzxw  (%rax), %rax
    push    %rax
    leaq    1f(%rip), %rax
    push    %rax
    lretq
1:  leaq    efi_idtp(%rip), %rax
    lidt    (%rax)

    /* Jump to the entry point. */
    call    *%rcx
FUNCTION_END(linux_platform_enter)

.section .data, "aw", @progbits

/** EFI function call address. */
SYMBOL(__efi_call_func)
    .quad   0

/** Saved EFI code segment. */
efi_cs:
    .word   0

/** Saved EFI GDT pointer. */
efi_gdtp:
    .word   0
    .quad   0

/** Saved EFI IDT pointer. */
efi_idtp:
    .word   0
    .quad   0

__dummy:
    .long   0

/**
 * Dummy PE relocation so that the EFI loader recognizes us as relocatable.
 */

.section .reloc, "aw", @progbits

__dummy_reloc:
    .long   __dummy - __dummy_reloc
    .long   10
    .word   0
