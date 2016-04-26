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

#include <arch/io.h>

#include <drivers/serial/ns16550.h>

#include <efi/console.h>
#include <efi/efi.h>
#include <efi/services.h>

#include <assert.h>
#include <console.h>
#include <loader.h>
#include <memory.h>

/** Serial port definitions. */
#ifdef CONFIG_ARCH_X86
#   define SERIAL_PORT      0x3f8
#   define SERIAL_CLOCK     1843200
#endif

/** EFI console input device. */
typedef struct efi_console_in {
    console_in_t console;                   /**< Console input device header. */

    efi_simple_text_input_protocol_t *in;   /**< Text input protocol. */
    uint16_t saved_key;                     /**< Key saved in efi_console_poll(). */
} efi_console_in_t;

/** EFI serial port structure. */
typedef struct efi_serial_port {
    serial_port_t port;                     /**< Serial port header. */
    efi_serial_io_protocol_t *serial;       /**< Serial I/O protocol. */
} efi_serial_port_t;

/** EFI scan code conversion table. */
static uint16_t efi_scan_codes[] = {
    0,
    CONSOLE_KEY_UP, CONSOLE_KEY_DOWN, CONSOLE_KEY_RIGHT, CONSOLE_KEY_LEFT,
    CONSOLE_KEY_HOME, CONSOLE_KEY_END, 0, 0x7f, CONSOLE_KEY_PGUP,
    CONSOLE_KEY_PGDOWN, CONSOLE_KEY_F1, CONSOLE_KEY_F2, CONSOLE_KEY_F3,
    CONSOLE_KEY_F4, CONSOLE_KEY_F5, CONSOLE_KEY_F6, CONSOLE_KEY_F7,
    CONSOLE_KEY_F8, CONSOLE_KEY_F9, CONSOLE_KEY_F10, 0, 0, '\e',
};

/** Serial protocol GUID. */
static efi_guid_t serial_io_guid = EFI_SERIAL_IO_PROTOCOL_GUID;

/** Check for a character from the console.
 * @param _console      Console input device.
 * @return              Whether a character is available. */
static bool efi_console_poll(console_in_t *_console) {
    efi_console_in_t *console = (efi_console_in_t *)_console;

    if (console->saved_key)
        return true;

    while (true) {
        efi_input_key_t key;
        efi_status_t ret = efi_call(console->in->read_key_stroke, console->in, &key);
        if (ret != EFI_SUCCESS)
            return false;

        if (key.scan_code) {
            if (key.scan_code >= array_size(efi_scan_codes)) {
                continue;
            } else if (!efi_scan_codes[key.scan_code]) {
                continue;
            }

            console->saved_key = efi_scan_codes[key.scan_code];
            return true;
        } else if (key.unicode_char <= 0x7f) {
            /* Some day we should handle Unicode properly. */
            console->saved_key = (key.unicode_char == '\r') ? '\n' : key.unicode_char;
            return true;
        }
    }
}

/** Read a character from the console.
 * @param _console      Console input device.
 * @return              Character read. */
static uint16_t efi_console_getc(console_in_t *_console) {
    efi_console_in_t *console = (efi_console_in_t *)_console;
    uint16_t ret;

    if (!console->saved_key) {
        while (!efi_console_poll(_console))
            ;
    }

    ret = console->saved_key;
    console->saved_key = 0;
    return ret;
}

/** EFI main console input operations. */
static console_in_ops_t efi_console_in_ops = {
    .poll = efi_console_poll,
    .getc = efi_console_getc,
};

/** EFI console input device. */
static efi_console_in_t efi_console_in = {
    .console = {
        .ops = &efi_console_in_ops,
    },
};

/** Set the port configuration.
 * @param _port         Port to configure.
 * @param config        Port configuration.
 * @return              Status code describing the result of the operation. */
static status_t efi_serial_port_config(serial_port_t *_port, const serial_config_t *config) {
    efi_serial_port_t *port = (efi_serial_port_t *)_port;
    efi_parity_type_t parity;
    efi_stop_bits_type_t stop_bits;
    efi_status_t ret;

    switch (config->parity) {
    case SERIAL_PARITY_NONE:
        parity = EFI_NO_PARITY;
        break;
    case SERIAL_PARITY_ODD:
        parity = EFI_ODD_PARITY;
        break;
    case SERIAL_PARITY_EVEN:
        parity = EFI_EVEN_PARITY;
        break;
    default:
        assert(0 && "Invalid parity type");
        return STATUS_INVALID_ARG;
    }

    switch (config->stop_bits) {
    case 1:
        stop_bits = EFI_ONE_STOP_BIT;
        break;
    case 2:
        stop_bits = EFI_TWO_STOP_BITS;
        break;
    default:
        assert(0 && "Invalid stop bits value");
        return STATUS_INVALID_ARG;
    }

    ret = efi_call(port->serial->set_attributes,
        port->serial, config->baud_rate, 0, 0, parity, config->data_bits,
        stop_bits);
    if (ret != EFI_SUCCESS)
        return efi_convert_status(ret);

    return STATUS_SUCCESS;
}

