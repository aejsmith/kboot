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
 * @brief               File decompression support.
 *
 * This file implements support for transparent decompression of gzip-compressed
 * files. We use the miniz/tinfl library for decompressing the DEFLATE stream.
 * Note that we do not support files with a decompressed size greater than 4GB,
 * as the ISIZE field in the gzip header is 32 bits and defined to be the
 * decompressed size mod 2^32. Since we rely on being able to get the total
 * size of a file in various places, we cannot correctly handle files that are
 * larger than 4GB unless we decompress the entire file when opening it to get
 * its size.
 *
 * We use a bunch of global state for decompression, as tinfl requires a large
 * (32K) temporary buffer to work on. Rather than allocate this per-handle,
 * just keep it globally. This means we have to re-decompress when switching
 * between files, but the typical access pattern in the loader is to work on a
 * whole file in one go.
 */

#include <fs/decompress.h>

#include <lib/string.h>
#include <lib/tinfl.h>
#include <lib/utility.h>

#include <assert.h>
#include <endian.h>
#include <memory.h>
#include <fs.h>
#include <loader.h>

/** Fixed part of the header of a gzip file. */
typedef struct gzip_header {
    uint8_t magic[2];                   /**< Magic number. */
    uint8_t method;                     /**< Compression method. */
    uint8_t flags;                      /**< Flags. */
    uint32_t time;                      /**< Modification time. */
    uint8_t xflags;                     /**< Extra flags. */
    uint8_t os;                         /**< OS type. */
} __packed gzip_header_t;

/** Magic numbers for a gzip file. */
#define GZIP_MAGIC0             0x1f
#define GZIP_MAGIC1             0x8b

/** Flags in a gzip header. */
#define GZIP_ASCII              (1<<0)
#define GZIP_HEADER_CRC         (1<<1)
#define GZIP_EXTRA_FIELD        (1<<2)
#define GZIP_ORIG_NAME          (1<<3)
#define GZIP_COMMENT            (1<<4)
#define GZIP_ENCRYPTED          (1<<5)

/** Compression methods. */
#define GZIP_METHOD_DEFLATE     8

/** Maximum header size. */
#define MAX_HEADER_SIZE         512

/** Size of the dictionary buffer. */
#define DICT_BUFFER_SIZE        TINFL_LZ_DICT_SIZE

/** Size of the payload buffer. */
#define PAYLOAD_BUFFER_SIZE     4096

/** Decompression wrapper handle structure. */
typedef struct decompress_handle {
    fs_handle_t handle;                 /**< Handle header structure. */

    fs_handle_t *source;                /**< Source handle. */
    uint32_t payload_start;             /**< Start of the payload in the file. */
    uint32_t payload_size;              /**< Total payload size. */
} decompress_handle_t;

/** Handle current decompression state refers to. */
static decompress_handle_t *current_decompress_handle;

/** Current decompression state. */
static uint32_t payload_offset;         /**< Current offset in the payload. */
static uint32_t dict_offset;            /**< Current offset in dictionary buffer. */
static uint32_t dict_avail;             /**< Available data in dictionary buffer. */
static uint32_t output_offset;          /**< Current offset in the output file. */
static tinfl_decompressor decompressor; /**< Decompression state. */

/** Temporary payload input buffer. */
static uint8_t payload_buffer[PAYLOAD_BUFFER_SIZE] __aligned(8);

/** Temporary buffer to decompress to, tinfl requires a large buffer. */
static uint8_t dict_buffer[DICT_BUFFER_SIZE];

/** Skip a variable-length field in the gzip header.
 * @param handle        Compressed file handle.
 * @return              Whether successfully skipped. */
static inline bool skip_variable_field(decompress_handle_t *handle) {
    do {
        if (handle->payload_start >= MAX_HEADER_SIZE) {
            dprintf("fs: warning: gzip header is too large\n");
            return false;
        }
    } while (payload_buffer[handle->payload_start++]);

    return true;
}

/** Open a handle for decompression.
 * @param source        Handle to source file.
 * @param _handle       Where to store pointer to decompression wrapper handle.
 * @return              Whether this is a compressed file. */
