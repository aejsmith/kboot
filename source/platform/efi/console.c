/*
 * Copyright (C) 2014-2015 Alex Smith
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
 * @brief               EFI console functions.
 */

#include <efi/efi.h>

#include <console.h>
#include <loader.h>
#include <memory.h>

/** Serial port parameters (match the defaults given in the EFI spec). */
#define SERIAL_BAUD_RATE    115200
#define SERIAL_PARITY       EFI_NO_PARITY
#define SERIAL_DATA_BITS    8
#define SERIAL_STOP_BITS    EFI_ONE_STOP_BIT

/** Serial protocol GUID. */
static efi_guid_t serial_io_guid = EFI_SERIAL_IO_PROTOCOL_GUID;

/** Console protocols. */
static efi_simple_text_input_protocol_t *console_in;
static efi_serial_io_protocol_t *serial_io;

/** Saved key press. */
static efi_input_key_t saved_key;

/** Write a character to the serial console.
 * @param ch            Character to write. */
static void efi_serial_putc(char ch) {
    efi_uintn_t size = 1;

    if (ch == '\n')
        efi_serial_putc('\r');

    efi_call(serial_io->write, serial_io, &size, &ch);
}

/** EFI serial console output operations. */
static console_out_ops_t efi_serial_out_ops = {
    .putc = efi_serial_putc,
};

/** Check for a character from the console.
 * @return          Whether a character is available. */
static bool efi_console_poll(void) {
    efi_input_key_t key;
    efi_status_t ret;

    if (saved_key.scan_code || saved_key.unicode_char)
        return true;

    ret = efi_call(console_in->read_key_stroke, console_in, &key);
    if (ret != EFI_SUCCESS)
        return false;

    /* Save the key press to be returned by getc(). */
    saved_key = key;
    return true;
}

/** EFI scan code conversion table. */
static uint16_t efi_scan_codes[] = {
    0,
    CONSOLE_KEY_UP, CONSOLE_KEY_DOWN, CONSOLE_KEY_RIGHT, CONSOLE_KEY_LEFT,
    CONSOLE_KEY_HOME, CONSOLE_KEY_END, 0, 0x7f, 0, 0,
    CONSOLE_KEY_F1, CONSOLE_KEY_F2, CONSOLE_KEY_F3, CONSOLE_KEY_F4,
    CONSOLE_KEY_F5, CONSOLE_KEY_F6, CONSOLE_KEY_F7, CONSOLE_KEY_F8,
    CONSOLE_KEY_F9, CONSOLE_KEY_F10, '\e',
};

/** Read a character from the console.
 * @return              Character read. */
static uint16_t efi_console_getc(void) {
    efi_input_key_t key;
    efi_status_t ret;

    while (true) {
        if (saved_key.scan_code || saved_key.unicode_char) {
            key = saved_key;
            saved_key.scan_code = saved_key.unicode_char = 0;
        } else {
            ret = efi_call(console_in->read_key_stroke, console_in, &key);
            if (ret != EFI_SUCCESS)
                continue;
        }

        if (key.scan_code) {
            if (key.scan_code >= array_size(efi_scan_codes)) {
                continue;
            } else if (!efi_scan_codes[key.scan_code]) {
                continue;
            }

            return efi_scan_codes[key.scan_code];
        } else if (key.unicode_char <= 0x7f) {
            /* Whee, Unicode! */
            return (key.unicode_char == '\r') ? '\n' : key.unicode_char;
        }
    }
}

/** EFI main console input operations. */
static console_in_ops_t efi_console_in_ops = {
    .poll = efi_console_poll,
    .getc = efi_console_getc,
};

/** Initialize the serial console. */
static void efi_serial_init(void) {
    efi_handle_t *handles;
    efi_uintn_t num_handles;
    efi_status_t ret;

    /* Look for a serial console. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &serial_io_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Just use the first handle. */
    ret = efi_open_protocol(handles[0], &serial_io_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL, (void **)&serial_io);
    free(handles);
    if (ret != EFI_SUCCESS)
        return;

    /* Configure the port. */
    ret = efi_call(serial_io->set_attributes,
        serial_io, SERIAL_BAUD_RATE, 0, 0, SERIAL_PARITY, SERIAL_DATA_BITS,
        SERIAL_STOP_BITS);
    if (ret != EFI_SUCCESS)
        return;

    ret = efi_call(serial_io->set_control, serial_io, EFI_SERIAL_DATA_TERMINAL_READY | EFI_SERIAL_REQUEST_TO_SEND);
    if (ret != EFI_SUCCESS)
        return;

    debug_console.out = &efi_serial_out_ops;
}

/** Initialize the EFI console. */
void efi_console_init(void) {
    /* Initialize a serial console as the debug console if available. */
    efi_serial_init();

    /* Set the main console input. */
    console_in = efi_system_table->con_in;
    main_console.in = &efi_console_in_ops;
}
