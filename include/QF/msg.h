/*
	msg.h

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
#ifndef _MSG_H
#define _MSG_H

/** \defgroup msg Message reading and writing
	\ingroup utils
*/
//@{

#include "QF/sizebuf.h"

void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteCoord (sizebuf_t *sb, float coord);
void MSG_WriteCoordV (sizebuf_t *sb, const vec3_t coord);
void MSG_WriteCoordAngleV (sizebuf_t *sb, const vec3_t coord,
						   const vec3_t angles);
void MSG_WriteAngle (sizebuf_t *sb, float angle);
void MSG_WriteAngleV (sizebuf_t *sb, const vec3_t angles);
void MSG_WriteAngle16 (sizebuf_t *sb, float angle16);
void MSG_WriteUTF8 (sizebuf_t *sb, unsigned utf8);

typedef struct msg_s {
	int readcount;
	qboolean badread;		// set if a read goes beyond end of message
	sizebuf_t *message;
	size_t badread_string_size;
	char *badread_string;
} qmsg_t;

void MSG_BeginReading (qmsg_t *msg);
int MSG_GetReadCount(qmsg_t *msg);
int MSG_ReadByte (qmsg_t *msg);
int MSG_ReadShort (qmsg_t *msg);
int MSG_ReadLong (qmsg_t *msg);
float MSG_ReadFloat (qmsg_t *msg);
const char *MSG_ReadString (qmsg_t *msg);
int MSG_ReadBytes (qmsg_t *msg, void *buf, int len);

float MSG_ReadCoord (qmsg_t *msg);
void MSG_ReadCoordV (qmsg_t *msg, vec3_t coord);
float MSG_ReadAngle (qmsg_t *msg);
void MSG_ReadCoordAngleV (qmsg_t *msg, vec3_t coord, vec3_t angles);
void MSG_ReadAngleV (qmsg_t *msg, vec3_t angles);
float MSG_ReadAngle16 (qmsg_t *msg);
int MSG_ReadUTF8 (qmsg_t *msg);

//@}

#endif
