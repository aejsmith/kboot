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
 * @brief               BCM283x firmware-based video driver.
 */

#include <drivers/platform/bcm283x/firmware.h>
#include <drivers/platform/bcm283x/memory.h>

#include <dt.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

typedef struct bcm283x_video {
    video_mode_t mode;

    bcm283x_mbox_t *mbox;
} bcm283x_video_t;

#define TAG_ALLOCATE_BUFFER     0x00040001
#define TAG_GET_PHYSICAL_SIZE   0x00040003
#define TAG_SET_PHYSICAL_SIZE   0x00048003
#define TAG_GET_VIRTUAL_SIZE    0x00040004
#define TAG_SET_VIRTUAL_SIZE    0x00048004
#define TAG_GET_DEPTH           0x00040005
#define TAG_SET_DEPTH           0x00048005
#define TAG_GET_PIXEL_ORDER     0x00040006
#define TAG_SET_PIXEL_ORDER     0x00048006
#define TAG_GET_ALPHA_MODE      0x00040007
#define TAG_SET_ALPHA_MODE      0x00048007
#define TAG_GET_PITCH           0x00040008
#define TAG_GET_VIRTUAL_OFFSET  0x00040009
#define TAG_SET_VIRTUAL_OFFSET  0x00048009

typedef struct tag_allocate_buffer {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {
            uint32_t alignment;     /**< Buffer alignment. */
        } req;
        struct {
            uint32_t address;       /**< Framebuffer address. */
            uint32_t size;          /**< Framebuffer size. */
        } resp;
    };
} tag_allocate_buffer_t;

typedef struct tag_get_size {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {} req;
        struct {
            uint32_t width;         /**< Display width. */
            uint32_t height;        /**< Display height. */
        } resp;
    };
} tag_get_size_t;

typedef struct tag_set_size {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {
            uint32_t width;         /**< Display width. */
            uint32_t height;        /**< Display height. */
        } req;
        struct {
            uint32_t width;         /**< Display width. */
            uint32_t height;        /**< Display height. */
        } resp;
    };
} tag_set_size_t;

typedef struct tag_get_depth {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {} req;
        struct {
            uint32_t depth;         /**< Display depth. */
        } resp;
    };
} tag_get_depth_t;

typedef struct tag_set_depth {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {
            uint32_t depth;         /**< Display depth. */
        } req;
        struct {
            uint32_t depth;         /**< Display depth. */
        } resp;
    };
} tag_set_depth_t;

typedef struct tag_get_pixel_order {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {} req;
        struct {
            uint32_t state;         /**< Pixel order. */
        } resp;
    };
} tag_get_pixel_order_t;

typedef struct tag_set_pixel_order {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {
            uint32_t state;         /**< Pixel order. */
        } req;
        struct {
            uint32_t state;         /**< Pixel order. */
        } resp;
    };
} tag_set_pixel_order_t;

typedef struct tag_get_alpha_mode {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {} req;
        struct {
            uint32_t state;         /**< Alpha mode. */
        } resp;
    };
} tag_get_alpha_mode_t;

typedef struct tag_set_alpha_mode {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {
            uint32_t state;         /**< Alpha mode. */
        } req;
        struct {
            uint32_t state;         /**< Alpha mode. */
        } resp;
    };
} tag_set_alpha_mode_t;

typedef struct tag_get_pitch {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {} req;
        struct {
            uint32_t pitch;         /**< Pitch. */
        } resp;
    };
} tag_get_pitch_t;

typedef struct tag_get_offset {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {} req;
        struct {
            uint32_t x;             /**< X offset in pixels. */
            uint32_t y;             /**< Y offset in pixels. */
        } resp;
    };
} tag_get_offset_t;

typedef struct tag_set_offset {
    bcm283x_firmware_tag_header_t header;
    union {
        struct {
            uint32_t x;             /**< X offset in pixels. */
            uint32_t y;             /**< Y offset in pixels. */
        } req;
        struct {
            uint32_t x;             /**< X offset in pixels. */
            uint32_t y;             /**< Y offset in pixels. */
        } resp;
    };
} tag_set_offset_t;

typedef struct message_get_config {
    bcm283x_firmware_message_header_t header;
    tag_get_size_t phys;
    tag_get_size_t virt;
    tag_get_depth_t depth;
    tag_get_pixel_order_t order;
    bcm283x_firmware_message_footer_t footer;
} __aligned(16) message_get_config_t;

typedef struct message_set_config {
    bcm283x_firmware_message_header_t header;
    tag_set_size_t size;
    tag_set_offset_t offset;
    tag_set_alpha_mode_t alpha;
    tag_allocate_buffer_t allocate;
    tag_get_pitch_t pitch;
    bcm283x_firmware_message_footer_t footer;
} __aligned(16) message_set_config_t;

static void bcm283x_video_set_mode(video_mode_t *mode) {
    /* We only have one mode, do nothing. */
}

static video_ops_t bcm283x_video_ops = {
    .set_mode = bcm283x_video_set_mode,
};

