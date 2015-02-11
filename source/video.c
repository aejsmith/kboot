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
 * @brief               Video mode management.
 *
 * TODO:
 *  - Get preferred mode via EDID.
 */

#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** Preferred/fallback video modes. */
#define PREFERRED_MODE_WIDTH    1024
#define PREFERRED_MODE_HEIGHT   768
#define FALLBACK_MODE_WIDTH     800
#define FALLBACK_MODE_HEIGHT    600

/** List of available video modes. */
static LIST_DECLARE(video_modes);

/** Current video mode. */
static video_mode_t *current_video_mode;

/** Set a mode as the current mode.
 * @param mode          Mode that is now current. */
static void set_current_mode(video_mode_t *mode) {
    video_mode_t *prev = current_video_mode;
    bool was_console = prev && prev->ops->console && main_console.out == prev->ops->console;

    if (was_console && prev->ops->console->deinit)
        prev->ops->console->deinit();

    current_video_mode = mode;

    if (!main_console.out || was_console) {
        main_console.out = mode->ops->console;

        if (mode->ops->console && mode->ops->console->init)
            mode->ops->console->init(mode);
    }
}

/** Set a video mode.
 * @param mode          Mode to set.
 * @return              Status code describing the result of the operation. */
status_t video_set_mode(video_mode_t *mode) {
    if (mode != current_video_mode) {
        status_t ret = mode->ops->set_mode(mode);
        if (ret != STATUS_SUCCESS)
            return ret;

        set_current_mode(mode);
    }

    return STATUS_SUCCESS;
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

    if (width == 0 != height == 0)
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
                    if (mode->bpp == bpp)
                        return mode;
                } else if (!ret || mode->bpp > ret->bpp) {
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

/** Get the current video mode.
 * @return              Current video mode. */
video_mode_t *video_current_mode(void) {
    return current_video_mode;
}

/** Register a video mode.
 * @param mode          Mode to register.
 * @param current       Whether the mode is the current mode. */
void video_mode_register(video_mode_t *mode, bool current) {
    assert(!current || !current_video_mode);

    list_init(&mode->header);
    list_append(&video_modes, &mode->header);

    if (current)
        set_current_mode(mode);
}

/** Print a list of video modes.
 * @param args          Argument list.
 * @return              Whether successful. */
static bool config_cmd_lsvideo(value_list_t *args) {
    if (args->count != 0) {
        config_printf("lsvideo: Invalid arguments");
        return false;
    }

    list_foreach(&video_modes, iter) {
        video_mode_t *mode = list_entry(iter, video_mode_t, header);

        switch (mode->type) {
        case VIDEO_MODE_VGA:
            config_printf("vga:%" PRIu32 "x%" PRIu32, mode->width, mode->height);
            break;
        case VIDEO_MODE_LFB:
            config_printf("lfb:%" PRIu32 "x%" PRIu32 "x%" PRIu8, mode->width, mode->height, mode->bpp);
            break;
        }

        config_printf("%s\n", (mode == current_video_mode) ? " (current)" : "");
    }

    return true;
}

BUILTIN_COMMAND("lsvideo", config_cmd_lsvideo);
