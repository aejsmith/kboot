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
 * @brief               Framebuffer drawing functions.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <fb.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** Framebuffer buffer structure. */
typedef struct fb_buffer {
    void *mapping;                  /**< Mapping of real buffer (optional). */
    void *back;                     /**< Back buffer. */
    const pixel_format_t *format;   /**< Pixel format. */
    uint16_t width;                 /**< Width of the buffer. */
    uint16_t height;                /**< Height of the buffer. */
    uint32_t pitch;                 /**< Pitch between lines (in bytes). */
} fb_buffer_t;

/** TGA header structure. */
typedef struct tga_header {
    uint8_t id_length;
    uint8_t colour_map_type;
    uint8_t image_type;
    uint16_t colour_map_origin;
    uint16_t colour_map_length;
    uint8_t colour_map_depth;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t depth;
    uint8_t image_descriptor;
} __packed tga_header_t;

/** Current framebuffer. */
static fb_buffer_t fb_buffer;

/** Convert an ARGB8888 pixel to a given format.
 * @param format        Format to convert to.
 * @param pixel         32-bit ARGB value.
 * @return              Calculated pixel value. */
static uint32_t pixel_to_format(const pixel_format_t *format, pixel_t pixel) {
    uint32_t a, r, g, b;

    a = ((pixel >> (32 - format->alpha_size)) & ((1 << format->alpha_size) - 1)) << format->alpha_pos;
    r = ((pixel >> (24 - format->red_size)) & ((1 << format->red_size) - 1)) << format->red_pos;
    g = ((pixel >> (16 - format->green_size)) & ((1 << format->green_size) - 1)) << format->green_pos;
    b = ((pixel >> (8 - format->blue_size)) & ((1 << format->blue_size) - 1)) << format->blue_pos;

    return a | r | g | b;
}

/** Convert a pixel in a given format to ARGB8888.
 * @param format        Format to convert to.
 * @param val           Pixel value.
 * @return              ARGB8888 pixel value. */
static pixel_t pixel_from_format(const pixel_format_t *format, uint32_t val) {
    pixel_t a, r, g, b;

    if (format->alpha_size) {
        a = ((val >> format->alpha_pos) & ((1 << format->alpha_size) - 1)) << (32 - format->alpha_size);
        if (likely(format->alpha_size >= 4)) {
            /* Reuse most significant bits in bottom missing bits. */
            a |= (a & (((1 << (8 - format->alpha_size)) - 1) << (24 + format->alpha_size))) >> format->alpha_size;
        } else if (a & (1 << (32 - format->alpha_size))) {
            /* Extend out lowest bit into the missing bits. */
            a |= ((1 << (8 - format->alpha_size)) - 1) << 24;
        }
    } else {
        a = 0xff000000;
    }

    r = ((val >> format->red_pos) & ((1 << format->red_size) - 1)) << (24 - format->red_size);
    if (likely(format->red_size >= 4)) {
        r |= (r & (((1 << (8 - format->red_size)) - 1) << (16 + format->red_size))) >> format->red_size;
    } else if (r & (1 << (24 - format->red_size))) {
        r |= ((1 << (8 - format->red_size)) - 1) << 16;
    }

    g = ((val >> format->green_pos) & ((1 << format->green_size) - 1)) << (16 - format->green_size);
    if (likely(format->green_size >= 4)) {
        g |= (g & (((1 << (8 - format->green_size)) - 1) << (8 + format->green_size))) >> format->green_size;
    } else if (g & (1 << (16 - format->green_size))) {
        g |= ((1 << (8 - format->green_size)) - 1) << 8;
    }

    b = ((val >> format->blue_pos) & ((1 << format->blue_size) - 1)) << (8 - format->blue_size);
    if (likely(format->blue_size >= 4)) {
        b |= (b & (((1 << (8 - format->blue_size)) - 1) << format->blue_size)) >> format->blue_size;
    } else if (b & (1 << (8 - format->blue_size))) {
        b |= (1 << (8 - format->blue_size)) - 1;
    }

    return a | r | g | b;
}

