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
 * @brief               x86 CPU definitions and functions.
 */

#ifndef __X86_CPU_H
#define __X86_CPU_H

/** Flags in the CR0 Control Register. */
#define X86_CR0_PE              (1<<0)      /**< Protected Mode Enable. */
#define X86_CR0_MP              (1<<1)      /**< Monitor Coprocessor. */
#define X86_CR0_EM              (1<<2)      /**< Emulation. */
#define X86_CR0_TS              (1<<3)      /**< Task Switched. */
#define X86_CR0_ET              (1<<4)      /**< Extension Type. */
#define X86_CR0_NE              (1<<5)      /**< Numeric Error. */
#define X86_CR0_WP              (1<<16)     /**< Write Protect. */
#define X86_CR0_AM              (1<<18)     /**< Alignment Mask. */
#define X86_CR0_NW              (1<<29)     /**< Not Write-through. */
#define X86_CR0_CD              (1<<30)     /**< Cache Disable. */
#define X86_CR0_PG              (1<<31)     /**< Paging Enable. */

/** Flags in the CR4 Control Register. */
#define X86_CR4_VME             (1<<0)      /**< Virtual-8086 Mode Extensions. */
#define X86_CR4_PVI             (1<<1)      /**< Protected Mode Virtual Interrupts. */
#define X86_CR4_TSD             (1<<2)      /**< Time Stamp Disable. */
#define X86_CR4_DE              (1<<3)      /**< Debugging Extensions. */
#define X86_CR4_PSE             (1<<4)      /**< Page Size Extensions. */
#define X86_CR4_PAE             (1<<5)      /**< Physical Address Extension. */
#define X86_CR4_MCE             (1<<6)      /**< Machine Check Enable. */
#define X86_CR4_PGE             (1<<7)      /**< Page Global Enable. */
#define X86_CR4_PCE             (1<<8)      /**< Performance-Monitoring Counter Enable. */
#define X86_CR4_OSFXSR          (1<<9)      /**< OS Support for FXSAVE/FXRSTOR. */
#define X86_CR4_OSXMMEXCPT      (1<<10)     /**< OS Support for Unmasked SIMD FPU Exceptions. */
#define X86_CR4_VMXE            (1<<13)     /**< VMX-Enable Bit. */
#define X86_CR4_SMXE            (1<<14)     /**< SMX-Enable Bit. */

/** Definitions for bits in the EFLAGS/RFLAGS register. */
#define X86_FLAGS_CF            (1<<0)      /**< Carry Flag. */
#define X86_FLAGS_ALWAYS1       (1<<1)      /**< Flag that must always be 1. */
#define X86_FLAGS_PF            (1<<2)      /**< Parity Flag. */
#define X86_FLAGS_AF            (1<<4)      /**< Auxilary Carry Flag. */
#define X86_FLAGS_ZF            (1<<6)      /**< Zero Flag. */
#define X86_FLAGS_SF            (1<<7)      /**< Sign Flag. */
#define X86_FLAGS_TF            (1<<8)      /**< Trap Flag. */
#define X86_FLAGS_IF            (1<<9)      /**< Interrupt Enable Flag. */
#define X86_FLAGS_DF            (1<<10)     /**< Direction Flag. */
#define X86_FLAGS_OF            (1<<11)     /**< Overflow Flag. */
#define X86_FLAGS_NT            (1<<14)     /**< Nested Task Flag. */
#define X86_FLAGS_RF            (1<<16)     /**< Resume Flag. */
#define X86_FLAGS_VM            (1<<17)     /**< Virtual-8086 Mode. */
#define X86_FLAGS_AC            (1<<18)     /**< Alignment Check. */
#define X86_FLAGS_VIF           (1<<19)     /**< Virtual Interrupt Flag. */
#define X86_FLAGS_VIP           (1<<20)     /**< Virtual Interrupt Pending Flag. */
#define X86_FLAGS_ID            (1<<21)     /**< ID Flag. */

/** Model Specific Registers. */
#define X86_MSR_EFER            0xc0000080  /**< Extended Feature Enable register. */
#define X86_MSR_FS_BASE         0xc0000100  /**< FS segment base register. */
#define X86_MSR_GS_BASE         0xc0000101  /**< GS segment base register. */
#define X86_MSR_KERNEL_GS_BASE  0xc0000102  /**< GS base to switch to with SWAPGS. */

/** EFER MSR flags. */
#define X86_EFER_LME            (1<<8)      /**< Long Mode (IA-32e) Enable. */

