/*
	sv_recorder.h

	Interface for recording server state (server side demos and qtv)

	Copyright (C) 2005 #AUTHOR#

	Author: Bill Currie
	Date: 2005/5/1

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

#ifndef __sv_recorder_h
#define __sv_recorder_h

struct sizebuf_s;
typedef struct recorder_s recorder_t;

void SVR_Init (void);
recorder_t *SVR_AddUser (void (*writer)(void *, struct sizebuf_s *, int),
						 int (*frame)(void *),
						 void (*finish)(void *, struct sizebuf_s *),
						 int demo, void *user);
void SVR_RemoveUser (recorder_t *r);
struct sizebuf_s *SVR_WriteBegin (byte type, int to, int size);
struct sizebuf_s *SVR_Datagram (void);
void SVR_ForceFrame (void);
int SVR_Frame (void);
void SV_SendDemoMessage (void);

#endif//__sv_recorder_h
