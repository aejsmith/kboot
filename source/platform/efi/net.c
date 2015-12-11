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
 * @brief               EFI network device support.
 *
 * TODO:
 *  - Annoyingly the PXE TFTP API provided by EFI is a regression compared to
 *    legacy PXE: it is only able to transfer a whole file, not packet by
 *    packet. This means we have to read a whole file in and buffer it somewhere
 *    in order to not have terrible performance. In future, it may be a better
 *    solution to implement TFTP ourselves over the UdpRead/UdpWrite functions
 *    provided by the PXE BC protocol.
 */

#include <efi/device.h>
#include <efi/efi.h>
#include <efi/net.h>
#include <efi/services.h>

#include <lib/string.h>

#include <fs.h>
#include <loader.h>
#include <memory.h>

/** EFI PXE network device structure. */
typedef struct efi_net {
    net_device_t net;                   /**< Network device header. */
    fs_mount_t mount;                   /**< Mount header. */

    efi_pxe_base_code_protocol_t *bc;   /**< PXE base code protocol. */
    efi_handle_t handle;                /**< Handle to network device. */
    efi_device_path_t *path;            /**< Device path. */
} efi_net_t;

/** EFI PXE file handle structure. */
typedef struct efi_net_handle {
    fs_handle_t handle;                 /**< Handle to the file. */
    void *data;                         /**< Data for the file. */
    char path[];                        /**< Path to the file. */
} efi_net_handle_t;

/** TFTP port number (hardcoded in EDK, assume it can't be changed at all). */
#define TFTP_PORT 69

/** Simple network protocol GUID. */
static efi_guid_t simple_network_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** PXE base code protocol GUID. */
static efi_guid_t pxe_base_code_guid = EFI_PXE_BASE_CODE_PROTOCOL_GUID;

/** Get identification information for an EFI network device.
 * @param _net          Device to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void efi_net_identify(net_device_t *_net, device_identify_t type, char *buf, size_t size) {
    efi_net_t *net = container_of(_net, efi_net_t, net);

    if (type == DEVICE_IDENTIFY_SHORT)
        snprintf(buf, size, "EFI network device %pE", net->path);
}

/** EFI network device operations. */
static net_ops_t efi_net_ops = {
    .identify = efi_net_identify,
};

/** Read from a file.
 * @param _handle       Handle to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset to read from.
 * @return              Status code describing the result of the operation. */
static status_t efi_net_fs_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    efi_net_handle_t *handle = container_of(_handle, efi_net_handle_t, handle);
    efi_net_t *net = container_of(_handle->mount, efi_net_t, mount);
    void *data;

    /* See the note at the top of the file. EFI only gives us an API to read a
     * whole file. Allocate a buffer for it and read it in, then keep it so we
     * don't have to re-read every read call. This is super nasty... */
    if (!handle->data) {
        efi_uint64_t size;
        efi_status_t ret;

        size = handle->handle.size;

        if (!offset && count == size) {
            /* Assume this is a single read of the whole file. */
            data = buf;
        } else {
            handle->data = malloc_large(size);
            data = handle->data;
        }

        ret = efi_call(net->bc->mtftp,
            net->bc, EFI_PXE_BASE_CODE_TFTP_READ_FILE, data, false, &size, NULL,
            (efi_ip_address_t *)&net->net.server_ip, (efi_char8_t *)handle->path,
            NULL, false);
        if (ret != STATUS_SUCCESS) {
            if (ret == EFI_TFTP_ERROR) {
                uint8_t error = net->bc->mode->tftp_error.error_code;

                dprintf("efi: TFTP error reading '%s': %u\n", handle->path, error);
                return STATUS_DEVICE_ERROR;
            } else {
                dprintf("efi: failed to read '%s': 0x%zx\n", handle->path, ret);
                return efi_convert_status(ret);
            }
        }
    } else {
        data = handle->data;
    }

    if (data != buf)
        memcpy(buf, data + offset, count);

    return STATUS_SUCCESS;
}

/** Open a path on the filesystem.
 * @param mount         Mount to open from.
 * @param path          Path to file/directory to open (can be modified).
 * @param from          Handle on this FS to open relative to.
 * @param _handle       Where to store pointer to opened handle.
 * @return              Status code describing the result of the operation. */
