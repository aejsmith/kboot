/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief               BIOS loader startup code.
 */

#include <arch/page.h>

#include <x86/asm.h>
#include <x86/descriptor.h>
#include <x86/multiboot.h>

#include <platform/loader.h>

/* Ensure that this code is linked in first. */
.section .init.text, "ax", @progbits

/** Real-mode startup function.
 * @param %dl           Boot device ID.
 * @param %ebx          Boot partition LBA (lower 32 bits).
 * @param %ecx          Boot partition LBA (upper 32 bits).
 * @param %si           Pointer (16-bit linear) to boot loader binary path. */
FUNCTION_START(loader_entry)
.code16
    /* Don't want interrupts. */
    cli

    /* Set up data segments. */
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    $LOADER_LOAD_SEGMENT, %ax
    movw    %ax, %es

    /* Save the boot device/partition offset LBA. */
    addr32 movb %dl, bios_boot_device
    addr32 movl %ebx, bios_boot_partition
    addr32 movl %ecx, bios_boot_partition + 4

    /* Copy the boot path into our buffer so we don't overwrite it. */
    testw   %si, %si
    jz      2f
    movl    $(bios_boot_path - LOADER_LOAD_ADDR), %edi
1:  lodsb
    stosb
    testb   %al, %al
    jz      2f
    jmp     1b
2:
    /* Make sure we're displaying page 0, and move the cursor to (0,0) */
    movb    $0x02, %ah
    xorb    %bh, %bh
    xorw    %dx, %dx
    int     $0x10

    /* Enable the A20 line. */
    call    enable_a20

    /* Now we need to switch to protected mode. */
    addr32 lgdt loader_gdtp
    movl    %cr0, %eax
    orl     $(1<<0), %eax
    movl    %eax, %cr0
    data32 ljmp $SEGMENT_CS, $3f
.code32
3:
    mov     $SEGMENT_DS, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Jump to the common startup code. */
    jmp     common_entry
FUNCTION_END(loader_entry)

/** Test whether the A20 line is enabled.
 * @note                Partially based on code found at http://wiki.osdev.org/A20_Line
 * @return              AX = 1 if enabled, AX = 0 if not. */
FUNCTION_START(test_a20)
.code16
    /* Iterate checking to give time for enabling attempts we make to take
     * effect. */
    push    %cx
    mov     $0x1000, %cx

    /* Set segment registers and addresses to compare. */
1:  xor     %ax, %ax
    mov     %ax, %fs
    not     %ax
    mov     %ax, %gs
    mov     $0x1000, %di
    mov     $0x1010, %si

    /* Write different values to the two addresses, and compare. If they
     * both equal the second value written, the A20 line is not enabled. */
    movb    $0x00, %fs:(%di)
    outb    %al, $0x80
    movb    $0xff, %gs:(%si)
    outb    %al, $0x80
    cmpb    $0xff, %fs:(%di)
    jne     2f
    loop    1b
    xor     %ax, %ax
    pop     %cx
    ret
2:  mov     $1, %ax
    pop     %cx
    ret
FUNCTION_END(test_a20)

/** Enable the A20 line. */
PRIVATE_FUNCTION_START(enable_a20)
    mov     $0x100, %cx
.Lbegin:
    /* Don't do anything if already enabled. */
    call    test_a20
    cmp     $0, %ax
    je      .Lenable_bios
    ret
.Lenable_bios:
    /* First try using a BIOS interrupt. */
    mov     $0x2401, %ax
    int     $0x15
    call    test_a20
    cmp     $0, %ax
    je      .Lenable_kbd
    ret
.Lenable_kbd:
    /* Try the keyboard controller. */
    call    .Lflush
    mov     $0xd1, %al
    outb    %al, $0x64
    call    .Lflush
    mov     $0xdf, %al
    outb    %al, $0x60
    call    .Lflush
    mov     $0xff, %al
    outb    %al, $0x64
    call    .Lflush
    call    test_a20
    cmp     $0, %ax
    je      .Lenable_fast
    ret
.Lenable_fast:
    /* Try the fast A20 mechanism. */
    inb     $0x92, %al
    testb   $(1<<1), %al
    jnz     .Lfail
    orb     $(1<<1), %al
    andb    $~(1<<0), %al
    outb    %al, $0x92
    call    test_a20
    cmp     $0, %ax
    je      .Lfail
    ret
.Lfail:
    loop    .Lbegin
    mov     $'a', %al
    mov     $0x3f8, %dx
    outb    %al, (%dx)
1:  jmp     1b
.Lflush:
    outb    %al, $0x80
    in      $0x64, %al
    test    $1, %al
    jz      2f
    outb    %al, $0x80
    in      $0x60, %al
    jmp     .Lflush
2:  test    $2, %al
    jnz     .Lflush
    ret
FUNCTION_END(enable_a20)

/** Flags for the Multiboot header. */
#define MULTIBOOT_HEADER_FLAGS      (MULTIBOOT_AOUT_KLUDGE | MULTIBOOT_PAGE_ALIGN)

