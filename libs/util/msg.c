/*
	msg.c

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/string.h"

#include "compat.h"

/*
	MESSAGE IO FUNCTIONS

	Handles byte ordering and avoids alignment errors
*/

// writing functions

void
MSG_WriteChar (sizebuf_t *sb, unsigned int c)
{
	byte       *buf;

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void
MSG_WriteByte (sizebuf_t *sb, unsigned int c)
{
	byte       *buf;

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void
MSG_WriteShort (sizebuf_t *sb, unsigned int c)
{
	byte       *buf;

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void
MSG_WriteLong (sizebuf_t *sb, unsigned int c)
{
	byte       *buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void
MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union {
		float       f;
		unsigned int l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void
MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen (s) + 1);
}

void
MSG_WriteCoord (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (unsigned int) (f * 8));
}

void
MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, (unsigned int) (f * 256 / 360) & 255);
}

void
MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (unsigned int) (f * 65536 / 360) & 65535);
}


// reading functions

void
MSG_BeginReading (msg_t *msg)
{
	msg->readcount = 0;
	msg->badread = false;
}

int
MSG_GetReadCount (msg_t *msg)
{
	return msg->readcount;
}

// returns -1 and sets msg->badread if no more characters are available
int
MSG_ReadChar (msg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (signed char) msg->message->data[msg->readcount++];

	msg->badread = true;
	return -1;
}

int
MSG_ReadByte (msg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (unsigned char) msg->message->data[msg->readcount++];

	msg->badread = true;
	return -1;
}

int
MSG_ReadShort (msg_t *msg)
{
	int         c;

	if (msg->readcount + 2 <= msg->message->cursize) {
		c = (short) (msg->message->data[msg->readcount]
					 + (msg->message->data[msg->readcount + 1] << 8));
		msg->readcount += 2;
		return c;
	}
	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;

}

int
MSG_ReadLong (msg_t *msg)
{
	int         c;

	if (msg->readcount + 4 <= msg->message->cursize) {
		c = msg->message->data[msg->readcount]
			+ (msg->message->data[msg->readcount + 1] << 8)
			+ (msg->message->data[msg->readcount + 2] << 16)
			+ (msg->message->data[msg->readcount + 3] << 24);
		msg->readcount += 4;
		return c;
	}
	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

float
MSG_ReadFloat (msg_t *msg)
{
	union {
		byte        b[4];
		float       f;
		int         l;
	} dat;

	if (msg->readcount + 4 <= msg->message->cursize) {
		dat.b[0] = msg->message->data[msg->readcount];
		dat.b[1] = msg->message->data[msg->readcount + 1];
		dat.b[2] = msg->message->data[msg->readcount + 2];
		dat.b[3] = msg->message->data[msg->readcount + 3];
		msg->readcount += 4;

		dat.l = LittleLong (dat.l);

		return dat.f;
	}

	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

const char *
MSG_ReadString (msg_t *msg)
{
	char   *string;
	int		len, maxlen;

	if (msg->badread || msg->readcount + 1 > msg->message->cursize) {
		msg->badread = true;
		return "";
	}

	string = &msg->message->data[msg->readcount];

	maxlen = msg->message->cursize - msg->readcount;
	len = strnlen (string, maxlen);
	if (len == maxlen) {
		msg->readcount = msg->readcount;
		msg->badread = true;
		if (len + 1 > msg->badread_string_size) {
			if (msg->badread_string)
				free (msg->badread_string);
			msg->badread_string = malloc (len + 1);
			msg->badread_string_size = len + 1;
		}
		if (!msg->badread_string)
			Sys_Error ("MSG_ReadString: out of memory\n");
		strncpy (msg->badread_string, string, len);
		msg->badread_string[len] = 0;
		return msg->badread_string;
	}
	msg->readcount += len + 1;

	return string;
}

float
MSG_ReadCoord (msg_t *msg)
{
	return MSG_ReadShort (msg) * (1.0 / 8);
}

float
MSG_ReadAngle (msg_t *msg)
{
	return MSG_ReadChar (msg) * (360.0 / 256);
}

float
MSG_ReadAngle16 (msg_t *msg)
{
	return MSG_ReadShort (msg) * (360.0 / 65536);
}
