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
 *
 * Notes:
 *  - When adding new menu-related environment variables, add them to the list
 *    of non-inheritable variables in config.c.
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
        fb_image_t icon;                /**< Icon to display in GUI menu. */
        uint16_t icon_x;                /**< X position to draw icon at. */
        uint16_t icon_y;                /**< Y position to draw icon at. */
    #endif
} menu_entry_t;

#ifdef CONFIG_GUI_MENU

/** Structure defining a GUI menu resource (colour or image). */
typedef struct menu_resource {
    enum {
        MENU_RESOURCE_COLOUR,
        MENU_RESOURCE_IMAGE,
    } type;

    union {
        pixel_t colour;
        fb_image_t image;
    };
} menu_resource_t;

#endif /* CONFIG_GUI_MENU */

/** Structure containing menu state. */
typedef struct menu_state {
    struct menu_state *prev;            /**< Previous menu. */
    environ_t *env;                     /**< Environment for the menu. */
    menu_entry_t *selected;             /**< Selected entry. */

    /** Action to perform as a result of displaying the menu. */
    enum {
        MENU_ACTION_NONE,               /**< Do nothing, return to previous menu. */
        MENU_ACTION_BOOT,               /**< Boot the selected entry. */
        MENU_ACTION_SHELL,              /**< Enter the shell. */
    } action;

    #ifdef CONFIG_GUI_MENU
        menu_resource_t background;     /**< Background colour/image. */
        menu_resource_t selection;      /**< Selection colour/image. */
    #endif
} menu_state_t;

/** Current menu state. */
static menu_state_t *current_menu;

/** Currently executing menu entry (used in config command for error handling). */
static menu_entry_t *executing_menu_entry;

static void do_menu(menu_state_t *state, const char *title);

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

    if (!entry->error && entry->env->loader && entry->env->loader->configure)
        ui_print_action(CONSOLE_KEY_F1, "Configure");

    /* If the entry has a loader but also has sub-entries, these are made
     * available via the F* keys. */
    if (entry->env->loader && !list_empty(&entry->env->menu_entries)) {
        unsigned i = 0;

        list_foreach(&entry->env->menu_entries, iter) {
            const menu_entry_t *other = list_entry(iter, menu_entry_t, header);

            ui_print_action(CONSOLE_KEY_F2 + i, other->name);

            /* Maximum is 6, keys F8 and above are reserved. */
            if (++i >= 6)
                break;
        }
    }

    ui_print_action(CONSOLE_KEY_F10, "Shell");
}

/** Display the configuration menu for an entry.
 * @param env           Environment for the entry.
 * @param name          Name of the entry (NULL for root). */
static void display_config_menu(environ_t *env, const char *name) {
    ui_window_t *window;
    environ_t *prev;

    if (name)
        ui_push_title(name);

    prev = current_environ;
    current_environ = env;

    window = env->loader->configure(env->loader_private, "Configure");

    current_environ = prev;

    ui_display(window, 0);
    ui_window_destroy(window);

    if (name)
        ui_pop_title();
}

/** Select an entry.
 * @param entry         Entry to select.
 * @return              Input handling result. */
static input_result_t select_entry(menu_entry_t *entry) {
    if (!entry->env->loader && !entry->error && !list_empty(&entry->env->menu_entries)) {
        /* This entry is a sub-menu, display it. */
        menu_state_t state;
        state.env = entry->env;
        do_menu(&state, entry->name);
        return (current_menu->action != MENU_ACTION_NONE) ? INPUT_CLOSE : INPUT_RENDER_WINDOW;
    } else {
        current_menu->action = MENU_ACTION_BOOT;
        current_menu->selected = entry;
        return INPUT_CLOSE;
    }
}

/** Handle input on a menu entry.
 * @param _entry        Entry input was performed on.
 * @param key           Key that was pressed.
 * @return              Input handling result. */