/** Get the byte offset of a pixel in a buffer.
 * @param buffer        Buffer to get offset for.
 * @param x             X position of pixel.
 * @param y             Y position of pixel.
 * @return              Byte offset of the pixel. */
static size_t buffer_offset(const fb_buffer_t *buffer, uint16_t x, uint16_t y) {
    return (y * buffer->pitch) + (x * (buffer->format->bpp >> 3));
}

/** Get a pixel from a buffer.
 * @param buffer        Buffer to read from.
 * @param x             X position to read from.
 * @param y             Y position to read from.
 * @return              Pixel value read. */
static pixel_t buffer_get_pixel(const fb_buffer_t *buffer, uint16_t x, uint16_t y) {
    size_t offset = buffer_offset(buffer, x, y);
    void *back = buffer->back + offset;
    uint32_t value = 0;

    switch (buffer->format->bpp >> 3) {
    case 2:
        value = *(uint16_t *)back;
        break;
    case 3:
        value = *(uint16_t *)back | ((uint16_t)(*(uint8_t *)(back + 2)) << 16);
        break;
    case 4:
        value = *(uint32_t *)back;
        break;
    }

    return pixel_from_format(buffer->format, value);
}

/** Put a pixel in a buffer.
 * @param buffer        Buffer to write to.
 * @param x             X position to write to.
 * @param y             Y position to write to.
 * @param pixel         Pixel to write. */
static void buffer_put_pixel(const fb_buffer_t *buffer, uint16_t x, uint16_t y, pixel_t pixel) {
    uint32_t alpha, inv_alpha, current, rb, g, value;
    size_t offset;
    void *back, *main;

    alpha = (pixel & 0xff000000) >> 24;

    if (!alpha) {
        return;
    } else if (alpha != 0xff) {
        current = buffer_get_pixel(buffer, x, y);
        inv_alpha = 0x100 - alpha;
        alpha++;

        rb = (((pixel & 0x00ff00ff) * alpha) + ((current & 0x00ff00ff) * inv_alpha)) & 0xff00ff00;
        g = (((pixel & 0x0000ff00) * alpha) + ((current & 0x0000ff00) * inv_alpha)) & 0x00ff0000;
        pixel = ((rb | g) >> 8) | 0xff000000;
    }

    value = pixel_to_format(buffer->format, pixel);
    offset = buffer_offset(buffer, x, y);
    back = buffer->back + offset;

    switch (buffer->format->bpp >> 3) {
    case 2:
        *(uint16_t *)back = value;
        break;
    case 3:
        *(uint16_t *)back = value & 0xffff;
        *(uint8_t *)(back + 2) = (value >> 16) & 0xff;
        break;
    case 4:
        *(uint32_t *)back = value;
        break;
    }

    if (buffer->mapping) {
        main = buffer->mapping + offset;

        switch (buffer->format->bpp >> 3) {
        case 2:
            *(uint16_t *)main = value;
            break;
        case 3:
            *(uint16_t *)main = value & 0xffff;
            *(uint8_t *)(main + 2) = (value >> 16) & 0xff;
            break;
        case 4:
            *(uint32_t *)main = value;
            break;
        }
    }
}

/** Fill a rectangle in a solid colour.
 * @param buffer        Buffer to fill in.
 * @param x             X position of rectangle.
 * @param y             Y position of rectangle.
 * @param width         Width of rectangle.
 * @param height        Height of rectangle.
 * @param rgb           Colour to draw in (alpha ignored). */
static void buffer_fill_rect(
    const fb_buffer_t *buffer, uint16_t x, uint16_t y, uint16_t width,
    uint16_t height, pixel_t rgb)
{
    rgb &= 0xffffff;

    if (!x && !width)
        width = buffer->width;
    if (!y && !height)
        height = buffer->height;

    if (x == 0 && width == buffer->width && (rgb == 0 || rgb == 0xffffff)) {
        /* Fast path where we can fill a block quickly. */
        memset(
            buffer->back + (y * buffer->pitch),
            (uint8_t)rgb,
            height * buffer->pitch);

        if (buffer->mapping) {
            memset(
                buffer->mapping + (y * buffer->pitch),
                (uint8_t)rgb,
                height * buffer->pitch);
        }
    } else {
        for (uint16_t i = 0; i < height; i++) {
            for (uint16_t j = 0; j < width; j++)
                buffer_put_pixel(buffer, x + j, y + i, rgb | 0xff000000);
        }
    }
}

