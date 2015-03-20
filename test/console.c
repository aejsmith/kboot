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

#include <drivers/console/fb.h>
#include <drivers/console/vga.h>

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <memory.h>
#include <test.h>
#include <time.h>
#include <video.h>

/** KBoot log buffer. */
static kboot_log_t *kboot_log = NULL;
static size_t kboot_log_size = 0;

/** Primary console. */
console_t primary_console;

/** Current primary console. */
console_t *current_console = &primary_console;

/** Debug output console. */
console_t *debug_console;

/** Framebuffer video mode information. */
video_mode_t *current_video_mode;

/** Helper for console_vprintf().
 * @param ch            Character to display.
 * @param data          Console to use.
 * @param total         Pointer to total character count. */
void console_vprintf_helper(char ch, void *data, int *total) {
    console_t *console = data;

    console_putc(console, ch);
    *total = *total + 1;
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_vprintf(console_t *console, const char *fmt, va_list args) {
    return do_vprintf(console_vprintf_helper, console, fmt, args);
}

/** Output a formatted message to a console.
 * @param console       Console to print to.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int console_printf(console_t *console, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = console_vprintf(console, fmt, args);
    va_end(args);

    return ret;
}

/** Helper for vprintf().
 * @param ch            Character to display.
 * @param data          Unused.
 * @param total         Pointer to total character count. */
static void vprintf_helper(char ch, void *data, int *total) {
    console_putc(current_console, ch);
    console_putc(debug_console, ch);

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

/** Initialize the primary console.
 * @param tags          Tag list. */
void primary_console_init(kboot_tag_t *tags) {
    kboot_tag_video_t *video;

    log_init(tags);

    while (tags->type != KBOOT_TAG_NONE) {
        if (tags->type == KBOOT_TAG_VIDEO) {
            video = (kboot_tag_video_t *)tags;

            if (video->type == KBOOT_VIDEO_LFB && video->lfb.flags & KBOOT_LFB_RGB) {
                current_video_mode = malloc(sizeof(*current_video_mode));
                current_video_mode->type = VIDEO_MODE_LFB;
                current_video_mode->width = video->lfb.width;
                current_video_mode->height = video->lfb.height;
                current_video_mode->bpp = video->lfb.bpp;
                current_video_mode->pitch = video->lfb.pitch;
                current_video_mode->red_size = video->lfb.red_size;
                current_video_mode->red_pos = video->lfb.red_pos;
                current_video_mode->green_size = video->lfb.green_size;
                current_video_mode->green_pos = video->lfb.green_pos;
                current_video_mode->blue_size = video->lfb.blue_size;
                current_video_mode->blue_pos = video->lfb.blue_pos;
                current_video_mode->mem_phys = video->lfb.fb_phys;
                current_video_mode->mem_virt = video->lfb.fb_virt;
                current_video_mode->mem_size = video->lfb.fb_size;

                primary_console.out = fb_console_create();
                primary_console.out->ops->init(primary_console.out);
            }

            #ifdef CONFIG_ARCH_X86
                if (video->type == KBOOT_VIDEO_VGA) {
                    current_video_mode = malloc(sizeof(*current_video_mode));
                    current_video_mode->type = VIDEO_MODE_VGA;
                    current_video_mode->width = video->vga.cols;
                    current_video_mode->height = video->vga.lines;
                    current_video_mode->x = video->vga.x;
                    current_video_mode->y = video->vga.y;
                    current_video_mode->mem_phys = video->vga.mem_phys;
                    current_video_mode->mem_virt = video->vga.mem_virt;
                    current_video_mode->mem_size = video->vga.mem_size;

                    primary_console.out = vga_console_create();
                    primary_console.out->ops->init(primary_console.out);
                }
            #endif

            break;
        }

        tags = (kboot_tag_t *)round_up((ptr_t)tags + tags->size, 8);
    }
}

/**
 * Compatibility stubs.
 */

void console_register(console_t *console) {
    /* Nothing happens. */
}

mstime_t current_time(void) {
    return 0;
}