static void calculate_pixel_format(pixel_format_t *format, uint32_t depth, uint32_t order) {
    format->bpp = depth;

    /* We disable alpha. */
    format->alpha_pos  = 0;
    format->alpha_size = 0;

    switch (depth) {
    case 15:
        format->red_pos   = 10; format->red_size   = 5;
        format->green_pos = 5;  format->green_size = 5;
        format->blue_pos  = 0;  format->blue_size  = 5;
        break;
    case 16:
        format->red_pos   = 11; format->red_size   = 5;
        format->green_pos = 5;  format->green_size = 6;
        format->blue_pos  = 0;  format->blue_size  = 5;
        break;
    case 24:
    case 32:
        format->red_pos   = 16; format->red_size   = 8;
        format->green_pos = 8;  format->green_size = 8;
        format->blue_pos  = 0;  format->blue_size  = 8;
        break;
    }

    if (order == 1) {
        swap(format->red_pos, format->blue_pos);
        swap(format->red_size, format->blue_size);
    }
}

static status_t bcm283x_video_init(dt_device_t *device) {
    uint32_t firmware_handle;
    if (!dt_get_prop_u32(device->node_offset, "brcm,firmware", &firmware_handle))
        return STATUS_INVALID_ARG;

    bcm283x_mbox_t *mbox = bcm283x_firmware_get(firmware_handle);
    if (!mbox)
        return STATUS_INVALID_ARG;

    bcm283x_video_t *video = malloc(sizeof(*video));

    video->mode.type = VIDEO_MODE_LFB;
    video->mode.ops  = &bcm283x_video_ops;
    video->mbox      = mbox;

	/* Query the current configuration. */
    message_get_config_t get_config;
	BCM283X_FIRMWARE_MESSAGE_INIT(get_config);
    BCM283X_FIRMWARE_TAG_INIT(get_config.phys, TAG_GET_PHYSICAL_SIZE);
    BCM283X_FIRMWARE_TAG_INIT(get_config.virt, TAG_GET_VIRTUAL_SIZE);
    BCM283X_FIRMWARE_TAG_INIT(get_config.depth, TAG_GET_DEPTH);
    BCM283X_FIRMWARE_TAG_INIT(get_config.order, TAG_GET_PIXEL_ORDER);
	if (!bcm283x_firmware_request(video->mbox, &get_config)) {
        dprintf("bcm283x: video: failed to get current configuration\n");
        free(video);
        return STATUS_DEVICE_ERROR;
    }

    video->mode.width  = get_config.phys.resp.width;
    video->mode.height = get_config.phys.resp.height;

    calculate_pixel_format(&video->mode.format, get_config.depth.resp.depth, get_config.order.resp.state);

	dprintf(
        "bcm283x: video: display configuration is %" PRIu32 "x%" PRIu32 "x%" PRIu8 " (virtual: %" PRIu32 "x%" PRIu32 ")\n",
        video->mode.width, video->mode.height, video->mode.format.bpp,
        get_config.virt.resp.width, get_config.virt.resp.height);

    /*
     * Set up a configuration.
     *
     * We're gonna assume that the firmware has left us with a physical size
     * matching the display, and set the virtual size (actual output signal) to
     * that. The behaviour of virtual at startup seems to differ between Pi
     * versions, the Pi 4 has virtual matching the display size while the Pi 3
     * has it as 2x2...
     *
     * Also disable alpha - the firmware could theoretically currently have it
     * in a mode that the OS might not expect (there is an inverted alpha mode).
     */
    message_set_config_t set_config;
	BCM283X_FIRMWARE_MESSAGE_INIT(set_config);
    BCM283X_FIRMWARE_TAG_INIT(set_config.size, TAG_SET_VIRTUAL_SIZE);
    BCM283X_FIRMWARE_TAG_INIT(set_config.offset, TAG_SET_VIRTUAL_OFFSET);
    BCM283X_FIRMWARE_TAG_INIT(set_config.alpha, TAG_SET_ALPHA_MODE);
    BCM283X_FIRMWARE_TAG_INIT(set_config.allocate, TAG_ALLOCATE_BUFFER);
    BCM283X_FIRMWARE_TAG_INIT(set_config.pitch, TAG_GET_PITCH);
    set_config.size.req.width  = video->mode.width;
    set_config.size.req.height = video->mode.height;
    set_config.offset.req.x    = 0;
    set_config.offset.req.y    = 0;
    set_config.alpha.req.state = 0x2;
    set_config.allocate.req.alignment = PAGE_SIZE;
    if (!bcm283x_firmware_request(video->mbox, &set_config)) {
        dprintf("bcm283x: video: failed to set configuration\n");
        free(video);
        return STATUS_DEVICE_ERROR;
    }

    video->mode.pitch    = set_config.pitch.resp.pitch;
    video->mode.mem_phys = bcm283x_bus_to_phys(set_config.allocate.resp.address);
    video->mode.mem_virt = phys_to_virt(video->mode.mem_phys);
    video->mode.mem_size = set_config.allocate.resp.size;

	dprintf(
        "bcm283x: video: framebuffer is at 0x%" PRIxPHYS " (size: 0x%" PRIx32 ", pitch: %" PRIu32 ")\n",
        video->mode.mem_phys, video->mode.mem_size, video->mode.pitch);

    video_mode_register(&video->mode, true);

    device->private = video;
    return STATUS_SUCCESS;
}

static const char *bcm283x_video_match[] = {
    "raspberrypi,rpi-firmware-kms",
    "raspberrypi,rpi-firmware-kms-2711",
};

BUILTIN_DT_DRIVER(bcm283x_video_driver) = {
    .matches = DT_MATCH_TABLE(bcm283x_video_match),

    /* We force use of this - this can be disabled in the DT depending on which
     * video driver Linux is configured to use, but this is the only option we
     * support. */
    .ignore_status = true,

    .init = bcm283x_video_init,
};
