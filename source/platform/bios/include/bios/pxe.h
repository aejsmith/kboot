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
 * @brief               PXE API definitions.
 */

#ifndef __BIOS_PXE_H
#define __BIOS_PXE_H

#include <net.h>

/** BIOS interrupt 1A function definitions. */
#define INT1A_PXE_INSTALL_CHECK         0x5650  /**< PXE installation check. */
#define INT1A_PXE_INSTALL_CHECK_RET     0x564e  /**< PXE installation check result. */

/** PXE function numbers. */
#define PXENV_UNDI_SHUTDOWN             0x05    /**< Reset the network adapter. */
#define PXENV_STOP_UNDI                 0x15    /**< Shutdown the UNDI stack. */
#define PXENV_TFTP_OPEN                 0x20    /**< Open TFTP connection. */
#define PXENV_TFTP_CLOSE                0x21    /**< Close TFTP connection. */
#define PXENV_TFTP_READ                 0x22    /**< Read from TFTP connection. */
#define PXENV_TFTP_GET_FSIZE            0x25    /**< Get TFTP file size. */
#define PXENV_UNLOAD_STACK              0x70    /**< Unload PXE stack. */
#define PXENV_GET_CACHED_INFO           0x71    /**< Get cached information. */

/** Packet types for PXENV_GET_CACHED_INFO. */
#define PXENV_PACKET_TYPE_DHCP_DISCOVER 1       /**< Get DHCPDISCOVER packet. */
#define PXENV_PACKET_TYPE_DHCP_ACK      2       /**< Get DHCPACK packet. */
#define PXENV_PACKET_TYPE_CACHED_REPLY  3       /**< Get DHCP reply packet. */

/** Return codes from PXE calls. */
#define PXENV_EXIT_SUCCESS              0
#define PXENV_EXIT_FAILURE              1

/** PXE status codes (subset). */
#define PXENV_STATUS_SUCCESS            0x0
#define PXENV_STATUS_TFTP_NOT_FOUND     0x3b

/** TFTP definitions. */
#define PXENV_TFTP_PORT                 69      /**< Port number. */
#define PXENV_TFTP_PACKET_SIZE          512     /**< Requested packet size. */
#define PXENV_TFTP_PATH_SIZE            128     /**< Size of the file path buffers. */

/** Type of a MAC address. */
typedef uint8_t pxe_mac_addr_t[16];

/** Type of a PXENV status code. */
typedef uint16_t pxenv_status_t;

/** PXENV+ structure. */
typedef struct pxenv {
    uint8_t signature[6];                       /**< Signature. */
    uint16_t version;                           /**< API version number. */
    uint8_t length;                             /**< Length of the structure. */
    uint8_t checksum;                           /**< Checksum. */
    uint32_t rm_entry;                          /**< Real mode entry point. */
    uint32_t pm_entry;                          /**< Protected mode entry point. */
    uint16_t pm_selector;                       /**< Protected mode segment selector. */
    uint16_t stack_seg;                         /**< Stack segment. */
    uint16_t stack_size;                        /**< Stack segment size. */
    uint16_t bc_code_seg;                       /**< BC code segment. */
    uint16_t bc_code_size;                      /**< BC code segment size. */
    uint16_t bc_data_seg;                       /**< BC data segment. */
    uint16_t bc_data_size;                      /**< BC data segment size. */
    uint16_t undi_code_seg;                     /**< UNDI code segment. */
    uint16_t undi_code_size;                    /**< UNDI code segment size. */
    uint16_t undi_data_seg;                     /**< UNDI data segment. */
    uint16_t undi_data_size;                    /**< UNDI data segment size. */
    uint32_t pxe_ptr;                           /**< Pointer to !PXE structure. */
} __packed pxenv_t;

/** PXENV+ structure signature. */
#define PXENV_SIGNATURE                 "PXENV+"

/** !PXE structure. */
typedef struct pxe {
    uint8_t signature[4];                       /**< Signature. */
    uint8_t length;                             /**< Structure length. */
    uint8_t checksum;                           /**< Checksum. */
    uint8_t revision;                           /**< Structure revision. */
    uint8_t reserved1;                          /**< Reserved. */
    uint32_t undi_rom_id;                       /**< Address of UNDI ROM ID structure. */
    uint32_t base_rom_id;                       /**< Address of BC ROM ID structure. */
    uint32_t entry_point_16;                    /**< Entry point for 16-bit stack segment. */
    uint32_t entry_point_32;                    /**< Entry point for 32-bit stack segment. */
    uint32_t status_callout;                    /**< Status call-out function. */
    uint8_t reserved2;                          /**< Reserved. */
    uint8_t seg_desc_count;                     /**< Number of segment descriptors. */
    uint16_t first_selector;                    /**< First segment selector. */
    uint8_t segments[56];                       /**< Segment information. */
} __packed pxe_t;

/** !PXE structure signature. */
#define PXE_SIGNATURE                 "!PXE"

/** Input structure for PXENV_TFTP_OPEN. */
typedef struct pxenv_tftp_open {
    pxenv_status_t status;                      /**< Status code. */
    ipv4_addr_t server_ip;                      /**< Server IP address. */
    ipv4_addr_t gateway_ip;                     /**< Gateway IP address. */
    uint8_t filename[PXENV_TFTP_PATH_SIZE];     /**< File name to open. */
    uint16_t udp_port;                          /**< Port that TFTP server is listening on. */
    uint16_t packet_size;                       /**< Requested packet size. */
} __packed pxenv_tftp_open_t;

/** Input structure for PXENV_TFTP_CLOSE. */
typedef struct pxenv_tftp_close {
    pxenv_status_t status;                      /**< Status code. */
} __packed pxenv_tftp_close_t;

/** Input structure for PXENV_TFTP_READ. */
typedef struct pxenv_tftp_read {
    pxenv_status_t status;                      /**< Status code. */
    uint16_t packet_number;                     /**< Packet number sent by server. */
    uint16_t buffer_size;                       /**< Number of bytes read. */
    uint32_t buffer;                            /**< Buffer address. */
} __packed pxenv_tftp_read_t;

/** Input structure for PXENV_TFTP_GET_FSIZE. */
typedef struct pxenv_tftp_get_fsize {
    pxenv_status_t status;                      /**< Status code. */
    ipv4_addr_t server_ip;                      /**< Server IP address. */
    ipv4_addr_t gateway_ip;                     /**< Gateway IP address. */
    uint8_t filename[PXENV_TFTP_PATH_SIZE];     /**< File name to open. */
    uint32_t file_size;                         /**< Size of the file. */
} __packed pxenv_tftp_get_fsize_t;

/** Input structure for PXENV_GET_CACHED_INFO. */
typedef struct pxenv_get_cached_info {
    pxenv_status_t status;                      /**< Status code. */
    uint16_t packet_type;                       /**< Requested packet. */
    uint16_t buffer_size;                       /**< Size of output buffer. */
    uint32_t buffer;                            /**< Buffer address. */
    uint16_t buffer_limit;                      /**< Maximum size of buffer. */
} __packed pxenv_get_cached_info_t;

extern uint32_t pxe_entry_point;

extern void pxe_init(void);

#endif /* __BIOS_PXE_H */
