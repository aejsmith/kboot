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

#include <lib/string.h>

#include <loader.h>
#include <memory.h>
#include <net.h>

/** Next network device ID. */
static unsigned next_net_id;

/** Get network device identification information.
 * @param device        Device to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void net_device_identify(device_t *device, device_identify_t type, char *buf, size_t size) {
    net_device_t *net = (net_device_t *)device;

    if (type == DEVICE_IDENTIFY_LONG) {
        size_t ret;

        ret = snprintf(buf, size, "client MAC = %pM\n", net->hw_addr);

        if (net->flags & NET_DEVICE_IPV6) {
            ret += snprintf(buf + ret, size - ret,
                "client IP  = %pI6\n"
                "gateway IP = %pI6\n"
                "server IP  = %pI6\n",
                &net->ip.v6, &net->gateway_ip.v6, &net->server_ip.v6);
        } else {
            ret += snprintf(buf + ret, size - ret,
                "client IP  = %pI4\n"
                "gateway IP = %pI4\n"
                "server IP  = %pI4\n",
                &net->ip.v4, &net->gateway_ip.v4, &net->server_ip.v4);
        }

        if (net->server_port)
            ret += snprintf(buf + ret, size - ret, "port       = %u\n", net->server_port);

        buf += ret;
        size -= ret;
    }

    if (net->ops->identify)
        net->ops->identify(net, type, buf, size);
}

/** Network device operations. */
static device_ops_t net_device_ops = {
    .identify = net_device_identify,
};

/** Register a network device.
 * @param net           Device to register (fields marked in structure should
 *                      be initialized).
 * @param boot          Whether the device is the boot device. */
void net_device_register(net_device_t *net, bool boot) {
    char *name;

    /* Assign an ID for the device and name it. */
    net->id = next_net_id++;
    name = malloc(6);
    snprintf(name, 6, "net%u", net->id);

    /* Add the device. */
    net->device.type = DEVICE_TYPE_NET;
    net->device.ops = &net_device_ops;
    net->device.name = name;
    device_register(&net->device);

    if (boot)
        boot_device = &net->device;
}

/**
 * Register a network device with BOOTP information.
 *
 * Fill in a network device's configuration information from a BOOTP reply
 * packet and register it.
 *
 * @param net           Device to register.
 * @param packet        BOOTP reply packet.
 * @param boot          Whether the device is the boot device.
 */
void net_device_register_with_bootp(net_device_t *net, bootp_packet_t *packet, bool boot) {
    net->flags = 0;

    memcpy(&net->ip.v4, &packet->your_ip, sizeof(net->ip.v4));
    memcpy(&net->gateway_ip.v4, &packet->gateway_ip, sizeof(net->gateway_ip.v4));
    memcpy(&net->server_ip.v4, &packet->server_ip, sizeof(net->server_ip.v4));
    memcpy(net->hw_addr, packet->client_addr, packet->hardware_len);
    net->hw_type = packet->hardware;
    net->hw_addr_size = packet->hardware_len;

    net_device_register(net, boot);

    dprintf("net: registered %s with configuration:\n", net->device.name);
    dprintf(" client IP:  %pI4\n", &net->ip.v4);
    dprintf(" gateway IP: %pI4\n", &net->gateway_ip.v4);
    dprintf(" server IP:  %pI4\n", &net->server_ip.v4);
    dprintf(" client MAC: %pM\n", net->hw_addr);
}
