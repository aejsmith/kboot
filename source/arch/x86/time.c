/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 timing functions.
 */

#include <arch/io.h>

#include <x86/cpu.h>
#include <x86/time.h>

#include <loader.h>

/** Frequency of the PIT. */
#define PIT_FREQUENCY       1193182ul

/** PIT port definitions. */
#define PIT_MODE                0x43
#define PIT_CHANNEL_0           0x40

/** PIT mode bit definitions. */
#define PIT_MODE_CHANNEL_0      (0 << 6)
#define PIT_MODE_RATE_GENERATOR (2 << 1)
#define PIT_MODE_ACCESS_LATCH   (0 << 4)
#define PIT_MODE_ACCESS_BOTH    (3 << 4)

/** Initial TSC start time. */
static uint64_t tsc_start_time;

/** TSC cycles per millisecond. */
static uint64_t tsc_cycles_per_msec;

/** Get the current internal time.
 * @return              Current internal time. */
mstime_t current_time(void) {
    return (x86_rdtsc() - tsc_start_time) / tsc_cycles_per_msec;
}

/** Initialize the TSC. */
void x86_time_init(void) {
    x86_cpuid_t cpuid;
    uint16_t start_high, start_low, end_high, end_low, ticks;
    uint64_t end_tsc, cycles;

    /* Check for TSC support. TODO: Use the RTC as a (terrible) fallback? */
    x86_cpuid(X86_CPUID_FEATURE_INFO, &cpuid);
    if (!(cpuid.edx & X86_FEATURE_TSC))
        boot_error("CPU does not support TSC");

    /* Calculate the TSC frequncy. First set the PIT to rate generator mode. */
    out8(PIT_MODE, PIT_MODE_CHANNEL_0 | PIT_MODE_RATE_GENERATOR | PIT_MODE_ACCESS_BOTH);
    out8(PIT_CHANNEL_0, 0xff);
    out8(PIT_CHANNEL_0, 0xff);

    /* Wait for the cycle to begin. */
    do {
        out8(PIT_MODE, PIT_MODE_CHANNEL_0 | PIT_MODE_ACCESS_LATCH);
        start_low = in8(PIT_CHANNEL_0);
        start_high = in8(PIT_CHANNEL_0);
    } while (start_high != 0xff);

    /* Get the start TSC value. */
    tsc_start_time = x86_rdtsc();

    /* Wait for the high byte to drop to 128. */
    do {
        out8(PIT_MODE, PIT_MODE_CHANNEL_0 | PIT_MODE_ACCESS_LATCH);
        end_low = in8(PIT_CHANNEL_0);
        end_high = in8(PIT_CHANNEL_0);
    } while (end_high > 0x80);

    /* Get the end TSC value. */
    end_tsc = x86_rdtsc();

    /* Calculate the differences between the values. */
    cycles = end_tsc - tsc_start_time;
    ticks = ((end_high << 8) | end_low) - ((start_high << 8) | start_low);

    /* Calculate frequency. */
    tsc_cycles_per_msec = (cycles * PIT_FREQUENCY) / (ticks * 1000);
}
