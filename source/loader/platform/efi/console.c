/*
 * Copyright (C) 2014 Alex Smith
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
 * @brief		EFI console functions.
 */

#include <efi/efi.h>

#include <console.h>

/** Console out protocol. */
static efi_simple_text_output_protocol_t *console_out;

/** Write a character to the console.
 * @param ch		Character to write. */
static void efi_console_putc(char ch) {
	efi_char16_t str[3] = {};

	if(ch == '\n') {
		str[0] = '\r';
		str[1] = '\n';
	} else {
		str[0] = ch & 0x7f;
	}

	console_out->output_string(console_out, str);
}

/** EFI main console implementation. */
static console_t efi_console = {
	.putc = efi_console_putc,
};

/** Initialize the EFI console. */
void efi_console_init(void) {
	/* Set up the main console. */
	console_out = efi_system_table->con_out;
	console_out->clear_screen(console_out);
	main_console = &efi_console;
}