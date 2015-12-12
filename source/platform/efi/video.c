/*
 * Copyright (C) 2014-2015 Alex Smith
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
 * @brief               EFI video mode detection.
 */

#include <drivers/console/fb.h>

#include <lib/utility.h>

#include <efi/efi.h>
#include <efi/video.h>
#include <efi/services.h>

#include <console.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** EFI video mode structure. */
typedef struct efi_video_mode {
    video_mode_t mode;                      /**< Video mode structure. */
    uint32_t num;                           /**< Mode number. */
} efi_video_mode_t;

/** Graphics output protocol GUID. */
static efi_guid_t graphics_output_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

/** Graphics output_protocol. */
static efi_graphics_output_protocol_t *graphics_output;

/** Original video mode number. */
static uint32_t original_mode;

/** Set an EFI video mode.
 * @param _mode         Mode to set. */
static void efi_video_set_mode(video_mode_t *_mode) {
    efi_video_mode_t *mode = (efi_video_mode_t *)_mode;
    efi_status_t ret;

    ret = efi_call(graphics_output->set_mode, graphics_output, mode->num);
    if (ret != EFI_SUCCESS)
        internal_error("Failed to set video mode %u (0x%zx)", mode->num, ret);

    /* Get the framebuffer information. */
    mode->mode.mem_phys = graphics_output->mode->frame_buffer_base;
    mode->mode.mem_virt = graphics_output->mode->frame_buffer_base;
    mode->mode.mem_size = graphics_output->mode->frame_buffer_size;
}

/** Create a console for a mode.
 * @param _mode         Mode to create for.
 * @return              Pointer to created console. */
static console_out_t *efi_video_create_console(video_mode_t *_mode) {
    return fb_console_create();
}

/** EFI video operations. */
static video_ops_t efi_video_ops = {
    .set_mode = efi_video_set_mode,
    .create_console = efi_video_create_console,
};

/** Get the depth for a GOP mode.
 * @param info          Mode information.
 * @return              Bits per pixel for the mode, or 0 if not supported. */
static uint8_t get_mode_bpp(efi_graphics_output_mode_information_t *info) {
    uint32_t mask;

    switch (info->pixel_format) {
    case EFI_PIXEL_FORMAT_RGBR8:
    case EFI_PIXEL_FORMAT_BGRR8:
        return 32;
    case EFI_PIXEL_FORMAT_BITMASK:
        /* Get the last set bit in the complete mask. */
        mask = info->pixel_bitmask.red_mask
            | info->pixel_bitmask.green_mask
            | info->pixel_bitmask.blue_mask
            | info->pixel_bitmask.reserved_mask;
        return fls(mask);
    default:
        return 0;
    }
}

/** Calculate a component size and position from a bitmask.
 * @param mask          Mask to convert.
 * @param _size         Where to store component size.
 * @param _pos          Where to store component position. */
static void get_component_size_pos(uint32_t mask, uint8_t *_size, uint8_t *_pos) {
    int first = ffs(mask);
    int last = fls(mask);

    *_size = last - first + 1;
    *_pos = first - 1;
}

/** Detect available video modes. */
void efi_video_init(void) {
    efi_handle_t *handles;
    efi_uintn_t num_handles;
    video_mode_t *best;
    efi_status_t ret;

    /* Look for a graphics output handle. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &graphics_output_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Just use the first handle. */
    ret = efi_open_protocol(
        handles[0], &graphics_output_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
        (void **)&graphics_output);
    free(handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Save original mode to be restored if we exit. */
    original_mode = graphics_output->mode->mode;

    /* Get information on all available modes. */
    best = NULL;
    for (efi_uint32_t i = 0; i < graphics_output->mode->max_mode; i++) {
        efi_graphics_output_mode_information_t *info;
        efi_uintn_t size;
        efi_video_mode_t *mode;

        ret = efi_call(graphics_output->query_mode, graphics_output, i, &size, &info);
        if (ret != STATUS_SUCCESS)
            continue;

        mode = malloc(sizeof(*mode));
        mode->num = i;
        mode->mode.type = VIDEO_MODE_LFB;
        mode->mode.ops = &efi_video_ops;
        mode->mode.width = info->horizontal_resolution;
        mode->mode.height = info->vertical_resolution;

        mode->mode.format.bpp = get_mode_bpp(info);
        if (!mode->mode.format.bpp || mode->mode.format.bpp & 0x3) {
            free(mode);
            continue;
        }

        mode->mode.pitch = info->pixels_per_scanline * (mode->mode.format.bpp >> 3);
        mode->mode.format.alpha_size = mode->mode.format.alpha_pos = 0;

        switch (info->pixel_format) {
        case EFI_PIXEL_FORMAT_RGBR8:
            mode->mode.format.red_size = mode->mode.format.green_size = mode->mode.format.blue_size = 8;
            mode->mode.format.red_pos = 0;
            mode->mode.format.green_pos = 8;
            mode->mode.format.blue_pos = 16;
            break;
        case EFI_PIXEL_FORMAT_BGRR8:
            mode->mode.format.red_size = mode->mode.format.green_size = mode->mode.format.blue_size = 8;
            mode->mode.format.red_pos = 16;
            mode->mode.format.green_pos = 8;
            mode->mode.format.blue_pos = 0;
            break;
        case EFI_PIXEL_FORMAT_BITMASK:
            get_component_size_pos(
                info->pixel_bitmask.red_mask,
                &mode->mode.format.red_size,
                &mode->mode.format.red_pos);
            get_component_size_pos(
                info->pixel_bitmask.green_mask,
                &mode->mode.format.green_size,
                &mode->mode.format.green_pos);
            get_component_size_pos(
                info->pixel_bitmask.blue_mask,
                &mode->mode.format.blue_size,
                &mode->mode.format.blue_pos);
            break;
        default:
            break;
        }

        /* If the current mode width is less than 1024, we try to set 1024x768,
         * else we just keep the current. */
        if (!best) {
            best = &mode->mode;
        } else if (i == graphics_output->mode->mode) {
            if (mode->mode.width >= 1024)
                best = &mode->mode;
        } else if (mode->mode.width == 1024 && mode->mode.height == 768) {
            if (best->width < 1024 ||
                (best->width == 1024 && best->height == 768 && mode->mode.format.bpp > best->format.bpp))
            {
                best = &mode->mode;
            }
        }

        video_mode_register(&mode->mode, false);
    }

    video_set_mode(best, true);
}

/** Reset video mode to original state. */
void efi_video_reset(void) {
    if (graphics_output) {
        video_set_mode(NULL, false);
        efi_call(graphics_output->set_mode, graphics_output, original_mode);
    }
}
