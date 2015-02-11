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
 * @brief               EFI console functions.
 */

#include <drivers/video/fb.h>

#include <lib/utility.h>

#include <efi/efi.h>

#include <console.h>
#include <loader.h>
#include <memory.h>
#include <video.h>

/** EFI video mode structure. */
typedef struct efi_video_mode {
    video_mode_t mode;                      /**< Video mode structure. */

    efi_graphics_output_protocol_t *gop;    /**< Graphics output protocol. */
    uint32_t num;                           /**< Mode number. */
} efi_video_mode_t;

/** Serial port parameters (match the defaults given in the EFI spec). */
#define SERIAL_BAUD_RATE    115200
#define SERIAL_PARITY       EFI_NO_PARITY
#define SERIAL_DATA_BITS    8
#define SERIAL_STOP_BITS    EFI_ONE_STOP_BIT

/** Serial protocol GUID. */
static efi_guid_t serial_io_guid = EFI_SERIAL_IO_PROTOCOL_GUID;

/** Graphics output protocol GUID. */
static efi_guid_t graphics_output_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

/** Console protocols. */
static efi_simple_text_input_protocol_t *console_in;
static efi_serial_io_protocol_t *serial_io;

/** Saved key press. */
static efi_input_key_t saved_key;

/** Write a character to the serial console.
 * @param ch            Character to write. */
static void efi_serial_putc(char ch) {
    efi_uintn_t size = 1;

    if (ch == '\n')
        efi_serial_putc('\r');

    efi_call(serial_io->write, serial_io, &size, &ch);
}

/** EFI serial console output operations. */
static console_out_ops_t efi_serial_out_ops = {
    .putc = efi_serial_putc,
};

/** Check for a character from the console.
 * @return          Whether a character is available. */
static bool efi_console_poll(void) {
    efi_input_key_t key;
    efi_status_t ret;

    if (saved_key.scan_code || saved_key.unicode_char)
        return true;

    ret = efi_call(console_in->read_key_stroke, console_in, &key);
    if (ret != EFI_SUCCESS)
        return false;

    /* Save the key press to be returned by getc(). */
    saved_key = key;
    return true;
}

/** EFI scan code conversion table. */
static uint16_t efi_scan_codes[] = {
    0,
    CONSOLE_KEY_UP, CONSOLE_KEY_DOWN, CONSOLE_KEY_RIGHT, CONSOLE_KEY_LEFT,
    CONSOLE_KEY_HOME, CONSOLE_KEY_END, 0, 0x7f, 0, 0,
    CONSOLE_KEY_F1, CONSOLE_KEY_F2, CONSOLE_KEY_F3, CONSOLE_KEY_F4,
    CONSOLE_KEY_F5, CONSOLE_KEY_F6, CONSOLE_KEY_F7, CONSOLE_KEY_F8,
    CONSOLE_KEY_F9, CONSOLE_KEY_F10, '\e',
};

/** Read a character from the console.
 * @return              Character read. */
static uint16_t efi_console_getc(void) {
    efi_input_key_t key;
    efi_status_t ret;

    while (true) {
        if (saved_key.scan_code || saved_key.unicode_char) {
            key = saved_key;
            saved_key.scan_code = saved_key.unicode_char = 0;
        } else {
            ret = efi_call(console_in->read_key_stroke, console_in, &key);
            if (ret != EFI_SUCCESS)
                continue;
        }

        if (key.scan_code) {
            if (key.scan_code >= array_size(efi_scan_codes)) {
                continue;
            } else if (!efi_scan_codes[key.scan_code]) {
                continue;
            }

            return efi_scan_codes[key.scan_code];
        } else if (key.unicode_char <= 0x7f) {
            /* Whee, Unicode! */
            return (key.unicode_char == '\r') ? '\n' : key.unicode_char;
        }
    }
}

/** EFI main console input operations. */
static console_in_ops_t efi_console_in_ops = {
    .poll = efi_console_poll,
    .getc = efi_console_getc,
};

/** Initialize the serial console. */
static void efi_serial_init(void) {
    efi_handle_t *handles;
    efi_uintn_t num_handles;
    efi_status_t ret;

    /* Look for a serial console. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &serial_io_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Just use the first handle. */
    ret = efi_open_protocol(handles[0], &serial_io_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void **)&serial_io);
    free(handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Configure the port. */
    ret = efi_call(serial_io->set_attributes,
        serial_io, SERIAL_BAUD_RATE, 0, 0, SERIAL_PARITY, SERIAL_DATA_BITS,
        SERIAL_STOP_BITS);
    if (ret != EFI_SUCCESS)
        return;

    ret = efi_call(serial_io->set_control, serial_io, EFI_SERIAL_DATA_TERMINAL_READY | EFI_SERIAL_REQUEST_TO_SEND);
    if (ret != EFI_SUCCESS)
        return;

    debug_console.out = &efi_serial_out_ops;
}

