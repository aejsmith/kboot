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
 * @brief               VBE definitions.
 */

#ifndef __BIOS_VBE_H
#define __BIOS_VBE_H

#include <types.h>

/** VBE information structure. */
typedef struct vbe_info {
    char vbe_signature[4];
    uint8_t vbe_version_minor;
    uint8_t vbe_version_major;
    uint32_t oem_string_ptr;
    uint32_t capabilities;
    uint32_t video_mode_ptr;
    uint16_t total_memory;
    uint16_t oem_software_rev;
    uint32_t oem_vendor_name_ptr;
    uint32_t oem_product_name_ptr;
    uint32_t oem_product_rev_ptr;
    uint8_t reserved[222];
    uint8_t oem_data[256];
} __packed vbe_info_t;

/** VBE mode information structure. */
typedef struct vbe_mode_info {
    uint16_t mode_attributes;
    uint8_t wina_attributes;
    uint8_t winb_attributes;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t wina_segment;
    uint16_t winb_segment;
    uint32_t win_func_ptr;
    uint16_t bytes_per_scan_line;

    /* VBE 1.2 */
    uint16_t x_resolution;
    uint16_t y_resolution;
    uint8_t x_char_size;
    uint8_t y_char_size;
    uint8_t num_planes;
    uint8_t bits_per_pixel;
    uint8_t num_banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t num_image_pages;
    uint8_t reserved1;

    /* Direct colour fields */
    uint8_t red_mask_size;
    uint8_t red_field_position;
    uint8_t green_mask_size;
    uint8_t green_field_position;
    uint8_t blue_mask_size;
    uint8_t blue_field_position;
    uint8_t rsvd_mask_size;
    uint8_t rsvd_field_position;
    uint8_t direct_color_mode_info;

    /* VBE 2.0 */
    uint32_t phys_base_ptr;
    uint32_t reserved2;
    uint16_t reserved3;

    /* VBE 3.0 */
    uint16_t lin_bytes_per_scan_line;
    uint8_t bnk_num_image_pages;
    uint8_t lin_num_image_pages;
    uint8_t lin_red_mask_size;
    uint8_t lin_red_field_position;
    uint8_t lin_green_mask_size;
    uint8_t lin_green_field_position;
    uint8_t lin_blue_mask_size;
    uint8_t lin_blue_field_position;
    uint8_t lin_rsvd_mask_size;
    uint8_t lin_rsvd_field_position;
    uint8_t max_pixel_clock;

    uint8_t reserved4[189];
} __packed vbe_mode_info_t;

/** VBE signature. */
#define VBE_SIGNATURE                   "VBE2"

/** VBE mode number bits. */
#define VBE_MODE_LFB                    (1<<14) /**< Use linear framebuffer model. */

/** VBE mode memory model values. */
#define VBE_MEMORY_MODEL_TEXT           0       /**< Text mode. */
#define VBE_MEMORY_MODEL_DIRECT_COLOUR  6       /**< Direct colour. */

/** VBE function definitions. */
#define VBE_FUNCTION_CONTROLLER_INFO    0x4f00  /**< Return VBE Controller Information. */
#define VBE_FUNCTION_MODE_INFO          0x4f01  /**< Return VBE Mode Information. */
#define VBE_FUNCTION_SET_MODE           0x4f02  /**< Set VBE Mode. */

#endif /* __BIOS_VBE_H */
