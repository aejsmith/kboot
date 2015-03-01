/*
 * Copyright (C) 2009-2014 Alex Smith
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
 * @brief               Formatted output function.
 */

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/printf.h>

#ifdef CONFIG_PLATFORM_EFI
#   include <efi/efi.h>
#endif

#include <endian.h>

/* Flags to specify special behaviour. */
#define PRINTF_ZERO_PAD         (1<<0)  /**< Pad with zeros. */
#define PRINTF_SIGN_CHAR        (1<<1)  /**< Always print a sign character for a signed conversion. */
#define PRINTF_SPACE_CHAR       (1<<2)  /**< Print a space before a positive result of a signed conversion. */
#define PRINTF_LEFT_JUSTIFY     (1<<3)  /**< Print left-justified. */
#define PRINTF_PREFIX           (1<<4)  /**< Print a 0x prefix for base 16 values. */
#define PRINTF_LOW_CASE         (1<<5)  /**< Print hexadecimal characters in lower case. */
#define PRINTF_SIGNED           (1<<6)  /**< Treat number as a signed value. */

/** Internal printf() state. */
typedef struct printf_state {
    printf_helper_t helper;             /**< Helper function. */
    void *data;                         /**< Argument to helper. */
    int total;                          /**< Current output total. */
    unsigned flags;                     /**< Flags for the current item. */
    long width;                         /**< Width of current item. */
    long precision;                     /**< Precision of current item. */
    int base;                           /**< Base of current item. */
} printf_state_t;

/** Digits to use for printing numbers. */
static const char printf_digits_upper[] = "0123456789ABCDEF";
static const char printf_digits_lower[] = "0123456789abcdef";

/** Print a single character.
 * @param state         Internal state structure.
 * @param ch            Character to write. */
static void print_char(printf_state_t *state, char ch) {
    state->helper(ch, state->data, &state->total);
}

/** Helper to print a string of characters.
 * @param state         Internal state structure.
 * @param str           String to write.
 * @param len           Length to print. */
static void print_string(printf_state_t *state, const char *str, size_t len) {
    size_t i;

    for (i = 0; i < len; i++)
        print_char(state, str[i]);
}

/** Helper to print a number.
 * @param state         Internal state structure.
 * @param num           Number to print. */
static void print_number(printf_state_t *state, uint64_t num) {
    char buffer[64];
    char sign = 0;
    uint32_t tmp;
    int i = 0;

    /* Work out the sign character to use, if any. Always print a sign character
     * if the number is negative. */
    if (state->flags & PRINTF_SIGNED) {
        if ((int64_t)num < 0) {
            sign = '-';
            num = -((int64_t)num);
            state->width--;
        } else if (state->flags & PRINTF_SIGN_CHAR) {
            sign = '+';
            state->width--;
        } else if (state->flags & PRINTF_SPACE_CHAR) {
            sign = ' ';
            state->width--;
        }
    }

    /* Reduce field width to accomodate any prefixes required. */
    if (state->flags & PRINTF_PREFIX) {
        if (state->base == 8) {
            state->width -= 1;
        } else if (state->base == 16) {
            state->width -= 2;
        }
    }

    /* Write the number out to the temporary buffer, in reverse order. */
    while (num != 0) {
        tmp = (uint32_t)(num % state->base);
        num /= state->base;
        buffer[i++] = (state->flags & PRINTF_LOW_CASE)
            ? printf_digits_lower[tmp]
            : printf_digits_upper[tmp];
    }

    /* Modify precision to store the number of actual digits we are going to
     * print. The precision is the minimum number of digits to print, so if the
     * digit count is higher than the precision, set precision to the digit
     * count. Width then becomes the amount of padding we require. */
    if (i > state->precision)
        state->precision = i;
    state->width -= state->precision;

    /* If we're not left aligned and require space padding, write it. Do not
     * handle zero padding here, sign and prefix characters must be before zero
     * padding but after space padding. */
    if (!(state->flags & (PRINTF_LEFT_JUSTIFY | PRINTF_ZERO_PAD))) {
        while (--state->width >= 0)
            print_char(state, ' ');
    }

    /* Write out the sign character, if any. */
    if (sign)
        print_char(state, sign);

    /* Write out any prefix required. Base 8 has a '0' prefix, base 16 has a '0x'
     * or '0X' prefix, depending on whether lower or upper case. */
    if (state->flags & PRINTF_PREFIX && (state->base == 8 || state->base == 16)) {
        print_char(state, '0');
        if (state->base == 16)
            print_char(state, (state->flags & PRINTF_LOW_CASE) ? 'x' : 'X');
    }

    /* Do zero padding. */
    if (state->flags & PRINTF_ZERO_PAD) {
        while (--state->width >= 0)
            print_char(state, '0');
    }
    while (i <= --state->precision)
        print_char(state, '0');

    /* Write out actual digits, reversed to the correct direction. */
    while (--i >= 0)
        print_char(state, buffer[i]);

    /* Finally handle space padding caused by left justification. */
    if (state->flags & PRINTF_LEFT_JUSTIFY) {
        while (--state->width >= 0)
            print_char(state, ' ');
    }
}

