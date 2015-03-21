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

#include <lib/ctype.h>

#include <config.h>
#include <device.h>
#include <loader.h>
#include <memory.h>
#include <menu.h>
#include <shell.h>

/** Main function of the loader. */
void loader_main(void) {
    environ_t *env;

    config_init();
    memory_init();
    device_init();

    /* Load the configuration file. */
    config_load();

    /* Display the menu. */
    env = menu_display();

    /* And finally boot the OS. */
    if (env->loader) {
        environ_boot(env);
    } else {
        boot_error("No operating system to boot");
    }
}
