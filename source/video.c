/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Video mode management.
 *
 * TODO:
 *  - Get preferred mode via EDID.
 */

#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <fb.h>
#include <loader.h>
#include <memory.h>
#include <ui.h>
#include <video.h>

/** Preferred/fallback video modes. */
#define PREFERRED_MODE_WIDTH    1024
#define PREFERRED_MODE_HEIGHT   768
#define FALLBACK_MODE_WIDTH     800
#define FALLBACK_MODE_HEIGHT    600

/** List of available video modes. */
static LIST_DECLARE(video_modes);

/** Current video mode. */
video_mode_t *current_video_mode;

/** Set a mode as the current mode.
 * @param mode          Mode that is now current.
 * @param set_console   Whether to set the mode as console. */
static void set_current_mode(video_mode_t *mode, bool set_console) {
    current_video_mode = mode;

    if (mode) {
        if (mode->type == VIDEO_MODE_LFB)
            fb_init();

        if (set_console && mode->ops->create_console) {
            console_out_t *console = mode->ops->create_console(mode);

            primary_console.out = console;

            if (primary_console.active && console && console->ops->init)
                console->ops->init(console);
        }
    }
}

/** Set a video mode.
 * @param mode          Mode to set.
 * @param set_console   Whether to set the mode as console. */
void video_set_mode(video_mode_t *mode, bool set_console) {
    video_mode_t *prev = current_video_mode;

    if (prev) {
        if (prev->type == VIDEO_MODE_LFB)
            fb_deinit();

        if (prev->ops->create_console && primary_console.out) {
            if (primary_console.active && primary_console.out->ops->deinit)
                primary_console.out->ops->deinit(primary_console.out);

            free(primary_console.out);
        }
    }

    primary_console.out = NULL;

    if (mode)
        mode->ops->set_mode(mode);

    set_current_mode(mode, set_console);
}

/**
 * Find a video mode.
 *
 * Finds a video mode matching the specified parameters. If width/height/bpp are
 * specified as zero, will return the preferred mode of the requested type. If
 * just bpp is zero, the highest available depth mode with the requested
 * dimensions will be returned.
 *
 * @param type          Type of the video mode to search for.
 * @param width         Desired pixel width for LFB, number of columns for VGA.
 * @param height        Desired pixel height for LFB, number of rows for VGA.
 * @param bpp           Bits per pixel for LFB, ignored for VGA.
 *
 * @return              Mode found, or NULL if none matching.
 */
video_mode_t *video_find_mode(video_mode_type_t type, uint32_t width, uint32_t height, uint32_t bpp) {
    video_mode_t *mode, *ret;

    if ((width == 0) != (height == 0))
        return NULL;

    /* Check if we've been asked for a preferred mode. TODO: EDID */
    if (width == 0 && height == 0 && bpp == 0) {
        switch (type) {
        case VIDEO_MODE_VGA:
            return video_find_mode(type, 80, 25, 0);
        case VIDEO_MODE_LFB:
            ret = video_find_mode(type, PREFERRED_MODE_WIDTH, PREFERRED_MODE_HEIGHT, 0);
            if (!ret)
                ret = video_find_mode(type, FALLBACK_MODE_WIDTH, FALLBACK_MODE_HEIGHT, 0);

            return ret;
        }
    }

    /* Search for a matching mode. */
    ret = NULL;
    list_foreach(&video_modes, iter) {
        mode = list_entry(iter, video_mode_t, header);

        if (mode->type != type)
            continue;

        if (mode->width == width && mode->height == height) {
            if (type == VIDEO_MODE_LFB) {
                if (bpp) {
                    if (mode->format.bpp == bpp)
                        return mode;
                } else if (!ret || mode->format.bpp > ret->format.bpp) {
                    ret = mode;
                }
            } else {
                return mode;
            }
        }
    }

    return ret;
}

/**
 * Parse a video mode string and find a matching mode.
 *
 * Parses a string in the form "<type>[:<width>x<height>[x<bpp>]]" and returns
 * the result of video_find_mode() on the parsed values.
 *
 * @param str           String to parse.
 *
 * @return              Mode found, or NULL if none matching.
 */
video_mode_t *video_parse_and_find_mode(const char *str) {
    char *orig __cleanup_free = NULL;
    char *dup, *tok;
    video_mode_type_t type;
    uint32_t width, height, bpp;

    dup = orig = strdup(str);

    tok = strsep(&dup, ":");
    if (!tok)
        return NULL;

    if (strcmp(tok, "vga") == 0) {
        type = VIDEO_MODE_VGA;
    } else if (strcmp(tok, "lfb") == 0) {
        type = VIDEO_MODE_LFB;
    } else {
        return NULL;
    }

    tok = strsep(&dup, "x");
    width = (tok) ? strtoul(tok, NULL, 10) : 0;
    tok = strsep(&dup, "x");
    height = (tok) ? strtoul(tok, NULL, 10) : 0;
    tok = strsep(&dup, "x");
    bpp = (tok) ? strtoul(tok, NULL, 10) : 0;

    return video_find_mode(type, width, height, bpp);
}

/** Format a video mode string.
 * @param mode          Video mode to generate string from.
 * @param buf           Buffer to write into.
 * @param size          Size of buffer. */