static input_result_t menu_entry_input(ui_entry_t *_entry, uint16_t key) {
    menu_entry_t *entry = (menu_entry_t *)_entry;

    switch (key) {
    case '\n':
        return select_entry(entry);
    case CONSOLE_KEY_F1:
        if (!entry->error && entry->env->loader && entry->env->loader->configure) {
            display_config_menu(entry->env, entry->name);
            return INPUT_RENDER_WINDOW;
        } else {
            return INPUT_HANDLED;
        }
    case CONSOLE_KEY_F2 ... CONSOLE_KEY_F7:
        /* If the entry has a loader but also has sub-entries, these are made
         * available via the F* keys. */
        if (entry->env->loader && !list_empty(&entry->env->menu_entries)) {
            unsigned function = key - CONSOLE_KEY_F2;
            unsigned i = 0;

            list_foreach(&entry->env->menu_entries, iter) {
                menu_entry_t *other = list_entry(iter, menu_entry_t, header);

                if (i++ == function) {
                    input_result_t result;

                    /* Push the title of the parent entry in so the title bar
                     * reflects the fact that the new menu is a child of it.
                     * Without this we would only get the title of the child
                     * menu. */
                    ui_push_title(entry->name);
                    result = select_entry(other);
                    ui_pop_title();
                    return result;
                }
            }
        }

        return INPUT_HANDLED;
    case CONSOLE_KEY_F9:
        debug_log_display();
        return INPUT_RENDER_WINDOW;
    case CONSOLE_KEY_F10:
        current_menu->action = MENU_ACTION_SHELL;
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

/** Display the text menu.
 * @param title         Title for the menu.
 * @param timeout       Timeout for the menu. */
static void display_text_menu(const char *title, unsigned timeout) {
    /* Window should be exitable if we are not the top level menu. */
    ui_window_t *window = ui_list_create(title, current_menu->prev);

    list_foreach(&current_menu->env->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);
        ui_list_insert(window, &entry->entry, entry == current_menu->selected);
    }

    if (!ui_display(window, timeout)) {
        /* Set action to boot on timeout. */
        current_menu->action = MENU_ACTION_BOOT;
    }

    ui_window_destroy(window);
}

#ifdef CONFIG_GUI_MENU

/** Load a resource for the GUI.
 * @param resource      Resource to load.
 * @param name          Name of the environment variable.
 * @param def           Default colour value.
 * @return              Whether successfully completed. */
static bool load_gui_resource(menu_resource_t *resource, const char *name, pixel_t def) {
    value_t *value;

    resource->type = MENU_RESOURCE_COLOUR;
    resource->colour = def;

    value = environ_lookup(current_menu->env, name);
    if (value) {
        if (value->type == VALUE_TYPE_INTEGER) {
            resource->colour = value->integer;
        } else if (value->type == VALUE_TYPE_STRING) {
            status_t ret;

            resource->type = MENU_RESOURCE_IMAGE;

            ret = fb_load_image(value->string, &resource->image);
            if (ret != STATUS_SUCCESS) {
                dprintf("menu: error loading '%s': %pS\n", value->string, ret);
                return false;
            }
        } else {
            dprintf("menu: '%s' is invalid\n", name);
            return false;
        }
    }

    return true;
}

/** Load icons for all entries and determine positions.
 * @return              Whether successfully completed. */
static bool load_gui_icons(void) {
    menu_entry_t *entry = NULL, *last;
    uint16_t total_width = 0, x;

    list_foreach(&current_menu->env->menu_entries, iter) {
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
    list_foreach(&current_menu->env->menu_entries, iter) {
        entry = list_entry(iter, menu_entry_t, header);

        entry->icon_x = x;
        entry->icon_y = (current_video_mode->height / 2) - (entry->icon.height / 2);

        x += entry->icon.width;
    }

    return true;

err:
    last = entry;

    list_foreach(&current_menu->env->menu_entries, iter) {
        entry = list_entry(iter, menu_entry_t, header);

        if (last == entry)
            break;

        fb_destroy_image(&entry->icon);
    }

    return false;
}

/** Free GUI icons. */
static void free_gui_icons(void) {
    list_foreach(&current_menu->env->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);

        fb_destroy_image(&entry->icon);
    }
}

/** Draw part of the background.
 * @param x             X position of area to draw.
 * @param y             Y position of area to draw.
 * @param width         Width of area to draw.
 * @param height        Height of area to draw. */
static void draw_gui_background(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (current_menu->background.type == MENU_RESOURCE_IMAGE) {
        fb_image_t *image = &current_menu->background.image;

        /* Image may be bigger or smaller than screen, we want to centre it. */
        int16_t bg_left = (current_video_mode->width - image->width) / 2;
        int16_t bg_right = bg_left + image->width;
        int16_t bg_top = (current_video_mode->height - image->height) / 2;
        int16_t bg_bottom = bg_top + image->height;

        /* Fill extra space with black. TODO: Could be optimised to do only the
         * areas not covered by the image. */
        if (bg_left > x || bg_right < x + width || bg_top > y || bg_bottom < y + height)
            fb_fill_rect(x, y, width, height, 0);

        fb_draw_image(
            &current_menu->background.image,
            (bg_left > x) ? bg_left : x,
            (bg_top > y) ? bg_top : y,
            max(x - bg_left, 0),
            max(y - bg_top, 0),
            min(width, bg_right - bg_left),
            min(height, bg_bottom - bg_top));
    } else {
        fb_fill_rect(x, y, width, height, current_menu->background.colour);
    }
}

/** Draw an entry.
 * @param entry         Entry to draw below.
 * @param selected      Whether the entry is selected.
 * @param draw_bg       Whether it is necessary to draw the background. */
static void draw_gui_entry(menu_entry_t *entry, bool selected, bool draw_bg) {
    uint16_t selection_x, selection_y, selection_width, selection_height;

    /* Determine where the selection should be drawn. */
    if (current_menu->selection.type == MENU_RESOURCE_IMAGE) {
        selection_width = current_menu->selection.image.width;
        selection_height = current_menu->selection.image.height;
        selection_x = entry->icon_x + (entry->icon.width / 2) - (selection_width / 2);
        selection_y = (current_video_mode->height / 2) - (selection_height / 2);
    } else {
        selection_width = entry->icon.width;
        selection_height = entry->icon.height;
        selection_x = entry->icon_x;
        selection_y = entry->icon_y;
    }

    /* Draw the background. */
    if (draw_bg) {
        /* Must fill the area including both the selection and the entry icon. */
        draw_gui_background(
            min(entry->icon_x, selection_x),
            min(entry->icon_y, selection_y),
            max(entry->icon.width, selection_width),
            max(entry->icon.height, selection_height));
    }

    /* Draw the selection image. */
    if (selected) {
        if (current_menu->selection.type == MENU_RESOURCE_IMAGE) {
            fb_draw_image(&current_menu->selection.image, selection_x, selection_y, 0, 0, 0, 0);
        } else {
            fb_fill_rect(
                selection_x, selection_y, selection_width, selection_height,
                current_menu->selection.colour);
        }
    }

    /* Draw the icon. */
    fb_draw_image(&entry->icon, entry->icon_x, entry->icon_y, 0, 0, 0, 0);
}

/** Draw the GUI menu. */
static void draw_gui_menu(void) {
    /* Draw the background. */
    draw_gui_background(0, 0, current_video_mode->width, current_video_mode->height);

    /* Draw the entries. */
    list_foreach(&current_menu->env->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);

        draw_gui_entry(entry, entry == current_menu->selected, false);
    }
}

