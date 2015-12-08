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
#include <fb.h>
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

    #ifdef CONFIG_GUI_MENU
    fb_image_t icon;                    /**< Icon to display in GUI menu. */
    uint16_t icon_x;                    /**< X position to draw icon at. */
    uint16_t icon_y;                    /**< Y position to draw icon at. */
    #endif
} menu_entry_t;

/** Currently executing menu entry. */
static menu_entry_t *executing_menu_entry;

/** Selected menu entry. */
static menu_entry_t *selected_menu_entry;

#ifdef CONFIG_GUI_MENU

/** Background colour for the GUI menu. */
static pixel_t gui_background_colour;

/** Selection image. */
static fb_image_t gui_selection_image;

#endif /* CONFIG_GUI_MENU */

/** Render a menu entry.
 * @param _entry        Entry to render. */
static void menu_entry_render(ui_entry_t *_entry) {
    menu_entry_t *entry = (menu_entry_t *)_entry;

    printf("%s", entry->name);
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
 * @param env           Environment for the entry.
 * @param name          Name of the entry (NULL for root). */
static void display_config_menu(environ_t *env, const char *name) {
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

    ui_display(window, 0);
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
            display_config_menu(entry->env, entry->name);
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

/** Display the text menu.
 * @param timeout       Timeout for the menu. */
static void display_text_menu(unsigned timeout) {
    ui_window_t *window = ui_list_create("Boot Menu", false);

    list_foreach(&current_environ->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);
        ui_list_insert(window, &entry->entry, entry == selected_menu_entry);
    }

    ui_display(window, timeout);
}

#ifdef CONFIG_GUI_MENU

/** Load the background for the GUI.
 * @return              Whether successfully completed. */
static bool load_gui_background(void) {
    value_t *value;

    gui_background_colour = 0;
    value = environ_lookup(current_environ, "gui_background");
    if (value) {
        if (value->type == VALUE_TYPE_INTEGER) {
            gui_background_colour = value->integer;
        } else if (value->type == VALUE_TYPE_STRING) {
            dprintf("menu: background image not implemented yet\n");
            return false;
        } else {
            dprintf("menu: 'gui_background' is invalid\n");
            return false;
        }
    }

    return true;
}

/** Load the selection image for the GUI.
 * @return              Whether successfully completed. */
static bool load_gui_selection_image(void) {
    value_t *value;
    status_t ret;

    value = environ_lookup(current_environ, "gui_selection");
    if (!value || value->type != VALUE_TYPE_STRING) {
        dprintf("menu: 'gui_selection' is invalid\n");
        return false;
    }

    ret = fb_load_image(value->string, &gui_selection_image);
    if (ret != STATUS_SUCCESS) {
        dprintf("menu: error loading '%s': %pS\n", value->string, ret);
        return false;
    }

    return true;
}

/** Load icons for all entries and determine positions.
 * @return              Whether successfully completed. */
static bool load_gui_icons(void) {
    menu_entry_t *entry = NULL, *last;
    uint16_t total_width = 0, x;

    list_foreach(&current_environ->menu_entries, iter) {
        value_t *value;
        status_t ret;

        entry = list_entry(iter, menu_entry_t, header);

        value = environ_lookup(entry->env, "gui_icon");
        if (!value || value->type != VALUE_TYPE_STRING) {
            dprintf("menu: 'gui_icon' for entry '%s' is invalid\n", entry->name);
            goto err;
        }

        ret = fb_load_image(value->string, &entry->icon);
        if (ret != STATUS_SUCCESS) {
            dprintf("menu: error loading '%s': %pS\n", value->string, ret);
            goto err;
        }

        total_width += entry->icon.width;
    }

    if (total_width > current_video_mode->width) {
        dprintf("menu: not enough horizontal screen space for GUI menu\n");
        goto err;
    }

    /* Calculate the position of each entry's icon */
    x = (current_video_mode->width / 2) - (total_width / 2);
    list_foreach(&current_environ->menu_entries, iter) {
        entry = list_entry(iter, menu_entry_t, header);

        entry->icon_x = x;
        entry->icon_y = (current_video_mode->height / 2) - (entry->icon.height / 2);

        x += entry->icon.width;
    }

    return true;

err:
    last = entry;

    list_foreach(&current_environ->menu_entries, iter) {
        entry = list_entry(iter, menu_entry_t, header);

        if (last == entry)
            break;

        fb_destroy_image(&entry->icon);
    }

    return false;
}

/** Free GUI icons. */
static void free_gui_icons(void) {
    list_foreach(&current_environ->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);

        fb_destroy_image(&entry->icon);
    }
}

/** Draw/clear the selection below an entry.
 * @param entry         Entry to draw below.
 * @param selected      Whether the entry is now selected. */
static void draw_gui_selection(menu_entry_t *entry, bool selected) {
    uint16_t x, y;

    x = entry->icon_x + (entry->icon.width / 2) - (gui_selection_image.width / 2);
    y = entry->icon_y + entry->icon.height;

    if (selected) {
        fb_draw_image(&gui_selection_image, x, y, 0, 0, 0, 0);
    } else {
        // FIXME: Will need to handle background image.
        fb_fill_rect(
            x, y, gui_selection_image.width, gui_selection_image.height,
            gui_background_colour);
    }
}

