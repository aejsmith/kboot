/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief               Status code definitions.
 */

#ifndef __STATUS_H
#define __STATUS_H

/** Definitions of status codes returned by loader functions. */
typedef enum status {
    STATUS_SUCCESS,                 /**< Operation completed successfully. */
    STATUS_NOT_SUPPORTED,           /**< Operation not supported. */
    STATUS_UNKNOWN_CMD,             /**< Unknown command. */
    STATUS_INVALID_ARG,             /**< Invalid argument. */
    STATUS_TIMED_OUT,               /**< Timed out while waiting. */
    STATUS_NO_MEMORY,               /**< Out of memory. */
    STATUS_NOT_DIR,                 /**< Path component is not a directory. */
    STATUS_NOT_FILE,                /**< Path does not refer to a regular file. */
    STATUS_NOT_FOUND,               /**< Requested object could not be found. */
    STATUS_UNKNOWN_FS,              /**< Filesystem on device is unknown. */
    STATUS_CORRUPT_FS,              /**< Corruption detected on the filesystem. */
    STATUS_FS_FULL,                 /**< No space is available on the filesystem. */
    STATUS_READ_ONLY,               /**< Filesystem is read only. */
    STATUS_END_OF_FILE,             /**< Attempted to read beyond end of file. */
    STATUS_SYMLINK_LIMIT,           /**< Exceeded nested symbolic link limit. */
    STATUS_DEVICE_ERROR,            /**< An error occurred during a hardware operation. */
    STATUS_UNKNOWN_IMAGE,           /**< Executable image has an unrecognised format. */
    STATUS_MALFORMED_IMAGE,         /**< Executable image format is incorrect. */
    STATUS_SYSTEM_ERROR,            /**< Other error from system firmware. */
} status_t;

#endif /* __STATUS_H */
