/*
 * Copyright (C) 2010-2015 Alex Smith
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
 * @brief               Console functions.
 */

#include <lib/printf.h>
#include <lib/string.h>

#include <config.h>
#include <console.h>
#include <loader.h>
#include <ui.h>

/** Debug log size. */
#define DEBUG_LOG_SIZE      8192

/** Debug output log. */
static char debug_log[DEBUG_LOG_SIZE];
static size_t debug_log_start;
static size_t debug_log_length;

/** List of available consoles. */
static LIST_DECLARE(console_list);

/** Primary console, driven by the video code. */
console_t primary_console = { .name = "con" };

/** Current main console. */
console_t *current_console = &primary_console;

/** Debug output console. */
console_t *debug_console;

/** Helper for console_vprintf().
 * @param ch            Character to display.
 * @param data          Console to use.
 * @param total         Pointer to total character count. */
void console_vprintf_helper(char ch, void *data, int *total) {
    console_t *console = data;

    console_putc(console, ch);
    *total = *total + 1;
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_vprintf(console_t *console, const char *fmt, va_list args) {
    return do_vprintf(console_vprintf_helper, console, fmt, args);
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_printf(console_t *console, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = console_vprintf(console, fmt, args);
    va_end(args);

    return ret;
}

/** Output a formatted message to the current console.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int vprintf(const char *fmt, va_list args) {
    return do_vprintf(console_vprintf_helper, current_console, fmt, args);
}

/** Output a formatted message to the current console.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int printf(const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);

    return ret;
}

#ifdef CONFIG_TARGET_HAS_UI

/** Display the debug log. */
void debug_log_display(void) {
    ui_window_t *textview = ui_textview_create(
        "Debug Log", debug_log, DEBUG_LOG_SIZE, debug_log_start,
        debug_log_length);

    ui_display(textview, 0);
    ui_window_destroy(textview);
}

#endif /* CONFIG_TARGET_HAS_UI */

/** Helper for dvprintf().
 * @param ch            Character to display.
 * @param data          Unused.
 * @param total         Pointer to total character count. */
void dvprintf_helper(char ch, void *data, int *total) {
    console_putc(debug_console, ch);

    /* Store in the log buffer. */
    debug_log[(debug_log_start + debug_log_length) % DEBUG_LOG_SIZE] = ch;
    if (debug_log_length < DEBUG_LOG_SIZE) {
        debug_log_length++;
    } else {
        debug_log_start = (debug_log_start + 1) % DEBUG_LOG_SIZE;
    }

    *total = *total + 1;
}

/** Output a formatted message to the debug console.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int dvprintf(const char *fmt, va_list args) {
    return do_vprintf(dvprintf_helper, NULL, fmt, args);
}

/** Output a formatted message to the debug console.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int dprintf(const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = dvprintf(fmt, args);
    va_end(args);

    return ret;
}

/** Check if a console has capabilities.
 * @param console       Console to check (if NULL false will be returned).
 * @param caps          Capabilities to check for.
 * @return              Whether the console has the capabilities. */
bool console_has_caps(console_t *console, unsigned caps) {
    unsigned has = 0;

    if (!console)
        return false;

    if (console->out) {
        has |= CONSOLE_CAP_OUT;
        if (console->out->ops->set_region)
            has |= CONSOLE_CAP_UI;
    }

    if (console->in)
        has |= CONSOLE_CAP_IN;

    return (has & caps) == caps;
}

/** Look up a console by name.
 * @param name          Name of the console to look up.
 * @return              Pointer to console if found, NULL if not. */
console_t *console_lookup(const char *name) {
    list_foreach(&console_list, iter) {
        console_t *console = list_entry(iter, console_t, header);

        if (strcmp(console->name, name) == 0)
            return console;
    }

    return NULL;
}

/** Register a console.
 * @param console       Console to register (details filled in). */
void console_register(console_t *console) {
    if (console_lookup(console->name))
        internal_error("Console named '%s' already exists", console->name);

    list_init(&console->header);
    list_append(&console_list, &console->header);
}

/** Make a console active.
 * @param var           Pointer to console variable to set.
 * @param console       Console to set. */
static void set_console(console_t **var, console_t *console) {
    console_t *prev = *var;

    if (console != prev) {
        if (prev) {
            if (prev->out && prev->out->ops->deinit)
                prev->out->ops->deinit(prev->out);
            if (prev->in && prev->in->ops->deinit)
                prev->in->ops->deinit(prev->in);
        }

        *var = console;

        if (console) {
            if (console->out && console->out->ops->init)
                console->out->ops->init(console->out);
            if (console->in && console->in->ops->init)
                console->in->ops->init(console->in);
        }
    }
}

/** Set a console as the current console.
 * @param console       Console to set as current. */
void console_set_current(console_t *console) {
    set_console(&current_console, console);
}

/** Set a console as the debug console.
 * @param console       Console to set as debug. */
void console_set_debug(console_t *console) {
    set_console(&debug_console, console);
}

/** Initialize the console. */
void console_init(void) {
    console_register(&primary_console);
    target_console_init();
}

/**
 * Configuration commands.
 */

/** List available consoles.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_lsconsole(value_list_t *args) {
    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    list_foreach(&console_list, iter) {
        console_t *console = list_entry(iter, console_t, header);

        printf("%s", console->name);

        if (console == current_console)
            printf(" (current)");
        if (console == debug_console)
            printf(" (debug)");

        printf("\n");
    }

    return true;
}

BUILTIN_COMMAND("lsconsole", "List available consoles", config_cmd_lsconsole);

/** Set the current console.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_console(value_list_t *args) {
    console_t *console;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    console = console_lookup(args->values[0].string);
    if (!console) {
        config_error("Console '%s' not found", args->values[0].string);
        return false;
    }

    console_set_current(console);
    return true;
}

BUILTIN_COMMAND("console", "Set the current console", config_cmd_console);
