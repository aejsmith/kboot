/*
 * Copyright (C) 2015 Alex Smith
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
 * @brief               Loader main function.
 */

#include <assert.h>
#include <config.h>
#include <device.h>
#include <loader.h>
#include <memory.h>
#include <shell.h>

/** Maximum number of pre-boot hooks. */
#define PREBOOT_HOOKS_MAX       8

/** Array of pre-boot hooks. */
static preboot_hook_t preboot_hooks[PREBOOT_HOOKS_MAX];
static size_t preboot_hooks_count = 0;

/** Add a pre-boot hook.
 * @param hook          Hook to add. */
void loader_register_preboot_hook(preboot_hook_t hook) {
    assert(preboot_hooks_count < PREBOOT_HOOKS_MAX);
    preboot_hooks[preboot_hooks_count++] = hook;
}

/** Perform pre-boot tasks. */
void loader_preboot(void) {
    for (size_t i = 0; i < preboot_hooks_count; i++)
        preboot_hooks[i]();
}

/** Main function of the loader. */
void loader_main(void) {
    dprintf("loader: version is %s\n", kboot_loader_version);

    config_init();
    memory_init();
    device_init();

    /* Load the configuration file. */
    config_load();
}
