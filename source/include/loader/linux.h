/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
    loader->video = video_env_set(current_environ, VIDEO_MODE_ENV, false);
}

#endif /* CONFIG_TARGET_HAS_VIDEO */

#endif /* __LOADER_LINUX_H */
