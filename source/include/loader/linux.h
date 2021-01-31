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
 * @brief               Linux kernel loader.
 */

#ifndef __LOADER_LINUX_H
#define __LOADER_LINUX_H

#include <config.h>
#include <fs.h>
#include <video.h>

/** Linux loader internal data. */
typedef struct linux_loader {
    fs_handle_t *kernel;                /**< Kernel image handle. */
    list_t initrds;                     /**< Initrd file list. */
    offset_t initrd_size;               /**< Combined initrd size. */
    char *cmdline;                      /**< Kernel command line (path + arguments). */
    char *path;                         /**< Separated path string. */
    value_t args;                       /**< Value for editing kernel arguments. */
    video_mode_t *video;                /**< Video mode set by linux_video_set(). */
} linux_loader_t;

/** Linux initrd structure. */
typedef struct linux_initrd {
    list_t header;                      /**< Link to initrd list. */
    fs_handle_t *handle;                /**< Handle to initrd. */
} linux_initrd_t;

extern bool linux_arch_check(linux_loader_t *loader);
extern void linux_arch_load(linux_loader_t *loader) __noreturn;

extern void linux_initrd_load(linux_loader_t *loader, void *addr);

#ifdef CONFIG_TARGET_HAS_VIDEO

/** Set the video mode for a Linux kernel.
 * @param loader            Loader internal data. */
static inline void linux_video_set(linux_loader_t *loader) {
    loader->video = video_env_set(current_environ, "video_mode");
}

#endif /* CONFIG_TARGET_HAS_VIDEO */

#endif /* __LOADER_LINUX_H */