/** Helper to print a UUID.
 * @param state         Internal state structure.
 * @param ptr           Pointer to UUID.
 * @param big_endian    Whether to interpret UUID as big endian. */
static void print_uuid(printf_state_t *state, const uint8_t *uuid, bool big_endian) {
    uint32_t val32;

    state->base = 16;
    state->flags = PRINTF_ZERO_PAD | PRINTF_LOW_CASE;

    state->width = 8;
    state->precision = -1;
    val32 = *(const uint32_t *)(&uuid[0]);
    print_number(state, (big_endian) ? be32_to_cpu(val32) : le32_to_cpu(val32));
    print_char(state, '-');

    for (size_t i = 0; i < 2; i++) {
        uint16_t val16 = *(const uint16_t *)(&uuid[4 + (i * 2)]);

        state->width = 4;
        state->precision = -1;
        print_number(state, (big_endian) ? be16_to_cpu(val16) : le16_to_cpu(val16));
        print_char(state, '-');
    }

    for (size_t i = 0; i < 8; i++) {
        state->width = 2;
        state->precision = -1;
        print_number(state, uuid[8 + i]);
        if (i == 1)
            print_char(state, '-');
    }
}

/** Helper to print a pointer.
 * @param state         Internal state structure.
 * @param fmt           Pointer to format string pointer.
 * @param ptr           Pointer to print. */
static void print_pointer(printf_state_t *state, const char **fmt, void *ptr) {
    /* Print lower-case and as though # was specified. */
    state->flags |= PRINTF_LOW_CASE | PRINTF_PREFIX;
    if (state->precision == -1)
        state->precision = 1;

    /*
     * Extensions for certain useful things. Idea borrowed from the Linux
     * kernel. The following formats are implemented:
     *  - %pE = Print an EFI device path (arg is device path protocol, EFI only).
     *  - %pu = Print a little-endian (i.e. EFI) UUID (arg is pointer to 16-byte UUID).
     *  - %pU = Print a big-endian UUID (arg is pointer to 16-byte UUID).
     */
    switch ((*fmt)[1]) {
    case 'E':
        #if defined(CONFIG_PLATFORM_EFI) && !defined(__TEST)
            ++(*fmt);
            efi_print_device_path((efi_device_path_t *)ptr, (void *)print_char, state);
            break;
        #endif
    case 'u':
        ++(*fmt);
        print_uuid(state, ptr, false);
        break;
    case 'U':
        ++(*fmt);
        print_uuid(state, ptr, true);
        break;
    default:
        state->base = 16;
        print_number(state, (ptr_t)ptr);
        break;
    }
}

/**
 * Internal implementation of printf()-style functions.
 *
 * This function does the main work of printf()-style functions. It parses
 * the format string, and uses a supplied helper function to actually put
 * characters in the desired output location (for example the console or a
 * string buffer).
 *
 * @note                Floating point values are not supported.
 * @note                The 'n' conversion specifier is not supported.
 * @note                The 't' length modifier is not supported.
 *
 * @param helper        Helper function to use.
 * @param data          Data to pass to helper function.
 * @param fmt           Format string.
 * @param args          List of arguments.
 *
 * @return              Number of characters written.
 */
