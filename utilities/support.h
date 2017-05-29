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
 * @brief               Userspace utility helper functions.
 */

#ifndef KBOOT_SUPPORT_H
#define KBOOT_SUPPORT_H

#if defined(__APPLE__)
#   include <libproc.h>
#elif defined(__FreeBSD__)
#   include <sys/types.h>
#   include <sys/sysctl.h>
#elif defined(__linux__)
#   include <sys/sysmacros.h>
#   include <sys/stat.h>
#   include <mntent.h>
#endif

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Helper definitions.
 */

/** Get the number of elements in an array. */
#define array_size(a)   (sizeof((a)) / sizeof((a)[0]))

/**
 * OS dependent functions.
 */

/** Get the path to directory containing the program.
 * @param argv0         Program path.
 * @return              Pointer to malloc()'d path string, or NULL on error. */
static inline char *os_get_program_dir(const char *argv0) {
    char buf[PATH_MAX];
    ssize_t len = 0;
    char *dir;

    #if defined(__linux__)
        len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    #elif defined(__APPLE__)
        len = proc_pidpath(getpid(), buf, sizeof(buf));
    #elif defined(__FreeBSD__)
        static int name[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };

        len = sizeof(buf);
        sysctl(name, array_size(name), buf, (size_t *)&len, NULL, 0);
    #endif

    if (len <= 0) {
        if (!realpath(argv0, buf))
            return NULL;
    } else {
        buf[len] = 0;
    }

    dir = dirname(buf);
    return strdup(dir);
}

/** Get the device containing a path.
 * @param path          Path to get device for.
 * @param _dev          Where to store path to device.
 * @param _root         Where to store path to root of mount.
 * @return              0 on success, -1 on failure. */
static inline int os_device_from_path(const char *path, char **_dev, char **_root) {
    #ifdef __linux__
        struct stat path_st;
        FILE *stream;
        char *dev = NULL, *root = NULL;
        struct mntent *ent;

        if (stat(path, &path_st) != 0)
            return -1;

        stream = setmntent("/proc/mounts", "r");
        if (!stream)
            return -1;

        while (!dev && (ent = getmntent(stream))) {
            struct stat dev_st;

            if (stat(ent->mnt_fsname, &dev_st) != 0) {
                continue;
            } else if (!S_ISBLK(dev_st.st_mode)) {
                continue;
            } else if (dev_st.st_rdev != path_st.st_dev) {
                continue;
            }

            dev = strdup(ent->mnt_fsname);
            root = strdup(ent->mnt_dir);
        }

        endmntent(stream);

        if (!dev) {
            errno = ENOENT;
            return -1;
        }

        *_dev = dev;
        *_root = root;
        return 0;
    #else
        errno = ENOTSUP;
        return -1;
    #endif
}

/** Get the partition number of a device.
 * @param fd            File descriptor to device.
 * @param _part         Where to store partition number (set to 0 if device is
 *                      not a partition).
 * @return              0 on success, -1 on failure. */
static inline int os_get_partition_number(int fd, unsigned *_part) {
    #ifdef __linux__
        struct stat st;
        char buf[PATH_MAX];
        FILE *stream;
        int ret;

        if (fstat(fd, &st))
            return -1;

        snprintf(
            buf, sizeof(buf), "/sys/dev/block/%u:%u/partition",
            major(st.st_rdev), minor(st.st_rdev));

        stream = fopen(buf, "r");
        if (!stream) {
            if (errno == ENOENT) {
                *_part = 0;
                return 0;
            } else {
                return -1;
            }
        }

        ret = fscanf(stream, "%u", _part);
        fclose(stream);
        return (ret == 1) ? 0 : -1;
    #else
        errno = ENOTSUP;
        return -1;
    #endif
}

/** Get the partition offset of a device.
 * @param fd            File descriptor to device.
 * @param _offset       Where to store byte offset of partition (set to 0 if
 *                      device is not a partition).
 * @return              0 on success, -1 on failure. */
static inline int os_get_partition_offset(int fd, uint64_t *_offset) {
    #ifdef __linux__
        struct stat st;
        char buf[PATH_MAX];
        FILE *stream;
        uint64_t offset;
        int ret;

        if (fstat(fd, &st))
            return -1;

        snprintf(
            buf, sizeof(buf), "/sys/dev/block/%u:%u/start",
            major(st.st_rdev), minor(st.st_rdev));

        stream = fopen(buf, "r");
        if (!stream) {
            if (errno == ENOENT) {
                *_offset = 0;
                return 0;
            } else {
                return -1;
            }
        }

        ret = fscanf(stream, "%llu", &offset);
        fclose(stream);
        if (ret != 1)
            return -1;

        /* Linux always counts sector size as 512. */
        *_offset = offset * 512;
        return 0;
    #else
        errno = ENOTSUP;
        return -1;
    #endif
}


/** Get the device containing a partition.
 * @param fd            File descriptor to partition.
 * @param part          Partition number.
 * @param _dev          Where to store path to device.
 * @return              0 on success, -1 on failure. */
static inline int os_get_parent_device(int fd, unsigned part, char **_dev) {
    #ifdef __linux__
        struct stat st;
        char buf[PATH_MAX];
        FILE *stream;
        int ret;
        unsigned parent_major, parent_minor;

        if (fstat(fd, &st))
            return -1;

        if (part) {
            snprintf(
                buf, sizeof(buf), "/sys/dev/block/%u:%u/../dev",
                major(st.st_rdev), minor(st.st_rdev));

            stream = fopen(buf, "r");
            if (!stream)
                return -1;

            ret = fscanf(stream, "%u:%u", &parent_major, &parent_minor);
            fclose(stream);
            if (ret != 2)
                return -1;
        } else {
            parent_major = major(st.st_rdev);
            parent_minor = minor(st.st_rdev);
        }

        snprintf(buf, sizeof(buf), "/dev/block/%u:%u", parent_major, parent_minor);
        *_dev = strdup(buf);
        return 0;
    #else
        errno = ENOTSUP;
        return -1;
    #endif
}

#endif /* KBOOT_SUPPORT_H */