/** Multiboot header structure. */
.align 4
multiboot_header:
    .long   MULTIBOOT_HEADER_MAGIC
    .long   MULTIBOOT_HEADER_FLAGS
    .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    .long   multiboot_header + MULTIBOOT_LOAD_OFFSET
    .long   __start + MULTIBOOT_LOAD_OFFSET
    .long   __data_end + MULTIBOOT_LOAD_OFFSET
    .long   __end + MULTIBOOT_LOAD_OFFSET
    .long   multiboot_entry + MULTIBOOT_LOAD_OFFSET

/** Multiboot startup function. */
PRIVATE_FUNCTION_START(multiboot_entry)
.code32
    cli

    /* Duplicate the Multiboot information that we want. It is possible that the
     * Multiboot loader has placed the information in the same place that we are
     * going to relocate to, so it must be duplicated to ensure that it won't be
     * overwritten. Everything gets copied into space allocated within the
     * loader image so we can guarantee we're not overwriting anything. */
    movl    %eax, (multiboot_magic + MULTIBOOT_LOAD_OFFSET)
    cld
    movl    $MULTIBOOT_INFO_SIZE, %ecx
    movl    $(multiboot_info + MULTIBOOT_LOAD_OFFSET), %edi
    movl    %ebx, %esi
    movl    %edi, %ebx
    rep movsb

    /* Copy everything else into the preallocated chunk of space and update the
     * addresses in the information structures. */
    movl    $(multiboot_storage + MULTIBOOT_LOAD_OFFSET), %edi

    /* Copy the command line string. */
    movl    MULTIBOOT_INFO_OFF_CMDLINE(%ebx), %esi
    leal    -MULTIBOOT_LOAD_OFFSET(%edi), %eax
    movl    %eax, MULTIBOOT_INFO_OFF_CMDLINE(%ebx)
1:  lodsb
    stosb
    test    %al, %al
    jnz     1b

    /* Copy the module information. */
    movl    MULTIBOOT_INFO_OFF_MODS_COUNT(%ebx), %ecx
    test    %ecx, %ecx
    jz      .Lrelocate
    shll    $4, %ecx
    movl    MULTIBOOT_INFO_OFF_MODS_ADDR(%ebx), %esi
    leal    -MULTIBOOT_LOAD_OFFSET(%edi), %eax
    movl    %eax, MULTIBOOT_INFO_OFF_MODS_ADDR(%ebx)
    movl    %edi, %edx
    rep movsb

    /* Iterate through the modules and copy the command lines. Assuming for now
     * that the modules are loaded in a sane location, i.e. above 1MB somewhere,
     * so we won't overwrite them. */
    movl    MULTIBOOT_INFO_OFF_MODS_COUNT(%ebx), %ecx
.Lloop:
    movl    MULTIBOOT_MODULE_OFF_CMDLINE(%edx), %esi
    test    %esi, %esi
    jz      2f
    leal    -MULTIBOOT_LOAD_OFFSET(%edi), %eax
    movl    %eax, MULTIBOOT_MODULE_OFF_CMDLINE(%edx)
1:  lodsb
    stosb
    test    %al, %al
    jnz     1b
2:  addl    $MULTIBOOT_MODULE_INFO_SIZE, %edx
    loop    .Lloop

.Lrelocate:
    /* Relocate the loader image. */
    cld
    mov     $(__end - LOADER_LOAD_ADDR), %ecx
    mov     $LOADER_LOAD_ADDR, %edi
    mov     $MULTIBOOT_LOAD_ADDR, %esi
    rep movsb

    /* Load the GDT. */
    lgdt    (loader_gdtp)
    ljmp    $SEGMENT_CS, $1f
1:
    mov     $SEGMENT_DS, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Jump to the common startup code. */
    jmp     common_entry
FUNCTION_END(multiboot_entry)

/** Common startup function. */
PRIVATE_FUNCTION_START(common_entry)
    /* Set the stack pointer to the physical location of our stack and clear
     * the stack frame/EFLAGS. */
    movl    $loader_stack + PAGE_SIZE, %esp
    xorl    %ebp, %ebp
    push    $0
    popf

    /* Zero the BSS section. */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorb    %al, %al
    rep stosb

    /* Call the main function. */
    jmp     bios_main
FUNCTION_END(common_entry)

.section .init.data, "aw", @progbits

/** Boot drive. */
SYMBOL(bios_boot_device)
    .byte   0

/** Boot partition offset LBA. */
SYMBOL(bios_boot_partition)
    .quad   0

/** Boot loader path. */
SYMBOL(bios_boot_path)
    .fill   32

.section .data, "aw", @progbits

/** Multiboot magic number. */
SYMBOL(multiboot_magic)
    .long   0

/** Multiboot information structure. */
SYMBOL(multiboot_info)
    .fill   MULTIBOOT_INFO_SIZE

/** Multiboot information storage. */
multiboot_storage:
    .fill   PAGE_SIZE

.section .bss, "aw", @nobits

/** Stack. */
SYMBOL_ALIGNED(loader_stack, PAGE_SIZE)
    .fill   PAGE_SIZE
