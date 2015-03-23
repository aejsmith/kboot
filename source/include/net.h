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
 * @brief               Network device support.
 */

#ifndef __NET_H
#define __NET_H

#include <device.h>

struct net_device;

/** Type used to store a MAC address. */
typedef uint8_t mac_addr_t[16];

/** Type used to store an IPv4 address. */
typedef union ipv4_addr {
    uint32_t val;
    uint8_t bytes[4];
} ipv4_addr_t;

/** Type used to store an IPv6 address. */
typedef union ipv6_addr {
    struct {
        uint64_t high;
        uint64_t low;
    } val;
    uint8_t bytes[16];
} ipv6_addr_t;

/** Type used to store an IP address. */
typedef union ip_addr {
    ipv4_addr_t v4;                     /**< IPv4 address. */
    ipv6_addr_t v6;                     /**< IPv6 address. */
} ip_addr_t;

/** Network device operations structure. */
typedef struct net_ops {
    /** Get identification information for the device.
     * @param net           Device to identify.
     * @param type          Type of the information to get.
     * @param buf           Where to store identification string.
     * @param size          Size of the buffer. */
    void (*identify)(struct net_device *net, device_identify_t type, char *buf, size_t size);
} net_ops_t;

/** Network device information. */
typedef struct net_device {
    device_t device;                    /**< Device header. */

    /** Fields which should be initialized before registering. */
    const net_ops_t *ops;               /**< Network device operations. */
    uint32_t flags;                     /**< Behaviour flags. */
    ip_addr_t ip;                       /**< IP address configured for the device. */
    ip_addr_t gateway_ip;               /**< Gateway IP address. */
    uint8_t hw_type;                    /**< Hardware type (according to RFC 1700). */
    mac_addr_t hw_addr;                 /**< MAC address of the device. */
    uint8_t hw_addr_size;               /**< Hardware address size (in bytes). */
    ip_addr_t server_ip;                /**< Server IP address. */
    uint16_t server_port;               /**< UDP port number of TFTP server. */

    /** Fields set internally. */
    unsigned id;                        /**< ID of the device. */
} net_device_t;

/** Network device flags. */
#define NET_DEVICE_IPV6         (1<<0)  /**< Device is configured using IPv6. */

/** Type used to store a MAC address in the BOOTP packet. */
typedef uint8_t bootp_mac_addr_t[16];

/** BOOTP packet structure. */
typedef struct bootp_packet {
    uint8_t opcode;                     /**< Message opcode. */
    uint8_t hardware;                   /**< Hardware type. */
    uint8_t hardware_len;               /**< Hardware address length. */
    uint8_t gate_hops;                  /**< Set to 0. */
    uint32_t ident;                     /**< Random number chosen by client. */
    uint16_t seconds;                   /**< Seconds since obtained address. */
    uint16_t flags;                     /**< BOOTP/DHCP flags. */
    ipv4_addr_t client_ip;              /**< Client IP. */
    ipv4_addr_t your_ip;                /**< Your IP. */
    ipv4_addr_t server_ip;              /**< Server IP. */
    ipv4_addr_t gateway_ip;             /**< Gateway IP. */
    bootp_mac_addr_t client_addr;       /**< Client hardware address. */
    uint8_t server_name[64];            /**< Server host name. */
    uint8_t boot_file[128];             /**< Boot file name. */
    uint8_t vendor[64];                 /**< DHCP vendor options. */
} __packed bootp_packet_t;

#ifdef CONFIG_TARGET_HAS_NET

extern void net_device_register(net_device_t *net, bool boot);
extern void net_device_register_with_bootp(net_device_t *net, bootp_packet_t *packet, bool boot);

#endif /* CONFIG_TARGET_HAS_NET */
#endif /* __NET_H */