int do_vprintf(printf_helper_t helper, void *data, const char *fmt, va_list args) {
    printf_state_t state;
    unsigned char ch;
    const char *str;
    uint64_t num;
    int32_t len;

    state.helper = helper;
    state.data = data;
    state.total = 0;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            print_char(&state, *fmt);
            continue;
        }

        /* Parse flags in the format string. */
        state.flags = 0;
        while (true) {
            switch (*(++fmt)) {
            case '#':
                state.flags |= PRINTF_PREFIX;
                continue;
            case '0':
                /* Left justify has greater priority than zero padding. */
                if (!(state.flags & PRINTF_LEFT_JUSTIFY))
                    state.flags |= PRINTF_ZERO_PAD;
                continue;
            case '-':
                state.flags &= ~PRINTF_ZERO_PAD;
                state.flags |= PRINTF_LEFT_JUSTIFY;
                continue;
            case ' ':
                state.flags |= PRINTF_SPACE_CHAR;
                continue;
            case '+':
                state.flags |= PRINTF_SIGN_CHAR;
                continue;
            }

            break;
        }

        /* Work out the field width. If there is a digit at the current position
         * in the string, then the field width is in the string. If there is a
         * '*' character, the field width is the next argument (a negative field
         * width argument implies left justification, and the width will be made
         * positive). */
        if (isdigit(*fmt)) {
            state.width = strtol(fmt, (char **)&fmt, 10);
        } else if (*fmt == '*') {
            state.width = (long)va_arg(args, int);
            if (state.width < 0) {
                state.flags |= PRINTF_LEFT_JUSTIFY;
                state.width = -state.width;
            }
            fmt++;
        } else {
            state.width = -1;
        }

        /* If there is a period character in the string, there is a precision.
         * This can also be specified as a '*' character, like for field width,
         * except a negative value means 0. */
        if (*fmt == '.') {
            fmt++;
            if (isdigit(*fmt)) {
                state.precision = strtol(fmt, (char **)&fmt, 10);
                if (state.precision < 0)
                    state.precision = 0;
            } else if (*fmt == '*') {
                state.precision = (long)va_arg(args, int);
                if (state.precision < 0)
                    state.precision = 0;
                fmt++;
            } else {
                state.precision = 0;
            }
        } else {
            state.precision = -1;
        }

        /* Get the length modifier. */
        switch (*fmt) {
        case 'h':
        case 'z':
            len = (uint32_t)*(fmt++);
            break;
        case 'l':
            len = (uint32_t)*(fmt++);
            if (*fmt == 'l') {
                len = 'L';
                fmt++;
            }
            break;
        case 'L':
            len = (uint32_t)*(fmt++);
            break;
        default:
            len = 0;
            break;
        }

        /* Get and handle the conversion specifier. For number conversions, we
         * break out of the switch to get to the number handling code. For
         * anything else, continue to the next iteration of the main loop. */
        state.base = 10;
        switch (*fmt) {
        case '%':
            print_char(&state, '%');
            continue;
        case 'c':
            ch = (unsigned char)va_arg(args, int);
            if (state.flags & PRINTF_LEFT_JUSTIFY) {
                print_char(&state, ch);
                while (--state.width > 0)
                    print_char(&state, ' ');
            } else {
                while (--state.width > 0)
                    print_char(&state, ' ');
                print_char(&state, ch);
            }
            continue;
        case 'd':
        case 'i':
            state.flags |= PRINTF_SIGNED;
            break;
        case 'o':
            state.base = 8;
            break;
        case 'p':
            print_pointer(&state, &fmt, va_arg(args, void *));
            continue;
        case 's':
            /* We won't need the length modifier here, can use the len variable. */
            str = va_arg(args, const char *);
            len = strnlen(str, state.precision);
            if (state.flags & PRINTF_LEFT_JUSTIFY) {
                print_string(&state, str, len);
                while (len < state.width--)
                    print_char(&state, ' ');
            } else {
                while (len < state.width--)
                    print_char(&state, ' ');
                print_string(&state, str, len);
            }
            continue;
        case 'u':
            break;
        case 'x':
            state.flags |= PRINTF_LOW_CASE;
        case 'X':
            state.base = 16;
            break;
        default:
            /* Unknown character, go back and reprint what we skipped over. */
            print_char(&state, '%');
            while (fmt[-1] != '%')
                fmt--;

            continue;
        }

        /* Default precision for numbers should be 1. */
        if (state.precision == -1)
            state.precision = 1;

        /* Perform conversions according to the length modifiers. */
        switch (len & 0xff) {
        case 'h':
            num = (unsigned short)va_arg(args, int);
            if (state.flags & PRINTF_SIGNED)
                num = (signed short)num;
            break;
        case 'l':
            num = va_arg(args, unsigned long);
            if (state.flags & PRINTF_SIGNED)
                num = (signed long)num;
            break;
        case 'L':
            num = va_arg(args, unsigned long long);
            if (state.flags & PRINTF_SIGNED)
                num = (signed long long)num;
            break;
        case 'z':
            num = va_arg(args, size_t);
            break;
        default:
            num = va_arg(args, unsigned int);
            if (state.flags & PRINTF_SIGNED)
                num = (signed int)num;
        }

        /* Print the number. */
        print_number(&state, num);
    }

    return state.total;
}

/**
 * Internal implementation of printf()-style functions.
 *
 * This function does the main work of printf()-style functions. It parses
 * the format string, and uses a supplied helper function to actually put
 * characters in the desired output location (for example the console or a
 * string buffer).
 *
 * @note                Floating point values are not supported.
 * @note                The 'n' conversion specifier is not supported.
 * @note                The 't' length modifier is not supported.
 *
 * @param helper        Helper function to use.
 * @param data          Data to pass to helper function.
 * @param fmt           Format string.
 * @param ...           List of arguments.
 *
 * @return              Number of characters written.
 */
int do_printf(printf_helper_t helper, void *data, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = do_vprintf(helper, data, fmt, args);
    va_end(args);

    return ret;
}
