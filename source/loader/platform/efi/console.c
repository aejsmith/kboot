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

/** Console input protocol. */
static efi_simple_text_input_protocol_t *console_in;

/** Saved key press. */
static efi_input_key_t saved_key;

/** Reset the console to a default state. */
static void efi_console_reset(void) {
	efi_call(console_out->clear_screen, console_out);
}

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

	efi_call(console_out->output_string, console_out, str);
}

/** EFI main console output operations. */
static console_out_ops_t efi_console_out_ops = {
	.reset = efi_console_reset,
	.putc = efi_console_putc,
};

/** Check for a character from the console.
 * @return		Whether a character is available. */
static bool efi_console_poll(void) {
	efi_input_key_t key;
	efi_status_t ret;

	if(saved_key.scan_code || saved_key.unicode_char)
		return true;

	ret = efi_call(console_in->read_key_stroke, console_in, &key);
	if(ret != EFI_SUCCESS)
		return false;

	/* Save the key press to be returned by getc(). */
	saved_key = key;
	return true;
}

/** EFI scan code conversion table. */
static uint16_t efi_scan_codes[] = {
	0,
	CONSOLE_KEY_UP, CONSOLE_KEY_DOWN, CONSOLE_KEY_RIGHT, CONSOLE_KEY_LEFT,
	CONSOLE_KEY_HOME, CONSOLE_KEY_END, 0, CONSOLE_KEY_DELETE, 0, 0,
	CONSOLE_KEY_F1, CONSOLE_KEY_F2, CONSOLE_KEY_F3, CONSOLE_KEY_F4,
	CONSOLE_KEY_F5, CONSOLE_KEY_F6, CONSOLE_KEY_F7, CONSOLE_KEY_F8,
	CONSOLE_KEY_F9, CONSOLE_KEY_F10, '\e',
};

#include <loader.h>

/** Read a character from the console.
 * @return		Character read. */
static uint16_t efi_console_getc(void) {
	efi_input_key_t key;
	efi_status_t ret;

	while(true) {
		if(saved_key.scan_code || saved_key.unicode_char) {
			key = saved_key;
			saved_key.scan_code = saved_key.unicode_char = 0;
		} else {
			ret = efi_call(console_in->read_key_stroke, console_in, &key);
			if(ret != EFI_SUCCESS)
				continue;
		}

		if(key.scan_code) {
			if(key.scan_code >= ARRAY_SIZE(efi_scan_codes)) {
				continue;
			} else if(!efi_scan_codes[key.scan_code]) {
				continue;
			}

			return efi_scan_codes[key.scan_code];
		} else if(key.unicode_char & 0x7f) {
			/* Whee, Unicode! */
			return key.unicode_char & 0x7f;
		}
	}
}

/** EFI main console input operations. */
static console_in_ops_t efi_console_in_ops = {
	.poll = efi_console_poll,
	.getc = efi_console_getc,
};

/** Initialize the EFI console. */
void efi_console_init(void) {
	console_out = efi_system_table->con_out;
	console_in = efi_system_table->con_in;

	efi_call(console_out->clear_screen, console_out);

	main_console.out = &efi_console_out_ops;
	main_console.in = &efi_console_in_ops;
}
