/*
 * Copyright (C) 2010-2015 Alex Smith
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
 * @brief               PXE filesystem support.
 */

#include <bios/bios.h>
#include <bios/disk.h>
#include <bios/pxe.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <device.h>
#include <endian.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** PXE entry point. */
uint32_t pxe_entry_point;

/** PXE filesystem information. */
typedef struct pxe_device {
    net_device_t net;                   /**< Network device header. */
    fs_mount_t mount;                   /**< Mount header. */
} pxe_device_t;

/** Structure containing details of a PXE handle. */
typedef struct pxe_handle {
    fs_handle_t handle;                 /**< Handle to the file. */

    uint16_t packet_size;               /**< Negotiated packet size. */
    uint32_t packet_number;             /**< Current packet number. */
    char path[];                        /**< Path to the file. */
} pxe_handle_t;

/** Current PXE handle. */
static pxe_handle_t *current_pxe_handle = NULL;

/** Call a PXE function.
 * @param func          Function to call.
 * @param linear        Linear address of data argument.
 * @return              Return code from call. */
static uint16_t pxe_call(uint16_t func, void *linear) {
    return bios_pxe_call(func, linear_to_segoff((ptr_t)linear));
}

/** Get identification information for a PXE device.
 * @param net           Device to identify.
 * @param type          Type of the information to get.
 * @param buf           Where to store identification string.
 * @param size          Size of the buffer. */
static void pxe_net_identify(net_device_t *net, device_identify_t type, char *buf, size_t size) {
    if (type == DEVICE_IDENTIFY_SHORT)
        snprintf(buf, size, "PXE network device");
}

/** PXE network device operations. */
static net_ops_t pxe_net_ops = {
    .identify = pxe_net_identify,
};

/** Set the current PXE handle.
 * @param handle        Handle to set to. If NULL, current will be closed.
 * @return              Status code describing the result of the operation. */
static status_t set_current_handle(pxe_handle_t *handle) {
    if (current_pxe_handle) {
        pxenv_tftp_close_t close;

        pxe_call(PXENV_TFTP_CLOSE, &close);
        current_pxe_handle = NULL;
    }

    if (handle) {
        pxe_device_t *device = container_of(handle->handle.mount, pxe_device_t, mount);
        pxenv_tftp_open_t open;

        strcpy((char *)open.filename, handle->path);
        memcpy(&open.server_ip, &device->net.server_ip, sizeof(open.server_ip));
        memcpy(&open.gateway_ip, &device->net.gateway_ip, sizeof(open.gateway_ip));
        open.udp_port = cpu_to_be16(device->net.server_port);
        open.packet_size = PXENV_TFTP_PACKET_SIZE;

        if (pxe_call(PXENV_TFTP_OPEN, &open) != PXENV_EXIT_SUCCESS || open.status) {
            if (open.status == PXENV_STATUS_TFTP_NOT_FOUND) {
                return STATUS_NOT_FOUND;
            } else {
                dprintf("pxe: open request for '%s' failed: 0x%x\n", handle->path, open.status);
                return STATUS_DEVICE_ERROR;
            }
        }

        handle->packet_size = open.packet_size;
        handle->packet_number = 0;

        current_pxe_handle = handle;
    }

    return STATUS_SUCCESS;
}

/** Read the next packet from a TFTP file.
 * @note                Reads to BIOS_MEM_BASE.
 * @param handle        Handle to read from.
 * @return              Status code describing the result of the operation. */
static status_t read_packet(pxe_handle_t *handle) {
    pxenv_tftp_read_t read;

    read.buffer = BIOS_MEM_BASE;
    read.buffer_size = handle->packet_size;

    if (pxe_call(PXENV_TFTP_READ, &read) != PXENV_EXIT_SUCCESS || read.status) {
        dprintf("pxe: reading packet %u in '%s' failed: 0x%x\n", handle->packet_number, handle->path, read.status);
        return STATUS_DEVICE_ERROR;
    }

    handle->packet_number++;
    return STATUS_SUCCESS;
}

/** Read from a file.
 * @param _handle       Handle to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset to read from.
 * @return              Status code describing the result of the operation. */
