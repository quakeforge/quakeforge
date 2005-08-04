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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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
char *
va (const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string;

	if (!string)
		string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	return string->str;
}

char *
nva (const char *fmt, ...)
{
	va_list     args;
	static dstring_t *string;

	if (!string)
		string = dstring_new ();

	va_start (args, fmt);
	dvsprintf (string, fmt, args);
	va_end (args);

	return strdup (string->str);
}
