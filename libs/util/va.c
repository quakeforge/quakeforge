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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>

#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"


/*
	va

	does a varargs printf into a temp buffer, so I don't need to have
	varargs versions of all text functions.
*/
char       *
va (const char *fmt, ...)
{
	va_list     args;
	static char *string;
	int         size;
	static int  string_size;

	va_start (args, fmt);
	size = vsnprintf (string, string_size, fmt, args) + 1;  // +1 for nul
	//printf ("size = %d\n", size);
	while (size <= 0 || size > string_size) {
		if (size > 0)
			string_size = (size + 1023) & ~1023; // 1k multiples
		else
			string_size += 1024;
		string = realloc (string, string_size);
		if (!string)
			Sys_Error ("console: could not allocate %d bytes\n",
					   string_size);
		size = vsnprintf (string, string_size, fmt, args) + 1;
		//printf ("size = %d\n", size);
	}
	va_end (args);

	return string;
}
