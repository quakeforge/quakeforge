/*
	va.c

	varargs printf function

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <stdarg.h>

#include "QF/dstring.h"
#include "QF/va.h"


/*
	va

	does a varargs printf into a temp buffer, so I don't need to have
	varargs versions of all text functions.
*/
VISIBLE char *
va (const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string[4];
#define NUM_STRINGS (sizeof (string) / sizeof (string[0]))
	static int  str_index;
	dstring_t  *dstr;

	if (!string[0]) {
		for (size_t i = 0; i < NUM_STRINGS; i++) {
			string[i] = dstring_new ();
		}
	}
	dstr = string[str_index++ % NUM_STRINGS];

	va_start (args, fmt);
	dvsprintf (dstr, fmt, args);
	va_end (args);

	return dstr->str;
}

VISIBLE char *
nva (const char *fmt, ...)
{
	va_list     args;
	dstring_t  *string;

	string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	return dstring_freeze (string);
}
