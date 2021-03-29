/*
	msg_backbuf.c

	user reliable data stream writes

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

#include "QF/msg.h"
#include "QF/sys.h"

#include "netchan.h"
#include "qw/msg_backbuf.h"


static void
PushBackbuf (backbuf_t *rel)
{
	int         tail_backbuf;

	Sys_MaskPrintf (SYS_dev, "backbuffering %d %s\n", rel->num_backbuf,
					rel->name);
	tail_backbuf = (rel->head_backbuf + rel->num_backbuf) % MAX_BACK_BUFFERS;
	memset (&rel->backbuf, 0, sizeof (rel->backbuf));
	rel->backbuf.allowoverflow = true;
	rel->backbuf.data = rel->backbuf_data[tail_backbuf];
	rel->backbuf.maxsize = sizeof (rel->backbuf_data[tail_backbuf]);
	rel->backbuf_size[tail_backbuf] = 0;
	rel->num_backbuf++;
}

int
MSG_ReliableCheckSize (backbuf_t *rel, int maxsize, int minsize)
{
	sizebuf_t  *msg = &rel->netchan->message;

	if (rel->num_backbuf)
		msg = &rel->backbuf;

	if (maxsize <= msg->maxsize - msg->cursize - 1)
		return maxsize;

	if (minsize <= msg->maxsize - msg->cursize - 1)
		return msg->maxsize - msg->cursize - 1;

	if (rel->num_backbuf == MAX_BACK_BUFFERS)
		return 0;

	return rel->backbuf.maxsize;
}

// check to see if client block will fit, if not, rotate buffers
sizebuf_t *
MSG_ReliableCheckBlock (backbuf_t *rel, int maxsize)
{
	sizebuf_t  *msg = &rel->netchan->message;

	if (rel->num_backbuf || msg->cursize > msg->maxsize - maxsize - 1) {
		// we would probably overflow the buffer, save it for next
		if (!rel->num_backbuf) {
			PushBackbuf (rel);
		}

		if (rel->backbuf.cursize > rel->backbuf.maxsize - maxsize - 1) {
			if (rel->num_backbuf == MAX_BACK_BUFFERS) {
				Sys_Printf ("WARNING: MAX_BACK_BUFFERS for %s\n", rel->name);
				rel->backbuf.cursize = 0;	// don't overflow without
											// allowoverflow set
				msg->overflowed = true;		// this will drop the client
				return 0;
			}
			PushBackbuf (rel);
		}
	}
	if (rel->num_backbuf)
		return &rel->backbuf;
	return &rel->netchan->message;
}

// begin a client block, estimated maximum size
sizebuf_t *
MSG_ReliableWrite_Begin (backbuf_t *rel, int c, int maxsize)
{
	sizebuf_t  *msg;
	msg = MSG_ReliableCheckBlock (rel, maxsize);
	MSG_ReliableWrite_Byte (rel, c);
	return msg;
}

void
MSG_Reliable_FinishWrite (backbuf_t *rel)
{
	int         tail_backbuf;

	if (rel->num_backbuf) {
		tail_backbuf = (rel->head_backbuf + rel->num_backbuf - 1)
					   % MAX_BACK_BUFFERS;
		rel->backbuf_size[tail_backbuf] = rel->backbuf.cursize;

		if (rel->backbuf.overflowed) {
			Sys_Printf ("WARNING: backbuf [%d] overflow for %s\n",
						rel->num_backbuf, rel->name);
			rel->netchan->message.overflowed = true;	// this will drop the
														// client
		}
	}
}

void
MSG_ReliableWrite_Angle (backbuf_t *rel, float f)
{
	if (rel->num_backbuf) {
		MSG_WriteAngle (&rel->backbuf, f);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteAngle (&rel->netchan->message, f);
}

void
MSG_ReliableWrite_Angle16 (backbuf_t *rel, float f)
{
	if (rel->num_backbuf) {
		MSG_WriteAngle16 (&rel->backbuf, f);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteAngle16 (&rel->netchan->message, f);
}

void
MSG_ReliableWrite_Byte (backbuf_t *rel, int c)
{
	if (rel->num_backbuf) {
		MSG_WriteByte (&rel->backbuf, c);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteByte (&rel->netchan->message, c);
}

void
MSG_ReliableWrite_Char (backbuf_t *rel, int c)
{
	if (rel->num_backbuf) {
		MSG_WriteByte (&rel->backbuf, c);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteByte (&rel->netchan->message, c);
}

void
MSG_ReliableWrite_Float (backbuf_t *rel, float f)
{
	if (rel->num_backbuf) {
		MSG_WriteFloat (&rel->backbuf, f);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteFloat (&rel->netchan->message, f);
}

void
MSG_ReliableWrite_Coord (backbuf_t *rel, float f)
{
	if (rel->num_backbuf) {
		MSG_WriteCoord (&rel->backbuf, f);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteCoord (&rel->netchan->message, f);
}

void
MSG_ReliableWrite_Long (backbuf_t *rel, int c)
{
	if (rel->num_backbuf) {
		MSG_WriteLong (&rel->backbuf, c);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteLong (&rel->netchan->message, c);
}

void
MSG_ReliableWrite_Short (backbuf_t *rel, int c)
{
	if (rel->num_backbuf) {
		MSG_WriteShort (&rel->backbuf, c);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteShort (&rel->netchan->message, c);
}

void
MSG_ReliableWrite_String (backbuf_t *rel, const char *s)
{
	if (rel->num_backbuf) {
		MSG_WriteString (&rel->backbuf, s);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteString (&rel->netchan->message, s);
}

void
MSG_ReliableWrite_SZ (backbuf_t *rel, const void *data, int len)
{
	if (rel->num_backbuf) {
		SZ_Write (&rel->backbuf, data, len);
		MSG_Reliable_FinishWrite (rel);
	} else
		SZ_Write (&rel->netchan->message, data, len);
}

void
MSG_ReliableWrite_AngleV (backbuf_t *rel, const vec3_t v)
{
	if (rel->num_backbuf) {
		MSG_WriteAngleV (&rel->backbuf, v);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteAngleV (&rel->netchan->message, v);
}

void
MSG_ReliableWrite_CoordV (backbuf_t *rel, const vec3_t v)
{
	if (rel->num_backbuf) {
		MSG_WriteCoordV (&rel->backbuf, v);
		MSG_Reliable_FinishWrite (rel);
	} else
		MSG_WriteCoordV (&rel->netchan->message, v);
}

void
MSG_Reliable_Send (backbuf_t *rel)
{
	sizebuf_t  *msg = &rel->netchan->message;
	int        *size = &rel->backbuf_size[rel->head_backbuf];
	byte       *data = rel->backbuf_data[rel->head_backbuf];

	if (!rel->num_backbuf)
		return;
	// will it fit?
	if (msg->cursize + *size < msg->maxsize) {
		Sys_MaskPrintf (SYS_dev, "%s: backbuf %d bytes\n", rel->name, *size);
		// it'll fit
		SZ_Write (msg, data, *size);

		// move along, move along
		if (--rel->num_backbuf)
			rel->head_backbuf = (rel->head_backbuf + 1) % MAX_BACK_BUFFERS;
	}
}