/** Check whether the RX buffer is empty.
 * @param _port         Port to check.
 * @return              Whether the RX buffer is empty. */
static bool efi_serial_port_rx_empty(serial_port_t *_port) {
    efi_serial_port_t *port = (efi_serial_port_t *)_port;
    efi_uint32_t control;

    if (efi_call(port->serial->get_control, port->serial, &control) != EFI_SUCCESS)
        return true;

    return control & EFI_SERIAL_INPUT_BUFFER_EMPTY;
}

/** Read from a port (RX will be non-empty).
 * @param _port         Port to write to.
 * @return              Value read from the port. */
static uint8_t efi_serial_port_read(serial_port_t *_port) {
    efi_serial_port_t *port = (efi_serial_port_t *)_port;
    efi_uintn_t size = 1;
    uint8_t val = 0;

    efi_call(port->serial->read, port->serial, &size, &val);
    return val;
}

/** Check whether the TX buffer is empty.
 * @param _port         Port to check.
 * @return              Whether the TX buffer is empty. */
static bool efi_serial_port_tx_empty(serial_port_t *_port) {
    efi_serial_port_t *port = (efi_serial_port_t *)_port;
    efi_uint32_t control;

    if (efi_call(port->serial->get_control, port->serial, &control) != EFI_SUCCESS)
        return false;

    return control & EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
}

/** Write to a port (TX will be empty).
 * @param _port         Port to write to.
 * @param val           Value to write. */
static void efi_serial_port_write(serial_port_t *_port, uint8_t val) {
    efi_serial_port_t *port = (efi_serial_port_t *)_port;
    efi_uintn_t size = 1;

    efi_call(port->serial->write, port->serial, &size, &val);
}

/** EFI serial port operations. */
static serial_port_ops_t efi_serial_port_ops = {
    .config = efi_serial_port_config,
    .rx_empty = efi_serial_port_rx_empty,
    .read = efi_serial_port_read,
    .tx_empty = efi_serial_port_tx_empty,
    .write = efi_serial_port_write,
};

/** Register a fallback serial port.
 * @param config        Configuration to use. */
static void register_fallback_port(const serial_config_t *config) {
    #ifdef CONFIG_ARCH_X86
        serial_port_t *port = ns16550_register(SERIAL_PORT, 0, SERIAL_CLOCK);

        if (port) {
            serial_port_config(port, config);
            #ifdef CONFIG_DEBUG
                console_set_debug(&port->console);
            #endif
        }
    #endif
}

/** Initialize the serial console. */
static void efi_serial_init(void) {
    serial_config_t config;
    efi_handle_t *handles __cleanup_free = NULL;
    efi_uintn_t num_handles;
    efi_status_t ret;

    config.baud_rate = SERIAL_DEFAULT_BAUD_RATE;
    config.data_bits = SERIAL_DEFAULT_DATA_BITS;
    config.parity = SERIAL_DEFAULT_PARITY;
    config.stop_bits = SERIAL_DEFAULT_STOP_BITS;

    /* Look for a serial console. */
    ret = efi_locate_handle(EFI_BY_PROTOCOL, &serial_io_guid, NULL, &handles, &num_handles);
    if (ret != EFI_SUCCESS) {
        /* Some EFI implementations don't bother exposing the serial port as an
         * EFI device (my Gigabyte board, for example). If we can't find an EFI
         * handle for it try using it directly. */
        register_fallback_port(&config);
        return;
    }

    for (efi_uintn_t i = 0; i < num_handles; i++) {
        efi_serial_port_t *port = malloc(sizeof(*port));

        port->port.ops = &efi_serial_port_ops;
        port->port.index = i;

        ret = efi_open_protocol(
            handles[i], &serial_io_guid, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
            (void **)&port->serial);
        if (ret != EFI_SUCCESS) {
            free(port);
            continue;
        }

        ret = efi_call(port->serial->set_control,
            port->serial,
            EFI_SERIAL_DATA_TERMINAL_READY | EFI_SERIAL_REQUEST_TO_SEND);
        if (ret != EFI_SUCCESS) {
            free(port);
            continue;
        }

        serial_port_register(&port->port);
        serial_port_config(&port->port, &config);

        #if !defined(CONFIG_ARCH_X86) || defined(CONFIG_DEBUG)
            if (i == 0)
                console_set_debug(&port->port.console);
        #endif
    }
}

/** Initialize the EFI console. */
void target_console_init(void) {
    /* Initialize a serial console as the debug console if available. */
    efi_serial_init();

    /* Set the main console input. */
    efi_console_in.in = efi_system_table->con_in;
    primary_console.in = &efi_console_in.console;
}

/** Reset the console to original state. */
void efi_console_reset(void) {
    efi_call(efi_system_table->con_in->reset, efi_system_table->con_in, false);
    efi_call(efi_system_table->con_out->reset, efi_system_table->con_out, false);
}
