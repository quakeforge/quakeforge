/*
	sizebuf.c

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

#include "QF/sizebuf.h"
#include "QF/sys.h"
#include "QF/zone.h"


void
SZ_Alloc (sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = Hunk_AllocName (startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}

void
SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void       *
SZ_GetSpace (sizebuf_t *buf, int length)
{
	void       *data;

	if (buf->cursize + length <= buf->maxsize) {
getspace:
		data = buf->data + buf->cursize;
		buf->cursize += length;
		return data;
	}

	if (!buf->allowoverflow)
		Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d)",
				   buf->maxsize);

	if (length > buf->maxsize)
		Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);

	Sys_Printf ("SZ_GetSpace: overflow\n");
	SZ_Clear (buf);
	buf->overflowed = true;
	goto getspace;
}

void
SZ_Write (sizebuf_t *buf, const void *data, int length)
{
	memcpy (SZ_GetSpace (buf, length), data, length);
}

void
SZ_Print (sizebuf_t *buf, const char *data)
{
	int         len;

	len = strlen (data) + 1;

	if (!buf->cursize || buf->data[buf->cursize - 1])
		// no trailing 0
		memcpy (SZ_GetSpace (buf, len), data, len);
	else
		// write over trailing 0
		memcpy (SZ_GetSpace (buf, len - 1) - 1, data, len);
}