/** Copy part of a buffer within itself.
 * @param buffer        Buffer to copy on.
 * @param dest_x        X position of destination.
 * @param dest_y        Y position of destination.
 * @param source_x      X position of source area.
 * @param source_y      Y position of source area.
 * @param width         Width of area to copy.
 * @param height        Height of area to copy. */
static void buffer_copy_rect(
    const fb_buffer_t *buffer,
    uint16_t dest_x, uint16_t dest_y, uint16_t source_x, uint16_t source_y,
    uint16_t width, uint16_t height)
{
    size_t dest_offset, source_offset;

    if (dest_x == 0 && source_x == 0 && width == buffer->width) {
        /* Fast path where we can copy everything in one go. */
        dest_offset = dest_y * buffer->pitch;
        source_offset = source_y * buffer->pitch;

        /* Copy everything on the back buffer. */
        memmove(
            buffer->back + dest_offset,
            buffer->back + source_offset,
            height * buffer->pitch);

        /* Copy the affected area to the main buffer. */
        if (buffer->mapping) {
            memcpy(
                buffer->mapping + dest_offset,
                buffer->back + dest_offset,
                height * current_video_mode->pitch);
        }
    } else {
        /* Copy line by line. */
        for (uint16_t i = 0; i < height; i++) {
            dest_offset = buffer_offset(buffer, dest_x, dest_y + i);
            source_offset = buffer_offset(buffer, source_x, source_y + i);

            memmove(
                buffer->back + dest_offset,
                buffer->back + source_offset,
                width * (buffer->format->bpp >> 3));

            if (buffer->mapping) {
                memcpy(
                    buffer->mapping + dest_offset,
                    buffer->back + dest_offset,
                    width * (buffer->format->bpp >> 3));
            }
        }
    }
}

/** Put a pixel on the framebuffer.
 * @param x             X position.
 * @param y             Y position.
 * @param pixel         Pixel to draw. */
void fb_put_pixel(uint16_t x, uint16_t y, pixel_t pixel) {
    buffer_put_pixel(&fb_buffer, x, y, pixel);
}

/** Draw a rectangle in a solid colour.
 * @param x             X position of rectangle.
 * @param y             Y position of rectangle.
 * @param width         Width of rectangle.
 * @param height        Height of rectangle.
 * @param rgb           Colour to draw in (alpha ignored). */
void fb_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, pixel_t rgb) {
    buffer_fill_rect(&fb_buffer, x, y, width, height, rgb);
}

/** Copy part of the framebuffer to another location.
 * @param dest_x        X position of destination.
 * @param dest_y        Y position of destination.
 * @param source_x      X position of source area.
 * @param source_y      Y position of source area.
 * @param width         Width of area to copy.
 * @param height        Height of area to copy. */
void fb_copy_rect(
    uint16_t dest_x, uint16_t dest_y, uint16_t source_x, uint16_t source_y,
    uint16_t width, uint16_t height)
{
    buffer_copy_rect(&fb_buffer, dest_x, dest_y, source_x, source_y, width, height);
}

#ifndef __TEST

/** Convert image data to ARGB888.
 * @param buffer        Buffer to convert.
 * @param image         Image structure to add to. */
static void convert_image(const fb_buffer_t *buffer, fb_image_t *image) {
    pixel_t *pixel;

    image->width = buffer->width;
    image->height = buffer->height;
    image->data = malloc_large(image->width * image->height * sizeof(pixel_t));

    pixel = image->data;

    for (uint16_t y = 0; y < image->height; y++) {
        for (uint16_t x = 0; x < image->width; x++)
            *pixel++ = buffer_get_pixel(buffer, x, y);
    }
}

/** Load a TGA image.
 * @param handle        Handle to image to load.
 * @param image         Image structure to fill in.
 * @return              Status code describing result of the operation. */
