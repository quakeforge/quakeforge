/*
	string.h

	A string helper function

	Copyright (C) 2001  Adam Olsen

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

#ifndef string_h
#define string_h

#include <sys/types.h>
#include <stdarg.h>

const char * Q_strcasestr (const char *haystack, const char *needle) __attribute__((pure));
size_t Q_strnlen (const char *s, size_t maxlen) __attribute__((pure));
char *Q_strndup (const char *s, size_t maxlen);
size_t Q_snprintfz (char *dest, size_t size, const char *fmt, ...) __attribute__((format(PRINTF,3,4)));
size_t Q_vsnprintfz (char *dest, size_t size, const char *fmt, va_list argptr);

#endif // string_h
