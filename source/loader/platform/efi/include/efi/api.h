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
 * @brief		EFI API definitions.
 */

#ifndef __EFI_API_H
#define __EFI_API_H

#include <lib/utility.h>

/**
 * Basic EFI type definitions.
 */

/** Basic integer types. */
typedef unsigned char efi_boolean_t;
#ifdef CONFIG_64BIT
typedef int64_t efi_intn_t;
typedef uint64_t efi_uintn_t;
#else
typedef int32_t efi_intn_t;
typedef uint32_t efi_uintn_t;
#endif
typedef int8_t efi_int8_t;
typedef uint8_t efi_uint8_t;
typedef int16_t efi_int16_t;
typedef uint16_t efi_uint16_t;
typedef int32_t efi_int32_t;
typedef uint32_t efi_uint32_t;
typedef int64_t efi_int64_t;
typedef uint64_t efi_uint64_t;

/** Other basic types. */
typedef uint8_t efi_char8_t;		/**< 1-byte character. */
typedef uint16_t efi_char16_t;		/**< 2-byte character. */
typedef efi_intn_t efi_status_t;	/**< Type used for EFI status codes. */
typedef void *efi_handle_t;		/**< Collection of related interfaces. */
typedef void *efi_event_t;		/**< Handle to an event structure. */
typedef efi_uint64_t efi_lba_t;		/**< Logical block address. */
typedef efi_uintn_t efi_tpl_t;		/**< Task priority level. */

/** Network MAC address. */
typedef uint8_t efi_mac_address_t[32];

/** IPv4 internet address. */
typedef uint8_t efi_ipv4_address_t[4];

/** IPv6 internet address. */
typedef uint8_t efi_ipv6_address_t[16];

/** Either an IPv4 or IPv6 address. */
typedef uint8_t efi_ip_address_t[16] __aligned(4);

/** Physical and virtual address types (always 64-bit). */
typedef efi_uint64_t efi_physical_address_t;
typedef efi_uint64_t efi_virtual_address_t;

/**
 * EFI status codes.
 */

/** Define an EFI error code (high bit set). */
#define EFI_ERROR(value)		\
	((((efi_status_t)1) << (BITS(efi_status_t) - 1)) | (value))

/** Define an EFI warning code (high bit clear). */
#define EFI_WARNING(value)		(value)

/** EFI success codes. */
#define EFI_SUCCESS                0

/** EFI error codes. */
#define EFI_LOAD_ERROR			EFI_ERROR(1)
#define EFI_INVALID_PARAMETER		EFI_ERROR(2)
#define EFI_UNSUPPORTED			EFI_ERROR(3)
#define EFI_BAD_BUFFER_SIZE		EFI_ERROR(4)
#define EFI_BUFFER_TOO_SMALL		EFI_ERROR(5)
#define EFI_NOT_READY			EFI_ERROR(6)
#define EFI_DEVICE_ERROR		EFI_ERROR(7)
#define EFI_WRITE_PROTECTED		EFI_ERROR(8)
#define EFI_OUT_OF_RESOURCES		EFI_ERROR(9)
#define EFI_VOLUME_CORRUPTED		EFI_ERROR(10)
#define EFI_VOLUME_FULL			EFI_ERROR(11)
#define EFI_NO_MEDIA			EFI_ERROR(12)
#define EFI_MEDIA_CHANGED		EFI_ERROR(13)
#define EFI_NOT_FOUND			EFI_ERROR(14)
#define EFI_ACCESS_DENIED		EFI_ERROR(15)
#define EFI_NO_RESPONSE			EFI_ERROR(16)
#define EFI_NO_MAPPING			EFI_ERROR(17)
#define EFI_TIMEOUT			EFI_ERROR(18)
#define EFI_NOT_STARTED			EFI_ERROR(19)
#define EFI_ALREADY_STARTED		EFI_ERROR(20)
#define EFI_ABORTED			EFI_ERROR(21)
#define EFI_ICMP_ERROR			EFI_ERROR(22)
#define EFI_TFTP_ERROR			EFI_ERROR(23)
#define EFI_PROTOCOL_ERROR		EFI_ERROR(24)
#define EFI_INCOMPATIBLE_VERSION	EFI_ERROR(25)
#define EFI_SECURITY_VIOLATION		EFI_ERROR(26)
#define EFI_CRC_ERROR			EFI_ERROR(27)
#define EFI_END_OF_MEDIA		EFI_ERROR(28)
#define EFI_END_OF_FILE			EFI_ERROR(31)
#define EFI_INVALID_LANGUAGE		EFI_ERROR(32)
#define EFI_COMPROMISED_DATA		EFI_ERROR(33)
#define EFI_IP_ADDRESS_CONFLICT		EFI_ERROR(34)

/** EFI warning codes. */
#define EFI_WARN_UNKNOWN_GLYPH		EFI_WARNING(1)
#define EFI_WARN_DELETE_FAILURE		EFI_WARNING(2)
#define EFI_WARN_WRITE_FAILURE		EFI_WARNING(3)
#define EFI_WARN_BUFFER_TOO_SMALL	EFI_WARNING(4)
#define EFI_WARN_STALE_DATA		EFI_WARNING(5)

#endif /* __EFI_API_H */
