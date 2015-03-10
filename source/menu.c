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
 * @brief               Menu interface.
 */

#include <lib/list.h>
#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <loader.h>
#include <memory.h>
#include <menu.h>
#include <shell.h>
#include <time.h>
#include <ui.h>

/** Structure containing a menu entry. */
typedef struct menu_entry {
    ui_entry_t entry;                   /**< UI entry header. */

    list_t header;                      /**< Link to menu entries list. */
    char *name;                         /**< Name of the entry. */
    environ_t *env;                     /**< Environment for the entry. */
    char *error;                        /**< If an error occurred, the error string. */
} menu_entry_t;

/** List of menu entries. */
static LIST_DECLARE(menu_entries);

/** Currently executing menu entry. */
static menu_entry_t *executing_menu_entry;

/** Selected menu entry. */
static menu_entry_t *selected_menu_entry;

/** Render a menu entry.
 * @param _entry        Entry to render. */
static void menu_entry_render(ui_entry_t *_entry) {
    menu_entry_t *entry = (menu_entry_t *)_entry;

    ui_printf("%s", entry->name);
}

/** Write the help text for a menu entry.
 * @param _entry        Entry to write for. */
static void menu_entry_help(ui_entry_t *_entry) {
    menu_entry_t *entry = (menu_entry_t *)_entry;

    ui_print_action('\n', "Select");

    if (!entry->error && entry->env->loader->configure)
        ui_print_action(CONSOLE_KEY_F1, "Configure");

    ui_print_action(CONSOLE_KEY_F2, "Shell");
}

/** Handle input on a menu entry.
 * @param _entry        Entry input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t menu_entry_input(ui_entry_t *_entry, uint16_t key) {
    menu_entry_t *entry = (menu_entry_t *)_entry;

    switch (key) {
    case '\n':
        selected_menu_entry = entry;
        return INPUT_CLOSE;
    case CONSOLE_KEY_F1:
        if (!entry->error && entry->env->loader->configure) {
            size_t len;
            char *title __cleanup_free;
            ui_window_t *window;
            environ_t *prev;

            /* Determine the window title. */
            len = strlen(entry->name) + 13;
            title = malloc(len);
            snprintf(title, len, "Configure '%s'", entry->name);

            prev = current_environ;
            current_environ = entry->env;
            window = entry->env->loader->configure(entry->env->loader_private, title);
            current_environ = prev;

            ui_display(window, ui_console, 0);
            ui_window_destroy(window);
            return INPUT_RENDER_WINDOW;
        } else {
            return INPUT_HANDLED;
        }
    case CONSOLE_KEY_F2:
        /* This is taken to mean the shell should be entered. */
        selected_menu_entry = NULL;
        return INPUT_CLOSE;
    default:
        return INPUT_HANDLED;
    }
}

/** Menu entry type. */
static ui_entry_type_t menu_entry_type = {
    .render = menu_entry_render,
    .help = menu_entry_help,
    .input = menu_entry_input,
};

/** Get the default menu entry.
 * @return              Default menu entry. */
static menu_entry_t *get_default_entry(void) {
    const value_t *value = environ_lookup(root_environ, "default");

    if (value) {
        size_t i = 0;

        list_foreach(&menu_entries, iter) {
            menu_entry_t *entry = list_entry(iter, menu_entry_t, header);

            if (value->type == VALUE_TYPE_INTEGER) {
                if (i == value->integer)
                    return entry;
            } else if (value->type == VALUE_TYPE_STRING) {
                if (strcmp(entry->name, value->string) == 0)
                    return entry;
            }

            i++;
        }
    }

    /* No default entry found, return the first list entry. */
    return list_first(&menu_entries, menu_entry_t, header);
}

/** Display the menu interface.
 * @return              Environment to boot. */
environ_t *menu_display(void) {
    ui_window_t *window;
    const value_t *value;
    bool display;
    unsigned timeout;

    /* Assume if no entries are declared the root environment is bootable. */
    if (list_empty(&menu_entries))
        return root_environ;

    /* Determine the default entry. */
    selected_menu_entry = get_default_entry();

    /* Check if the menu was requested to be hidden. */
    value = environ_lookup(root_environ, "hidden");
    if (value && value->type == VALUE_TYPE_BOOLEAN && value->boolean) {
        display = false;
        timeout = 0;

        /* Wait half a second for Esc to be pressed. */
        delay(500);
        while (console_poll(&main_console)) {
            if (console_getc(&main_console) == '\e') {
                /* We don't set a timeout if forced to show. */
                display = true;
                break;
            }
        }
    } else {
        display = true;

        /* Get the timeout. */
        value = environ_lookup(root_environ, "timeout");
        timeout = (value && value->type == VALUE_TYPE_INTEGER) ? value->integer : 0;
    }

    if (display) {
        /* Build the UI. */
        window = ui_list_create("Boot Menu", false);

        list_foreach(&menu_entries, iter) {
            menu_entry_t *entry = list_entry(iter, menu_entry_t, header);
            ui_list_insert(window, &entry->entry, entry == selected_menu_entry);
        }

        ui_display(window, &main_console, timeout);
    }

    if (selected_menu_entry) {
        dprintf("menu: booting menu entry '%s'\n", selected_menu_entry->name);

        if (selected_menu_entry->error) {
            boot_error("%s", selected_menu_entry->error);
        } else {
            return selected_menu_entry->env;
        }
    } else {
        shell_main();
        target_reboot();
    }
}

/** Handler for configuration errors in a menu entry.
 * @param cmd           Name of the command that caused the error.
 * @param fmt           Error format string.
 * @param args          Arguments to substitute into format. */
static void entry_error_handler(const char *cmd, const char *fmt, va_list args) {
    char *buf = malloc(256);
    size_t count = 0;

    if (cmd)
        count = snprintf(buf + count, 256 - count, "%s: ", cmd);

    vsnprintf(buf + count, 256 - count, fmt, args);

    assert(!executing_menu_entry->error);
    executing_menu_entry->error = buf;
}

/** Add a new menu entry.
 * @param args          Arguments to the command.
 * @return              Whether successful. */
static bool config_cmd_entry(value_list_t *args) {
    menu_entry_t *entry;
    config_error_handler_t prev_handler;

    if (args->count != 2
        || args->values[0].type != VALUE_TYPE_STRING
        || args->values[1].type != VALUE_TYPE_COMMAND_LIST)
    {
        config_error("Invalid arguments");
        return false;
    }

    if (current_environ != root_environ) {
        config_error("Nested entries not allowed");
        return false;
    }

    entry = malloc(sizeof(menu_entry_t));
    entry->entry.type = &menu_entry_type;
    entry->name = args->values[0].string;
    args->values[0].string = NULL;
    entry->env = environ_create(current_environ);
    entry->error = NULL;

    executing_menu_entry = entry;
    prev_handler = config_set_error_handler(entry_error_handler);

    /* Execute the command list. */
    if (!command_list_exec(args->values[1].cmds, entry->env)) {
        /* We don't return an error here. We store the error string, and will
         * display it when the user attempts to boot the failed entry. */
        assert(entry->error);
    } else if (!entry->env->loader) {
        config_error("No operating system loader set");
    }

    config_set_error_handler(prev_handler);

    list_init(&entry->header);
    list_append(&menu_entries, &entry->header);
    return true;
}

BUILTIN_COMMAND("entry", config_cmd_entry);
