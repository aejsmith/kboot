/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Status code definitions.
 */

#ifndef __STATUS_H
#define __STATUS_H

/**
 * Status codes.
 *
 * Status codes used for reporting errors. When updating, be sure to update
 * the error string table in lib/printf.c.
 */
typedef enum status {
    STATUS_SUCCESS,                 /**< Operation completed successfully. */
    STATUS_NOT_SUPPORTED,           /**< Operation not supported. */
    STATUS_INVALID_ARG,             /**< Invalid argument. */
    STATUS_TIMED_OUT,               /**< Timed out while waiting. */
    STATUS_NO_MEMORY,               /**< Out of memory. */
    STATUS_NOT_DIR,                 /**< Not a directory. */
    STATUS_NOT_FILE,                /**< Not a regular file. */
    STATUS_NOT_FOUND,               /**< Not found. */
    STATUS_UNKNOWN_FS,              /**< Filesystem on device is unknown. */
    STATUS_CORRUPT_FS,              /**< Corruption detected on the filesystem. */
    STATUS_READ_ONLY,               /**< Filesystem is read only. */
    STATUS_END_OF_FILE,             /**< Read beyond end of file. */
    STATUS_SYMLINK_LIMIT,           /**< Exceeded nested symbolic link limit. */
    STATUS_DEVICE_ERROR,            /**< Device error. */
    STATUS_UNKNOWN_IMAGE,           /**< Image has an unrecognised format. */
    STATUS_MALFORMED_IMAGE,         /**< Image format is incorrect. */
    STATUS_SYSTEM_ERROR,            /**< Error from system firmware. */
} status_t;

#endif /* __STATUS_H */
