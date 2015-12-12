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
 * @brief               Shell interface.
 */

#include <lib/ctype.h>
#include <lib/line_editor.h>
#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <console.h>
#include <loader.h>
#include <memory.h>
#include <shell.h>

/** Current line being read out by the config parser. */
static char *shell_line;
static size_t shell_line_offset;
static size_t shell_line_len;

/** Line editing state. */
static line_editor_t shell_line_editor;

/** Whether the shell is currently enabled. */
bool shell_enabled;

/** Handler for configuration errors in the shell.
 * @param cmd           Name of the command that caused the error.
 * @param fmt           Error format string.
 * @param args          Arguments to substitute into format. */
static void shell_error_handler(const char *cmd, const char *fmt, va_list args) {
    vprintf(fmt, args);
    console_putc(current_console, '\n');
}

/** Input helper for the shell.
 * @param nest          Nesting count (unused).
 * @return              Character read, or EOF on end of file. */
static int shell_input_helper(unsigned nest) {
    if (shell_line_offset) {
        if (shell_line_offset < shell_line_len)
            return shell_line[shell_line_offset++];

        free(shell_line);
        shell_line = NULL;
        shell_line_offset = shell_line_len = 0;

        if (!nest) {
            /* Not expecting more input, return end. */
            return EOF;
        } else {
            /* Expecting more input, get another line. */
            console_set_colour(current_console, COLOUR_WHITE, COLOUR_DEFAULT);
            printf("> ");
            console_set_colour(current_console, COLOUR_DEFAULT, COLOUR_DEFAULT);
        }
    }

    line_editor_init(&shell_line_editor, current_console, NULL);

    /* Accumulate another line. */
    while (true) {
        uint16_t key = console_getc(current_console);

        /* Parser requires a new line at the end of the buffer to work properly,
         * so always add here. */
        line_editor_input(&shell_line_editor, key);

        if (key == '\n') {
            assert(!shell_line);
            shell_line_len = shell_line_editor.len;
            shell_line = line_editor_finish(&shell_line_editor);
            return shell_line[shell_line_offset++];
        }
    }
}

/** Main function of the shell. */
__noreturn void shell_main(void) {
    assert(shell_enabled);

    if (!console_has_caps(current_console, CONSOLE_CAP_OUT | CONSOLE_CAP_IN))
        target_reboot();

    current_environ = environ_create(root_environ);
    config_set_error_handler(shell_error_handler);

    while (true) {
        command_list_t *list;

        console_set_cursor_visible(current_console, true);

        console_set_colour(current_console, COLOUR_WHITE, COLOUR_DEFAULT);
        printf("KBoot> ");
        console_set_colour(current_console, COLOUR_DEFAULT, COLOUR_DEFAULT);

        shell_line = NULL;
        shell_line_offset = shell_line_len = 0;

        list = config_parse("<shell>", shell_input_helper);
        if (list) {
            command_list_exec(list, current_environ);
            command_list_destroy(list);

            /* If we now have a loader, load it. */
            if (current_environ->loader)
                environ_boot(current_environ);
        }
    }
}