/** Display the GUI menu.
 * @param title         Title for the menu.
 * @param timeout       Timeout for the menu.
 * @return              Whether the GUI menu can be displayed. */
static bool display_gui_menu(const char *title, unsigned timeout) {
    value_t *value;
    mstime_t msecs;
    bool ret = false;

    if (current_video_mode->type != VIDEO_MODE_LFB)
        return false;

    value = environ_lookup(current_menu->env, "gui");
    if (!value || value->type != VALUE_TYPE_BOOLEAN || !value->boolean)
        return false;

    /* Get the background. */
    if (!load_gui_resource(&current_menu->background, "gui_background", 0))
        return false;

    /* Get the selection image. */
    if (!load_gui_resource(&current_menu->selection, "gui_selection", 0xffffff))
        goto out_free_background;

    /* Load entry icons and calculate positioning information. */
    if (!load_gui_icons())
        goto out_free_selection;

    /* Make this visible in the title on any text UI windows we display for
     * consistency with the text menu. */
    ui_push_title(title);

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
                    if (!timeout) {
                        /* Set action to boot on timeout. */
                        current_menu->action = MENU_ACTION_BOOT;
                        break;
                    }
                }
            }
        } else {
            uint16_t key = console_getc(current_console);

            if (key == '\e') {
                if (current_menu->prev)
                    break;
            } else if (key == CONSOLE_KEY_LEFT) {
                menu_entry_t *first = list_first(&current_menu->env->menu_entries, menu_entry_t, header);

                if (current_menu->selected != first) {
                    draw_gui_entry(current_menu->selected, false, true);
                    current_menu->selected = list_prev(current_menu->selected, header);
                    draw_gui_entry(current_menu->selected, true, true);
                }
            } else if (key == CONSOLE_KEY_RIGHT) {
                menu_entry_t *last = list_last(&current_menu->env->menu_entries, menu_entry_t, header);

                if (current_menu->selected != last) {
                    draw_gui_entry(current_menu->selected, false, true);
                    current_menu->selected = list_next(current_menu->selected, header);
                    draw_gui_entry(current_menu->selected, true, true);
                }
            } else {
                /* Reuse the text menu input code. */
                input_result_t result = menu_entry_input(&current_menu->selected->entry, key);

                if (result == INPUT_CLOSE) {
                    break;
                } else if (result == INPUT_RENDER_WINDOW) {
                    draw_gui_menu();
                }
            }
        }
    }

    /* Clear the framebuffer. */
    fb_fill_rect(0, 0, 0, 0, 0);

    ui_pop_title();

    free_gui_icons();

out_free_selection:
    if (current_menu->selection.type == MENU_RESOURCE_IMAGE)
        fb_destroy_image(&current_menu->selection.image);

out_free_background:
    if (current_menu->background.type == MENU_RESOURCE_IMAGE)
        fb_destroy_image(&current_menu->background.image);

    return ret;
}

#else

