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
 * @brief               Test kernel console functions.
 */

#include <drivers/video/fb.h>

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <test.h>
#include <video.h>

/** KBoot log buffer. */
static kboot_log_t *kboot_log = NULL;
static size_t kboot_log_size = 0;

/** Framebuffer video mode information. */
static video_mode_t video_mode;

/** Main console. */
console_t main_console;

/** Debug console. */
console_t debug_console;

/** Helper for vprintf().
 * @param ch            Character to display.
 * @param data          Unused.
 * @param total         Pointer to total character count. */
static void vprintf_helper(char ch, void *data, int *total) {
    console_putc(&main_console, ch);
    console_putc(&debug_console, ch);

    if (kboot_log) {
        kboot_log->buffer[(kboot_log->start + kboot_log->length) % kboot_log_size] = ch;
        if (kboot_log->length < kboot_log_size) {
            kboot_log->length++;
        } else {
            kboot_log->start = (kboot_log->start + 1) % kboot_log_size;
        }
    }

    *total = *total + 1;
}

/** Output a formatted message to the console.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int vprintf(const char *fmt, va_list args) {
    return do_vprintf(vprintf_helper, NULL, fmt, args);
}

/** Output a formatted message to the console.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int printf(const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);

    return ret;
}

/** Initialize the log.
 * @param tags          Tag list. */
static void log_init(kboot_tag_t *tags) {
    kboot_tag_log_t *log;

    while (tags->type != KBOOT_TAG_NONE) {
        if (tags->type == KBOOT_TAG_LOG) {
            log = (kboot_tag_log_t *)tags;
            kboot_log = (kboot_log_t *)((ptr_t)log->log_virt);
            kboot_log_size = log->log_size - sizeof(kboot_log_t);
            break;
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }
}

/** Raise an internal error.
 * @param fmt           Error format string.
 * @param ...           Values to substitute into format. */
void __noreturn internal_error(const char *fmt, ...) {
    va_list args;

    printf("Internal Error: ");

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");

    while (true)
        arch_pause();
}

/** Initialize the console.
 * @param tags          Tag list. */
void console_init(kboot_tag_t *tags) {
    kboot_tag_video_t *video;

    log_init(tags);

    while (tags->type != KBOOT_TAG_NONE) {
        if (tags->type == KBOOT_TAG_VIDEO) {
            video = (kboot_tag_video_t *)tags;

            if (video->type == KBOOT_VIDEO_LFB && video->lfb.flags & KBOOT_LFB_RGB) {
                video_mode.type = VIDEO_MODE_LFB;
                video_mode.width = video->lfb.width;
                video_mode.height = video->lfb.height;
                video_mode.bpp = video->lfb.bpp;
                video_mode.pitch = video->lfb.pitch;
                video_mode.red_size = video->lfb.red_size;
                video_mode.red_pos = video->lfb.red_pos;
                video_mode.green_size = video->lfb.green_size;
                video_mode.green_pos = video->lfb.green_pos;
                video_mode.blue_size = video->lfb.blue_size;
                video_mode.blue_pos = video->lfb.blue_pos;
                video_mode.mem_phys = video->lfb.fb_phys;
                video_mode.mem_virt = video->lfb.fb_virt;
                video_mode.mem_size = video->lfb.fb_size;

                main_console.out_private = fb_console_out_ops.init(&video_mode);
                main_console.out = &fb_console_out_ops;
            }

            break;
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }
}
