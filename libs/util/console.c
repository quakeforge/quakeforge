/*
	console.c

	(description)

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

	$Id$
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

#include "compat.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/sys.h"

/*
	Con_Printf

	Handles cursor positioning, line wrapping, etc
*/
#define	MAXPRINTMSG	4096

void
Con_Printf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	// write it to the scrollable buffer
	Con_Print (msg);
}

/*
	Con_DPrintf

	A Con_Printf that only shows up if the "developer" cvar is set
*/
void
Con_DPrintf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	if (!developer->int_val)
		return;							// don't confuse non-developers with
	// techie stuff...

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Con_Print (msg);
}