static inline bool display_gui_menu(unsigned timeout) { return false; }

#endif /* CONFIG_GUI_MENU */

/** Get the default menu entry.
 * @return              Default menu entry. */
static menu_entry_t *get_default_entry(void) {
    const value_t *value = environ_lookup(current_menu->env, "default");

    if (value) {
        unsigned i = 0;

        list_foreach(&current_menu->env->menu_entries, iter) {
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
    return list_first(&current_menu->env->menu_entries, menu_entry_t, header);
}

/** Check if the user requested the menu to be displayed with a key press.
 * @return              Whether the menu should be displayed. */
static bool check_key_press(void) {
    /* Wait half a second for F8 or F10 to be pressed. */
    mstime_t target = current_time() + 500;

    while (current_time() < target) {
        if (console_poll(current_console)) {
            uint16_t key = console_getc(current_console);

            if (key == CONSOLE_KEY_F8) {
                return true;
            } else if (key == CONSOLE_KEY_F10) {
                shell_main();
            }
        }
    }

    return false;
}

/** Display the menu.
 * @param state         State for the menu (env must be set).
 * @param title         Title for the menu. */
static void do_menu(menu_state_t *state, const char *title) {
    environ_t *prev_env;
    const value_t *value;
    bool display;
    unsigned timeout;

    /* Must set current environment so that, for example, relative theme paths
     * will be looked up correctly. */
    prev_env = current_environ;
    current_environ = state->env;

    state->prev = current_menu;
    current_menu = state;

    state->action = MENU_ACTION_NONE;
    state->selected = get_default_entry();

    /* Check if the menu was requested to be hidden. */
    value = environ_lookup(state->env, "hidden");
    if (value && value->type == VALUE_TYPE_BOOLEAN && value->boolean) {
        /* Don't set a timeout if the user manually enters the menu. */
        timeout = 0;
        display = check_key_press();
    } else {
        /* Get the timeout. */
        value = environ_lookup(state->env, "timeout");
        timeout = (value && value->type == VALUE_TYPE_INTEGER) ? value->integer : 0;

        display = true;
    }

    if (display) {
        if (!display_gui_menu(title, timeout))
            display_text_menu(title, timeout);
    } else {
        state->action = MENU_ACTION_BOOT;
    }

    current_menu = state->prev;
    current_environ = prev_env;

    /* Propagate action back to the previous menu. */
    if (state->action != MENU_ACTION_NONE) {
        current_menu->action = state->action;
        current_menu->selected = state->selected;
    }
}

/** Select the environment to boot, possibly displaying the menu.
 * @param env           Environment containing the menu to display.
 * @return              Environment to boot. */
environ_t *menu_select(environ_t *env) {
    menu_state_t state;

    if (list_empty(&env->menu_entries)) {
        assert(!current_menu);

        /* Assume if no entries are declared the root environment is bootable.
         * If it is not an error will be raised by the caller. We do give the
         * user the option to bring up the configuration menu by pressing F8
         * here. */
        if (env->loader && env->loader->configure) {
            if (check_key_press())
                display_config_menu(env, NULL);
        }

        return env;
    }

    state.env = env;
    do_menu(&state, "Boot Menu");

    assert(state.action != MENU_ACTION_NONE);

    /* Perform the action. */
    if (state.action == MENU_ACTION_SHELL) {
        environ_destroy(env);
        shell_main();
    } else {
        dprintf("menu: booting menu entry '%s'\n", state.selected->name);

        if (state.selected->error)
            boot_error("%s", state.selected->error);

        return state.selected->env;
    }
}

/** Clean up menu entries from an environment.
 * @param env           Environment to clean up. */
void menu_cleanup(environ_t *env) {
    list_foreach_safe(&env->menu_entries, iter) {
        menu_entry_t *entry = list_entry(iter, menu_entry_t, header);

        environ_destroy(entry->env);
        free(entry->error);
        free(entry->name);
        free(entry);
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
    menu_entry_t *entry, *prev;
    config_error_handler_t prev_handler;

    if (args->count != 2 ||
        args->values[0].type != VALUE_TYPE_STRING ||
        args->values[1].type != VALUE_TYPE_COMMAND_LIST)
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

    prev = executing_menu_entry;
    executing_menu_entry = entry;

    prev_handler = config_set_error_handler(entry_error_handler);

    /* Execute the command list. */
    if (!command_list_exec(args->values[1].cmds, entry->env)) {
        /* We don't return an error here. We store the error string, and will
         * display it when the user attempts to boot the failed entry. */
        assert(entry->error);
    }

    config_set_error_handler(prev_handler);
    executing_menu_entry = prev;

    list_init(&entry->header);
    list_append(&current_environ->menu_entries, &entry->header);
    return true;
}

BUILTIN_COMMAND("entry", NULL, config_cmd_entry);