static void format_mode_string(video_mode_t *mode, char *buf, size_t size) {
    switch (mode->type) {
    case VIDEO_MODE_VGA:
        snprintf(buf, size, "vga:%ux%u", mode->width, mode->height);
        break;
    case VIDEO_MODE_LFB:
        snprintf(buf, size, "lfb:%ux%ux%u", mode->width, mode->height, mode->format.bpp);
        break;
    default:
        unreachable();
    }
}

/** Initialize a video mode environment variable.
 * @param env           Environment to use.
 * @param name          Name of the variable.
 * @param types         Bitmask of allowed mode types.
 * @param def           Default mode to set, or NULL to pick. */
void video_env_init(environ_t *env, const char *name, uint32_t types, video_mode_t *def) {
    video_mode_t *mode = NULL;
    value_t *exist, value;
    char buf[20];

    /* Check if the value exists and is valid. */
    exist = environ_lookup(env, name);
    if (exist && exist->type == VALUE_TYPE_STRING) {
        mode = video_parse_and_find_mode(exist->string);

        if (mode && !(types & mode->type))
            mode = NULL;
    }

    if (!mode) {
        /* Not valid, create an entry referring to the default mode. */
        if (def) {
            mode = def;
        } else {
            assert(current_video_mode);
            mode = current_video_mode;
        }
    }

    /* Set the mode in the environment. This is done even if the existing value
     * was valid, in case it was specified as a shortened string. In this case
     * we have to set it to the actual mode it was expanded to, so that it
     * matches exactly against a mode if we come to create a mode chooser UI
     * for it later. */
    format_mode_string(mode, buf, sizeof(buf));
    value.type = VALUE_TYPE_STRING;
    value.string = buf;
    environ_insert(env, name, &value);
}

/** Set the video mode from the environment.
 * @param env           Environment to use.
 * @param name          Name of the variable.
 * @return              Pointer to mode set, or NULL if non-existant. */
video_mode_t *video_env_set(environ_t *env, const char *name) {
    value_t *value;
    video_mode_t *mode;

    value = environ_lookup(env, name);
    if (!value)
        return NULL;

    assert(value->type == VALUE_TYPE_STRING);

    mode = video_parse_and_find_mode(value->string);
    assert(mode);

    /* Assume we're setting for the OS, so don't enable the console. */
    video_set_mode(mode, false);
    return mode;
}

#ifdef CONFIG_TARGET_HAS_UI

/** Create a video mode chooser.
 * @param env           Environment to use.
 * @param name          Name of the value to modify.
 * @param types         Bitmask of allowed mode types. */
ui_entry_t *video_env_chooser(environ_t *env, const char *name, uint32_t types) {
    value_t *value;
    ui_entry_t *chooser;

    value = environ_lookup(env, name);
    assert(value && value->type == VALUE_TYPE_STRING);

    chooser = ui_chooser_create("Video mode", value);

    list_foreach(&video_modes, iter) {
        video_mode_t *mode = list_entry(iter, video_mode_t, header);

        if (types & mode->type) {
            char buf[20];
            value_t entry;

            format_mode_string(mode, buf, sizeof(buf));
            entry.type = VALUE_TYPE_STRING;
            entry.string = buf;

            ui_chooser_insert(chooser, &entry, NULL);
        }
    }

    return chooser;
}

#endif /* CONFIG_TARGET_HAS_UI */

/** Register a video mode.
 * @param mode          Mode to register.
 * @param current       Whether the mode is the current mode. */
void video_mode_register(video_mode_t *mode, bool current) {
    assert(!current || !current_video_mode);

    list_init(&mode->header);
    list_append(&video_modes, &mode->header);

    if (current)
        set_current_mode(mode, true);
}

/**
 * Shell commands.
 */

/** List available video modes.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_lsvideo(value_list_t *args) {
    if (args->count != 0) {
        config_error("Invalid arguments");
        return false;
    }

    list_foreach(&video_modes, iter) {
        video_mode_t *mode = list_entry(iter, video_mode_t, header);

        switch (mode->type) {
        case VIDEO_MODE_VGA:
            printf("vga:%" PRIu32 "x%" PRIu32, mode->width, mode->height);
            break;
        case VIDEO_MODE_LFB:
            printf("lfb:%" PRIu32 "x%" PRIu32 "x%" PRIu8, mode->width, mode->height, mode->format.bpp);
            break;
        }

        printf("%s\n", (mode == current_video_mode) ? " (current)" : "");
    }

    return true;
}

BUILTIN_COMMAND("lsvideo", "List available video modes", config_cmd_lsvideo);

/** Set the current video mode.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_video(value_list_t *args) {
    video_mode_t *mode, *prev;

    if (args->count != 1 || args->values[0].type != VALUE_TYPE_STRING) {
        config_error("Invalid arguments");
        return false;
    }

    mode = video_parse_and_find_mode(args->values[0].string);
    if (!mode) {
        config_error("Mode '%s' not found", args->values[0].string);
        return false;
    }

    prev = current_video_mode;
    video_set_mode(mode, true);

    /* If currently outputting to this console, need output support. */
    if (primary_console.active && !primary_console.out) {
        video_set_mode(prev, true);
        config_error("Mode '%s' does not support console output", args->values[0].string);
        return false;
    }

    return true;
}

BUILTIN_COMMAND("video", "Set the current video mode", config_cmd_video);
