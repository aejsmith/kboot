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
    if (cmd)
        config_printf("%s: ", cmd);

    console_vprintf(config_console, fmt, args);
    console_putc(config_console, '\n');
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
            console_set_colour(config_console, COLOUR_WHITE, CONSOLE_COLOUR_BG);
            config_printf("> ");
            console_set_colour(config_console, CONSOLE_COLOUR_FG, CONSOLE_COLOUR_BG);
        }
    }

    line_editor_init(&shell_line_editor, config_console, NULL);

    /* Accumulate another line. */
    while (true) {
        uint16_t key = console_getc(config_console);

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
void shell_main(void) {
    config_error_handler_t prev_handler;

    assert(shell_enabled);

    if (main_console.out && main_console.in) {
        config_console = &main_console;
    } else if (debug_console.out && debug_console.in) {
        config_console = &debug_console;
    } else {
        return;
    }

    current_environ = root_environ;

    prev_handler = config_set_error_handler(shell_error_handler);

    while (true) {
        command_list_t *list;

        console_set_colour(config_console, COLOUR_WHITE, CONSOLE_COLOUR_BG);
        config_printf("KBoot> ");
        console_set_colour(config_console, CONSOLE_COLOUR_FG, CONSOLE_COLOUR_BG);

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

    config_set_error_handler(prev_handler);
}