/** Draw the GUI menu. */
static void draw_gui_menu(void) {
    /* Fill to the background colour. */
    fb_fill_rect(0, 0, 0, 0, gui_background_colour);

    /* Draw the entry icons. */
    list_foreach(&current_environ->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);

        fb_draw_image(&entry->icon, entry->icon_x, entry->icon_y, 0, 0, 0, 0);
    }

    /* Mark the current selection. */
    draw_gui_selection(selected_menu_entry, true);
}

/** Display the GUI menu.
 * @param timeout       Timeout for the menu.
 * @return              Whether the GUI menu can be displayed. */
static bool display_gui_menu(unsigned timeout) {
    value_t *value;
    mstime_t msecs;
    bool ret = false;

    if (current_video_mode->type != VIDEO_MODE_LFB)
        return false;

    value = environ_lookup(current_environ, "gui");
    if (!value || value->type != VALUE_TYPE_BOOLEAN || !value->boolean)
        return false;

    /* Get the background. */
    if (!load_gui_background())
        return false;

    /* Get the selection image. */
    if (!load_gui_selection_image())
        return false;

    /* Load entry icons and calculate positioning information. */
    if (!load_gui_icons())
        goto out_free_selection;

    ret = true;
    draw_gui_menu();

    msecs = secs_to_msecs(timeout);
    while (true) {
        if (timeout) {
            if (console_poll(current_console)) {
                timeout = 0;
            } else {
                delay(10);
                msecs -= 10;

                if (round_up(msecs, 1000) / 1000 < timeout) {
                    timeout--;
                    if (!timeout)
                        break;
                }
            }
        } else {
            uint16_t key = console_getc(current_console);
            input_result_t result;
            bool done = false;

            switch (key) {
            case CONSOLE_KEY_LEFT:
                if (selected_menu_entry != list_first(&current_environ->menu_entries, menu_entry_t, header)) {
                    draw_gui_selection(selected_menu_entry, false);
                    selected_menu_entry = list_prev(selected_menu_entry, header);
                    draw_gui_selection(selected_menu_entry, true);
                }

                break;
            case CONSOLE_KEY_RIGHT:
                if (selected_menu_entry != list_last(&current_environ->menu_entries, menu_entry_t, header)) {
                    draw_gui_selection(selected_menu_entry, false);
                    selected_menu_entry = list_next(selected_menu_entry, header);
                    draw_gui_selection(selected_menu_entry, true);
                }

                break;
            default:
                /* Reuse the text menu input code. */
                result = menu_entry_input(&selected_menu_entry->entry, key);

                if (result == INPUT_CLOSE) {
                    done = true;
                } else if (result == INPUT_RENDER_WINDOW) {
                    draw_gui_menu();
                }

                break;
            }

            if (done)
                break;
        }
    }

    /* Clear the framebuffer. */
    fb_fill_rect(0, 0, 0, 0, 0);

    free_gui_icons();

out_free_selection:
    fb_destroy_image(&gui_selection_image);

    return ret;
}

#else

static inline bool display_gui_menu(unsigned timeout) { return false; }

#endif /* CONFIG_GUI_MENU */

/** Get the default menu entry.
 * @return              Default menu entry. */
static menu_entry_t *get_default_entry(void) {
    const value_t *value = environ_lookup(current_environ, "default");

    if (value) {
        size_t i = 0;

        list_foreach(&current_environ->menu_entries, iter) {
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
    return list_first(&current_environ->menu_entries, menu_entry_t, header);
}

/** Check if the user requested the menu to be displayed with a key press.
 * @return              Whether the menu should be displayed. */
static bool check_key_press(void) {
    /* Wait half a second for Esc to be pressed. */
    delay(500);
    while (console_poll(current_console)) {
        uint16_t key = console_getc(current_console);

        if (key == CONSOLE_KEY_F8) {
            return true;
        } else if (key == CONSOLE_KEY_F2) {
            shell_main();
        }
    }

    return false;
}

/** Select the environment to boot, possibly displaying the menu.
 * @return              Environment to boot. */
environ_t *menu_select(void) {
    const value_t *value;
    bool display;
    unsigned timeout;

    if (list_empty(&current_environ->menu_entries)) {
        /* Assume if no entries are declared the root environment is bootable.
         * If it is not an error will be raised in loader_main(). We do give
         * the user the option to bring up the configuration menu by pressing
         * escape here. */
        if (current_environ->loader && current_environ->loader->configure) {
            if (check_key_press())
                display_config_menu(current_environ, NULL);
        }

        return current_environ;
    }

    /* Determine the default entry. */
    selected_menu_entry = get_default_entry();

    /* Check if the menu was requested to be hidden. */
    value = environ_lookup(current_environ, "hidden");
    if (value && value->type == VALUE_TYPE_BOOLEAN && value->boolean) {
        /* Don't set a timeout if the user manually enters the menu. */
        timeout = 0;
        display = check_key_press();
    } else {
        /* Get the timeout. */
        value = environ_lookup(current_environ, "timeout");
        timeout = (value && value->type == VALUE_TYPE_INTEGER) ? value->integer : 0;

        display = true;
    }

    if (display) {
        if (!display_gui_menu(timeout))
            display_text_menu(timeout);
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
    list_append(&current_environ->menu_entries, &entry->header);
    return true;
}

BUILTIN_COMMAND("entry", NULL, config_cmd_entry);