/** Set an EFI video mode.
 * @param _mode         Mode to set.
 * @return              Status code describing the result of the operation. */
static status_t efi_video_set_mode(video_mode_t *_mode) {
    efi_video_mode_t *mode = (efi_video_mode_t *)_mode;
    efi_status_t ret;

    ret = efi_call(mode->gop->set_mode, mode->gop, mode->num);
    if (ret != EFI_SUCCESS) {
        dprintf("efi: failed to set video mode %u with status 0x%zx\n", mode->num, ret);
        return efi_convert_status(ret);
    }

    /* Get the framebuffer information. */
    mode->mode.mem_phys = mode->gop->mode->frame_buffer_base;
    mode->mode.mem_virt = mode->gop->mode->frame_buffer_base;
    mode->mode.mem_size = mode->gop->mode->frame_buffer_size;

    return STATUS_SUCCESS;
}

/** EFI video operations. */
static video_ops_t efi_video_ops = {
    .console = &fb_console_out_ops,
    .set_mode = efi_video_set_mode,
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

/** Initialize the graphics output protocol. */
static void efi_gop_init(void) {
    efi_handle_t *handles;
    efi_uintn_t num_handles;
    efi_graphics_output_protocol_t *gop;
    video_mode_t *best;
    efi_status_t ret;

    /* Look for a graphics output handle. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &graphics_output_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Just use the first handle. */
    ret = efi_open_protocol(handles[0], &graphics_output_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void **)&gop);
    free(handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Get information on all available modes. */
    best = NULL;
    for (efi_uint32_t i = 0; i < gop->mode->max_mode; i++) {
        efi_graphics_output_mode_information_t *info;
        efi_uintn_t size;
        efi_video_mode_t *mode;

        ret = efi_call(gop->query_mode, gop, i, &size, &info);
        if (ret != STATUS_SUCCESS)
            continue;

        mode = malloc(sizeof(*mode));
        mode->gop = gop;
        mode->num = i;
        mode->mode.type = VIDEO_MODE_LFB;
        mode->mode.ops = &efi_video_ops;
        mode->mode.width = info->horizontal_resolution;
        mode->mode.height = info->vertical_resolution;

        mode->mode.bpp = get_mode_bpp(info);
        if (!mode->mode.bpp || mode->mode.bpp & 0x3)
            continue;

        mode->mode.pitch = info->pixels_per_scanline * (mode->mode.bpp >> 3);

        switch (info->pixel_format) {
        case EFI_PIXEL_FORMAT_RGBR8:
            mode->mode.red_size = mode->mode.green_size = mode->mode.blue_size = 8;
            mode->mode.red_pos = 0;
            mode->mode.green_pos = 8;
            mode->mode.blue_pos = 16;
            break;
        case EFI_PIXEL_FORMAT_BGRR8:
            mode->mode.red_size = mode->mode.green_size = mode->mode.blue_size = 8;
            mode->mode.red_pos = 16;
            mode->mode.green_pos = 8;
            mode->mode.blue_pos = 0;
            break;
        case EFI_PIXEL_FORMAT_BITMASK:
            get_component_size_pos(info->pixel_bitmask.red_mask, &mode->mode.red_size, &mode->mode.red_pos);
            get_component_size_pos(info->pixel_bitmask.green_mask, &mode->mode.green_size, &mode->mode.green_pos);
            get_component_size_pos(info->pixel_bitmask.blue_mask, &mode->mode.blue_size, &mode->mode.blue_pos);
            break;
        default:
            break;
        }

        /* If the current mode width is less than 1024, we try to set 1024x768,
         * else we just keep the current. */
        if (i == gop->mode->mode) {
            if (!best || mode->mode.width >= 1024)
                best = &mode->mode;
        } else if (mode->mode.width == 1024 && mode->mode.height == 768) {
            if (!best || best->width < 1024 || (best->width == 1024 && best->height == 768 && mode->mode.bpp > best->bpp))
                best = &mode->mode;
        }

        video_mode_register(&mode->mode, false);
    }

    video_set_mode(best);
}

/** Initialize the EFI console. */
void efi_console_init(void) {
    /* Initialize a serial console as the debug console if available. */
    efi_serial_init();

    /* Set the main console input. */
    console_in = efi_system_table->con_in;
    main_console.in = &efi_console_in_ops;

    /* Use the framebuffer as the console output. */
    efi_gop_init();
}