static status_t pxe_fs_read(fs_handle_t *_handle, void *buf, size_t count, offset_t offset) {
    pxe_handle_t *handle = container_of(_handle, pxe_handle_t, handle);
    uint32_t start;
    status_t ret;

    start = offset / handle->packet_size;

    /* If the file is not already open, just open it - we will be at the
     * beginning of the file. If it is open, and the current packet is greater
     * than the start packet, we must re-open it. */
    if (handle != current_pxe_handle || handle->packet_number > start) {
        ret = set_current_handle(handle);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    while (count) {
        uint32_t current = handle->packet_number;

        ret = read_packet(handle);
        if (ret != STATUS_SUCCESS)
            return ret;

        /* If the current packet number is less than the start packet, do
         * nothing - we're seeking to the start packet. */
        if (current >= start) {
            uint32_t size = min(count, handle->packet_size - (offset % handle->packet_size));

            memcpy(buf, (void *)(ptr_t)(BIOS_MEM_BASE + (offset % handle->packet_size)), size);
            buf += size;
            offset += size;
            count -= size;
        }
    }

    return STATUS_SUCCESS;
}

/** Open a path on the filesystem.
 * @param mount         Mount to open from.
 * @param path          Path to file/directory to open (can be modified).
 * @param from          Handle on this FS to open relative to.
 * @param _handle       Where to store pointer to opened handle.
 * @return              Status code describing the result of the operation. */
static status_t pxe_fs_open_path(fs_mount_t *mount, char *path, fs_handle_t *from, fs_handle_t **_handle) {
    pxe_device_t *device = container_of(mount, pxe_device_t, mount);
    pxenv_tftp_get_fsize_t fsize;
    pxe_handle_t *handle;
    size_t len;
    status_t ret;

    if (from)
        return STATUS_NOT_SUPPORTED;

    len = strlen(path);
    if (len >= PXENV_TFTP_PATH_SIZE)
        return STATUS_NOT_FOUND;

    /* Get the file size. I'm not actually sure whether it's necessary to close
     * the current handle beforehand, but I'm doing it to be on the safe side. */
    set_current_handle(NULL);
    strcpy((char *)fsize.filename, path);
    memcpy(&fsize.server_ip, &device->net.server_ip, sizeof(fsize.server_ip));
    memcpy(&fsize.gateway_ip, &device->net.gateway_ip, sizeof(fsize.gateway_ip));

    if (pxe_call(PXENV_TFTP_GET_FSIZE, &fsize) != PXENV_EXIT_SUCCESS || fsize.status) {
        if (fsize.status == PXENV_STATUS_TFTP_NOT_FOUND) {
            return STATUS_NOT_FOUND;
        } else {
            dprintf("pxe: file size request for '%s' failed: 0x%x\n", path, fsize.status);
            return STATUS_DEVICE_ERROR;
        }
    }

    handle = malloc(sizeof(*handle) + len + 1);
    handle->handle.mount = mount;
    handle->handle.type = FILE_TYPE_REGULAR;
    handle->handle.size = fsize.file_size;
    handle->handle.count = 1;
    strcpy(handle->path, path);

    /* Try to open the file as the current. */
    ret = set_current_handle(handle);
    if (ret != STATUS_SUCCESS) {
        free(handle);
        return ret;
    }

    *_handle = &handle->handle;
    return STATUS_SUCCESS;
}

/** Close a handle.
 * @param _handle       Handle to close. */
static void pxe_fs_close(fs_handle_t *_handle) {
    pxe_handle_t *handle = container_of(_handle, pxe_handle_t, handle);

    if (handle == current_pxe_handle)
        set_current_handle(NULL);
}

/** PXE filesystem operations structure. */
static fs_ops_t pxe_fs_ops = {
    .name = "PXE/TFTP",
    .read = pxe_fs_read,
    .open_path = pxe_fs_open_path,
    .close = pxe_fs_close,
};

/** Get the PXE entry point address.
 * @return              Whether the entry point could be found. */
static bool get_entry_point(void) {
    bios_regs_t regs;
    pxenv_t *pxenv;
    pxe_t *pxe;

    /* Use the PXE installation check function. */
    bios_regs_init(&regs);
    regs.eax = INT1A_PXE_INSTALL_CHECK;
    bios_call(0x1a, &regs);
    if (regs.eax != INT1A_PXE_INSTALL_CHECK_RET || regs.eflags & X86_FLAGS_CF) {
        dprintf("pxe: loaded via PXE but PXE not available\n");
        return false;
    }

    /* Get the PXENV+ structure. */
    pxenv = (pxenv_t *)segoff_to_linear((regs.es << 16) | regs.bx);
    if (memcmp(pxenv->signature, PXENV_SIGNATURE, sizeof(pxenv->signature)) != 0) {
        boot_error("PXENV+ structure has incorrect signature");
    } else if (!checksum_range(pxenv, pxenv->length)) {
        boot_error("PXENV+ structure is corrupt");
    }

    /* Get the !PXE structure. */
    pxe = (pxe_t *)segoff_to_linear(pxenv->pxe_ptr);
    if (memcmp(pxe->signature, PXE_SIGNATURE, sizeof(pxe->signature)) != 0) {
        boot_error("!PXE structure has incorrect signature");
    } else if (!checksum_range(pxe, pxe->length)) {
        boot_error("!PXE structure is corrupt");
    }

    pxe_entry_point = pxe->entry_point_16;
    return true;
}

/** Get the BOOTP reply packet.
 * @return              Pointer to BOOTP reply packet. */
static bootp_packet_t *get_bootp_packet(void) {
    pxenv_get_cached_info_t ci;

    /* Obtain the BOOTP reply packet. */
    ci.packet_type = PXENV_PACKET_TYPE_CACHED_REPLY;
    ci.buffer = 0;
    ci.buffer_size = 0;
    if (pxe_call(PXENV_GET_CACHED_INFO, &ci) != PXENV_EXIT_SUCCESS)
        boot_error("Failed to get PXE BOOTP packet (0x%x)", ci.status);

    return (bootp_packet_t *)segoff_to_linear(ci.buffer);
}

/** Shut down PXE before booting an OS. */
static void shutdown_pxe(void) {
    if (pxe_call(PXENV_UNDI_SHUTDOWN, (void *)BIOS_MEM_BASE) != PXENV_EXIT_SUCCESS)
        dprintf("pxe: warning: PXENV_UNDI_SHUTDOWN failed\n");
    if (pxe_call(PXENV_UNLOAD_STACK, (void *)BIOS_MEM_BASE) != PXENV_EXIT_SUCCESS)
        dprintf("pxe: warning: PXENV_UNLOAD_STACK failed\n");
    if (pxe_call(PXENV_STOP_UNDI, (void *)BIOS_MEM_BASE) != PXENV_EXIT_SUCCESS)
        dprintf("pxe: warning: PXENV_STOP_UNDI failed\n");
}

/** Initialize PXE. */
void pxe_init(void) {
    bootp_packet_t *bootp;
    pxe_device_t *pxe;

    /* Boot device is set to 0x7f by the PXE boot sector. */
    if (bios_boot_device != 0x7f)
        return;

    if (!get_entry_point())
        return;

    dprintf(
        "pxe: booting via PXE, entry point at %04x:%04x (%p)\n",
        pxe_entry_point >> 16, pxe_entry_point & 0xffff, segoff_to_linear(pxe_entry_point));

    /* When using PXE, 0x8d000 onwards is reserved for use by the PXE stack so
     * we need to mark it as internal to ensure we don't load anything there.
     * Also reserve a bit more because the PXE ROM on one of my test machines
     * appears to take a dump over memory below there as well. */
    memory_protect(0x80000, 0x1f000);

    /* Obtain the BOOTP reply packet. */
    bootp = get_bootp_packet();

    /* Create a device. */
    pxe = malloc(sizeof(*pxe));
    memset(pxe, 0, sizeof(*pxe));
    pxe->net.ops = &pxe_net_ops;
    pxe->net.server_port = PXENV_TFTP_PORT;
    pxe->mount.device = &pxe->net.device;
    pxe->mount.ops = &pxe_fs_ops;
    net_device_register_with_bootp(&pxe->net, bootp, true);
    pxe->net.device.mount = &pxe->mount;

    /* Register a pre-boot hook to shut down the PXE stack. */
    loader_register_preboot_hook(shutdown_pxe);
}

