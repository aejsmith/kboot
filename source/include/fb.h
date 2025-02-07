/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Framebuffer drawing functions.
 */

#ifndef __FB_H
#define __FB_H

#include <video.h>

#if defined(CONFIG_FB) || defined(__TEST)

/** Framebuffer image data. */
typedef struct fb_image {
    uint16_t width;                 /**< Width of the image. */
    uint16_t height;                /**< Height of the image. */
    pixel_t *data;                  /**< Image data. */
} fb_image_t;

extern void fb_set_autoflush(bool autoflush);
extern void fb_flush_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

extern void fb_put_pixel(uint16_t x, uint16_t y, pixel_t rgb);
extern void fb_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, pixel_t rgb);
extern void fb_copy_rect(
    uint16_t dest_x, uint16_t dest_y, uint16_t src_x, uint16_t src_y,
    uint16_t width, uint16_t height);

extern status_t fb_load_image(const char *path, fb_image_t *image);
extern void fb_destroy_image(fb_image_t *image);
extern void fb_draw_image(
    fb_image_t *image, uint16_t dest_x, uint16_t dest_y, uint16_t src_x,
    uint16_t src_y, uint16_t width, uint16_t height);

extern void fb_init(void);
extern void fb_deinit(void);

#else

static inline void fb_init(void) {}
static inline void fb_deinit(void) {}

#endif /* CONFIG_FB */
#endif /* __FB_H */
