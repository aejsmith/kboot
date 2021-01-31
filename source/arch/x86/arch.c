/*
 * Copyright (C) 2009-2021 Alex Smith
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
