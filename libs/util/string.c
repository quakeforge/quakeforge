/*
	string.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "qstring.h"

#ifdef IRIX
#define _vsnprintf vsnprintf
#endif

/*
	Q_strcasestr

	case-insensitive version of strstr
*/
const char *
Q_strcasestr (const char *haystack, const char *needle)
{
	size_t len = strlen (needle);
	while (*haystack) {
		if (!strncasecmp (haystack, needle, len))
			return haystack;
		haystack++;
	}
	return 0;
}

/*
	Q_strnlen

	strlen with a cutoff
*/
size_t
Q_strnlen (const char *s, size_t maxlen)
{
	size_t i;
	for (i = 0; i < maxlen && s[i]; i++);
	return i;
}

char *
Q_strndup (const char *s, size_t n)
{
	size_t l = strnlen (s, n);
	char *str = malloc (l + 1);
	strncpy (str, s, l);
	str[l] = 0;
	return str;
}

#if defined(HAVE__VSNPRINTF) && !defined(HAVE_VSNPRINTF)
size_t
Q_snprintfz (char *dest, size_t size, const char *fmt, ...)
{
	int   len;
	va_list  argptr;

	va_start (argptr, fmt);
	len = _vsnprintf (dest, size - 1, fmt, argptr);
	va_end (argptr);
	if (len < 0 && size) // the string didn't fit into the buffer
		dest[size - 1] = 0;
	return len;
}

size_t
Q_vsnprintfz (char *dest, size_t size, const char *fmt, va_list argptr)
{
	int   len;

	len = _vsnprintf (dest, size - 1, fmt, argptr);

	if (len < 0 && size) // the string didn't fit into the buffer
		dest[size - 1] = 0;
	return len;
}
#endif