/** Standard CPUID function definitions. */
#define X86_CPUID_VENDOR_ID     0x0         /**< Vendor ID/Highest Standard Function. */
#define X86_CPUID_FEATURE_INFO  0x1         /**< Feature Information. */
#define X86_CPUID_CACHE_DESC    0x2         /**< Cache Descriptors. */
#define X86_CPUID_SERIAL_NUM    0x3         /**< Processor Serial Number. */
#define X86_CPUID_CACHE_PARMS   0x4         /**< Deterministic Cache Parameters. */
#define X86_CPUID_MONITOR_MWAIT 0x5         /**< MONITOR/MWAIT Parameters. */
#define X86_CPUID_DTS_POWER     0x6         /**< Digital Thermal Sensor and Power Management Parameters. */
#define X86_CPUID_DCA           0x9         /**< Direct Cache Access (DCA) Parameters. */
#define X86_CPUID_PERFMON       0xa         /**< Architectural Performance Monitor Features. */
#define X86_CPUID_X2APIC        0xb         /**< x2APIC Features/Processor Topology. */
#define X86_CPUID_XSAVE         0xd         /**< XSAVE Features. */

/** Extended CPUID function definitions. */
#define X86_CPUID_EXT_MAX       0x80000000  /**< Largest Extended Function. */
#define X86_CPUID_EXT_FEATURE   0x80000001  /**< Extended Feature Bits. */
#define X86_CPUID_BRAND_STRING1 0x80000002  /**< Processor Name/Brand String (Part 1). */
#define X86_CPUID_BRAND_STRING2 0x80000003  /**< Processor Name/Brand String (Part 2). */
#define X86_CPUID_BRAND_STRING3 0x80000004  /**< Processor Name/Brand String (Part 3). */
#define X86_CPUID_L2_CACHE      0x80000006  /**< Extended L2 Cache Features. */
#define X86_CPUID_ADVANCED_PM   0x80000007  /**< Advanced Power Management. */
#define X86_CPUID_ADDRESS_SIZE  0x80000008  /**< Virtual/Physical Address Sizes. */

/** CPUID feature bits. */
#define X86_FEATURE_PSE         (1<<3)      /**< Page Size Extension. */
#define X86_FEATURE_TSC         (1<<4)      /**< Time Stamp Counter. */

/** CPUID extended feature bits. */
#define X86_EXT_FEATURE_LM      (1<<29)     /**< Long mode. */

#ifndef __ASM__

#include <types.h>

/** Macros to generate functions to access registers. */
#define BUILD_READ_REG(name, type) \
    static inline type x86_read_ ## name (void) { \
        type r; \
        __asm__ __volatile__("mov %%" #name ", %0" : "=r"(r)); \
        return r; \
    }
#define BUILD_WRITE_REG(name, type) \
    static inline void x86_write_ ## name (type val) { \
        __asm__ __volatile__("mov %0, %%" #name :: "r"(val)); \
    }

/** Read the CR0 register.
 * @return              Value of the CR0 register. */
BUILD_READ_REG(cr0, unsigned long);

/** Write the CR0 register.
 * @param val           New value of the CR0 register. */
BUILD_WRITE_REG(cr0, unsigned long);

/** Read the CR3 register.
 * @return              Value of the CR3 register. */
BUILD_READ_REG(cr3, unsigned long);

/** Write the CR3 register.
 * @param val           New value of the CR3 register. */
BUILD_WRITE_REG(cr3, unsigned long);

/** Read the CR4 register.
 * @return              Value of the CR4 register. */
BUILD_READ_REG(cr4, unsigned long);

/** Write the CR4 register.
 * @param val           New value of the CR4 register. */
BUILD_WRITE_REG(cr4, unsigned long);

#undef BUILD_READ_REG
#undef BUILD_WRITE_REG

/** Get the current frame pointer (ebp/rbp).
 * @return              Current frame pointer. */
static inline unsigned long x86_read_bp(void) {
    unsigned long r;

    #ifdef __LP64__
        __asm__ __volatile__("mov %%rbp, %0" : "=r"(r));
    #else
        __asm__ __volatile__("mov %%ebp, %0" : "=r"(r));
    #endif

    return r;
}

/** Get current value of EFLAGS/RFLAGS.
 * @return              Current value of EFLAGS/RFLAGS. */
static inline unsigned long x86_read_flags(void) {
    unsigned long val;

    __asm__ __volatile__("pushf; pop %0" : "=rm"(val));
    return val;
}

/** Set value of EFLAGS/RFLAGS.
 * @param val           New value for EFLAGS/RFLAGS. */
static inline void x86_write_flags(unsigned long val) {
    __asm__ __volatile__("push %0; popf" :: "rm"(val));
}

/** Structure containing the result of the CPUID instruction. */
typedef struct x86_cpuid {
    uint32_t eax, ebx, ecx, edx;
} x86_cpuid_t;

/** Execute the CPUID instruction.
 * @param level         CPUID level.
 * @param cpuid         Where to store result of instruction. */
static inline void x86_cpuid(uint32_t level, x86_cpuid_t *cpuid) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(cpuid->eax), "=b"(cpuid->ebx), "=c"(cpuid->ecx), "=d"(cpuid->edx)
        : "0"(level));
}

/** Read the Time Stamp Counter.
 * @return              Value of the TSC. */
static inline uint64_t x86_rdtsc(void) {
    uint32_t high, low;

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

#endif /* __ASM__ */
#endif /* __ARCH_X86_CPU_H */
