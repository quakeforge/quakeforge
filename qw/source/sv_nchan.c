/*
	sv_nchan.c

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/msg.h"

#include "server.h"


static void
PushBackbuf (client_t *cl)
{
	memset (&cl->backbuf, 0, sizeof (cl->backbuf));
	cl->backbuf.allowoverflow = true;
	cl->backbuf.data = cl->backbuf_data[cl->num_backbuf];
	cl->backbuf.maxsize = sizeof (cl->backbuf_data[cl->num_backbuf]);
	cl->backbuf_size[cl->num_backbuf] = 0;
	cl->num_backbuf++;
}

int
ClientReliableCheckSize (client_t *cl, int maxsize, int minsize)
{
	sizebuf_t  *msg = &cl->netchan.message;

	if (cl->num_backbuf)
		msg = &cl->backbuf;

	if (maxsize <= msg->maxsize - msg->cursize - 1)
		return maxsize;

	if (minsize <= msg->maxsize - msg->cursize - 1)
		return msg->maxsize - msg->cursize - 1;

	if (cl->num_backbuf == MAX_BACK_BUFFERS)
		return 0;

	return cl->backbuf.maxsize;
}

// check to see if client block will fit, if not, rotate buffers
void
ClientReliableCheckBlock (client_t *cl, int maxsize)
{
	sizebuf_t  *msg = &cl->netchan.message;

	if (cl->num_backbuf || msg->cursize > msg->maxsize - maxsize - 1) {
		// we would probably overflow the buffer, save it for next
		if (!cl->num_backbuf) {
			PushBackbuf (cl);
		}

		if (cl->backbuf.cursize > cl->backbuf.maxsize - maxsize - 1) {
			if (cl->num_backbuf == MAX_BACK_BUFFERS) {
				SV_Printf ("WARNING: MAX_BACK_BUFFERS for %s\n", cl->name);
				cl->backbuf.cursize = 0;	// don't overflow without
											// allowoverflow set
				msg->overflowed = true;		// this will drop the client
				return;
			}
			PushBackbuf (cl);
		}
	}
}

// begin a client block, estimated maximum size
void
ClientReliableWrite_Begin (client_t *cl, int c, int maxsize)
{
	ClientReliableCheckBlock (cl, maxsize);
	ClientReliableWrite_Byte (cl, c);
}

void
ClientReliable_FinishWrite (client_t *cl)
{
	if (cl->num_backbuf) {
		cl->backbuf_size[cl->num_backbuf - 1] = cl->backbuf.cursize;

		if (cl->backbuf.overflowed) {
			SV_Printf ("WARNING: backbuf [%d] reliable overflow for %s\n",
						cl->num_backbuf, cl->name);
			cl->netchan.message.overflowed = true;	// this will drop the
													// client
		}
	}
}

void
ClientReliableWrite_Angle (client_t *cl, float f)
{
	if (cl->num_backbuf) {
		MSG_WriteAngle (&cl->backbuf, f);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteAngle (&cl->netchan.message, f);
}

void
ClientReliableWrite_Angle16 (client_t *cl, float f)
{
	if (cl->num_backbuf) {
		MSG_WriteAngle16 (&cl->backbuf, f);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteAngle16 (&cl->netchan.message, f);
}

void
ClientReliableWrite_Byte (client_t *cl, int c)
{
	if (cl->num_backbuf) {
		MSG_WriteByte (&cl->backbuf, c);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteByte (&cl->netchan.message, c);
}

void
ClientReliableWrite_Char (client_t *cl, int c)
{
	if (cl->num_backbuf) {
		MSG_WriteByte (&cl->backbuf, c);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteByte (&cl->netchan.message, c);
}

void
ClientReliableWrite_Float (client_t *cl, float f)
{
	if (cl->num_backbuf) {
		MSG_WriteFloat (&cl->backbuf, f);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteFloat (&cl->netchan.message, f);
}

void
ClientReliableWrite_Coord (client_t *cl, float f)
{
	if (cl->num_backbuf) {
		MSG_WriteCoord (&cl->backbuf, f);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteCoord (&cl->netchan.message, f);
}

void
ClientReliableWrite_Long (client_t *cl, int c)
{
	if (cl->num_backbuf) {
		MSG_WriteLong (&cl->backbuf, c);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteLong (&cl->netchan.message, c);
}

void
ClientReliableWrite_Short (client_t *cl, int c)
{
	if (cl->num_backbuf) {
		MSG_WriteShort (&cl->backbuf, c);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteShort (&cl->netchan.message, c);
}

void
ClientReliableWrite_String (client_t *cl, const char *s)
{
	if (cl->num_backbuf) {
		MSG_WriteString (&cl->backbuf, s);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteString (&cl->netchan.message, s);
}

void
ClientReliableWrite_SZ (client_t *cl, const void *data, int len)
{
	if (cl->num_backbuf) {
		SZ_Write (&cl->backbuf, data, len);
		ClientReliable_FinishWrite (cl);
	} else
		SZ_Write (&cl->netchan.message, data, len);
}

void
ClientReliableWrite_AngleV (client_t *cl, const vec3_t v)
{
	if (cl->num_backbuf) {
		MSG_WriteAngleV (&cl->backbuf, v);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteAngleV (&cl->netchan.message, v);
}

void
ClientReliableWrite_CoordV (client_t *cl, const vec3_t v)
{
	if (cl->num_backbuf) {
		MSG_WriteCoordV (&cl->backbuf, v);
		ClientReliable_FinishWrite (cl);
	} else
		MSG_WriteCoordV (&cl->netchan.message, v);
}
