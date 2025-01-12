/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               DT platform KBoot loader functions.
 */

#include <lib/string.h>

#include <loader/kboot.h>

#include <assert.h>
#include <loader.h>

/** Perform platform-specific setup for a KBoot kernel.
 * @param loader        Loader internal data. */
void kboot_platform_setup(kboot_loader_t *loader) {
    /* Nothing to do - FDT is handled by generic code. */
}
