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
 * @brief               Device Tree (DT) support.
 *
 * This provides some common functionality for platforms which use a Device
 * Tree. It is in generic code rather than the DT platform, since other
 * platforms might make use of a DT in some way (e.g. EFI on ARM).
 */

#ifndef __DT_H
#define __DT_H

#ifdef CONFIG_TARGET_HAS_FDT

#include <libfdt.h>

extern const void *fdt_address;

extern bool dt_get_reg(int node_offset, int index, phys_ptr_t *_address, phys_size_t *_size);
extern bool dt_is_compatible(int node_offset, const char **strings, size_t count);

extern void dt_init(void *fdt);

#endif /* CONFIG_TARGET_HAS_FDT */
#endif /* __DT_H */
