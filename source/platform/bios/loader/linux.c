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
 * @brief               BIOS platform Linux loader.
 */

#include <x86/linux.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <bios/bios.h>
#include <bios/console.h>
#include <bios/memory.h>

#include <loader.h>
#include <memory.h>

extern void linux_platform_enter(ptr_t entry, linux_params_t *params) __noreturn;

/** Check for platform-specific requirements.
 * @param loader        Loader internal data.
 * @param header        Kernel image header.
 * @return              Whether the kernel image is valid. */
bool linux_platform_check(linux_loader_t *loader, linux_header_t *header) {
    return true;
}

/** Get memory information.
 * @param params        Kernel parameters structure. */
static void get_memory_info(linux_params_t *params) {
    void *buf __cleanup_free;
    size_t num_entries, entry_size;

    /* Add memory ranges. */
    params->e820_entries = 0;
    bios_memory_get_mmap(&buf, &num_entries, &entry_size);
    for (size_t i = 0; i < num_entries; i++) {
        e820_entry_t *entry = buf + (i * entry_size);

        if (entry->start <= 0x100000 && entry->start + entry->length > 0x100000) {
            uint64_t upper = (entry->start + entry->length - 0x100000) / 1024;

            params->alt_mem_k = min(upper, 0xffffffff);
            params->screen_info.ext_mem_k = min(upper, 0xfc00);
        }

        if (params->e820_entries < array_size(params->e820_map)) {
            memcpy(&params->e820_map[params->e820_entries], entry, sizeof(params->e820_map[0]));
            params->e820_entries++;
        }
    }
}

/** Get APM BIOS information.
 * @param params        Kernel parameters structure. */
static void get_apm_info(linux_params_t *params) {
    bios_regs_t regs;

    bios_regs_init(&regs);
    regs.eax = 0x5300;
    bios_call(0x15, &regs);
    if (regs.eflags & X86_FLAGS_CF || regs.bx != 0x504d || !(regs.cx & (1 << 1)))
        return;

    /* Connect 32-bit interface. */
    regs.eax = 0x5304;
    bios_call(0x15, &regs);
    regs.eax = 0x5303;
    bios_call(0x15, &regs);
    if (regs.eflags & X86_FLAGS_CF)
        return;

    params->apm_bios_info.cseg = regs.ax;
    params->apm_bios_info.offset = regs.ebx;
    params->apm_bios_info.cseg_16 = regs.cx;
    params->apm_bios_info.dseg = regs.dx;
    params->apm_bios_info.cseg_len = regs.si;
    params->apm_bios_info.cseg_16_len = regs.esi >> 16;
    params->apm_bios_info.dseg_len = regs.di;

    regs.eax = 0x5300;
    bios_call(0x15, &regs);
    if (regs.eflags & X86_FLAGS_CF || regs.bx != 0x504d) {
        /* Failed to connect 32-bit interface, disconnect. */
        regs.eax = 0x5304;
        bios_call(0x15, &regs);
        return;
    }

    params->apm_bios_info.version = regs.ax;
    params->apm_bios_info.flags = regs.cx;
}

/** Get Intel SpeedStep BIOS information.
 * @param params        Kernel parameters structure. */
static void get_ist_info(linux_params_t *params) {
    bios_regs_t regs;

    bios_regs_init(&regs);
    regs.eax = 0xe980;
    regs.edx = 0x47534943;
    bios_call(0x15, &regs);

    params->ist_info.signature = regs.eax;
    params->ist_info.command = regs.ebx;
    params->ist_info.event = regs.ecx;
    params->ist_info.perf_level = regs.edx;
}

/** Get video mode information for the kernel.
 * @param loader        Loader internal data.
 * @param params        Kernel parameters structure. */
static void get_video_info(linux_loader_t *loader, linux_params_t *params) {
    video_mode_t *mode = loader->video;
    bios_regs_t regs;

    memset(&params->screen_info, 0, sizeof(params->screen_info));

    switch (mode->type) {
    case VIDEO_MODE_VGA:
        params->screen_info.orig_video_isVGA = LINUX_VIDEO_TYPE_VGA;
        params->screen_info.orig_video_cols = mode->width;
        params->screen_info.orig_video_lines = mode->height;
        params->screen_info.orig_x = mode->x;
        params->screen_info.orig_y = mode->y;

        /* Font height from BIOS data area. */
        params->screen_info.orig_video_points = *(uint16_t *)0x485;

        bios_regs_init(&regs);
        regs.ax = 0x1200;
        regs.bx = 0x10;
        bios_call(0x10, &regs);
        params->screen_info.orig_video_ega_bx = regs.bx;
        break;
    case VIDEO_MODE_LFB:
        params->screen_info.orig_video_isVGA = LINUX_VIDEO_TYPE_VESA;
        params->screen_info.lfb_width = mode->width;
        params->screen_info.lfb_height = mode->height;
        params->screen_info.lfb_depth = mode->format.bpp;
        params->screen_info.lfb_linelength = mode->pitch;
        params->screen_info.lfb_base = mode->mem_phys;
        params->screen_info.lfb_size = round_up(mode->pitch * mode->height, 65536) >> 16;
        params->screen_info.red_size = mode->format.red_size;
        params->screen_info.red_pos = mode->format.red_pos;
        params->screen_info.green_size = mode->format.green_size;
        params->screen_info.green_pos = mode->format.green_pos;
        params->screen_info.blue_size = mode->format.blue_size;
        params->screen_info.blue_pos = mode->format.blue_pos;
        break;
    }

    bios_regs_init(&regs);
    regs.ax = 0xf00;
    bios_call(0x10, &regs);
    params->screen_info.orig_video_mode = regs.ax & 0x7f;
    params->screen_info.orig_video_page = regs.bx >> 8;
}

/** Enter a Linux kernel.
 * @param loader        Loader internal data.
 * @param params        Kernel parameters structure. */
void linux_platform_load(linux_loader_t *loader, linux_params_t *params) {
    get_memory_info(params);
    get_apm_info(params);
    get_ist_info(params);
    get_video_info(loader, params);

    /* Don't bother with EDD and MCA, AFAIK they're not used. */

    /* Start the kernel. */
    dprintf("linux: kernel entry point at 0x%x, params at %p\n", params->hdr.code32_start, params);
    linux_platform_enter(params->hdr.code32_start, params);
}
