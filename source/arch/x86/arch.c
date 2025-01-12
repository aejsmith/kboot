/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 architecture main functions.
 */

#include <x86/cpu.h>
#include <x86/descriptor.h>
#include <x86/time.h>

#include <loader.h>

/** Perform early architecture initialization. */
void arch_init(void) {
    unsigned long flags;

    /* Check if CPUID is supported - if we can change EFLAGS.ID, it is. */
    flags = x86_read_flags();
    x86_write_flags(flags ^ X86_FLAGS_ID);
    if ((x86_read_flags() & X86_FLAGS_ID) == (flags & X86_FLAGS_ID))
        boot_error("CPU does not support CPUID");

    x86_descriptor_init();
    x86_time_init();
}

/** Halt the system. */
__noreturn void target_halt(void) {
    while (true) {
        __asm__ __volatile__(
            "cli\n"
            "hlt\n");
    }
}
