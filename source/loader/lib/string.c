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
 * @brief		String handling functions.
 *
 * @note		These functions are written with code size rather than
 *			speed in mind.
 */

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>

#include <loader.h>

#ifndef ARCH_HAS_MEMCPY

/**
 * Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		Destination location.
 */
void *memcpy(void *__restrict dest, const void *__restrict src, size_t count) {
	const unsigned char *s = src;
	unsigned char *d = dest;

	for(; count != 0; count--)
		*d++ = *s++;

	return dest;
}

#endif /* ARCH_HAS_MEMCPY */

#ifndef ARCH_HAS_MEMSET

/** Fill a memory area.
 * @param dest		The memory area to fill.
 * @param val		The value to fill with (converted to an unsigned char).
 * @param count		The number of bytes to fill.
 * @return		Destination location. */
void *memset(void *dest, int val, size_t count) {
	unsigned char *d = dest;

	for(; count != 0; count--)
		*d++ = (unsigned char)val;

	return dest;
}

#endif /* ARCH_HAS_MEMSET */

/**
 * Copy overlapping data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may overlap.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		Destination location.
 */
void *memmove(void *dest, const void *src, size_t count) {
	const unsigned char *s;
	unsigned char *d;

	if(src != dest) {
		if(src > dest) {
			memcpy(dest, src, count);
		} else {
			d = (unsigned char *)dest + (count - 1);
			s = (const unsigned char *)src + (count - 1);
			while(count--)
				*d-- = *s--;
		}
	}

	return dest;
}

/** Get the length of a string.
 * @param str		Pointer to the string.
 * @return		Length of the string. */
size_t strlen(const char *str) {
	size_t ret;

	for(ret = 0; *str; str++, ret++)
		;

	return ret;
}

/** Get length of a string with limit.
 * @param str		Pointer to the string.
 * @param count		Maximum length of the string.
 * @return		Length of the string. */
size_t strnlen(const char *str, size_t count) {
	size_t ret;

	for(ret = 0; *str && ret < count; str++, ret++)
		;

	return ret;
}

/** Compare two strings.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
	char x;

	for(;;) {
		x = *s1;
		if(x != *s2 || !x)
			break;

		s1++;
		s2++;
	}

	return x - *s2;
}

/** Compare two strings with a length limit.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @param count		Maximum number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
	const char *a = s1;
	const char *b = s2;
	const char *fini = a + count;
	int res;

	while(a < fini) {
		res = *a - *b;
		if(res) {
			return res;
		} else if(!*a) {
			return 0;
		}

		a++;
		b++;
	}

	return 0;
}

/**
 * Separate a string.
 *
 * Finds the first occurrence of a symbol in the string delim in *stringp.
 * If one is found, the delimeter is replaced by a NULL byte and the pointer
 * pointed to by stringp is updated to point past the string. If no delimeter
 * is found *stringp is made NULL and the token is taken to be the entire
 * string.
 *
 * @param stringp	Pointer to a pointer to the string to separate.
 * @param delim		String containing all possible delimeters.
 *
 * @return		NULL if stringp is NULL, otherwise a pointer to the
 *			token found.
 */
char *strsep(char **stringp, const char *delim) {
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if(!(s = *stringp))
		return NULL;

	for(tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if((sc = *spanp++) == c) {
				if(c == 0) {
					s = NULL;
				} else {
					s[-1] = 0;
				}

				*stringp = s;
				return tok;
			}
		} while(sc != 0);
	}
}

/** Find first occurrence of a character in a string.
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * @return		NULL if token not found, otherwise pointer to token. */
char *strchr(const char *s, int c) {
	char ch = c;

	for(;;) {
		if(*s == ch) {
			break;
		} else if(!*s) {
			return NULL;
		} else {
			s++;
		}
	}

	return (char *)s;
}

/** Find last occurrence of a character in a string.
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * @return		NULL if token not found, otherwise pointer to token. */
char *strrchr(const char *s, int c) {
	const char *l = NULL;

	for(;;) {
		if(*s == c)
			l = s;
		if(!*s)
			return (char *)l;
		s++;
	}

	return (char *)l;
}

/** Find the first occurrence of a substring in a string.
 * @param s		String to search.
 * @param what		Substring to search for.
 * @return		Pointer to start of match if found, null if not. */
char *strstr(const char *s, const char *what) {
	size_t len = strlen(what);

	while(*s) {
		if(strncmp(s, what, len) == 0)
			return (char *)s;
		s++;
	}

	return NULL;
}

/**
 * Strip whitespace from a string.
 *
 * Strips whitespace from the start and end of a string. The string is modified
 * in-place.
 *
 * @param str		String to remove from.
 *
 * @return		Pointer to new start of string.
 */
char *strstrip(char *str) {
	size_t len;

	/* Strip from beginning. */
	while(isspace(*str))
		str++;

	/* Strip from end. */
	len = strlen(str);
	while(len--) {
		if(!isspace(str[len]))
			break;
	}

	str[++len] = 0;
	return str;
}