static status_t load_tga(fs_handle_t *handle, fb_image_t *image) {
    tga_header_t header;
    pixel_format_t format;
    fb_buffer_t buffer;
    size_t size;
    offset_t offset;
    status_t ret;

    ret = fs_read(handle, &header, sizeof(header), 0);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Only support uncompressed true colour images for now. */
    if (header.image_type != 2)
        return STATUS_UNKNOWN_IMAGE;

    format.bpp = header.depth;

    if (header.depth == 16) {
        format.red_size = format.green_size = format.blue_size = 5;
        format.red_pos = 10;
        format.green_pos = 5;
        format.blue_pos = 0;
        format.alpha_size = 1;
        format.alpha_pos = 15;
    } else if (header.depth == 24 || header.depth == 32) {
        format.red_size = format.green_size = format.blue_size = 8;
        format.red_pos = 16;
        format.green_pos = 8;
        format.blue_pos = 0;
        format.alpha_size = (header.depth == 32) ? 8 : 0;
        format.alpha_pos = (header.depth == 32) ? 24 : 0;
    } else {
        return STATUS_UNKNOWN_IMAGE;
    }

    buffer.format = &format;
    buffer.width = header.width;
    buffer.height = header.height;
    buffer.pitch = (format.bpp >> 3) * buffer.width;

    /* Read in the data, which is after the ID and colour map. */
    size = header.width * header.height * (header.depth / 8);
    offset = sizeof(header)
        + header.id_length
        + (header.colour_map_length * (header.colour_map_depth / 8));

    buffer.back = malloc_large(size);

    ret = fs_read(handle, buffer.back, size, offset);
    if (ret != STATUS_SUCCESS)
        goto out_free;

    convert_image(&buffer, image);

out_free:
    free_large(buffer.back);
    return ret;
}

/** Load an image from the filesystem.
 * @param path          Path to image to load.
 * @param image         Image structure to fill in.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_UNKNOWN_IMAGE if image file type unknown.
 *                      Other status codes for filesystem errors. */
status_t fb_load_image(const char *path, fb_image_t *image) {
    fs_handle_t *handle __cleanup_close = NULL;
    status_t ret;

    ret = fs_open(path, NULL, FILE_TYPE_REGULAR, 0, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (str_ends_with(path, ".tga")) {
        return load_tga(handle, image);
    } else {
        return STATUS_UNKNOWN_IMAGE;
    }
}

/** Destroy previously loaded image data.
 * @param image         Image to destroy. */
void fb_destroy_image(fb_image_t *image) {
    free_large(image->data);
}

/** Draw all or part of an image to the framebuffer.
 * @param image         Image to draw.
 * @param dest_x        Destination X coordinate.
 * @param dest_y        Destination Y coordinate.
 * @param src_x         Source X coordinate.
 * @param src_y         Source Y coordinate.
 * @param width         Width of area to draw.
 * @param height        Height of area to draw. */
void fb_draw_image(
    fb_image_t *image, uint16_t dest_x, uint16_t dest_y, uint16_t src_x,
    uint16_t src_y, uint16_t width, uint16_t height)
{
    if (!src_x && !width)
        width = image->width;
    if (!src_y && !height)
        height = image->height;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            fb_put_pixel(
                dest_x + x,
                dest_y + y,
                image->data[((y + src_y) * image->width) + x + src_x]);
        }
    }
}

#endif /* __TEST */

/** Initialize the framebuffer for current video mode. */
void fb_init(void) {
    assert(current_video_mode->type == VIDEO_MODE_LFB);

    fb_buffer.mapping = (void *)current_video_mode->mem_virt;
    fb_buffer.format = &current_video_mode->format;
    fb_buffer.width = current_video_mode->width;
    fb_buffer.height = current_video_mode->height;
    fb_buffer.pitch = current_video_mode->pitch;

    /* Allocate a backbuffer. */
    fb_buffer.back = malloc_large(fb_buffer.pitch * fb_buffer.height);
}

/** Deinitialize the framebuffer. */
void fb_deinit(void) {
    free_large(fb_buffer.back);
}