bool decompress_open(fs_handle_t *source, fs_handle_t **_handle) {
    gzip_header_t *header;
    decompress_handle_t *handle;
    uint32_t size;
    status_t ret;

    assert(source->type == FILE_TYPE_REGULAR);

    /* We're about to trash the temporary buffer. */
    current_decompress_handle = NULL;

    /* Read in a large chunk to identify the file. We do this because the header
     * is variable length so we cannot read just a fixed length, and on disk
     * devices reads will always be at least 512 bytes (the block size), so
     * reading byte by byte would be terribly inefficient. */
    ret = fs_read(source, payload_buffer, min(source->size, MAX_HEADER_SIZE), 0);
    if (ret != STATUS_SUCCESS)
        return false;

    /* Check if this is a gzip header. */
    header = (gzip_header_t *)payload_buffer;
    if (header->magic[0] != GZIP_MAGIC0 || header->magic[1] != GZIP_MAGIC1) {
        return false;
    } else if (header->method != GZIP_METHOD_DEFLATE) {
        dprintf("fs: warning: cannot handle gzip compression method %u\n", header->method);
        return false;
    } else if (header->flags & GZIP_ENCRYPTED) {
        dprintf("fs: warning: cannot handle encrypted gzip files\n");
        return false;
    }

    handle = malloc(sizeof(*handle));
    handle->source = source;

    /* Find the beginning of the payload in the file. */
    handle->payload_start = sizeof(gzip_header_t);

    if (header->flags & GZIP_EXTRA_FIELD) {
        uint16_t xlen;

        /* Read length. */
        ret = fs_read(source, &xlen, sizeof(xlen), handle->payload_start);
        if (ret != STATUS_SUCCESS)
            goto err_free;

        handle->payload_start += 2 + le16_to_cpu(xlen);
    }

    if (header->flags & GZIP_ORIG_NAME) {
        if (!skip_variable_field(handle))
            goto err_free;
    }

    if (header->flags & GZIP_COMMENT) {
        if (!skip_variable_field(handle))
            goto err_free;
    }

    if (header->flags & GZIP_HEADER_CRC)
        handle->payload_start += 2;

    /* There is a CRC32 and size at the end of the payload. */
    handle->payload_size = source->size - handle->payload_start - 8;

    /* Read in the decompressed file size. */
    ret = fs_read(source, &size, sizeof(size), source->size - 4);
    if (ret != STATUS_SUCCESS)
        goto err_free;

    size = le32_to_cpu(size);

    fs_handle_init(&handle->handle, source->mount, FILE_TYPE_REGULAR, size);
    handle->handle.flags |= FS_HANDLE_COMPRESSED;

    *_handle = &handle->handle;
    return true;

err_free:
    free(handle);
    return false;
}

/** Free decompression state for a file.
 * @param _handle       Handle to close. */
void decompress_close(fs_handle_t *_handle) {
    decompress_handle_t *handle = (decompress_handle_t *)_handle;

    if (handle == current_decompress_handle)
        current_decompress_handle = NULL;

    fs_close(handle->source);
}

/** Read from a compressed file.
 * @param _handle       Handle to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset to read from.
 * @return              Status code describing the result of the operation. */
status_t decompress_read(fs_handle_t *_handle, void *buf, uint32_t count, uint32_t offset) {
    decompress_handle_t *handle = (decompress_handle_t *)_handle;

    if (current_decompress_handle != handle || offset < output_offset) {
        /* Seek back to the beginning. */
        payload_offset = dict_offset = dict_avail = output_offset = 0;
        tinfl_init(&decompressor);
    }

    while (true) {
        uint32_t skip, size;
        size_t out_size, in_size;
        tinfl_status status;
        status_t ret;

        /* Return available data. Do this first in the loop in case we have any
         * remaining data left from a previous call. */
        if (dict_avail) {
            skip = min(dict_avail, offset - output_offset);
            size = min(dict_avail - skip, count);

            if (size) {
                memcpy(buf, &dict_buffer[dict_offset + skip], size);
                buf += size;
                offset += size;
                count -= size;
            }

            dict_offset = (dict_offset + skip + size) % DICT_BUFFER_SIZE;
            output_offset += skip + size;
            dict_avail -= skip + size;
        }

        if (!count)
            break;

        assert(payload_offset < handle->payload_size);

        /* Calculate size we have available in buffers. */
        out_size = DICT_BUFFER_SIZE - dict_offset;
        in_size = min(
            handle->payload_size - payload_offset,
            PAYLOAD_BUFFER_SIZE - (payload_offset % PAYLOAD_BUFFER_SIZE));

        /* Need to read more data if we're on an input block boundary. FIXME:
         * Could make this more efficient, since the payload start is likely
         * not on a disk block boundary this is probably doing some partial
         * block reads. */
        if (!(payload_offset % PAYLOAD_BUFFER_SIZE)) {
            ret = fs_read(handle->source, payload_buffer, in_size, handle->payload_start + payload_offset);
            if (ret != STATUS_SUCCESS)
                return ret;
        }

        /* Decompress the data. */
        status = tinfl_decompress(
            &decompressor, &payload_buffer[payload_offset % PAYLOAD_BUFFER_SIZE],
            &in_size, dict_buffer, &dict_buffer[dict_offset], &out_size,
            (in_size < handle->payload_size - payload_offset) ? TINFL_FLAG_HAS_MORE_INPUT : 0);
        if (status < TINFL_STATUS_DONE) {
            dprintf("fs: warning: error %d decompressing data\n", status);

            /* Don't know what state things are in, reset everything. */
            current_decompress_handle = NULL;
            return STATUS_DEVICE_ERROR;
        }

        payload_offset += in_size;
        dict_avail = out_size;
    }

    return STATUS_SUCCESS;
}
