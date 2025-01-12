/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 architecture main functions.
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>

#include <loader.h>

extern void arm64_do_switch_to_el1(void);

/** Exception Level the loader is running in (EL1 or EL2). */
int arm64_loader_el;

/** Architecture-specific initialization. */
void arch_init(void) {
    unsigned long currentel = arm64_read_sysreg(currentel);
    arm64_loader_el = (currentel >> 2) & 3;

    unsigned long sctlr = arm64_read_sysreg_el(sctlr);

    dprintf("arch: booted in EL%d, SCTLR = 0x%lx\n", arm64_loader_el, sctlr);

    arm64_exception_init();
}

/** Switch to EL1 if we're in EL2. */
void arm64_switch_to_el1(void) {
    if (arm64_is_el2()) {
        arm64_do_switch_to_el1();

        arm64_loader_el = 1;

        /* Re-enable exception handling in the new EL. */
        arm64_exception_init();
    }
}

/** Halt the system. */
__noreturn void target_halt(void) {
    __asm__ __volatile__("msr daifset, #2");

    while (true)
        __asm__ __volatile("wfi");
}

/** Halt the system. */
__noreturn void target_reboot(void) {
    /* TODO */
    internal_error("TODO: Reboot");
}
