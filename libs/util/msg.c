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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"

#include "compat.h"

/*
	MESSAGE IO FUNCTIONS

	Handles byte ordering and avoids alignment errors
*/

// writing functions ==========================================================

void
MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte	   *buf;

	buf = SZ_GetSpace (sb, 1);
	*buf = c;
}

void
MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte	   *buf;

	buf = SZ_GetSpace (sb, 2);
	*buf++ = ((unsigned int) c) & 0xff;
	*buf = ((unsigned int) c) >> 8;
}

void
MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte	   *buf;

	buf = SZ_GetSpace (sb, 4);
	*buf++ = ((unsigned int) c) & 0xff;
	*buf++ = (((unsigned int) c) >> 8) & 0xff;
	*buf++ = (((unsigned int) c) >> 16) & 0xff;
	*buf = ((unsigned int) c) >> 24;
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
MSG_WriteCoord (sizebuf_t *sb, float coord)
{
	MSG_WriteShort (sb, (int) (coord * 8.0));
}

void
MSG_WriteCoordV (sizebuf_t *sb, const vec3_t coord)
{
	byte        *buf;
	unsigned int i, j;

	buf = SZ_GetSpace (sb, 6);
	for (i = 0; i < 3; i++) {
		j = (int) (coord[i] * 8.0);
		*buf++ = j & 0xff;
		*buf++ = j >> 8;
	}
}

void
MSG_WriteCoordAngleV (sizebuf_t *sb, const vec3_t coord, const vec3_t angles)
{
	byte   *buf;
	int		i, j;

	buf = SZ_GetSpace (sb, 9);
	for (i = 0; i < 3; i++) {
		j = (int) (coord[i] * 8.0);
		*buf++ = j & 0xff;
		*buf++ = j >> 8;
		*buf++ = ((int) (angles[i] * (256.0 / 360.0)) & 255);
	}
}

void
MSG_WriteAngle (sizebuf_t *sb, float angle)
{
	MSG_WriteByte (sb, (int) (angle * (256.0 / 360.0)) & 255);
}

void
MSG_WriteAngleV (sizebuf_t *sb, const vec3_t angles)
{
	byte   *buf;
	int		i;

	buf = SZ_GetSpace (sb, 3);
	for (i = 0; i < 3; i++) {
		*buf++ = ((int) (angles[i] * (256.0 / 360.0)) & 255);
	}
}

void
MSG_WriteAngle16 (sizebuf_t *sb, float angle16)
{
	MSG_WriteShort (sb, (int) (angle16 * (65536.0 / 360.0)) & 65535);
}

// reading functions ==========================================================

void
MSG_BeginReading (qmsg_t *msg)
{
	msg->readcount = 0;
	msg->badread = false;
}

int
MSG_GetReadCount (qmsg_t *msg)
{
	return msg->readcount;
}

int
MSG_ReadByte (qmsg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (unsigned char) msg->message->data[msg->readcount++];

	msg->badread = true;
	return -1;
}

int
MSG_ReadShort (qmsg_t *msg)
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
MSG_ReadLong (qmsg_t *msg)
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
MSG_ReadFloat (qmsg_t *msg)
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
MSG_ReadString (qmsg_t *msg)
{
	char   *string;
	size_t len, maxlen;

	if (msg->badread || msg->readcount + 1 > msg->message->cursize) {
		msg->badread = true;
		return "";
	}

	string = (char *) &msg->message->data[msg->readcount];

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
			Sys_Error ("MSG_ReadString: out of memory");
		strncpy (msg->badread_string, string, len);
		msg->badread_string[len] = 0;
		return msg->badread_string;
	}
	msg->readcount += len + 1;

	return string;
}

float
MSG_ReadCoord (qmsg_t *msg)
{
	return MSG_ReadShort (msg) * (1.0 / 8.0);
}

void
MSG_ReadCoordV (qmsg_t *msg, vec3_t coord)
{
	int		i;

	for (i = 0; i < 3; i++)
		coord[i] = MSG_ReadShort (msg) * (1.0 / 8.0);
}

void
MSG_ReadCoordAngleV (qmsg_t *msg, vec3_t coord, vec3_t angles)
{
	int		i;

	for (i = 0; i < 3; i++) {
		coord[i] = MSG_ReadShort (msg) * (1.0 / 8.0);
		angles[i] = ((signed char) MSG_ReadByte (msg)) * (360.0 / 256.0);
	}
}

float
MSG_ReadAngle (qmsg_t *msg)
{
	return ((signed char) MSG_ReadByte (msg)) * (360.0 / 256.0);
}

void
MSG_ReadAngleV (qmsg_t *msg, vec3_t angles)
{
	int		i;

	for (i = 0; i < 3; i++)
		angles[i] = ((signed char) MSG_ReadByte (msg)) * (360.0 / 256.0);
}

float
MSG_ReadAngle16 (qmsg_t *msg)
{
	return MSG_ReadShort (msg) * (360.0 / 65536.0);
}
