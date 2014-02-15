/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief		Console functions.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <types.h>

/** Console output operations structure. */
typedef struct console_out_ops {
	/** Reset the console to a default state. */
	void (*reset)(void);

	/** Write a character to the console.
	 * @param ch		Character to write. */
	void (*putc)(char ch);
} console_out_ops_t;

/** Console input operations structure. */
typedef struct console_in_ops {
	/** Check for a character from the console.
	 * @return		Whether a character is available. */
	bool (*poll)(void);

	/** Read a character from the console.
	 * @return		Character read. */
	uint16_t (*getc)(void);
} console_in_ops_t;

/** Special key codes. */
#define CONSOLE_KEY_UP		0x100
#define CONSOLE_KEY_DOWN	0x101
#define CONSOLE_KEY_LEFT	0x102
#define CONSOLE_KEY_RIGHT	0x103
#define CONSOLE_KEY_HOME	0x104
#define CONSOLE_KEY_END		0x105
#define CONSOLE_KEY_DELETE	0x106
#define CONSOLE_KEY_F1		0x107
#define CONSOLE_KEY_F2		0x108
#define CONSOLE_KEY_F3		0x109
#define CONSOLE_KEY_F4		0x10a
#define CONSOLE_KEY_F5		0x10b
#define CONSOLE_KEY_F6		0x10c
#define CONSOLE_KEY_F7		0x10d
#define CONSOLE_KEY_F8		0x10e
#define CONSOLE_KEY_F9		0x10f
#define CONSOLE_KEY_F10		0x110

/** Structure describing a console. */
typedef struct console {
	console_out_ops_t *out;		/**< Output operations. */
	console_in_ops_t *in;		/**< Input operations. */
} console_t;

/** Debug log size. */
#define DEBUG_LOG_SIZE		8192

extern char debug_log[DEBUG_LOG_SIZE];
extern size_t debug_log_start;
extern size_t debug_log_length;

extern console_t main_console;
extern console_t debug_console;

#endif /* __CONSOLE_H */