static status_t efi_net_fs_open_path(fs_mount_t *mount, char *path, fs_handle_t *from, fs_handle_t **_handle) {
    efi_net_t *net = container_of(mount, efi_net_t, mount);
    uint64_t size;
    size_t len;
    efi_net_handle_t *handle;
    efi_status_t ret;

    if (from)
        return STATUS_NOT_SUPPORTED;

    /* Get the file size. */
    ret = efi_call(net->bc->mtftp,
        net->bc, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE, NULL, false, &size, NULL,
        (efi_ip_address_t *)&net->net.server_ip, (efi_char8_t *)path,
        NULL, false);
    if (ret != STATUS_SUCCESS) {
        if (ret == EFI_TFTP_ERROR) {
            uint8_t error = net->bc->mode->tftp_error.error_code;

            if (error == 0 || error == 1) {
                return STATUS_NOT_FOUND;
            } else {
                dprintf("efi: TFTP error getting size of '%s': %u\n", path, error);
                return STATUS_DEVICE_ERROR;
            }
        } else {
            dprintf("efi: failed to get size of '%s': 0x%zx\n", path, ret);
            return efi_convert_status(ret);
        }
    }

    len = strlen(path);
    handle = malloc(sizeof(*handle) + len + 1);
    fs_handle_init(&handle->handle, mount, FILE_TYPE_REGULAR, size);
    handle->data = NULL;
    strcpy(handle->path, path);

    *_handle = &handle->handle;
    return STATUS_SUCCESS;
}

/** Close a handle.
 * @param _handle       Handle to close. */
static void efi_net_fs_close(fs_handle_t *_handle) {
    efi_net_handle_t *handle = container_of(_handle, efi_net_handle_t, handle);

    free_large(handle->data);
}

/** EFI network filesystem operations structure. */
static fs_ops_t efi_net_fs_ops = {
    .name = "TFTP",
    .read = efi_net_fs_read,
    .open_path = efi_net_fs_open_path,
    .close = efi_net_fs_close,
};

/** Check if a handle is a network device.
 * @param handle        Handle to check.
 * @return              Whether the handle supports the simple network protocol. */
bool efi_net_is_net_device(efi_handle_t handle) {
    void *protocol;
    efi_status_t ret;

    ret = efi_open_protocol(handle, &simple_network_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, &protocol);
    if (ret != EFI_SUCCESS) {
        ret = efi_open_protocol(handle, &pxe_base_code_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, &protocol);
        if (ret != EFI_SUCCESS)
            return false;
    }

    return true;
}

/** Gets an EFI handle from a network device.
 * @param _net          Network device to get handle for.
 * @return              Handle to disk, or NULL if not found. */
efi_handle_t efi_net_get_handle(net_device_t *_net) {
    efi_net_t *net;

    if (_net->ops != &efi_net_ops)
        return NULL;

    net = (efi_net_t *)_net;
    return net->handle;
}

/** Detect EFI network devices. */
void efi_net_init(void) {
    efi_handle_t *handles __cleanup_free = NULL;
    efi_uintn_t num_handles;
    efi_status_t ret;

    /* Get a list of all handles supporting the PXE base code protocol. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &pxe_base_code_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS)
        return;

    for (efi_uintn_t i = 0; i < num_handles; i++) {
        efi_net_t *net;
        efi_pxe_base_code_mode_t *mode;
        efi_pxe_base_code_packet_t *packet;

        net = malloc(sizeof(*net));
        memset(net, 0, sizeof(*net));
        net->net.ops = &efi_net_ops;
        net->net.server_port = TFTP_PORT;
        net->mount.device = &net->net.device;
        net->mount.ops = &efi_net_fs_ops;
        net->handle = handles[i];

        net->path = efi_get_device_path(handles[i]);
        if (!net->path) {
            free(net);
            continue;
        }

        ret = efi_open_protocol(handles[i], &pxe_base_code_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void **)&net->bc);
        if (ret != EFI_SUCCESS) {
            dprintf("efi: warning: failed to open PXE base code for %pE\n", net->path);
            free(net);
            continue;
        }

        mode = net->bc->mode;

        /* Ignore devices where the BC protocol has not been started. Since we
         * do not have any support for configuring devices, it's not of any use
         * to us. */
        if (!mode->started) {
            free(net);
            continue;
        }

        if (mode->using_ipv6) {
            dprintf("efi: warning: device %pE is using IPv6 which is currently unsupported\n", net->path);
            free(net);
            continue;
        }

        if (mode->pxe_reply_received) {
            packet = &mode->pxe_reply;
        } else if (mode->proxy_offer_received) {
            packet = &mode->proxy_offer;
        } else if (mode->dhcp_ack_received) {
            packet = &mode->dhcp_ack;
        } else {
            /* No configuration information, not useful to us. */
            free(net);
            continue;
        }

        /* Register a device. */
        net_device_register_with_bootp(
            &net->net,
            (bootp_packet_t *)packet,
            handles[i] == efi_loaded_image->device_handle);
        net->net.device.mount = &net->mount;
    }
}
