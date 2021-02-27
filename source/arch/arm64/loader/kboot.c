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
 * @brief               ARM64 KBoot kernel loader.
 */

#include <arch/page.h>

#include <lib/string.h>

#include <loader/kboot.h>

#include <loader.h>

/** Check whether a kernel image is supported.
 * @param loader        Loader internal data. */
void kboot_arch_check_kernel(kboot_loader_t *loader) {
    internal_error("TODO: kboot_arch_check_kernel");
}

/** Validate kernel load parameters.
 * @param loader        Loader internal data.
 * @param load          Load image tag. */
void kboot_arch_check_load_params(kboot_loader_t *loader, kboot_itag_load_t *load) {
    internal_error("TODO: kboot_arch_check_load_params");
}

/** Perform architecture-specific setup tasks.
 * @param loader        Loader internal data. */
void kboot_arch_setup(kboot_loader_t *loader) {
    internal_error("TODO: kboot_arch_setup");
}

/** Enter the kernel.
 * @param loader        Loader internal data. */
__noreturn void kboot_arch_enter(kboot_loader_t *loader) {
    internal_error("TODO: kboot_arch_enter");
}