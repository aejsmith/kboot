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

/** Display the configuration menu for an entry.
 * @param console       Console to display on.
 * @param env           Environment for the entry.
 * @param name          Name of the entry (NULL for root). */
static void display_config_menu(console_t *console, environ_t *env, const char *name) {
    char *title;
    ui_window_t *window;
    environ_t *prev;

    /* Determine the window title. */
    if (name) {
        size_t len = strlen(name) + 13;
        title = malloc(len);
        snprintf(title, len, "Configure '%s'", name);
    }

    prev = current_environ;
    current_environ = env;
    window = env->loader->configure(env->loader_private, (name) ? title : "Configure");
    current_environ = prev;

    ui_display(window, console, 0);
    ui_window_destroy(window);

    if (name)
        free(title);
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
            display_config_menu(ui_console, entry->env, entry->name);
            return INPUT_RENDER_WINDOW;
        } else {
            return INPUT_HANDLED;
        }
    case CONSOLE_KEY_F2:
        /* This is taken to mean the shell should be entered. */
        selected_menu_entry = NULL;
        return INPUT_CLOSE;
    case CONSOLE_KEY_F10:
        debug_log_display();
        return INPUT_RENDER_WINDOW;
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

/** Check if the user requested the menu to be displayed with a key press.
 * @return              Whether the menu should be displayed. */
static bool check_key_press(void) {
    /* Wait half a second for Esc to be pressed. */
    delay(500);
    while (console_poll(&main_console)) {
        uint16_t key = console_getc(&main_console);

        if (key == CONSOLE_KEY_F8) {
            return true;
        } else if (key == CONSOLE_KEY_F2) {
            shell_main();
        }
    }

    return false;
}


/** Display the menu interface.
 * @return              Environment to boot. */
environ_t *menu_display(void) {
    const value_t *value;
    bool display;
    unsigned timeout;

    if (list_empty(&menu_entries)) {
        /* Assume if no entries are declared the root environment is bootable.
         * If it is not an error will be raised in loader_main(). We do give
         * the user the option to bring up the configuration menu by pressing
         * escape here. */
        if (root_environ->loader && root_environ->loader->configure) {
            if (check_key_press())
                display_config_menu(&main_console, root_environ, NULL);
        }

        return root_environ;
    }

    /* Determine the default entry. */
    selected_menu_entry = get_default_entry();

    /* Check if the menu was requested to be hidden. */
    value = environ_lookup(root_environ, "hidden");
    if (value && value->type == VALUE_TYPE_BOOLEAN && value->boolean) {
        /* Don't set a timeout if the user manually enters the menu. */
        timeout = 0;
        display = check_key_press();
    } else {
        /* Get the timeout. */
        value = environ_lookup(root_environ, "timeout");
        timeout = (value && value->type == VALUE_TYPE_INTEGER) ? value->integer : 0;

        display = true;
    }

    if (display) {
        ui_window_t *window = ui_list_create("Boot Menu", false);

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
        /* Selected entry is set NULL to indicate that F2 was pressed. */
        shell_main();
    }
}

/** Handler for configuration errors in a menu entry.
 * @param cmd           Name of the command that caused the error.
 * @param fmt           Error format string.
 * @param args          Arguments to substitute into format. */
static void entry_error_handler(const char *cmd, const char *fmt, va_list args) {
    char *buf = malloc(256);

    vsnprintf(buf, 256, fmt, args);

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
    }

    config_set_error_handler(prev_handler);

    list_init(&entry->header);
    list_append(&menu_entries, &entry->header);
    return true;
}

BUILTIN_COMMAND("entry", NULL, config_cmd_entry);
