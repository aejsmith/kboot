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
 */

#ifndef __VIDEO_H
#define __VIDEO_H

#include <lib/list.h>

#include <status.h>
#include <types.h>

struct console_out;
struct environ;

/** Pixel format information. */
typedef struct pixel_format {
    uint8_t bpp;                        /**< Number of bits per pixel. */
    uint8_t red_size;                   /**< Size of red component of each pixel. */
    uint8_t red_pos;                    /**< Bit position of the red component of each pixel. */
    uint8_t green_size;                 /**< Size of green component of each pixel. */
    uint8_t green_pos;                  /**< Bit position of the green component of each pixel. */
    uint8_t blue_size;                  /**< Size of blue component of each pixel. */
    uint8_t blue_pos;                   /**< Bit position of the blue component of each pixel. */
    uint8_t alpha_size;                 /**< Size of alpha component of each pixel. */
    uint8_t alpha_pos;                  /**< Bit position of the alpha component of each pixel. */
} pixel_format_t;

/** Pixel in ARGB8888 format. */
typedef uint32_t pixel_t;

/** Video mode types (defined to match KBoot types). */
typedef enum video_mode_type {
    VIDEO_MODE_VGA = (1<<0),            /**< VGA. */
    VIDEO_MODE_LFB = (1<<1),            /**< Linear framebuffer. */
} video_mode_type_t;

/** Tag containing video mode information. */
typedef struct video_mode {
    list_t header;                      /**< Link to mode list. */

    video_mode_type_t type;             /**< Type of the video mode. */
    const struct video_ops *ops;        /**< Operations for the video mode. */

    /** Common information. */
    uint32_t width;                     /**< LFB pixel width/VGA number of columns. */
    uint32_t height;                    /**< LFB pixel height/VGA number of rows. */
    phys_ptr_t mem_phys;                /**< Physical address of LFB/VGA memory. */
    ptr_t mem_virt;                     /**< Loader virtual address of LFB/VGA memory. */
    uint32_t mem_size;                  /**< Size of LFB/VGA memory. */

    union {
        /** VGA information. */
        struct {
            /** Cursor position information, stored in case OS wants it. */
            uint8_t x;                  /**< Cursor X position. */
            uint8_t y;                  /**< Cursor Y position. */
        };

        /** Linear framebuffer information. */
        struct {
            pixel_format_t format;      /**< Pixel format information. */
            uint32_t pitch;             /**< Number of bytes per line of the framebuffer. */
        };
    };
} video_mode_t;

#ifdef CONFIG_TARGET_HAS_VIDEO

/** Structure containing video mode operations. */
typedef struct video_ops {
    /** Set the mode.
     * @param mode          Mode to set. */
    void (*set_mode)(video_mode_t *mode);

    /** Create a console for a mode (optional).
     * @param mode          Mode to create for.
     * @return              Pointer to created console. */
    struct console_out *(*create_console)(video_mode_t *mode);
} video_ops_t;

extern video_mode_t *current_video_mode;

extern void video_set_mode(video_mode_t *mode, bool set_console);

extern video_mode_t *video_find_mode(video_mode_type_t type, uint32_t width, uint32_t height, uint32_t bpp);
extern video_mode_t *video_parse_and_find_mode(const char *str);

extern void video_env_init(struct environ *env, const char *name, uint32_t types, video_mode_t *def);
extern video_mode_t *video_env_set(struct environ *env, const char *name);

#ifdef CONFIG_TARGET_HAS_UI
extern struct ui_entry *video_env_chooser(struct environ *env, const char *name, uint32_t types);
#endif

extern void video_mode_register(video_mode_t *mode, bool current);

#endif /* CONFIG_TARGET_HAS_VIDEO */

#endif /* __VIDEO_H */