/**
 * Copy a string.
 *
 * Copies a string from one place to another. Assumes that the destination
 * is big enough to hold the string.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 *
 * @return		The value specified for dest.
 */
char *strcpy(char *__restrict dest, const char *__restrict src) {
	char *d = dest;

	while((*d++ = *src++))
		;

	return dest;
}

/**
 * Copy a string with a length limit.
 *
 * Copies a string from one place to another. Will copy at most the number
 * of bytes specified.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * @param count		Maximum number of bytes to copy.
 *
 * @return		The value specified for dest.
 */
char *strncpy(char *__restrict dest, const char *__restrict src, size_t count) {
	size_t i;

	for(i = 0; i < count; i++) {
		dest[i] = src[i];
		if(!src[i])
			break;
	}

	return dest;
}

/**
 * Concatenate two strings.
 *
 * Appends one string to another. Assumes that the destination string has
 * enough space to store the contents of both strings and the NULL terminator.
 *
 * @param dest		Pointer to the string to append to.
 * @param src		Pointer to the string to append.
 *
 * @return		Pointer to dest.
 */
char *strcat(char *__restrict dest, const char *__restrict src) {
	size_t len = strlen(dest);
	char *d = dest + len;

	while((*d++ = *src++))
		;

	return dest;
}

/** Macro to implement strtoul() and strtoull(). */
#define __strtoux(type, cp, endp, base)		\
	__extension__ \
	({ \
		type result = 0, value; \
		if(!base) { \
			if(*cp == '0') { \
				if((tolower(*(++cp)) == 'x') && isxdigit(cp[1])) { \
					cp++; \
					base = 16; \
				} else { \
					base = 8; \
				} \
			} else { \
				base = 10; \
			} \
		} else if(base == 16) { \
			if(cp[0] == '0' && tolower(cp[1]) == 'x') \
				cp += 2; \
		} \
		\
		while(isxdigit(*cp) && (value = isdigit(*cp) \
			? *cp - '0' : tolower(*cp) - 'a' + 10) < base) \
		{ \
			result = result * base + value; \
			cp++; \
		} \
		\
		if(endp) \
			*endp = (char *)cp; \
		result; \
	})

/**
 * Convert a string to an unsigned long.
 *
 * Converts a string to an unsigned long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use (if zero will guess).
 *
 * @return		Converted value.
 */
unsigned long strtoul(const char *cp, char **endp, unsigned int base) {
	return __strtoux(unsigned long, cp, endp, base);
}

/**
 * Convert a string to a signed long.
 *
 * Converts a string to an signed long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 *
 * @return		Converted value.
 */
long strtol(const char *cp, char **endp, unsigned int base) {
	if(*cp == '-')
		return -strtoul(cp + 1, endp, base);

	return strtoul(cp, endp, base);
}

/**
 * Convert a string to an unsigned long long.
 *
 * Converts a string to an unsigned long long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 *
 * @return		Converted value.
 */
unsigned long long strtoull(const char *cp, char **endp, unsigned int base) {
	return __strtoux(unsigned long long, cp, endp, base);
}

/**
 * Convert a string to an signed long long.
 *
 * Converts a string to an signed long long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 *
 * @return		Converted value.
 */
long long strtoll(const char *cp, char **endp, unsigned int base) {
	if(*cp == '-')
		return -strtoull(cp + 1, endp, base);

	return strtoull(cp, endp, base);
}

/** Data used by vsnprintf_helper(). */
struct vsnprintf_data {
	char *buf;			/**< Buffer to write to. */
	size_t size;			/**< Total size of buffer. */
	size_t off;			/**< Current number of bytes written. */
};

/** Helper for vsnprintf().
 * @param ch		Character to place in buffer.
 * @param _data		Data.
 * @param total		Pointer to total character count. */
static void vsnprintf_helper(char ch, void *_data, int *total) {
	struct vsnprintf_data *data = _data;

	if(data->off < data->size) {
		data->buf[data->off++] = ch;
		*total = *total + 1;
	}
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param size		The size of the buffer, including the trailing NULL.
 * @param fmt		The format string to use.
 * @param args		Arguments for the format string.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL.
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
	struct vsnprintf_data data;
	int ret;

	data.buf = buf;
	data.size = size - 1;
	data.off = 0;

	ret = do_printf(vsnprintf_helper, &data, fmt, args);

	if(data.off < data.size) {
		data.buf[data.off] = 0;
	} else {
		data.buf[data.size-1] = 0;
	}

	return ret;
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param fmt		The format string to use.
 * @param args		Arguments for the format string.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL.
 */
int vsprintf(char *buf, const char *fmt, va_list args) {
	return vsnprintf(buf, (size_t)-1, fmt, args);
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param size		The size of the buffer, including the trailing NULL.
 * @param fmt		The format string to use.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return ret;
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param fmt		The format string to use.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int sprintf(char *buf, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsprintf(buf, fmt, args);
	va_end(args);

	return ret;
}
