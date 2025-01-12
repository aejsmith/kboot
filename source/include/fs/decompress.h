/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File decompression support.
 */

#ifndef __FS_DECOMPRESS_H
#define __FS_DECOMPRESS_H

#include <fs.h>

extern bool decompress_open(fs_handle_t *source, fs_handle_t **_handle);
extern void decompress_close(fs_handle_t *handle);
extern status_t decompress_read(fs_handle_t *handle, void *buf, uint32_t count, uint32_t offset);

#endif /* __FS_DECOMPRESS_H */
