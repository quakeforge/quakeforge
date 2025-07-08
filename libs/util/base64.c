/*
	base64.c

	Base-64 encoding and decoding

	Copyright (C) 2025  Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/base64.h"
#include "QF/dstring.h"
#include "QF/qtypes.h"

static const char encode_table[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static byte decode_table[256];

static void __attribute__((constructor))
base64_init (void)
{
	for (const char *c = encode_table; *c; c++) {
		decode_table[(byte)*c] = (c - encode_table) | (1 << 6);
	}
	decode_table['-'] = 62 | (1 << 6);
	decode_table['_'] = 63 | (1 << 6);
	decode_table[','] = 63 | (1 << 6);
}

size_t
base64_encode (dstring_t *out, const void *data, size_t count)
{
	const byte *src = data;
	size_t out_count = 4 * ((count + 2) / 3);
	char *dst = dstring_openstr (out, out_count);

	while (count >= 3) {
		uint32_t v = *src++ << 16;
		v |= *src++ << 8;
		v |= *src++;
		*dst++ = encode_table[(v >> 18) & 0x3f];
		*dst++ = encode_table[(v >> 12) & 0x3f];
		*dst++ = encode_table[(v >>  6) & 0x3f];
		*dst++ = encode_table[(v >>  0) & 0x3f];
		count -= 3;
	}
	if (count == 2) {
		uint32_t v = *src++ << 16;
		v |= *src++ << 8;
		*dst++ = encode_table[(v >> 18) & 0x3f];
		*dst++ = encode_table[(v >> 12) & 0x3f];
		*dst++ = encode_table[(v >>  6) & 0x3f];
		*dst++ = '=';
	} else if (count == 1) {
		uint32_t v = *src++ << 16;
		*dst++ = encode_table[(v >> 18) & 0x3f];
		*dst++ = encode_table[(v >> 12) & 0x3f];
		*dst++ = '=';
		*dst++ = '=';
	}
	*dst = 0;

	return out_count;
}

size_t
base64_decode (dstring_t *out, const char *data, size_t count)
{
	auto src = (const byte *) data;
	// guaranteed maximum
	size_t out_count = ((count + 3) / 4) * 3;
	auto dst = (byte *) dstring_open (out, out_count);
	auto start = dst;

	uint32_t v = 0;
	int shift = 0;
	while (count-- > 0) {
		byte e = decode_table[*src++];
		v |= (e & 0x3f) << (10 - shift);
		shift += 6 * ((e >> 6) & 1);
		*dst = v >> 8;
		dst += shift >> 3;
		v <<= shift & 8;
		shift -= shift & 8;
	}
	*dst = 0;
	return dst - start;
}
