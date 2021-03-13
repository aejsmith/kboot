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
 * @brief               BCM283x VideoCore firmware interface.
 */

#ifndef __DRIVERS_PLATFORM_BCM283X_FIRMWARE_H
#define __DRIVERS_PLATFORM_BCM283X_FIRMWARE_H

#include <drivers/platform/bcm283x/mbox.h>

/** Property interface message header. */
typedef struct bcm283x_firmware_message_header {
    uint32_t size;              /**< Total buffer size. */
    uint32_t code;              /**< Request/response code. */
} bcm283x_firmware_message_header_t;

/** Property interface message footer. */
typedef struct bcm283x_firmware_message_footer {
    uint32_t end;               /**< End tag (0). */
} bcm283x_firmware_message_footer_t;

/** Initialize a property request message. */
#define BCM283X_FIRMWARE_MESSAGE_INIT(msg)  \
    do { \
        static_assert(__alignof__(msg) == 16); \
        (msg).header.size = sizeof(msg); \
        (msg).header.code = 0; \
        (msg).footer.end = 0; \
    } while (0)

/** Property request status codes. */
#define BCM283X_FIRMWARE_STATUS_SUCCESS     0x80000000
#define BCM283X_FIRMWARE_STATUS_FAILURE     0x80000001

/** Property interface tag header. */
typedef struct bcm283x_firmware_tag_header {
    uint32_t id;                /**< Tag identifier. */
    uint32_t buf_size;          /**< Total size of tag buffer. */
    uint32_t req_size;          /**< Size of request data. */
} bcm283x_firmware_tag_header_t;

/** Initialize a tag. */
#define BCM283X_FIRMWARE_TAG_INIT(tag, _id) \
    do { \
        (tag).header.id = (_id); \
        (tag).header.buf_size = sizeof((tag)) - sizeof((tag).header); \
        (tag).header.req_size = sizeof((tag).req); \
    } while (0)

extern bool bcm283x_firmware_request(bcm283x_mbox_t *mbox, void *buffer);

extern bcm283x_mbox_t *bcm283x_firmware_get(uint32_t phandle);

#endif /* __DRIVERS_PLATFORM_BCM283X_FIRMWARE_H */
