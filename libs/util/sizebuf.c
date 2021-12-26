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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/sizebuf.h"
#include "QF/sys.h"
#include "QF/zone.h"


VISIBLE void
SZ_Alloc (sizebuf_t *buf, unsigned maxsize)
{
	if (maxsize < 256)
		maxsize = 256;
	buf->data = Hunk_AllocName (0, maxsize, "sizebuf");
	buf->maxsize = maxsize;
	buf->cursize = 0;
}

VISIBLE void
SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

VISIBLE void *
SZ_GetSpace (sizebuf_t *buf, unsigned length)
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

VISIBLE void
SZ_Write (sizebuf_t *buf, const void *data, unsigned length)
{
	memcpy (SZ_GetSpace (buf, length), data, length);
}

VISIBLE void
SZ_Print (sizebuf_t *buf, const char *data)
{
	unsigned    len;

	len = strlen (data) + 1;
	if (buf->cursize && !buf->data[buf->cursize - 1])
		buf->cursize--;					// write over trailing 0

	memcpy (SZ_GetSpace (buf, len), data, len);
}

VISIBLE void
SZ_Dump (sizebuf_t *buf)
{
	unsigned    i;
	char        chars[17], c;

	chars[16] = 0;
	for (i = 0; i < buf->cursize; i++) {
		if (i % 16 == 0) {
			Sys_Printf ("%04x:", i);
		} else if (i % 16 == 8) {
			Sys_Printf (" ");
		}
		Sys_Printf (" %02x", buf->data[i]);
		c = buf->data[i] & 0x7f;
		if (c < ' ') {
			c = '.';
		}
		chars[i % 16] = c;
		if (i % 16 == 15) {
			Sys_Printf ("  %s\n", chars);
		}
	}
	i %= 16;
	if (i) {
		chars[i] = 0;
		i = 16 - i;
		Sys_Printf ("%*s  %s\n", i * 3 + (i > 7), "", chars);
	}
}
