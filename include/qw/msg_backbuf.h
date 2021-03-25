/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifndef __qw_msg_backbuf_h
#define __qw_msg_backbuf_h

#define MAX_BACK_BUFFERS    8

typedef struct backbuf_s {
	sizebuf_t   backbuf;
	int         num_backbuf;
	int         head_backbuf;
	int         backbuf_size[MAX_BACK_BUFFERS];
	byte        backbuf_data[MAX_BACK_BUFFERS][MAX_MSGLEN];
	struct netchan_s *netchan;
	const char *name;
} backbuf_t;

int MSG_ReliableCheckSize (backbuf_t *rel, int maxsize, int minsize) __attribute__((pure));
sizebuf_t *MSG_ReliableCheckBlock(backbuf_t *rel, int maxsize);
void MSG_Reliable_FinishWrite(backbuf_t *rel);
sizebuf_t *MSG_ReliableWrite_Begin(backbuf_t *rel, int c, int maxsize);
void MSG_ReliableWrite_Angle(backbuf_t *rel, float f);
void MSG_ReliableWrite_Angle16(backbuf_t *rel, float f);
void MSG_ReliableWrite_Byte(backbuf_t *rel, int c);
void MSG_ReliableWrite_Char(backbuf_t *rel, int c);
void MSG_ReliableWrite_Float(backbuf_t *rel, float f);
void MSG_ReliableWrite_Coord(backbuf_t *rel, float f);
void MSG_ReliableWrite_Long(backbuf_t *rel, int c);
void MSG_ReliableWrite_Short(backbuf_t *rel, int c);
void MSG_ReliableWrite_String(backbuf_t *rel, const char *s);
void MSG_ReliableWrite_SZ(backbuf_t *rel, const void *data, int len);
void MSG_ReliableWrite_AngleV(backbuf_t *rel, const vec3_t v);
void MSG_ReliableWrite_CoordV(backbuf_t *rel, const vec3_t v);
void MSG_Reliable_Send (backbuf_t *rel);

#endif//__qw_msg_backbuf_h
