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

#include "QF/sizebuf.h"

void MSG_WriteChar (sizebuf_t *sb, unsigned int c);
void MSG_WriteByte (sizebuf_t *sb, unsigned int c);
void MSG_WriteShort (sizebuf_t *sb, unsigned int c);
void MSG_WriteLong (sizebuf_t *sb, unsigned int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteCoord (sizebuf_t *sb, float coord);
void MSG_WriteCoordV (sizebuf_t *sb, vec3_t coord);
void MSG_WriteCoordAngleV (sizebuf_t *sb, vec3_t coord, vec3_t angles);
void MSG_WriteAngle (sizebuf_t *sb, float angle);
void MSG_WriteAngleV (sizebuf_t *sb, vec3_t angles);
void MSG_WriteAngle16 (sizebuf_t *sb, float angle16);

typedef struct msg_s {
	int readcount;
	qboolean badread;		// set if a read goes beyond end of message
	sizebuf_t *message;
	size_t badread_string_size;
	char *badread_string;
} msg_t;

void MSG_BeginReading (msg_t *msg);
int MSG_GetReadCount(msg_t *msg);
int MSG_ReadChar (msg_t *msg);
int MSG_ReadByte (msg_t *msg);
int MSG_ReadShort (msg_t *msg);
int MSG_ReadLong (msg_t *msg);
float MSG_ReadFloat (msg_t *msg);
const char *MSG_ReadString (msg_t *msg);
const char *MSG_ReadStringLine (msg_t *msg);

float MSG_ReadCoord (msg_t *msg);
void MSG_ReadCoordV (msg_t *msg, vec3_t coord);
float MSG_ReadAngle (msg_t *msg);
void MSG_ReadCoordAngleV (msg_t *msg, vec3_t coord, vec3_t angles);
void MSG_ReadAngleV (msg_t *msg, vec3_t angles);
float MSG_ReadAngle16 (msg_t *msg);

#endif
