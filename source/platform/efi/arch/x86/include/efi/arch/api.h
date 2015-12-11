/*
 * Copyright (C) 2014-2015 Alex Smith
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
 * @brief               x86 EFI API definitions.
 */

#ifndef __EFI_ARCH_API_H
#define __EFI_ARCH_API_H

/**
 * EFI calling convention attribute.
 *
 * On AMD64, EFI uses the Microsoft calling convention. We could use the ms_abi
 * attribute, however support for it in clang does not work right, so for now
 * we use the normal calling convention and convert in a wrapper function. On
 * IA32, EFI uses the System V calling convention, hurrah.
 */
#define __efiapi

#ifdef __LP64__

#define __efi_vcall_(n) __efi_call##n
#define __efi_vcall(n) __efi_vcall_(n)

extern uint64_t __efi_call0(void);
extern uint64_t __efi_call1(uint64_t);
extern uint64_t __efi_call2(uint64_t, uint64_t);
extern uint64_t __efi_call3(uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call4(uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call5(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call6(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call7(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call8(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call9(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t __efi_call10(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#else /* __LP64__ */

/* Only one wrapper independent of the number of arguments. */
#define __efi_vcall(n) __efi_call

extern uint32_t __efi_call(void);

#endif /* __LP64__ */

extern void *__efi_call_func;

/* Mostly borrowed from here:
 * http://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments */
#define __VA_NARG(...) __VA_NARG_I(_0, ## __VA_ARGS__, __RSEQ_N())
#define __VA_NARG_I(...) __VA_NARG_N(__VA_ARGS__)
#define __VA_NARG_N(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define __RSEQ_N() 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/**
 * EFI call wrapper.
 *
 * We wrap EFI calls both to convert between calling conventions (for AMD64) and
 * to restore the firmware's GDT/IDT before calling, and restore ours afterward.
 * This is a horrible bundle of preprocessor abuse that forwards EFI calls to
 * the right wrapper for the number of arguments, while keeping type safety.
 */
#define efi_call(func, ...) \
    __extension__ \
    ({ \
        typeof(func) __wrapper = (typeof(func)) __efi_vcall(__VA_NARG(__VA_ARGS__)); \
        __efi_call_func = (void *)func; \
        __wrapper(__VA_ARGS__); \
    })

#endif /* __EFI_ARCH_API_H */
